/*
 * Copyright © 2008 Mozilla Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Mozilla Corporation not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Mozilla Corporation makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 *
 * Author:  Jeff Muizelaar (jeff@infidigm.net)
 *
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "pixman-private.h"

static void
arm_composite_add_8000_8000 (pixman_implementation_t * impl,
			     pixman_op_t               op,
			     pixman_image_t *          src_image,
			     pixman_image_t *          mask_image,
			     pixman_image_t *          dst_image,
			     int32_t                   src_x,
			     int32_t                   src_y,
			     int32_t                   mask_x,
			     int32_t                   mask_y,
			     int32_t                   dest_x,
			     int32_t                   dest_y,
			     int32_t                   width,
			     int32_t                   height)
{
    uint8_t     *dst_line, *dst;
    uint8_t     *src_line, *src;
    int dst_stride, src_stride;
    uint16_t w;
    uint8_t s, d;

    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint8_t, src_stride, src_line, 1);
    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint8_t, dst_stride, dst_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	/* ensure both src and dst are properly aligned before doing 32 bit reads
	 * we'll stay in this loop if src and dst have differing alignments
	 */
	while (w && (((unsigned long)dst & 3) || ((unsigned long)src & 3)))
	{
	    s = *src;
	    d = *dst;
	    asm ("uqadd8 %0, %1, %2" : "+r" (d) : "r" (s));
	    *dst = d;

	    dst++;
	    src++;
	    w--;
	}

	while (w >= 4)
	{
	    asm ("uqadd8 %0, %1, %2"
		 : "=r" (*(uint32_t*)dst)
		 : "r" (*(uint32_t*)src), "r" (*(uint32_t*)dst));
	    dst += 4;
	    src += 4;
	    w -= 4;
	}

	while (w)
	{
	    s = *src;
	    d = *dst;
	    asm ("uqadd8 %0, %1, %2" : "+r" (d) : "r" (s));
	    *dst = d;

	    dst++;
	    src++;
	    w--;
	}
    }

}

static void
arm_composite_over_8888_8888 (pixman_implementation_t * impl,
			      pixman_op_t               op,
			      pixman_image_t *          src_image,
			      pixman_image_t *          mask_image,
			      pixman_image_t *          dst_image,
			      int32_t                   src_x,
			      int32_t                   src_y,
			      int32_t                   mask_x,
			      int32_t                   mask_y,
			      int32_t                   dest_x,
			      int32_t                   dest_y,
			      int32_t                   width,
			      int32_t                   height)
{
    uint32_t    *dst_line, *dst;
    uint32_t    *src_line, *src;
    int dst_stride, src_stride;
    uint16_t w;
    uint32_t component_half = 0x800080;
    uint32_t upper_component_mask = 0xff00ff00;
    uint32_t alpha_mask = 0xff;

    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint32_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint32_t, src_stride, src_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

/* #define inner_branch */
	asm volatile (
	    "cmp %[w], #0\n\t"
	    "beq 2f\n\t"
	    "1:\n\t"
	    /* load src */
	    "ldr r5, [%[src]], #4\n\t"
#ifdef inner_branch
	    /* We can avoid doing the multiplication in two cases: 0x0 or 0xff.
	     * The 0x0 case also allows us to avoid doing an unecessary data
	     * write which is more valuable so we only check for that
	     */
	    "cmp r5, #0\n\t"
	    "beq 3f\n\t"

	    /* = 255 - alpha */
	    "sub r8, %[alpha_mask], r5, lsr #24\n\t"

	    "ldr r4, [%[dest]] \n\t"

#else
	    "ldr r4, [%[dest]] \n\t"

	    /* = 255 - alpha */
	    "sub r8, %[alpha_mask], r5, lsr #24\n\t"
#endif
	    "uxtb16 r6, r4\n\t"
	    "uxtb16 r7, r4, ror #8\n\t"

	    /* multiply by 257 and divide by 65536 */
	    "mla r6, r6, r8, %[component_half]\n\t"
	    "mla r7, r7, r8, %[component_half]\n\t"

	    "uxtab16 r6, r6, r6, ror #8\n\t"
	    "uxtab16 r7, r7, r7, ror #8\n\t"

	    /* recombine the 0xff00ff00 bytes of r6 and r7 */
	    "and r7, r7, %[upper_component_mask]\n\t"
	    "uxtab16 r6, r7, r6, ror #8\n\t"

	    "uqadd8 r5, r6, r5\n\t"

#ifdef inner_branch
	    "3:\n\t"

#endif
	    "str r5, [%[dest]], #4\n\t"
	    /* increment counter and jmp to top */
	    "subs	%[w], %[w], #1\n\t"
	    "bne	1b\n\t"
	    "2:\n\t"
	    : [w] "+r" (w), [dest] "+r" (dst), [src] "+r" (src)
	    : [component_half] "r" (component_half), [upper_component_mask] "r" (upper_component_mask),
	      [alpha_mask] "r" (alpha_mask)
	    : "r4", "r5", "r6", "r7", "r8", "cc", "memory"
	    );
    }
}

