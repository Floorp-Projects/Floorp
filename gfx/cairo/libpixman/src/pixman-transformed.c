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

#include "pixman-private.h"

#define Alpha(x) ((x) >> 24)
#define Red(x) (((x) >> 16) & 0xff)
#define Green(x) (((x) >> 8) & 0xff)
#define Blue(x) ((x) & 0xff)

#define Alpha64(x) ((x) >> 48)
#define Red64(x) (((x) >> 32) & 0xffff)
#define Green64(x) (((x) >> 16) & 0xffff)
#define Blue64(x) ((x) & 0xffff)

/*
 * Fetch from region strategies
 */
typedef FASTCALL uint32_t (*fetchFromRegionProc)(bits_image_t *pict, int x, int y, uint32_t *buffer, fetchPixelProc32 fetch, pixman_box32_t *box);

/*
 * There are two properties we can make use of when fetching pixels
 *
 * (a) Is the source clip just the image itself?
 *
 * (b) Do we know the coordinates of the pixel to fetch are
 *     within the image boundaries;
 *
 * Source clips are almost never used, so the important case to optimize
 * for is when src_clip is false. Since inside_bounds is statically known,
 * the last part of the if statement will normally be optimized away.
 */
static force_inline uint32_t
do_fetch (bits_image_t *pict, int x, int y, fetchPixelProc32 fetch,
	  pixman_bool_t src_clip,
	  pixman_bool_t inside_bounds)
{
    if (src_clip)
    {
	if (pixman_region32_contains_point (pict->common.src_clip, x, y,NULL))
	    return fetch (pict, x, y);
	else
	    return 0;
    }
    else if (inside_bounds)
    {
	return fetch (pict, x, y);
    }
    else
    {
	if (x >= 0 && x < pict->width && y >= 0 && y < pict->height)
	    return fetch (pict, x, y);
	else
	    return 0;
    }
}

/*
 * Fetching Algorithms
 */
static inline uint32_t
fetch_nearest (bits_image_t		*pict,
	       fetchPixelProc32		 fetch,
	       pixman_bool_t		 affine,
	       pixman_repeat_t		 repeat,
	       pixman_bool_t             has_src_clip,
	       const pixman_vector_t    *v)
{
    if (!v->vector[2])
    {
	return 0;
    }
    else
    {
	int x, y;
	pixman_bool_t inside_bounds;

	if (!affine)
	{
	    x = DIV(v->vector[0], v->vector[2]);
	    y = DIV(v->vector[1], v->vector[2]);
	}
	else
	{
	    x = v->vector[0]>>16;
	    y = v->vector[1]>>16;
	}

	switch (repeat)
	{
	case PIXMAN_REPEAT_NORMAL:
	    x = MOD (x, pict->width);
	    y = MOD (y, pict->height);
	    inside_bounds = TRUE;
	    break;
	    
	case PIXMAN_REPEAT_PAD:
	    x = CLIP (x, 0, pict->width-1);
	    y = CLIP (y, 0, pict->height-1);
	    inside_bounds = TRUE;
	    break;
	    
	case PIXMAN_REPEAT_REFLECT: /* FIXME: this should be implemented for images */
	case PIXMAN_REPEAT_NONE:
	    inside_bounds = FALSE;
	    break;

	default:
	    return 0;
	}

	return do_fetch (pict, x, y, fetch, has_src_clip, inside_bounds);
    }
}

