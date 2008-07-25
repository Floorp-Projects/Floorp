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
#include <math.h>
#include <assert.h>
#include <limits.h>

#include "pixman-private.h"

#ifdef PIXMAN_FB_ACCESSORS
#define PIXMAN_COMPOSITE_RECT_GENERAL pixman_composite_rect_general_accessors
#else
#define PIXMAN_COMPOSITE_RECT_GENERAL pixman_composite_rect_general_no_accessors
#endif

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

static void fbFetchSolid(bits_image_t * pict, int x, int y, int width, uint32_t *buffer, uint32_t *mask, uint32_t maskBits)
{
    uint32_t color;
    uint32_t *end;
    fetchPixelProc32 fetch = ACCESS(pixman_fetchPixelProcForPicture32)(pict);

    color = fetch(pict, 0, 0);

    end = buffer + width;
    while (buffer < end)
	*(buffer++) = color;
}

static void fbFetchSolid64(bits_image_t * pict, int x, int y, int width, uint64_t *buffer, void *unused, uint32_t unused2)
{
    uint64_t color;
    uint64_t *end;
    fetchPixelProc64 fetch = ACCESS(pixman_fetchPixelProcForPicture64)(pict);

    color = fetch(pict, 0, 0);

    end = buffer + width;
    while (buffer < end)
	*(buffer++) = color;
}

static void fbFetch(bits_image_t * pict, int x, int y, int width, uint32_t *buffer, uint32_t *mask, uint32_t maskBits)
{
    fetchProc32 fetch = ACCESS(pixman_fetchProcForPicture32)(pict);

    fetch(pict, x, y, width, buffer);
}

static void fbFetch64(bits_image_t * pict, int x, int y, int width, uint64_t *buffer, void *unused, uint32_t unused2)
{
    fetchProc64 fetch = ACCESS(pixman_fetchProcForPicture64)(pict);

    fetch(pict, x, y, width, buffer);
}

static void
fbStore(bits_image_t * pict, int x, int y, int width, uint32_t *buffer)
{
    uint32_t *bits;
    int32_t stride;
    storeProc32 store = ACCESS(pixman_storeProcForPicture32)(pict);
    const pixman_indexed_t * indexed = pict->indexed;

    bits = pict->bits;
    stride = pict->rowstride;
    bits += y*stride;
    store((pixman_image_t *)pict, bits, buffer, x, width, indexed);
}

static void
fbStore64(bits_image_t * pict, int x, int y, int width, uint64_t *buffer)
{
    uint32_t *bits;
    int32_t stride;
    storeProc64 store = ACCESS(pixman_storeProcForPicture64)(pict);
    const pixman_indexed_t * indexed = pict->indexed;

    bits = pict->bits;
    stride = pict->rowstride;
    bits += y*stride;
    store((pixman_image_t *)pict, bits, buffer, x, width, indexed);
}

typedef void (*scanStoreProc)(pixman_image_t *, int, int, int, uint32_t *);
typedef void (*scanFetchProc)(pixman_image_t *, int, int, int, uint32_t *,
			      uint32_t *, uint32_t);

static inline scanFetchProc get_fetch_source_pict(const int wide)
{
    if (wide)
	return (scanFetchProc)pixmanFetchSourcePict64;
    else
	return (scanFetchProc)pixmanFetchSourcePict;
}

static inline scanFetchProc get_fetch_solid(const int wide)
{
    if (wide)
	return (scanFetchProc)fbFetchSolid64;
    else
	return (scanFetchProc)fbFetchSolid;
}

static inline scanFetchProc get_fetch(const int wide)
{
    if (wide)
	return (scanFetchProc)fbFetch64;
    else
	return (scanFetchProc)fbFetch;
}

static inline scanFetchProc get_fetch_external_alpha(const int wide)
{
    if (wide)
	return (scanFetchProc)ACCESS(fbFetchExternalAlpha64);
    else
	return (scanFetchProc)ACCESS(fbFetchExternalAlpha);
}

