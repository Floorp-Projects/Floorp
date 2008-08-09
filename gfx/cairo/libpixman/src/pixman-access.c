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

#define Red(x) (((x) >> 16) & 0xff)
#define Green(x) (((x) >> 8) & 0xff)
#define Blue(x) ((x) & 0xff)

/*
 * YV12 setup and access macros
 */

#define YV12_SETUP(pict) \
	uint32_t *bits = pict->bits; \
	int stride = pict->rowstride; \
	int offset0 = stride < 0 ? \
		((-stride) >> 1) * ((pict->height - 1) >> 1) - stride : \
		stride * pict->height; \
	int offset1 = stride < 0 ? \
		offset0 + ((-stride) >> 1) * ((pict->height) >> 1) : \
		offset0 + (offset0 >> 2)
/* Note n trailing semicolon on the above macro; if it's there, then
 * the typical usage of YV12_SETUP(pict); will have an extra trailing ;
 * that some compilers will interpret as a statement -- and then any further
 * variable declarations will cause an error.
 */

#define YV12_Y(line)		\
    ((uint8_t *) ((bits) + (stride) * (line)))

#define YV12_U(line)	      \
    ((uint8_t *) ((bits) + offset1 + \
		((stride) >> 1) * ((line) >> 1)))

#define YV12_V(line)	      \
    ((uint8_t *) ((bits) + offset0 + \
		((stride) >> 1) * ((line) >> 1)))

/*********************************** Fetch ************************************/

static FASTCALL void
fbFetch_a8r8g8b8 (bits_image_t *pict, int x, int y, int width, uint32_t *buffer)
{
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    MEMCPY_WRAPPED(pict,
                   buffer, (const uint32_t *)bits + x,
		   width*sizeof(uint32_t));
}

static FASTCALL void
fbFetch_x8r8g8b8 (bits_image_t *pict, int x, int y, int width, uint32_t *buffer)
{
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    const uint32_t *pixel = (const uint32_t *)bits + x;
    const uint32_t *end = pixel + width;
    while (pixel < end) {
	*buffer++ = READ(pict, pixel++) | 0xff000000;
    }
}

static FASTCALL void
fbFetch_a8b8g8r8 (bits_image_t *pict, int x, int y, int width, uint32_t *buffer)
{
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    const uint32_t *pixel = (uint32_t *)bits + x;
    const uint32_t *end = pixel + width;
    while (pixel < end) {
	uint32_t p = READ(pict, pixel++);
	*buffer++ = (p & 0xff00ff00) |
	            ((p >> 16) & 0xff) |
	    ((p & 0xff) << 16);
    }
}

static FASTCALL void
fbFetch_x8b8g8r8 (bits_image_t *pict, int x, int y, int width, uint32_t *buffer)
{
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    const uint32_t *pixel = (uint32_t *)bits + x;
    const uint32_t *end = pixel + width;
    while (pixel < end) {
	uint32_t p = READ(pict, pixel++);
	*buffer++ = 0xff000000 |
	    (p & 0x0000ff00) |
	    ((p >> 16) & 0xff) |
	    ((p & 0xff) << 16);
    }
}

static FASTCALL void
fbFetch_a2b10g10r10 (bits_image_t *pict, int x, int y, int width, uint64_t *buffer)
{
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    const uint32_t *pixel = bits + x;
    const uint32_t *end = pixel + width;
    while (pixel < end) {
        uint32_t p = READ(pict, pixel++);
        uint64_t a = p >> 30;
        uint64_t b = (p >> 20) & 0x3ff;
        uint64_t g = (p >> 10) & 0x3ff;
        uint64_t r = p & 0x3ff;

        r = r << 6 | r >> 4;
        g = g << 6 | g >> 4;
        b = b << 6 | b >> 4;

        a <<= 62;
        a |= a >> 2;
        a |= a >> 4;
        a |= a >> 8;

        *buffer++ = a << 48 | r << 32 | g << 16 | b;
    }
}

static FASTCALL void
fbFetch_x2b10g10r10 (bits_image_t *pict, int x, int y, int width, uint64_t *buffer)
{
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    const uint32_t *pixel = (uint32_t *)bits + x;
    const uint32_t *end = pixel + width;
    while (pixel < end) {
        uint32_t p = READ(pict, pixel++);
        uint64_t b = (p >> 20) & 0x3ff;
        uint64_t g = (p >> 10) & 0x3ff;
        uint64_t r = p & 0x3ff;

        r = r << 6 | r >> 4;
        g = g << 6 | g >> 4;
        b = b << 6 | b >> 4;

        *buffer++ = 0xffffULL << 48 | r << 32 | g << 16 | b;
    }
}

static FASTCALL void
fbFetch_r8g8b8 (bits_image_t *pict, int x, int y, int width, uint32_t *buffer)
{
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    const uint8_t *pixel = (const uint8_t *)bits + 3*x;
    const uint8_t *end = pixel + 3*width;
    while (pixel < end) {
	uint32_t b = Fetch24(pict, pixel) | 0xff000000;
	pixel += 3;
	*buffer++ = b;
    }
}

static FASTCALL void
fbFetch_b8g8r8 (bits_image_t *pict, int x, int y, int width, uint32_t *buffer)
{
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    const uint8_t *pixel = (const uint8_t *)bits + 3*x;
    const uint8_t *end = pixel + 3*width;
    while (pixel < end) {
	uint32_t b = 0xff000000;
#if IMAGE_BYTE_ORDER == MSBFirst
	b |= (READ(pict, pixel++));
	b |= (READ(pict, pixel++) << 8);
	b |= (READ(pict, pixel++) << 16);
#else
	b |= (READ(pict, pixel++) << 16);
	b |= (READ(pict, pixel++) << 8);
	b |= (READ(pict, pixel++));
#endif
	*buffer++ = b;
    }
}

static FASTCALL void
fbFetch_r5g6b5 (bits_image_t *pict, int x, int y, int width, uint32_t *buffer)
{
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    const uint16_t *pixel = (const uint16_t *)bits + x;
    const uint16_t *end = pixel + width;
    while (pixel < end) {
	uint32_t p = READ(pict, pixel++);
	uint32_t r = (((p) << 3) & 0xf8) |
	    (((p) << 5) & 0xfc00) |
	    (((p) << 8) & 0xf80000);
	r |= (r >> 5) & 0x70007;
	r |= (r >> 6) & 0x300;
	*buffer++ = 0xff000000 | r;
    }
}

static FASTCALL void
fbFetch_b5g6r5 (bits_image_t *pict, int x, int y, int width, uint32_t *buffer)
{
    uint32_t  r,g,b;
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    const uint16_t *pixel = (const uint16_t *)bits + x;
    const uint16_t *end = pixel + width;
    while (pixel < end) {
	uint32_t  p = READ(pict, pixel++);
	b = ((p & 0xf800) | ((p & 0xe000) >> 5)) >> 8;
	g = ((p & 0x07e0) | ((p & 0x0600) >> 6)) << 5;
	r = ((p & 0x001c) | ((p & 0x001f) << 5)) << 14;
	*buffer++ = 0xff000000 | r | g | b;
    }
}

static FASTCALL void
fbFetch_a1r5g5b5 (bits_image_t *pict, int x, int y, int width, uint32_t *buffer)
{
    uint32_t  r,g,b, a;
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    const uint16_t *pixel = (const uint16_t *)bits + x;
    const uint16_t *end = pixel + width;
    while (pixel < end) {
	uint32_t  p = READ(pict, pixel++);

	a = (uint32_t) ((uint8_t) (0 - ((p & 0x8000) >> 15))) << 24;
	r = ((p & 0x7c00) | ((p & 0x7000) >> 5)) << 9;
	g = ((p & 0x03e0) | ((p & 0x0380) >> 5)) << 6;
	b = ((p & 0x001c) | ((p & 0x001f) << 5)) >> 2;
	*buffer++ = a | r | g | b;
    }
}

static FASTCALL void
fbFetch_x1r5g5b5 (bits_image_t *pict, int x, int y, int width, uint32_t *buffer)
{
    uint32_t  r,g,b;
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    const uint16_t *pixel = (const uint16_t *)bits + x;
    const uint16_t *end = pixel + width;
    while (pixel < end) {
	uint32_t  p = READ(pict, pixel++);

	r = ((p & 0x7c00) | ((p & 0x7000) >> 5)) << 9;
	g = ((p & 0x03e0) | ((p & 0x0380) >> 5)) << 6;
	b = ((p & 0x001c) | ((p & 0x001f) << 5)) >> 2;
	*buffer++ = 0xff000000 | r | g | b;
    }
}

static FASTCALL void
fbFetch_a1b5g5r5 (bits_image_t *pict, int x, int y, int width, uint32_t *buffer)
{
    uint32_t  r,g,b, a;
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    const uint16_t *pixel = (const uint16_t *)bits + x;
    const uint16_t *end = pixel + width;
    while (pixel < end) {
	uint32_t  p = READ(pict, pixel++);

	a = (uint32_t) ((uint8_t) (0 - ((p & 0x8000) >> 15))) << 24;
	b = ((p & 0x7c00) | ((p & 0x7000) >> 5)) >> 7;
	g = ((p & 0x03e0) | ((p & 0x0380) >> 5)) << 6;
	r = ((p & 0x001c) | ((p & 0x001f) << 5)) << 14;
	*buffer++ = a | r | g | b;
    }
}

