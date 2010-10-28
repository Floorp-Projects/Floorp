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

#define RGB15_TO_ENTRY(mif,rgb15)					\
    ((mif)->ent[rgb15])

#define RGB24_TO_ENTRY(mif,rgb24)					\
    RGB15_TO_ENTRY (mif,CONVERT_RGB24_TO_RGB15 (rgb24))

#define RGB24_TO_ENTRY_Y(mif,rgb24)					\
    ((mif)->ent[CONVERT_RGB24_TO_Y15 (rgb24)])

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

/********************************** Fetch ************************************/

static void
fetch_scanline_a8r8g8b8 (pixman_image_t *image,
                         int             x,
                         int             y,
                         int             width,
                         uint32_t *      buffer,
                         const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    
    MEMCPY_WRAPPED (image,
                    buffer, (const uint32_t *)bits + x,
                    width * sizeof(uint32_t));
}

static void
fetch_scanline_x8r8g8b8 (pixman_image_t *image,
                         int             x,
                         int             y,
                         int             width,
                         uint32_t *      buffer,
                         const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    const uint32_t *pixel = (const uint32_t *)bits + x;
    const uint32_t *end = pixel + width;
    
    while (pixel < end)
	*buffer++ = READ (image, pixel++) | 0xff000000;
}

static void
fetch_scanline_a8b8g8r8 (pixman_image_t *image,
                         int             x,
                         int             y,
                         int             width,
                         uint32_t *      buffer,
                         const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    const uint32_t *pixel = (uint32_t *)bits + x;
    const uint32_t *end = pixel + width;
    
    while (pixel < end)
    {
	uint32_t p = READ (image, pixel++);
	
	*buffer++ = (p & 0xff00ff00)	|
	    ((p >> 16) & 0xff)		|
	    ((p & 0xff) << 16);
    }
}

static void
fetch_scanline_x8b8g8r8 (pixman_image_t *image,
                         int             x,
                         int             y,
                         int             width,
                         uint32_t *      buffer,
                         const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    const uint32_t *pixel = (uint32_t *)bits + x;
    const uint32_t *end = pixel + width;
    
    while (pixel < end)
    {
	uint32_t p = READ (image, pixel++);
	
	*buffer++ = 0xff000000		|
	    (p & 0x0000ff00)		|
	    ((p >> 16) & 0xff)		|
	    ((p & 0xff) << 16);
    }
}

static void
fetch_scanline_b8g8r8a8 (pixman_image_t *image,
                         int             x,
                         int             y,
                         int             width,
                         uint32_t *      buffer,
                         const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    const uint32_t *pixel = (uint32_t *)bits + x;
    const uint32_t *end = pixel + width;

    while (pixel < end)
    {
	uint32_t p = READ (image, pixel++);

	*buffer++ = (((p & 0xff000000) >> 24)	|
	             ((p & 0x00ff0000) >> 8)	|
	             ((p & 0x0000ff00) << 8)	|
	             ((p & 0x000000ff) << 24));
    }
}

static void
fetch_scanline_b8g8r8x8 (pixman_image_t *image,
                         int             x,
                         int             y,
                         int             width,
                         uint32_t *      buffer,
                         const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    const uint32_t *pixel = (uint32_t *)bits + x;
    const uint32_t *end = pixel + width;
    
    while (pixel < end)
    {
	uint32_t p = READ (image, pixel++);
	
	*buffer++ = (0xff000000 |
	             ((p & 0xff000000) >> 24)	|
	             ((p & 0x00ff0000) >> 8)	|
	             ((p & 0x0000ff00) << 8));
    }
}

static void
fetch_scanline_x14r6g6b6 (pixman_image_t *image,
                          int             x,
                          int             y,
                          int             width,
                          uint32_t *      buffer,
                          const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    const uint32_t *pixel = (const uint32_t *)bits + x;
    const uint32_t *end = pixel + width;

    while (pixel < end)
    {
	uint32_t p = READ (image, pixel++);
	uint32_t r, g, b;

	r = ((p & 0x3f000) << 6) | ((p & 0x30000));
	g = ((p & 0x00fc0) << 4) | ((p & 0x00c00) >> 2);
	b = ((p & 0x0003f) << 2) | ((p & 0x00030) >> 4);

	*buffer++ = 0xff000000 | r | g | b;
    }
}

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
fetch_scanline_r8g8b8 (pixman_image_t *image,
                       int             x,
                       int             y,
                       int             width,
                       uint32_t *      buffer,
                       const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    const uint8_t *pixel = (const uint8_t *)bits + 3 * x;
    const uint8_t *end = pixel + 3 * width;
    
    while (pixel < end)
    {
	uint32_t b = 0xff000000;
	
#ifdef WORDS_BIGENDIAN
	b |= (READ (image, pixel++) << 16);
	b |= (READ (image, pixel++) << 8);
	b |= (READ (image, pixel++));
#else
	b |= (READ (image, pixel++));
	b |= (READ (image, pixel++) << 8);
	b |= (READ (image, pixel++) << 16);
#endif
	
	*buffer++ = b;
    }
}

static void
fetch_scanline_b8g8r8 (pixman_image_t *image,
                       int             x,
                       int             y,
                       int             width,
                       uint32_t *      buffer,
                       const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    const uint8_t *pixel = (const uint8_t *)bits + 3 * x;
    const uint8_t *end = pixel + 3 * width;
    
    while (pixel < end)
    {
	uint32_t b = 0xff000000;
#ifdef WORDS_BIGENDIAN
	b |= (READ (image, pixel++));
	b |= (READ (image, pixel++) << 8);
	b |= (READ (image, pixel++) << 16);
#else
	b |= (READ (image, pixel++) << 16);
	b |= (READ (image, pixel++) << 8);
	b |= (READ (image, pixel++));
#endif
	*buffer++ = b;
    }
}

static void
fetch_scanline_r5g6b5 (pixman_image_t *image,
                       int             x,
                       int             y,
                       int             width,
                       uint32_t *      buffer,
                       const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    const uint16_t *pixel = (const uint16_t *)bits + x;
    const uint16_t *end = pixel + width;
    
    while (pixel < end)
    {
	uint32_t p = READ (image, pixel++);
	uint32_t r = (((p) << 3) & 0xf8) |
	    (((p) << 5) & 0xfc00) |
	    (((p) << 8) & 0xf80000);
	
	r |= (r >> 5) & 0x70007;
	r |= (r >> 6) & 0x300;
	
	*buffer++ = 0xff000000 | r;
    }
}

static void
fetch_scanline_b5g6r5 (pixman_image_t *image,
                       int             x,
                       int             y,
                       int             width,
                       uint32_t *      buffer,
                       const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    const uint16_t *pixel = (const uint16_t *)bits + x;
    const uint16_t *end = pixel + width;
    
    while (pixel < end)
    {
	uint32_t p = READ (image, pixel++);
	uint32_t r, g, b;
	
	b = ((p & 0xf800) | ((p & 0xe000) >> 5)) >> 8;
	g = ((p & 0x07e0) | ((p & 0x0600) >> 6)) << 5;
	r = ((p & 0x001c) | ((p & 0x001f) << 5)) << 14;
	
	*buffer++ = 0xff000000 | r | g | b;
    }
}