static inline scanFetchProc get_fetch_transformed(const int wide)
{
    if (wide)
	return (scanFetchProc)ACCESS(fbFetchTransformed64);
    else
	return (scanFetchProc)ACCESS(fbFetchTransformed);
}

static inline scanStoreProc get_store(const int wide)
{
    if (wide)
	return (scanStoreProc)fbStore64;
    else
	return (scanStoreProc)fbStore;
}

static inline scanStoreProc get_store_external_alpha(const int wide)
{
    if (wide)
	return (scanStoreProc)ACCESS(fbStoreExternalAlpha64);
    else
	return (scanStoreProc)ACCESS(fbStoreExternalAlpha);
}

#ifndef PIXMAN_FB_ACCESSORS
static
#endif
void
PIXMAN_COMPOSITE_RECT_GENERAL (const FbComposeData *data,
			       void *src_buffer, void *mask_buffer, 
			       void *dest_buffer, const int wide)
{
    int i;
    scanStoreProc store;
    scanFetchProc fetchSrc = NULL, fetchMask = NULL, fetchDest = NULL;
    unsigned int srcClass = SOURCE_IMAGE_CLASS_UNKNOWN;
    unsigned int maskClass = SOURCE_IMAGE_CLASS_UNKNOWN;
    uint32_t *bits;
    int32_t stride;
    int xoff, yoff;

    if (data->op == PIXMAN_OP_CLEAR)
        fetchSrc = NULL;
    else if (IS_SOURCE_IMAGE (data->src))
    {
	fetchSrc = get_fetch_source_pict(wide);
	srcClass = SourcePictureClassify ((source_image_t *)data->src,
					  data->xSrc, data->ySrc,
					  data->width, data->height);
    }
    else
    {
	bits_image_t *bits = (bits_image_t *)data->src;

	if (bits->common.alpha_map)
	{
	    fetchSrc = get_fetch_external_alpha(wide);
	}
	else if ((bits->common.repeat == PIXMAN_REPEAT_NORMAL || bits->common.repeat == PIXMAN_REPEAT_PAD) &&
		 bits->width == 1 &&
		 bits->height == 1)
	{
	    fetchSrc = get_fetch_solid(wide);
	    srcClass = SOURCE_IMAGE_CLASS_HORIZONTAL;
	}
	else if (!bits->common.transform && bits->common.filter != PIXMAN_FILTER_CONVOLUTION
                && bits->common.repeat != PIXMAN_REPEAT_PAD)
	{
	    fetchSrc = get_fetch(wide);
	}
	else
	{
	    fetchSrc = get_fetch_transformed(wide);
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
		fetchMask = get_fetch_external_alpha(wide);
	    }
	    else if ((bits->common.repeat == PIXMAN_REPEAT_NORMAL || bits->common.repeat == PIXMAN_REPEAT_PAD) &&
		     bits->width == 1 && bits->height == 1)
	    {
		fetchMask = get_fetch_solid(wide);
		maskClass = SOURCE_IMAGE_CLASS_HORIZONTAL;
	    }
	    else if (!bits->common.transform && bits->common.filter != PIXMAN_FILTER_CONVOLUTION
                    && bits->common.repeat != PIXMAN_REPEAT_PAD)
		fetchMask = get_fetch(wide);
	    else
		fetchMask = get_fetch_transformed(wide);
	}
    }

    if (data->dest->common.alpha_map)
    {
	fetchDest = get_fetch_external_alpha(wide);
	store = get_store_external_alpha(wide);

	if (data->op == PIXMAN_OP_CLEAR || data->op == PIXMAN_OP_SRC)
	    fetchDest = NULL;
    }
    else
    {
	fetchDest = get_fetch(wide);
	store = get_store(wide);

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
		// Skip the store step and composite directly into the
		// destination if the output format of the compose func matches
		// the destination format.
		if (!wide)
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
	CombineFuncC32 compose =
	    wide ? (CombineFuncC32)pixman_composeFunctions64.combineC[data->op] :
		   pixman_composeFunctions.combineC[data->op];
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
	void *src_mask_buffer = 0;
	const int useMask = (fetchMask != NULL);
	CombineFuncU32 compose =
	    wide ? (CombineFuncU32)pixman_composeFunctions64.combineU[data->op] :
		   pixman_composeFunctions.combineU[data->op];
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

		    if (useMask)
		    {
			if (wide)
			    pixman_composeFunctions64.combineU[PIXMAN_OP_IN] (mask_buffer, src_buffer, data->width);
			else
			    pixman_composeFunctions.combineU[PIXMAN_OP_IN] (mask_buffer, src_buffer, data->width);

			src_mask_buffer = mask_buffer;
		    }
		    else
			src_mask_buffer = src_buffer;

		    fetchSrc = NULL;
		}
		else
		{
		    fetchSrc (data->src, data->xSrc, data->ySrc + i,
			      data->width, src_buffer,
			      useMask ? mask_buffer : NULL, 0xff000000);

		    if (useMask) {
			if (wide)
			    pixman_composeFunctions64.combineMaskU (src_buffer,
								    mask_buffer,
								    data->width);
			else
			    pixman_composeFunctions.combineMaskU (src_buffer,
								  mask_buffer,
								  data->width);
		    }

		    src_mask_buffer = src_buffer;
		}
	    }
	    else if (fetchMask)
	    {
		fetchMask (data->mask, data->xMask, data->yMask + i,
			   data->width, mask_buffer, 0, 0);

		if (wide)
		    pixman_composeFunctions64.combineU[PIXMAN_OP_IN] (mask_buffer, src_buffer, data->width);
		else
		    pixman_composeFunctions.combineU[PIXMAN_OP_IN] (mask_buffer, src_buffer, data->width);

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
}