static FASTCALL void
fbFetch_x1b5g5r5 (bits_image_t *pict, int x, int y, int width, uint32_t *buffer)
{
    uint32_t  r,g,b;
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    const uint16_t *pixel = (const uint16_t *)bits + x;
    const uint16_t *end = pixel + width;
    while (pixel < end) {
	uint32_t  p = READ(pict, pixel++);

	b = ((p & 0x7c00) | ((p & 0x7000) >> 5)) >> 7;
	g = ((p & 0x03e0) | ((p & 0x0380) >> 5)) << 6;
	r = ((p & 0x001c) | ((p & 0x001f) << 5)) << 14;
	*buffer++ = 0xff000000 | r | g | b;
    }
}

static FASTCALL void
fbFetch_a4r4g4b4 (bits_image_t *pict, int x, int y, int width, uint32_t *buffer)
{
    uint32_t  r,g,b, a;
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    const uint16_t *pixel = (const uint16_t *)bits + x;
    const uint16_t *end = pixel + width;
    while (pixel < end) {
	uint32_t  p = READ(pict, pixel++);

	a = ((p & 0xf000) | ((p & 0xf000) >> 4)) << 16;
	r = ((p & 0x0f00) | ((p & 0x0f00) >> 4)) << 12;
	g = ((p & 0x00f0) | ((p & 0x00f0) >> 4)) << 8;
	b = ((p & 0x000f) | ((p & 0x000f) << 4));
	*buffer++ = a | r | g | b;
    }
}

static FASTCALL void
fbFetch_x4r4g4b4 (bits_image_t *pict, int x, int y, int width, uint32_t *buffer)
{
    uint32_t  r,g,b;
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    const uint16_t *pixel = (const uint16_t *)bits + x;
    const uint16_t *end = pixel + width;
    while (pixel < end) {
	uint32_t  p = READ(pict, pixel++);

	r = ((p & 0x0f00) | ((p & 0x0f00) >> 4)) << 12;
	g = ((p & 0x00f0) | ((p & 0x00f0) >> 4)) << 8;
	b = ((p & 0x000f) | ((p & 0x000f) << 4));
	*buffer++ = 0xff000000 | r | g | b;
    }
}

static FASTCALL void
fbFetch_a4b4g4r4 (bits_image_t *pict, int x, int y, int width, uint32_t *buffer)
{
    uint32_t  r,g,b, a;
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    const uint16_t *pixel = (const uint16_t *)bits + x;
    const uint16_t *end = pixel + width;
    while (pixel < end) {
	uint32_t  p = READ(pict, pixel++);

	a = ((p & 0xf000) | ((p & 0xf000) >> 4)) << 16;
	b = ((p & 0x0f00) | ((p & 0x0f00) >> 4)) >> 4;
	g = ((p & 0x00f0) | ((p & 0x00f0) >> 4)) << 8;
	r = ((p & 0x000f) | ((p & 0x000f) << 4)) << 16;
	*buffer++ = a | r | g | b;
    }
}

static FASTCALL void
fbFetch_x4b4g4r4 (bits_image_t *pict, int x, int y, int width, uint32_t *buffer)
{
    uint32_t  r,g,b;
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    const uint16_t *pixel = (const uint16_t *)bits + x;
    const uint16_t *end = pixel + width;
    while (pixel < end) {
	uint32_t  p = READ(pict, pixel++);

	b = ((p & 0x0f00) | ((p & 0x0f00) >> 4)) >> 4;
	g = ((p & 0x00f0) | ((p & 0x00f0) >> 4)) << 8;
	r = ((p & 0x000f) | ((p & 0x000f) << 4)) << 16;
	*buffer++ = 0xff000000 | r | g | b;
    }
}

static FASTCALL void
fbFetch_a8 (bits_image_t *pict, int x, int y, int width, uint32_t *buffer)
{
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    const uint8_t *pixel = (const uint8_t *)bits + x;
    const uint8_t *end = pixel + width;
    while (pixel < end) {
	*buffer++ = READ(pict, pixel++) << 24;
    }
}

static FASTCALL void
fbFetch_r3g3b2 (bits_image_t *pict, int x, int y, int width, uint32_t *buffer)
{
    uint32_t  r,g,b;
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    const uint8_t *pixel = (const uint8_t *)bits + x;
    const uint8_t *end = pixel + width;
    while (pixel < end) {
	uint32_t  p = READ(pict, pixel++);

	r = ((p & 0xe0) | ((p & 0xe0) >> 3) | ((p & 0xc0) >> 6)) << 16;
	g = ((p & 0x1c) | ((p & 0x18) >> 3) | ((p & 0x1c) << 3)) << 8;
	b = (((p & 0x03)     ) |
	     ((p & 0x03) << 2) |
	     ((p & 0x03) << 4) |
	     ((p & 0x03) << 6));
	*buffer++ = 0xff000000 | r | g | b;
    }
}

static FASTCALL void
fbFetch_b2g3r3 (bits_image_t *pict, int x, int y, int width, uint32_t *buffer)
{
    uint32_t  r,g,b;
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    const uint8_t *pixel = (const uint8_t *)bits + x;
    const uint8_t *end = pixel + width;
    while (pixel < end) {
	uint32_t  p = READ(pict, pixel++);

	b = (((p & 0xc0)     ) |
	     ((p & 0xc0) >> 2) |
	     ((p & 0xc0) >> 4) |
	     ((p & 0xc0) >> 6));
	g = ((p & 0x38) | ((p & 0x38) >> 3) | ((p & 0x30) << 2)) << 8;
	r = (((p & 0x07)     ) |
	     ((p & 0x07) << 3) |
	     ((p & 0x06) << 6)) << 16;
	*buffer++ = 0xff000000 | r | g | b;
    }
}

static FASTCALL void
fbFetch_a2r2g2b2 (bits_image_t *pict, int x, int y, int width, uint32_t *buffer)
{
    uint32_t   a,r,g,b;
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    const uint8_t *pixel = (const uint8_t *)bits + x;
    const uint8_t *end = pixel + width;
    while (pixel < end) {
	uint32_t  p = READ(pict, pixel++);

	a = ((p & 0xc0) * 0x55) << 18;
	r = ((p & 0x30) * 0x55) << 12;
	g = ((p & 0x0c) * 0x55) << 6;
	b = ((p & 0x03) * 0x55);
	*buffer++ = a|r|g|b;
    }
}

static FASTCALL void
fbFetch_a2b2g2r2 (bits_image_t *pict, int x, int y, int width, uint32_t *buffer)
{
    uint32_t   a,r,g,b;
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    const uint8_t *pixel = (const uint8_t *)bits + x;
    const uint8_t *end = pixel + width;
    while (pixel < end) {
	uint32_t  p = READ(pict, pixel++);

	a = ((p & 0xc0) * 0x55) << 18;
	b = ((p & 0x30) * 0x55) >> 6;
	g = ((p & 0x0c) * 0x55) << 6;
	r = ((p & 0x03) * 0x55) << 16;
	*buffer++ = a|r|g|b;
    }
}

static FASTCALL void
fbFetch_c8 (bits_image_t *pict, int x, int y, int width, uint32_t *buffer)
{
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    const pixman_indexed_t * indexed = pict->indexed;
    const uint8_t *pixel = (const uint8_t *)bits + x;
    const uint8_t *end = pixel + width;
    while (pixel < end) {
	uint32_t  p = READ(pict, pixel++);
	*buffer++ = indexed->rgba[p];
    }
}

static FASTCALL void
fbFetch_x4a4 (bits_image_t *pict, int x, int y, int width, uint32_t *buffer)
{
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    const uint8_t *pixel = (const uint8_t *)bits + x;
    const uint8_t *end = pixel + width;
    while (pixel < end) {
	uint8_t p = READ(pict, pixel++) & 0xf;
	*buffer++ = (p | (p << 4)) << 24;
    }
}

#define Fetch8(img,l,o)    (READ(img, (uint8_t *)(l) + ((o) >> 2)))
#if IMAGE_BYTE_ORDER == MSBFirst
#define Fetch4(img,l,o)    ((o) & 2 ? Fetch8(img,l,o) & 0xf : Fetch8(img,l,o) >> 4)
#else
#define Fetch4(img,l,o)    ((o) & 2 ? Fetch8(img,l,o) >> 4 : Fetch8(img,l,o) & 0xf)
#endif

static FASTCALL void
fbFetch_a4 (bits_image_t *pict, int x, int y, int width, uint32_t *buffer)
{
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    int i;
    for (i = 0; i < width; ++i) {
	uint32_t  p = Fetch4(pict, bits, i + x);

	p |= p << 4;
	*buffer++ = p << 24;
    }
}

static FASTCALL void
fbFetch_r1g2b1 (bits_image_t *pict, int x, int y, int width, uint32_t *buffer)
{
    uint32_t  r,g,b;
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    int i;
    for (i = 0; i < width; ++i) {
	uint32_t  p = Fetch4(pict, bits, i + x);

	r = ((p & 0x8) * 0xff) << 13;
	g = ((p & 0x6) * 0x55) << 7;
	b = ((p & 0x1) * 0xff);
	*buffer++ = 0xff000000|r|g|b;
    }
}

