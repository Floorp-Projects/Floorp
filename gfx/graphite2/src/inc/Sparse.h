/*  GRAPHITE2 LICENSING

    Copyright 2010, SIL International
    All rights reserved.

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 2.1 of License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should also have received a copy of the GNU Lesser General Public
    License along with this library in the file named "LICENSE".
    If not, write to the Free Software Foundation, 51 Franklin Street, 
    Suite 500, Boston, MA 02110-1335, USA or visit their web page on the 
    internet at http://www.fsf.org/licenses/lgpl.html.

Alternatively, the contents of this file may be used under the terms of the
Mozilla Public License (http://mozilla.org/MPL) or the GNU General Public
License, as published by the Free Software Foundation, either version 2
of the License or (at your option) any later version.
*/
#pragma once

#include "inc/Main.h"

namespace graphite2 {



class sparse
{
	typedef unsigned long	mask_t;

	static const unsigned char  SIZEOF_CHUNK = (sizeof(mask_t) - sizeof(uint16))*8;

	struct chunk
	{
		mask_t			mask:SIZEOF_CHUNK;
		uint16			offset;
	};

public:
	typedef	uint16	key;
	typedef uint16	value;

	template<typename I>
	sparse(I first, const I last);
	sparse() throw();
	~sparse() throw();

	value 	operator [] (int k) const throw();
	operator bool () const throw();

	size_t capacity() const throw();
	size_t size()     const throw();

	size_t _sizeof() const throw();
	
	CLASS_NEW_DELETE;
private:
	union {
		chunk * map;
		value * values;
	}           m_array;
	key         m_nchunks;
};


inline
sparse::sparse() throw() : m_nchunks(0)
{
	m_array.map = 0;
}


template <typename I>
sparse::sparse(I attr, const I last)
: m_nchunks(0)
{
	// Find the maximum extent of the key space.
	size_t n_values=0;
	for (I i = attr; i != last; ++i, ++n_values)
	{
		const key k = i->id / SIZEOF_CHUNK;
		if (k >= m_nchunks) m_nchunks = k+1;
	}

	m_array.values = grzeroalloc<value>((m_nchunks*sizeof(chunk) + sizeof(value)/2)/sizeof(value) + n_values*sizeof(value));

	if (m_array.values == 0 || m_nchunks == 0)
	{
		free(m_array.values); m_array.map=0;
		return;
	}

	chunk * ci = m_array.map;
	ci->offset = (m_nchunks*sizeof(chunk) + sizeof(value)-1)/sizeof(value);
	value * vi = m_array.values + ci->offset;
	for (; attr != last; ++attr, ++vi)
	{
		const typename I::value_type v = *attr;
		chunk * const ci_ = m_array.map + v.id/SIZEOF_CHUNK;

		if (ci != ci_)
		{
			ci = ci_;
			ci->offset = vi - m_array.values;
		}

		ci->mask |= 1UL << (SIZEOF_CHUNK - 1 - (v.id % SIZEOF_CHUNK));
		*vi = v.value;
	}
}


inline
sparse::operator bool () const throw()
{
	return m_array.map != 0;
}


inline
size_t sparse::capacity() const throw()
{
	return m_nchunks;
}


inline
size_t sparse::_sizeof() const throw()
{
	return sizeof(sparse) + size()*sizeof(value) + m_nchunks*sizeof(chunk);
}

} // namespace graphite2
