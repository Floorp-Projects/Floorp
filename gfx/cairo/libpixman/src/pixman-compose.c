/*
 *
 * Copyright © 2000 Keith Packard, member of The XFree86 Project, Inc.
 *             2005 Lars Knoll & Zack Rusin, Trolltech
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
#include <math.h>
#include <assert.h>
#include <limits.h>

#include "pixman-private.h"

/*
 *    FIXME:
 *		The stuff here is added just to get it to compile. Something sensible needs to
 *              be done before this can be used.
 *
 *   we should go through this code and clean up some of the weird stuff that have
 *   resulted from unmacro-ifying it.
 *
 */
#define INLINE inline

/*   End of stuff added to get it to compile
 */ 

static unsigned int
SourcePictureClassify (source_image_t *pict,
		       int	       x,
		       int	       y,
		       int	       width,
		       int	       height)
{
    if (pict->common.type == SOLID)
    {
	pict->class = SOURCE_IMAGE_CLASS_HORIZONTAL;
    }
    else if (pict->common.type == LINEAR)
    {
	linear_gradient_t *linear = (linear_gradient_t *)pict;
	pixman_vector_t   v;
	pixman_fixed_32_32_t l;
	pixman_fixed_48_16_t dx, dy, a, b, off;
	pixman_fixed_48_16_t factors[4];
	int	     i;
	
	dx = linear->p2.x - linear->p1.x;
	dy = linear->p2.y - linear->p1.y;
	l = dx * dx + dy * dy;
	if (l)
	{
	    a = (dx << 32) / l;
	    b = (dy << 32) / l;
	}
	else
	{
	    a = b = 0;
	}
	
	off = (-a * linear->p1.x
	       -b * linear->p1.y) >> 16;
	
	for (i = 0; i < 3; i++)
	{
	    v.vector[0] = pixman_int_to_fixed ((i % 2) * (width  - 1) + x);
	    v.vector[1] = pixman_int_to_fixed ((i / 2) * (height - 1) + y);
	    v.vector[2] = pixman_fixed_1;
	    
	    if (pict->common.transform)
	    {
		if (!pixman_transform_point_3d (pict->common.transform, &v))
		    return SOURCE_IMAGE_CLASS_UNKNOWN;
	    }
	    
	    factors[i] = ((a * v.vector[0] + b * v.vector[1]) >> 16) + off;
	}
	
	if (factors[2] == factors[0])
	    pict->class = SOURCE_IMAGE_CLASS_HORIZONTAL;
	else if (factors[1] == factors[0])
	    pict->class = SOURCE_IMAGE_CLASS_VERTICAL;
    }
    
    return pict->class;
}

#define SCANLINE_BUFFER_LENGTH 2048

typedef FASTCALL void (*fetchProc)(pixman_image_t *image,
				   const uint32_t *bits,
				   int x, int width,
				   uint32_t *buffer,
				   const pixman_indexed_t * indexed);

/*
 * All of the fetch functions
 */

static FASTCALL void
fbFetch_a8r8g8b8 (pixman_image_t *image,
		  const uint32_t *bits, int x, int width, uint32_t *buffer, const pixman_indexed_t * indexed)
{
    MEMCPY_WRAPPED(buffer, (const uint32_t *)bits + x,
		   width*sizeof(uint32_t));
}

static FASTCALL void
fbFetch_x8r8g8b8 (pixman_image_t *image,
		  const uint32_t *bits, int x, int width, uint32_t *buffer, const pixman_indexed_t * indexed)
{
    const uint32_t *pixel = (const uint32_t *)bits + x;
    const uint32_t *end = pixel + width;
    while (pixel < end) {
	*buffer++ = READ(pixel++) | 0xff000000;
    }
}

static FASTCALL void
fbFetch_a8b8g8r8 (pixman_image_t *image,
		  const uint32_t *bits, int x, int width, uint32_t *buffer, const pixman_indexed_t * indexed)
{
    const uint32_t *pixel = (uint32_t *)bits + x;
    const uint32_t *end = pixel + width;
    while (pixel < end) {
	uint32_t p = READ(pixel++);
	*buffer++ = (p & 0xff00ff00) |
	            ((p >> 16) & 0xff) |
	    ((p & 0xff) << 16);
    }
}

static FASTCALL void
fbFetch_x8b8g8r8 (pixman_image_t *image,
		  const uint32_t *bits, int x, int width, uint32_t *buffer, const pixman_indexed_t * indexed)
{
    const uint32_t *pixel = (uint32_t *)bits + x;
    const uint32_t *end = pixel + width;
    while (pixel < end) {
	uint32_t p = READ(pixel++);
	*buffer++ = 0xff000000 |
	    (p & 0x0000ff00) |
	    ((p >> 16) & 0xff) |
	    ((p & 0xff) << 16);
    }
}

static FASTCALL void
fbFetch_r8g8b8 (pixman_image_t *image,
		const uint32_t *bits, int x, int width, uint32_t *buffer, const pixman_indexed_t * indexed)
{
    const uint8_t *pixel = (const uint8_t *)bits + 3*x;
    const uint8_t *end = pixel + 3*width;
    while (pixel < end) {
	uint32_t b = Fetch24(pixel) | 0xff000000;
	pixel += 3;
	*buffer++ = b;
    }
}

static FASTCALL void
fbFetch_b8g8r8 (pixman_image_t *image,
		const uint32_t *bits, int x, int width, uint32_t *buffer, const pixman_indexed_t * indexed)
{
    const uint8_t *pixel = (const uint8_t *)bits + 3*x;
    const uint8_t *end = pixel + 3*width;
    while (pixel < end) {
	uint32_t b = 0xff000000;
#if IMAGE_BYTE_ORDER == MSBFirst
	b |= (READ(pixel++));
	b |= (READ(pixel++) << 8);
	b |= (READ(pixel++) << 16);
#else
	b |= (READ(pixel++) << 16);
	b |= (READ(pixel++) << 8);
	b |= (READ(pixel++));
#endif
	*buffer++ = b;
    }
}

static FASTCALL void
fbFetch_r5g6b5 (pixman_image_t *image,
		const uint32_t *bits, int x, int width, uint32_t *buffer,
		const pixman_indexed_t * indexed)
{
    const uint16_t *pixel = (const uint16_t *)bits + x;
    const uint16_t *end = pixel + width;
    while (pixel < end) {
	uint32_t p = READ(pixel++);
	uint32_t r = (((p) << 3) & 0xf8) | 
	    (((p) << 5) & 0xfc00) |
	    (((p) << 8) & 0xf80000);
	r |= (r >> 5) & 0x70007;
	r |= (r >> 6) & 0x300;
	*buffer++ = 0xff000000 | r;
    }
}

static FASTCALL void
fbFetch_b5g6r5 (pixman_image_t *image,
		const uint32_t *bits, int x, int width, uint32_t *buffer,
		const pixman_indexed_t * indexed)
{
    uint32_t  r,g,b;
    
    const uint16_t *pixel = (const uint16_t *)bits + x;
    const uint16_t *end = pixel + width;
    while (pixel < end) {
	uint32_t  p = READ(pixel++);
	b = ((p & 0xf800) | ((p & 0xe000) >> 5)) >> 8;
	g = ((p & 0x07e0) | ((p & 0x0600) >> 6)) << 5;
	r = ((p & 0x001c) | ((p & 0x001f) << 5)) << 14;
	*buffer++ = 0xff000000 | r | g | b;
    }
}

static FASTCALL void
fbFetch_a1r5g5b5 (pixman_image_t *image,
		  const uint32_t *bits, int x, int width, uint32_t *buffer, const pixman_indexed_t * indexed)
{
    uint32_t  r,g,b, a;
    
    const uint16_t *pixel = (const uint16_t *)bits + x;
    const uint16_t *end = pixel + width;
    while (pixel < end) {
	uint32_t  p = READ(pixel++);
	
	a = (uint32_t) ((uint8_t) (0 - ((p & 0x8000) >> 15))) << 24;
	r = ((p & 0x7c00) | ((p & 0x7000) >> 5)) << 9;
	g = ((p & 0x03e0) | ((p & 0x0380) >> 5)) << 6;
	b = ((p & 0x001c) | ((p & 0x001f) << 5)) >> 2;
	*buffer++ = a | r | g | b;
    }
}

static FASTCALL void
fbFetch_x1r5g5b5 (pixman_image_t *image,
		  const uint32_t *bits, int x, int width, uint32_t *buffer, const pixman_indexed_t * indexed)
{
    uint32_t  r,g,b;
    
    const uint16_t *pixel = (const uint16_t *)bits + x;
    const uint16_t *end = pixel + width;
    while (pixel < end) {
	uint32_t  p = READ(pixel++);
	
	r = ((p & 0x7c00) | ((p & 0x7000) >> 5)) << 9;
	g = ((p & 0x03e0) | ((p & 0x0380) >> 5)) << 6;
	b = ((p & 0x001c) | ((p & 0x001f) << 5)) >> 2;
	*buffer++ = 0xff000000 | r | g | b;
    }
}

static FASTCALL void
fbFetch_a1b5g5r5 (pixman_image_t *image,
		  const uint32_t *bits, int x, int width, uint32_t *buffer, const pixman_indexed_t * indexed)
{
    uint32_t  r,g,b, a;
    
    const uint16_t *pixel = (const uint16_t *)bits + x;
    const uint16_t *end = pixel + width;
    while (pixel < end) {
	uint32_t  p = READ(pixel++);
	
	a = (uint32_t) ((uint8_t) (0 - ((p & 0x8000) >> 15))) << 24;
	b = ((p & 0x7c00) | ((p & 0x7000) >> 5)) >> 7;
	g = ((p & 0x03e0) | ((p & 0x0380) >> 5)) << 6;
	r = ((p & 0x001c) | ((p & 0x001f) << 5)) << 14;
	*buffer++ = a | r | g | b;
    }
}

static FASTCALL void
fbFetch_x1b5g5r5 (pixman_image_t *image,
		  const uint32_t *bits, int x, int width, uint32_t *buffer, const pixman_indexed_t * indexed)
{
    uint32_t  r,g,b;
    
    const uint16_t *pixel = (const uint16_t *)bits + x;
    const uint16_t *end = pixel + width;
    while (pixel < end) {
	uint32_t  p = READ(pixel++);
	
	b = ((p & 0x7c00) | ((p & 0x7000) >> 5)) >> 7;
	g = ((p & 0x03e0) | ((p & 0x0380) >> 5)) << 6;
	r = ((p & 0x001c) | ((p & 0x001f) << 5)) << 14;
	*buffer++ = 0xff000000 | r | g | b;
    }
}

static FASTCALL void
fbFetch_a4r4g4b4 (pixman_image_t *image,
		  const uint32_t *bits, int x, int width, uint32_t *buffer, const pixman_indexed_t * indexed)
{
    uint32_t  r,g,b, a;
    const uint16_t *pixel = (const uint16_t *)bits + x;
    const uint16_t *end = pixel + width;
    while (pixel < end) {
	uint32_t  p = READ(pixel++);
	
	a = ((p & 0xf000) | ((p & 0xf000) >> 4)) << 16;
	r = ((p & 0x0f00) | ((p & 0x0f00) >> 4)) << 12;
	g = ((p & 0x00f0) | ((p & 0x00f0) >> 4)) << 8;
	b = ((p & 0x000f) | ((p & 0x000f) << 4));
	*buffer++ = a | r | g | b;
    }
}

static FASTCALL void
fbFetch_x4r4g4b4 (pixman_image_t *image,
		  const uint32_t *bits, int x, int width, uint32_t *buffer, const pixman_indexed_t * indexed)
{
    uint32_t  r,g,b;
    
    const uint16_t *pixel = (const uint16_t *)bits + x;
    const uint16_t *end = pixel + width;
    while (pixel < end) {
	uint32_t  p = READ(pixel++);
	
	r = ((p & 0x0f00) | ((p & 0x0f00) >> 4)) << 12;
	g = ((p & 0x00f0) | ((p & 0x00f0) >> 4)) << 8;
	b = ((p & 0x000f) | ((p & 0x000f) << 4));
	*buffer++ = 0xff000000 | r | g | b;
    }
}

static FASTCALL void
fbFetch_a4b4g4r4 (pixman_image_t *image,
		  const uint32_t *bits, int x, int width, uint32_t *buffer, const pixman_indexed_t * indexed)
{
    uint32_t  r,g,b, a;
    
    const uint16_t *pixel = (const uint16_t *)bits + x;
    const uint16_t *end = pixel + width;
    while (pixel < end) {
	uint32_t  p = READ(pixel++);
	
	a = ((p & 0xf000) | ((p & 0xf000) >> 4)) << 16;
	b = ((p & 0x0f00) | ((p & 0x0f00) >> 4)) >> 4;
	g = ((p & 0x00f0) | ((p & 0x00f0) >> 4)) << 8;
	r = ((p & 0x000f) | ((p & 0x000f) << 4)) << 16;
	*buffer++ = a | r | g | b;
    }
}

static FASTCALL void
fbFetch_x4b4g4r4 (pixman_image_t *image,
		  const uint32_t *bits, int x, int width, uint32_t *buffer, const pixman_indexed_t * indexed)
{
    uint32_t  r,g,b;
    
    const uint16_t *pixel = (const uint16_t *)bits + x;
    const uint16_t *end = pixel + width;
    while (pixel < end) {
	uint32_t  p = READ(pixel++);
	
	b = ((p & 0x0f00) | ((p & 0x0f00) >> 4)) >> 4;
	g = ((p & 0x00f0) | ((p & 0x00f0) >> 4)) << 8;
	r = ((p & 0x000f) | ((p & 0x000f) << 4)) << 16;
	*buffer++ = 0xff000000 | r | g | b;
    }
}

static FASTCALL void
fbFetch_a8 (pixman_image_t *image,
	    const uint32_t *bits, int x, int width, uint32_t *buffer, const pixman_indexed_t * indexed)
{
    const uint8_t *pixel = (const uint8_t *)bits + x;
    const uint8_t *end = pixel + width;
    while (pixel < end) {
	*buffer++ = READ(pixel++) << 24;
    }
}

