/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2004 Calum Robinson
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is Calum Robinson
 *
 * Contributor(s):
 *    Calum Robinson <calumr@mac.com>
 */

#include "cairoint.h"
#include "cairo-quartz.h"

#pragma mark Types

typedef struct cairo_quartz_surface {
    cairo_surface_t	    base;
    
    CGContextRef	    context;

    int			    width;
    int			    height;
    
    cairo_image_surface_t   *image;
    
    CGImageRef		    cgImage;
} cairo_quartz_surface_t;





#pragma mark Private functions




void ImageDataReleaseFunc(void *info, const void *data, size_t size)
{
    if (data != NULL)
    {
        free((void *)data);
    }
}




#pragma mark Public functions





void
cairo_set_target_quartz_context(    cairo_t	    *cr,
				    CGContextRef    context,
				    int		    width,
				    int		    height)
{
    cairo_surface_t *surface;
    
    
    if (cr->status && cr->status != CAIRO_STATUS_NO_TARGET_SURFACE)
        return;
    
    surface = cairo_quartz_surface_create(context, width, height);
    if (surface == NULL)
    {
        cr->status = CAIRO_STATUS_NO_MEMORY;
        return;
    }
    
    cairo_set_target_surface(cr, surface);

    /* cairo_set_target_surface takes a reference, so we must destroy ours */
    cairo_surface_destroy(surface);
}


static cairo_surface_t *
_cairo_quartz_surface_create_similar(   void		*abstract_src,
					cairo_format_t  format,
					int		drawable,
					int		width,
					int		height)
{
    return NULL;
}


static void
_cairo_quartz_surface_destroy(void *abstract_surface)
{
    cairo_quartz_surface_t *surface = abstract_surface;
    
    
    if (surface->cgImage)
    {
	CGImageRelease(surface->cgImage);
    }
    

    free(surface);
}


static double
_cairo_quartz_surface_pixels_per_inch(void *abstract_surface)
{
    
    
    // TODO - get this from CGDirectDisplay somehow?
    return 96.0;
}


static cairo_image_surface_t *
_cairo_quartz_surface_get_image(void *abstract_surface)
{
    cairo_quartz_surface_t  *surface = abstract_surface;
    CGColorSpaceRef	    colorSpace;
    void		    *imageData;
    UInt32		    imageDataSize, rowBytes;
    CGDataProviderRef       dataProvider;
    
    
    // We keep a cached (cairo_image_surface_t *) in the cairo_quartz_surface_t
    // struct. If the window is ever drawn to without going through Cairo, then
    // we would need to refetch the pixel data from the window into the cached
    // image surface. 
    if (surface->image)
    {
        cairo_surface_reference(&surface->image->base);

        return surface->image;
    }
    
    colorSpace = CGColorSpaceCreateDeviceRGB();
    
    
    rowBytes = surface->width * 4;
    imageDataSize = rowBytes * surface->height;
    imageData = malloc(imageDataSize);
    
    dataProvider = CGDataProviderCreateWithData(NULL, imageData, imageDataSize, ImageDataReleaseFunc);
    
    surface->cgImage = CGImageCreate(   surface->width,
					surface->height,
					8,
					32, 
					rowBytes, 
					colorSpace, 
					kCGImageAlphaPremultipliedFirst, 
					dataProvider, 
					NULL, 
					false, 
					kCGRenderingIntentDefault);
    
    
    CGColorSpaceRelease(colorSpace);
    CGDataProviderRelease(dataProvider);
    
    
    surface->image = (cairo_image_surface_t *)
			cairo_image_surface_create_for_data(    imageData,
								CAIRO_FORMAT_ARGB32,
								surface->width, 
								surface->height,
								rowBytes);
    
    
    // Set the image surface Cairo state to match our own. 
    _cairo_image_surface_set_repeat(surface->image, surface->base.repeat);
    _cairo_image_surface_set_matrix(surface->image, &(surface->base.matrix));
    

    return surface->image;
}


static cairo_status_t
_cairo_quartz_surface_set_image(    void		    *abstract_surface,
				    cairo_image_surface_t   *image)
{
    cairo_quartz_surface_t  *surface = abstract_surface;
    cairo_status_t	    status;
    
    
    if (surface->image == image)
    {
	CGRect	rect;
	
	
	rect = CGRectMake(0, 0, surface->width, surface->height);
	
	CGContextDrawImage(surface->context, rect, surface->cgImage);
	
	
	status = CAIRO_STATUS_SUCCESS;
    }
    else
    {
	// TODO - set_image from something other than what we returned from get_image
	status = CAIRO_STATUS_NO_TARGET_SURFACE;
    }
    
    
    return status;
}