static void
arm_composite_over_8888_n_8888 (pixman_implementation_t * impl,
				pixman_op_t               op,
				pixman_image_t *          src_image,
				pixman_image_t *          mask_image,
				pixman_image_t *          dst_image,
				int32_t                   src_x,
				int32_t                   src_y,
				int32_t                   mask_x,
				int32_t                   mask_y,
				int32_t                   dest_x,
				int32_t                   dest_y,
				int32_t                   width,
				int32_t                   height)
{
    uint32_t *dst_line, *dst;
    uint32_t *src_line, *src;
    uint32_t mask;
    int dst_stride, src_stride;
    uint16_t w;
    uint32_t component_half = 0x800080;
    uint32_t alpha_mask = 0xff;

    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint32_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, uint32_t, src_stride, src_line, 1);

    mask = _pixman_image_get_solid (mask_image, PIXMAN_a8r8g8b8);
    mask = (mask) >> 24;

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

/* #define inner_branch */
	asm volatile (
	    "cmp %[w], #0\n\t"
	    "beq 2f\n\t"
	    "1:\n\t"
	    /* load src */
	    "ldr r5, [%[src]], #4\n\t"
#ifdef inner_branch
	    /* We can avoid doing the multiplication in two cases: 0x0 or 0xff.
	     * The 0x0 case also allows us to avoid doing an unecessary data
	     * write which is more valuable so we only check for that
	     */
	    "cmp r5, #0\n\t"
	    "beq 3f\n\t"

#endif
	    "ldr r4, [%[dest]] \n\t"

	    "uxtb16 r6, r5\n\t"
	    "uxtb16 r7, r5, ror #8\n\t"

	    /* multiply by alpha (r8) then by 257 and divide by 65536 */
	    "mla r6, r6, %[mask_alpha], %[component_half]\n\t"
	    "mla r7, r7, %[mask_alpha], %[component_half]\n\t"

	    "uxtab16 r6, r6, r6, ror #8\n\t"
	    "uxtab16 r7, r7, r7, ror #8\n\t"

	    "uxtb16 r6, r6, ror #8\n\t"
	    "uxtb16 r7, r7, ror #8\n\t"

	    /* recombine */
	    "orr r5, r6, r7, lsl #8\n\t"

	    "uxtb16 r6, r4\n\t"
	    "uxtb16 r7, r4, ror #8\n\t"

	    /* 255 - alpha */
	    "sub r8, %[alpha_mask], r5, lsr #24\n\t"

	    /* multiply by alpha (r8) then by 257 and divide by 65536 */
	    "mla r6, r6, r8, %[component_half]\n\t"
	    "mla r7, r7, r8, %[component_half]\n\t"

	    "uxtab16 r6, r6, r6, ror #8\n\t"
	    "uxtab16 r7, r7, r7, ror #8\n\t"

	    "uxtb16 r6, r6, ror #8\n\t"
	    "uxtb16 r7, r7, ror #8\n\t"

	    /* recombine */
	    "orr r6, r6, r7, lsl #8\n\t"

	    "uqadd8 r5, r6, r5\n\t"

#ifdef inner_branch
	    "3:\n\t"

#endif
	    "str r5, [%[dest]], #4\n\t"
	    /* increment counter and jmp to top */
	    "subs	%[w], %[w], #1\n\t"
	    "bne	1b\n\t"
	    "2:\n\t"
	    : [w] "+r" (w), [dest] "+r" (dst), [src] "+r" (src)
	    : [component_half] "r" (component_half), [mask_alpha] "r" (mask),
	      [alpha_mask] "r" (alpha_mask)
	    : "r4", "r5", "r6", "r7", "r8", "r9", "cc", "memory"
	    );
    }
}

