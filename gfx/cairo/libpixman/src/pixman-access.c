/*
 *
 * Copyright © 2000 Keith Packard, member of The XFree86 Project, Inc.
 *             2005 Lars Knoll & Zack Rusin, Trolltech
 *             2008 Aaron Plattner, NVIDIA Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "pixman-private.h"
#include "pixman-accessor.h"

#define CONVERT_RGB24_TO_Y15(s)						\
    (((((s) >> 16) & 0xff) * 153 +					\
      (((s) >>  8) & 0xff) * 301 +					\
      (((s)      ) & 0xff) * 58) >> 2)

#define CONVERT_RGB24_TO_RGB15(s)                                       \
    ((((s) >> 3) & 0x001f) |                                            \
     (((s) >> 6) & 0x03e0) |                                            \
     (((s) >> 9) & 0x7c00))

/* Fetch macros */

#ifdef WORDS_BIGENDIAN
#define FETCH_1(img,l,o)						\
    (((READ ((img), ((uint32_t *)(l)) + ((o) >> 5))) >> (0x1f - ((o) & 0x1f))) & 0x1)
#else
#define FETCH_1(img,l,o)						\
    ((((READ ((img), ((uint32_t *)(l)) + ((o) >> 5))) >> ((o) & 0x1f))) & 0x1)
#endif

#define FETCH_8(img,l,o)    (READ (img, (((uint8_t *)(l)) + ((o) >> 3))))

#ifdef WORDS_BIGENDIAN
#define FETCH_4(img,l,o)						\
    (((4 * (o)) & 4) ? (FETCH_8 (img,l, 4 * (o)) & 0xf) : (FETCH_8 (img,l,(4 * (o))) >> 4))
#else
#define FETCH_4(img,l,o)						\
    (((4 * (o)) & 4) ? (FETCH_8 (img, l, 4 * (o)) >> 4) : (FETCH_8 (img, l, (4 * (o))) & 0xf))
#endif

#ifdef WORDS_BIGENDIAN
#define FETCH_24(img,l,o)                                              \
    ((READ (img, (((uint8_t *)(l)) + ((o) * 3) + 0)) << 16)    |       \
     (READ (img, (((uint8_t *)(l)) + ((o) * 3) + 1)) << 8)     |       \
     (READ (img, (((uint8_t *)(l)) + ((o) * 3) + 2)) << 0))
#else
#define FETCH_24(img,l,o)						\
    ((READ (img, (((uint8_t *)(l)) + ((o) * 3) + 0)) << 0)	|	\
     (READ (img, (((uint8_t *)(l)) + ((o) * 3) + 1)) << 8)	|	\
     (READ (img, (((uint8_t *)(l)) + ((o) * 3) + 2)) << 16))
#endif

/* Store macros */

#ifdef WORDS_BIGENDIAN
#define STORE_1(img,l,o,v)						\
    do									\
    {									\
	uint32_t  *__d = ((uint32_t *)(l)) + ((o) >> 5);		\
	uint32_t __m, __v;						\
									\
	__m = 1 << (0x1f - ((o) & 0x1f));				\
	__v = (v)? __m : 0;						\
									\
	WRITE((img), __d, (READ((img), __d) & ~__m) | __v);		\
    }									\
    while (0)
#else
#define STORE_1(img,l,o,v)						\
    do									\
    {									\
	uint32_t  *__d = ((uint32_t *)(l)) + ((o) >> 5);		\
	uint32_t __m, __v;						\
									\
	__m = 1 << ((o) & 0x1f);					\
	__v = (v)? __m : 0;						\
									\
	WRITE((img), __d, (READ((img), __d) & ~__m) | __v);		\
    }									\
    while (0)
#endif

#define STORE_8(img,l,o,v)  (WRITE (img, (uint8_t *)(l) + ((o) >> 3), (v)))

#ifdef WORDS_BIGENDIAN
#define STORE_4(img,l,o,v)						\
    do									\
    {									\
	int bo = 4 * (o);						\
	int v4 = (v) & 0x0f;						\
									\
	STORE_8 (img, l, bo, (						\
		     bo & 4 ?						\
		     (FETCH_8 (img, l, bo) & 0xf0) | (v4) :		\
		     (FETCH_8 (img, l, bo) & 0x0f) | (v4 << 4)));	\
    } while (0)
#else
#define STORE_4(img,l,o,v)						\
    do									\
    {									\
	int bo = 4 * (o);						\
	int v4 = (v) & 0x0f;						\
									\
	STORE_8 (img, l, bo, (						\
		     bo & 4 ?						\
		     (FETCH_8 (img, l, bo) & 0x0f) | (v4 << 4) :	\
		     (FETCH_8 (img, l, bo) & 0xf0) | (v4)));		\
    } while (0)
#endif

#ifdef WORDS_BIGENDIAN
#define STORE_24(img,l,o,v)                                            \
    do                                                                 \
    {                                                                  \
	uint8_t *__tmp = (l) + 3 * (o);				       \
        							       \
	WRITE ((img), __tmp++, ((v) & 0x00ff0000) >> 16);	       \
	WRITE ((img), __tmp++, ((v) & 0x0000ff00) >>  8);	       \
	WRITE ((img), __tmp++, ((v) & 0x000000ff) >>  0);	       \
    }                                                                  \
    while (0)