static FASTCALL void
fbFetch_b1g2r1 (bits_image_t *pict, int x, int y, int width, uint32_t *buffer)
{
    uint32_t  r,g,b;
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    int i;
    for (i = 0; i < width; ++i) {
	uint32_t  p = Fetch4(pict, bits, i + x);

	b = ((p & 0x8) * 0xff) >> 3;
	g = ((p & 0x6) * 0x55) << 7;
	r = ((p & 0x1) * 0xff) << 16;
	*buffer++ = 0xff000000|r|g|b;
    }
}

static FASTCALL void
fbFetch_a1r1g1b1 (bits_image_t *pict, int x, int y, int width, uint32_t *buffer)
{
    uint32_t  a,r,g,b;
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    int i;
    for (i = 0; i < width; ++i) {
	uint32_t  p = Fetch4(pict, bits, i + x);

	a = ((p & 0x8) * 0xff) << 21;
	r = ((p & 0x4) * 0xff) << 14;
	g = ((p & 0x2) * 0xff) << 7;
	b = ((p & 0x1) * 0xff);
	*buffer++ = a|r|g|b;
    }
}

static FASTCALL void
fbFetch_a1b1g1r1 (bits_image_t *pict, int x, int y, int width, uint32_t *buffer)
{
    uint32_t  a,r,g,b;
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    int i;
    for (i = 0; i < width; ++i) {
	uint32_t  p = Fetch4(pict, bits, i + x);

	a = ((p & 0x8) * 0xff) << 21;
	r = ((p & 0x4) * 0xff) >> 3;
	g = ((p & 0x2) * 0xff) << 7;
	b = ((p & 0x1) * 0xff) << 16;
	*buffer++ = a|r|g|b;
    }
}

static FASTCALL void
fbFetch_c4 (bits_image_t *pict, int x, int y, int width, uint32_t *buffer)
{
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    const pixman_indexed_t * indexed = pict->indexed;
    int i;
    for (i = 0; i < width; ++i) {
	uint32_t  p = Fetch4(pict, bits, i + x);

	*buffer++ = indexed->rgba[p];
    }
}


static FASTCALL void
fbFetch_a1 (bits_image_t *pict, int x, int y, int width, uint32_t *buffer)
{
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    int i;
    for (i = 0; i < width; ++i) {
	uint32_t  p = READ(pict, bits + ((i + x) >> 5));
	uint32_t  a;
#if BITMAP_BIT_ORDER == MSBFirst
	a = p >> (0x1f - ((i+x) & 0x1f));
#else
	a = p >> ((i+x) & 0x1f);
#endif
	a = a & 1;
	a |= a << 1;
	a |= a << 2;
	a |= a << 4;
	*buffer++ = a << 24;
    }
}

static FASTCALL void
fbFetch_g1 (bits_image_t *pict, int x, int y, int width, uint32_t *buffer)
{
    const uint32_t *bits = pict->bits + y*pict->rowstride;
    const pixman_indexed_t * indexed = pict->indexed;
    int i;
    for (i = 0; i < width; ++i) {
	uint32_t p = READ(pict, bits + ((i+x) >> 5));
	uint32_t a;
#if BITMAP_BIT_ORDER == MSBFirst
	a = p >> (0x1f - ((i+x) & 0x1f));
#else
	a = p >> ((i+x) & 0x1f);
#endif
	a = a & 1;
	*buffer++ = indexed->rgba[a];
    }
}

static FASTCALL void
fbFetch_yuy2 (bits_image_t *pict, int x, int line, int width, uint32_t *buffer)
{
    int16_t y, u, v;
    int32_t r, g, b;
    int   i;

    const uint32_t *bits = pict->bits + pict->rowstride * line;

    for (i = 0; i < width; i++)
    {
	y = ((uint8_t *) bits)[(x + i) << 1] - 16;
	u = ((uint8_t *) bits)[(((x + i) << 1) & -4) + 1] - 128;
	v = ((uint8_t *) bits)[(((x + i) << 1) & -4) + 3] - 128;

	/* R = 1.164(Y - 16) + 1.596(V - 128) */
	r = 0x012b27 * y + 0x019a2e * v;
	/* G = 1.164(Y - 16) - 0.813(V - 128) - 0.391(U - 128) */
	g = 0x012b27 * y - 0x00d0f2 * v - 0x00647e * u;
	/* B = 1.164(Y - 16) + 2.018(U - 128) */
	b = 0x012b27 * y + 0x0206a2 * u;

    WRITE(pict, buffer++, 0xff000000 |
	(r >= 0 ? r < 0x1000000 ? r         & 0xff0000 : 0xff0000 : 0) |
	(g >= 0 ? g < 0x1000000 ? (g >> 8)  & 0x00ff00 : 0x00ff00 : 0) |
	(b >= 0 ? b < 0x1000000 ? (b >> 16) & 0x0000ff : 0x0000ff : 0));
    }
}

static FASTCALL void
fbFetch_yv12 (bits_image_t *pict, int x, int line, int width, uint32_t *buffer)
{
    YV12_SETUP(pict);
    uint8_t *pY = YV12_Y (line);
    uint8_t *pU = YV12_U (line);
    uint8_t *pV = YV12_V (line);
    int16_t y, u, v;
    int32_t r, g, b;
    int   i;

    for (i = 0; i < width; i++)
    {
	y = pY[x + i] - 16;
	u = pU[(x + i) >> 1] - 128;
	v = pV[(x + i) >> 1] - 128;

	/* R = 1.164(Y - 16) + 1.596(V - 128) */
	r = 0x012b27 * y + 0x019a2e * v;
	/* G = 1.164(Y - 16) - 0.813(V - 128) - 0.391(U - 128) */
	g = 0x012b27 * y - 0x00d0f2 * v - 0x00647e * u;
	/* B = 1.164(Y - 16) + 2.018(U - 128) */
	b = 0x012b27 * y + 0x0206a2 * u;

	WRITE(pict, buffer++, 0xff000000 |
	    (r >= 0 ? r < 0x1000000 ? r         & 0xff0000 : 0xff0000 : 0) |
	    (g >= 0 ? g < 0x1000000 ? (g >> 8)  & 0x00ff00 : 0x00ff00 : 0) |
	    (b >= 0 ? b < 0x1000000 ? (b >> 16) & 0x0000ff : 0x0000ff : 0));
    }
}

fetchProc32 ACCESS(pixman_fetchProcForPicture32) (bits_image_t * pict)
{
    switch(pict->format) {
    case PIXMAN_a8r8g8b8: return fbFetch_a8r8g8b8;
    case PIXMAN_x8r8g8b8: return fbFetch_x8r8g8b8;
    case PIXMAN_a8b8g8r8: return fbFetch_a8b8g8r8;
    case PIXMAN_x8b8g8r8: return fbFetch_x8b8g8r8;
    /* These two require wide compositing */
    case PIXMAN_a2b10g10r10: return NULL;
    case PIXMAN_x2b10g10r10: return NULL;

        /* 24bpp formats */
    case PIXMAN_r8g8b8: return fbFetch_r8g8b8;
    case PIXMAN_b8g8r8: return fbFetch_b8g8r8;

        /* 16bpp formats */
    case PIXMAN_r5g6b5: return fbFetch_r5g6b5;
    case PIXMAN_b5g6r5: return fbFetch_b5g6r5;

    case PIXMAN_a1r5g5b5: return fbFetch_a1r5g5b5;
    case PIXMAN_x1r5g5b5: return fbFetch_x1r5g5b5;
    case PIXMAN_a1b5g5r5: return fbFetch_a1b5g5r5;
    case PIXMAN_x1b5g5r5: return fbFetch_x1b5g5r5;
    case PIXMAN_a4r4g4b4: return fbFetch_a4r4g4b4;
    case PIXMAN_x4r4g4b4: return fbFetch_x4r4g4b4;
    case PIXMAN_a4b4g4r4: return fbFetch_a4b4g4r4;
    case PIXMAN_x4b4g4r4: return fbFetch_x4b4g4r4;

        /* 8bpp formats */
    case PIXMAN_a8: return  fbFetch_a8;
    case PIXMAN_r3g3b2: return fbFetch_r3g3b2;
    case PIXMAN_b2g3r3: return fbFetch_b2g3r3;
    case PIXMAN_a2r2g2b2: return fbFetch_a2r2g2b2;
    case PIXMAN_a2b2g2r2: return fbFetch_a2b2g2r2;
    case PIXMAN_c8: return  fbFetch_c8;
    case PIXMAN_g8: return  fbFetch_c8;
    case PIXMAN_x4a4: return fbFetch_x4a4;

        /* 4bpp formats */
    case PIXMAN_a4: return  fbFetch_a4;
    case PIXMAN_r1g2b1: return fbFetch_r1g2b1;
    case PIXMAN_b1g2r1: return fbFetch_b1g2r1;
    case PIXMAN_a1r1g1b1: return fbFetch_a1r1g1b1;
    case PIXMAN_a1b1g1r1: return fbFetch_a1b1g1r1;
    case PIXMAN_c4: return  fbFetch_c4;
    case PIXMAN_g4: return  fbFetch_c4;

        /* 1bpp formats */
    case PIXMAN_a1: return  fbFetch_a1;
    case PIXMAN_g1: return  fbFetch_g1;

        /* YUV formats */
    case PIXMAN_yuy2: return fbFetch_yuy2;
    case PIXMAN_yv12: return fbFetch_yv12;
    }

    return NULL;
}