static FASTCALL void
fbFetch_r3g3b2 (pixman_image_t *image,
		const uint32_t *bits, int x, int width, uint32_t *buffer, const pixman_indexed_t * indexed)
{
    uint32_t  r,g,b;
    
    const uint8_t *pixel = (const uint8_t *)bits + x;
    const uint8_t *end = pixel + width;
    while (pixel < end) {
	uint32_t  p = READ(pixel++);
	
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
fbFetch_b2g3r3 (pixman_image_t *image,
		const uint32_t *bits, int x, int width, uint32_t *buffer, const pixman_indexed_t * indexed)
{
    uint32_t  r,g,b;
    
    const uint8_t *pixel = (const uint8_t *)bits + x;
    const uint8_t *end = pixel + width;
    while (pixel < end) {
	uint32_t  p = READ(pixel++);
	
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
fbFetch_a2r2g2b2 (pixman_image_t *image,
		  const uint32_t *bits, int x, int width, uint32_t *buffer, const pixman_indexed_t * indexed)
{
    uint32_t   a,r,g,b;
    const uint8_t *pixel = (const uint8_t *)bits + x;
    const uint8_t *end = pixel + width;
    while (pixel < end) {
	uint32_t  p = READ(pixel++);
	
	a = ((p & 0xc0) * 0x55) << 18;
	r = ((p & 0x30) * 0x55) << 12;
	g = ((p & 0x0c) * 0x55) << 6;
	b = ((p & 0x03) * 0x55);
	*buffer++ = a|r|g|b;
    }
}

static FASTCALL void
fbFetch_a2b2g2r2 (pixman_image_t *image,
		  const uint32_t *bits, int x, int width, uint32_t *buffer, const pixman_indexed_t * indexed)
{
    uint32_t   a,r,g,b;
    const uint8_t *pixel = (const uint8_t *)bits + x;
    const uint8_t *end = pixel + width;
    while (pixel < end) {
	uint32_t  p = READ(pixel++);
	
	a = ((p & 0xc0) * 0x55) << 18;
	b = ((p & 0x30) * 0x55) >> 6;
	g = ((p & 0x0c) * 0x55) << 6;
	r = ((p & 0x03) * 0x55) << 16;
	*buffer++ = a|r|g|b;
    }
}

static FASTCALL void
fbFetch_c8 (pixman_image_t *image,
	    const uint32_t *bits, int x, int width, uint32_t *buffer, const pixman_indexed_t * indexed)
{
    const uint8_t *pixel = (const uint8_t *)bits + x;
    const uint8_t *end = pixel + width;
    while (pixel < end) {
	uint32_t  p = READ(pixel++);
	*buffer++ = indexed->rgba[p];
    }
}

static FASTCALL void
fbFetch_x4a4 (pixman_image_t *image,
	      const uint32_t *bits, int x, int width, uint32_t *buffer, const pixman_indexed_t * indexed)
{
    const uint8_t *pixel = (const uint8_t *)bits + x;
    const uint8_t *end = pixel + width;
    while (pixel < end) {
	uint8_t p = READ(pixel++) & 0xf;
	*buffer++ = (p | (p << 4)) << 24;
    }
}

#define Fetch8(l,o)    (READ((uint8_t *)(l) + ((o) >> 2)))
#if IMAGE_BYTE_ORDER == MSBFirst
#define Fetch4(l,o)    ((o) & 2 ? Fetch8(l,o) & 0xf : Fetch8(l,o) >> 4)
#else
#define Fetch4(l,o)    ((o) & 2 ? Fetch8(l,o) >> 4 : Fetch8(l,o) & 0xf)
#endif

static FASTCALL void
fbFetch_a4 (pixman_image_t *image,
	    const uint32_t *bits, int x, int width, uint32_t *buffer, const pixman_indexed_t * indexed)
{
    int i;
    for (i = 0; i < width; ++i) {
	uint32_t  p = Fetch4(bits, i + x);
	
	p |= p << 4;
	*buffer++ = p << 24;
    }
}

static FASTCALL void
fbFetch_r1g2b1 (pixman_image_t *image,
		const uint32_t *bits, int x, int width, uint32_t *buffer, const pixman_indexed_t * indexed)
{
    uint32_t  r,g,b;
    int i;
    for (i = 0; i < width; ++i) {
	uint32_t  p = Fetch4(bits, i + x);
	
	r = ((p & 0x8) * 0xff) << 13;
	g = ((p & 0x6) * 0x55) << 7;
	b = ((p & 0x1) * 0xff);
	*buffer++ = 0xff000000|r|g|b;
    }
}

static FASTCALL void
fbFetch_b1g2r1 (pixman_image_t *image,
		const uint32_t *bits, int x, int width, uint32_t *buffer, const pixman_indexed_t * indexed)
{
    uint32_t  r,g,b;
    int i;
    for (i = 0; i < width; ++i) {
	uint32_t  p = Fetch4(bits, i + x);
	
	b = ((p & 0x8) * 0xff) >> 3;
	g = ((p & 0x6) * 0x55) << 7;
	r = ((p & 0x1) * 0xff) << 16;
	*buffer++ = 0xff000000|r|g|b;
    }
}

static FASTCALL void
fbFetch_a1r1g1b1 (pixman_image_t *image,
		  const uint32_t *bits, int x, int width, uint32_t *buffer, const pixman_indexed_t * indexed)
{
    uint32_t  a,r,g,b;
    int i;
    for (i = 0; i < width; ++i) {
	uint32_t  p = Fetch4(bits, i + x);
	
	a = ((p & 0x8) * 0xff) << 21;
	r = ((p & 0x4) * 0xff) << 14;
	g = ((p & 0x2) * 0xff) << 7;
	b = ((p & 0x1) * 0xff);
	*buffer++ = a|r|g|b;
    }
}

static FASTCALL void
fbFetch_a1b1g1r1 (pixman_image_t *image,
		  const uint32_t *bits, int x, int width, uint32_t *buffer, const pixman_indexed_t * indexed)
{
    uint32_t  a,r,g,b;
    int i;
    for (i = 0; i < width; ++i) {
	uint32_t  p = Fetch4(bits, i + x);
	
	a = ((p & 0x8) * 0xff) << 21;
	r = ((p & 0x4) * 0xff) >> 3;
	g = ((p & 0x2) * 0xff) << 7;
	b = ((p & 0x1) * 0xff) << 16;
	*buffer++ = a|r|g|b;
    }
}

static FASTCALL void
fbFetch_c4 (pixman_image_t *image,
	    const uint32_t *bits, int x, int width, uint32_t *buffer, const pixman_indexed_t * indexed)
{
    int i;
    for (i = 0; i < width; ++i) {
	uint32_t  p = Fetch4(bits, i + x);
	
	*buffer++ = indexed->rgba[p];
    }
}


static FASTCALL void
fbFetch_a1 (pixman_image_t *image,
	    const uint32_t *bits, int x, int width, uint32_t *buffer, const pixman_indexed_t * indexed)
{
    int i;
    for (i = 0; i < width; ++i) {
	uint32_t  p = READ(bits + ((i + x) >> 5));
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
fbFetch_g1 (pixman_image_t *image,
	    const uint32_t *bits, int x, int width, uint32_t *buffer, const pixman_indexed_t * indexed)
{
    int i;
    for (i = 0; i < width; ++i) {
	uint32_t p = READ(bits + ((i+x) >> 5));
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

static fetchProc fetchProcForPicture (bits_image_t * pict)
{
    switch(pict->format) {
    case PIXMAN_a8r8g8b8: return fbFetch_a8r8g8b8;
    case PIXMAN_x8r8g8b8: return fbFetch_x8r8g8b8;
    case PIXMAN_a8b8g8r8: return fbFetch_a8b8g8r8;
    case PIXMAN_x8b8g8r8: return fbFetch_x8b8g8r8;
	
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
    }
    
    return NULL;
}

/*
 * Pixel wise fetching
 */

typedef FASTCALL uint32_t (*fetchPixelProc)(pixman_image_t *image,
					    const uint32_t *bits, int offset,
					    const pixman_indexed_t * indexed);

static FASTCALL uint32_t
fbFetchPixel_a8r8g8b8 (pixman_image_t *image,
		       const uint32_t *bits, int offset, const pixman_indexed_t * indexed)
{
    return READ((uint32_t *)bits + offset);
}

static FASTCALL uint32_t
fbFetchPixel_x8r8g8b8 (pixman_image_t *image,
		       const uint32_t *bits, int offset, const pixman_indexed_t * indexed)
{
    return READ((uint32_t *)bits + offset) | 0xff000000;
}

static FASTCALL uint32_t
fbFetchPixel_a8b8g8r8 (pixman_image_t *image,
		       const uint32_t *bits, int offset, const pixman_indexed_t * indexed)
{
    uint32_t  pixel = READ((uint32_t *)bits + offset);
    
    return ((pixel & 0xff000000) |
	    ((pixel >> 16) & 0xff) |
	    (pixel & 0x0000ff00) |
	    ((pixel & 0xff) << 16));
}

static FASTCALL uint32_t
fbFetchPixel_x8b8g8r8 (pixman_image_t *image,
		       const uint32_t *bits, int offset, const pixman_indexed_t * indexed)
{
    uint32_t  pixel = READ((uint32_t *)bits + offset);
    
    return ((0xff000000) |
	    ((pixel >> 16) & 0xff) |
	    (pixel & 0x0000ff00) |
	    ((pixel & 0xff) << 16));
}

static FASTCALL uint32_t
fbFetchPixel_r8g8b8 (pixman_image_t *image,
		     const uint32_t *bits, int offset, const pixman_indexed_t * indexed)
{
    uint8_t   *pixel = ((uint8_t *) bits) + (offset*3);
#if IMAGE_BYTE_ORDER == MSBFirst
    return (0xff000000 |
	    (READ(pixel + 0) << 16) |
	    (READ(pixel + 1) << 8) |
	    (READ(pixel + 2)));
#else
    return (0xff000000 |
	    (READ(pixel + 2) << 16) |
	    (READ(pixel + 1) << 8) |
	    (READ(pixel + 0)));
#endif
}

static FASTCALL uint32_t
fbFetchPixel_b8g8r8 (pixman_image_t *image,
		     const uint32_t *bits, int offset, const pixman_indexed_t * indexed)
{
    uint8_t   *pixel = ((uint8_t *) bits) + (offset*3);
#if IMAGE_BYTE_ORDER == MSBFirst
    return (0xff000000 |
	    (READ(pixel + 2) << 16) |
	    (READ(pixel + 1) << 8) |
	    (READ(pixel + 0)));
#else
    return (0xff000000 |
	    (READ(pixel + 0) << 16) |
	    (READ(pixel + 1) << 8) |
	    (READ(pixel + 2)));
#endif
}

static FASTCALL uint32_t
fbFetchPixel_r5g6b5 (pixman_image_t *image,
		     const uint32_t *bits, int offset, const pixman_indexed_t * indexed)
{
    uint32_t  r,g,b;
    uint32_t  pixel = READ((uint16_t *) bits + offset);
    
    r = ((pixel & 0xf800) | ((pixel & 0xe000) >> 5)) << 8;
    g = ((pixel & 0x07e0) | ((pixel & 0x0600) >> 6)) << 5;
    b = ((pixel & 0x001c) | ((pixel & 0x001f) << 5)) >> 2;
    return (0xff000000 | r | g | b);
}

static FASTCALL uint32_t
fbFetchPixel_b5g6r5 (pixman_image_t *image,
		     const uint32_t *bits, int offset, const pixman_indexed_t * indexed)
{
    uint32_t  r,g,b;
    uint32_t  pixel = READ((uint16_t *) bits + offset);
    
    b = ((pixel & 0xf800) | ((pixel & 0xe000) >> 5)) >> 8;
    g = ((pixel & 0x07e0) | ((pixel & 0x0600) >> 6)) << 5;
    r = ((pixel & 0x001c) | ((pixel & 0x001f) << 5)) << 14;
    return (0xff000000 | r | g | b);
}

static FASTCALL uint32_t
fbFetchPixel_a1r5g5b5 (pixman_image_t *image,
		       const uint32_t *bits, int offset, const pixman_indexed_t * indexed)
{
    uint32_t  a,r,g,b;
    uint32_t  pixel = READ((uint16_t *) bits + offset);
    
    a = (uint32_t) ((uint8_t) (0 - ((pixel & 0x8000) >> 15))) << 24;
    r = ((pixel & 0x7c00) | ((pixel & 0x7000) >> 5)) << 9;
    g = ((pixel & 0x03e0) | ((pixel & 0x0380) >> 5)) << 6;
    b = ((pixel & 0x001c) | ((pixel & 0x001f) << 5)) >> 2;
    return (a | r | g | b);
}

static FASTCALL uint32_t
fbFetchPixel_x1r5g5b5 (pixman_image_t *image,
		       const uint32_t *bits, int offset, const pixman_indexed_t * indexed)
{
    uint32_t  r,g,b;
    uint32_t  pixel = READ((uint16_t *) bits + offset);
    
    r = ((pixel & 0x7c00) | ((pixel & 0x7000) >> 5)) << 9;
    g = ((pixel & 0x03e0) | ((pixel & 0x0380) >> 5)) << 6;
    b = ((pixel & 0x001c) | ((pixel & 0x001f) << 5)) >> 2;
    return (0xff000000 | r | g | b);
}

static FASTCALL uint32_t
fbFetchPixel_a1b5g5r5 (pixman_image_t *image,
		       const uint32_t *bits, int offset, const pixman_indexed_t * indexed)
{
    uint32_t  a,r,g,b;
    uint32_t  pixel = READ((uint16_t *) bits + offset);
    
    a = (uint32_t) ((uint8_t) (0 - ((pixel & 0x8000) >> 15))) << 24;
    b = ((pixel & 0x7c00) | ((pixel & 0x7000) >> 5)) >> 7;
    g = ((pixel & 0x03e0) | ((pixel & 0x0380) >> 5)) << 6;
    r = ((pixel & 0x001c) | ((pixel & 0x001f) << 5)) << 14;
    return (a | r | g | b);
}

static FASTCALL uint32_t
fbFetchPixel_x1b5g5r5 (pixman_image_t *image,
		       const uint32_t *bits, int offset, const pixman_indexed_t * indexed)
{
    uint32_t  r,g,b;
    uint32_t  pixel = READ((uint16_t *) bits + offset);
    
    b = ((pixel & 0x7c00) | ((pixel & 0x7000) >> 5)) >> 7;
    g = ((pixel & 0x03e0) | ((pixel & 0x0380) >> 5)) << 6;
    r = ((pixel & 0x001c) | ((pixel & 0x001f) << 5)) << 14;
    return (0xff000000 | r | g | b);
}

static FASTCALL uint32_t
fbFetchPixel_a4r4g4b4 (pixman_image_t *image,
		       const uint32_t *bits, int offset, const pixman_indexed_t * indexed)
{
    uint32_t  a,r,g,b;
    uint32_t  pixel = READ((uint16_t *) bits + offset);
    
    a = ((pixel & 0xf000) | ((pixel & 0xf000) >> 4)) << 16;
    r = ((pixel & 0x0f00) | ((pixel & 0x0f00) >> 4)) << 12;
    g = ((pixel & 0x00f0) | ((pixel & 0x00f0) >> 4)) << 8;
    b = ((pixel & 0x000f) | ((pixel & 0x000f) << 4));
    return (a | r | g | b);
}

static FASTCALL uint32_t
fbFetchPixel_x4r4g4b4 (pixman_image_t *image,
		       const uint32_t *bits, int offset, const pixman_indexed_t * indexed)
{
    uint32_t  r,g,b;
    uint32_t  pixel = READ((uint16_t *) bits + offset);
    
    r = ((pixel & 0x0f00) | ((pixel & 0x0f00) >> 4)) << 12;
    g = ((pixel & 0x00f0) | ((pixel & 0x00f0) >> 4)) << 8;
    b = ((pixel & 0x000f) | ((pixel & 0x000f) << 4));
    return (0xff000000 | r | g | b);
}

static FASTCALL uint32_t
fbFetchPixel_a4b4g4r4 (pixman_image_t *image,
		       const uint32_t *bits, int offset, const pixman_indexed_t * indexed)
{
    uint32_t  a,r,g,b;
    uint32_t  pixel = READ((uint16_t *) bits + offset);
    
    a = ((pixel & 0xf000) | ((pixel & 0xf000) >> 4)) << 16;
    b = ((pixel & 0x0f00) | ((pixel & 0x0f00) >> 4)) >> 4;
    g = ((pixel & 0x00f0) | ((pixel & 0x00f0) >> 4)) << 8;
    r = ((pixel & 0x000f) | ((pixel & 0x000f) << 4)) << 16;
    return (a | r | g | b);
}

static FASTCALL uint32_t
fbFetchPixel_x4b4g4r4 (pixman_image_t *image,
		       const uint32_t *bits, int offset, const pixman_indexed_t * indexed)
{
    uint32_t  r,g,b;
    uint32_t  pixel = READ((uint16_t *) bits + offset);
    
    b = ((pixel & 0x0f00) | ((pixel & 0x0f00) >> 4)) >> 4;
    g = ((pixel & 0x00f0) | ((pixel & 0x00f0) >> 4)) << 8;
    r = ((pixel & 0x000f) | ((pixel & 0x000f) << 4)) << 16;
    return (0xff000000 | r | g | b);
}

static FASTCALL uint32_t
fbFetchPixel_a8 (pixman_image_t *image,
		 const uint32_t *bits, int offset, const pixman_indexed_t * indexed)
{
    uint32_t   pixel = READ((uint8_t *) bits + offset);
    
    return pixel << 24;
}

static FASTCALL uint32_t
fbFetchPixel_r3g3b2 (pixman_image_t *image,
		     const uint32_t *bits, int offset, const pixman_indexed_t * indexed)
{
    uint32_t  r,g,b;
    uint32_t   pixel = READ((uint8_t *) bits + offset);
    
    r = ((pixel & 0xe0) | ((pixel & 0xe0) >> 3) | ((pixel & 0xc0) >> 6)) << 16;
    g = ((pixel & 0x1c) | ((pixel & 0x18) >> 3) | ((pixel & 0x1c) << 3)) << 8;
    b = (((pixel & 0x03)     ) |
	 ((pixel & 0x03) << 2) |
	 ((pixel & 0x03) << 4) |
	 ((pixel & 0x03) << 6));
    return (0xff000000 | r | g | b);
}

static FASTCALL uint32_t
fbFetchPixel_b2g3r3 (pixman_image_t *image,
		     const uint32_t *bits, int offset, const pixman_indexed_t * indexed)
{
    uint32_t  r,g,b;
    uint32_t   pixel = READ((uint8_t *) bits + offset);
    
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
fbFetchPixel_a2r2g2b2 (pixman_image_t *image,
		       const uint32_t *bits, int offset, const pixman_indexed_t * indexed)
{
    uint32_t   a,r,g,b;
    uint32_t   pixel = READ((uint8_t *) bits + offset);
    
    a = ((pixel & 0xc0) * 0x55) << 18;
    r = ((pixel & 0x30) * 0x55) << 12;
    g = ((pixel & 0x0c) * 0x55) << 6;
    b = ((pixel & 0x03) * 0x55);
    return a|r|g|b;
}

static FASTCALL uint32_t
fbFetchPixel_a2b2g2r2 (pixman_image_t *image,
		       const uint32_t *bits, int offset, const pixman_indexed_t * indexed)
{
    uint32_t   a,r,g,b;
    uint32_t   pixel = READ((uint8_t *) bits + offset);
    
    a = ((pixel & 0xc0) * 0x55) << 18;
    b = ((pixel & 0x30) * 0x55) >> 6;
    g = ((pixel & 0x0c) * 0x55) << 6;
    r = ((pixel & 0x03) * 0x55) << 16;
    return a|r|g|b;
}

static FASTCALL uint32_t
fbFetchPixel_c8 (pixman_image_t *image,
		 const uint32_t *bits, int offset, const pixman_indexed_t * indexed)
{
    uint32_t   pixel = READ((uint8_t *) bits + offset);
    return indexed->rgba[pixel];
}

static FASTCALL uint32_t
fbFetchPixel_x4a4 (pixman_image_t *image,
		   const uint32_t *bits, int offset, const pixman_indexed_t * indexed)
{
    uint32_t   pixel = READ((uint8_t *) bits + offset);
    
    return ((pixel & 0xf) | ((pixel & 0xf) << 4)) << 24;
}

static FASTCALL uint32_t
fbFetchPixel_a4 (pixman_image_t *image,
		 const uint32_t *bits, int offset, const pixman_indexed_t * indexed)
{
    uint32_t  pixel = Fetch4(bits, offset);
    
    pixel |= pixel << 4;
    return pixel << 24;
}

static FASTCALL uint32_t
fbFetchPixel_r1g2b1 (pixman_image_t *image,
		     const uint32_t *bits, int offset, const pixman_indexed_t * indexed)
{
    uint32_t  r,g,b;
    uint32_t  pixel = Fetch4(bits, offset);
    
    r = ((pixel & 0x8) * 0xff) << 13;
    g = ((pixel & 0x6) * 0x55) << 7;
    b = ((pixel & 0x1) * 0xff);
    return 0xff000000|r|g|b;
}

static FASTCALL uint32_t
fbFetchPixel_b1g2r1 (pixman_image_t *image,
		     const uint32_t *bits, int offset, const pixman_indexed_t * indexed)
{
    uint32_t  r,g,b;
    uint32_t  pixel = Fetch4(bits, offset);
    
    b = ((pixel & 0x8) * 0xff) >> 3;
    g = ((pixel & 0x6) * 0x55) << 7;
    r = ((pixel & 0x1) * 0xff) << 16;
    return 0xff000000|r|g|b;
}

static FASTCALL uint32_t
fbFetchPixel_a1r1g1b1 (pixman_image_t *image,
		       const uint32_t *bits, int offset, const pixman_indexed_t * indexed)
{
    uint32_t  a,r,g,b;
    uint32_t  pixel = Fetch4(bits, offset);
    
    a = ((pixel & 0x8) * 0xff) << 21;
    r = ((pixel & 0x4) * 0xff) << 14;
    g = ((pixel & 0x2) * 0xff) << 7;
    b = ((pixel & 0x1) * 0xff);
    return a|r|g|b;
}

static FASTCALL uint32_t
fbFetchPixel_a1b1g1r1 (pixman_image_t *image,
		       const uint32_t *bits, int offset, const pixman_indexed_t * indexed)
{
    uint32_t  a,r,g,b;
    uint32_t  pixel = Fetch4(bits, offset);
    
    a = ((pixel & 0x8) * 0xff) << 21;
    r = ((pixel & 0x4) * 0xff) >> 3;
    g = ((pixel & 0x2) * 0xff) << 7;
    b = ((pixel & 0x1) * 0xff) << 16;
    return a|r|g|b;
}

static FASTCALL uint32_t
fbFetchPixel_c4 (pixman_image_t *image,
		 const uint32_t *bits, int offset, const pixman_indexed_t * indexed)
{
    uint32_t  pixel = Fetch4(bits, offset);
    
    return indexed->rgba[pixel];
}


static FASTCALL uint32_t
fbFetchPixel_a1 (pixman_image_t *image,
		 const uint32_t *bits, int offset, const pixman_indexed_t * indexed)
{
    uint32_t  pixel = READ(bits + (offset >> 5));
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
fbFetchPixel_g1 (pixman_image_t *image,
		 const uint32_t *bits, int offset, const pixman_indexed_t * indexed)
{
    uint32_t pixel = READ(bits + (offset >> 5));
    uint32_t a;
#if BITMAP_BIT_ORDER == MSBFirst
    a = pixel >> (0x1f - (offset & 0x1f));
#else
    a = pixel >> (offset & 0x1f);
#endif
    a = a & 1;
    return indexed->rgba[a];
}

static fetchPixelProc fetchPixelProcForPicture (bits_image_t * pict)
{
    switch(pict->format) {
    case PIXMAN_a8r8g8b8: return fbFetchPixel_a8r8g8b8;
    case PIXMAN_x8r8g8b8: return fbFetchPixel_x8r8g8b8;
    case PIXMAN_a8b8g8r8: return fbFetchPixel_a8b8g8r8;
    case PIXMAN_x8b8g8r8: return fbFetchPixel_x8b8g8r8;
	
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
    }
    
    return NULL;
}



/*
 * All the store functions
 */

typedef FASTCALL void (*storeProc) (pixman_image_t *image,
				    uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed);

#define Splita(v)	uint32_t	a = ((v) >> 24), r = ((v) >> 16) & 0xff, g = ((v) >> 8) & 0xff, b = (v) & 0xff
#define Split(v)	uint32_t	r = ((v) >> 16) & 0xff, g = ((v) >> 8) & 0xff, b = (v) & 0xff

static FASTCALL void
fbStore_a8r8g8b8 (pixman_image_t *image,
		  uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    MEMCPY_WRAPPED(((uint32_t *)bits) + x, values, width*sizeof(uint32_t));
}

static FASTCALL void
fbStore_x8r8g8b8 (pixman_image_t *image,
		  uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    uint32_t *pixel = (uint32_t *)bits + x;
    for (i = 0; i < width; ++i)
	WRITE(pixel++, values[i] & 0xffffff);
}

static FASTCALL void
fbStore_a8b8g8r8 (pixman_image_t *image,
		  uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    uint32_t *pixel = (uint32_t *)bits + x;
    for (i = 0; i < width; ++i)
	WRITE(pixel++, (values[i] & 0xff00ff00) | ((values[i] >> 16) & 0xff) | ((values[i] & 0xff) << 16));
}

static FASTCALL void
fbStore_x8b8g8r8 (pixman_image_t *image,
		  uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    uint32_t *pixel = (uint32_t *)bits + x;
    for (i = 0; i < width; ++i)
	WRITE(pixel++, (values[i] & 0x0000ff00) | ((values[i] >> 16) & 0xff) | ((values[i] & 0xff) << 16));
}

static FASTCALL void
fbStore_r8g8b8 (pixman_image_t *image,
		uint32_t *bits, const uint32_t *values, int x, int width,
		const pixman_indexed_t * indexed)
{
    int i;
    uint8_t *pixel = ((uint8_t *) bits) + 3*x;
    for (i = 0; i < width; ++i) {
	Store24(pixel, values[i]);
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
	WRITE(pixel++, Blue(val));
	WRITE(pixel++, Green(val));
	WRITE(pixel++, Red(val));
#else
	WRITE(pixel++, Red(val));
	WRITE(pixel++, Green(val));
	WRITE(pixel++, Blue(val));
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
	WRITE(pixel++, ((s >> 3) & 0x001f) |
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
	WRITE(pixel++, ((b << 8) & 0xf800) |
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
	WRITE(pixel++, ((a << 8) & 0x8000) |
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
	WRITE(pixel++, ((r << 7) & 0x7c00) |
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
	WRITE(pixel++, ((a << 8) & 0x8000) |
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
	WRITE(pixel++, ((b << 7) & 0x7c00) |
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
	WRITE(pixel++, ((a << 8) & 0xf000) |
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
	WRITE(pixel++, ((r << 4) & 0x0f00) |
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
	WRITE(pixel++, ((a << 8) & 0xf000) |
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
	WRITE(pixel++, ((b << 4) & 0x0f00) |
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
	WRITE(pixel++, values[i] >> 24);
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
	WRITE(pixel++,
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
	WRITE(pixel++,
	      ((b     ) & 0xc0) |
	      ((g >> 2) & 0x1c) |
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
	WRITE(pixel++, ((a     ) & 0xc0) |
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
	WRITE(pixel++, miIndexToEnt24(indexed,values[i]));
    }
}

static FASTCALL void
fbStore_x4a4 (pixman_image_t *image,
	      uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    uint8_t   *pixel = ((uint8_t *) bits) + x;
    for (i = 0; i < width; ++i) {
	WRITE(pixel++, values[i] >> 28);
    }
}

#define Store8(l,o,v)  (WRITE((uint8_t *)(l) + ((o) >> 3), (v)))
#if IMAGE_BYTE_ORDER == MSBFirst
#define Store4(l,o,v)  Store8(l,o,((o) & 4 ?				\
				   (Fetch8(l,o) & 0xf0) | (v) :		\
				   (Fetch8(l,o) & 0x0f) | ((v) << 4)))
#else
#define Store4(l,o,v)  Store8(l,o,((o) & 4 ?			       \
				   (Fetch8(l,o) & 0x0f) | ((v) << 4) : \
				   (Fetch8(l,o) & 0xf0) | (v)))
#endif

static FASTCALL void
fbStore_a4 (pixman_image_t *image,
	    uint32_t *bits, const uint32_t *values, int x, int width, const pixman_indexed_t * indexed)
{
    int i;
    for (i = 0; i < width; ++i) {
	Store4(bits, i + x, values[i]>>28);
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
	Store4(bits, i + x, pixel);
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
	Store4(bits, i + x, pixel);
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
	Store4(bits, i + x, pixel);
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
	Store4(bits, i + x, pixel);
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
	Store4(bits, i + x, pixel);
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
	WRITE(pixel, (READ(pixel) & ~mask) | v);
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
	WRITE(pixel, (READ(pixel) & ~mask) | v);
    }
}


static storeProc storeProcForPicture (bits_image_t * pict)
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
 * Combine src and mask
 */
static FASTCALL void
pixman_fbCombineMaskU (uint32_t *src, const uint32_t *mask, int width)
{
    int i;
    for (i = 0; i < width; ++i) {
        uint32_t a = *(mask + i) >> 24;
        uint32_t s = *(src + i);
        FbByteMul(s, a);
        *(src + i) = s;
    }
}

/*
 * All of the composing functions
 */

static FASTCALL void
fbCombineClear (uint32_t *dest, const uint32_t *src, int width)
{
    memset(dest, 0, width*sizeof(uint32_t));
}

static FASTCALL void
fbCombineSrcU (uint32_t *dest, const uint32_t *src, int width)
{
    memcpy(dest, src, width*sizeof(uint32_t));
}


static FASTCALL void
fbCombineOverU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    for (i = 0; i < width; ++i) {
        uint32_t s = *(src + i);
        uint32_t d = *(dest + i);
        uint32_t ia = Alpha(~s);
	
        FbByteMulAdd(d, ia, s);
	*(dest + i) = d;
    }
}

static FASTCALL void
fbCombineOverReverseU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    for (i = 0; i < width; ++i) {
        uint32_t s = *(src + i);
        uint32_t d = *(dest + i);
        uint32_t ia = Alpha(~*(dest + i));
        FbByteMulAdd(s, ia, d);
	*(dest + i) = s;
    }
}

static FASTCALL void
fbCombineInU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    for (i = 0; i < width; ++i) {
        uint32_t s = *(src + i);
        uint32_t a = Alpha(*(dest + i));
        FbByteMul(s, a);
	*(dest + i) = s;
    }
}

static FASTCALL void
fbCombineInReverseU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    for (i = 0; i < width; ++i) {
        uint32_t d = *(dest + i);
        uint32_t a = Alpha(*(src + i));
        FbByteMul(d, a);
	*(dest + i) = d;
    }
}

static FASTCALL void
fbCombineOutU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    for (i = 0; i < width; ++i) {
        uint32_t s = *(src + i);
        uint32_t a = Alpha(~*(dest + i));
        FbByteMul(s, a);
	*(dest + i) = s;
    }
}

static FASTCALL void
fbCombineOutReverseU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    for (i = 0; i < width; ++i) {
        uint32_t d = *(dest + i);
        uint32_t a = Alpha(~*(src + i));
        FbByteMul(d, a);
	*(dest + i) = d;
    }
}

static FASTCALL void
fbCombineAtopU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    for (i = 0; i < width; ++i) {
        uint32_t s = *(src + i);
        uint32_t d = *(dest + i);
        uint32_t dest_a = Alpha(d);
        uint32_t src_ia = Alpha(~s);
	
        FbByteAddMul(s, dest_a, d, src_ia);
	*(dest + i) = s;
    }
}

static FASTCALL void
fbCombineAtopReverseU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    for (i = 0; i < width; ++i) {
        uint32_t s = *(src + i);
        uint32_t d = *(dest + i);
        uint32_t src_a = Alpha(s);
        uint32_t dest_ia = Alpha(~d);
	
        FbByteAddMul(s, dest_ia, d, src_a);
	*(dest + i) = s;
    }
}

static FASTCALL void
fbCombineXorU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    for (i = 0; i < width; ++i) {
        uint32_t s = *(src + i);
        uint32_t d = *(dest + i);
        uint32_t src_ia = Alpha(~s);
        uint32_t dest_ia = Alpha(~d);
	
        FbByteAddMul(s, dest_ia, d, src_ia);
	*(dest + i) = s;
    }
}

static FASTCALL void
fbCombineAddU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    for (i = 0; i < width; ++i) {
        uint32_t s = *(src + i);
        uint32_t d = *(dest + i);
        FbByteAdd(d, s);
	*(dest + i) = d;
    }
}

static FASTCALL void
fbCombineSaturateU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    for (i = 0; i < width; ++i) {
        uint32_t  s = *(src + i);
        uint32_t d = *(dest + i);
        uint16_t  sa, da;
	
        sa = s >> 24;
        da = ~d >> 24;
        if (sa > da)
        {
            sa = FbIntDiv(da, sa);
            FbByteMul(s, sa);
        };
        FbByteAdd(d, s);
	*(dest + i) = d;
    }
}

/*
 * All of the disjoint composing functions
 
 The four entries in the first column indicate what source contributions
 come from each of the four areas of the picture -- areas covered by neither
 A nor B, areas covered only by A, areas covered only by B and finally
 areas covered by both A and B.
 
 Disjoint			Conjoint
 Fa		Fb		Fa		Fb
 (0,0,0,0)	0		0		0		0
 (0,A,0,A)	1		0		1		0
 (0,0,B,B)	0		1		0		1
 (0,A,B,A)	1		min((1-a)/b,1)	1		max(1-a/b,0)
 (0,A,B,B)	min((1-b)/a,1)	1		max(1-b/a,0)	1
 (0,0,0,A)	max(1-(1-b)/a,0) 0		min(1,b/a)	0
 (0,0,0,B)	0		max(1-(1-a)/b,0) 0		min(a/b,1)
 (0,A,0,0)	min(1,(1-b)/a)	0		max(1-b/a,0)	0
 (0,0,B,0)	0		min(1,(1-a)/b)	0		max(1-a/b,0)
 (0,0,B,A)	max(1-(1-b)/a,0) min(1,(1-a)/b)	 min(1,b/a)	max(1-a/b,0)
 (0,A,0,B)	min(1,(1-b)/a)	max(1-(1-a)/b,0) max(1-b/a,0)	min(1,a/b)
 (0,A,B,0)	min(1,(1-b)/a)	min(1,(1-a)/b)	max(1-b/a,0)	max(1-a/b,0)
 
*/

#define CombineAOut 1
#define CombineAIn  2
#define CombineBOut 4
#define CombineBIn  8

#define CombineClear	0
#define CombineA	(CombineAOut|CombineAIn)
#define CombineB	(CombineBOut|CombineBIn)
#define CombineAOver	(CombineAOut|CombineBOut|CombineAIn)
#define CombineBOver	(CombineAOut|CombineBOut|CombineBIn)
#define CombineAAtop	(CombineBOut|CombineAIn)
#define CombineBAtop	(CombineAOut|CombineBIn)
#define CombineXor	(CombineAOut|CombineBOut)

/* portion covered by a but not b */
static INLINE uint8_t
fbCombineDisjointOutPart (uint8_t a, uint8_t b)
{
    /* min (1, (1-b) / a) */
    
    b = ~b;		    /* 1 - b */
    if (b >= a)		    /* 1 - b >= a -> (1-b)/a >= 1 */
	return 0xff;	    /* 1 */
    return FbIntDiv(b,a);   /* (1-b) / a */
}

/* portion covered by both a and b */
static INLINE uint8_t
fbCombineDisjointInPart (uint8_t a, uint8_t b)
{
    /* max (1-(1-b)/a,0) */
    /*  = - min ((1-b)/a - 1, 0) */
    /*  = 1 - min (1, (1-b)/a) */
    
    b = ~b;		    /* 1 - b */
    if (b >= a)		    /* 1 - b >= a -> (1-b)/a >= 1 */
	return 0;	    /* 1 - 1 */
    return ~FbIntDiv(b,a);  /* 1 - (1-b) / a */
}

static FASTCALL void
fbCombineDisjointGeneralU (uint32_t *dest, const uint32_t *src, int width, uint8_t combine)
{
    int i;
    for (i = 0; i < width; ++i) {
        uint32_t s = *(src + i);
        uint32_t d = *(dest + i);
        uint32_t m,n,o,p;
        uint16_t Fa, Fb, t, u, v;
        uint8_t sa = s >> 24;
        uint8_t da = d >> 24;
	
        switch (combine & CombineA) {
        default:
            Fa = 0;
            break;
        case CombineAOut:
            Fa = fbCombineDisjointOutPart (sa, da);
            break;
        case CombineAIn:
            Fa = fbCombineDisjointInPart (sa, da);
            break;
        case CombineA:
            Fa = 0xff;
            break;
        }
	
        switch (combine & CombineB) {
        default:
            Fb = 0;
            break;
        case CombineBOut:
            Fb = fbCombineDisjointOutPart (da, sa);
            break;
        case CombineBIn:
            Fb = fbCombineDisjointInPart (da, sa);
            break;
        case CombineB:
            Fb = 0xff;
            break;
        }
        m = FbGen (s,d,0,Fa,Fb,t, u, v);
        n = FbGen (s,d,8,Fa,Fb,t, u, v);
        o = FbGen (s,d,16,Fa,Fb,t, u, v);
        p = FbGen (s,d,24,Fa,Fb,t, u, v);
        s = m|n|o|p;
	*(dest + i) = s;
    }
}

static FASTCALL void
fbCombineDisjointOverU (uint32_t *dest, const uint32_t *src, int width)
{
    int i;
    for (i = 0; i < width; ++i) {
        uint32_t  s = *(src + i);
        uint16_t  a = s >> 24;
	
        if (a != 0x00)
        {
            if (a != 0xff)
            {
                uint32_t d = *(dest + i);
                a = fbCombineDisjointOutPart (d >> 24, a);
                FbByteMulAdd(d, a, s);
                s = d;
            }
	    *(dest + i) = s;
        }
    }
}

static FASTCALL void
fbCombineDisjointInU (uint32_t *dest, const uint32_t *src, int width)
{
    fbCombineDisjointGeneralU (dest, src, width, CombineAIn);
}

static FASTCALL void
fbCombineDisjointInReverseU (uint32_t *dest, const uint32_t *src, int width)
{
    fbCombineDisjointGeneralU (dest, src, width, CombineBIn);
}

static FASTCALL void
fbCombineDisjointOutU (uint32_t *dest, const uint32_t *src, int width)
{
    fbCombineDisjointGeneralU (dest, src, width, CombineAOut);
}

static FASTCALL void
fbCombineDisjointOutReverseU (uint32_t *dest, const uint32_t *src, int width)
{
    fbCombineDisjointGeneralU (dest, src, width, CombineBOut);
}

static FASTCALL void
fbCombineDisjointAtopU (uint32_t *dest, const uint32_t *src, int width)
{
    fbCombineDisjointGeneralU (dest, src, width, CombineAAtop);
}

static FASTCALL void
fbCombineDisjointAtopReverseU (uint32_t *dest, const uint32_t *src, int width)
{
    fbCombineDisjointGeneralU (dest, src, width, CombineBAtop);
}

static FASTCALL void
fbCombineDisjointXorU (uint32_t *dest, const uint32_t *src, int width)
{
    fbCombineDisjointGeneralU (dest, src, width, CombineXor);
}

/* portion covered by a but not b */
static INLINE uint8_t
fbCombineConjointOutPart (uint8_t a, uint8_t b)
{
    /* max (1-b/a,0) */
    /* = 1-min(b/a,1) */
    
    /* min (1, (1-b) / a) */
    
    if (b >= a)		    /* b >= a -> b/a >= 1 */
	return 0x00;	    /* 0 */
    return ~FbIntDiv(b,a);   /* 1 - b/a */
}

/* portion covered by both a and b */
static INLINE uint8_t
fbCombineConjointInPart (uint8_t a, uint8_t b)
{
    /* min (1,b/a) */
    
    if (b >= a)		    /* b >= a -> b/a >= 1 */
	return 0xff;	    /* 1 */
    return FbIntDiv(b,a);   /* b/a */
}

static FASTCALL void
fbCombineConjointGeneralU (uint32_t *dest, const uint32_t *src, int width, uint8_t combine)
{
    int i;
    for (i = 0; i < width; ++i) {
        uint32_t  s = *(src + i);
        uint32_t d = *(dest + i);
        uint32_t  m,n,o,p;
        uint16_t  Fa, Fb, t, u, v;
        uint8_t sa = s >> 24;
        uint8_t da = d >> 24;
	
        switch (combine & CombineA) {
        default:
            Fa = 0;
            break;
        case CombineAOut:
            Fa = fbCombineConjointOutPart (sa, da);
            break;
        case CombineAIn:
            Fa = fbCombineConjointInPart (sa, da);
            break;
        case CombineA:
            Fa = 0xff;
            break;
        }
	
        switch (combine & CombineB) {
        default:
            Fb = 0;
            break;
        case CombineBOut:
            Fb = fbCombineConjointOutPart (da, sa);
            break;
        case CombineBIn:
            Fb = fbCombineConjointInPart (da, sa);
            break;
        case CombineB:
            Fb = 0xff;
            break;
        }
        m = FbGen (s,d,0,Fa,Fb,t, u, v);
        n = FbGen (s,d,8,Fa,Fb,t, u, v);
        o = FbGen (s,d,16,Fa,Fb,t, u, v);
        p = FbGen (s,d,24,Fa,Fb,t, u, v);
        s = m|n|o|p;
	*(dest + i) = s;
    }
}

static FASTCALL void
fbCombineConjointOverU (uint32_t *dest, const uint32_t *src, int width)
{
    fbCombineConjointGeneralU (dest, src, width, CombineAOver);
}


static FASTCALL void
fbCombineConjointOverReverseU (uint32_t *dest, const uint32_t *src, int width)
{
    fbCombineConjointGeneralU (dest, src, width, CombineBOver);
}


static FASTCALL void
fbCombineConjointInU (uint32_t *dest, const uint32_t *src, int width)
{
    fbCombineConjointGeneralU (dest, src, width, CombineAIn);
}


static FASTCALL void
fbCombineConjointInReverseU (uint32_t *dest, const uint32_t *src, int width)
{
    fbCombineConjointGeneralU (dest, src, width, CombineBIn);
}

static FASTCALL void
fbCombineConjointOutU (uint32_t *dest, const uint32_t *src, int width)
{
    fbCombineConjointGeneralU (dest, src, width, CombineAOut);
}

static FASTCALL void
fbCombineConjointOutReverseU (uint32_t *dest, const uint32_t *src, int width)
{
    fbCombineConjointGeneralU (dest, src, width, CombineBOut);
}

static FASTCALL void
fbCombineConjointAtopU (uint32_t *dest, const uint32_t *src, int width)
{
    fbCombineConjointGeneralU (dest, src, width, CombineAAtop);
}

static FASTCALL void
fbCombineConjointAtopReverseU (uint32_t *dest, const uint32_t *src, int width)
{
    fbCombineConjointGeneralU (dest, src, width, CombineBAtop);
}

static FASTCALL void
fbCombineConjointXorU (uint32_t *dest, const uint32_t *src, int width)
{
    fbCombineConjointGeneralU (dest, src, width, CombineXor);
}

static CombineFuncU pixman_fbCombineFuncU[] = {
    fbCombineClear,
    fbCombineSrcU,
    NULL, /* CombineDst */
    fbCombineOverU,
    fbCombineOverReverseU,
    fbCombineInU,
    fbCombineInReverseU,
    fbCombineOutU,
    fbCombineOutReverseU,
    fbCombineAtopU,
    fbCombineAtopReverseU,
    fbCombineXorU,
    fbCombineAddU,
    fbCombineSaturateU,
    NULL,
    NULL,
    fbCombineClear,
    fbCombineSrcU,
    NULL, /* CombineDst */
    fbCombineDisjointOverU,
    fbCombineSaturateU, /* DisjointOverReverse */
    fbCombineDisjointInU,
    fbCombineDisjointInReverseU,
    fbCombineDisjointOutU,
    fbCombineDisjointOutReverseU,
    fbCombineDisjointAtopU,
    fbCombineDisjointAtopReverseU,
    fbCombineDisjointXorU,
    NULL,
    NULL,
    NULL,
    NULL,
    fbCombineClear,
    fbCombineSrcU,
    NULL, /* CombineDst */
    fbCombineConjointOverU,
    fbCombineConjointOverReverseU,
    fbCombineConjointInU,
    fbCombineConjointInReverseU,
    fbCombineConjointOutU,
    fbCombineConjointOutReverseU,
    fbCombineConjointAtopU,
    fbCombineConjointAtopReverseU,
    fbCombineConjointXorU,
};

static INLINE void
fbCombineMaskC (uint32_t *src, uint32_t *mask)
{
    uint32_t a = *mask;
    
    uint32_t	x;
    uint16_t	xa;
    
    if (!a)
    {
	*(src) = 0;
	return;
    }
    
    x = *(src);
    if (a == 0xffffffff)
    {
	x = x >> 24;
	x |= x << 8;
	x |= x << 16;
	*(mask) = x;
	return;
    }
    
    xa = x >> 24;
    FbByteMulC(x, a);
    *(src) = x;
    FbByteMul(a, xa);
    *(mask) = a;
}

static INLINE void
fbCombineMaskValueC (uint32_t *src, const uint32_t *mask)
{
    uint32_t a = *mask;
    uint32_t	x;
    
    if (!a)
    {
	*(src) = 0;
	return;
    }
    
    if (a == 0xffffffff)
	return;
    
    x = *(src);
    FbByteMulC(x, a);
    *(src) =x;
}

static INLINE void
fbCombineMaskAlphaC (const uint32_t *src, uint32_t *mask)
{
    uint32_t a = *(mask);
    uint32_t	x;
    
    if (!a)
	return;
    
    x = *(src) >> 24;
    if (x == 0xff)
	return;
    if (a == 0xffffffff)
    {
	x = x >> 24;
	x |= x << 8;
	x |= x << 16;
	*(mask) = x;
	return;
    }
    
    FbByteMul(a, x);
    *(mask) = a;
}

static FASTCALL void
fbCombineClearC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    memset(dest, 0, width*sizeof(uint32_t));
}

static FASTCALL void
fbCombineSrcC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;
    
    for (i = 0; i < width; ++i) {
	uint32_t s = *(src + i);
	uint32_t m = *(mask + i);
	
	fbCombineMaskValueC (&s, &m);
	
	*(dest) = s;
    }
}

static FASTCALL void
fbCombineOverC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;
    
    for (i = 0; i < width; ++i) {
	uint32_t s = *(src + i);
	uint32_t m = *(mask + i);
	uint32_t a;
	
	fbCombineMaskC (&s, &m);
	
	a = ~m;
        if (a != 0xffffffff)
        {
            if (a)
            {
                uint32_t d = *(dest + i);
                FbByteMulAddC(d, a, s);
                s = d;
            }
	    *(dest + i) = s;
        }
    }
}

static FASTCALL void
fbCombineOverReverseC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;
    
    for (i = 0; i < width; ++i) {
        uint32_t d = *(dest + i);
        uint32_t a = ~d >> 24;
	
        if (a)
        {
            uint32_t s = *(src + i);
	    uint32_t m = *(mask + i);
	    
	    fbCombineMaskValueC (&s, &m);
	    
            if (a != 0xff)
            {
                FbByteMulAdd(s, a, d);
            }
	    *(dest + i) = s;
        }
    }
}

static FASTCALL void
fbCombineInC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;
    
    for (i = 0; i < width; ++i) {
        uint32_t d = *(dest + i);
        uint16_t a = d >> 24;
        uint32_t s = 0;
        if (a)
        {
	    uint32_t m = *(mask + i);
	    
	    s = *(src + i);
	    fbCombineMaskValueC (&s, &m);
            if (a != 0xff)
            {
                FbByteMul(s, a);
            }
        }
	*(dest + i) = s;
    }
}

static FASTCALL void
fbCombineInReverseC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;
    
    for (i = 0; i < width; ++i) {
        uint32_t s = *(src + i);
        uint32_t m = *(mask + i);
        uint32_t a;
	
	fbCombineMaskAlphaC (&s, &m);
	
	a = m;
        if (a != 0xffffffff)
        {
            uint32_t d = 0;
            if (a)
            {
                d = *(dest + i);
                FbByteMulC(d, a);
            }
	    *(dest + i) = d; 
        }
    }
}

static FASTCALL void
fbCombineOutC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;
    
    for (i = 0; i < width; ++i) {
        uint32_t d = *(dest + i);
        uint16_t a = ~d >> 24;
        uint32_t s = 0;
        if (a)
        {
	    uint32_t m = *(mask + i);
	    
	    s = *(src + i);
	    fbCombineMaskValueC (&s, &m);
	    
            if (a != 0xff)
            {
                FbByteMul(s, a);
            }
        }
	*(dest + i) = s;
    }
}

static FASTCALL void
fbCombineOutReverseC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;
    
    for (i = 0; i < width; ++i) {
	uint32_t s = *(src + i);
	uint32_t m = *(mask + i);
	uint32_t a;
	
	fbCombineMaskAlphaC (&s, &m);
	
        a = ~m;
        if (a != 0xffffffff)
        {
            uint32_t d = 0;
            if (a)
            {
                d = *(dest + i);
                FbByteMulC(d, a);
            }
	    *(dest + i) = d;
        }
    }
}