static void
arm_composite_over_n_8_8888 (pixman_implementation_t * impl,
			     pixman_op_t               op,
			     pixman_image_t *          src_image,
			     pixman_image_t *          mask_image,
			     pixman_image_t *          dst_image,
			     int32_t                   src_x,
			     int32_t                   src_y,
			     int32_t                   mask_x,
			     int32_t                   mask_y,
			     int32_t                   dest_x,
			     int32_t                   dest_y,
			     int32_t                   width,
			     int32_t                   height)
{
    uint32_t src, srca;
    uint32_t *dst_line, *dst;
    uint8_t  *mask_line, *mask;
    int dst_stride, mask_stride;
    uint16_t w;

    src = _pixman_image_get_solid (src_image, dst_image->bits.format);

    /* bail out if fully transparent */
    srca = src >> 24;
    if (src == 0)
	return;

    uint32_t component_mask = 0xff00ff;
    uint32_t component_half = 0x800080;

    uint32_t src_hi = (src >> 8) & component_mask;
    uint32_t src_lo = src & component_mask;

    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, uint32_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, uint8_t, mask_stride, mask_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;
	w = width;

/* #define inner_branch */
	asm volatile (
	    "cmp %[w], #0\n\t"
	    "beq 2f\n\t"
	    "1:\n\t"
	    /* load mask */
	    "ldrb r5, [%[mask]], #1\n\t"
#ifdef inner_branch
	    /* We can avoid doing the multiplication in two cases: 0x0 or 0xff.
	     * The 0x0 case also allows us to avoid doing an unecessary data
	     * write which is more valuable so we only check for that
	     */
	    "cmp r5, #0\n\t"
	    "beq 3f\n\t"

#endif
	    "ldr r4, [%[dest]] \n\t"

	    /* multiply by alpha (r8) then by 257 and divide by 65536 */
	    "mla r6, %[src_lo], r5, %[component_half]\n\t"
	    "mla r7, %[src_hi], r5, %[component_half]\n\t"

	    "uxtab16 r6, r6, r6, ror #8\n\t"
	    "uxtab16 r7, r7, r7, ror #8\n\t"

	    "uxtb16 r6, r6, ror #8\n\t"
	    "uxtb16 r7, r7, ror #8\n\t"

	    /* recombine */
	    "orr r5, r6, r7, lsl #8\n\t"

	    "uxtb16 r6, r4\n\t"
	    "uxtb16 r7, r4, ror #8\n\t"

	    /* we could simplify this to use 'sub' if we were
	     * willing to give up a register for alpha_mask
	     */
	    "mvn r8, r5\n\t"
	    "mov r8, r8, lsr #24\n\t"

	    /* multiply by alpha (r8) then by 257 and divide by 65536 */
	    "mla r6, r6, r8, %[component_half]\n\t"
	    "mla r7, r7, r8, %[component_half]\n\t"

	    "uxtab16 r6, r6, r6, ror #8\n\t"
	    "uxtab16 r7, r7, r7, ror #8\n\t"

	    "uxtb16 r6, r6, ror #8\n\t"
	    "uxtb16 r7, r7, ror #8\n\t"

	    /* recombine */
	    "orr r6, r6, r7, lsl #8\n\t"

	    "uqadd8 r5, r6, r5\n\t"

#ifdef inner_branch
	    "3:\n\t"

#endif
	    "str r5, [%[dest]], #4\n\t"
	    /* increment counter and jmp to top */
	    "subs	%[w], %[w], #1\n\t"
	    "bne	1b\n\t"
	    "2:\n\t"
	    : [w] "+r" (w), [dest] "+r" (dst), [src] "+r" (src), [mask] "+r" (mask)
	    : [component_half] "r" (component_half),
	      [src_hi] "r" (src_hi), [src_lo] "r" (src_lo)
	    : "r4", "r5", "r6", "r7", "r8", "cc", "memory");
    }
}