static FASTCALL void
fbFetch64_generic (bits_image_t *pict, int x, int y, int width, uint64_t *buffer)
{
    fetchProc32 fetch32 = ACCESS(pixman_fetchProcForPicture32) (pict);

    // Fetch the pixels into the first half of buffer and then expand them in
    // place.
    fetch32(pict, x, y, width, (uint32_t*)buffer);
    pixman_expand(buffer, (uint32_t*)buffer, pict->format, width);
}

fetchProc64 ACCESS(pixman_fetchProcForPicture64) (bits_image_t * pict)
{
    switch(pict->format) {
    case PIXMAN_a2b10g10r10: return fbFetch_a2b10g10r10;
    case PIXMAN_x2b10g10r10: return fbFetch_x2b10g10r10;
    default: return fbFetch64_generic;
    }
}

/**************************** Pixel wise fetching *****************************/

static FASTCALL uint64_t
fbFetchPixel_a2b10g10r10 (bits_image_t *pict, int offset, int line)
{
    uint32_t *bits = pict->bits + line*pict->rowstride;
    uint32_t p = READ(pict, bits + offset);
    uint64_t a = p >> 30;
    uint64_t b = (p >> 20) & 0x3ff;
    uint64_t g = (p >> 10) & 0x3ff;
    uint64_t r = p & 0x3ff;

    r = r << 6 | r >> 4;
    g = g << 6 | g >> 4;
    b = b << 6 | b >> 4;

    a <<= 62;
    a |= a >> 2;
    a |= a >> 4;
    a |= a >> 8;

    return a << 48 | r << 32 | g << 16 | b;
}

static FASTCALL uint64_t
fbFetchPixel_x2b10g10r10 (bits_image_t *pict, int offset, int line)
{
    uint32_t *bits = pict->bits + line*pict->rowstride;
    uint32_t p = READ(pict, bits + offset);
    uint64_t b = (p >> 20) & 0x3ff;
    uint64_t g = (p >> 10) & 0x3ff;
    uint64_t r = p & 0x3ff;

    r = r << 6 | r >> 4;
    g = g << 6 | g >> 4;
    b = b << 6 | b >> 4;

    return 0xffffULL << 48 | r << 32 | g << 16 | b;
}

static FASTCALL uint32_t
fbFetchPixel_a8r8g8b8 (bits_image_t *pict, int offset, int line)
{
    uint32_t *bits = pict->bits + line*pict->rowstride;
    return READ(pict, (uint32_t *)bits + offset);
}

static FASTCALL uint32_t
fbFetchPixel_x8r8g8b8 (bits_image_t *pict, int offset, int line)
{
    uint32_t *bits = pict->bits + line*pict->rowstride;
    return READ(pict, (uint32_t *)bits + offset) | 0xff000000;
}

static FASTCALL uint32_t
fbFetchPixel_a8b8g8r8 (bits_image_t *pict, int offset, int line)
{
    uint32_t *bits = pict->bits + line*pict->rowstride;
    uint32_t  pixel = READ(pict, (uint32_t *)bits + offset);

    return ((pixel & 0xff000000) |
	    ((pixel >> 16) & 0xff) |
	    (pixel & 0x0000ff00) |
	    ((pixel & 0xff) << 16));
}

static FASTCALL uint32_t
fbFetchPixel_x8b8g8r8 (bits_image_t *pict, int offset, int line)
{
    uint32_t *bits = pict->bits + line*pict->rowstride;
    uint32_t  pixel = READ(pict, (uint32_t *)bits + offset);

    return ((0xff000000) |
	    ((pixel >> 16) & 0xff) |
	    (pixel & 0x0000ff00) |
	    ((pixel & 0xff) << 16));
}

static FASTCALL uint32_t
fbFetchPixel_r8g8b8 (bits_image_t *pict, int offset, int line)
{
    uint32_t *bits = pict->bits + line*pict->rowstride;
    uint8_t   *pixel = ((uint8_t *) bits) + (offset*3);
#if IMAGE_BYTE_ORDER == MSBFirst
    return (0xff000000 |
	    (READ(pict, pixel + 0) << 16) |
	    (READ(pict, pixel + 1) << 8) |
	    (READ(pict, pixel + 2)));
#else
    return (0xff000000 |
	    (READ(pict, pixel + 2) << 16) |
	    (READ(pict, pixel + 1) << 8) |
	    (READ(pict, pixel + 0)));
#endif
}

static FASTCALL uint32_t
fbFetchPixel_b8g8r8 (bits_image_t *pict, int offset, int line)
{
    uint32_t *bits = pict->bits + line*pict->rowstride;
    uint8_t   *pixel = ((uint8_t *) bits) + (offset*3);
#if IMAGE_BYTE_ORDER == MSBFirst
    return (0xff000000 |
	    (READ(pict, pixel + 2) << 16) |
	    (READ(pict, pixel + 1) << 8) |
	    (READ(pict, pixel + 0)));
#else
    return (0xff000000 |
	    (READ(pict, pixel + 0) << 16) |
	    (READ(pict, pixel + 1) << 8) |
	    (READ(pict, pixel + 2)));
#endif
}

static FASTCALL uint32_t
fbFetchPixel_r5g6b5 (bits_image_t *pict, int offset, int line)
{
    uint32_t  r,g,b;
    uint32_t *bits = pict->bits + line*pict->rowstride;
    uint32_t  pixel = READ(pict, (uint16_t *) bits + offset);

    r = ((pixel & 0xf800) | ((pixel & 0xe000) >> 5)) << 8;
    g = ((pixel & 0x07e0) | ((pixel & 0x0600) >> 6)) << 5;
    b = ((pixel & 0x001c) | ((pixel & 0x001f) << 5)) >> 2;
    return (0xff000000 | r | g | b);
}

static FASTCALL uint32_t
fbFetchPixel_b5g6r5 (bits_image_t *pict, int offset, int line)
{
    uint32_t  r,g,b;
    uint32_t *bits = pict->bits + line*pict->rowstride;
    uint32_t  pixel = READ(pict, (uint16_t *) bits + offset);

    b = ((pixel & 0xf800) | ((pixel & 0xe000) >> 5)) >> 8;
    g = ((pixel & 0x07e0) | ((pixel & 0x0600) >> 6)) << 5;
    r = ((pixel & 0x001c) | ((pixel & 0x001f) << 5)) << 14;
    return (0xff000000 | r | g | b);
}

static FASTCALL uint32_t
fbFetchPixel_a1r5g5b5 (bits_image_t *pict, int offset, int line)
{
    uint32_t  a,r,g,b;
    uint32_t *bits = pict->bits + line*pict->rowstride;
    uint32_t  pixel = READ(pict, (uint16_t *) bits + offset);

    a = (uint32_t) ((uint8_t) (0 - ((pixel & 0x8000) >> 15))) << 24;
    r = ((pixel & 0x7c00) | ((pixel & 0x7000) >> 5)) << 9;
    g = ((pixel & 0x03e0) | ((pixel & 0x0380) >> 5)) << 6;
    b = ((pixel & 0x001c) | ((pixel & 0x001f) << 5)) >> 2;
    return (a | r | g | b);
}

static FASTCALL uint32_t
fbFetchPixel_x1r5g5b5 (bits_image_t *pict, int offset, int line)
{
    uint32_t  r,g,b;
    uint32_t *bits = pict->bits + line*pict->rowstride;
    uint32_t  pixel = READ(pict, (uint16_t *) bits + offset);

    r = ((pixel & 0x7c00) | ((pixel & 0x7000) >> 5)) << 9;
    g = ((pixel & 0x03e0) | ((pixel & 0x0380) >> 5)) << 6;
    b = ((pixel & 0x001c) | ((pixel & 0x001f) << 5)) >> 2;
    return (0xff000000 | r | g | b);
}

static FASTCALL uint32_t
fbFetchPixel_a1b5g5r5 (bits_image_t *pict, int offset, int line)
{
    uint32_t  a,r,g,b;
    uint32_t *bits = pict->bits + line*pict->rowstride;
    uint32_t  pixel = READ(pict, (uint16_t *) bits + offset);

    a = (uint32_t) ((uint8_t) (0 - ((pixel & 0x8000) >> 15))) << 24;
    b = ((pixel & 0x7c00) | ((pixel & 0x7000) >> 5)) >> 7;
    g = ((pixel & 0x03e0) | ((pixel & 0x0380) >> 5)) << 6;
    r = ((pixel & 0x001c) | ((pixel & 0x001f) << 5)) << 14;
    return (a | r | g | b);
}

