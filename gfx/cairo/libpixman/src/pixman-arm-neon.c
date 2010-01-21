/*
 * Copyright © 2009 ARM Ltd, Movial Creative Technologies Oy
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of ARM Ltd not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  ARM Ltd makes no
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
 * Author:  Ian Rickards (ian.rickards@arm.com)
 * Author:  Jonathan Morton (jonathan.morton@movial.com)
 * Author:  Markku Vire (markku.vire@movial.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include "pixman-private.h"

#define BIND_SRC_NULL_DST(name, src_type, src_cnt, dst_type, dst_cnt)   \
void                                                                    \
pixman_composite_##name##_asm_neon (int32_t   w,                        \
                                    int32_t   h,                        \
                                    dst_type *dst,                      \
                                    int32_t   dst_stride,               \
                                    src_type *src,                      \
                                    int32_t   src_stride);              \
                                                                        \
static void                                                             \
neon_composite_##name (pixman_implementation_t *imp,                    \
                       pixman_op_t              op,                     \
                       pixman_image_t *         src_image,              \
                       pixman_image_t *         mask_image,             \
                       pixman_image_t *         dst_image,              \
                       int32_t                  src_x,                  \
                       int32_t                  src_y,                  \
                       int32_t                  mask_x,                 \
                       int32_t                  mask_y,                 \
                       int32_t                  dest_x,                 \
                       int32_t                  dest_y,                 \
                       int32_t                  width,                  \
                       int32_t                  height)                 \
{                                                                       \
    dst_type *dst_line;                                                 \
    src_type *src_line;                                                 \
    int32_t dst_stride, src_stride;                                     \
                                                                        \
    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, src_type,           \
                           src_stride, src_line, src_cnt);              \
    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, dst_type,         \
                           dst_stride, dst_line, dst_cnt);              \
                                                                        \
    pixman_composite_##name##_asm_neon (width, height,                  \
                                        dst_line, dst_stride,           \
                                        src_line, src_stride);          \
}

#define BIND_N_NULL_DST(name, dst_type, dst_cnt)                        \
void                                                                    \
pixman_composite_##name##_asm_neon (int32_t    w,                       \
                                    int32_t    h,                       \
                                    dst_type  *dst,                     \
                                    int32_t    dst_stride,              \
                                    uint32_t   src);                    \
                                                                        \
static void                                                             \
neon_composite_##name (pixman_implementation_t *imp,                    \
                       pixman_op_t              op,                     \
                       pixman_image_t *         src_image,              \
                       pixman_image_t *         mask_image,             \
                       pixman_image_t *         dst_image,              \
                       int32_t                  src_x,                  \
                       int32_t                  src_y,                  \
                       int32_t                  mask_x,                 \
                       int32_t                  mask_y,                 \
                       int32_t                  dest_x,                 \
                       int32_t                  dest_y,                 \
                       int32_t                  width,                  \
                       int32_t                  height)                 \
{                                                                       \
    dst_type  *dst_line;                                                \
    int32_t    dst_stride;                                              \
    uint32_t   src;                                                     \
                                                                        \
    src = _pixman_image_get_solid (src_image, dst_image->bits.format);  \
                                                                        \
    if (src == 0)                                                       \
	return;                                                         \
                                                                        \
    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, dst_type,         \
                           dst_stride, dst_line, dst_cnt);              \
                                                                        \
    pixman_composite_##name##_asm_neon (width, height,                  \
                                        dst_line, dst_stride,           \
                                        src);                           \
}

#define BIND_N_MASK_DST(name, mask_type, mask_cnt, dst_type, dst_cnt)   \
void                                                                    \
pixman_composite_##name##_asm_neon (int32_t    w,                       \
                                    int32_t    h,                       \
                                    dst_type  *dst,                     \
                                    int32_t    dst_stride,              \
                                    uint32_t   src,                     \
                                    int32_t    unused,                  \
                                    mask_type *mask,                    \
                                    int32_t    mask_stride);            \
                                                                        \
static void                                                             \
neon_composite_##name (pixman_implementation_t *imp,                    \
                       pixman_op_t              op,                     \
                       pixman_image_t *         src_image,              \
                       pixman_image_t *         mask_image,             \
                       pixman_image_t *         dst_image,              \
                       int32_t                  src_x,                  \
                       int32_t                  src_y,                  \
                       int32_t                  mask_x,                 \
                       int32_t                  mask_y,                 \
                       int32_t                  dest_x,                 \
                       int32_t                  dest_y,                 \
                       int32_t                  width,                  \
                       int32_t                  height)                 \
{                                                                       \
    dst_type  *dst_line;                                                \
    mask_type *mask_line;                                               \
    int32_t    dst_stride, mask_stride;                                 \
    uint32_t   src;                                                     \
                                                                        \
    src = _pixman_image_get_solid (src_image, dst_image->bits.format);  \
                                                                        \
    if (src == 0)                                                       \
	return;                                                         \
                                                                        \
    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, dst_type,         \
                           dst_stride, dst_line, dst_cnt);              \
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, mask_type,       \
                           mask_stride, mask_line, mask_cnt);           \
                                                                        \
    pixman_composite_##name##_asm_neon (width, height,                  \
                                        dst_line, dst_stride,           \
                                        src, 0,                         \
                                        mask_line, mask_stride);        \
}

#define BIND_SRC_N_DST(name, src_type, src_cnt, dst_type, dst_cnt)      \
void                                                                    \
pixman_composite_##name##_asm_neon (int32_t    w,                       \
                                    int32_t    h,                       \
                                    dst_type  *dst,                     \
                                    int32_t    dst_stride,              \
                                    src_type  *src,                     \
                                    int32_t    src_stride,              \
                                    uint32_t   mask);                   \
                                                                        \
static void                                                             \
neon_composite_##name (pixman_implementation_t *imp,                    \
                       pixman_op_t              op,                     \
                       pixman_image_t *         src_image,              \
                       pixman_image_t *         mask_image,             \
                       pixman_image_t *         dst_image,              \
                       int32_t                  src_x,                  \
                       int32_t                  src_y,                  \
                       int32_t                  mask_x,                 \
                       int32_t                  mask_y,                 \
                       int32_t                  dest_x,                 \
                       int32_t                  dest_y,                 \
                       int32_t                  width,                  \
                       int32_t                  height)                 \
{                                                                       \
    dst_type  *dst_line;                                                \
    src_type  *src_line;                                                \
    int32_t    dst_stride, src_stride;                                  \
    uint32_t   mask;                                                    \
                                                                        \
    mask = _pixman_image_get_solid (mask_image, dst_image->bits.format);\
                                                                        \
    if (mask == 0)                                                      \
	return;                                                         \
                                                                        \
    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, dst_type,         \
                           dst_stride, dst_line, dst_cnt);              \
    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, src_type,           \
                           src_stride, src_line, src_cnt);              \
                                                                        \
    pixman_composite_##name##_asm_neon (width, height,                  \
                                        dst_line, dst_stride,           \
                                        src_line, src_stride,           \
                                        mask);                          \
}

#define BIND_SRC_MASK_DST(name, src_type, src_cnt, mask_type, mask_cnt, \
                          dst_type, dst_cnt)                            \
void                                                                    \
pixman_composite_##name##_asm_neon (int32_t    w,                       \
                                    int32_t    h,                       \
                                    dst_type  *dst,                     \
                                    int32_t    dst_stride,              \
                                    src_type  *src,                     \
                                    int32_t    src_stride,              \
                                    mask_type *mask,                    \
                                    int32_t    mask_stride);            \
                                                                        \
static void                                                             \
neon_composite_##name (pixman_implementation_t *imp,                    \
                       pixman_op_t              op,                     \
                       pixman_image_t *         src_image,              \
                       pixman_image_t *         mask_image,             \
                       pixman_image_t *         dst_image,              \
                       int32_t                  src_x,                  \
                       int32_t                  src_y,                  \
                       int32_t                  mask_x,                 \
                       int32_t                  mask_y,                 \
                       int32_t                  dest_x,                 \
                       int32_t                  dest_y,                 \
                       int32_t                  width,                  \
                       int32_t                  height)                 \
{                                                                       \
    dst_type  *dst_line;                                                \
    src_type  *src_line;                                                \
    mask_type *mask_line;                                               \
    int32_t    dst_stride, src_stride, mask_stride;                     \
                                                                        \
    PIXMAN_IMAGE_GET_LINE (dst_image, dest_x, dest_y, dst_type,         \
                           dst_stride, dst_line, dst_cnt);              \
    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, src_type,           \
                           src_stride, src_line, src_cnt);              \
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, mask_type,       \
                           mask_stride, mask_line, mask_cnt);           \
                                                                        \
    pixman_composite_##name##_asm_neon (width, height,                  \
                                        dst_line, dst_stride,           \
                                        src_line, src_stride,           \
                                        mask_line, mask_stride);        \
}


BIND_SRC_NULL_DST(src_8888_8888, uint32_t, 1, uint32_t, 1)
BIND_SRC_NULL_DST(src_0565_0565, uint16_t, 1, uint16_t, 1)
BIND_SRC_NULL_DST(src_0888_0888, uint8_t, 3, uint8_t, 3)
BIND_SRC_NULL_DST(src_8888_0565, uint32_t, 1, uint16_t, 1)
BIND_SRC_NULL_DST(src_0565_8888, uint16_t, 1, uint32_t, 1)
BIND_SRC_NULL_DST(src_0888_8888_rev, uint8_t, 3, uint32_t, 1)
BIND_SRC_NULL_DST(src_0888_0565_rev, uint8_t, 3, uint16_t, 1)
BIND_SRC_NULL_DST(src_pixbuf_8888, uint32_t, 1, uint32_t, 1)
BIND_SRC_NULL_DST(add_8000_8000, uint8_t, 1, uint8_t, 1)
BIND_SRC_NULL_DST(add_8888_8888, uint32_t, 1, uint32_t, 1)

BIND_N_NULL_DST(over_n_0565, uint16_t, 1)
BIND_N_NULL_DST(over_n_8888, uint32_t, 1)

BIND_SRC_NULL_DST(over_8888_0565, uint32_t, 1, uint16_t, 1)
BIND_SRC_NULL_DST(over_8888_8888, uint32_t, 1, uint32_t, 1)

BIND_N_MASK_DST(over_n_8_0565, uint8_t, 1, uint16_t, 1)
BIND_N_MASK_DST(over_n_8_8888, uint8_t, 1, uint32_t, 1)
BIND_N_MASK_DST(add_n_8_8, uint8_t, 1, uint8_t, 1)

BIND_SRC_N_DST(over_8888_n_8888, uint32_t, 1, uint32_t, 1)

BIND_SRC_MASK_DST(add_8_8_8, uint8_t, 1, uint8_t, 1, uint8_t, 1)
BIND_SRC_MASK_DST(add_8888_8888_8888, uint32_t, 1, uint32_t, 1, uint32_t, 1)
BIND_SRC_MASK_DST(over_8888_8_8888, uint32_t, 1, uint8_t, 1, uint32_t, 1)
BIND_SRC_MASK_DST(over_8888_8888_8888, uint32_t, 1, uint32_t, 1, uint32_t, 1)

void
pixman_composite_src_n_8_asm_neon (int32_t   w,
                                   int32_t   h,
                                   uint8_t  *dst,
                                   int32_t   dst_stride,
                                   uint8_t   src);

void
pixman_composite_src_n_0565_asm_neon (int32_t   w,
                                      int32_t   h,
                                      uint16_t *dst,
                                      int32_t   dst_stride,
                                      uint16_t  src);

void
pixman_composite_src_n_8888_asm_neon (int32_t   w,
                                      int32_t   h,
                                      uint32_t *dst,
                                      int32_t   dst_stride,
                                      uint32_t  src);

static pixman_bool_t
pixman_fill_neon (uint32_t *bits,
                  int       stride,
                  int       bpp,
                  int       x,
                  int       y,
                  int       width,
                  int       height,
                  uint32_t  _xor)
{
    /* stride is always multiple of 32bit units in pixman */
    uint32_t byte_stride = stride * sizeof(uint32_t);

    switch (bpp)
    {
    case 8:
	pixman_composite_src_n_8_asm_neon (
		width,
		height,
		(uint8_t *)(((char *) bits) + y * byte_stride + x),
		byte_stride,
		_xor & 0xff);
	return TRUE;
    case 16:
	pixman_composite_src_n_0565_asm_neon (
		width,
		height,
		(uint16_t *)(((char *) bits) + y * byte_stride + x * 2),
		byte_stride / 2,
		_xor & 0xffff);
	return TRUE;
    case 32:
	pixman_composite_src_n_8888_asm_neon (
		width,
		height,
		(uint32_t *)(((char *) bits) + y * byte_stride + x * 4),
		byte_stride / 4,
		_xor);
	return TRUE;
    default:
	return FALSE;
    }
}

