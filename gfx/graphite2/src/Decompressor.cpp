/*  Copyright (c) 2012, Siyuan Fu <fusiyuan2010@gmail.com>
    Copyright (c) 2015, SIL International
    All rights reserved.
    
    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    
    1. Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.
    
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    
    3. Neither the name of the copyright holder nor the names of its
       contributors may be used to endorse or promote products derived from
       this software without specific prior written permission.
    
    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
    POSSIBILITY OF SUCH DAMAGE.
*/
#include <cassert>

#include "inc/Decompressor.h"
#include "inc/Shrinker.h"

using namespace shrinker;

namespace {

u8 const LONG_DIST = 0x10;
u8 const MATCH_LEN = 0x0f;

template <int M>
inline
u32 read_literal(u8 const * &s, u8 const * const e, u32 l) {
    if (unlikely(l == M))
    {
        u8 b = 0; 
        do { l += b = *s++; } while(b==0xff && s != e);
    }
    return l;
}

bool read_directive(u8 const * &src, u8 const * const end, u32 & literal_len, u32 & match_len, u32 & match_dist)
{
    u8 const flag = *src++;
    
    literal_len = read_literal<7>(src, end, flag >> 5);
    match_len = read_literal<15>(src, end, flag & MATCH_LEN);
    
    match_dist = *src++;
    if (flag & LONG_DIST) 
        match_dist |= ((*src++) << 8);
    
    return match_dist != 0xffff;
}

}

int shrinker::decompress(void const *in, size_t in_size, void *out, size_t out_size)
{
    u8 const *       src     = static_cast<u8 const *>(in),
             * const src_end = src + in_size;

    u8 *       dst     = static_cast<u8*>(out),
       * const dst_end = dst + out_size;
    
    u32 literal_len = 0,
        match_len = 0,
        match_dist = 0;
        
    while (read_directive(src, src_end, literal_len, match_len, match_dist))
    {
        // Copy in literal
        if (unlikely(dst + literal_len + sizeof(unsigned long) > dst_end)) return -1;
        dst = memcpy_nooverlap(dst, src, literal_len);
        src += literal_len;
        
        // Copy, possibly repeating, match from earlier in the
        //  decoded output.
        u8 const * const pcpy = dst - match_dist - 1;
        if (unlikely(pcpy < static_cast<u8*>(out) 
                  || dst + match_len + MINMATCH  + sizeof(unsigned long) > dst_end)) return -1;
        dst = memcpy_(dst, pcpy, match_len + MINMATCH);
    }
    
    if (unlikely(dst + literal_len > dst_end)) return -1;
    dst = memcpy_nooverlap_surpass(dst, src, literal_len);
    
    return dst - (u8*)out;
}