static FASTCALL void
fbCombineAtopC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;
    
    for (i = 0; i < width; ++i) {
        uint32_t d = *(dest + i);
        uint32_t s = *(src + i);
        uint32_t m = *(mask + i);
        uint32_t ad;
        uint16_t as = d >> 24;
	
	fbCombineMaskC (&s, &m);
	
        ad = ~m;
	
        FbByteAddMulC(d, ad, s, as);
	*(dest + i) = d;
    }
}

static FASTCALL void
fbCombineAtopReverseC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;
    
    for (i = 0; i < width; ++i) {
	
        uint32_t d = *(dest + i);
        uint32_t s = *(src + i);
        uint32_t m = *(mask + i);
        uint32_t ad;
        uint16_t as = ~d >> 24;
	
	fbCombineMaskC (&s, &m);
	
	ad = m;
	
        FbByteAddMulC(d, ad, s, as);
	*(dest + i) = d;
    }
}

static FASTCALL void
fbCombineXorC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;
    
    for (i = 0; i < width; ++i) {
        uint32_t d = *(dest + i);
        uint32_t s = *(src + i);
        uint32_t m = *(mask + i);
        uint32_t ad;
        uint16_t as = ~d >> 24;
	
	fbCombineMaskC (&s, &m);
	
	ad = ~m;
	
        FbByteAddMulC(d, ad, s, as);
	*(dest + i) = d;
    }
}