static pixman_bool_t
pixman_blt_neon (uint32_t *src_bits,
                 uint32_t *dst_bits,
                 int       src_stride,
                 int       dst_stride,
                 int       src_bpp,
                 int       dst_bpp,
                 int       src_x,
                 int       src_y,
                 int       dst_x,
                 int       dst_y,
                 int       width,
                 int       height)
{
    if (src_bpp != dst_bpp)
	return FALSE;

    switch (src_bpp)
    {
    case 16:
	pixman_composite_src_0565_0565_asm_neon (
		width, height,
		(uint16_t *)(((char *) dst_bits) +
		dst_y * dst_stride * 4 + dst_x * 2), dst_stride * 2,
		(uint16_t *)(((char *) src_bits) +
		src_y * src_stride * 4 + src_x * 2), src_stride * 2);
	return TRUE;
    case 32:
	pixman_composite_src_8888_8888_asm_neon (
		width, height,
		(uint32_t *)(((char *) dst_bits) +
		dst_y * dst_stride * 4 + dst_x * 4), dst_stride,
		(uint32_t *)(((char *) src_bits) +
		src_y * src_stride * 4 + src_x * 4), src_stride);
	return TRUE;
    default:
	return FALSE;
    }
}

static const pixman_fast_path_t arm_neon_fast_path_array[] =
{
    { PIXMAN_OP_SRC,  PIXMAN_r5g6b5,   PIXMAN_null,     PIXMAN_r5g6b5,   neon_composite_src_0565_0565    },
    { PIXMAN_OP_SRC,  PIXMAN_b5g6r5,   PIXMAN_null,     PIXMAN_b5g6r5,   neon_composite_src_0565_0565    },
    { PIXMAN_OP_SRC,  PIXMAN_a8r8g8b8, PIXMAN_null,     PIXMAN_r5g6b5,   neon_composite_src_8888_0565    },
    { PIXMAN_OP_SRC,  PIXMAN_x8r8g8b8, PIXMAN_null,     PIXMAN_r5g6b5,   neon_composite_src_8888_0565    },
    { PIXMAN_OP_SRC,  PIXMAN_a8b8g8r8, PIXMAN_null,     PIXMAN_b5g6r5,   neon_composite_src_8888_0565    },
    { PIXMAN_OP_SRC,  PIXMAN_x8b8g8r8, PIXMAN_null,     PIXMAN_b5g6r5,   neon_composite_src_8888_0565    },
    { PIXMAN_OP_SRC,  PIXMAN_r5g6b5,   PIXMAN_null,     PIXMAN_a8r8g8b8, neon_composite_src_0565_8888    },
    { PIXMAN_OP_SRC,  PIXMAN_r5g6b5,   PIXMAN_null,     PIXMAN_x8r8g8b8, neon_composite_src_0565_8888    },
    { PIXMAN_OP_SRC,  PIXMAN_b5g6r5,   PIXMAN_null,     PIXMAN_a8b8g8r8, neon_composite_src_0565_8888    },
    { PIXMAN_OP_SRC,  PIXMAN_b5g6r5,   PIXMAN_null,     PIXMAN_x8b8g8r8, neon_composite_src_0565_8888    },
    { PIXMAN_OP_SRC,  PIXMAN_a8r8g8b8, PIXMAN_null,     PIXMAN_x8r8g8b8, neon_composite_src_8888_8888    },
    { PIXMAN_OP_SRC,  PIXMAN_x8r8g8b8, PIXMAN_null,     PIXMAN_x8r8g8b8, neon_composite_src_8888_8888    },
    { PIXMAN_OP_SRC,  PIXMAN_a8b8g8r8, PIXMAN_null,     PIXMAN_x8b8g8r8, neon_composite_src_8888_8888    },
    { PIXMAN_OP_SRC,  PIXMAN_x8b8g8r8, PIXMAN_null,     PIXMAN_x8b8g8r8, neon_composite_src_8888_8888    },
    { PIXMAN_OP_SRC,  PIXMAN_r8g8b8,   PIXMAN_null,     PIXMAN_r8g8b8,   neon_composite_src_0888_0888    },
    { PIXMAN_OP_SRC,  PIXMAN_b8g8r8,   PIXMAN_null,     PIXMAN_x8r8g8b8, neon_composite_src_0888_8888_rev },
    { PIXMAN_OP_SRC,  PIXMAN_b8g8r8,   PIXMAN_null,     PIXMAN_r5g6b5,   neon_composite_src_0888_0565_rev },
    { PIXMAN_OP_SRC,  PIXMAN_pixbuf,   PIXMAN_pixbuf,   PIXMAN_a8r8g8b8, neon_composite_src_pixbuf_8888  },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_r5g6b5,   neon_composite_over_n_8_0565    },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_b5g6r5,   neon_composite_over_n_8_0565    },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_a8r8g8b8, neon_composite_over_n_8_8888    },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_x8r8g8b8, neon_composite_over_n_8_8888    },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_a8b8g8r8, neon_composite_over_n_8_8888    },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_a8,       PIXMAN_x8b8g8r8, neon_composite_over_n_8_8888    },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_null,     PIXMAN_r5g6b5,   neon_composite_over_n_0565      },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_null,     PIXMAN_a8r8g8b8, neon_composite_over_n_8888      },
    { PIXMAN_OP_OVER, PIXMAN_solid,    PIXMAN_null,     PIXMAN_x8r8g8b8, neon_composite_over_n_8888      },
    { PIXMAN_OP_OVER, PIXMAN_a8r8g8b8, PIXMAN_solid,    PIXMAN_a8r8g8b8, neon_composite_over_8888_n_8888 },
    { PIXMAN_OP_OVER, PIXMAN_a8r8g8b8, PIXMAN_solid,    PIXMAN_x8r8g8b8, neon_composite_over_8888_n_8888 },
    { PIXMAN_OP_OVER, PIXMAN_a8r8g8b8, PIXMAN_a8,       PIXMAN_a8r8g8b8, neon_composite_over_8888_8_8888 },
    { PIXMAN_OP_OVER, PIXMAN_a8r8g8b8, PIXMAN_a8,       PIXMAN_x8r8g8b8, neon_composite_over_8888_8_8888 },
    { PIXMAN_OP_OVER, PIXMAN_a8b8g8r8, PIXMAN_a8,       PIXMAN_a8b8g8r8, neon_composite_over_8888_8_8888 },
    { PIXMAN_OP_OVER, PIXMAN_a8b8g8r8, PIXMAN_a8,       PIXMAN_x8b8g8r8, neon_composite_over_8888_8_8888 },
    { PIXMAN_OP_OVER, PIXMAN_a8r8g8b8, PIXMAN_a8r8g8b8, PIXMAN_a8r8g8b8, neon_composite_over_8888_8888_8888 },
    { PIXMAN_OP_OVER, PIXMAN_a8r8g8b8, PIXMAN_null,     PIXMAN_r5g6b5,   neon_composite_over_8888_0565   },
    { PIXMAN_OP_OVER, PIXMAN_a8b8g8r8, PIXMAN_null,     PIXMAN_b5g6r5,   neon_composite_over_8888_0565   },
    { PIXMAN_OP_OVER, PIXMAN_a8r8g8b8, PIXMAN_null,     PIXMAN_a8r8g8b8, neon_composite_over_8888_8888   },
    { PIXMAN_OP_OVER, PIXMAN_a8r8g8b8, PIXMAN_null,     PIXMAN_x8r8g8b8, neon_composite_over_8888_8888   },
    { PIXMAN_OP_OVER, PIXMAN_a8b8g8r8, PIXMAN_null,     PIXMAN_a8b8g8r8, neon_composite_over_8888_8888   },
    { PIXMAN_OP_OVER, PIXMAN_a8b8g8r8, PIXMAN_null,     PIXMAN_x8b8g8r8, neon_composite_over_8888_8888   },
    { PIXMAN_OP_ADD,  PIXMAN_solid,    PIXMAN_a8,       PIXMAN_a8,       neon_composite_add_n_8_8        },
    { PIXMAN_OP_ADD,  PIXMAN_a8,       PIXMAN_a8,       PIXMAN_a8,       neon_composite_add_8_8_8        },
    { PIXMAN_OP_ADD,  PIXMAN_a8r8g8b8, PIXMAN_a8r8g8b8, PIXMAN_a8r8g8b8, neon_composite_add_8888_8888_8888 },
    { PIXMAN_OP_ADD,  PIXMAN_a8,       PIXMAN_null,     PIXMAN_a8,       neon_composite_add_8000_8000    },
    { PIXMAN_OP_ADD,  PIXMAN_a8r8g8b8, PIXMAN_null,     PIXMAN_a8r8g8b8, neon_composite_add_8888_8888    },
    { PIXMAN_OP_ADD,  PIXMAN_a8b8g8r8, PIXMAN_null,     PIXMAN_a8b8g8r8, neon_composite_add_8888_8888    },
    { PIXMAN_OP_NONE },
};