#else
#define STORE_24(img,l,o,v)                                            \
    do                                                                 \
    {                                                                  \
	uint8_t *__tmp = (l) + 3 * (o);				       \
        							       \
	WRITE ((img), __tmp++, ((v) & 0x000000ff) >>  0);	       \
	WRITE ((img), __tmp++, ((v) & 0x0000ff00) >>  8);	       \
	WRITE ((img), __tmp++, ((v) & 0x00ff0000) >> 16);	       \
    }								       \
    while (0)
#endif

/*
 * YV12 setup and access macros
 */

#define YV12_SETUP(image)                                               \
    bits_image_t *__bits_image = (bits_image_t *)image;                 \
    uint32_t *bits = __bits_image->bits;                                \
    int stride = __bits_image->rowstride;                               \
    int offset0 = stride < 0 ?                                          \
    ((-stride) >> 1) * ((__bits_image->height - 1) >> 1) - stride :	\
    stride * __bits_image->height;					\
    int offset1 = stride < 0 ?                                          \
    offset0 + ((-stride) >> 1) * ((__bits_image->height) >> 1) :	\
	offset0 + (offset0 >> 2)

/* Note no trailing semicolon on the above macro; if it's there, then
 * the typical usage of YV12_SETUP(image); will have an extra trailing ;
 * that some compilers will interpret as a statement -- and then any further
 * variable declarations will cause an error.
 */

#define YV12_Y(line)                                                    \
    ((uint8_t *) ((bits) + (stride) * (line)))

#define YV12_U(line)                                                    \
    ((uint8_t *) ((bits) + offset1 +                                    \
                  ((stride) >> 1) * ((line) >> 1)))

#define YV12_V(line)                                                    \
    ((uint8_t *) ((bits) + offset0 +                                    \
                  ((stride) >> 1) * ((line) >> 1)))

/* Misc. helpers */

static force_inline void
get_shifts (pixman_format_code_t  format,
	    int			 *a,
	    int			 *r,
	    int                  *g,
	    int                  *b)
{
    switch (PIXMAN_FORMAT_TYPE (format))
    {
    case PIXMAN_TYPE_A:
	*b = 0;
	*g = 0;
	*r = 0;
	*a = 0;
	break;

    case PIXMAN_TYPE_ARGB:
	*b = 0;
	*g = *b + PIXMAN_FORMAT_B (format);
	*r = *g + PIXMAN_FORMAT_G (format);
	*a = *r + PIXMAN_FORMAT_R (format);
	break;

    case PIXMAN_TYPE_ABGR:
	*r = 0;
	*g = *r + PIXMAN_FORMAT_R (format);
	*b = *g + PIXMAN_FORMAT_G (format);
	*a = *b + PIXMAN_FORMAT_B (format);
	break;

    case PIXMAN_TYPE_BGRA:
	/* With BGRA formats we start counting at the high end of the pixel */
	*b = PIXMAN_FORMAT_BPP (format) - PIXMAN_FORMAT_B (format);
	*g = *b - PIXMAN_FORMAT_B (format);
	*r = *g - PIXMAN_FORMAT_G (format);
	*a = *r - PIXMAN_FORMAT_R (format);
	break;

    case PIXMAN_TYPE_RGBA:
	/* With BGRA formats we start counting at the high end of the pixel */
	*r = PIXMAN_FORMAT_BPP (format) - PIXMAN_FORMAT_R (format);
	*g = *r - PIXMAN_FORMAT_R (format);
	*b = *g - PIXMAN_FORMAT_G (format);
	*a = *b - PIXMAN_FORMAT_B (format);
	break;

    default:
	assert (0);
	break;
    }
}

static force_inline uint32_t
convert_channel (uint32_t pixel, uint32_t def_value,
		 int n_from_bits, int from_shift,
		 int n_to_bits, int to_shift)
{
    uint32_t v;

    if (n_from_bits && n_to_bits)
	v  = unorm_to_unorm (pixel >> from_shift, n_from_bits, n_to_bits);
    else if (n_to_bits)
	v = def_value;
    else
	v = 0;

    return (v & ((1 << n_to_bits) - 1)) << to_shift;
}

static force_inline uint32_t
convert_pixel (pixman_format_code_t from, pixman_format_code_t to, uint32_t pixel)
{
    int a_from_shift, r_from_shift, g_from_shift, b_from_shift;
    int a_to_shift, r_to_shift, g_to_shift, b_to_shift;
    uint32_t a, r, g, b;

    get_shifts (from, &a_from_shift, &r_from_shift, &g_from_shift, &b_from_shift);
    get_shifts (to, &a_to_shift, &r_to_shift, &g_to_shift, &b_to_shift);

    a = convert_channel (pixel, ~0,
			 PIXMAN_FORMAT_A (from), a_from_shift,
			 PIXMAN_FORMAT_A (to), a_to_shift);

    r = convert_channel (pixel, 0,
			 PIXMAN_FORMAT_R (from), r_from_shift,
			 PIXMAN_FORMAT_R (to), r_to_shift);

    g = convert_channel (pixel, 0,
			 PIXMAN_FORMAT_G (from), g_from_shift,
			 PIXMAN_FORMAT_G (to), g_to_shift);

    b = convert_channel (pixel, 0,
			 PIXMAN_FORMAT_B (from), b_from_shift,
			 PIXMAN_FORMAT_B (to), b_to_shift);

    return a | r | g | b;
}