static FASTCALL void
fbCombineAddC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;
    
    for (i = 0; i < width; ++i) {
        uint32_t s = *(src + i);
        uint32_t m = *(mask + i);
        uint32_t d = *(dest + i);
	
	fbCombineMaskValueC (&s, &m);
	
        FbByteAdd(d, s);
	*(dest + i) = d;
    }
}

static FASTCALL void
fbCombineSaturateC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    int i;
    
    for (i = 0; i < width; ++i) {
        uint32_t  s, d;
        uint16_t  sa, sr, sg, sb, da;
        uint16_t  t, u, v;
        uint32_t  m,n,o,p;
	
        d = *(dest + i);
        s = *(src + i);
	m = *(mask + i);
	
	fbCombineMaskC (&s, &m);
	
        sa = (m >> 24);
        sr = (m >> 16) & 0xff;
        sg = (m >>  8) & 0xff;
        sb = (m      ) & 0xff;
        da = ~d >> 24;
	
        if (sb <= da)
            m = FbAdd(s,d,0,t);
        else
            m = FbGen (s, d, 0, (da << 8) / sb, 0xff, t, u, v);
	
        if (sg <= da)
            n = FbAdd(s,d,8,t);
        else
            n = FbGen (s, d, 8, (da << 8) / sg, 0xff, t, u, v);
	
        if (sr <= da)
            o = FbAdd(s,d,16,t);
        else
            o = FbGen (s, d, 16, (da << 8) / sr, 0xff, t, u, v);
	
        if (sa <= da)
            p = FbAdd(s,d,24,t);
        else
            p = FbGen (s, d, 24, (da << 8) / sa, 0xff, t, u, v);
	
	*(dest + i) = m|n|o|p;
    }
}