static void
fetch_scanline_a1r5g5b5 (pixman_image_t *image,
                         int             x,
                         int             y,
                         int             width,
                         uint32_t *      buffer,
                         const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    const uint16_t *pixel = (const uint16_t *)bits + x;
    const uint16_t *end = pixel + width;
    
    while (pixel < end)
    {
	uint32_t p = READ (image, pixel++);
	uint32_t r, g, b, a;
	
	a = (uint32_t) ((uint8_t) (0 - ((p & 0x8000) >> 15))) << 24;
	r = ((p & 0x7c00) | ((p & 0x7000) >> 5)) << 9;
	g = ((p & 0x03e0) | ((p & 0x0380) >> 5)) << 6;
	b = ((p & 0x001c) | ((p & 0x001f) << 5)) >> 2;
	
	*buffer++ = a | r | g | b;
    }
}

static void
fetch_scanline_x1r5g5b5 (pixman_image_t *image,
                         int             x,
                         int             y,
                         int             width,
                         uint32_t *      buffer,
                         const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    const uint16_t *pixel = (const uint16_t *)bits + x;
    const uint16_t *end = pixel + width;
    
    while (pixel < end)
    {
	uint32_t p = READ (image, pixel++);
	uint32_t r, g, b;
	
	r = ((p & 0x7c00) | ((p & 0x7000) >> 5)) << 9;
	g = ((p & 0x03e0) | ((p & 0x0380) >> 5)) << 6;
	b = ((p & 0x001c) | ((p & 0x001f) << 5)) >> 2;
	
	*buffer++ = 0xff000000 | r | g | b;
    }
}

static void
fetch_scanline_a1b5g5r5 (pixman_image_t *image,
                         int             x,
                         int             y,
                         int             width,
                         uint32_t *      buffer,
                         const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    const uint16_t *pixel = (const uint16_t *)bits + x;
    const uint16_t *end = pixel + width;
    uint32_t r, g, b, a;
    
    while (pixel < end)
    {
	uint32_t p = READ (image, pixel++);
	
	a = (uint32_t) ((uint8_t) (0 - ((p & 0x8000) >> 15))) << 24;
	b = ((p & 0x7c00) | ((p & 0x7000) >> 5)) >> 7;
	g = ((p & 0x03e0) | ((p & 0x0380) >> 5)) << 6;
	r = ((p & 0x001c) | ((p & 0x001f) << 5)) << 14;
	
	*buffer++ = a | r | g | b;
    }
}

static void
fetch_scanline_x1b5g5r5 (pixman_image_t *image,
                         int             x,
                         int             y,
                         int             width,
                         uint32_t *      buffer,
                         const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    const uint16_t *pixel = (const uint16_t *)bits + x;
    const uint16_t *end = pixel + width;
    
    while (pixel < end)
    {
	uint32_t p = READ (image, pixel++);
	uint32_t r, g, b;
	
	b = ((p & 0x7c00) | ((p & 0x7000) >> 5)) >> 7;
	g = ((p & 0x03e0) | ((p & 0x0380) >> 5)) << 6;
	r = ((p & 0x001c) | ((p & 0x001f) << 5)) << 14;
	
	*buffer++ = 0xff000000 | r | g | b;
    }
}

static void
fetch_scanline_a4r4g4b4 (pixman_image_t *image,
                         int             x,
                         int             y,
                         int             width,
                         uint32_t *      buffer,
                         const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    const uint16_t *pixel = (const uint16_t *)bits + x;
    const uint16_t *end = pixel + width;
    
    while (pixel < end)
    {
	uint32_t p = READ (image, pixel++);
	uint32_t r, g, b, a;
	
	a = ((p & 0xf000) | ((p & 0xf000) >> 4)) << 16;
	r = ((p & 0x0f00) | ((p & 0x0f00) >> 4)) << 12;
	g = ((p & 0x00f0) | ((p & 0x00f0) >> 4)) << 8;
	b = ((p & 0x000f) | ((p & 0x000f) << 4));
	
	*buffer++ = a | r | g | b;
    }
}

static void
fetch_scanline_x4r4g4b4 (pixman_image_t *image,
                         int             x,
                         int             y,
                         int             width,
                         uint32_t *      buffer,
                         const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    const uint16_t *pixel = (const uint16_t *)bits + x;
    const uint16_t *end = pixel + width;
    
    while (pixel < end)
    {
	uint32_t p = READ (image, pixel++);
	uint32_t r, g, b;
	
	r = ((p & 0x0f00) | ((p & 0x0f00) >> 4)) << 12;
	g = ((p & 0x00f0) | ((p & 0x00f0) >> 4)) << 8;
	b = ((p & 0x000f) | ((p & 0x000f) << 4));
	
	*buffer++ = 0xff000000 | r | g | b;
    }
}

static void
fetch_scanline_a4b4g4r4 (pixman_image_t *image,
                         int             x,
                         int             y,
                         int             width,
                         uint32_t *      buffer,
                         const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    const uint16_t *pixel = (const uint16_t *)bits + x;
    const uint16_t *end = pixel + width;
    
    while (pixel < end)
    {
	uint32_t p = READ (image, pixel++);
	uint32_t r, g, b, a;
	
	a = ((p & 0xf000) | ((p & 0xf000) >> 4)) << 16;
	b = ((p & 0x0f00) | ((p & 0x0f00) >> 4)) >> 4;
	g = ((p & 0x00f0) | ((p & 0x00f0) >> 4)) << 8;
	r = ((p & 0x000f) | ((p & 0x000f) << 4)) << 16;
	
	*buffer++ = a | r | g | b;
    }
}

static void
fetch_scanline_x4b4g4r4 (pixman_image_t *image,
                         int             x,
                         int             y,
                         int             width,
                         uint32_t *      buffer,
                         const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    const uint16_t *pixel = (const uint16_t *)bits + x;
    const uint16_t *end = pixel + width;
    
    while (pixel < end)
    {
	uint32_t p = READ (image, pixel++);
	uint32_t r, g, b;
	
	b = ((p & 0x0f00) | ((p & 0x0f00) >> 4)) >> 4;
	g = ((p & 0x00f0) | ((p & 0x00f0) >> 4)) << 8;
	r = ((p & 0x000f) | ((p & 0x000f) << 4)) << 16;
	
	*buffer++ = 0xff000000 | r | g | b;
    }
}

static void
fetch_scanline_a8 (pixman_image_t *image,
                   int             x,
                   int             y,
                   int             width,
                   uint32_t *      buffer,
                   const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    const uint8_t *pixel = (const uint8_t *)bits + x;
    const uint8_t *end = pixel + width;
    
    while (pixel < end)
	*buffer++ = READ (image, pixel++) << 24;
}