static force_inline uint32_t
convert_pixel_to_a8r8g8b8 (pixman_image_t *image,
			   pixman_format_code_t format,
			   uint32_t pixel)
{
    if (PIXMAN_FORMAT_TYPE (format) == PIXMAN_TYPE_GRAY		||
	PIXMAN_FORMAT_TYPE (format) == PIXMAN_TYPE_COLOR)
    {
	return image->bits.indexed->rgba[pixel];
    }
    else
    {
	return convert_pixel (format, PIXMAN_a8r8g8b8, pixel);
    }
}

static force_inline uint32_t
convert_pixel_from_a8r8g8b8 (pixman_image_t *image,
			     pixman_format_code_t format, uint32_t pixel)
{
    if (PIXMAN_FORMAT_TYPE (format) == PIXMAN_TYPE_GRAY)
    {
	pixel = CONVERT_RGB24_TO_Y15 (pixel);

	return image->bits.indexed->ent[pixel & 0x7fff];
    }
    else if (PIXMAN_FORMAT_TYPE (format) == PIXMAN_TYPE_COLOR)
    {
	pixel = convert_pixel (PIXMAN_a8r8g8b8, PIXMAN_x1r5g5b5, pixel);

	return image->bits.indexed->ent[pixel & 0x7fff];
    }
    else
    {
	return convert_pixel (PIXMAN_a8r8g8b8, format, pixel);
    }
}

static force_inline uint32_t
fetch_and_convert_pixel (pixman_image_t	*	image,
			 const uint8_t *	bits,
			 int			offset,
			 pixman_format_code_t	format)
{
    uint32_t pixel;

    switch (PIXMAN_FORMAT_BPP (format))
    {
    case 1:
	pixel = FETCH_1 (image, bits, offset);
	break;

    case 4:
	pixel = FETCH_4 (image, bits, offset);
	break;

    case 8:
	pixel = READ (image, bits + offset);
	break;

    case 16:
	pixel = READ (image, ((uint16_t *)bits + offset));
	break;

    case 24:
	pixel = FETCH_24 (image, bits, offset);
	break;

    case 32:
	pixel = READ (image, ((uint32_t *)bits + offset));
	break;

    default:
	pixel = 0xffff00ff; /* As ugly as possible to detect the bug */
	break;
    }

    return convert_pixel_to_a8r8g8b8 (image, format, pixel);
}

static force_inline void
convert_and_store_pixel (bits_image_t *		image,
			 uint8_t *		dest,
			 int                    offset,
			 pixman_format_code_t	format,
			 uint32_t		pixel)
{
    uint32_t converted = convert_pixel_from_a8r8g8b8 (
	(pixman_image_t *)image, format, pixel);

    switch (PIXMAN_FORMAT_BPP (format))
    {
    case 1:
	STORE_1 (image, dest, offset, converted & 0x01);
	break;

    case 4:
	STORE_4 (image, dest, offset, converted & 0xf);
	break;

    case 8:
	WRITE (image, (dest + offset), converted & 0xff);
	break;

    case 16:
	WRITE (image, ((uint16_t *)dest + offset), converted & 0xffff);
	break;

    case 24:
	STORE_24 (image, dest, offset, converted);
	break;

    case 32:
	WRITE (image, ((uint32_t *)dest + offset), converted);
	break;

    default:
	*dest = 0x0;
	break;
    }
}