static FASTCALL void
fbCombineDisjointGeneralC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width, uint8_t combine)
{
    int i;
    
    for (i = 0; i < width; ++i) {
        uint32_t  s, d;
        uint32_t  m,n,o,p;
        uint32_t  Fa, Fb;
        uint16_t  t, u, v;
        uint32_t  sa;
        uint8_t   da;
	
        s = *(src + i);
        m = *(mask + i);
        d = *(dest + i);
        da = d >> 24;
	
	fbCombineMaskC (&s, &m);
	
	sa = m;
	
        switch (combine & CombineA) {
        default:
            Fa = 0;
            break;
        case CombineAOut:
            m = fbCombineDisjointOutPart ((uint8_t) (sa >> 0), da);
            n = fbCombineDisjointOutPart ((uint8_t) (sa >> 8), da) << 8;
            o = fbCombineDisjointOutPart ((uint8_t) (sa >> 16), da) << 16;
            p = fbCombineDisjointOutPart ((uint8_t) (sa >> 24), da) << 24;
            Fa = m|n|o|p;
            break;
        case CombineAIn:
            m = fbCombineDisjointInPart ((uint8_t) (sa >> 0), da);
            n = fbCombineDisjointInPart ((uint8_t) (sa >> 8), da) << 8;
            o = fbCombineDisjointInPart ((uint8_t) (sa >> 16), da) << 16;
            p = fbCombineDisjointInPart ((uint8_t) (sa >> 24), da) << 24;
            Fa = m|n|o|p;
            break;
        case CombineA:
            Fa = 0xffffffff;
            break;
        }
	
        switch (combine & CombineB) {
        default:
            Fb = 0;
            break;
        case CombineBOut:
            m = fbCombineDisjointOutPart (da, (uint8_t) (sa >> 0));
            n = fbCombineDisjointOutPart (da, (uint8_t) (sa >> 8)) << 8;
            o = fbCombineDisjointOutPart (da, (uint8_t) (sa >> 16)) << 16;
            p = fbCombineDisjointOutPart (da, (uint8_t) (sa >> 24)) << 24;
            Fb = m|n|o|p;
            break;
        case CombineBIn:
            m = fbCombineDisjointInPart (da, (uint8_t) (sa >> 0));
            n = fbCombineDisjointInPart (da, (uint8_t) (sa >> 8)) << 8;
            o = fbCombineDisjointInPart (da, (uint8_t) (sa >> 16)) << 16;
            p = fbCombineDisjointInPart (da, (uint8_t) (sa >> 24)) << 24;
            Fb = m|n|o|p;
            break;
        case CombineB:
            Fb = 0xffffffff;
            break;
        }
        m = FbGen (s,d,0,FbGet8(Fa,0),FbGet8(Fb,0),t, u, v);
        n = FbGen (s,d,8,FbGet8(Fa,8),FbGet8(Fb,8),t, u, v);
        o = FbGen (s,d,16,FbGet8(Fa,16),FbGet8(Fb,16),t, u, v);
        p = FbGen (s,d,24,FbGet8(Fa,24),FbGet8(Fb,24),t, u, v);
        s = m|n|o|p;
	*(dest + i) = s;
    }
}

static FASTCALL void
fbCombineDisjointOverC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineDisjointGeneralC (dest, src, mask, width, CombineAOver);
}

static FASTCALL void
fbCombineDisjointInC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineDisjointGeneralC (dest, src, mask, width, CombineAIn);
}

static FASTCALL void
fbCombineDisjointInReverseC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineDisjointGeneralC (dest, src, mask, width, CombineBIn);
}

static FASTCALL void
fbCombineDisjointOutC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineDisjointGeneralC (dest, src, mask, width, CombineAOut);
}

static FASTCALL void
fbCombineDisjointOutReverseC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineDisjointGeneralC (dest, src, mask, width, CombineBOut);
}

static FASTCALL void
fbCombineDisjointAtopC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineDisjointGeneralC (dest, src, mask, width, CombineAAtop);
}

static FASTCALL void
fbCombineDisjointAtopReverseC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineDisjointGeneralC (dest, src, mask, width, CombineBAtop);
}

static FASTCALL void
fbCombineDisjointXorC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineDisjointGeneralC (dest, src, mask, width, CombineXor);
}

static FASTCALL void
fbCombineConjointGeneralC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width, uint8_t combine)
{
    int i;
    
    for (i = 0; i < width; ++i) {
        uint32_t  s, d;
        uint32_t  m,n,o,p;
        uint32_t  Fa, Fb;
        uint16_t  t, u, v;
        uint32_t  sa;
        uint8_t   da;
	
        s = *(src + i);
        m = *(mask + i);
        d = *(dest + i);
        da = d >> 24;
	
	fbCombineMaskC (&s, &m);
	
        sa = m;
	
        switch (combine & CombineA) {
        default:
            Fa = 0;
            break;
        case CombineAOut:
            m = fbCombineConjointOutPart ((uint8_t) (sa >> 0), da);
            n = fbCombineConjointOutPart ((uint8_t) (sa >> 8), da) << 8;
            o = fbCombineConjointOutPart ((uint8_t) (sa >> 16), da) << 16;
            p = fbCombineConjointOutPart ((uint8_t) (sa >> 24), da) << 24;
            Fa = m|n|o|p;
            break;
        case CombineAIn:
            m = fbCombineConjointInPart ((uint8_t) (sa >> 0), da);
            n = fbCombineConjointInPart ((uint8_t) (sa >> 8), da) << 8;
            o = fbCombineConjointInPart ((uint8_t) (sa >> 16), da) << 16;
            p = fbCombineConjointInPart ((uint8_t) (sa >> 24), da) << 24;
            Fa = m|n|o|p;
            break;
        case CombineA:
            Fa = 0xffffffff;
            break;
        }
	
        switch (combine & CombineB) {
        default:
            Fb = 0;
            break;
        case CombineBOut:
            m = fbCombineConjointOutPart (da, (uint8_t) (sa >> 0));
            n = fbCombineConjointOutPart (da, (uint8_t) (sa >> 8)) << 8;
            o = fbCombineConjointOutPart (da, (uint8_t) (sa >> 16)) << 16;
            p = fbCombineConjointOutPart (da, (uint8_t) (sa >> 24)) << 24;
            Fb = m|n|o|p;
            break;
        case CombineBIn:
            m = fbCombineConjointInPart (da, (uint8_t) (sa >> 0));
            n = fbCombineConjointInPart (da, (uint8_t) (sa >> 8)) << 8;
            o = fbCombineConjointInPart (da, (uint8_t) (sa >> 16)) << 16;
            p = fbCombineConjointInPart (da, (uint8_t) (sa >> 24)) << 24;
            Fb = m|n|o|p;
            break;
        case CombineB:
            Fb = 0xffffffff;
            break;
        }
        m = FbGen (s,d,0,FbGet8(Fa,0),FbGet8(Fb,0),t, u, v);
        n = FbGen (s,d,8,FbGet8(Fa,8),FbGet8(Fb,8),t, u, v);
        o = FbGen (s,d,16,FbGet8(Fa,16),FbGet8(Fb,16),t, u, v);
        p = FbGen (s,d,24,FbGet8(Fa,24),FbGet8(Fb,24),t, u, v);
        s = m|n|o|p;
	*(dest + i) = s;
    }
}

static FASTCALL void
fbCombineConjointOverC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineConjointGeneralC (dest, src, mask, width, CombineAOver);
}

static FASTCALL void
fbCombineConjointOverReverseC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineConjointGeneralC (dest, src, mask, width, CombineBOver);
}

static FASTCALL void
fbCombineConjointInC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineConjointGeneralC (dest, src, mask, width, CombineAIn);
}

static FASTCALL void
fbCombineConjointInReverseC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineConjointGeneralC (dest, src, mask, width, CombineBIn);
}

static FASTCALL void
fbCombineConjointOutC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineConjointGeneralC (dest, src, mask, width, CombineAOut);
}

static FASTCALL void
fbCombineConjointOutReverseC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineConjointGeneralC (dest, src, mask, width, CombineBOut);
}

static FASTCALL void
fbCombineConjointAtopC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineConjointGeneralC (dest, src, mask, width, CombineAAtop);
}

static FASTCALL void
fbCombineConjointAtopReverseC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineConjointGeneralC (dest, src, mask, width, CombineBAtop);
}

static FASTCALL void
fbCombineConjointXorC (uint32_t *dest, uint32_t *src, uint32_t *mask, int width)
{
    fbCombineConjointGeneralC (dest, src, mask, width, CombineXor);
}

static CombineFuncC pixman_fbCombineFuncC[] = {
    fbCombineClearC,
    fbCombineSrcC,
    NULL, /* Dest */
    fbCombineOverC,
    fbCombineOverReverseC,
    fbCombineInC,
    fbCombineInReverseC,
    fbCombineOutC,
    fbCombineOutReverseC,
    fbCombineAtopC,
    fbCombineAtopReverseC,
    fbCombineXorC,
    fbCombineAddC,
    fbCombineSaturateC,
    NULL,
    NULL,
    fbCombineClearC,	    /* 0x10 */
    fbCombineSrcC,
    NULL, /* Dest */
    fbCombineDisjointOverC,
    fbCombineSaturateC, /* DisjointOverReverse */
    fbCombineDisjointInC,
    fbCombineDisjointInReverseC,
    fbCombineDisjointOutC,
    fbCombineDisjointOutReverseC,
    fbCombineDisjointAtopC,
    fbCombineDisjointAtopReverseC,
    fbCombineDisjointXorC,  /* 0x1b */
    NULL,
    NULL,
    NULL,
    NULL,
    fbCombineClearC,
    fbCombineSrcC,
    NULL, /* Dest */
    fbCombineConjointOverC,
    fbCombineConjointOverReverseC,
    fbCombineConjointInC,
    fbCombineConjointInReverseC,
    fbCombineConjointOutC,
    fbCombineConjointOutReverseC,
    fbCombineConjointAtopC,
    fbCombineConjointAtopReverseC,
    fbCombineConjointXorC,
};


static void fbFetchSolid(bits_image_t * pict, int x, int y, int width, uint32_t *buffer, uint32_t *mask, uint32_t maskBits)
{
    uint32_t *bits;
    uint32_t color;
    uint32_t *end;
    fetchPixelProc fetch = fetchPixelProcForPicture(pict);
    const pixman_indexed_t * indexed = pict->indexed;
    
    bits = pict->bits;
    
    color = fetch((pixman_image_t *)pict, bits, 0, indexed);
    
    end = buffer + width;
    while (buffer < end)
	*(buffer++) = color;
    fbFinishAccess (pict->pDrawable);
}

static void fbFetch(bits_image_t * pict, int x, int y, int width, uint32_t *buffer, uint32_t *mask, uint32_t maskBits)
{
    uint32_t *bits;
    uint32_t stride;
    fetchProc fetch = fetchProcForPicture(pict);
    const pixman_indexed_t * indexed = pict->indexed;
    
    bits = pict->bits;
    stride = pict->rowstride;
    
    bits += y*stride;
    
    fetch((pixman_image_t *)pict, bits, x, width, buffer, indexed);
}

#ifdef PIXMAN_FB_ACCESSORS
#define PIXMAN_COMPOSITE_RECT_GENERAL pixman_composite_rect_general_accessors
#define PIXMAN_COMPOSE_FUNCTIONS pixman_composeFunctions_accessors
#else
#define PIXMAN_COMPOSITE_RECT_GENERAL pixman_composite_rect_general_no_accessors
#define PIXMAN_COMPOSE_FUNCTIONS pixman_composeFunctions
#endif

#ifdef PIXMAN_FB_ACCESSORS	/* The accessor version can't be parameterized from outside */
static const
#endif
FbComposeFunctions PIXMAN_COMPOSE_FUNCTIONS = {
    pixman_fbCombineFuncU,
    pixman_fbCombineFuncC,
    pixman_fbCombineMaskU
};

typedef struct
{
    uint32_t        left_ag;
    uint32_t        left_rb;
    uint32_t        right_ag;
    uint32_t        right_rb;
    int32_t       left_x;
    int32_t       right_x;
    int32_t       stepper;
    
    pixman_gradient_stop_t	*stops;
    int                      num_stops;
    unsigned int             spread;
    
    int		  need_reset;
} GradientWalker;

static void
_gradient_walker_init (GradientWalker  *walker,
		       gradient_t      *gradient,
		       unsigned int     spread)
{
    walker->num_stops = gradient->n_stops;
    walker->stops     = gradient->stops;
    walker->left_x    = 0;
    walker->right_x   = 0x10000;
    walker->stepper   = 0;
    walker->left_ag   = 0;
    walker->left_rb   = 0;
    walker->right_ag  = 0;
    walker->right_rb  = 0;
    walker->spread    = spread;
    
    walker->need_reset = TRUE;
}

static void
_gradient_walker_reset (GradientWalker  *walker,
                        pixman_fixed_32_32_t     pos)
{
    int32_t                  x, left_x, right_x;
    pixman_color_t          *left_c, *right_c;
    int                      n, count = walker->num_stops;
    pixman_gradient_stop_t *      stops = walker->stops;
    
    static const pixman_color_t   transparent_black = { 0, 0, 0, 0 };
    
    switch (walker->spread)
    {
    case PIXMAN_REPEAT_NORMAL:
	x = (int32_t)pos & 0xFFFF;
	for (n = 0; n < count; n++)
	    if (x < stops[n].x)
		break;
	if (n == 0) {
	    left_x =  stops[count-1].x - 0x10000;
	    left_c = &stops[count-1].color;
	} else {
	    left_x =  stops[n-1].x;
	    left_c = &stops[n-1].color;
	}
	
	if (n == count) {
	    right_x =  stops[0].x + 0x10000;
	    right_c = &stops[0].color;
	} else {
	    right_x =  stops[n].x;
	    right_c = &stops[n].color;
	}
	left_x  += (pos - x);
	right_x += (pos - x);
	break;
	
    case PIXMAN_REPEAT_PAD:
	for (n = 0; n < count; n++)
	    if (pos < stops[n].x)
		break;
	
	if (n == 0) {
	    left_x =  INT32_MIN;
	    left_c = &stops[0].color;
	} else {
	    left_x =  stops[n-1].x;
	    left_c = &stops[n-1].color;
	}
	
	if (n == count) {
	    right_x =  INT32_MAX;
	    right_c = &stops[n-1].color;
	} else {
	    right_x =  stops[n].x;
	    right_c = &stops[n].color;
	}
	break;
	
    case PIXMAN_REPEAT_REFLECT:
	x = (int32_t)pos & 0xFFFF;
	if ((int32_t)pos & 0x10000)
	    x = 0x10000 - x;
	for (n = 0; n < count; n++)
	    if (x < stops[n].x)
		break;
	
	if (n == 0) {
	    left_x =  -stops[0].x;
	    left_c = &stops[0].color;
	} else {
	    left_x =  stops[n-1].x;
	    left_c = &stops[n-1].color;
	}
	
	if (n == count) {
	    right_x = 0x20000 - stops[n-1].x;
	    right_c = &stops[n-1].color;
	} else {
	    right_x =  stops[n].x;
	    right_c = &stops[n].color;
	}
	
	if ((int32_t)pos & 0x10000) {
	    pixman_color_t  *tmp_c;
	    int32_t          tmp_x;
	    
	    tmp_x   = 0x10000 - right_x;
	    right_x = 0x10000 - left_x;
	    left_x  = tmp_x;
	    
	    tmp_c   = right_c;
	    right_c = left_c;
	    left_c  = tmp_c;
	    
	    x = 0x10000 - x;
	}
	left_x  += (pos - x);
	right_x += (pos - x);
	break;
	
    default:  /* RepeatNone */
	for (n = 0; n < count; n++)
	    if (pos < stops[n].x)
		break;
	
	if (n == 0)
	{
	    left_x  =  INT32_MIN;
	    right_x =  stops[0].x;
	    left_c  = right_c = (pixman_color_t*) &transparent_black;
	}
	else if (n == count)
	{
	    left_x  = stops[n-1].x;
	    right_x = INT32_MAX;
	    left_c  = right_c = (pixman_color_t*) &transparent_black;
	}
	else
	{
	    left_x  =  stops[n-1].x;
	    right_x =  stops[n].x;
	    left_c  = &stops[n-1].color;
	    right_c = &stops[n].color;
	}
    }
    
    walker->left_x   = left_x;
    walker->right_x  = right_x;
    walker->left_ag  = ((left_c->alpha >> 8) << 16)   | (left_c->green >> 8);
    walker->left_rb  = ((left_c->red & 0xff00) << 8)  | (left_c->blue >> 8);
    walker->right_ag = ((right_c->alpha >> 8) << 16)  | (right_c->green >> 8);
    walker->right_rb = ((right_c->red & 0xff00) << 8) | (right_c->blue >> 8);
    
    if ( walker->left_x == walker->right_x                ||
	 ( walker->left_ag == walker->right_ag &&
	   walker->left_rb == walker->right_rb )   )
    {
	walker->stepper = 0;
    }
    else
    {
	int32_t width = right_x - left_x;
	walker->stepper = ((1 << 24) + width/2)/width;
    }
    
    walker->need_reset = FALSE;
}

#define  GRADIENT_WALKER_NEED_RESET(w,x)				\
    ( (w)->need_reset || (x) < (w)->left_x || (x) >= (w)->right_x)