static void
fetch_scanline_r3g3b2 (pixman_image_t *image,
                       int             x,
                       int             y,
                       int             width,
                       uint32_t *      buffer,
                       const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    const uint8_t *pixel = (const uint8_t *)bits + x;
    const uint8_t *end = pixel + width;
    
    while (pixel < end)
    {
	uint32_t p = READ (image, pixel++);
	uint32_t r, g, b;
	
	r = ((p & 0xe0) | ((p & 0xe0) >> 3) | ((p & 0xc0) >> 6)) << 16;
	g = ((p & 0x1c) | ((p & 0x18) >> 3) | ((p & 0x1c) << 3)) << 8;
	b = (((p & 0x03)     ) |
	     ((p & 0x03) << 2) |
	     ((p & 0x03) << 4) |
	     ((p & 0x03) << 6));
	
	*buffer++ = 0xff000000 | r | g | b;
    }
}

static void
fetch_scanline_b2g3r3 (pixman_image_t *image,
                       int             x,
                       int             y,
                       int             width,
                       uint32_t *      buffer,
                       const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    const uint8_t *pixel = (const uint8_t *)bits + x;
    const uint8_t *end = pixel + width;

    while (pixel < end)
    {
	uint32_t p = READ (image, pixel++);
	uint32_t r, g, b;

	b  = p & 0xc0;
	b |= b >> 2;
	b |= b >> 4;
	b &= 0xff;

	g  = (p & 0x38) << 10;
	g |= g >> 3;
	g |= g >> 6;
	g &= 0xff00;

	r  = (p & 0x7) << 21;
	r |= r >> 3;
	r |= r >> 6;
	r &= 0xff0000;

	*buffer++ = 0xff000000 | r | g | b;
    }
}

static void
fetch_scanline_a2r2g2b2 (pixman_image_t *image,
                         int             x,
                         int             y,
                         int             width,
                         uint32_t *      buffer,
                         const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    const uint8_t *pixel = (const uint8_t *)bits + x;
    const uint8_t *end = pixel + width;
    
    while (pixel < end)
    {
	uint32_t p = READ (image, pixel++);
	uint32_t a, r, g, b;
	
	a = ((p & 0xc0) * 0x55) << 18;
	r = ((p & 0x30) * 0x55) << 12;
	g = ((p & 0x0c) * 0x55) << 6;
	b = ((p & 0x03) * 0x55);
	
	*buffer++ = a | r | g | b;
    }
}

static void
fetch_scanline_a2b2g2r2 (pixman_image_t *image,
                         int             x,
                         int             y,
                         int             width,
                         uint32_t *      buffer,
                         const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    const uint8_t *pixel = (const uint8_t *)bits + x;
    const uint8_t *end = pixel + width;
    
    while (pixel < end)
    {
	uint32_t p = READ (image, pixel++);
	uint32_t a, r, g, b;
	
	a = ((p & 0xc0) * 0x55) << 18;
	b = ((p & 0x30) * 0x55) >> 4;
	g = ((p & 0x0c) * 0x55) << 6;
	r = ((p & 0x03) * 0x55) << 16;
	
	*buffer++ = a | r | g | b;
    }
}

static void
fetch_scanline_c8 (pixman_image_t *image,
                   int             x,
                   int             y,
                   int             width,
                   uint32_t *      buffer,
                   const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    const pixman_indexed_t * indexed = image->bits.indexed;
    const uint8_t *pixel = (const uint8_t *)bits + x;
    const uint8_t *end = pixel + width;
    
    while (pixel < end)
    {
	uint32_t p = READ (image, pixel++);
	
	*buffer++ = indexed->rgba[p];
    }
}

static void
fetch_scanline_x4a4 (pixman_image_t *image,
                     int             x,
                     int             y,
                     int             width,
                     uint32_t *      buffer,
                     const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    const uint8_t *pixel = (const uint8_t *)bits + x;
    const uint8_t *end = pixel + width;
   
    while (pixel < end)
    {
	uint8_t p = READ (image, pixel++) & 0xf;

	*buffer++ = (p | (p << 4)) << 24;
    }
}

#define FETCH_8(img,l,o)    (READ (img, (((uint8_t *)(l)) + ((o) >> 3))))
#ifdef WORDS_BIGENDIAN
#define FETCH_4(img,l,o)						\
    (((4 * (o)) & 4) ? (FETCH_8 (img,l, 4 * (o)) & 0xf) : (FETCH_8 (img,l,(4 * (o))) >> 4))
#else
#define FETCH_4(img,l,o)						\
    (((4 * (o)) & 4) ? (FETCH_8 (img, l, 4 * (o)) >> 4) : (FETCH_8 (img, l, (4 * (o))) & 0xf))
#endif

static void
fetch_scanline_a4 (pixman_image_t *image,
                   int             x,
                   int             y,
                   int             width,
                   uint32_t *      buffer,
                   const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    int i;

    for (i = 0; i < width; ++i)
    {
	uint32_t p = FETCH_4 (image, bits, i + x);

	p |= p << 4;

	*buffer++ = p << 24;
    }
}

static void
fetch_scanline_r1g2b1 (pixman_image_t *image,
                       int             x,
                       int             y,
                       int             width,
                       uint32_t *      buffer,
                       const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	uint32_t p = FETCH_4 (image, bits, i + x);
	uint32_t r, g, b;
	
	r = ((p & 0x8) * 0xff) << 13;
	g = ((p & 0x6) * 0x55) << 7;
	b = ((p & 0x1) * 0xff);
	
	*buffer++ = 0xff000000 | r | g | b;
    }
}

static void
fetch_scanline_b1g2r1 (pixman_image_t *image,
                       int             x,
                       int             y,
                       int             width,
                       uint32_t *      buffer,
                       const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	uint32_t p = FETCH_4 (image, bits, i + x);
	uint32_t r, g, b;
	
	b = ((p & 0x8) * 0xff) >> 3;
	g = ((p & 0x6) * 0x55) << 7;
	r = ((p & 0x1) * 0xff) << 16;

	*buffer++ = 0xff000000 | r | g | b;
    }
}

static void
fetch_scanline_a1r1g1b1 (pixman_image_t *image,
                         int             x,
                         int             y,
                         int             width,
                         uint32_t *      buffer,
                         const uint32_t *mask)
{
    uint32_t a, r, g, b;
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    int i;

    for (i = 0; i < width; ++i)
    {
	uint32_t p = FETCH_4 (image, bits, i + x);

	a = ((p & 0x8) * 0xff) << 21;
	r = ((p & 0x4) * 0xff) << 14;
	g = ((p & 0x2) * 0xff) << 7;
	b = ((p & 0x1) * 0xff);

	*buffer++ = a | r | g | b;
    }
}

static void
fetch_scanline_a1b1g1r1 (pixman_image_t *image,
                         int             x,
                         int             y,
                         int             width,
                         uint32_t *      buffer,
                         const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    int i;

    for (i = 0; i < width; ++i)
    {
	uint32_t p = FETCH_4 (image, bits, i + x);
	uint32_t a, r, g, b;

	a = ((p & 0x8) * 0xff) << 21;
	b = ((p & 0x4) * 0xff) >> 2;
	g = ((p & 0x2) * 0xff) << 7;
	r = ((p & 0x1) * 0xff) << 16;

	*buffer++ = a | r | g | b;
    }
}