#define MAKE_ACCESSORS(format)						\
    static void								\
    fetch_scanline_ ## format (pixman_image_t *image,			\
			       int	       x,			\
			       int             y,			\
			       int             width,			\
			       uint32_t *      buffer,			\
			       const uint32_t *mask)			\
    {									\
	uint8_t *bits =							\
	    (uint8_t *)(image->bits.bits + y * image->bits.rowstride);	\
	int i;								\
									\
	for (i = 0; i < width; ++i)					\
	{								\
	    *buffer++ =							\
		fetch_and_convert_pixel (image, bits, x + i, PIXMAN_ ## format); \
	}								\
    }									\
									\
    static void								\
    store_scanline_ ## format (bits_image_t *  image,			\
			       int             x,			\
			       int             y,			\
			       int             width,			\
			       const uint32_t *values)			\
    {									\
	uint8_t *dest =							\
	    (uint8_t *)(image->bits + y * image->rowstride);		\
	int i;								\
									\
	for (i = 0; i < width; ++i)					\
	{								\
	    convert_and_store_pixel (					\
		image, dest, i + x, PIXMAN_ ## format, values[i]);	\
	}								\
    }									\
									\
    static uint32_t							\
    fetch_pixel_ ## format (bits_image_t *image,			\
			    int		offset,				\
			    int		line)				\
    {									\
	uint8_t *bits =							\
	    (uint8_t *)(image->bits + line * image->rowstride);		\
									\
	return fetch_and_convert_pixel ((pixman_image_t *)image,	\
					bits, offset, PIXMAN_ ## format); \
    }									\
									\
    static const void *const __dummy__ ## format

MAKE_ACCESSORS(a8r8g8b8);
MAKE_ACCESSORS(x8r8g8b8);
MAKE_ACCESSORS(a8b8g8r8);
MAKE_ACCESSORS(x8b8g8r8);
MAKE_ACCESSORS(x14r6g6b6);
MAKE_ACCESSORS(b8g8r8a8);
MAKE_ACCESSORS(b8g8r8x8);
MAKE_ACCESSORS(r8g8b8x8);
MAKE_ACCESSORS(r8g8b8a8);
MAKE_ACCESSORS(r8g8b8);
MAKE_ACCESSORS(b8g8r8);
MAKE_ACCESSORS(r5g6b5);
MAKE_ACCESSORS(b5g6r5);
MAKE_ACCESSORS(a1r5g5b5);
MAKE_ACCESSORS(x1r5g5b5);
MAKE_ACCESSORS(a1b5g5r5);
MAKE_ACCESSORS(x1b5g5r5);
MAKE_ACCESSORS(a4r4g4b4);
MAKE_ACCESSORS(x4r4g4b4);
MAKE_ACCESSORS(a4b4g4r4);
MAKE_ACCESSORS(x4b4g4r4);
MAKE_ACCESSORS(a8);
MAKE_ACCESSORS(c8);
MAKE_ACCESSORS(g8);
MAKE_ACCESSORS(r3g3b2);
MAKE_ACCESSORS(b2g3r3);
MAKE_ACCESSORS(a2r2g2b2);
MAKE_ACCESSORS(a2b2g2r2);
MAKE_ACCESSORS(x4a4);
MAKE_ACCESSORS(a4);
MAKE_ACCESSORS(g4);
MAKE_ACCESSORS(c4);
MAKE_ACCESSORS(r1g2b1);
MAKE_ACCESSORS(b1g2r1);
MAKE_ACCESSORS(a1r1g1b1);
MAKE_ACCESSORS(a1b1g1r1);
MAKE_ACCESSORS(a1);
MAKE_ACCESSORS(g1);

/********************************** Fetch ************************************/

/* Expects a uint64_t buffer */
static void
fetch_scanline_a2r10g10b10 (pixman_image_t *image,
                            int             x,
                            int             y,
                            int             width,
                            uint32_t *      b,
                            const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    const uint32_t *pixel = bits + x;
    const uint32_t *end = pixel + width;
    uint64_t *buffer = (uint64_t *)b;

    while (pixel < end)
    {
	uint32_t p = READ (image, pixel++);
	uint64_t a = p >> 30;
	uint64_t r = (p >> 20) & 0x3ff;
	uint64_t g = (p >> 10) & 0x3ff;
	uint64_t b = p & 0x3ff;

	r = r << 6 | r >> 4;
	g = g << 6 | g >> 4;
	b = b << 6 | b >> 4;

	a <<= 14;
	a |= a >> 2;
	a |= a >> 4;
	a |= a >> 8;

	*buffer++ = a << 48 | r << 32 | g << 16 | b;
    }
}

/* Expects a uint64_t buffer */
static void
fetch_scanline_x2r10g10b10 (pixman_image_t *image,
                            int             x,
                            int             y,
                            int             width,
                            uint32_t *      b,
                            const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    const uint32_t *pixel = (uint32_t *)bits + x;
    const uint32_t *end = pixel + width;
    uint64_t *buffer = (uint64_t *)b;
    
    while (pixel < end)
    {
	uint32_t p = READ (image, pixel++);
	uint64_t r = (p >> 20) & 0x3ff;
	uint64_t g = (p >> 10) & 0x3ff;
	uint64_t b = p & 0x3ff;
	
	r = r << 6 | r >> 4;
	g = g << 6 | g >> 4;
	b = b << 6 | b >> 4;
	
	*buffer++ = 0xffffULL << 48 | r << 32 | g << 16 | b;
    }
}

/* Expects a uint64_t buffer */
static void
fetch_scanline_a2b10g10r10 (pixman_image_t *image,
                            int             x,
                            int             y,
                            int             width,
                            uint32_t *      b,
                            const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    const uint32_t *pixel = bits + x;
    const uint32_t *end = pixel + width;
    uint64_t *buffer = (uint64_t *)b;
    
    while (pixel < end)
    {
	uint32_t p = READ (image, pixel++);
	uint64_t a = p >> 30;
	uint64_t b = (p >> 20) & 0x3ff;
	uint64_t g = (p >> 10) & 0x3ff;
	uint64_t r = p & 0x3ff;
	
	r = r << 6 | r >> 4;
	g = g << 6 | g >> 4;
	b = b << 6 | b >> 4;
	
	a <<= 14;
	a |= a >> 2;
	a |= a >> 4;
	a |= a >> 8;

	*buffer++ = a << 48 | r << 32 | g << 16 | b;
    }
}

/* Expects a uint64_t buffer */
static void
fetch_scanline_x2b10g10r10 (pixman_image_t *image,
                            int             x,
                            int             y,
                            int             width,
                            uint32_t *      b,
                            const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    const uint32_t *pixel = (uint32_t *)bits + x;
    const uint32_t *end = pixel + width;
    uint64_t *buffer = (uint64_t *)b;
    
    while (pixel < end)
    {
	uint32_t p = READ (image, pixel++);
	uint64_t b = (p >> 20) & 0x3ff;
	uint64_t g = (p >> 10) & 0x3ff;
	uint64_t r = p & 0x3ff;
	
	r = r << 6 | r >> 4;
	g = g << 6 | g >> 4;
	b = b << 6 | b >> 4;
	
	*buffer++ = 0xffffULL << 48 | r << 32 | g << 16 | b;
    }
}

static void
fetch_scanline_yuy2 (pixman_image_t *image,
                     int             x,
                     int             line,
                     int             width,
                     uint32_t *      buffer,
                     const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + image->bits.rowstride * line;
    int i;
    
    for (i = 0; i < width; i++)
    {
	int16_t y, u, v;
	int32_t r, g, b;
	
	y = ((uint8_t *) bits)[(x + i) << 1] - 16;
	u = ((uint8_t *) bits)[(((x + i) << 1) & - 4) + 1] - 128;
	v = ((uint8_t *) bits)[(((x + i) << 1) & - 4) + 3] - 128;
	
	/* R = 1.164(Y - 16) + 1.596(V - 128) */
	r = 0x012b27 * y + 0x019a2e * v;
	/* G = 1.164(Y - 16) - 0.813(V - 128) - 0.391(U - 128) */
	g = 0x012b27 * y - 0x00d0f2 * v - 0x00647e * u;
	/* B = 1.164(Y - 16) + 2.018(U - 128) */
	b = 0x012b27 * y + 0x0206a2 * u;
	
	*buffer++ = 0xff000000 |
	    (r >= 0 ? r < 0x1000000 ? r         & 0xff0000 : 0xff0000 : 0) |
	    (g >= 0 ? g < 0x1000000 ? (g >> 8)  & 0x00ff00 : 0x00ff00 : 0) |
	    (b >= 0 ? b < 0x1000000 ? (b >> 16) & 0x0000ff : 0x0000ff : 0);
    }
}

static void
fetch_scanline_yv12 (pixman_image_t *image,
                     int             x,
                     int             line,
                     int             width,
                     uint32_t *      buffer,
                     const uint32_t *mask)
{
    YV12_SETUP (image);
    uint8_t *y_line = YV12_Y (line);
    uint8_t *u_line = YV12_U (line);
    uint8_t *v_line = YV12_V (line);
    int i;
    
    for (i = 0; i < width; i++)
    {
	int16_t y, u, v;
	int32_t r, g, b;

	y = y_line[x + i] - 16;
	u = u_line[(x + i) >> 1] - 128;
	v = v_line[(x + i) >> 1] - 128;

	/* R = 1.164(Y - 16) + 1.596(V - 128) */
	r = 0x012b27 * y + 0x019a2e * v;
	/* G = 1.164(Y - 16) - 0.813(V - 128) - 0.391(U - 128) */
	g = 0x012b27 * y - 0x00d0f2 * v - 0x00647e * u;
	/* B = 1.164(Y - 16) + 2.018(U - 128) */
	b = 0x012b27 * y + 0x0206a2 * u;

	*buffer++ = 0xff000000 |
	    (r >= 0 ? r < 0x1000000 ? r         & 0xff0000 : 0xff0000 : 0) |
	    (g >= 0 ? g < 0x1000000 ? (g >> 8)  & 0x00ff00 : 0x00ff00 : 0) |
	    (b >= 0 ? b < 0x1000000 ? (b >> 16) & 0x0000ff : 0x0000ff : 0);
    }
}

/**************************** Pixel wise fetching *****************************/

/* Despite the type, expects a uint64_t buffer */
static uint64_t
fetch_pixel_a2r10g10b10 (bits_image_t *image,
			 int		  offset,
			 int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t p = READ (image, bits + offset);
    uint64_t a = p >> 30;
    uint64_t r = (p >> 20) & 0x3ff;
    uint64_t g = (p >> 10) & 0x3ff;
    uint64_t b = p & 0x3ff;

    r = r << 6 | r >> 4;
    g = g << 6 | g >> 4;
    b = b << 6 | b >> 4;

    a <<= 14;
    a |= a >> 2;
    a |= a >> 4;
    a |= a >> 8;

    return a << 48 | r << 32 | g << 16 | b;
}

/* Despite the type, this function expects a uint64_t buffer */
static uint64_t
fetch_pixel_x2r10g10b10 (bits_image_t *image,
			 int	   offset,
			 int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t p = READ (image, bits + offset);
    uint64_t r = (p >> 20) & 0x3ff;
    uint64_t g = (p >> 10) & 0x3ff;
    uint64_t b = p & 0x3ff;
    
    r = r << 6 | r >> 4;
    g = g << 6 | g >> 4;
    b = b << 6 | b >> 4;
    
    return 0xffffULL << 48 | r << 32 | g << 16 | b;
}

/* Despite the type, expects a uint64_t buffer */
static uint64_t
fetch_pixel_a2b10g10r10 (bits_image_t *image,
			 int           offset,
			 int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t p = READ (image, bits + offset);
    uint64_t a = p >> 30;
    uint64_t b = (p >> 20) & 0x3ff;
    uint64_t g = (p >> 10) & 0x3ff;
    uint64_t r = p & 0x3ff;
    
    r = r << 6 | r >> 4;
    g = g << 6 | g >> 4;
    b = b << 6 | b >> 4;
    
    a <<= 14;
    a |= a >> 2;
    a |= a >> 4;
    a |= a >> 8;
    
    return a << 48 | r << 32 | g << 16 | b;
}

/* Despite the type, this function expects a uint64_t buffer */
static uint64_t
fetch_pixel_x2b10g10r10 (bits_image_t *image,
			 int           offset,
			 int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t p = READ (image, bits + offset);
    uint64_t b = (p >> 20) & 0x3ff;
    uint64_t g = (p >> 10) & 0x3ff;
    uint64_t r = p & 0x3ff;
    
    r = r << 6 | r >> 4;
    g = g << 6 | g >> 4;
    b = b << 6 | b >> 4;
    
    return 0xffffULL << 48 | r << 32 | g << 16 | b;
}

static uint32_t
fetch_pixel_yuy2 (bits_image_t *image,
		  int           offset,
		  int           line)
{
    const uint32_t *bits = image->bits + image->rowstride * line;
    
    int16_t y, u, v;
    int32_t r, g, b;
    
    y = ((uint8_t *) bits)[offset << 1] - 16;
    u = ((uint8_t *) bits)[((offset << 1) & - 4) + 1] - 128;
    v = ((uint8_t *) bits)[((offset << 1) & - 4) + 3] - 128;
    
    /* R = 1.164(Y - 16) + 1.596(V - 128) */
    r = 0x012b27 * y + 0x019a2e * v;
    
    /* G = 1.164(Y - 16) - 0.813(V - 128) - 0.391(U - 128) */
    g = 0x012b27 * y - 0x00d0f2 * v - 0x00647e * u;
    
    /* B = 1.164(Y - 16) + 2.018(U - 128) */
    b = 0x012b27 * y + 0x0206a2 * u;
    
    return 0xff000000 |
	(r >= 0 ? r < 0x1000000 ? r         & 0xff0000 : 0xff0000 : 0) |
	(g >= 0 ? g < 0x1000000 ? (g >> 8)  & 0x00ff00 : 0x00ff00 : 0) |
	(b >= 0 ? b < 0x1000000 ? (b >> 16) & 0x0000ff : 0x0000ff : 0);
}

static uint32_t
fetch_pixel_yv12 (bits_image_t *image,
		  int           offset,
		  int           line)
{
    YV12_SETUP (image);
    int16_t y = YV12_Y (line)[offset] - 16;
    int16_t u = YV12_U (line)[offset >> 1] - 128;
    int16_t v = YV12_V (line)[offset >> 1] - 128;
    int32_t r, g, b;
    
    /* R = 1.164(Y - 16) + 1.596(V - 128) */
    r = 0x012b27 * y + 0x019a2e * v;
    
    /* G = 1.164(Y - 16) - 0.813(V - 128) - 0.391(U - 128) */
    g = 0x012b27 * y - 0x00d0f2 * v - 0x00647e * u;
    
    /* B = 1.164(Y - 16) + 2.018(U - 128) */
    b = 0x012b27 * y + 0x0206a2 * u;
    
    return 0xff000000 |
	(r >= 0 ? r < 0x1000000 ? r         & 0xff0000 : 0xff0000 : 0) |
	(g >= 0 ? g < 0x1000000 ? (g >> 8)  & 0x00ff00 : 0x00ff00 : 0) |
	(b >= 0 ? b < 0x1000000 ? (b >> 16) & 0x0000ff : 0x0000ff : 0);
}

/*********************************** Store ************************************/

static void
store_scanline_a2r10g10b10 (bits_image_t *  image,
                            int             x,
                            int             y,
                            int             width,
                            const uint32_t *v)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    uint32_t *pixel = bits + x;
    uint64_t *values = (uint64_t *)v;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	WRITE (image, pixel++,
	       ((values[i] >> 32) & 0xc0000000) |
	       ((values[i] >> 18) & 0x3ff00000) |
	       ((values[i] >> 12) & 0xffc00) | 
	       ((values[i] >> 6) & 0x3ff));    
    }
}

