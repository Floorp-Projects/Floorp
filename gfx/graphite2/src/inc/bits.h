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

namespace graphite2
{

template<typename T>
inline unsigned int bit_set_count(T v)
{
	v = v - ((v >> 1) & T(~T(0)/3));                           // temp
	v = (v & T(~T(0)/15*3)) + ((v >> 2) & T(~T(0)/15*3));      // temp
	v = (v + (v >> 4)) & T(~T(0)/255*15);                      // temp
	return (T)(v * T(~T(0)/255)) >> (sizeof(T)-1)*8;           // count
}


template<int S>
inline unsigned long _mask_over_val(unsigned long v)
{
	v = _mask_over_val<S/2>(v);
	v |= v >> S*4;
	return v;
}

template<>
inline unsigned long _mask_over_val<1>(unsigned long v)
{
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	return v;
}

template<typename T>
inline T mask_over_val(T v)
{
	return _mask_over_val<sizeof(T)>(v);
}

template<typename T>
inline unsigned long next_highest_power2(T v)
{
	return _mask_over_val<sizeof(T)>(v-1)+1;
}

template<typename T>
inline unsigned int log_binary(T v)
{
    return bit_set_count(mask_over_val(v))-1;
}

template<typename T>
inline T has_zero(const T x)
{
	return (x - T(~T(0)/255)) & ~x & T(~T(0)/255*128);
}

template<typename T>
inline T zero_bytes(const T x, unsigned char n)
{
	const T t = T(~T(0)/255*n);
	return T((has_zero(x^t) >> 7)*n);
}

}