static void
fetch_scanline_c4 (pixman_image_t *image,
                   int             x,
                   int             y,
                   int             width,
                   uint32_t *      buffer,
                   const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    const pixman_indexed_t * indexed = image->bits.indexed;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	uint32_t p = FETCH_4 (image, bits, i + x);
	
	*buffer++ = indexed->rgba[p];
    }
}

static void
fetch_scanline_a1 (pixman_image_t *image,
                   int             x,
                   int             y,
                   int             width,
                   uint32_t *      buffer,
                   const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	uint32_t p = READ (image, bits + ((i + x) >> 5));
	uint32_t a;
	
#ifdef WORDS_BIGENDIAN
	a = p >> (0x1f - ((i + x) & 0x1f));
#else
	a = p >> ((i + x) & 0x1f);
#endif
	a = a & 1;
	a |= a << 1;
	a |= a << 2;
	a |= a << 4;
	
	*buffer++ = a << 24;
    }
}

static void
fetch_scanline_g1 (pixman_image_t *image,
                   int             x,
                   int             y,
                   int             width,
                   uint32_t *      buffer,
                   const uint32_t *mask)
{
    const uint32_t *bits = image->bits.bits + y * image->bits.rowstride;
    const pixman_indexed_t * indexed = image->bits.indexed;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	uint32_t p = READ (image, bits + ((i + x) >> 5));
	uint32_t a;
	
#ifdef WORDS_BIGENDIAN
	a = p >> (0x1f - ((i + x) & 0x1f));
#else
	a = p >> ((i + x) & 0x1f);
#endif
	a = a & 1;
	
	*buffer++ = indexed->rgba[a];
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
fetch_pixel_a8r8g8b8 (bits_image_t *image,
		      int           offset,
		      int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    return READ (image, (uint32_t *)bits + offset);
}

static uint32_t
fetch_pixel_x8r8g8b8 (bits_image_t *image,
		      int           offset,
		      int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;

    return READ (image, (uint32_t *)bits + offset) | 0xff000000;
}

static uint32_t
fetch_pixel_a8b8g8r8 (bits_image_t *image,
		      int           offset,
		      int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t pixel = READ (image, (uint32_t *)bits + offset);
    
    return ((pixel & 0xff000000) |
	    ((pixel >> 16) & 0xff) |
	    (pixel & 0x0000ff00) |
	    ((pixel & 0xff) << 16));
}

static uint32_t
fetch_pixel_x8b8g8r8 (bits_image_t *image,
		      int           offset,
		      int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t pixel = READ (image, (uint32_t *)bits + offset);
    
    return ((0xff000000) |
	    ((pixel >> 16) & 0xff) |
	    (pixel & 0x0000ff00) |
	    ((pixel & 0xff) << 16));
}

static uint32_t
fetch_pixel_b8g8r8a8 (bits_image_t *image,
		      int           offset,
		      int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t pixel = READ (image, (uint32_t *)bits + offset);
    
    return ((pixel & 0xff000000) >> 24 |
	    (pixel & 0x00ff0000) >> 8 |
	    (pixel & 0x0000ff00) << 8 |
	    (pixel & 0x000000ff) << 24);
}

static uint32_t
fetch_pixel_b8g8r8x8 (bits_image_t *image,
		      int           offset,
		      int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t pixel = READ (image, (uint32_t *)bits + offset);
    
    return ((0xff000000) |
	    (pixel & 0xff000000) >> 24 |
	    (pixel & 0x00ff0000) >> 8 |
	    (pixel & 0x0000ff00) << 8);
}

static uint32_t
fetch_pixel_x14r6g6b6 (bits_image_t *image,
                       int           offset,
                       int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t pixel = READ (image, (uint32_t *) bits + offset);
    uint32_t r, g, b;

    r = ((pixel & 0x3f000) << 6) | ((pixel & 0x30000));
    g = ((pixel & 0x00fc0) << 4) | ((pixel & 0x00c00) >> 2);
    b = ((pixel & 0x0003f) << 2) | ((pixel & 0x00030) >> 4);

    return 0xff000000 | r | g | b;
}

static uint32_t
fetch_pixel_r8g8b8 (bits_image_t *image,
		    int           offset,
		    int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint8_t   *pixel = ((uint8_t *) bits) + (offset * 3);
    
#ifdef WORDS_BIGENDIAN
    return (0xff000000 |
	    (READ (image, pixel + 0) << 16) |
	    (READ (image, pixel + 1) << 8) |
	    (READ (image, pixel + 2)));
#else
    return (0xff000000 |
	    (READ (image, pixel + 2) << 16) |
	    (READ (image, pixel + 1) << 8) |
	    (READ (image, pixel + 0)));
#endif
}

static uint32_t
fetch_pixel_b8g8r8 (bits_image_t *image,
		    int           offset,
		    int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint8_t   *pixel = ((uint8_t *) bits) + (offset * 3);
#ifdef WORDS_BIGENDIAN
    return (0xff000000 |
	    (READ (image, pixel + 2) << 16) |
	    (READ (image, pixel + 1) << 8) |
	    (READ (image, pixel + 0)));
#else
    return (0xff000000 |
	    (READ (image, pixel + 0) << 16) |
	    (READ (image, pixel + 1) << 8) |
	    (READ (image, pixel + 2)));
#endif
}

static uint32_t
fetch_pixel_r5g6b5 (bits_image_t *image,
		    int           offset,
		    int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t pixel = READ (image, (uint16_t *) bits + offset);
    uint32_t r, g, b;
    
    r = ((pixel & 0xf800) | ((pixel & 0xe000) >> 5)) << 8;
    g = ((pixel & 0x07e0) | ((pixel & 0x0600) >> 6)) << 5;
    b = ((pixel & 0x001c) | ((pixel & 0x001f) << 5)) >> 2;
    
    return (0xff000000 | r | g | b);
}

static uint32_t
fetch_pixel_b5g6r5 (bits_image_t *image,
		    int           offset,
		    int           line)
{
    uint32_t r, g, b;
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t pixel = READ (image, (uint16_t *) bits + offset);
    
    b = ((pixel & 0xf800) | ((pixel & 0xe000) >> 5)) >> 8;
    g = ((pixel & 0x07e0) | ((pixel & 0x0600) >> 6)) << 5;
    r = ((pixel & 0x001c) | ((pixel & 0x001f) << 5)) << 14;
    
    return (0xff000000 | r | g | b);
}

static uint32_t
fetch_pixel_a1r5g5b5 (bits_image_t *image,
		      int           offset,
		      int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t pixel = READ (image, (uint16_t *) bits + offset);
    uint32_t a, r, g, b;
    
    a = (uint32_t) ((uint8_t) (0 - ((pixel & 0x8000) >> 15))) << 24;
    r = ((pixel & 0x7c00) | ((pixel & 0x7000) >> 5)) << 9;
    g = ((pixel & 0x03e0) | ((pixel & 0x0380) >> 5)) << 6;
    b = ((pixel & 0x001c) | ((pixel & 0x001f) << 5)) >> 2;
    
    return (a | r | g | b);
}

static uint32_t
fetch_pixel_x1r5g5b5 (bits_image_t *image,
		      int           offset,
		      int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t pixel = READ (image, (uint16_t *) bits + offset);
    uint32_t r, g, b;
    
    r = ((pixel & 0x7c00) | ((pixel & 0x7000) >> 5)) << 9;
    g = ((pixel & 0x03e0) | ((pixel & 0x0380) >> 5)) << 6;
    b = ((pixel & 0x001c) | ((pixel & 0x001f) << 5)) >> 2;
    
    return (0xff000000 | r | g | b);
}

static uint32_t
fetch_pixel_a1b5g5r5 (bits_image_t *image,
		      int           offset,
		      int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t pixel = READ (image, (uint16_t *) bits + offset);
    uint32_t a, r, g, b;
    
    a = (uint32_t) ((uint8_t) (0 - ((pixel & 0x8000) >> 15))) << 24;
    b = ((pixel & 0x7c00) | ((pixel & 0x7000) >> 5)) >> 7;
    g = ((pixel & 0x03e0) | ((pixel & 0x0380) >> 5)) << 6;
    r = ((pixel & 0x001c) | ((pixel & 0x001f) << 5)) << 14;
    
    return (a | r | g | b);
}

static uint32_t
fetch_pixel_x1b5g5r5 (bits_image_t *image,
		      int           offset,
		      int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t pixel = READ (image, (uint16_t *) bits + offset);
    uint32_t r, g, b;
    
    b = ((pixel & 0x7c00) | ((pixel & 0x7000) >> 5)) >> 7;
    g = ((pixel & 0x03e0) | ((pixel & 0x0380) >> 5)) << 6;
    r = ((pixel & 0x001c) | ((pixel & 0x001f) << 5)) << 14;
    
    return (0xff000000 | r | g | b);
}

static uint32_t
fetch_pixel_a4r4g4b4 (bits_image_t *image,
		      int           offset,
		      int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t pixel = READ (image, (uint16_t *) bits + offset);
    uint32_t a, r, g, b;
    
    a = ((pixel & 0xf000) | ((pixel & 0xf000) >> 4)) << 16;
    r = ((pixel & 0x0f00) | ((pixel & 0x0f00) >> 4)) << 12;
    g = ((pixel & 0x00f0) | ((pixel & 0x00f0) >> 4)) << 8;
    b = ((pixel & 0x000f) | ((pixel & 0x000f) << 4));
    
    return (a | r | g | b);
}

static uint32_t
fetch_pixel_x4r4g4b4 (bits_image_t *image,
		      int           offset,
		      int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t pixel = READ (image, (uint16_t *) bits + offset);
    uint32_t r, g, b;
    
    r = ((pixel & 0x0f00) | ((pixel & 0x0f00) >> 4)) << 12;
    g = ((pixel & 0x00f0) | ((pixel & 0x00f0) >> 4)) << 8;
    b = ((pixel & 0x000f) | ((pixel & 0x000f) << 4));
    
    return (0xff000000 | r | g | b);
}

static uint32_t
fetch_pixel_a4b4g4r4 (bits_image_t *image,
		      int           offset,
		      int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t pixel = READ (image, (uint16_t *) bits + offset);
    uint32_t a, r, g, b;
    
    a = ((pixel & 0xf000) | ((pixel & 0xf000) >> 4)) << 16;
    b = ((pixel & 0x0f00) | ((pixel & 0x0f00) >> 4)) >> 4;
    g = ((pixel & 0x00f0) | ((pixel & 0x00f0) >> 4)) << 8;
    r = ((pixel & 0x000f) | ((pixel & 0x000f) << 4)) << 16;
    
    return (a | r | g | b);
}

static uint32_t
fetch_pixel_x4b4g4r4 (bits_image_t *image,
		      int           offset,
		      int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t pixel = READ (image, (uint16_t *) bits + offset);
    uint32_t r, g, b;
    
    b = ((pixel & 0x0f00) | ((pixel & 0x0f00) >> 4)) >> 4;
    g = ((pixel & 0x00f0) | ((pixel & 0x00f0) >> 4)) << 8;
    r = ((pixel & 0x000f) | ((pixel & 0x000f) << 4)) << 16;
    
    return (0xff000000 | r | g | b);
}

static uint32_t
fetch_pixel_a8 (bits_image_t *image,
		int           offset,
		int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t pixel = READ (image, (uint8_t *) bits + offset);
    
    return pixel << 24;
}

static uint32_t
fetch_pixel_r3g3b2 (bits_image_t *image,
		    int           offset,
		    int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t pixel = READ (image, (uint8_t *) bits + offset);
    uint32_t r, g, b;
    
    r = ((pixel & 0xe0) |
	 ((pixel & 0xe0) >> 3) |
	 ((pixel & 0xc0) >> 6)) << 16;
    
    g = ((pixel & 0x1c) |
	 ((pixel & 0x18) >> 3) |
	 ((pixel & 0x1c) << 3)) << 8;
    
    b = (((pixel & 0x03)     ) |
	 ((pixel & 0x03) << 2) |
	 ((pixel & 0x03) << 4) |
	 ((pixel & 0x03) << 6));
    
    return (0xff000000 | r | g | b);
}

static uint32_t
fetch_pixel_b2g3r3 (bits_image_t *image,
		    int           offset,
		    int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t p = READ (image, (uint8_t *) bits + offset);
    uint32_t r, g, b;

    b  = p & 0xc0;
    b |= b >> 2;
    b |= b >> 4;
    b &= 0xff;

    g  = (p & 0x38) << 10;
    g |= g >> 3;
    g |= g >> 6;
    g &= 0xff00;

    r  = (p & 0x7) << 21;
    r |= r >> 3;
    r |= r >> 6;
    r &= 0xff0000;

    return 0xff000000 | r | g | b;
}

static uint32_t
fetch_pixel_a2r2g2b2 (bits_image_t *image,
		      int           offset,
		      int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t pixel = READ (image, (uint8_t *) bits + offset);
    uint32_t a, r, g, b;
    
    a = ((pixel & 0xc0) * 0x55) << 18;
    r = ((pixel & 0x30) * 0x55) << 12;
    g = ((pixel & 0x0c) * 0x55) << 6;
    b = ((pixel & 0x03) * 0x55);
    
    return a | r | g | b;
}

static uint32_t
fetch_pixel_a2b2g2r2 (bits_image_t *image,
		      int           offset,
		      int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t pixel = READ (image, (uint8_t *) bits + offset);
    uint32_t a, r, g, b;
    
    a = ((pixel & 0xc0) * 0x55) << 18;
    b = ((pixel & 0x30) * 0x55) >> 4;
    g = ((pixel & 0x0c) * 0x55) << 6;
    r = ((pixel & 0x03) * 0x55) << 16;
    
    return a | r | g | b;
}

static uint32_t
fetch_pixel_c8 (bits_image_t *image,
		int           offset,
		int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t pixel = READ (image, (uint8_t *) bits + offset);
    const pixman_indexed_t * indexed = image->indexed;
    
    return indexed->rgba[pixel];
}

static uint32_t
fetch_pixel_x4a4 (bits_image_t *image,
		  int           offset,
		  int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t pixel = READ (image, (uint8_t *) bits + offset);
    
    return ((pixel & 0xf) | ((pixel & 0xf) << 4)) << 24;
}

static uint32_t
fetch_pixel_a4 (bits_image_t *image,
		int           offset,
		int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t pixel = FETCH_4 (image, bits, offset);
    
    pixel |= pixel << 4;
    return pixel << 24;
}

static uint32_t
fetch_pixel_r1g2b1 (bits_image_t *image,
		    int           offset,
		    int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t pixel = FETCH_4 (image, bits, offset);
    uint32_t r, g, b;
    
    r = ((pixel & 0x8) * 0xff) << 13;
    g = ((pixel & 0x6) * 0x55) << 7;
    b = ((pixel & 0x1) * 0xff);
    
    return 0xff000000 | r | g | b;
}

static uint32_t
fetch_pixel_b1g2r1 (bits_image_t *image,
		    int           offset,
		    int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t pixel = FETCH_4 (image, bits, offset);
    uint32_t r, g, b;
    
    b = ((pixel & 0x8) * 0xff) >> 3;
    g = ((pixel & 0x6) * 0x55) << 7;
    r = ((pixel & 0x1) * 0xff) << 16;
    
    return 0xff000000 | r | g | b;
}

static uint32_t
fetch_pixel_a1r1g1b1 (bits_image_t *image,
		      int           offset,
		      int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t pixel = FETCH_4 (image, bits, offset);
    uint32_t a, r, g, b;

    a = ((pixel & 0x8) * 0xff) << 21;
    r = ((pixel & 0x4) * 0xff) << 14;
    g = ((pixel & 0x2) * 0xff) << 7;
    b = ((pixel & 0x1) * 0xff);

    return a | r | g | b;
}

static uint32_t
fetch_pixel_a1b1g1r1 (bits_image_t *image,
		      int           offset,
		      int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t pixel = FETCH_4 (image, bits, offset);
    uint32_t a, r, g, b;

    a = ((pixel & 0x8) * 0xff) << 21;
    b = ((pixel & 0x4) * 0xff) >> 2;
    g = ((pixel & 0x2) * 0xff) << 7;
    r = ((pixel & 0x1) * 0xff) << 16;

    return a | r | g | b;
}

static uint32_t
fetch_pixel_c4 (bits_image_t *image,
		int           offset,
		int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t pixel = FETCH_4 (image, bits, offset);
    const pixman_indexed_t * indexed = image->indexed;

    return indexed->rgba[pixel];
}

static uint32_t
fetch_pixel_a1 (bits_image_t *image,
		int           offset,
		int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t pixel = READ (image, bits + (offset >> 5));
    uint32_t a;
    
#ifdef WORDS_BIGENDIAN
    a = pixel >> (0x1f - (offset & 0x1f));
#else
    a = pixel >> (offset & 0x1f);
#endif
    a = a & 1;
    a |= a << 1;
    a |= a << 2;
    a |= a << 4;
    
    return a << 24;
}

static uint32_t
fetch_pixel_g1 (bits_image_t *image,
		int           offset,
		int           line)
{
    uint32_t *bits = image->bits + line * image->rowstride;
    uint32_t pixel = READ (image, bits + (offset >> 5));
    const pixman_indexed_t * indexed = image->indexed;
    uint32_t a;
    
#ifdef WORDS_BIGENDIAN
    a = pixel >> (0x1f - (offset & 0x1f));
#else
    a = pixel >> (offset & 0x1f);
#endif
    a = a & 1;
    
    return indexed->rgba[a];
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

#define SPLIT_A(v)              \
    uint32_t a = ((v) >> 24),   \
	r = ((v) >> 16) & 0xff, \
	g = ((v) >> 8) & 0xff,  \
	b = (v) & 0xff

#define SPLIT(v)                     \
    uint32_t r = ((v) >> 16) & 0xff, \
	g = ((v) >> 8) & 0xff,       \
	b = (v) & 0xff

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

static void
store_scanline_a8r8g8b8 (bits_image_t *  image,
                         int             x,
                         int             y,
                         int             width,
                         const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    
    MEMCPY_WRAPPED (image, ((uint32_t *)bits) + x, values,
                    width * sizeof(uint32_t));
}

static void
store_scanline_x8r8g8b8 (bits_image_t *  image,
                         int             x,
                         int             y,
                         int             width,
                         const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    uint32_t *pixel = (uint32_t *)bits + x;
    int i;
    
    for (i = 0; i < width; ++i)
	WRITE (image, pixel++, values[i] & 0xffffff);
}

static void
store_scanline_a8b8g8r8 (bits_image_t *  image,
                         int             x,
                         int             y,
                         int             width,
                         const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    uint32_t *pixel = (uint32_t *)bits + x;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	WRITE (image, pixel++,
	       (values[i] & 0xff00ff00)         |
	       ((values[i] >> 16) & 0xff)       |
	       ((values[i] & 0xff) << 16));
    }
}

static void
store_scanline_x8b8g8r8 (bits_image_t *  image,
                         int             x,
                         int             y,
                         int             width,
                         const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    uint32_t *pixel = (uint32_t *)bits + x;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	WRITE (image, pixel++,
	       (values[i] & 0x0000ff00)         |
	       ((values[i] >> 16) & 0xff)       |
	       ((values[i] & 0xff) << 16));
    }
}