const pixman_fast_path_t *const arm_neon_fast_paths = arm_neon_fast_path_array;

static void
arm_neon_composite (pixman_implementation_t *imp,
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
    if (_pixman_run_fast_path (arm_neon_fast_paths, imp,
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

static pixman_bool_t
arm_neon_blt (pixman_implementation_t *imp,
              uint32_t *               src_bits,
              uint32_t *               dst_bits,
              int                      src_stride,
              int                      dst_stride,
              int                      src_bpp,
              int                      dst_bpp,
              int                      src_x,
              int                      src_y,
              int                      dst_x,
              int                      dst_y,
              int                      width,
              int                      height)
{
    if (!pixman_blt_neon (
            src_bits, dst_bits, src_stride, dst_stride, src_bpp, dst_bpp,
            src_x, src_y, dst_x, dst_y, width, height))

    {
	return _pixman_implementation_blt (
	    imp->delegate,
	    src_bits, dst_bits, src_stride, dst_stride, src_bpp, dst_bpp,
	    src_x, src_y, dst_x, dst_y, width, height);
    }

    return TRUE;
}

static pixman_bool_t
arm_neon_fill (pixman_implementation_t *imp,
               uint32_t *               bits,
               int                      stride,
               int                      bpp,
               int                      x,
               int                      y,
               int                      width,
               int                      height,
               uint32_t xor)
{
    if (pixman_fill_neon (bits, stride, bpp, x, y, width, height, xor))
	return TRUE;

    return _pixman_implementation_fill (
	imp->delegate, bits, stride, bpp, x, y, width, height, xor);
}

#define BIND_COMBINE_U(name)                                             \
void                                                                     \
pixman_composite_scanline_##name##_mask_asm_neon (int32_t         w,     \
                                                  const uint32_t *dst,   \
                                                  const uint32_t *src,   \
                                                  const uint32_t *mask); \
                                                                         \
void                                                                     \
pixman_composite_scanline_##name##_asm_neon (int32_t         w,          \
                                             const uint32_t *dst,        \
                                             const uint32_t *src);       \
                                                                         \
static void                                                              \
neon_combine_##name##_u (pixman_implementation_t *imp,                   \
                         pixman_op_t              op,                    \
                         uint32_t *               dest,                  \
                         const uint32_t *         src,                   \
                         const uint32_t *         mask,                  \
                         int                      width)                 \
{                                                                        \
    if (mask)                                                            \
	pixman_composite_scanline_##name##_mask_asm_neon (width, dest,   \
	                                                  src, mask);    \
    else                                                                 \
	pixman_composite_scanline_##name##_asm_neon (width, dest, src);  \
}

BIND_COMBINE_U (over)
BIND_COMBINE_U (add)

pixman_implementation_t *
_pixman_implementation_create_arm_neon (void)
{
    pixman_implementation_t *general = _pixman_implementation_create_fast_path ();
    pixman_implementation_t *imp = _pixman_implementation_create (general);

    imp->combine_32[PIXMAN_OP_OVER] = neon_combine_over_u;
    imp->combine_32[PIXMAN_OP_ADD] = neon_combine_add_u;

    imp->composite = arm_neon_composite;
    imp->blt = arm_neon_blt;
    imp->fill = arm_neon_fill;

    return imp;
}