static void
store_scanline_x2r10g10b10 (bits_image_t *  image,
                            int             x,
                            int             y,
                            int             width,
                            const uint32_t *v)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    uint64_t *values = (uint64_t *)v;
    uint32_t *pixel = bits + x;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	WRITE (image, pixel++,
	       ((values[i] >> 18) & 0x3ff00000) | 
	       ((values[i] >> 12) & 0xffc00) |
	       ((values[i] >> 6) & 0x3ff));
    }
}

static void
store_scanline_a2b10g10r10 (bits_image_t *  image,
                            int             x,
                            int             y,
                            int             width,
                            const uint32_t *v)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    uint32_t *pixel = bits + x;
    uint64_t *values = (uint64_t *)v;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	WRITE (image, pixel++,
	       ((values[i] >> 32) & 0xc0000000) |
	       ((values[i] >> 38) & 0x3ff) |
	       ((values[i] >> 12) & 0xffc00) |
	       ((values[i] << 14) & 0x3ff00000));
    }
}

static void
store_scanline_x2b10g10r10 (bits_image_t *  image,
                            int             x,
                            int             y,
                            int             width,
                            const uint32_t *v)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    uint64_t *values = (uint64_t *)v;
    uint32_t *pixel = bits + x;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	WRITE (image, pixel++,
	       ((values[i] >> 38) & 0x3ff) |
	       ((values[i] >> 12) & 0xffc00) |
	       ((values[i] << 14) & 0x3ff00000));
    }
}