static void
store_scanline_b8g8r8a8 (bits_image_t *  image,
                         int             x,
                         int             y,
                         int             width,
                         const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    uint32_t *pixel = (uint32_t *)bits + x;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	WRITE (image, pixel++,
	       ((values[i] >> 24) & 0x000000ff) |
	       ((values[i] >>  8) & 0x0000ff00) |
	       ((values[i] <<  8) & 0x00ff0000) |
	       ((values[i] << 24) & 0xff000000));
    }
}

static void
store_scanline_b8g8r8x8 (bits_image_t *  image,
                         int             x,
                         int             y,
                         int             width,
                         const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    uint32_t *pixel = (uint32_t *)bits + x;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	WRITE (image, pixel++,
	       ((values[i] >>  8) & 0x0000ff00) |
	       ((values[i] <<  8) & 0x00ff0000) |
	       ((values[i] << 24) & 0xff000000));
    }
}

static void
store_scanline_x14r6g6b6 (bits_image_t *  image,
                          int             x,
                          int             y,
                          int             width,
                          const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    uint32_t *pixel = ((uint32_t *) bits) + x;
    int i;

    for (i = 0; i < width; ++i)
    {
	uint32_t s = values[i];
	uint32_t r, g, b;

	r = (s & 0xfc0000) >> 6;
	g = (s & 0x00fc00) >> 4;
	b = (s & 0x0000fc) >> 2;

	WRITE (image, pixel++, r | g | b);
    }
}