static FASTCALL uint32_t
fbFetchPixel_x1b5g5r5 (bits_image_t *pict, int offset, int line)
{
    uint32_t  r,g,b;
    uint32_t *bits = pict->bits + line*pict->rowstride;
    uint32_t  pixel = READ(pict, (uint16_t *) bits + offset);

    b = ((pixel & 0x7c00) | ((pixel & 0x7000) >> 5)) >> 7;
    g = ((pixel & 0x03e0) | ((pixel & 0x0380) >> 5)) << 6;
    r = ((pixel & 0x001c) | ((pixel & 0x001f) << 5)) << 14;
    return (0xff000000 | r | g | b);
}

static FASTCALL uint32_t
fbFetchPixel_a4r4g4b4 (bits_image_t *pict, int offset, int line)
{
    uint32_t  a,r,g,b;
    uint32_t *bits = pict->bits + line*pict->rowstride;
    uint32_t  pixel = READ(pict, (uint16_t *) bits + offset);

    a = ((pixel & 0xf000) | ((pixel & 0xf000) >> 4)) << 16;
    r = ((pixel & 0x0f00) | ((pixel & 0x0f00) >> 4)) << 12;
    g = ((pixel & 0x00f0) | ((pixel & 0x00f0) >> 4)) << 8;
    b = ((pixel & 0x000f) | ((pixel & 0x000f) << 4));
    return (a | r | g | b);
}

static FASTCALL uint32_t
fbFetchPixel_x4r4g4b4 (bits_image_t *pict, int offset, int line)
{
    uint32_t  r,g,b;
    uint32_t *bits = pict->bits + line*pict->rowstride;
    uint32_t  pixel = READ(pict, (uint16_t *) bits + offset);

    r = ((pixel & 0x0f00) | ((pixel & 0x0f00) >> 4)) << 12;
    g = ((pixel & 0x00f0) | ((pixel & 0x00f0) >> 4)) << 8;
    b = ((pixel & 0x000f) | ((pixel & 0x000f) << 4));
    return (0xff000000 | r | g | b);
}

static FASTCALL uint32_t
fbFetchPixel_a4b4g4r4 (bits_image_t *pict, int offset, int line)
{
    uint32_t  a,r,g,b;
    uint32_t *bits = pict->bits + line*pict->rowstride;
    uint32_t  pixel = READ(pict, (uint16_t *) bits + offset);

    a = ((pixel & 0xf000) | ((pixel & 0xf000) >> 4)) << 16;
    b = ((pixel & 0x0f00) | ((pixel & 0x0f00) >> 4)) >> 4;
    g = ((pixel & 0x00f0) | ((pixel & 0x00f0) >> 4)) << 8;
    r = ((pixel & 0x000f) | ((pixel & 0x000f) << 4)) << 16;
    return (a | r | g | b);
}

static FASTCALL uint32_t
fbFetchPixel_x4b4g4r4 (bits_image_t *pict, int offset, int line)
{
    uint32_t  r,g,b;
    uint32_t *bits = pict->bits + line*pict->rowstride;
    uint32_t  pixel = READ(pict, (uint16_t *) bits + offset);

    b = ((pixel & 0x0f00) | ((pixel & 0x0f00) >> 4)) >> 4;
    g = ((pixel & 0x00f0) | ((pixel & 0x00f0) >> 4)) << 8;
    r = ((pixel & 0x000f) | ((pixel & 0x000f) << 4)) << 16;
    return (0xff000000 | r | g | b);
}

static FASTCALL uint32_t
fbFetchPixel_a8 (bits_image_t *pict, int offset, int line)
{
    uint32_t *bits = pict->bits + line*pict->rowstride;
    uint32_t   pixel = READ(pict, (uint8_t *) bits + offset);

    return pixel << 24;
}

static FASTCALL uint32_t
fbFetchPixel_r3g3b2 (bits_image_t *pict, int offset, int line)
{
    uint32_t  r,g,b;
    uint32_t *bits = pict->bits + line*pict->rowstride;
    uint32_t   pixel = READ(pict, (uint8_t *) bits + offset);

    r = ((pixel & 0xe0) | ((pixel & 0xe0) >> 3) | ((pixel & 0xc0) >> 6)) << 16;
    g = ((pixel & 0x1c) | ((pixel & 0x18) >> 3) | ((pixel & 0x1c) << 3)) << 8;
    b = (((pixel & 0x03)     ) |
	 ((pixel & 0x03) << 2) |
	 ((pixel & 0x03) << 4) |
	 ((pixel & 0x03) << 6));
    return (0xff000000 | r | g | b);
}

static FASTCALL uint32_t
fbFetchPixel_b2g3r3 (bits_image_t *pict, int offset, int line)
{
    uint32_t  r,g,b;
    uint32_t *bits = pict->bits + line*pict->rowstride;
    uint32_t   pixel = READ(pict, (uint8_t *) bits + offset);

    b = (((pixel & 0xc0)     ) |
	 ((pixel & 0xc0) >> 2) |
	 ((pixel & 0xc0) >> 4) |
	 ((pixel & 0xc0) >> 6));
    g = ((pixel & 0x38) | ((pixel & 0x38) >> 3) | ((pixel & 0x30) << 2)) << 8;
    r = (((pixel & 0x07)     ) |
	 ((pixel & 0x07) << 3) |
	 ((pixel & 0x06) << 6)) << 16;
    return (0xff000000 | r | g | b);
}

static FASTCALL uint32_t
fbFetchPixel_a2r2g2b2 (bits_image_t *pict, int offset, int line)
{
    uint32_t   a,r,g,b;
    uint32_t *bits = pict->bits + line*pict->rowstride;
    uint32_t   pixel = READ(pict, (uint8_t *) bits + offset);

    a = ((pixel & 0xc0) * 0x55) << 18;
    r = ((pixel & 0x30) * 0x55) << 12;
    g = ((pixel & 0x0c) * 0x55) << 6;
    b = ((pixel & 0x03) * 0x55);
    return a|r|g|b;
}

static FASTCALL uint32_t
fbFetchPixel_a2b2g2r2 (bits_image_t *pict, int offset, int line)
{
    uint32_t   a,r,g,b;
    uint32_t *bits = pict->bits + line*pict->rowstride;
    uint32_t   pixel = READ(pict, (uint8_t *) bits + offset);

    a = ((pixel & 0xc0) * 0x55) << 18;
    b = ((pixel & 0x30) * 0x55) >> 6;
    g = ((pixel & 0x0c) * 0x55) << 6;
    r = ((pixel & 0x03) * 0x55) << 16;
    return a|r|g|b;
}

static FASTCALL uint32_t
fbFetchPixel_c8 (bits_image_t *pict, int offset, int line)
{
    uint32_t *bits = pict->bits + line*pict->rowstride;
    uint32_t   pixel = READ(pict, (uint8_t *) bits + offset);
    const pixman_indexed_t * indexed = pict->indexed;
    return indexed->rgba[pixel];
}

static FASTCALL uint32_t
fbFetchPixel_x4a4 (bits_image_t *pict, int offset, int line)
{
    uint32_t *bits = pict->bits + line*pict->rowstride;
    uint32_t   pixel = READ(pict, (uint8_t *) bits + offset);

    return ((pixel & 0xf) | ((pixel & 0xf) << 4)) << 24;
}

static FASTCALL uint32_t
fbFetchPixel_a4 (bits_image_t *pict, int offset, int line)
{
    uint32_t *bits = pict->bits + line*pict->rowstride;
    uint32_t  pixel = Fetch4(pict, bits, offset);

    pixel |= pixel << 4;
    return pixel << 24;
}

static FASTCALL uint32_t
fbFetchPixel_r1g2b1 (bits_image_t *pict, int offset, int line)
{
    uint32_t  r,g,b;
    uint32_t *bits = pict->bits + line*pict->rowstride;
    uint32_t  pixel = Fetch4(pict, bits, offset);

    r = ((pixel & 0x8) * 0xff) << 13;
    g = ((pixel & 0x6) * 0x55) << 7;
    b = ((pixel & 0x1) * 0xff);
    return 0xff000000|r|g|b;
}

static FASTCALL uint32_t
fbFetchPixel_b1g2r1 (bits_image_t *pict, int offset, int line)
{
    uint32_t  r,g,b;
    uint32_t *bits = pict->bits + line*pict->rowstride;
    uint32_t  pixel = Fetch4(pict, bits, offset);

    b = ((pixel & 0x8) * 0xff) >> 3;
    g = ((pixel & 0x6) * 0x55) << 7;
    r = ((pixel & 0x1) * 0xff) << 16;
    return 0xff000000|r|g|b;
}

static FASTCALL uint32_t
fbFetchPixel_a1r1g1b1 (bits_image_t *pict, int offset, int line)
{
    uint32_t  a,r,g,b;
    uint32_t *bits = pict->bits + line*pict->rowstride;
    uint32_t  pixel = Fetch4(pict, bits, offset);

    a = ((pixel & 0x8) * 0xff) << 21;
    r = ((pixel & 0x4) * 0xff) << 14;
    g = ((pixel & 0x2) * 0xff) << 7;
    b = ((pixel & 0x1) * 0xff);
    return a|r|g|b;
}

static FASTCALL uint32_t
fbFetchPixel_a1b1g1r1 (bits_image_t *pict, int offset, int line)
{
    uint32_t  a,r,g,b;
    uint32_t *bits = pict->bits + line*pict->rowstride;
    uint32_t  pixel = Fetch4(pict, bits, offset);

    a = ((pixel & 0x8) * 0xff) << 21;
    r = ((pixel & 0x4) * 0xff) >> 3;
    g = ((pixel & 0x2) * 0xff) << 7;
    b = ((pixel & 0x1) * 0xff) << 16;
    return a|r|g|b;
}