/* the following assumes that GRADIENT_WALKER_NEED_RESET(w,x) is FALSE */
static uint32_t
_gradient_walker_pixel (GradientWalker  *walker,
                        pixman_fixed_32_32_t     x)
{
    int  dist, idist;
    uint32_t  t1, t2, a, color;
    
    if (GRADIENT_WALKER_NEED_RESET (walker, x))
        _gradient_walker_reset (walker, x);
    
    dist  = ((int)(x - walker->left_x)*walker->stepper) >> 16;
    idist = 256 - dist;
    
    /* combined INTERPOLATE and premultiply */
    t1 = walker->left_rb*idist + walker->right_rb*dist;
    t1 = (t1 >> 8) & 0xff00ff;
    
    t2  = walker->left_ag*idist + walker->right_ag*dist;
    t2 &= 0xff00ff00;
    
    color = t2 & 0xff000000;
    a     = t2 >> 24;
    
    t1  = t1*a + 0x800080;
    t1  = (t1 + ((t1 >> 8) & 0xff00ff)) >> 8;
    
    t2  = (t2 >> 8)*a + 0x800080;
    t2  = (t2 + ((t2 >> 8) & 0xff00ff));
    
    return (color | (t1 & 0xff00ff) | (t2 & 0xff00));
}

static void pixmanFetchSourcePict(source_image_t * pict, int x, int y, int width, uint32_t *buffer, uint32_t *mask, uint32_t maskBits)
{
#if 0
    SourcePictPtr   pGradient = pict->pSourcePict;
#endif
    GradientWalker  walker;
    uint32_t       *end = buffer + width;
    gradient_t	    *gradient;
    
    if (pict->common.type == SOLID)
    {
	register uint32_t color = ((solid_fill_t *)pict)->color;
	
	while (buffer < end)
	    *(buffer++) = color;
	
	return;
    }
    
    gradient = (gradient_t *)pict;
    
    _gradient_walker_init (&walker, gradient, pict->common.repeat);
    
    if (pict->common.type == LINEAR) {
	pixman_vector_t v, unit;
	pixman_fixed_32_32_t l;
	pixman_fixed_48_16_t dx, dy, a, b, off;
	linear_gradient_t *linear = (linear_gradient_t *)pict;
	
        /* reference point is the center of the pixel */
        v.vector[0] = pixman_int_to_fixed(x) + pixman_fixed_1/2;
        v.vector[1] = pixman_int_to_fixed(y) + pixman_fixed_1/2;
        v.vector[2] = pixman_fixed_1;
        if (pict->common.transform) {
            if (!pixman_transform_point_3d (pict->common.transform, &v))
                return;
            unit.vector[0] = pict->common.transform->matrix[0][0];
            unit.vector[1] = pict->common.transform->matrix[1][0];
            unit.vector[2] = pict->common.transform->matrix[2][0];
        } else {
            unit.vector[0] = pixman_fixed_1;
            unit.vector[1] = 0;
            unit.vector[2] = 0;
        }
	
        dx = linear->p2.x - linear->p1.x;
        dy = linear->p2.y - linear->p1.y;
        l = dx*dx + dy*dy;
        if (l != 0) {
            a = (dx << 32) / l;
            b = (dy << 32) / l;
            off = (-a*linear->p1.x - b*linear->p1.y)>>16;
        }
        if (l == 0  || (unit.vector[2] == 0 && v.vector[2] == pixman_fixed_1)) {
            pixman_fixed_48_16_t inc, t;
            /* affine transformation only */
            if (l == 0) {
                t = 0;
                inc = 0;
            } else {
                t = ((a*v.vector[0] + b*v.vector[1]) >> 16) + off;
                inc = (a * unit.vector[0] + b * unit.vector[1]) >> 16;
            }
	    
	    if (pict->class == SOURCE_IMAGE_CLASS_VERTICAL)
	    {
		register uint32_t color;
		
		color = _gradient_walker_pixel( &walker, t );
		while (buffer < end)
		    *(buffer++) = color;
	    }
	    else
	    {
                if (!mask) {
                    while (buffer < end)
                    {
			*(buffer) = _gradient_walker_pixel (&walker, t);
                        buffer += 1;
                        t      += inc;
                    }
                } else {
                    while (buffer < end) {
                        if (*mask++ & maskBits)
                        {
			    *(buffer) = _gradient_walker_pixel (&walker, t);
                        }
                        buffer += 1;
                        t      += inc;
                    }
                }
	    }
	}
	else /* projective transformation */
	{
	    pixman_fixed_48_16_t t;
	    
	    if (pict->class == SOURCE_IMAGE_CLASS_VERTICAL)
	    {
		register uint32_t color;
		
		if (v.vector[2] == 0)
		{
		    t = 0;
		}
		else
		{
		    pixman_fixed_48_16_t x, y;
		    
		    x = ((pixman_fixed_48_16_t) v.vector[0] << 16) / v.vector[2];
		    y = ((pixman_fixed_48_16_t) v.vector[1] << 16) / v.vector[2];
		    t = ((a * x + b * y) >> 16) + off;
		}
		
 		color = _gradient_walker_pixel( &walker, t );
		while (buffer < end)
		    *(buffer++) = color;
	    }
	    else
	    {
		while (buffer < end)
		{
		    if (!mask || *mask++ & maskBits)
		    {
			if (v.vector[2] == 0) {
			    t = 0;
			} else {
			    pixman_fixed_48_16_t x, y;
			    x = ((pixman_fixed_48_16_t)v.vector[0] << 16) / v.vector[2];
			    y = ((pixman_fixed_48_16_t)v.vector[1] << 16) / v.vector[2];
			    t = ((a*x + b*y) >> 16) + off;
			}
			*(buffer) = _gradient_walker_pixel (&walker, t);
		    }
		    ++buffer;
		    v.vector[0] += unit.vector[0];
		    v.vector[1] += unit.vector[1];
		    v.vector[2] += unit.vector[2];
		}
            }
        }
    } else {
	
/*
 * In the radial gradient problem we are given two circles (c₁,r₁) and
 * (c₂,r₂) that define the gradient itself. Then, for any point p, we
 * must compute the value(s) of t within [0.0, 1.0] representing the
 * circle(s) that would color the point.
 *
 * There are potentially two values of t since the point p can be
 * colored by both sides of the circle, (which happens whenever one
 * circle is not entirely contained within the other).
 *
 * If we solve for a value of t that is outside of [0.0, 1.0] then we
 * use the extend mode (NONE, REPEAT, REFLECT, or PAD) to map to a
 * value within [0.0, 1.0].
 *
 * Here is an illustration of the problem:
 *
 *              p₂
 *           p  •
 *           •   ╲
 *        ·       ╲r₂
 *  p₁ ·           ╲
 *  •              θ╲
 *   ╲             ╌╌•
 *    ╲r₁        ·   c₂
 *    θ╲    ·
 *    ╌╌•
 *      c₁
 *
 * Given (c₁,r₁), (c₂,r₂) and p, we must find an angle θ such that two
 * points p₁ and p₂ on the two circles are collinear with p. Then, the
 * desired value of t is the ratio of the length of p₁p to the length
 * of p₁p₂.
 *
 * So, we have six unknown values: (p₁x, p₁y), (p₂x, p₂y), θ and t.
 * We can also write six equations that constrain the problem:
 *
 * Point p₁ is a distance r₁ from c₁ at an angle of θ:
 *
 *	1. p₁x = c₁x + r₁·cos θ
 *	2. p₁y = c₁y + r₁·sin θ
 *
 * Point p₂ is a distance r₂ from c₂ at an angle of θ:
 *
 *	3. p₂x = c₂x + r2·cos θ
 *	4. p₂y = c₂y + r2·sin θ
 *
 * Point p lies at a fraction t along the line segment p₁p₂:
 *
 *	5. px = t·p₂x + (1-t)·p₁x
 *	6. py = t·p₂y + (1-t)·p₁y
 *
 * To solve, first subtitute 1-4 into 5 and 6:
 *
 * px = t·(c₂x + r₂·cos θ) + (1-t)·(c₁x + r₁·cos θ)
 * py = t·(c₂y + r₂·sin θ) + (1-t)·(c₁y + r₁·sin θ)
 *
 * Then solve each for cos θ and sin θ expressed as a function of t:
 *
 * cos θ = (-(c₂x - c₁x)·t + (px - c₁x)) / ((r₂-r₁)·t + r₁)
 * sin θ = (-(c₂y - c₁y)·t + (py - c₁y)) / ((r₂-r₁)·t + r₁)
 *
 * To simplify this a bit, we define new variables for several of the
 * common terms as shown below:
 *
 *              p₂
 *           p  •
 *           •   ╲
 *        ·  ┆    ╲r₂
 *  p₁ ·     ┆     ╲
 *  •     pdy┆      ╲
 *   ╲       ┆       •c₂
 *    ╲r₁    ┆   ·   ┆
 *     ╲    ·┆       ┆cdy
 *      •╌╌╌╌┴╌╌╌╌╌╌╌┘
 *    c₁  pdx   cdx
 *
 * cdx = (c₂x - c₁x)
 * cdy = (c₂y - c₁y)
 *  dr =  r₂-r₁
 * pdx =  px - c₁x
 * pdy =  py - c₁y
 *
 * Note that cdx, cdy, and dr do not depend on point p at all, so can
 * be pre-computed for the entire gradient. The simplifed equations
 * are now:
 *
 * cos θ = (-cdx·t + pdx) / (dr·t + r₁)
 * sin θ = (-cdy·t + pdy) / (dr·t + r₁)
 *
 * Finally, to get a single function of t and eliminate the last
 * unknown θ, we use the identity sin²θ + cos²θ = 1. First, square
 * each equation, (we knew a quadratic was coming since it must be
 * possible to obtain two solutions in some cases):
 *
 * cos²θ = (cdx²t² - 2·cdx·pdx·t + pdx²) / (dr²·t² + 2·r₁·dr·t + r₁²)
 * sin²θ = (cdy²t² - 2·cdy·pdy·t + pdy²) / (dr²·t² + 2·r₁·dr·t + r₁²)
 *
 * Then add both together, set the result equal to 1, and express as a
 * standard quadratic equation in t of the form At² + Bt + C = 0
 *
 * (cdx² + cdy² - dr²)·t² - 2·(cdx·pdx + cdy·pdy + r₁·dr)·t + (pdx² + pdy² - r₁²) = 0
 *
 * In other words:
 *
 * A = cdx² + cdy² - dr²
 * B = -2·(pdx·cdx + pdy·cdy + r₁·dr)
 * C = pdx² + pdy² - r₁²
 *
 * And again, notice that A does not depend on p, so can be
 * precomputed. From here we just use the quadratic formula to solve
 * for t:
 *
 * t = (-2·B ± ⎷(B² - 4·A·C)) / 2·A
 */
        /* radial or conical */
        pixman_bool_t affine = TRUE;
        double cx = 1.;
        double cy = 0.;
        double cz = 0.;
	double rx = x + 0.5;
	double ry = y + 0.5;
        double rz = 1.;
	
        if (pict->common.transform) {
            pixman_vector_t v;
            /* reference point is the center of the pixel */
            v.vector[0] = pixman_int_to_fixed(x) + pixman_fixed_1/2;
            v.vector[1] = pixman_int_to_fixed(y) + pixman_fixed_1/2;
            v.vector[2] = pixman_fixed_1;
            if (!pixman_transform_point_3d (pict->common.transform, &v))
                return;
	    
            cx = pict->common.transform->matrix[0][0]/65536.;
            cy = pict->common.transform->matrix[1][0]/65536.;
            cz = pict->common.transform->matrix[2][0]/65536.;
            rx = v.vector[0]/65536.;
            ry = v.vector[1]/65536.;
            rz = v.vector[2]/65536.;
            affine = pict->common.transform->matrix[2][0] == 0 && v.vector[2] == pixman_fixed_1;
        }
	
        if (pict->common.type == RADIAL) {
	    radial_gradient_t *radial = (radial_gradient_t *)pict;
            if (affine) {
                while (buffer < end) {
		    if (!mask || *mask++ & maskBits)
		    {
			double pdx, pdy;
			double B, C;
			double det;
			double c1x = radial->c1.x / 65536.0;
			double c1y = radial->c1.y / 65536.0;
			double r1  = radial->c1.radius / 65536.0;
                        pixman_fixed_48_16_t t;
			
			pdx = rx - c1x;
			pdy = ry - c1y;
			
			B = -2 * (  pdx * radial->cdx
				    + pdy * radial->cdy
				    + r1 * radial->dr);
			C = (pdx * pdx + pdy * pdy - r1 * r1);
			
                        det = (B * B) - (4 * radial->A * C);
			if (det < 0.0)
			    det = 0.0;
			
			if (radial->A < 0)
			    t = (pixman_fixed_48_16_t) ((- B - sqrt(det)) / (2.0 * radial->A) * 65536);
			else
			    t = (pixman_fixed_48_16_t) ((- B + sqrt(det)) / (2.0 * radial->A) * 65536);
			
			*(buffer) = _gradient_walker_pixel (&walker, t);
		    }
		    ++buffer;
		    
                    rx += cx;
                    ry += cy;
                }
            } else {
		/* projective */
                while (buffer < end) {
		    if (!mask || *mask++ & maskBits)
		    {
			double pdx, pdy;
			double B, C;
			double det;
			double c1x = radial->c1.x / 65536.0;
			double c1y = radial->c1.y / 65536.0;
			double r1  = radial->c1.radius / 65536.0;
                        pixman_fixed_48_16_t t;
			double x, y;
			
			if (rz != 0) {
			    x = rx/rz;
			    y = ry/rz;
			} else {
			    x = y = 0.;
			}
			
			pdx = x - c1x;
			pdy = y - c1y;
			
			B = -2 * (  pdx * radial->cdx
				    + pdy * radial->cdy
				    + r1 * radial->dr);
			C = (pdx * pdx + pdy * pdy - r1 * r1);
			
                        det = (B * B) - (4 * radial->A * C);
			if (det < 0.0)
			    det = 0.0;
			
			if (radial->A < 0)
			    t = (pixman_fixed_48_16_t) ((- B - sqrt(det)) / (2.0 * radial->A) * 65536);
			else
			    t = (pixman_fixed_48_16_t) ((- B + sqrt(det)) / (2.0 * radial->A) * 65536);
			
			*(buffer) = _gradient_walker_pixel (&walker, t);
		    }
		    ++buffer;
		    
                    rx += cx;
                    ry += cy;
		    rz += cz;
                }
            }
        } else /* SourcePictTypeConical */ {
	    conical_gradient_t *conical = (conical_gradient_t *)pict;
            double a = conical->angle/(180.*65536);
            if (affine) {
                rx -= conical->center.x/65536.;
                ry -= conical->center.y/65536.;
		
                while (buffer < end) {
		    double angle;
		    
                    if (!mask || *mask++ & maskBits)
		    {
                        pixman_fixed_48_16_t   t;
			
                        angle = atan2(ry, rx) + a;
			t     = (pixman_fixed_48_16_t) (angle * (65536. / (2*M_PI)));
			
			*(buffer) = _gradient_walker_pixel (&walker, t);
		    }
		    
                    ++buffer;
                    rx += cx;
                    ry += cy;
                }
            } else {
                while (buffer < end) {
                    double x, y;
                    double angle;
		    
                    if (!mask || *mask++ & maskBits)
                    {
			pixman_fixed_48_16_t  t;
			
			if (rz != 0) {
			    x = rx/rz;
			    y = ry/rz;
			} else {
			    x = y = 0.;
			}
			x -= conical->center.x/65536.;
			y -= conical->center.y/65536.;
			angle = atan2(y, x) + a;
			t     = (pixman_fixed_48_16_t) (angle * (65536. / (2*M_PI)));
			
			*(buffer) = _gradient_walker_pixel (&walker, t);
		    }
		    
                    ++buffer;
                    rx += cx;
                    ry += cy;
                    rz += cz;
                }
            }
        }
    }
}