static cairo_status_t
_cairo_quartz_surface_set_matrix(void *abstract_surface, cairo_matrix_t *matrix)
{
    cairo_quartz_surface_t *surface = abstract_surface;

    return _cairo_image_surface_set_matrix(surface->image, matrix);
}


static cairo_status_t
_cairo_quartz_surface_set_filter(void *abstract_surface, cairo_filter_t filter)
{
    cairo_quartz_surface_t *surface = abstract_surface;

    return _cairo_image_surface_set_filter(surface->image, filter);
}


static cairo_status_t
_cairo_quartz_surface_set_repeat(void *abstract_surface, int repeat)
{
    cairo_quartz_surface_t *surface = abstract_surface;

    return _cairo_image_surface_set_repeat(surface->image, repeat);
}


static cairo_int_status_t
_cairo_quartz_surface_composite(    cairo_operator_t    operator,
				    cairo_surface_t     *generic_src,
				    cairo_surface_t     *generic_mask,
				    void		*abstract_dst,
				    int			src_x,
				    int			src_y,
				    int			mask_x,
				    int			mask_y,
				    int			dst_x,
				    int			dst_y,
				    unsigned int	width,
				    unsigned int	height)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}


static cairo_int_status_t
_cairo_quartz_surface_fill_rectangles(  void		    *abstract_surface,
					cairo_operator_t    operator,
					const cairo_color_t *color,
					cairo_rectangle_t   *rects,
					int		    num_rects)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}


static cairo_int_status_t
_cairo_quartz_surface_composite_trapezoids( cairo_operator_t    operator,
					    cairo_surface_t     *generic_src,
					    void		*abstract_dst,
					    int			xSrc,
					    int			ySrc,
					    cairo_trapezoid_t   *traps,
					    int			num_traps)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}


static cairo_int_status_t
_cairo_quartz_surface_copy_page(void *abstract_surface)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}


static cairo_int_status_t
_cairo_quartz_surface_show_page(void *abstract_surface)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}


static cairo_int_status_t
_cairo_quartz_surface_set_clip_region(  void		    *abstract_surface,
					pixman_region16_t   *region)
{
    cairo_quartz_surface_t *surface = abstract_surface;

    return _cairo_image_surface_set_clip_region(surface->image, region);
}


static cairo_int_status_t
_cairo_quartz_surface_create_pattern(   void		*abstract_surface,
					cairo_pattern_t *pattern,
					cairo_box_t     *extents)
{
    return CAIRO_INT_STATUS_UNSUPPORTED;
}


static const struct _cairo_surface_backend cairo_quartz_surface_backend = {
    _cairo_quartz_surface_create_similar,
    _cairo_quartz_surface_destroy,
    _cairo_quartz_surface_pixels_per_inch,
    _cairo_quartz_surface_get_image,
    _cairo_quartz_surface_set_image,
    _cairo_quartz_surface_set_matrix,
    _cairo_quartz_surface_set_filter,
    _cairo_quartz_surface_set_repeat,
    _cairo_quartz_surface_composite,
    _cairo_quartz_surface_fill_rectangles,
    _cairo_quartz_surface_composite_trapezoids,
    _cairo_quartz_surface_copy_page,
    _cairo_quartz_surface_show_page,
    _cairo_quartz_surface_set_clip_region,
    _cairo_quartz_surface_create_pattern
};


cairo_surface_t *
cairo_quartz_surface_create(	CGContextRef    context,
				int		width,
				int		height)
{
    cairo_quartz_surface_t *surface;
    
    
    surface = malloc(sizeof(cairo_quartz_surface_t));
    if (surface == NULL)
        return NULL;
    
    _cairo_surface_init(&surface->base, &cairo_quartz_surface_backend);
    
    
    surface->context	    = context;
    
    surface->width	    = width;
    surface->height	    = height;
    
    surface->image	    = NULL;
    
    surface->cgImage	    = NULL;
    
    
    // Set up the image surface which Cairo draws into and we blit to & from. 
    surface->image	    = _cairo_quartz_surface_get_image(surface);
    
    
    return (cairo_surface_t *)surface;
}


DEPRECATE (cairo_surface_create_for_drawable, cairo_quartz_surface_create);