#ifndef PIXMAN_FB_ACCESSORS

#define SCANLINE_BUFFER_LENGTH 2048

void
pixman_composite_rect_general (const FbComposeData *data)
{
    uint32_t _scanline_buffer[SCANLINE_BUFFER_LENGTH * 3];
    const pixman_format_code_t srcFormat = data->src->type == BITS ? data->src->bits.format : 0;
    const pixman_format_code_t maskFormat = data->mask && data->mask->type == BITS ? data->mask->bits.format : 0;
    const pixman_format_code_t destFormat = data->dest->type == BITS ? data->dest->bits.format : 0;
    const int srcWide = PIXMAN_FORMAT_16BPC(srcFormat);
    const int maskWide = data->mask && PIXMAN_FORMAT_16BPC(maskFormat);
    const int destWide = PIXMAN_FORMAT_16BPC(destFormat);
    const int wide = srcWide || maskWide || destWide;
    const int Bpp = wide ? 8 : 4;
    uint8_t *scanline_buffer = (uint8_t*)_scanline_buffer;
    uint8_t *src_buffer, *mask_buffer, *dest_buffer;

    if (data->width * Bpp > SCANLINE_BUFFER_LENGTH * sizeof(uint32_t))
    {
	scanline_buffer = pixman_malloc_abc (data->width, 3, Bpp);

	if (!scanline_buffer)
	    return;
    }

    src_buffer = scanline_buffer;
    mask_buffer = src_buffer + data->width * Bpp;
    dest_buffer = mask_buffer + data->width * Bpp;

    if (data->src->common.read_func			||
	data->src->common.write_func			||
	(data->mask && data->mask->common.read_func)	||
	(data->mask && data->mask->common.write_func)	||
	data->dest->common.read_func			||
	data->dest->common.write_func)
    {
	pixman_composite_rect_general_accessors (data, src_buffer, mask_buffer,
						 dest_buffer, wide);
    }
    else
    {
	pixman_composite_rect_general_no_accessors (data, src_buffer,
						    mask_buffer, dest_buffer,
						    wide);
    }

    if ((void*)scanline_buffer != (void*)_scanline_buffer)
	free (scanline_buffer);
}

#endif