static FASTCALL uint32_t
fbFetchPixel_c4 (bits_image_t *pict, int offset, int line)
{
    uint32_t *bits = pict->bits + line*pict->rowstride;
    uint32_t  pixel = Fetch4(pict, bits, offset);
    const pixman_indexed_t * indexed = pict->indexed;

    return indexed->rgba[pixel];
}


static FASTCALL uint32_t
fbFetchPixel_a1 (bits_image_t *pict, int offset, int line)
{
    uint32_t *bits = pict->bits + line*pict->rowstride;
    uint32_t  pixel = READ(pict, bits + (offset >> 5));
    uint32_t  a;
#if BITMAP_BIT_ORDER == MSBFirst
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

static FASTCALL uint32_t
fbFetchPixel_g1 (bits_image_t *pict, int offset, int line)
{
    uint32_t *bits = pict->bits + line*pict->rowstride;
    uint32_t pixel = READ(pict, bits + (offset >> 5));
    const pixman_indexed_t * indexed = pict->indexed;
    uint32_t a;
#if BITMAP_BIT_ORDER == MSBFirst
    a = pixel >> (0x1f - (offset & 0x1f));
#else
    a = pixel >> (offset & 0x1f);
#endif
    a = a & 1;
    return indexed->rgba[a];
}

static FASTCALL uint32_t
fbFetchPixel_yuy2 (bits_image_t *pict, int offset, int line)
{
    int16_t y, u, v;
    int32_t r, g, b;

    const uint32_t *bits = pict->bits + pict->rowstride * line;

    y = ((uint8_t *) bits)[offset << 1] - 16;
    u = ((uint8_t *) bits)[((offset << 1) & -4) + 1] - 128;
    v = ((uint8_t *) bits)[((offset << 1) & -4) + 3] - 128;

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

static FASTCALL uint32_t
fbFetchPixel_yv12 (bits_image_t *pict, int offset, int line)
{
    YV12_SETUP(pict);
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

/*
 * XXX: The transformed fetch path only works at 32-bpp so far.  When all paths
 * have wide versions, this can be removed.
 *
 * WARNING: This function loses precision!
 */
static FASTCALL uint32_t
fbFetchPixel32_generic_lossy (bits_image_t *pict, int offset, int line)
{
    fetchPixelProc64 fetchPixel64 = ACCESS(pixman_fetchPixelProcForPicture64) (pict);
    const uint64_t argb16Pixel = fetchPixel64(pict, offset, line);
    uint32_t argb8Pixel;

    pixman_contract(&argb8Pixel, &argb16Pixel, 1);

    return argb8Pixel;
}

fetchPixelProc32 ACCESS(pixman_fetchPixelProcForPicture32) (bits_image_t * pict)
{
    switch(pict->format) {
    case PIXMAN_a8r8g8b8: return fbFetchPixel_a8r8g8b8;
    case PIXMAN_x8r8g8b8: return fbFetchPixel_x8r8g8b8;
    case PIXMAN_a8b8g8r8: return fbFetchPixel_a8b8g8r8;
    case PIXMAN_x8b8g8r8: return fbFetchPixel_x8b8g8r8;
    /* These two require wide compositing */
    case PIXMAN_a2b10g10r10: return fbFetchPixel32_generic_lossy;
    case PIXMAN_x2b10g10r10: return fbFetchPixel32_generic_lossy;

        /* 24bpp formats */
    case PIXMAN_r8g8b8: return fbFetchPixel_r8g8b8;
    case PIXMAN_b8g8r8: return fbFetchPixel_b8g8r8;

        /* 16bpp formats */
    case PIXMAN_r5g6b5: return fbFetchPixel_r5g6b5;
    case PIXMAN_b5g6r5: return fbFetchPixel_b5g6r5;

    case PIXMAN_a1r5g5b5: return fbFetchPixel_a1r5g5b5;
    case PIXMAN_x1r5g5b5: return fbFetchPixel_x1r5g5b5;
    case PIXMAN_a1b5g5r5: return fbFetchPixel_a1b5g5r5;
    case PIXMAN_x1b5g5r5: return fbFetchPixel_x1b5g5r5;
    case PIXMAN_a4r4g4b4: return fbFetchPixel_a4r4g4b4;
    case PIXMAN_x4r4g4b4: return fbFetchPixel_x4r4g4b4;
    case PIXMAN_a4b4g4r4: return fbFetchPixel_a4b4g4r4;
    case PIXMAN_x4b4g4r4: return fbFetchPixel_x4b4g4r4;

        /* 8bpp formats */
    case PIXMAN_a8: return  fbFetchPixel_a8;
    case PIXMAN_r3g3b2: return fbFetchPixel_r3g3b2;
    case PIXMAN_b2g3r3: return fbFetchPixel_b2g3r3;
    case PIXMAN_a2r2g2b2: return fbFetchPixel_a2r2g2b2;
    case PIXMAN_a2b2g2r2: return fbFetchPixel_a2b2g2r2;
    case PIXMAN_c8: return  fbFetchPixel_c8;
    case PIXMAN_g8: return  fbFetchPixel_c8;
    case PIXMAN_x4a4: return fbFetchPixel_x4a4;

        /* 4bpp formats */
    case PIXMAN_a4: return  fbFetchPixel_a4;
    case PIXMAN_r1g2b1: return fbFetchPixel_r1g2b1;
    case PIXMAN_b1g2r1: return fbFetchPixel_b1g2r1;
    case PIXMAN_a1r1g1b1: return fbFetchPixel_a1r1g1b1;
    case PIXMAN_a1b1g1r1: return fbFetchPixel_a1b1g1r1;
    case PIXMAN_c4: return  fbFetchPixel_c4;
    case PIXMAN_g4: return  fbFetchPixel_c4;

        /* 1bpp formats */
    case PIXMAN_a1: return  fbFetchPixel_a1;
    case PIXMAN_g1: return  fbFetchPixel_g1;

        /* YUV formats */
    case PIXMAN_yuy2: return fbFetchPixel_yuy2;
    case PIXMAN_yv12: return fbFetchPixel_yv12;
    }

    return NULL;
}

static FASTCALL uint64_t
fbFetchPixel64_generic (bits_image_t *pict, int offset, int line)
{
    fetchPixelProc32 fetchPixel32 = ACCESS(pixman_fetchPixelProcForPicture32) (pict);
    uint32_t argb8Pixel = fetchPixel32(pict, offset, line);
    uint64_t argb16Pixel;

    pixman_expand(&argb16Pixel, &argb8Pixel, pict->format, 1);

    return argb16Pixel;
}

fetchPixelProc64 ACCESS(pixman_fetchPixelProcForPicture64) (bits_image_t * pict)
{
    switch(pict->format) {
    case PIXMAN_a2b10g10r10: return fbFetchPixel_a2b10g10r10;
    case PIXMAN_x2b10g10r10: return fbFetchPixel_x2b10g10r10;
    default: return fbFetchPixel64_generic;
    }
}

/*********************************** Store ************************************/

#define Splita(v)	uint32_t	a = ((v) >> 24), r = ((v) >> 16) & 0xff, g = ((v) >> 8) & 0xff, b = (v) & 0xff
#define Split(v)	uint32_t	r = ((v) >> 16) & 0xff, g = ((v) >> 8) & 0xff, b = (v) & 0xff

static FASTCALL void
fbStore_a2b10g10r10 (pixman_image_t *image,
		     uint32_t *bits, const uint64_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    uint32_t *pixel = bits + x;
    for (i = 0; i < width; ++i) {
        WRITE(image, pixel++,
            ((values[i] >> 32) & 0xc0000000) | // A
            ((values[i] >> 38) & 0x3ff) |      // R
            ((values[i] >> 12) & 0xffc00) |    // G
            ((values[i] << 14) & 0x3ff00000)); // B
    }
}

static FASTCALL void
fbStore_x2b10g10r10 (pixman_image_t *image,
		     uint32_t *bits, const uint64_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    uint32_t *pixel = bits + x;
    for (i = 0; i < width; ++i) {
        WRITE(image, pixel++,
            ((values[i] >> 38) & 0x3ff) |      // R
            ((values[i] >> 12) & 0xffc00) |    // G
            ((values[i] << 14) & 0x3ff00000)); // B
    }
}

static FASTCALL void
fbStore_a8r8g8b8 (pixman_image_t *image,
		  uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    MEMCPY_WRAPPED(image, ((uint32_t *)bits) + x, values, width*sizeof(uint32_t));
}

static FASTCALL void
fbStore_x8r8g8b8 (pixman_image_t *image,
		  uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    uint32_t *pixel = (uint32_t *)bits + x;
    for (i = 0; i < width; ++i)
	WRITE(image, pixel++, values[i] & 0xffffff);
}

static FASTCALL void
fbStore_a8b8g8r8 (pixman_image_t *image,
		  uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    uint32_t *pixel = (uint32_t *)bits + x;
    for (i = 0; i < width; ++i)
	WRITE(image, pixel++, (values[i] & 0xff00ff00) | ((values[i] >> 16) & 0xff) | ((values[i] & 0xff) << 16));
}

static FASTCALL void
fbStore_x8b8g8r8 (pixman_image_t *image,
		  uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    uint32_t *pixel = (uint32_t *)bits + x;
    for (i = 0; i < width; ++i)
	WRITE(image, pixel++, (values[i] & 0x0000ff00) | ((values[i] >> 16) & 0xff) | ((values[i] & 0xff) << 16));
}

static FASTCALL void
fbStore_r8g8b8 (pixman_image_t *image,
		uint32_t *bits, const uint32_t *values, int x, int width,
		const pixman_indexed_t * indexed)
{
    int i;
    uint8_t *pixel = ((uint8_t *) bits) + 3*x;
    for (i = 0; i < width; ++i) {
	Store24(image, pixel, values[i]);
	pixel += 3;
    }
}

static FASTCALL void
fbStore_b8g8r8 (pixman_image_t *image,
		uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    uint8_t *pixel = ((uint8_t *) bits) + 3*x;
    for (i = 0; i < width; ++i) {
	uint32_t val = values[i];
#if IMAGE_BYTE_ORDER == MSBFirst
	WRITE(image, pixel++, Blue(val));
	WRITE(image, pixel++, Green(val));
	WRITE(image, pixel++, Red(val));
#else
	WRITE(image, pixel++, Red(val));
	WRITE(image, pixel++, Green(val));
	WRITE(image, pixel++, Blue(val));
#endif
    }
}

static FASTCALL void
fbStore_r5g6b5 (pixman_image_t *image,
		uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    uint16_t *pixel = ((uint16_t *) bits) + x;
    for (i = 0; i < width; ++i) {
	uint32_t s = values[i];
	WRITE(image, pixel++, ((s >> 3) & 0x001f) |
	      ((s >> 5) & 0x07e0) |
	      ((s >> 8) & 0xf800));
    }
}

static FASTCALL void
fbStore_b5g6r5 (pixman_image_t *image,
		uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    uint16_t  *pixel = ((uint16_t *) bits) + x;
    for (i = 0; i < width; ++i) {
	Split(values[i]);
	WRITE(image, pixel++, ((b << 8) & 0xf800) |
	      ((g << 3) & 0x07e0) |
	      ((r >> 3)         ));
    }
}

static FASTCALL void
fbStore_a1r5g5b5 (pixman_image_t *image,
		  uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    uint16_t  *pixel = ((uint16_t *) bits) + x;
    for (i = 0; i < width; ++i) {
	Splita(values[i]);
	WRITE(image, pixel++, ((a << 8) & 0x8000) |
	      ((r << 7) & 0x7c00) |
	      ((g << 2) & 0x03e0) |
	      ((b >> 3)         ));
    }
}

static FASTCALL void
fbStore_x1r5g5b5 (pixman_image_t *image,
		  uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    uint16_t  *pixel = ((uint16_t *) bits) + x;
    for (i = 0; i < width; ++i) {
	Split(values[i]);
	WRITE(image, pixel++, ((r << 7) & 0x7c00) |
	      ((g << 2) & 0x03e0) |
	      ((b >> 3)         ));
    }
}

static FASTCALL void
fbStore_a1b5g5r5 (pixman_image_t *image,
		  uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    uint16_t  *pixel = ((uint16_t *) bits) + x;
    for (i = 0; i < width; ++i) {
	Splita(values[i]);
	WRITE(image, pixel++, ((a << 8) & 0x8000) |
	      ((b << 7) & 0x7c00) |
	      ((g << 2) & 0x03e0) |
	      ((r >> 3)         ));
    }
}

static FASTCALL void
fbStore_x1b5g5r5 (pixman_image_t *image,
		  uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    uint16_t  *pixel = ((uint16_t *) bits) + x;
    for (i = 0; i < width; ++i) {
	Split(values[i]);
	WRITE(image, pixel++, ((b << 7) & 0x7c00) |
	      ((g << 2) & 0x03e0) |
	      ((r >> 3)         ));
    }
}

static FASTCALL void
fbStore_a4r4g4b4 (pixman_image_t *image,
		  uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    uint16_t  *pixel = ((uint16_t *) bits) + x;
    for (i = 0; i < width; ++i) {
	Splita(values[i]);
	WRITE(image, pixel++, ((a << 8) & 0xf000) |
	      ((r << 4) & 0x0f00) |
	      ((g     ) & 0x00f0) |
	      ((b >> 4)         ));
    }
}

static FASTCALL void
fbStore_x4r4g4b4 (pixman_image_t *image,
		  uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    uint16_t  *pixel = ((uint16_t *) bits) + x;
    for (i = 0; i < width; ++i) {
	Split(values[i]);
	WRITE(image, pixel++, ((r << 4) & 0x0f00) |
	      ((g     ) & 0x00f0) |
	      ((b >> 4)         ));
    }
}

static FASTCALL void
fbStore_a4b4g4r4 (pixman_image_t *image,
		  uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    uint16_t  *pixel = ((uint16_t *) bits) + x;
    for (i = 0; i < width; ++i) {
	Splita(values[i]);
	WRITE(image, pixel++, ((a << 8) & 0xf000) |
	      ((b << 4) & 0x0f00) |
	      ((g     ) & 0x00f0) |
	      ((r >> 4)         ));
    }
}

static FASTCALL void
fbStore_x4b4g4r4 (pixman_image_t *image,
		  uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    uint16_t  *pixel = ((uint16_t *) bits) + x;
    for (i = 0; i < width; ++i) {
	Split(values[i]);
	WRITE(image, pixel++, ((b << 4) & 0x0f00) |
	      ((g     ) & 0x00f0) |
	      ((r >> 4)         ));
    }
}

static FASTCALL void
fbStore_a8 (pixman_image_t *image,
	    uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    uint8_t   *pixel = ((uint8_t *) bits) + x;
    for (i = 0; i < width; ++i) {
	WRITE(image, pixel++, values[i] >> 24);
    }
}

static FASTCALL void
fbStore_r3g3b2 (pixman_image_t *image,
		uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    uint8_t   *pixel = ((uint8_t *) bits) + x;
    for (i = 0; i < width; ++i) {
	Split(values[i]);
	WRITE(image, pixel++,
	      ((r     ) & 0xe0) |
	      ((g >> 3) & 0x1c) |
	      ((b >> 6)       ));
    }
}

static FASTCALL void
fbStore_b2g3r3 (pixman_image_t *image,
		uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    uint8_t   *pixel = ((uint8_t *) bits) + x;
    for (i = 0; i < width; ++i) {
	Split(values[i]);
	WRITE(image, pixel++,
	      ((b     ) & 0xc0) |
	      ((g >> 2) & 0x38) |
	      ((r >> 5)       ));
    }
}

static FASTCALL void
fbStore_a2r2g2b2 (pixman_image_t *image,
		  uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    uint8_t   *pixel = ((uint8_t *) bits) + x;
    for (i = 0; i < width; ++i) {
	Splita(values[i]);
	WRITE(image, pixel++, ((a     ) & 0xc0) |
	      ((r >> 2) & 0x30) |
	      ((g >> 4) & 0x0c) |
	      ((b >> 6)       ));
    }
}

static FASTCALL void
fbStore_c8 (pixman_image_t *image,
	    uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    uint8_t   *pixel = ((uint8_t *) bits) + x;
    for (i = 0; i < width; ++i) {
	WRITE(image, pixel++, miIndexToEnt24(indexed,values[i]));
    }
}

static FASTCALL void
fbStore_x4a4 (pixman_image_t *image,
	      uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    uint8_t   *pixel = ((uint8_t *) bits) + x;
    for (i = 0; i < width; ++i) {
	WRITE(image, pixel++, values[i] >> 28);
    }
}

#define Store8(img,l,o,v)  (WRITE(img, (uint8_t *)(l) + ((o) >> 3), (v)))
#if IMAGE_BYTE_ORDER == MSBFirst
#define Store4(img,l,o,v)  Store8(img,l,o,((o) & 4 ?				\
				   (Fetch8(img,l,o) & 0xf0) | (v) :		\
				   (Fetch8(img,l,o) & 0x0f) | ((v) << 4)))
#else
#define Store4(img,l,o,v)  Store8(img,l,o,((o) & 4 ?			       \
				   (Fetch8(img,l,o) & 0x0f) | ((v) << 4) : \
				   (Fetch8(img,l,o) & 0xf0) | (v)))