/*
 * Contracts a 64bpp image to 32bpp and then stores it using a regular 32-bit
 * store proc. Despite the type, this function expects a uint64_t buffer.
 */
static void
store_scanline_generic_64 (bits_image_t *  image,
                           int             x,
                           int             y,
                           int             width,
                           const uint32_t *values)
{
    uint32_t *argb8_pixels;
    
    assert (image->common.type == BITS);
    
    argb8_pixels = pixman_malloc_ab (width, sizeof(uint32_t));
    if (!argb8_pixels)
	return;
    
    /* Contract the scanline.  We could do this in place if values weren't
     * const.
     */
    pixman_contract (argb8_pixels, (uint64_t *)values, width);
    
    image->store_scanline_32 (image, x, y, width, argb8_pixels);
    
    free (argb8_pixels);
}

/* Despite the type, this function expects both buffer
 * and mask to be uint64_t
 */
static void
fetch_scanline_generic_64 (pixman_image_t *image,
                           int             x,
                           int             y,
                           int             width,
                           uint32_t *      buffer,
                           const uint32_t *mask)
{
    pixman_format_code_t format;

    /* Fetch the pixels into the first half of buffer and then expand them in
     * place.
     */
    image->bits.fetch_scanline_32 (image, x, y, width, buffer, NULL);

    format = image->bits.format;
    if (PIXMAN_FORMAT_TYPE (format) == PIXMAN_TYPE_COLOR	||
	PIXMAN_FORMAT_TYPE (format) == PIXMAN_TYPE_GRAY)
    {
	/* Indexed formats are mapped to a8r8g8b8 with full
	 * precision, so when expanding we shouldn't correct
	 * for the width of the channels
	 */

	format = PIXMAN_a8r8g8b8;
    }

    pixman_expand ((uint64_t *)buffer, buffer, format, width);
}