static void fbFetchTransformed(bits_image_t * pict, int x, int y, int width, uint32_t *buffer, uint32_t *mask, uint32_t maskBits)
{
    uint32_t     *bits;
    uint32_t    stride;
    fetchPixelProc   fetch;
    pixman_vector_t	v;
    pixman_vector_t  unit;
    int         i;
    pixman_box16_t box;
    const pixman_indexed_t * indexed = pict->indexed;
    pixman_bool_t affine = TRUE;
    
    fetch = fetchPixelProcForPicture(pict);
    
    bits = pict->bits;
    stride = pict->rowstride;
    
    /* reference point is the center of the pixel */
    v.vector[0] = pixman_int_to_fixed(x) + pixman_fixed_1 / 2;
    v.vector[1] = pixman_int_to_fixed(y) + pixman_fixed_1 / 2;
    v.vector[2] = pixman_fixed_1;
    
    /* when using convolution filters one might get here without a transform */
    if (pict->common.transform)
    {
        if (!pixman_transform_point_3d (pict->common.transform, &v))
	{
            fbFinishAccess (pict->pDrawable);
            return;
        }
        unit.vector[0] = pict->common.transform->matrix[0][0];
        unit.vector[1] = pict->common.transform->matrix[1][0];
        unit.vector[2] = pict->common.transform->matrix[2][0];
        affine = v.vector[2] == pixman_fixed_1 && unit.vector[2] == 0;
    }
    else
    {
        unit.vector[0] = pixman_fixed_1;
        unit.vector[1] = 0;
        unit.vector[2] = 0;
    }
    
    if (pict->common.filter == PIXMAN_FILTER_NEAREST || pict->common.filter == PIXMAN_FILTER_FAST)
    {
        if (pict->common.repeat == PIXMAN_REPEAT_NORMAL) {
            if (pixman_region_n_rects (pict->common.src_clip) == 1) {
		for (i = 0; i < width; ++i) {
		    if (!mask || mask[i] & maskBits)
		    {
			if (!v.vector[2]) {
			    *(buffer + i) = 0;
			} else {
			    if (!affine) {
				y = MOD(DIV(v.vector[1],v.vector[2]), pict->height);
				x = MOD(DIV(v.vector[0],v.vector[2]), pict->width);
			    } else {
				y = MOD(v.vector[1]>>16, pict->height);
				x = MOD(v.vector[0]>>16, pict->width);
			    }
			    *(buffer + i) = fetch((pixman_image_t *)pict, bits + y * stride, x, indexed);
			}
		    }
		    
                    v.vector[0] += unit.vector[0];
                    v.vector[1] += unit.vector[1];
                    v.vector[2] += unit.vector[2];
                }
            } else {
                for (i = 0; i < width; ++i) {
		    if (!mask || mask[i] & maskBits)
		    {
			if (!v.vector[2]) {
			    *(buffer + i) = 0;
			} else {
			    if (!affine) {
				y = MOD(DIV(v.vector[1],v.vector[2]), pict->height);
				x = MOD(DIV(v.vector[0],v.vector[2]), pict->width);
			    } else {
				y = MOD(v.vector[1]>>16, pict->height);
				x = MOD(v.vector[0]>>16, pict->width);
			    }
			    if (pixman_region_contains_point (pict->common.src_clip, x, y, &box))
				*(buffer + i) = fetch ((pixman_image_t *)pict, bits + y*stride, x, indexed);
			    else
				*(buffer + i) = 0;
			}
		    }
		    
                    v.vector[0] += unit.vector[0];
                    v.vector[1] += unit.vector[1];
                    v.vector[2] += unit.vector[2];
                }
            }
        } else {
            if (pixman_region_n_rects(pict->common.src_clip) == 1) {
                box = pict->common.src_clip->extents;
                for (i = 0; i < width; ++i) {
		    if (!mask || mask[i] & maskBits)
		    {
			if (!v.vector[2]) {
			    *(buffer + i) = 0;
			} else {
			    if (!affine) {
				y = DIV(v.vector[1],v.vector[2]);
				x = DIV(v.vector[0],v.vector[2]);
			    } else {
				y = v.vector[1]>>16;
				x = v.vector[0]>>16;
			    }
			    *(buffer + i) = ((x < box.x1) | (x >= box.x2) | (y < box.y1) | (y >= box.y2)) ?
				0 : fetch((pixman_image_t *)pict, bits + (y)*stride, x, indexed);
			}
		    }
                    v.vector[0] += unit.vector[0];
                    v.vector[1] += unit.vector[1];
                    v.vector[2] += unit.vector[2];
                }
            } else {
                for (i = 0; i < width; ++i) {
                    if (!mask || mask[i] & maskBits)
		    {
			if (!v.vector[2]) {
			    *(buffer + i) = 0;
			} else {
			    if (!affine) {
				y = DIV(v.vector[1],v.vector[2]);
				x = DIV(v.vector[0],v.vector[2]);
			    } else {
				y = v.vector[1]>>16;
				x = v.vector[0]>>16;
			    }
			    if (pixman_region_contains_point (pict->common.src_clip, x, y, &box))
				*(buffer + i) = fetch((pixman_image_t *)pict, bits + y*stride, x, indexed);
			    else
				*(buffer + i) = 0;
			}
		    }
                    v.vector[0] += unit.vector[0];
                    v.vector[1] += unit.vector[1];
                    v.vector[2] += unit.vector[2];
                }
            }
        }
    } else if (pict->common.filter == PIXMAN_FILTER_BILINEAR	||
	       pict->common.filter == PIXMAN_FILTER_GOOD	||
	       pict->common.filter == PIXMAN_FILTER_BEST)
    {
        /* adjust vector for maximum contribution at 0.5, 0.5 of each texel. */
        v.vector[0] -= v.vector[2] / 2;
        v.vector[1] -= v.vector[2] / 2;
        unit.vector[0] -= unit.vector[2] / 2;
        unit.vector[1] -= unit.vector[2] / 2;
	
        if (pict->common.repeat == PIXMAN_REPEAT_NORMAL) {
            if (pixman_region_n_rects(pict->common.src_clip) == 1) {
                for (i = 0; i < width; ++i) {
                    if (!mask || mask[i] & maskBits)
		    {
			if (!v.vector[2]) {
			    *(buffer + i) = 0;
			} else {
			    int x1, x2, y1, y2, distx, idistx, disty, idisty;
			    uint32_t *b;
			    uint32_t tl, tr, bl, br, r;
			    uint32_t ft, fb;
			    
			    if (!affine) {
				pixman_fixed_48_16_t div;
				div = ((pixman_fixed_48_16_t)v.vector[0] << 16)/v.vector[2];
				x1 = div >> 16;
				distx = ((pixman_fixed_t)div >> 8) & 0xff;
				div = ((pixman_fixed_48_16_t)v.vector[1] << 16)/v.vector[2];
				y1 = div >> 16;
				disty = ((pixman_fixed_t)div >> 8) & 0xff;
			    } else {
				x1 = v.vector[0] >> 16;
				distx = (v.vector[0] >> 8) & 0xff;
				y1 = v.vector[1] >> 16;
				disty = (v.vector[1] >> 8) & 0xff;
			    }
			    x2 = x1 + 1;
			    y2 = y1 + 1;
			    
			    idistx = 256 - distx;
			    idisty = 256 - disty;
			    
			    x1 = MOD (x1, pict->width);
			    x2 = MOD (x2, pict->width);
			    y1 = MOD (y1, pict->height);
			    y2 = MOD (y2, pict->height);
			    
			    b = bits + y1*stride;
			    
			    tl = fetch((pixman_image_t *)pict, b, x1, indexed);
			    tr = fetch((pixman_image_t *)pict, b, x2, indexed);
			    b = bits + y2*stride;
			    bl = fetch((pixman_image_t *)pict, b, x1, indexed);
			    br = fetch((pixman_image_t *)pict, b, x2, indexed);
			    
			    ft = FbGet8(tl,0) * idistx + FbGet8(tr,0) * distx;
			    fb = FbGet8(bl,0) * idistx + FbGet8(br,0) * distx;
			    r = (((ft * idisty + fb * disty) >> 16) & 0xff);
			    ft = FbGet8(tl,8) * idistx + FbGet8(tr,8) * distx;
			    fb = FbGet8(bl,8) * idistx + FbGet8(br,8) * distx;
			    r |= (((ft * idisty + fb * disty) >> 8) & 0xff00);
			    ft = FbGet8(tl,16) * idistx + FbGet8(tr,16) * distx;
			    fb = FbGet8(bl,16) * idistx + FbGet8(br,16) * distx;
			    r |= (((ft * idisty + fb * disty)) & 0xff0000);
			    ft = FbGet8(tl,24) * idistx + FbGet8(tr,24) * distx;
			    fb = FbGet8(bl,24) * idistx + FbGet8(br,24) * distx;
			    r |= (((ft * idisty + fb * disty) << 8) & 0xff000000);
			    *(buffer + i) = r;
			}
		    }
                    v.vector[0] += unit.vector[0];
                    v.vector[1] += unit.vector[1];
                    v.vector[2] += unit.vector[2];
                }
            } else {
                for (i = 0; i < width; ++i) {
		    if (!mask || mask[i] & maskBits)
		    {
			if (!v.vector[2]) {
			    *(buffer + i) = 0;
			} else {
			    int x1, x2, y1, y2, distx, idistx, disty, idisty;
			    uint32_t *b;
			    uint32_t tl, tr, bl, br, r;
			    uint32_t ft, fb;
			    
			    if (!affine) {
				pixman_fixed_48_16_t div;
				div = ((pixman_fixed_48_16_t)v.vector[0] << 16)/v.vector[2];
				x1 = div >> 16;
				distx = ((pixman_fixed_t)div >> 8) & 0xff;
				div = ((pixman_fixed_48_16_t)v.vector[1] << 16)/v.vector[2];
				y1 = div >> 16;
				disty = ((pixman_fixed_t)div >> 8) & 0xff;
			    } else {
				x1 = v.vector[0] >> 16;
				distx = (v.vector[0] >> 8) & 0xff;
				y1 = v.vector[1] >> 16;
				disty = (v.vector[1] >> 8) & 0xff;
			    }
			    x2 = x1 + 1;
			    y2 = y1 + 1;
			    
			    idistx = 256 - distx;
			    idisty = 256 - disty;
			    
			    x1 = MOD (x1, pict->width);
			    x2 = MOD (x2, pict->width);
			    y1 = MOD (y1, pict->height);
			    y2 = MOD (y2, pict->height);
			    
			    b = bits + y1*stride;
			    
			    tl = pixman_region_contains_point(pict->common.src_clip, x1, y1, &box)
				? fetch((pixman_image_t *)pict, b, x1, indexed) : 0;
			    tr = pixman_region_contains_point(pict->common.src_clip, x2, y1, &box)
				? fetch((pixman_image_t *)pict, b, x2, indexed) : 0;
			    b = bits + (y2)*stride;
			    bl = pixman_region_contains_point(pict->common.src_clip, x1, y2, &box)
				? fetch((pixman_image_t *)pict, b, x1, indexed) : 0;
			    br = pixman_region_contains_point(pict->common.src_clip, x2, y2, &box)
				? fetch((pixman_image_t *)pict, b, x2, indexed) : 0;
			    
			    ft = FbGet8(tl,0) * idistx + FbGet8(tr,0) * distx;
			    fb = FbGet8(bl,0) * idistx + FbGet8(br,0) * distx;
			    r = (((ft * idisty + fb * disty) >> 16) & 0xff);
			    ft = FbGet8(tl,8) * idistx + FbGet8(tr,8) * distx;
			    fb = FbGet8(bl,8) * idistx + FbGet8(br,8) * distx;
			    r |= (((ft * idisty + fb * disty) >> 8) & 0xff00);
			    ft = FbGet8(tl,16) * idistx + FbGet8(tr,16) * distx;
			    fb = FbGet8(bl,16) * idistx + FbGet8(br,16) * distx;
			    r |= (((ft * idisty + fb * disty)) & 0xff0000);
			    ft = FbGet8(tl,24) * idistx + FbGet8(tr,24) * distx;
			    fb = FbGet8(bl,24) * idistx + FbGet8(br,24) * distx;
			    r |= (((ft * idisty + fb * disty) << 8) & 0xff000000);
			    *(buffer + i) = r;
			}
		    }
		    
                    v.vector[0] += unit.vector[0];
                    v.vector[1] += unit.vector[1];
                    v.vector[2] += unit.vector[2];
                }
            }
        } else {
            if (pixman_region_n_rects(pict->common.src_clip) == 1) {
                box = pict->common.src_clip->extents;
                for (i = 0; i < width; ++i) {
		    if (!mask || mask[i] & maskBits)
		    {
			if (!v.vector[2]) {
			    *(buffer + i) = 0;
			} else {
			    int x1, x2, y1, y2, distx, idistx, disty, idisty, x_off;
			    uint32_t *b;
			    uint32_t tl, tr, bl, br, r;
			    pixman_bool_t x1_out, x2_out, y1_out, y2_out;
			    uint32_t ft, fb;
			    
			    if (!affine) {
				pixman_fixed_48_16_t div;
				div = ((pixman_fixed_48_16_t)v.vector[0] << 16)/v.vector[2];
				x1 = div >> 16;
				distx = ((pixman_fixed_t)div >> 8) & 0xff;
				div = ((pixman_fixed_48_16_t)v.vector[1] << 16)/v.vector[2];
				y1 = div >> 16;
				disty = ((pixman_fixed_t)div >> 8) & 0xff;
			    } else {
				x1 = v.vector[0] >> 16;
				distx = (v.vector[0] >> 8) & 0xff;
				y1 = v.vector[1] >> 16;
				disty = (v.vector[1] >> 8) & 0xff;
			    }
			    x2 = x1 + 1;
			    y2 = y1 + 1;
			    
			    idistx = 256 - distx;
			    idisty = 256 - disty;
			    
			    b = bits + (y1)*stride;
			    x_off = x1;
			    
			    x1_out = (x1 < box.x1) | (x1 >= box.x2);
			    x2_out = (x2 < box.x1) | (x2 >= box.x2);
			    y1_out = (y1 < box.y1) | (y1 >= box.y2);
			    y2_out = (y2 < box.y1) | (y2 >= box.y2);
			    
			    tl = x1_out|y1_out ? 0 : fetch((pixman_image_t *)pict, b, x_off, indexed);
			    tr = x2_out|y1_out ? 0 : fetch((pixman_image_t *)pict, b, x_off + 1, indexed);
			    b  =  bits + (y2)*stride;
			    bl = x1_out|y2_out ? 0 : fetch((pixman_image_t *)pict, b, x_off, indexed);
			    br = x2_out|y2_out ? 0 : fetch((pixman_image_t *)pict, b, x_off + 1, indexed);
			    
			    ft = FbGet8(tl,0) * idistx + FbGet8(tr,0) * distx;
			    fb = FbGet8(bl,0) * idistx + FbGet8(br,0) * distx;
			    r = (((ft * idisty + fb * disty) >> 16) & 0xff);
			    ft = FbGet8(tl,8) * idistx + FbGet8(tr,8) * distx;
			    fb = FbGet8(bl,8) * idistx + FbGet8(br,8) * distx;
			    r |= (((ft * idisty + fb * disty) >> 8) & 0xff00);
			    ft = FbGet8(tl,16) * idistx + FbGet8(tr,16) * distx;
			    fb = FbGet8(bl,16) * idistx + FbGet8(br,16) * distx;
			    r |= (((ft * idisty + fb * disty)) & 0xff0000);
			    ft = FbGet8(tl,24) * idistx + FbGet8(tr,24) * distx;
			    fb = FbGet8(bl,24) * idistx + FbGet8(br,24) * distx;
			    r |= (((ft * idisty + fb * disty) << 8) & 0xff000000);
			    *(buffer + i) = r;
			}
		    }
		    
                    v.vector[0] += unit.vector[0];
                    v.vector[1] += unit.vector[1];
                    v.vector[2] += unit.vector[2];
                }
            } else {
                for (i = 0; i < width; ++i) {
                    if (!mask || mask[i] & maskBits)
		    {
			if (!v.vector[2]) {
			    *(buffer + i) = 0;
			} else {
			    int x1, x2, y1, y2, distx, idistx, disty, idisty, x_off;
			    uint32_t *b;
			    uint32_t tl, tr, bl, br, r;
			    uint32_t ft, fb;
			    
			    if (!affine) {
				pixman_fixed_48_16_t div;
				div = ((pixman_fixed_48_16_t)v.vector[0] << 16)/v.vector[2];
				x1 = div >> 16;
				distx = ((pixman_fixed_t)div >> 8) & 0xff;
				div = ((pixman_fixed_48_16_t)v.vector[1] << 16)/v.vector[2];
				y1 = div >> 16;
				disty = ((pixman_fixed_t)div >> 8) & 0xff;
			    } else {
				x1 = v.vector[0] >> 16;
				distx = (v.vector[0] >> 8) & 0xff;
				y1 = v.vector[1] >> 16;
				disty = (v.vector[1] >> 8) & 0xff;
			    }
			    x2 = x1 + 1;
			    y2 = y1 + 1;
			    
			    idistx = 256 - distx;
			    idisty = 256 - disty;
			    
			    b = bits + (y1)*stride;
			    x_off = x1;
			    
			    tl = pixman_region_contains_point(pict->common.src_clip, x1, y1, &box)
				? fetch((pixman_image_t *)pict, b, x_off, indexed) : 0;
			    tr = pixman_region_contains_point(pict->common.src_clip, x2, y1, &box)
				? fetch((pixman_image_t *)pict, b, x_off + 1, indexed) : 0;
			    b  =  bits + (y2)*stride;
			    bl = pixman_region_contains_point(pict->common.src_clip, x1, y2, &box)
				? fetch((pixman_image_t *)pict, b, x_off, indexed) : 0;
			    br = pixman_region_contains_point(pict->common.src_clip, x2, y2, &box)
				? fetch((pixman_image_t *)pict, b, x_off + 1, indexed) : 0;
			    
			    ft = FbGet8(tl,0) * idistx + FbGet8(tr,0) * distx;
			    fb = FbGet8(bl,0) * idistx + FbGet8(br,0) * distx;
			    r = (((ft * idisty + fb * disty) >> 16) & 0xff);
			    ft = FbGet8(tl,8) * idistx + FbGet8(tr,8) * distx;
			    fb = FbGet8(bl,8) * idistx + FbGet8(br,8) * distx;
			    r |= (((ft * idisty + fb * disty) >> 8) & 0xff00);
			    ft = FbGet8(tl,16) * idistx + FbGet8(tr,16) * distx;
			    fb = FbGet8(bl,16) * idistx + FbGet8(br,16) * distx;
			    r |= (((ft * idisty + fb * disty)) & 0xff0000);
			    ft = FbGet8(tl,24) * idistx + FbGet8(tr,24) * distx;
			    fb = FbGet8(bl,24) * idistx + FbGet8(br,24) * distx;
			    r |= (((ft * idisty + fb * disty) << 8) & 0xff000000);
			    *(buffer + i) = r;
			}
		    }
		    
                    v.vector[0] += unit.vector[0];
                    v.vector[1] += unit.vector[1];
                    v.vector[2] += unit.vector[2];
                }
            }
        }
    } else if (pict->common.filter == PIXMAN_FILTER_CONVOLUTION) {
        pixman_fixed_t *params = pict->common.filter_params;
        int32_t cwidth = pixman_fixed_to_int(params[0]);
        int32_t cheight = pixman_fixed_to_int(params[1]);
        int xoff = (params[0] - pixman_fixed_1) >> 1;
	int yoff = (params[1] - pixman_fixed_1) >> 1;
        params += 2;
        for (i = 0; i < width; ++i) {
	    if (!mask || mask[i] & maskBits)
	    {
		if (!v.vector[2]) {
		    *(buffer + i) = 0;
		} else {
		    int x1, x2, y1, y2, x, y;
		    int32_t srtot, sgtot, sbtot, satot;
		    pixman_fixed_t *p = params;
		    
		    if (!affine) {
			pixman_fixed_48_16_t tmp;
			tmp = ((pixman_fixed_48_16_t)v.vector[0] << 16)/v.vector[2] - xoff;
			x1 = pixman_fixed_to_int(tmp);
			tmp = ((pixman_fixed_48_16_t)v.vector[1] << 16)/v.vector[2] - yoff;
			y1 = pixman_fixed_to_int(tmp);
		    } else {
			x1 = pixman_fixed_to_int(v.vector[0] - xoff);
			y1 = pixman_fixed_to_int(v.vector[1] - yoff);
		    }
		    x2 = x1 + cwidth;
		    y2 = y1 + cheight;
		    
		    srtot = sgtot = sbtot = satot = 0;
		    
		    for (y = y1; y < y2; y++) {
			int ty = (pict->common.repeat == PIXMAN_REPEAT_NORMAL) ? MOD (y, pict->height) : y;
			for (x = x1; x < x2; x++) {
			    if (*p) {
				int tx = (pict->common.repeat == PIXMAN_REPEAT_NORMAL) ? MOD (x, pict->width) : x;
				if (pixman_region_contains_point (pict->common.src_clip, tx, ty, &box)) {
				    uint32_t *b = bits + (ty)*stride;
				    uint32_t c = fetch((pixman_image_t *)pict, b, tx, indexed);
				    
				    srtot += Red(c) * *p;
				    sgtot += Green(c) * *p;
				    sbtot += Blue(c) * *p;
				    satot += Alpha(c) * *p;
				}
			    }
			    p++;
			}
		    }
		    
		    satot >>= 16;
		    srtot >>= 16;
		    sgtot >>= 16;
		    sbtot >>= 16;
		    
		    if (satot < 0) satot = 0; else if (satot > 0xff) satot = 0xff;
		    if (srtot < 0) srtot = 0; else if (srtot > 0xff) srtot = 0xff;
		    if (sgtot < 0) sgtot = 0; else if (sgtot > 0xff) sgtot = 0xff;
		    if (sbtot < 0) sbtot = 0; else if (sbtot > 0xff) sbtot = 0xff;
		    
		    *(buffer + i) = ((satot << 24) |
				     (srtot << 16) |
				     (sgtot <<  8) |
				     (sbtot       ));
		}
	    }
            v.vector[0] += unit.vector[0];
            v.vector[1] += unit.vector[1];
            v.vector[2] += unit.vector[2];
        }
    }
    
    fbFinishAccess (pict->pDrawable);
}


