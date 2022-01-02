/*
   Copyright (C) Intel Corp.  2006.  All Rights Reserved.
   Intel funded Tungsten Graphics (http://www.tungstengraphics.com) to
   develop this 3D driver.

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice (including the
   next paragraph) shall be included in all copies or substantial
   portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
   LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
   OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
   WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 **********************************************************************/
/*
 * Authors:
 *   Keith Whitwell <keith@tungstengraphics.com>
 */


#include "cairoint.h"
#include "cairo-drm-intel-brw-eu.h"


void brw_math_invert( struct brw_compile *p,
		      struct brw_reg dst,
		      struct brw_reg src)
{
    brw_math( p,
	      dst,
	      BRW_MATH_FUNCTION_INV,
	      BRW_MATH_SATURATE_NONE,
	      0,
	      src,
	      BRW_MATH_PRECISION_FULL,
	      BRW_MATH_DATA_VECTOR );
}



void brw_copy4(struct brw_compile *p,
	       struct brw_reg dst,
	       struct brw_reg src,
	       uint32_t count)
{
    uint32_t i;

    dst = vec4(dst);
    src = vec4(src);

    for (i = 0; i < count; i++)
    {
	uint32_t delta = i*32;
	brw_MOV(p, byte_offset(dst, delta),    byte_offset(src, delta));
	brw_MOV(p, byte_offset(dst, delta+16), byte_offset(src, delta+16));
    }
}


void brw_copy8(struct brw_compile *p,
	       struct brw_reg dst,
	       struct brw_reg src,
	       uint32_t count)
{
    uint32_t i;

    dst = vec8(dst);
    src = vec8(src);

    for (i = 0; i < count; i++)
    {
	uint32_t delta = i*32;
	brw_MOV(p, byte_offset(dst, delta),    byte_offset(src, delta));
    }
}


void brw_copy_indirect_to_indirect(struct brw_compile *p,
				   struct brw_indirect dst_ptr,
				   struct brw_indirect src_ptr,
				   uint32_t count)
{
    uint32_t i;

    for (i = 0; i < count; i++)
    {
	uint32_t delta = i*32;
	brw_MOV(p, deref_4f(dst_ptr, delta),    deref_4f(src_ptr, delta));
	brw_MOV(p, deref_4f(dst_ptr, delta+16), deref_4f(src_ptr, delta+16));
    }
}


void brw_copy_from_indirect(struct brw_compile *p,
			    struct brw_reg dst,
			    struct brw_indirect ptr,
			    uint32_t count)
{
    uint32_t i;

    dst = vec4(dst);

    for (i = 0; i < count; i++)
    {
	uint32_t delta = i*32;
	brw_MOV(p, byte_offset(dst, delta),    deref_4f(ptr, delta));
	brw_MOV(p, byte_offset(dst, delta+16), deref_4f(ptr, delta+16));
    }
}