static inline uint32_t
fetch_bilinear (bits_image_t		*pict,
		fetchPixelProc32	 fetch,
		pixman_bool_t		 affine,
		pixman_repeat_t		 repeat,
		pixman_bool_t		 has_src_clip,
		const pixman_vector_t   *v)
{
    if (!v->vector[2])
    {
	return 0;
    }
    else
    {
	int x1, x2, y1, y2, distx, idistx, disty, idisty;
	uint32_t tl, tr, bl, br, r;
	uint32_t ft, fb;
	pixman_bool_t inside_bounds;
	
	if (!affine)
	{
	    pixman_fixed_48_16_t div;
	    div = ((pixman_fixed_48_16_t)v->vector[0] << 16)/v->vector[2];
	    x1 = div >> 16;
	    distx = ((pixman_fixed_t)div >> 8) & 0xff;
	    div = ((pixman_fixed_48_16_t)v->vector[1] << 16)/v->vector[2];
	    y1 = div >> 16;
	    disty = ((pixman_fixed_t)div >> 8) & 0xff;
	}
	else
	{
	    x1 = v->vector[0] >> 16;
	    distx = (v->vector[0] >> 8) & 0xff;
	    y1 = v->vector[1] >> 16;
	    disty = (v->vector[1] >> 8) & 0xff;
	}
	x2 = x1 + 1;
	y2 = y1 + 1;
	
	idistx = 256 - distx;
	idisty = 256 - disty;

	switch (repeat)
	{
	case PIXMAN_REPEAT_NORMAL:
	    x1 = MOD (x1, pict->width);
	    x2 = MOD (x2, pict->width);
	    y1 = MOD (y1, pict->height);
	    y2 = MOD (y2, pict->height);
	    inside_bounds = TRUE;
	    break;
	    
	case PIXMAN_REPEAT_PAD:
	    x1 = CLIP (x1, 0, pict->width-1);
	    x2 = CLIP (x2, 0, pict->width-1);
	    y1 = CLIP (y1, 0, pict->height-1);
	    y2 = CLIP (y2, 0, pict->height-1);
	    inside_bounds = TRUE;
	    break;
	    
	case PIXMAN_REPEAT_REFLECT: /* FIXME: this should be implemented for images */
	case PIXMAN_REPEAT_NONE:
	    inside_bounds = FALSE;
	    break;

	default:
	    return 0;
	}
	
	tl = do_fetch(pict, x1, y1, fetch, has_src_clip, inside_bounds);
	tr = do_fetch(pict, x2, y1, fetch, has_src_clip, inside_bounds);
	bl = do_fetch(pict, x1, y2, fetch, has_src_clip, inside_bounds);
	br = do_fetch(pict, x2, y2, fetch, has_src_clip, inside_bounds);
	
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

	return r;
    }
}

