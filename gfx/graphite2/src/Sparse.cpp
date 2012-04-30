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

#include "inc/Sparse.h"

using namespace graphite2;

namespace
{
	template<typename T>
	inline unsigned int bit_set_count(T v)
	{
		v = v - ((v >> 1) & T(~T(0)/3));                           // temp
		v = (v & T(~T(0)/15*3)) + ((v >> 2) & T(~T(0)/15*3));      // temp
		v = (v + (v >> 4)) & T(~T(0)/255*15);                      // temp
		return (T)(v * T(~T(0)/255)) >> (sizeof(T)-1)*8;           // count
	}
}


sparse::~sparse() throw()
{
	free(m_array.values);
}


sparse::value sparse::operator [] (int k) const throw()
{
	value g = value(k < m_nchunks*SIZEOF_CHUNK);	// This will be 0 is were out of bounds
	k *= g;									// Force k to 0 if out of bounds making the map look up safe
	const chunk & 		c = m_array.map[k/SIZEOF_CHUNK];
	const mask_t 		m = c.mask >> (SIZEOF_CHUNK - 1 - (k%SIZEOF_CHUNK));
	g *= m & 1;			// Extend the guard value to consider the residency bit

	return g*m_array.values[c.offset + g*bit_set_count(m >> 1)];
}


size_t sparse::size() const throw()
{
	size_t n = m_nchunks,
		   s = 0;

	for (const chunk *ci=m_array.map; n; --n, ++ci)
		s += bit_set_count(ci->mask);

	return s;
}