static const pixman_fast_path_t arm_simd_fast_path_array[] =
{
    { PIXMAN_OP_OVER, PIXMAN_a8r8g8b8, PIXMAN_null,     PIXMAN_a8r8g8b8, arm_composite_over_8888_8888    },
    { PIXMAN_OP_OVER, PIXMAN_a8r8g8b8, PIXMAN_null,     PIXMAN_x8r8g8b8, arm_composite_over_8888_8888    },
    { PIXMAN_OP_OVER, PIXMAN_a8b8g8r8, PIXMAN_null,     PIXMAN_a8b8g8r8, arm_composite_over_8888_8888    },
    { PIXMAN_OP_OVER, PIXMAN_a8b8g8r8, PIXMAN_null,     PIXMAN_x8b8g8r8, arm_composite_over_8888_8888    },
    { PIXMAN_OP_OVER, PIXMAN_a8r8g8b8, PIXMAN_solid,    PIXMAN_a8r8g8b8, arm_composite_over_8888_n_8888  },
    { PIXMAN_OP_OVER, PIXMAN_a8r8g8b8, PIXMAN_solid,    PIXMAN_x8r8g8b8, arm_composite_over_8888_n_8888  },

    { PIXMAN_OP_ADD, PIXMAN_a8,        PIXMAN_null,     PIXMAN_a8,       arm_composite_add_8000_8000     },

    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_a8r8g8b8, arm_composite_over_n_8_8888     },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_x8r8g8b8, arm_composite_over_n_8_8888     },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_a8b8g8r8, arm_composite_over_n_8_8888     },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_x8b8g8r8, arm_composite_over_n_8_8888     },

    { PIXMAN_OP_NONE },
};

const pixman_fast_path_t *const arm_simd_fast_paths = arm_simd_fast_path_array;

static void
arm_simd_composite (pixman_implementation_t *imp,
                    pixman_op_t              op,
                    pixman_image_t *         src,
                    pixman_image_t *         mask,
                    pixman_image_t *         dest,
                    int32_t                  src_x,
                    int32_t                  src_y,
                    int32_t                  mask_x,
                    int32_t                  mask_y,
                    int32_t                  dest_x,
                    int32_t                  dest_y,
                    int32_t                  width,
                    int32_t                  height)
{
    if (_pixman_run_fast_path (arm_simd_fast_paths, imp,
                               op, src, mask, dest,
                               src_x, src_y,
                               mask_x, mask_y,
                               dest_x, dest_y,
                               width, height))
    {
	return;
    }

    _pixman_implementation_composite (imp->delegate, op,
                                      src, mask, dest,
                                      src_x, src_y,
                                      mask_x, mask_y,
                                      dest_x, dest_y,
                                      width, height);
}

pixman_implementation_t *
_pixman_implementation_create_arm_simd (void)
{
    pixman_implementation_t *general = _pixman_implementation_create_fast_path ();
    pixman_implementation_t *imp = _pixman_implementation_create (general);

    imp->composite = arm_simd_composite;

    return imp;
}