static void
store_scanline_r8g8b8 (bits_image_t *  image,
                       int             x,
                       int             y,
                       int             width,
                       const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    uint8_t *pixel = ((uint8_t *) bits) + 3 * x;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	uint32_t val = values[i];
	
#ifdef WORDS_BIGENDIAN
	WRITE (image, pixel++, (val & 0x00ff0000) >> 16);
	WRITE (image, pixel++, (val & 0x0000ff00) >>  8);
	WRITE (image, pixel++, (val & 0x000000ff) >>  0);
#else
	WRITE (image, pixel++, (val & 0x000000ff) >>  0);
	WRITE (image, pixel++, (val & 0x0000ff00) >>  8);
	WRITE (image, pixel++, (val & 0x00ff0000) >> 16);
#endif
    }
}

static void
store_scanline_b8g8r8 (bits_image_t *  image,
                       int             x,
                       int             y,
                       int             width,
                       const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    uint8_t *pixel = ((uint8_t *) bits) + 3 * x;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	uint32_t val = values[i];
	
#ifdef WORDS_BIGENDIAN
	WRITE (image, pixel++, (val & 0x000000ff) >>  0);
	WRITE (image, pixel++, (val & 0x0000ff00) >>  8);
	WRITE (image, pixel++, (val & 0x00ff0000) >> 16);
#else
	WRITE (image, pixel++, (val & 0x00ff0000) >> 16);
	WRITE (image, pixel++, (val & 0x0000ff00) >>  8);
	WRITE (image, pixel++, (val & 0x000000ff) >>  0);
#endif
    }
}

