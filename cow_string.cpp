#include "cow_string.hpp"
#include <cstring>
#include <stdexcept>
#include <ext/utility.hpp>

namespace ext
{
	cow_string::heap_body cow_string::ms_shared_null = cow_string::init_shared_null();

	cow_string::heap_body cow_string::init_shared_null()
	{
		cow_string::heap_body body;
		body.capacity = 0;
		body.size = 0;
		body.buffer[0] = 0;
		
		intrusive_ptr_add_ref(&body);
		return body;
	}

	static cow_string::size_type increase_size(cow_string::size_type cursize,
	                                           cow_string::size_type incsize)
	{
		incsize = cursize + incsize;
		// overflow or more than max_size
		// size_type is unsigned so overflow is well defined
		if (incsize < cursize || incsize >= cow_string::max_size())
			cow_string::throw_xlen();

		return incsize;
	}

	template <class SizeType>
	BOOST_FORCEINLINE
	static SizeType decrease_size(SizeType cursize, cow_string::size_type decsize)
	{
		assert(decsize <= cursize);
		return static_cast<SizeType>(cursize - decsize);
	}

	BOOST_FORCEINLINE auto cow_string::mutable_buffer() const noexcept -> value_type *
	{
		return const_cast<value_type *>(m_body->buffer);
	}

	inline auto cow_string::alloc_body(std::nothrow_t, size_type cap) -> heap_body * 
	{
		cap += sizeof(size_type) * 3 + 1; // 1 for null terminator
		heap_body * body = reinterpret_cast<heap_body *>(new(std::nothrow) char[cap]);
		new (body) heap_body;
		return body;
	}

	inline auto cow_string::alloc_body(size_type cap) -> heap_body *
	{		
		cap += sizeof(heap_body) - alignof(heap_body) + 1; // 1 for null terminator
		heap_body * body = reinterpret_cast<heap_body *>(new char[cap]);
		new (body) heap_body;
		return body;
	}

	cow_string::heap_body * intrusive_ptr_clone(cow_string::heap_body  * ptr)
	{
		auto cap = sizeof(cow_string::size_type) * 3 + ptr->capacity + 1; // 1 for null terminator
		cow_string::heap_body * body = reinterpret_cast<cow_string::heap_body *>(new char[cap]);
		new (body) cow_string::heap_body;

		body->capacity = ptr->capacity;
		body->size = ptr->size;
		std::memcpy(body->buffer, ptr->buffer, ptr->size);

		intrusive_ptr_add_ref(body);
		return body;
	}

	auto cow_string::alloc_body_adjusted(const heap_body & body, size_type newcap) -> heap_body *
	{
		cow_string::heap_body * newbody;
		auto oldcap = body.capacity;
		auto cap = (oldcap / 2 <= newcap / 3) ? newcap : oldcap + oldcap / 2;

		// if was overflow or allocation failed
		if (cap < newcap || (newbody = alloc_body(std::nothrow, cap)) == nullptr)
		{
			cap = newcap;
			newbody = alloc_body(cap);
		}

		newbody->capacity = cap;
		newbody->size = body.size;
		std::memcpy(newbody->buffer, body.buffer, body.size);

		return newbody;
	}

	BOOST_NORETURN void cow_string::throw_xlen()
	{
		throw std::length_error("length_error");
	}

	void cow_string::resize(size_type newsize)
	{
		if (newsize >= max_size()) throw_xlen();

		const auto & oldbody = *ext::as_const(m_body);
		if (newsize <= oldbody.capacity)
			m_body->size = newsize;
		else
		{
			auto * newbody = alloc_body_adjusted(oldbody, newsize);
			newbody->size = newsize;
			m_body.reset(newbody);
		}

		set_eos(mutable_buffer());
	}


	void cow_string::reserve(size_type newcap)
	{
		if (newcap >= max_size()) throw_xlen();

		const auto & oldbody = *ext::as_const(m_body);
		if (newcap <= oldbody.capacity) return;

		auto * newbody = alloc_body_adjusted(oldbody, newcap);
		m_body.reset(newbody);

		set_eos(mutable_buffer());
	}

	void cow_string::shrink_to_fit()
	{
		const auto & body = *ext::as_const(m_body);
		if (body.capacity == body.size) return;

		auto * newbody = alloc_body_adjusted(body, body.size);
		m_body.reset(newbody);

		set_eos(mutable_buffer());
	}


	auto cow_string::grow_to(size_type newsize) -> value_type *
	{
		if (newsize >= max_size()) throw_xlen();

		const auto & oldbody = *ext::as_const(m_body);
		if (newsize <= oldbody.capacity)
			m_body->size = newsize;
		else
		{
			auto * newbody = alloc_body_adjusted(oldbody, newsize);
			newbody->size = newsize;
			m_body.reset(newbody);
		}

		return mutable_buffer();
	}

	auto cow_string::grow_by(size_type size_increment) -> std::pair<value_type *, size_type>
	{
		const auto & body = *ext::as_const(m_body);
		auto newsize = increase_size(body.size, size_increment);

		if (newsize <= body.capacity)
			m_body->size = newsize;
		else
		{
			auto * newbody = alloc_body_adjusted(body, newsize);
			newbody->size = newsize;
			m_body.reset(newbody);
		}

		return {mutable_buffer(), newsize};
	}

	auto cow_string::shrink_by(size_type size_decrement) -> std::pair<value_type *, size_type>
	{
		const auto & body = *ext::as_const(m_body);
		auto newsize = decrease_size(body.size, size_decrement);
		m_body->size = newsize;
		
		return {mutable_buffer(), newsize};
	}
}