/* Despite the type, this function expects a uint64_t *buffer */
static uint64_t
fetch_pixel_generic_64 (bits_image_t *image,
			int	      offset,
			int           line)
{
    uint32_t pixel32 = image->fetch_pixel_32 (image, offset, line);
    uint64_t result;
    pixman_format_code_t format;

    format = image->format;
    if (PIXMAN_FORMAT_TYPE (format) == PIXMAN_TYPE_COLOR	||
	PIXMAN_FORMAT_TYPE (format) == PIXMAN_TYPE_GRAY)
    {
	/* Indexed formats are mapped to a8r8g8b8 with full
	 * precision, so when expanding we shouldn't correct
	 * for the width of the channels
	 */

	format = PIXMAN_a8r8g8b8;
    }

    pixman_expand ((uint64_t *)&result, &pixel32, format, 1);

    return result;
}

/*
 * XXX: The transformed fetch path only works at 32-bpp so far.  When all
 * paths have wide versions, this can be removed.
 *
 * WARNING: This function loses precision!
 */
static uint32_t
fetch_pixel_generic_lossy_32 (bits_image_t *image,
			      int           offset,
			      int           line)
{
    uint64_t pixel64 = image->fetch_pixel_64 (image, offset, line);
    uint32_t result;

    pixman_contract (&result, &pixel64, 1);

    return result;
}

typedef struct
{
    pixman_format_code_t	format;
    fetch_scanline_t		fetch_scanline_32;
    fetch_scanline_t		fetch_scanline_64;
    fetch_pixel_32_t		fetch_pixel_32;
    fetch_pixel_64_t		fetch_pixel_64;
    store_scanline_t		store_scanline_32;
    store_scanline_t		store_scanline_64;
} format_info_t;

#define FORMAT_INFO(format) 						\
    {									\
	PIXMAN_ ## format,						\
	    fetch_scanline_ ## format,					\
	    fetch_scanline_generic_64,					\
	    fetch_pixel_ ## format, fetch_pixel_generic_64,		\
	    store_scanline_ ## format, store_scanline_generic_64	\
    }