static void
store_scanline_r5g6b5 (bits_image_t *  image,
                       int             x,
                       int             y,
                       int             width,
                       const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    uint16_t *pixel = ((uint16_t *) bits) + x;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	uint32_t s = values[i];
	
	WRITE (image, pixel++,
	       ((s >> 3) & 0x001f) |
	       ((s >> 5) & 0x07e0) |
	       ((s >> 8) & 0xf800));
    }
}

static void
store_scanline_b5g6r5 (bits_image_t *  image,
                       int             x,
                       int             y,
                       int             width,
                       const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    uint16_t  *pixel = ((uint16_t *) bits) + x;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	SPLIT (values[i]);
	
	WRITE (image, pixel++,
	       ((b << 8) & 0xf800) |
	       ((g << 3) & 0x07e0) |
	       ((r >> 3)         ));
    }
}

static void
store_scanline_a1r5g5b5 (bits_image_t *  image,
                         int             x,
                         int             y,
                         int             width,
                         const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    uint16_t  *pixel = ((uint16_t *) bits) + x;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	SPLIT_A (values[i]);
	
	WRITE (image, pixel++,
	       ((a << 8) & 0x8000) |
	       ((r << 7) & 0x7c00) |
	       ((g << 2) & 0x03e0) |
	       ((b >> 3)         ));
    }
}

static void
store_scanline_x1r5g5b5 (bits_image_t *  image,
                         int             x,
                         int             y,
                         int             width,
                         const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    uint16_t  *pixel = ((uint16_t *) bits) + x;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	SPLIT (values[i]);
	
	WRITE (image, pixel++,
	       ((r << 7) & 0x7c00) |
	       ((g << 2) & 0x03e0) |
	       ((b >> 3)         ));
    }
}

static void
store_scanline_a1b5g5r5 (bits_image_t *  image,
                         int             x,
                         int             y,
                         int             width,
                         const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    uint16_t  *pixel = ((uint16_t *) bits) + x;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	SPLIT_A (values[i]);
	
	WRITE (image, pixel++,
	       ((a << 8) & 0x8000) |
	       ((b << 7) & 0x7c00) |
	       ((g << 2) & 0x03e0) |
	       ((r >> 3)         ));
    }
}

static void
store_scanline_x1b5g5r5 (bits_image_t *  image,
                         int             x,
                         int             y,
                         int             width,
                         const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    uint16_t  *pixel = ((uint16_t *) bits) + x;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	SPLIT (values[i]);
	
	WRITE (image, pixel++, ((b << 7) & 0x7c00) |
	       ((g << 2) & 0x03e0) |
	       ((r >> 3)         ));
    }
}

static void
store_scanline_a4r4g4b4 (bits_image_t *  image,
                         int             x,
                         int             y,
                         int             width,
                         const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    uint16_t  *pixel = ((uint16_t *) bits) + x;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	SPLIT_A (values[i]);
	
	WRITE (image, pixel++,
	       ((a << 8) & 0xf000) |
	       ((r << 4) & 0x0f00) |
	       ((g     ) & 0x00f0) |
	       ((b >> 4)         ));
    }
}

static void
store_scanline_x4r4g4b4 (bits_image_t *  image,
                         int             x,
                         int             y,
                         int             width,
                         const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    uint16_t  *pixel = ((uint16_t *) bits) + x;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	SPLIT (values[i]);
	
	WRITE (image, pixel++,
	       ((r << 4) & 0x0f00) |
	       ((g     ) & 0x00f0) |
	       ((b >> 4)         ));
    }
}

static void
store_scanline_a4b4g4r4 (bits_image_t *  image,
                         int             x,
                         int             y,
                         int             width,
                         const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    uint16_t  *pixel = ((uint16_t *) bits) + x;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	SPLIT_A (values[i]);
	WRITE (image, pixel++, ((a << 8) & 0xf000) |
	       ((b << 4) & 0x0f00) |
	       ((g     ) & 0x00f0) |
	       ((r >> 4)         ));
    }
}

static void
store_scanline_x4b4g4r4 (bits_image_t *  image,
                         int             x,
                         int             y,
                         int             width,
                         const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    uint16_t  *pixel = ((uint16_t *) bits) + x;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	SPLIT (values[i]);
	
	WRITE (image, pixel++,
	       ((b << 4) & 0x0f00) |
	       ((g     ) & 0x00f0) |
	       ((r >> 4)         ));
    }
}

static void
store_scanline_a8 (bits_image_t *  image,
                   int             x,
                   int             y,
                   int             width,
                   const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    uint8_t   *pixel = ((uint8_t *) bits) + x;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	WRITE (image, pixel++, values[i] >> 24);
    }
}

static void
store_scanline_r3g3b2 (bits_image_t *  image,
                       int             x,
                       int             y,
                       int             width,
                       const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    uint8_t   *pixel = ((uint8_t *) bits) + x;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	SPLIT (values[i]);
	
	WRITE (image, pixel++,
	       ((r     ) & 0xe0) |
	       ((g >> 3) & 0x1c) |
	       ((b >> 6)       ));
    }
}

static void
store_scanline_b2g3r3 (bits_image_t *  image,
                       int             x,
                       int             y,
                       int             width,
                       const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    uint8_t   *pixel = ((uint8_t *) bits) + x;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	SPLIT (values[i]);
	
	WRITE (image, pixel++,
	       ((b     ) & 0xc0) |
	       ((g >> 2) & 0x38) |
	       ((r >> 5)       ));
    }
}