static void fbFetchExternalAlpha(bits_image_t * pict, int x, int y, int width, uint32_t *buffer, uint32_t *mask, uint32_t maskBits)
{
    int i;
    uint32_t _alpha_buffer[SCANLINE_BUFFER_LENGTH];
    uint32_t *alpha_buffer = _alpha_buffer;
    
    if (!pict->common.alpha_map) {
        fbFetchTransformed (pict, x, y, width, buffer, mask, maskBits);
	return;
    }
    if (width > SCANLINE_BUFFER_LENGTH)
        alpha_buffer = (uint32_t *) pixman_malloc_ab (width, sizeof(uint32_t));
    
    fbFetchTransformed(pict, x, y, width, buffer, mask, maskBits);
    fbFetchTransformed((bits_image_t *)pict->common.alpha_map, x - pict->common.alpha_origin.x,
		       y - pict->common.alpha_origin.y, width, alpha_buffer,
		       mask, maskBits);
    for (i = 0; i < width; ++i) {
        if (!mask || mask[i] & maskBits)
	{
	    int a = alpha_buffer[i]>>24;
	    *(buffer + i) = (a << 24)
		| (div_255(Red(*(buffer + i)) * a) << 16)
		| (div_255(Green(*(buffer + i)) * a) << 8)
		| (div_255(Blue(*(buffer + i)) * a));
	}
    }
    
    if (alpha_buffer != _alpha_buffer)
        free(alpha_buffer);
}

static void fbStore(bits_image_t * pict, int x, int y, int width, uint32_t *buffer)
{
    uint32_t *bits;
    uint32_t stride;
    storeProc store = storeProcForPicture(pict);
    const pixman_indexed_t * indexed = pict->indexed;
    
    bits = pict->bits;
    stride = pict->rowstride;
    bits += y*stride;
    store((pixman_image_t *)pict, bits, buffer, x, width, indexed);
    fbFinishAccess (pict->pDrawable);
}

static void fbStoreExternalAlpha(bits_image_t * pict, int x, int y, int width, uint32_t *buffer)
{
    uint32_t *bits, *alpha_bits;
    uint32_t stride, astride;
    int ax, ay;
    storeProc store;
    storeProc astore;
    const pixman_indexed_t * indexed = pict->indexed;
    const pixman_indexed_t * aindexed;
    
    if (!pict->common.alpha_map) {
        fbStore(pict, x, y, width, buffer);
	return;
    }
    
    store = storeProcForPicture(pict);
    astore = storeProcForPicture(pict->common.alpha_map);
    aindexed = pict->common.alpha_map->indexed;
    
    ax = x;
    ay = y;
    
    bits = pict->bits;
    stride = pict->rowstride;
    
    alpha_bits = pict->common.alpha_map->bits;
    astride = pict->common.alpha_map->rowstride;
    
    bits       += y*stride;
    alpha_bits += (ay - pict->common.alpha_origin.y)*astride;
    
    
    store((pixman_image_t *)pict, bits, buffer, x, width, indexed);
    astore((pixman_image_t *)pict->common.alpha_map,
	   alpha_bits, buffer, ax - pict->common.alpha_origin.x, width, aindexed);
    
    fbFinishAccess (pict->alpha_map->pDrawable);
    fbFinishAccess (pict->pDrawable);
}

typedef void (*scanStoreProc)(pixman_image_t *, int, int, int, uint32_t *);
typedef void (*scanFetchProc)(pixman_image_t *, int, int, int, uint32_t *,
			      uint32_t *, uint32_t);

#ifndef PIXMAN_FB_ACCESSORS
static
#endif
void
PIXMAN_COMPOSITE_RECT_GENERAL (const FbComposeData *data,
			       uint32_t *scanline_buffer)
{
    uint32_t *src_buffer = scanline_buffer;
    uint32_t *dest_buffer = src_buffer + data->width;
    int i;
    scanStoreProc store;
    scanFetchProc fetchSrc = NULL, fetchMask = NULL, fetchDest = NULL;
    unsigned int srcClass = SOURCE_IMAGE_CLASS_UNKNOWN;
    unsigned int maskClass = SOURCE_IMAGE_CLASS_UNKNOWN;
    uint32_t *bits;
    uint32_t stride;
    int xoff, yoff;
    
    if (data->op == PIXMAN_OP_CLEAR)
        fetchSrc = NULL;
    else if (IS_SOURCE_IMAGE (data->src))
    {
	fetchSrc = (scanFetchProc)pixmanFetchSourcePict;
	srcClass = SourcePictureClassify ((source_image_t *)data->src,
					  data->xSrc, data->ySrc,
					  data->width, data->height);
    }
    else
    {
	bits_image_t *bits = (bits_image_t *)data->src;
	
	if (bits->common.alpha_map)
	{
	    fetchSrc = (scanFetchProc)fbFetchExternalAlpha;
	}
	else if (bits->common.repeat == PIXMAN_REPEAT_NORMAL &&
		 bits->width == 1 &&
		 bits->height == 1)
	{
	    fetchSrc = (scanFetchProc)fbFetchSolid;
	    srcClass = SOURCE_IMAGE_CLASS_HORIZONTAL;
	}
	else if (!bits->common.transform && bits->common.filter != PIXMAN_FILTER_CONVOLUTION)
	{
	    fetchSrc = (scanFetchProc)fbFetch;
	}
	else
	{
	    fetchSrc = (scanFetchProc)fbFetchTransformed;
	}
    }
    
    if (!data->mask || data->op == PIXMAN_OP_CLEAR)
    {
	fetchMask = NULL;
    }
    else
    {
	if (IS_SOURCE_IMAGE (data->mask))
	{
	    fetchMask = (scanFetchProc)pixmanFetchSourcePict;
	    maskClass = SourcePictureClassify ((source_image_t *)data->mask,
					       data->xMask, data->yMask,
					       data->width, data->height);
	}
	else
	{
	    bits_image_t *bits = (bits_image_t *)data->mask;
	    
	    if (bits->common.alpha_map)
	    {
		fetchMask = (scanFetchProc)fbFetchExternalAlpha;
	    }
	    else if (bits->common.repeat == PIXMAN_REPEAT_NORMAL &&
		     bits->width == 1 && bits->height == 1)
	    {
		fetchMask = (scanFetchProc)fbFetchSolid;
		maskClass = SOURCE_IMAGE_CLASS_HORIZONTAL;
	    }
	    else if (!bits->common.transform && bits->common.filter != PIXMAN_FILTER_CONVOLUTION)
		fetchMask = (scanFetchProc)fbFetch;
	    else
		fetchMask = (scanFetchProc)fbFetchTransformed;
	}
    }
    
    if (data->dest->common.alpha_map)
    {
	fetchDest = (scanFetchProc)fbFetchExternalAlpha;
	store = (scanStoreProc)fbStoreExternalAlpha;
	
	if (data->op == PIXMAN_OP_CLEAR || data->op == PIXMAN_OP_SRC)
	    fetchDest = NULL;
    }
    else
    {
	fetchDest = (scanFetchProc)fbFetch;
	store = (scanStoreProc)fbStore;
	
	switch (data->op)
	{
	case PIXMAN_OP_CLEAR:
	case PIXMAN_OP_SRC:
	    fetchDest = NULL;
#ifndef PIXMAN_FB_ACCESSORS
	    /* fall-through */
	case PIXMAN_OP_ADD:
	case PIXMAN_OP_OVER:
	    switch (data->dest->bits.format) {
	    case PIXMAN_a8r8g8b8:
	    case PIXMAN_x8r8g8b8:
		store = NULL;
		break;
	    default:
		break;
	    }
#endif
	    break;
	}
    }
    
    if (!store)
    {
	bits = data->dest->bits.bits;
	stride = data->dest->bits.rowstride;
	xoff = yoff = 0;
    }
    else
    {
	bits = NULL;
	stride = 0;
	xoff = yoff = 0;
    }
    
    if (fetchSrc		   &&
	fetchMask		   &&
	data->mask		   &&
	data->mask->common.type == BITS && 
	data->mask->common.component_alpha &&
	PIXMAN_FORMAT_RGB (data->mask->bits.format))
    {
	uint32_t *mask_buffer = dest_buffer + data->width;
	CombineFuncC compose = PIXMAN_COMPOSE_FUNCTIONS.combineC[data->op];
	if (!compose)
	    return;
	
	for (i = 0; i < data->height; ++i) {
	    /* fill first half of scanline with source */
	    if (fetchSrc)
	    {
		if (fetchMask)
		{
		    /* fetch mask before source so that fetching of
		       source can be optimized */
		    fetchMask (data->mask, data->xMask, data->yMask + i,
			       data->width, mask_buffer, 0, 0);
		    
		    if (maskClass == SOURCE_IMAGE_CLASS_HORIZONTAL)
			fetchMask = NULL;
		}
		
		if (srcClass == SOURCE_IMAGE_CLASS_HORIZONTAL)
		{
		    fetchSrc (data->src, data->xSrc, data->ySrc + i,
			      data->width, src_buffer, 0, 0);
		    fetchSrc = NULL;
		}
		else
		{
		    fetchSrc (data->src, data->xSrc, data->ySrc + i,
			      data->width, src_buffer, mask_buffer,
			      0xffffffff);
		}
	    }
	    else if (fetchMask)
	    {
		fetchMask (data->mask, data->xMask, data->yMask + i,
			   data->width, mask_buffer, 0, 0);
	    }
	    
	    if (store)
	    {
		/* fill dest into second half of scanline */
		if (fetchDest)
		    fetchDest (data->dest, data->xDest, data->yDest + i,
			       data->width, dest_buffer, 0, 0);
		
		/* blend */
		compose (dest_buffer, src_buffer, mask_buffer, data->width);
		
		/* write back */
		store (data->dest, data->xDest, data->yDest + i, data->width,
		       dest_buffer);
	    }
	    else
	    {
		/* blend */
		compose (bits + (data->yDest + i+ yoff) * stride +
			 data->xDest + xoff,
			 src_buffer, mask_buffer, data->width);
	    }
	}
    }
    else
    {
	uint32_t *src_mask_buffer = 0, *mask_buffer = 0;
	CombineFuncU compose = PIXMAN_COMPOSE_FUNCTIONS.combineU[data->op];
	if (!compose)
	    return;
	
	if (fetchMask)
	    mask_buffer = dest_buffer + data->width;
	
	for (i = 0; i < data->height; ++i) {
	    /* fill first half of scanline with source */
	    if (fetchSrc)
	    {
		if (fetchMask)
		{
		    /* fetch mask before source so that fetching of
		       source can be optimized */
		    fetchMask (data->mask, data->xMask, data->yMask + i,
			       data->width, mask_buffer, 0, 0);
		    
		    if (maskClass == SOURCE_IMAGE_CLASS_HORIZONTAL)
			fetchMask = NULL;
		}
		
		if (srcClass == SOURCE_IMAGE_CLASS_HORIZONTAL)
		{
		    fetchSrc (data->src, data->xSrc, data->ySrc + i,
			      data->width, src_buffer, 0, 0);
		    
		    if (mask_buffer)
		    {
			fbCombineInU (mask_buffer, src_buffer, data->width);
			src_mask_buffer = mask_buffer;
		    }
		    else
			src_mask_buffer = src_buffer;
		    
		    fetchSrc = NULL;
		}
		else
		{
		    fetchSrc (data->src, data->xSrc, data->ySrc + i,
			      data->width, src_buffer, mask_buffer,
			      0xff000000);
		    
		    if (mask_buffer)
			PIXMAN_COMPOSE_FUNCTIONS.combineMaskU (src_buffer,
							       mask_buffer,
							       data->width);
		    
		    src_mask_buffer = src_buffer;
		}
	    }
	    else if (fetchMask)
	    {
		fetchMask (data->mask, data->xMask, data->yMask + i,
			   data->width, mask_buffer, 0, 0);
		
		fbCombineInU (mask_buffer, src_buffer, data->width);
		
		src_mask_buffer = mask_buffer;
	    }
	    
	    if (store)
	    {
		/* fill dest into second half of scanline */
		if (fetchDest)
		    fetchDest (data->dest, data->xDest, data->yDest + i,
			       data->width, dest_buffer, 0, 0);
		
		/* blend */
		compose (dest_buffer, src_mask_buffer, data->width);
		
		/* write back */
		store (data->dest, data->xDest, data->yDest + i, data->width,
		       dest_buffer);
	    }
	    else
	    {
		/* blend */
		compose (bits + (data->yDest + i+ yoff) * stride +
			 data->xDest + xoff,
			 src_mask_buffer, data->width);
	    }
	}
    }
    
    if (!store)
	fbFinishAccess (data->dest->pDrawable);
}

#ifndef PIXMAN_FB_ACCESSORS

void
pixman_composite_rect_general (const FbComposeData *data,
			       uint32_t *scanline_buffer)
{
    if (data->src->common.read_func			||
	data->src->common.write_func			||
	(data->mask && data->mask->common.read_func)	||
	(data->mask && data->mask->common.write_func)	||
	data->dest->common.read_func			||
	data->dest->common.write_func)
    {
	pixman_composite_rect_general_accessors (data, scanline_buffer);
    }
    else
    {
	pixman_composite_rect_general_no_accessors (data, scanline_buffer);
    }
}

#endif