static void
fbFetchTransformed_Convolution(bits_image_t * pict, int width, uint32_t *buffer, uint32_t *mask, uint32_t maskBits,
			       pixman_bool_t affine, pixman_vector_t v, pixman_vector_t unit)
{
    fetchPixelProc32 fetch;
    int i;

    pixman_fixed_t *params = pict->common.filter_params;
    int32_t cwidth = pixman_fixed_to_int(params[0]);
    int32_t cheight = pixman_fixed_to_int(params[1]);
    int xoff = (params[0] - pixman_fixed_1) >> 1;
    int yoff = (params[1] - pixman_fixed_1) >> 1;
    fetch = ACCESS(pixman_fetchPixelProcForPicture32)(pict);

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
                    int ty;
                    switch (pict->common.repeat) {
                        case PIXMAN_REPEAT_NORMAL:
                            ty = MOD (y, pict->height);
                            break;
                        case PIXMAN_REPEAT_PAD:
                            ty = CLIP (y, 0, pict->height-1);
                            break;
                        default:
                            ty = y;
                    }
                    for (x = x1; x < x2; x++) {
                        if (*p) {
                            int tx;
                            switch (pict->common.repeat) {
                                case PIXMAN_REPEAT_NORMAL:
                                    tx = MOD (x, pict->width);
                                    break;
                                case PIXMAN_REPEAT_PAD:
                                    tx = CLIP (x, 0, pict->width-1);
                                    break;
                                default:
                                    tx = x;
                            }
                            if (pixman_region32_contains_point (pict->common.src_clip, tx, ty, NULL)) {
                                uint32_t c = fetch(pict, tx, ty);

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

static void
adjust (pixman_vector_t *v, pixman_vector_t *u, pixman_fixed_t adjustment)
{
    int delta_v = (adjustment * v->vector[2]) >> 16;
    int delta_u = (adjustment * u->vector[2]) >> 16;
    
    v->vector[0] += delta_v;
    v->vector[1] += delta_v;
    
    u->vector[0] += delta_u;
    u->vector[1] += delta_u;
}

void
ACCESS(fbFetchTransformed)(bits_image_t * pict, int x, int y, int width,
                           uint32_t *buffer, uint32_t *mask, uint32_t maskBits)
{
    uint32_t     *bits;
    int32_t    stride;
    pixman_vector_t v;
    pixman_vector_t unit;
    pixman_bool_t affine = TRUE;

    bits = pict->bits;
    stride = pict->rowstride;

    /* reference point is the center of the pixel */
    v.vector[0] = pixman_int_to_fixed(x) + pixman_fixed_1 / 2;
    v.vector[1] = pixman_int_to_fixed(y) + pixman_fixed_1 / 2;
    v.vector[2] = pixman_fixed_1;

    /* when using convolution filters or PIXMAN_REPEAT_PAD one might get here without a transform */
    if (pict->common.transform)
    {
        if (!pixman_transform_point_3d (pict->common.transform, &v))
            return;
        unit.vector[0] = pict->common.transform->matrix[0][0];
        unit.vector[1] = pict->common.transform->matrix[1][0];
        unit.vector[2] = pict->common.transform->matrix[2][0];

        affine = (v.vector[2] == pixman_fixed_1 && unit.vector[2] == 0);
    }
    else
    {
        unit.vector[0] = pixman_fixed_1;
        unit.vector[1] = 0;
        unit.vector[2] = 0;
    }

    /* This allows filtering code to pretend that pixels are located at integer coordinates */
    if (pict->common.filter == PIXMAN_FILTER_NEAREST || pict->common.filter == PIXMAN_FILTER_FAST)
    {
	fetchPixelProc32   fetch;
	pixman_bool_t src_clip;
	int i;

	/* Round down to closest integer, ensuring that 0.5 rounds to 0, not 1 */
	adjust (&v, &unit, - pixman_fixed_e);

	fetch = ACCESS(pixman_fetchPixelProcForPicture32)(pict);
	
	src_clip = pict->common.src_clip != &(pict->common.full_region);
	
	for ( i = 0; i < width; ++i)
	{
	    if (!mask || mask[i] & maskBits)
		*(buffer + i) = fetch_nearest (pict, fetch, affine, pict->common.repeat, src_clip, &v);
	    
	    v.vector[0] += unit.vector[0];
	    v.vector[1] += unit.vector[1];
	    v.vector[2] += unit.vector[2];
	}
    }
    else if (pict->common.filter == PIXMAN_FILTER_BILINEAR	||
	       pict->common.filter == PIXMAN_FILTER_GOOD	||
	       pict->common.filter == PIXMAN_FILTER_BEST)
    {
	pixman_bool_t src_clip;
	fetchPixelProc32   fetch;
	int i;

	/* Let the bilinear code pretend that pixels fall on integer coordinaters */
	adjust (&v, &unit, -(pixman_fixed_1 / 2));

	fetch = ACCESS(pixman_fetchPixelProcForPicture32)(pict);
	src_clip = pict->common.src_clip != &(pict->common.full_region);
	
	for (i = 0; i < width; ++i)
	{
	    if (!mask || mask[i] & maskBits)
		*(buffer + i) = fetch_bilinear (pict, fetch, affine, pict->common.repeat, src_clip, &v);
	    
	    v.vector[0] += unit.vector[0];
	    v.vector[1] += unit.vector[1];
	    v.vector[2] += unit.vector[2];
	}
    }
    else if (pict->common.filter == PIXMAN_FILTER_CONVOLUTION)
    {
	/* Round to closest integer, ensuring that 0.5 rounds to 0, not 1 */
	adjust (&v, &unit, - pixman_fixed_e);
	
        fbFetchTransformed_Convolution(pict, width, buffer, mask, maskBits, affine, v, unit);
    }
}

void
ACCESS(fbFetchTransformed64)(bits_image_t * pict, int x, int y, int width,
                             uint64_t *buffer, uint64_t *mask, uint32_t maskBits)
{
    // TODO: Don't lose precision for wide pictures!
    uint32_t *mask8 = NULL;

    // Contract the mask image, if one exists, so that the 32-bit fetch function
    // can use it.
    if (mask) {
        mask8 = pixman_malloc_ab(width, sizeof(uint32_t));
        pixman_contract(mask8, mask, width);
    }

    // Fetch the image into the first half of buffer.
    ACCESS(fbFetchTransformed)(pict, x, y, width, (uint32_t*)buffer, mask8,
                               maskBits);

    // Expand from 32bpp to 64bpp in place.
    pixman_expand(buffer, (uint32_t*)buffer, PIXMAN_a8r8g8b8, width);

    free(mask8);
}

#define SCANLINE_BUFFER_LENGTH 2048

void
ACCESS(fbFetchExternalAlpha)(bits_image_t * pict, int x, int y, int width,
                             uint32_t *buffer, uint32_t *mask,
                             uint32_t maskBits)
{
    int i;
    uint32_t _alpha_buffer[SCANLINE_BUFFER_LENGTH];
    uint32_t *alpha_buffer = _alpha_buffer;

    if (!pict->common.alpha_map) {
        ACCESS(fbFetchTransformed) (pict, x, y, width, buffer, mask, maskBits);
	return;
    }
    if (width > SCANLINE_BUFFER_LENGTH)
        alpha_buffer = (uint32_t *) pixman_malloc_ab (width, sizeof(uint32_t));

    ACCESS(fbFetchTransformed)(pict, x, y, width, buffer, mask, maskBits);
    ACCESS(fbFetchTransformed)((bits_image_t *)pict->common.alpha_map, x - pict->common.alpha_origin.x,
                               y - pict->common.alpha_origin.y, width,
                               alpha_buffer, mask, maskBits);
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

void
ACCESS(fbFetchExternalAlpha64)(bits_image_t * pict, int x, int y, int width,
                               uint64_t *buffer, uint64_t *mask,
                               uint32_t maskBits)
{
    int i;
    uint64_t _alpha_buffer[SCANLINE_BUFFER_LENGTH];
    uint64_t *alpha_buffer = _alpha_buffer;
    uint64_t maskBits64;

    if (!pict->common.alpha_map) {
        ACCESS(fbFetchTransformed64) (pict, x, y, width, buffer, mask, maskBits);
	return;
    }
    if (width > SCANLINE_BUFFER_LENGTH)
        alpha_buffer = (uint64_t *) pixman_malloc_ab (width, sizeof(uint64_t));

    ACCESS(fbFetchTransformed64)(pict, x, y, width, buffer, mask, maskBits);
    ACCESS(fbFetchTransformed64)((bits_image_t *)pict->common.alpha_map, x - pict->common.alpha_origin.x,
                                 y - pict->common.alpha_origin.y, width,
                                 alpha_buffer, mask, maskBits);

    pixman_expand(&maskBits64, &maskBits, PIXMAN_a8r8g8b8, 1);

    for (i = 0; i < width; ++i) {
        if (!mask || mask[i] & maskBits64)
	{
	    int64_t a = alpha_buffer[i]>>48;
	    *(buffer + i) = (a << 48)
		| (div_65535(Red64(*(buffer + i)) * a) << 32)
		| (div_65535(Green64(*(buffer + i)) * a) << 16)
		| (div_65535(Blue64(*(buffer + i)) * a));
	}
    }

    if (alpha_buffer != _alpha_buffer)
        free(alpha_buffer);
}

void
ACCESS(fbStoreExternalAlpha)(bits_image_t * pict, int x, int y, int width,
                             uint32_t *buffer)
{
    uint32_t *bits, *alpha_bits;
    int32_t stride, astride;
    int ax, ay;
    storeProc32 store;
    storeProc32 astore;
    const pixman_indexed_t * indexed = pict->indexed;
    const pixman_indexed_t * aindexed;

    if (!pict->common.alpha_map) {
        // XXX[AGP]: This should never happen!
        // fbStore(pict, x, y, width, buffer);
        abort();
	return;
    }

    store = ACCESS(pixman_storeProcForPicture32)(pict);
    astore = ACCESS(pixman_storeProcForPicture32)(pict->common.alpha_map);
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
}

void
ACCESS(fbStoreExternalAlpha64)(bits_image_t * pict, int x, int y, int width,
                               uint64_t *buffer)
{
    uint32_t *bits, *alpha_bits;
    int32_t stride, astride;
    int ax, ay;
    storeProc64 store;
    storeProc64 astore;
    const pixman_indexed_t * indexed = pict->indexed;
    const pixman_indexed_t * aindexed;

    store = ACCESS(pixman_storeProcForPicture64)(pict);
    astore = ACCESS(pixman_storeProcForPicture64)(pict->common.alpha_map);
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
}