static void
store_scanline_a2r2g2b2 (bits_image_t *  image,
                         int             x,
                         int             y,
                         int             width,
                         const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    uint8_t   *pixel = ((uint8_t *) bits) + x;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	SPLIT_A (values[i]);
	
	WRITE (image, pixel++,
	       ((a     ) & 0xc0) |
	       ((r >> 2) & 0x30) |
	       ((g >> 4) & 0x0c) |
	       ((b >> 6)       ));
    }
}

static void
store_scanline_a2b2g2r2 (bits_image_t *  image,
                         int             x,
                         int             y,
                         int             width,
                         const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    uint8_t   *pixel = ((uint8_t *) bits) + x;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	SPLIT_A (values[i]);
	
	WRITE (image, pixel++,
	       ((a     ) & 0xc0) |
	       ((b >> 2) & 0x30) |
	       ((g >> 4) & 0x0c) |
	       ((r >> 6)       ));
    }
}

static void
store_scanline_c8 (bits_image_t *  image,
                   int             x,
                   int             y,
                   int             width,
                   const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    uint8_t *pixel = ((uint8_t *) bits) + x;
    const pixman_indexed_t *indexed = image->indexed;
    int i;
    
    for (i = 0; i < width; ++i)
	WRITE (image, pixel++, RGB24_TO_ENTRY (indexed,values[i]));
}

static void
store_scanline_g8 (bits_image_t *  image,
                   int             x,
                   int             y,
                   int             width,
                   const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    uint8_t *pixel = ((uint8_t *) bits) + x;
    const pixman_indexed_t *indexed = image->indexed;
    int i;

    for (i = 0; i < width; ++i)
	WRITE (image, pixel++, RGB24_TO_ENTRY_Y (indexed,values[i]));
}

static void
store_scanline_x4a4 (bits_image_t *  image,
                     int             x,
                     int             y,
                     int             width,
                     const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    uint8_t   *pixel = ((uint8_t *) bits) + x;
    int i;

    for (i = 0; i < width; ++i)
	WRITE (image, pixel++, values[i] >> 28);
}

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

static void
store_scanline_a4 (bits_image_t *  image,
                   int             x,
                   int             y,
                   int             width,
                   const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    int i;

    for (i = 0; i < width; ++i)
	STORE_4 (image, bits, i + x, values[i] >> 28);
}

static void
store_scanline_r1g2b1 (bits_image_t *  image,
                       int             x,
                       int             y,
                       int             width,
                       const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    int i;

    for (i = 0; i < width; ++i)
    {
	uint32_t pixel;

	SPLIT (values[i]);
	pixel = (((r >> 4) & 0x8) |
	         ((g >> 5) & 0x6) |
	         ((b >> 7)      ));
	STORE_4 (image, bits, i + x, pixel);
    }
}

static void
store_scanline_b1g2r1 (bits_image_t *  image,
                       int             x,
                       int             y,
                       int             width,
                       const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    int i;

    for (i = 0; i < width; ++i)
    {
	uint32_t pixel;

	SPLIT (values[i]);
	pixel = (((b >> 4) & 0x8) |
	         ((g >> 5) & 0x6) |
	         ((r >> 7)      ));
	STORE_4 (image, bits, i + x, pixel);
    }
}

static void
store_scanline_a1r1g1b1 (bits_image_t *  image,
                         int             x,
                         int             y,
                         int             width,
                         const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    int i;

    for (i = 0; i < width; ++i)
    {
	uint32_t pixel;

	SPLIT_A (values[i]);
	pixel = (((a >> 4) & 0x8) |
	         ((r >> 5) & 0x4) |
	         ((g >> 6) & 0x2) |
	         ((b >> 7)      ));

	STORE_4 (image, bits, i + x, pixel);
    }
}

static void
store_scanline_a1b1g1r1 (bits_image_t *  image,
                         int             x,
                         int             y,
                         int             width,
                         const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    int i;

    for (i = 0; i < width; ++i)
    {
	uint32_t pixel;

	SPLIT_A (values[i]);
	pixel = (((a >> 4) & 0x8) |
	         ((b >> 5) & 0x4) |
	         ((g >> 6) & 0x2) |
	         ((r >> 7)      ));

	STORE_4 (image, bits, i + x, pixel);
    }
}

static void
store_scanline_c4 (bits_image_t *  image,
                   int             x,
                   int             y,
                   int             width,
                   const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    const pixman_indexed_t *indexed = image->indexed;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	uint32_t pixel;
	
	pixel = RGB24_TO_ENTRY (indexed, values[i]);
	STORE_4 (image, bits, i + x, pixel);
    }
}

static void
store_scanline_g4 (bits_image_t *  image,
                   int             x,
                   int             y,
                   int             width,
                   const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    const pixman_indexed_t *indexed = image->indexed;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	uint32_t pixel;
	
	pixel = RGB24_TO_ENTRY_Y (indexed, values[i]);
	STORE_4 (image, bits, i + x, pixel);
    }
}

static void
store_scanline_a1 (bits_image_t *  image,
                   int             x,
                   int             y,
                   int             width,
                   const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	uint32_t  *pixel = ((uint32_t *) bits) + ((i + x) >> 5);
	uint32_t mask, v;
	
#ifdef WORDS_BIGENDIAN
	mask = 1 << (0x1f - ((i + x) & 0x1f));
#else
	mask = 1 << ((i + x) & 0x1f);
#endif
	v = values[i] & 0x80000000 ? mask : 0;
	
	WRITE (image, pixel, (READ (image, pixel) & ~mask) | v);
    }
}

static void
store_scanline_g1 (bits_image_t *  image,
                   int             x,
                   int             y,
                   int             width,
                   const uint32_t *values)
{
    uint32_t *bits = image->bits + image->rowstride * y;
    const pixman_indexed_t *indexed = image->indexed;
    int i;
    
    for (i = 0; i < width; ++i)
    {
	uint32_t  *pixel = ((uint32_t *) bits) + ((i + x) >> 5);
	uint32_t mask, v;
	
#ifdef WORDS_BIGENDIAN
	mask = 1 << (0x1f - ((i + x) & 0x1f));
#else
	mask = 1 << ((i + x) & 0x1f);
#endif
	v = RGB24_TO_ENTRY_Y (indexed, values[i]) & 0x1 ? mask : 0;
	
	WRITE (image, pixel, (READ (image, pixel) & ~mask) | v);
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
    
#define fetch_scanline_g8 fetch_scanline_c8
#define fetch_pixel_g8 fetch_pixel_c8
    FORMAT_INFO (g8),
    
#define fetch_scanline_x4c4 fetch_scanline_c8
#define fetch_pixel_x4c4 fetch_pixel_c8
#define store_scanline_x4c4 store_scanline_c8
    FORMAT_INFO (x4c4),
    
#define fetch_scanline_x4g4 fetch_scanline_c8
#define fetch_pixel_x4g4 fetch_pixel_c8
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
    
#define fetch_scanline_g4 fetch_scanline_c4
#define fetch_pixel_g4 fetch_pixel_c4
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