#endif

static FASTCALL void
fbStore_a4 (pixman_image_t *image,
	    uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    for (i = 0; i < width; ++i) {
	Store4(image, bits, i + x, values[i]>>28);
    }
}

static FASTCALL void
fbStore_r1g2b1 (pixman_image_t *image,
		uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    for (i = 0; i < width; ++i) {
	uint32_t  pixel;

	Split(values[i]);
	pixel = (((r >> 4) & 0x8) |
		 ((g >> 5) & 0x6) |
		 ((b >> 7)      ));
	Store4(image, bits, i + x, pixel);
    }
}

static FASTCALL void
fbStore_b1g2r1 (pixman_image_t *image,
		uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    for (i = 0; i < width; ++i) {
	uint32_t  pixel;

	Split(values[i]);
	pixel = (((b >> 4) & 0x8) |
		 ((g >> 5) & 0x6) |
		 ((r >> 7)      ));
	Store4(image, bits, i + x, pixel);
    }
}

static FASTCALL void
fbStore_a1r1g1b1 (pixman_image_t *image,
		  uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    for (i = 0; i < width; ++i) {
	uint32_t  pixel;
	Splita(values[i]);
	pixel = (((a >> 4) & 0x8) |
		 ((r >> 5) & 0x4) |
		 ((g >> 6) & 0x2) |
		 ((b >> 7)      ));
	Store4(image, bits, i + x, pixel);
    }
}