static const format_info_t accessors[] =
{
/* 32 bpp formats */
    FORMAT_INFO (a8r8g8b8),
    FORMAT_INFO (x8r8g8b8),
    FORMAT_INFO (a8b8g8r8),
    FORMAT_INFO (x8b8g8r8),
    FORMAT_INFO (b8g8r8a8),
    FORMAT_INFO (b8g8r8x8),
    FORMAT_INFO (r8g8b8a8),
    FORMAT_INFO (r8g8b8x8),
    FORMAT_INFO (x14r6g6b6),

/* 24bpp formats */
    FORMAT_INFO (r8g8b8),
    FORMAT_INFO (b8g8r8),
    
/* 16bpp formats */
    FORMAT_INFO (r5g6b5),
    FORMAT_INFO (b5g6r5),
    
    FORMAT_INFO (a1r5g5b5),
    FORMAT_INFO (x1r5g5b5),
    FORMAT_INFO (a1b5g5r5),
    FORMAT_INFO (x1b5g5r5),
    FORMAT_INFO (a4r4g4b4),
    FORMAT_INFO (x4r4g4b4),
    FORMAT_INFO (a4b4g4r4),
    FORMAT_INFO (x4b4g4r4),
    
/* 8bpp formats */
    FORMAT_INFO (a8),
    FORMAT_INFO (r3g3b2),
    FORMAT_INFO (b2g3r3),
    FORMAT_INFO (a2r2g2b2),
    FORMAT_INFO (a2b2g2r2),
    
    FORMAT_INFO (c8),
    
    FORMAT_INFO (g8),
    
#define fetch_scanline_x4c4 fetch_scanline_c8
#define fetch_pixel_x4c4 fetch_pixel_c8
#define store_scanline_x4c4 store_scanline_c8
    FORMAT_INFO (x4c4),
    
#define fetch_scanline_x4g4 fetch_scanline_g8
#define fetch_pixel_x4g4 fetch_pixel_g8
#define store_scanline_x4g4 store_scanline_g8
    FORMAT_INFO (x4g4),
    
    FORMAT_INFO (x4a4),
    
/* 4bpp formats */
    FORMAT_INFO (a4),
    FORMAT_INFO (r1g2b1),
    FORMAT_INFO (b1g2r1),
    FORMAT_INFO (a1r1g1b1),
    FORMAT_INFO (a1b1g1r1),
    
    FORMAT_INFO (c4),
    
    FORMAT_INFO (g4),
    
/* 1bpp formats */
    FORMAT_INFO (a1),
    FORMAT_INFO (g1),
    
/* Wide formats */
    
    { PIXMAN_a2r10g10b10,
      NULL, fetch_scanline_a2r10g10b10,
      fetch_pixel_generic_lossy_32, fetch_pixel_a2r10g10b10,
      NULL, store_scanline_a2r10g10b10 },
    
    { PIXMAN_x2r10g10b10,
      NULL, fetch_scanline_x2r10g10b10,
      fetch_pixel_generic_lossy_32, fetch_pixel_x2r10g10b10,
      NULL, store_scanline_x2r10g10b10 },
    
    { PIXMAN_a2b10g10r10,
      NULL, fetch_scanline_a2b10g10r10,
      fetch_pixel_generic_lossy_32, fetch_pixel_a2b10g10r10,
      NULL, store_scanline_a2b10g10r10 },
    
    { PIXMAN_x2b10g10r10,
      NULL, fetch_scanline_x2b10g10r10,
      fetch_pixel_generic_lossy_32, fetch_pixel_x2b10g10r10,
      NULL, store_scanline_x2b10g10r10 },
    
/* YUV formats */
    { PIXMAN_yuy2,
      fetch_scanline_yuy2, fetch_scanline_generic_64,
      fetch_pixel_yuy2, fetch_pixel_generic_64,
      NULL, NULL },
    
    { PIXMAN_yv12,
      fetch_scanline_yv12, fetch_scanline_generic_64,
      fetch_pixel_yv12, fetch_pixel_generic_64,
      NULL, NULL },
    
    { PIXMAN_null },
};

static void
setup_accessors (bits_image_t *image)
{
    const format_info_t *info = accessors;
    
    while (info->format != PIXMAN_null)
    {
	if (info->format == image->format)
	{
	    image->fetch_scanline_32 = info->fetch_scanline_32;
	    image->fetch_scanline_64 = info->fetch_scanline_64;
	    image->fetch_pixel_32 = info->fetch_pixel_32;
	    image->fetch_pixel_64 = info->fetch_pixel_64;
	    image->store_scanline_32 = info->store_scanline_32;
	    image->store_scanline_64 = info->store_scanline_64;
	    
	    return;
	}
	
	info++;
    }
}

#ifndef PIXMAN_FB_ACCESSORS
void
_pixman_bits_image_setup_accessors_accessors (bits_image_t *image);

void
_pixman_bits_image_setup_accessors (bits_image_t *image)
{
    if (image->read_func || image->write_func)
	_pixman_bits_image_setup_accessors_accessors (image);
    else
	setup_accessors (image);
}

#else

void
_pixman_bits_image_setup_accessors_accessors (bits_image_t *image)
{
    setup_accessors (image);
}

#endif
