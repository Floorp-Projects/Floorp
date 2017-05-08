/*  GRAPHITE2 LICENSING

    Copyright 2015, SIL International
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
#include <cassert>

#include "inc/Decompressor.h"
#include "inc/Compression.h"

using namespace lz4;

namespace {

inline
u32 read_literal(u8 const * &s, u8 const * const e, u32 l) {
    if (l == 15 && s != e)
    {
        u8 b = 0;
        do { l += b = *s++; } while(b==0xff && s != e);
    }
    return l;
}

bool read_sequence(u8 const * &src, u8 const * const end, u8 const * &literal, u32 & literal_len, u32 & match_len, u32 & match_dist)
{
    u8 const token = *src++;
    
    literal_len = read_literal(src, end, token >> 4);
    literal = src;
    src += literal_len;
    
    if (src > end - 2 || src < literal)
        return false;
    
    match_dist  = *src++;
    match_dist |= *src++ << 8;
    match_len = read_literal(src, end, token & 0xf);
    
    return src <= end-5;
}

}

int lz4::decompress(void const *in, size_t in_size, void *out, size_t out_size)
{
    if (out_size <= in_size || in_size < sizeof(unsigned long)+1)
        return -1;
    
    u8 const *       src     = static_cast<u8 const *>(in),
             *       literal = 0,
             * const src_end = src + in_size;

    u8 *       dst     = static_cast<u8*>(out),
       * const dst_end = dst + out_size;
    
    u32 literal_len = 0,
        match_len = 0,
        match_dist = 0;
    
    while (read_sequence(src, src_end, literal, literal_len, match_len, match_dist))
    {
        if (literal_len != 0)
        {
            // Copy in literal. At this point the last full sequence must be at
            // least MINMATCH + 5 from the end of the output buffer.
            if (align(literal_len) > unsigned(dst_end - dst - (MINMATCH+5)) || dst_end - dst < MINMATCH + 5)
                return -1;
            dst = overrun_copy(dst, literal, literal_len);
        }
        
        // Copy, possibly repeating, match from earlier in the
        //  decoded output.
        u8 const * const pcpy = dst - match_dist;
        if (pcpy < static_cast<u8*>(out)
                  || pcpy >= dst
                  || match_len > unsigned(dst_end - dst - (MINMATCH+5))
                  || dst_end - dst < MINMATCH + 5)
            return -1;
        if (dst > pcpy+sizeof(unsigned long) 
            && dst + align(match_len + MINMATCH) <= dst_end)
            dst = overrun_copy(dst, pcpy, match_len + MINMATCH);
        else 
            dst = safe_copy(dst, pcpy, match_len + MINMATCH);
    }
    
    if (literal_len > src_end - literal
              || literal_len > dst_end - dst)
        return -1;
    dst = fast_copy(dst, literal, literal_len);
    
    return dst - (u8*)out;
}