static FASTCALL void
fbStore_a1b1g1r1 (pixman_image_t *image,
		  uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    for (i = 0; i < width; ++i) {
	uint32_t  pixel;
	Splita(values[i]);
	pixel = (((a >> 4) & 0x8) |
		 ((b >> 5) & 0x4) |
		 ((g >> 6) & 0x2) |
		 ((r >> 7)      ));
	Store4(image, bits, i + x, pixel);
    }
}

static FASTCALL void
fbStore_c4 (pixman_image_t *image,
	    uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    for (i = 0; i < width; ++i) {
	uint32_t  pixel;

	pixel = miIndexToEnt24(indexed, values[i]);
	Store4(image, bits, i + x, pixel);
    }
}

static FASTCALL void
fbStore_a1 (pixman_image_t *image,
	    uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    for (i = 0; i < width; ++i) {
	uint32_t  *pixel = ((uint32_t *) bits) + ((i+x) >> 5);
	uint32_t  mask = FbStipMask((i+x) & 0x1f, 1);

	uint32_t v = values[i] & 0x80000000 ? mask : 0;
	WRITE(image, pixel, (READ(image, pixel) & ~mask) | v);
    }
}

static FASTCALL void
fbStore_g1 (pixman_image_t *image,
	    uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    for (i = 0; i < width; ++i) {
	uint32_t  *pixel = ((uint32_t *) bits) + ((i+x) >> 5);
	uint32_t  mask = FbStipMask((i+x) & 0x1f, 1);

	uint32_t v = miIndexToEntY24(indexed,values[i]) ? mask : 0;
	WRITE(image, pixel, (READ(image, pixel) & ~mask) | v);
    }
}


storeProc32 ACCESS(pixman_storeProcForPicture32) (bits_image_t * pict)
{
    switch(pict->format) {
    case PIXMAN_a8r8g8b8: return fbStore_a8r8g8b8;
    case PIXMAN_x8r8g8b8: return fbStore_x8r8g8b8;
    case PIXMAN_a8b8g8r8: return fbStore_a8b8g8r8;
    case PIXMAN_x8b8g8r8: return fbStore_x8b8g8r8;

        /* 24bpp formats */
    case PIXMAN_r8g8b8: return fbStore_r8g8b8;
    case PIXMAN_b8g8r8: return fbStore_b8g8r8;

        /* 16bpp formats */
    case PIXMAN_r5g6b5: return fbStore_r5g6b5;
    case PIXMAN_b5g6r5: return fbStore_b5g6r5;

    case PIXMAN_a1r5g5b5: return fbStore_a1r5g5b5;
    case PIXMAN_x1r5g5b5: return fbStore_x1r5g5b5;
    case PIXMAN_a1b5g5r5: return fbStore_a1b5g5r5;
    case PIXMAN_x1b5g5r5: return fbStore_x1b5g5r5;
    case PIXMAN_a4r4g4b4: return fbStore_a4r4g4b4;
    case PIXMAN_x4r4g4b4: return fbStore_x4r4g4b4;
    case PIXMAN_a4b4g4r4: return fbStore_a4b4g4r4;
    case PIXMAN_x4b4g4r4: return fbStore_x4b4g4r4;

        /* 8bpp formats */
    case PIXMAN_a8: return  fbStore_a8;
    case PIXMAN_r3g3b2: return fbStore_r3g3b2;
    case PIXMAN_b2g3r3: return fbStore_b2g3r3;
    case PIXMAN_a2r2g2b2: return fbStore_a2r2g2b2;
    case PIXMAN_c8: return  fbStore_c8;
    case PIXMAN_g8: return  fbStore_c8;
    case PIXMAN_x4a4: return fbStore_x4a4;

        /* 4bpp formats */
    case PIXMAN_a4: return  fbStore_a4;
    case PIXMAN_r1g2b1: return fbStore_r1g2b1;
    case PIXMAN_b1g2r1: return fbStore_b1g2r1;
    case PIXMAN_a1r1g1b1: return fbStore_a1r1g1b1;
    case PIXMAN_a1b1g1r1: return fbStore_a1b1g1r1;
    case PIXMAN_c4: return  fbStore_c4;
    case PIXMAN_g4: return  fbStore_c4;

        /* 1bpp formats */
    case PIXMAN_a1: return  fbStore_a1;
    case PIXMAN_g1: return  fbStore_g1;
    default:
        return NULL;
    }
}

/*
 * Contracts a 64bpp image to 32bpp and then stores it using a regular 32-bit
 * store proc.
 */
static FASTCALL void
fbStore64_generic (pixman_image_t *image,
		   uint32_t *bits, const uint64_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    bits_image_t *pict = (bits_image_t*)image;
    storeProc32 store32 = ACCESS(pixman_storeProcForPicture32) (pict);
    uint32_t *argb8Pixels;

    assert(image->common.type == BITS);
    assert(store32);

    argb8Pixels = pixman_malloc_ab (width, sizeof(uint32_t));
    if (!argb8Pixels) return;

    // Contract the scanline.  We could do this in place if values weren't
    // const.
    pixman_contract(argb8Pixels, values, width);
    store32(image, bits, argb8Pixels, x, width, indexed);

    free(argb8Pixels);
}

storeProc64 ACCESS(pixman_storeProcForPicture64) (bits_image_t * pict)
{
    switch(pict->format) {
    case PIXMAN_a2b10g10r10: return fbStore_a2b10g10r10;
    case PIXMAN_x2b10g10r10: return fbStore_x2b10g10r10;
    default: return fbStore64_generic;
    }
}

#ifndef PIXMAN_FB_ACCESSORS
/*
 * Helper routine to expand a color component from 0 < n <= 8 bits to 16 bits by
 * replication.
 */
static inline uint64_t expand16(const uint8_t val, int nbits)
{
    // Start out with the high bit of val in the high bit of result.
    uint16_t result = (uint16_t)val << (16 - nbits);

    if (nbits == 0)
        return 0;

    // Copy the bits in result, doubling the number of bits each time, until we
    // fill all 16 bits.
    while (nbits < 16) {
        result |= result >> nbits;
        nbits *= 2;
    }

    return result;
}

/*
 * This function expands images from ARGB8 format to ARGB16.  To preserve
 * precision, it needs to know the original source format.  For example, if the
 * source was PIXMAN_x1r5g5b5 and the red component contained bits 12345, then
 * the expanded value is 12345123.  To correctly expand this to 16 bits, it
 * should be 1234512345123451 and not 1234512312345123.
 */
void pixman_expand(uint64_t *dst, const uint32_t *src,
                   pixman_format_code_t format, int width)
{
    /*
     * Determine the sizes of each component and the masks and shifts required
     * to extract them from the source pixel.
     */
    const int a_size = PIXMAN_FORMAT_A(format),
              r_size = PIXMAN_FORMAT_R(format),
              g_size = PIXMAN_FORMAT_G(format),
              b_size = PIXMAN_FORMAT_B(format);
    const int a_shift = 32 - a_size,
              r_shift = 24 - r_size,
              g_shift = 16 - g_size,
              b_shift =  8 - b_size;
    const uint8_t a_mask = ~(~0 << a_size),
                  r_mask = ~(~0 << r_size),
                  g_mask = ~(~0 << g_size),
                  b_mask = ~(~0 << b_size);
    int i;

    /* Start at the end so that we can do the expansion in place when src == dst */
    for (i = width - 1; i >= 0; i--)
    {
        const uint32_t pixel = src[i];
        // Extract the components.
        const uint8_t a = (pixel >> a_shift) & a_mask,
                      r = (pixel >> r_shift) & r_mask,
                      g = (pixel >> g_shift) & g_mask,
                      b = (pixel >> b_shift) & b_mask;
        const uint64_t a16 = a_size ? expand16(a, a_size) : 0xffff,
                       r16 = expand16(r, r_size),
                       g16 = expand16(g, g_size),
                       b16 = expand16(b, b_size);

        dst[i] = a16 << 48 | r16 << 32 | g16 << 16 | b16;
    }
}

/*
 * Contracting is easier than expanding.  We just need to truncate the
 * components.
 */
void pixman_contract(uint32_t *dst, const uint64_t *src, int width)
{
    int i;

    /* Start at the beginning so that we can do the contraction in place when
     * src == dst */
    for (i = 0; i < width; i++)
    {
        const uint8_t a = src[i] >> 56,
                      r = src[i] >> 40,
                      g = src[i] >> 24,
                      b = src[i] >> 8;
        dst[i] = a << 24 | r << 16 | g << 8 | b;
    }
}
#endif // PIXMAN_FB_ACCESSORS
