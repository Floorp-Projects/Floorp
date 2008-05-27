/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2003 University of Southern California
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
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *    Michael Emmel <mike.emmel@gmail.com>
 *    Claudio Ciccani <klan@users.sf.net>
 */
 

#include "cairoint.h"
#include "cairo-directfb.h"

#include <stdlib.h>

#include <directfb.h>
#include <direct/types.h>
#include <direct/debug.h>
#include <direct/memcpy.h>
#include <direct/util.h>


/*
 * Rectangle works fine.
 * Bugs 361377, 359553, 359243 in Gnome BTS are caused
 * by GDK/DirectFB, not by Cairo/DirectFB.
 */
#define DFB_RECTANGLES 1

/*
 * Composite works fine.
 */
#define DFB_COMPOSITE 1 

/*
 * CompositeTrapezoids works (without antialiasing).
 */
#define DFB_COMPOSITE_TRAPEZOIDS 0

/*
 * ShowGlyphs works fine.
 */
#define DFB_SHOW_GLYPHS 1


D_DEBUG_DOMAIN (Cairo_DirectFB, "Cairo/DirectFB", "Cairo DirectFB backend");

/*****************************************************************************/


typedef struct _cairo_directfb_surface {
    cairo_surface_t      base;
    cairo_format_t       format;
    cairo_content_t      content;
    
    IDirectFB           *dfb;
    IDirectFBSurface    *dfbsurface;
    IDirectFBSurface    *tmpsurface;
    
    /* color buffer */
    cairo_surface_t     *color;
        
    DFBRegion           *clips;
    int                  n_clips;
    
    int                  width;
    int                  height;
    
    cairo_bool_t         local;
} cairo_directfb_surface_t;


typedef struct _cairo_directfb_font_cache {
    IDirectFB           *dfb; 
    IDirectFBSurface    *dfbsurface;

    int                  width;
    int                  height;

    /* coordinates within the surface 
     * of the last loaded glyph */
    int                  x;
    int                  y;
} cairo_directfb_font_cache_t;
    

static cairo_surface_backend_t cairo_directfb_surface_backend;

/*****************************************************************************/

static int _directfb_argb_font = 0;

/*****************************************************************************/


#define RUN_CLIPPED( surface, clip, func ) {\
    if ((surface)->clips) {\
        int k;\
        for (k = 0; k < (surface)->n_clips; k++) {\
            if (clip) {\
                DFBRegion  reg = (surface)->clips[k];\
                DFBRegion *cli = (clip);\
                if (reg.x2 < cli->x1 || reg.y2 < cli->y1 ||\
                    reg.x1 > cli->x2 || reg.y1 > cli->y2)\
                    continue;\
                if (reg.x1 < cli->x1)\
                    reg.x1 = cli->x1;\
                if (reg.y1 < cli->y1)\
                    reg.y1 = cli->y1;\
                if (reg.x2 > cli->x2)\
                    reg.x2 = cli->x2;\
                if (reg.y2 > cli->y2)\
                    reg.y2 = cli->y2;\
                (surface)->dfbsurface->SetClip ((surface)->dfbsurface, &reg);\
            }\
            else {\
                (surface)->dfbsurface->SetClip ((surface)->dfbsurface,\
                                           &(surface)->clips[k]);\
            }\
            func;\
        }\
    }\
    else {\
        (surface)->dfbsurface->SetClip ((surface)->dfbsurface, clip);\
        func;\
    }\
}

#define TRANSFORM_POINT2X( m, x, y, ret_x, ret_y ) {\
    double _x = (x);\
    double _y = (y);\
    (ret_x) = (_x * (m).xx + (m).x0);\
    (ret_y) = (_y * (m).yy + (m).y0);\
}

#define TRANSFORM_POINT3X( m, x, y, ret_x, ret_y ) {\
    double _x = (x);\
    double _y = (y);\
    (ret_x) = (_x * (m).xx + _y * (m).xy + (m).x0);\
    (ret_y) = (_x * (m).yx + _y * (m).yy + (m).y0);\
}

/* XXX: A1 has a different bits ordering in cairo.
 *      Probably we should drop it.
 */

static cairo_content_t 
_directfb_format_to_content (DFBSurfacePixelFormat format)
{
    if (DFB_PIXELFORMAT_HAS_ALPHA(format)) {
        if (DFB_COLOR_BITS_PER_PIXEL(format))
            return CAIRO_CONTENT_COLOR_ALPHA;
            
        return CAIRO_CONTENT_ALPHA;
    }
    
    return CAIRO_CONTENT_COLOR;
}

                                                           
static inline DFBSurfacePixelFormat 
_cairo_to_directfb_format (cairo_format_t format)
{
    switch (format) {
        case CAIRO_FORMAT_RGB24:
            return DSPF_RGB32;
        case CAIRO_FORMAT_ARGB32:
            return DSPF_ARGB; 
        case CAIRO_FORMAT_A8:
            return DSPF_A8; 
        case CAIRO_FORMAT_A1:
            return DSPF_A1; 
        default:
            break;
    }
    
    return -1;
}

static inline cairo_format_t
_directfb_to_cairo_format (DFBSurfacePixelFormat format)
{
    switch (format) {
        case DSPF_RGB32:
            return CAIRO_FORMAT_RGB24;
        case DSPF_ARGB:
            return CAIRO_FORMAT_ARGB32;
        case DSPF_A8:
            return CAIRO_FORMAT_A8;
        case DSPF_A1:
            return CAIRO_FORMAT_A1;
        default:
            break;
    }
    
    return -1;
}


static cairo_status_t
_directfb_get_operator (cairo_operator_t         operator,
                        DFBSurfaceBlendFunction *ret_srcblend,
                        DFBSurfaceBlendFunction *ret_dstblend )
{
    DFBSurfaceBlendFunction srcblend = DSBF_ONE;
    DFBSurfaceBlendFunction dstblend = DSBF_ZERO;
    
    switch (operator) {
        case CAIRO_OPERATOR_CLEAR:
            srcblend = DSBF_ZERO;
            dstblend = DSBF_ZERO;
            break;
        case CAIRO_OPERATOR_SOURCE:
            srcblend = DSBF_ONE;
            dstblend = DSBF_ZERO;
            break;
        case CAIRO_OPERATOR_OVER:
            srcblend = DSBF_ONE;
            dstblend = DSBF_INVSRCALPHA;
            break;
        case CAIRO_OPERATOR_IN:
            srcblend = DSBF_DESTALPHA;
            dstblend = DSBF_ZERO;
            break;
        case CAIRO_OPERATOR_OUT:
            srcblend = DSBF_INVDESTALPHA;
            dstblend = DSBF_ZERO;
            break;
        case CAIRO_OPERATOR_ATOP:
            srcblend = DSBF_DESTALPHA;
            dstblend = DSBF_INVSRCALPHA;
            break;
        case CAIRO_OPERATOR_DEST:
            srcblend = DSBF_ZERO;
            dstblend = DSBF_ONE;
            break;
        case CAIRO_OPERATOR_DEST_OVER:
            srcblend = DSBF_INVDESTALPHA;
            dstblend = DSBF_ONE;
            break;
        case CAIRO_OPERATOR_DEST_IN:
            srcblend = DSBF_ZERO;
            dstblend = DSBF_SRCALPHA;
            break;
        case CAIRO_OPERATOR_DEST_OUT:
            srcblend = DSBF_ZERO;
            dstblend = DSBF_INVSRCALPHA;
            break;
        case CAIRO_OPERATOR_DEST_ATOP:
            srcblend = DSBF_INVDESTALPHA;
            dstblend = DSBF_SRCALPHA;
            break;                        
        case CAIRO_OPERATOR_XOR:
            srcblend = DSBF_INVDESTALPHA;
            dstblend = DSBF_INVSRCALPHA;
            break;
        case CAIRO_OPERATOR_ADD:
            srcblend = DSBF_ONE;
            dstblend = DSBF_ONE;
            break;
        case CAIRO_OPERATOR_SATURATE:
            srcblend = DSBF_SRCALPHASAT;
            dstblend = DSBF_ONE;
            break;
        default:
            return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    if (ret_srcblend)
        *ret_srcblend = srcblend;
    if (ret_dstblend)
        *ret_dstblend = dstblend;
    
    return CAIRO_STATUS_SUCCESS;
}

static IDirectFBSurface*
_directfb_buffer_surface_create (IDirectFB             *dfb, 
                                 DFBSurfacePixelFormat  format,
                                 int                    width,
                                 int                    height)
{
    IDirectFBSurface      *buffer;
    DFBSurfaceDescription  dsc;
    DFBResult              ret;
    
    dsc.flags       = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
    dsc.width       = width;
    dsc.height      = height;
    dsc.pixelformat = format;

    ret = dfb->CreateSurface (dfb, &dsc, &buffer);
    if (ret) {
        DirectFBError ("IDirectFB::CreateSurface()", ret);
        return NULL;
    }
    
    return buffer;
}

static cairo_status_t
_directfb_acquire_surface (cairo_directfb_surface_t  *surface, 
                           cairo_rectangle_int_t     *intrest_rec,
                           cairo_image_surface_t    **image_out,
                           cairo_rectangle_int_t     *image_rect_out,
                           void                     **image_extra,
                           DFBSurfaceLockFlags        lock_flags)
{   
    IDirectFBSurface *buffer = NULL;
    DFBRectangle source_rect;
    cairo_format_t cairo_format;
    void *data;
    int   pitch; 
        
    if (surface->format == (cairo_format_t) -1) {
        DFBSurfaceCapabilities caps;
        
        if (intrest_rec) {
            source_rect.x = intrest_rec->x;
            source_rect.y = intrest_rec->y;
            source_rect.w = intrest_rec->width; 
            source_rect.h = intrest_rec->height; 
        } else {
            source_rect.x = 0;
            source_rect.y = 0;
            surface->dfbsurface->GetSize (surface->dfbsurface,
                                          &source_rect.w, &source_rect.h);   
        }

        if (surface->tmpsurface) {
            int w, h;
            surface->tmpsurface->GetSize (surface->tmpsurface, &w, &h);
            if (w < source_rect.w || h < source_rect.h) {
                surface->tmpsurface->Release (surface->tmpsurface);
                surface->tmpsurface = NULL;
            }
        }
        
        cairo_format = _cairo_format_from_content (surface->content);
        if (!surface->tmpsurface) {
            D_DEBUG_AT (Cairo_DirectFB, "Allocating buffer for surface %p.\n", surface);
       
            surface->tmpsurface = 
                _directfb_buffer_surface_create (surface->dfb, 
                                                 _cairo_to_directfb_format (cairo_format),
                                                 source_rect.w, source_rect.h);
            if (!surface->tmpsurface)
                goto ERROR;
        }
        buffer = surface->tmpsurface;
        
        surface->dfbsurface->GetCapabilities (surface->dfbsurface, &caps);
        if (caps & DSCAPS_FLIPPING) {
            DFBRegion region = { .x1 = source_rect.x, .y1 = source_rect.y, 
                                 .x2 = source_rect.x + source_rect.w - 1,
                                 .y2 = source_rect.y + source_rect.h - 1 };
            surface->dfbsurface->Flip (surface->dfbsurface, &region, DSFLIP_BLIT);
        } 
        buffer->Blit (buffer, surface->dfbsurface, &source_rect, 0, 0);
    }
    else {
        /*might be a subsurface get the offset*/
        surface->dfbsurface->GetVisibleRectangle (surface->dfbsurface, &source_rect);
        cairo_format = surface->format;
        buffer = surface->dfbsurface;     
    }
    
    *image_extra = buffer;
    
    if (buffer->Lock (buffer, lock_flags, &data, &pitch)) {
        D_DEBUG_AT (Cairo_DirectFB, "Couldn't lock surface!\n");
        goto ERROR;
    }
            
    *image_out = (cairo_image_surface_t *)
                    cairo_image_surface_create_for_data (data, cairo_format,
                                            source_rect.w, source_rect.h, pitch);
    if (*image_out == NULL) 
        goto ERROR;

    if (image_rect_out) {
        image_rect_out->x = source_rect.x;
        image_rect_out->y = source_rect.y;
        image_rect_out->width = source_rect.w;
        image_rect_out->height = source_rect.h;
    } else {
        /* lock for read */
        cairo_surface_t *sur = &((*image_out)->base); 
        /* might be a subsurface */
        if (buffer == surface->dfbsurface)
            cairo_surface_set_device_offset (sur, source_rect.x, source_rect.y);
    }
   
    return CAIRO_STATUS_SUCCESS;

ERROR:
    *image_extra = NULL;
    if (buffer) {
        buffer->Unlock (buffer);
        if (buffer != surface->dfbsurface) 
            buffer->Release (buffer);
    }
    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
}



static cairo_surface_t *
_cairo_directfb_surface_create_similar (void            *abstract_src,
                                        cairo_content_t  content,
                                        int              width,
                                        int              height)
{
    cairo_directfb_surface_t *source  = abstract_src;
    cairo_directfb_surface_t *surface;
    cairo_format_t            format;
    
    D_DEBUG_AT (Cairo_DirectFB, 
                "%s( src=%p, content=0x%x, width=%d, height=%d).\n",
                __FUNCTION__, source, content, width, height);
    
    width = (width <= 0) ? 1 : width;
    height = (height<= 0) ? 1 : height;

    format = _cairo_format_from_content (content);             
    surface = calloc (1, sizeof(cairo_directfb_surface_t));
    if (!surface) {
        _cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
        return NULL;
    }
   
    surface->dfb = source->dfb;
    
    if (width < 8 || height < 8) {
        IDirectFBSurface *tmp;
        /* Some cards (e.g. Matrox) don't support surfaces smaller than 8x8 */
        tmp =  _directfb_buffer_surface_create (surface->dfb, 
                                        _cairo_to_directfb_format (format),
                                        MAX (width, 8), MAX (height, 8));
        if (tmp) {
            DFBRectangle rect = { .x=0, .y=0, .w=width, .h=height };
            tmp->GetSubSurface (tmp, &rect, &surface->dfbsurface);
            tmp->Release (tmp);
        }
    }
    else {
        surface->dfbsurface = _directfb_buffer_surface_create (surface->dfb,
                                    _cairo_to_directfb_format (format), width, height);
    }
    
    if (!surface->dfbsurface) {
        D_ASSERT (surface->dfbsurface != NULL);
        free (surface);
        return NULL;
    }
    
    _cairo_surface_init (&surface->base, &cairo_directfb_surface_backend, content);
    surface->format  = format;
    surface->content = content;
    surface->width   = width;
    surface->height  = height;
    surface->local   = true;
    
    return &surface->base;
}

static cairo_status_t
_cairo_directfb_surface_finish (void *data)
{
    cairo_directfb_surface_t *surface = (cairo_directfb_surface_t *)data;
    
    D_DEBUG_AT (Cairo_DirectFB, 
                "%s( surface=%p ).\n", __FUNCTION__, surface);
        
    if (surface->clips) {
        free (surface->clips);
        surface->clips   = NULL;
        surface->n_clips = 0;
    }
    
    if (surface->color) {
        cairo_surface_destroy (surface->color);
        surface->color = NULL;
    }
    
    if (surface->tmpsurface) {
        surface->tmpsurface->Release (surface->tmpsurface);
        surface->tmpsurface = NULL;
    }

    if (surface->dfbsurface) {
        surface->dfbsurface->Release (surface->dfbsurface);
        surface->dfbsurface = NULL;
    }
    
    if (surface->dfb)
        surface->dfb = NULL;
        
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_directfb_surface_acquire_source_image (void                   *abstract_surface,
                                              cairo_image_surface_t **image_out,
                                              void                  **image_extra)
{
    cairo_directfb_surface_t *surface = abstract_surface;
    
    D_DEBUG_AT (Cairo_DirectFB, 
                "%s( surface=%p ).\n", __FUNCTION__, surface);
    
    return _directfb_acquire_surface (surface, NULL, image_out, 
                                      NULL, image_extra, DSLF_READ);
}

static void
_cairo_directfb_surface_release_source_image (void                  *abstract_surface,
                                              cairo_image_surface_t *image,
                                              void                  *image_extra)
{
    cairo_directfb_surface_t *surface = abstract_surface;
    IDirectFBSurface *buffer = image_extra;
    
    D_DEBUG_AT (Cairo_DirectFB, 
                "%s( surface=%p ).\n", __FUNCTION__, surface);
    
    buffer->Unlock (buffer);
    
    cairo_surface_destroy (&image->base);
}

static cairo_status_t
_cairo_directfb_surface_acquire_dest_image (void                     *abstract_surface,
                                            cairo_rectangle_int_t    *interest_rect,
                                            cairo_image_surface_t   **image_out,
                                            cairo_rectangle_int_t    *image_rect_out,
                                            void                    **image_extra)
{
    cairo_directfb_surface_t *surface = abstract_surface;
    
    D_DEBUG_AT (Cairo_DirectFB, 
                "%s( surface=%p, interest_rect={ %d %d %d %d } ).\n", 
                __FUNCTION__, surface, 
                interest_rect ? interest_rect->x : 0,
                interest_rect ? interest_rect->y : 0,
                interest_rect ? interest_rect->width : surface->width,
                interest_rect ? interest_rect->height : surface->height);
    
    return _directfb_acquire_surface (surface, interest_rect, image_out, 
                                      image_rect_out, image_extra, DSLF_READ | DSLF_WRITE);
}

static void
_cairo_directfb_surface_release_dest_image (void                  *abstract_surface,
                                            cairo_rectangle_int_t *interest_rect,
                                            cairo_image_surface_t *image,
                                            cairo_rectangle_int_t *image_rect,
                                            void                  *image_extra)
{
    cairo_directfb_surface_t *surface = abstract_surface;
    IDirectFBSurface *buffer = image_extra; 
    
    D_DEBUG_AT (Cairo_DirectFB, 
                "%s( surface=%p ).\n", __FUNCTION__, surface);
    
    buffer->Unlock (buffer);

    if (surface->dfbsurface != buffer) {
        DFBRegion region = { .x1 = interest_rect->x, .y1 = interest_rect->y,
                             .x2 = interest_rect->x+interest_rect->width-1,
                             .y2 = interest_rect->y+interest_rect->height-1 };
        surface->dfbsurface->SetClip (surface->dfbsurface, &region);
        surface->dfbsurface->SetBlittingFlags (surface->dfbsurface, DSBLIT_NOFX);
        surface->dfbsurface->Blit (surface->dfbsurface, buffer,
                                   NULL, image_rect->x, image_rect->y);
    }
    
    cairo_surface_destroy (&image->base);
}

static cairo_status_t
_cairo_directfb_surface_clone_similar (void             *abstract_surface,
                                       cairo_surface_t  *src,
                                       int               src_x,
                                       int               src_y,
                                       int               width,
                                       int               height,
                                       cairo_surface_t **clone_out)
{
    cairo_directfb_surface_t *surface = abstract_surface;
    cairo_directfb_surface_t *clone;
    
    D_DEBUG_AT (Cairo_DirectFB, 
                "%s( surface=%p, src=%p ).\n", __FUNCTION__, surface, src);

    if (src->backend == surface->base.backend) {
        cairo_surface_reference (src);
        *clone_out = src;
         
        return CAIRO_STATUS_SUCCESS;
    }
    else if (_cairo_surface_is_image (src)) {
        cairo_image_surface_t *image_src = (cairo_image_surface_t *) src;
        unsigned char         *dst, *src = image_src->data;
        int                    pitch;
        int                    i, j;
        DFBResult              ret;
        
        clone = (cairo_directfb_surface_t *)
                _cairo_directfb_surface_create_similar (surface, 
                            _cairo_content_from_format (image_src->format),
                            image_src->width, image_src->height);
        if (!clone)
            return _cairo_error (CAIRO_STATUS_NO_MEMORY);
            
        ret = clone->dfbsurface->Lock (clone->dfbsurface, 
                                   DSLF_WRITE, (void *)&dst, &pitch);
        if (ret) {
            DirectFBError ("IDirectFBSurface::Lock()", ret);
            cairo_surface_destroy ((cairo_surface_t *)clone);
            return _cairo_error (CAIRO_STATUS_NO_MEMORY);
        }

        dst += pitch * src_y;
        src += image_src->stride * src_y;
        
        if (image_src->format == CAIRO_FORMAT_A1) {
            /* A1 -> A8 */
            for (i = 0; i < height; i++) {
                for (j = src_x; j < src_x + width; j++)
                    dst[j] = (src[j>>3] & (1 << (j&7))) ? 0xff : 0x00;
                dst += pitch;
                src += image_src->stride;
            }
        }
        else {
            int len;
            
            if (image_src->format == CAIRO_FORMAT_A8) {
                dst += src_x;
                src += src_x;
                len  = width;
            } else {
                dst += src_x * 4;
                src += src_x * 4;
                len  = width * 4;
            }
            
            for (i = 0; i < height; i++) {
                direct_memcpy (dst, src, len);
                dst += pitch;
                src += image_src->stride;
            }
        }
        
        clone->dfbsurface->Unlock (clone->dfbsurface);
        
        *clone_out = &clone->base;
                
        return CAIRO_STATUS_SUCCESS;
    }
    
    return CAIRO_INT_STATUS_UNSUPPORTED;
}

#if DFB_COMPOSITE || DFB_COMPOSITE_TRAPEZOIDS
static cairo_int_status_t
_directfb_prepare_composite (cairo_directfb_surface_t    *dst,
                             cairo_pattern_t             *src_pattern,
                             cairo_pattern_t             *mask_pattern,
                             cairo_operator_t             op,
                             int *src_x,             int *src_y,
                             int *mask_x,            int *mask_y,
                             unsigned int                 width,
                             unsigned int                 height,                        
                             cairo_directfb_surface_t   **ret_src,
                             cairo_surface_attributes_t  *ret_src_attr)
{
    cairo_directfb_surface_t   *src;
    cairo_surface_attributes_t  src_attr;
    cairo_status_t              ret;
    DFBSurfaceBlittingFlags     flags;
    DFBSurfaceBlendFunction     sblend;
    DFBSurfaceBlendFunction     dblend;
    DFBColor                    color;
    
    if (_directfb_get_operator (op, &sblend, &dblend))
        return CAIRO_INT_STATUS_UNSUPPORTED;
     
    if (mask_pattern) {
        cairo_solid_pattern_t *pattern;
        
        if (mask_pattern->type != CAIRO_PATTERN_TYPE_SOLID) {
            cairo_pattern_t *tmp;
            int              tmp_x, tmp_y;
            
            if (src_pattern->type != CAIRO_PATTERN_TYPE_SOLID ||
                sblend == DSBF_INVDESTALPHA) /* Doesn't work correctly */
                return CAIRO_INT_STATUS_UNSUPPORTED;
                
            D_DEBUG_AT (Cairo_DirectFB, "Replacing src pattern by mask pattern.\n");
                
            tmp = src_pattern;
            tmp_x = *src_x; tmp_y = *src_y;
            
            src_pattern = mask_pattern;
            *src_x = *mask_x; *src_y = *mask_y;
            
            mask_pattern = tmp;
            *mask_x = tmp_x; *mask_y = tmp_y;
            
            if (sblend == DSBF_ONE) {                  
                sblend = DSBF_SRCALPHA;
                /*dblend = DSBF_INVSRCALPHA;*/
            }
        }
            
        pattern = (cairo_solid_pattern_t *)mask_pattern;
        color.a = pattern->color.alpha_short >> 8;
        color.r = pattern->color.red_short   >> 8;
        color.g = pattern->color.green_short >> 8;
        color.b = pattern->color.blue_short  >> 8;
    } 
    else {
        color.a = color.r = color.g = color.b = 0xff;
    }
        
    if (src_pattern->type == CAIRO_PATTERN_TYPE_SOLID) {
        cairo_solid_pattern_t *pattern = (cairo_solid_pattern_t *)src_pattern;
         
        if (!dst->color) {
            dst->color = _cairo_directfb_surface_create_similar (dst,
                                                CAIRO_CONTENT_COLOR_ALPHA, 1, 1);
            if (!dst->color)
                return _cairo_error (CAIRO_STATUS_NO_MEMORY);
        }
        
        src = (cairo_directfb_surface_t *)dst->color;
        src->dfbsurface->Clear (src->dfbsurface,
                                pattern->color.red_short   >> 8,
                                pattern->color.green_short >> 8,
                                pattern->color.blue_short  >> 8,
                                pattern->color.alpha_short >> 8);
                                  
        src_attr.matrix   = src_pattern->matrix;
        src_attr.extend   = CAIRO_EXTEND_NONE;
        src_attr.filter   = CAIRO_FILTER_NEAREST;
        src_attr.x_offset =
        src_attr.y_offset = 0;
    }
    else {
        ret = _cairo_pattern_acquire_surface (src_pattern, &dst->base,
                                              *src_x, *src_y, width, height,
                                              (cairo_surface_t **)&src, &src_attr);
        if (ret)
            return ret;
    }
    
    if (src->content == CAIRO_CONTENT_COLOR) {
        if (sblend == DSBF_SRCALPHA)
            sblend = DSBF_ONE;
        else if (sblend == DSBF_INVSRCALPHA)
            sblend = DSBF_ZERO;
            
        if (dblend == DSBF_SRCALPHA)
            dblend = DSBF_ONE;
        else if (dblend == DSBF_INVSRCALPHA)
            dblend = DSBF_ZERO;
    }
    if (dst->content == CAIRO_CONTENT_COLOR) {
        if (sblend == DSBF_DESTALPHA)
            sblend = DSBF_ONE;
        else if (sblend == DSBF_INVDESTALPHA)
            sblend = DSBF_ZERO;
         
        if (dblend == DSBF_DESTALPHA)
            dblend = DSBF_ONE;
        else if (dblend == DSBF_INVDESTALPHA)
            dblend = DSBF_ZERO;   
    }

    flags = (sblend == DSBF_ONE && dblend == DSBF_ZERO) 
            ? DSBLIT_NOFX : DSBLIT_BLEND_ALPHACHANNEL; 
    if (color.a != 0xff)
        flags |= DSBLIT_BLEND_COLORALPHA;
    if (color.r != 0xff || color.g != 0xff || color.b != 0xff)
        flags |= DSBLIT_COLORIZE;
            
    dst->dfbsurface->SetBlittingFlags (dst->dfbsurface, flags);
    
    if (flags & (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA)) {
        dst->dfbsurface->SetSrcBlendFunction (dst->dfbsurface, sblend);
        dst->dfbsurface->SetDstBlendFunction (dst->dfbsurface, dblend);
    }
    
    if (flags & (DSBLIT_BLEND_COLORALPHA | DSBLIT_COLORIZE))    
        dst->dfbsurface->SetColor (dst->dfbsurface, color.r, color.g, color.b, color.a); 
        
    *ret_src = src;
    *ret_src_attr = src_attr;
    
    return CAIRO_STATUS_SUCCESS;
}

static void
_directfb_finish_composite (cairo_directfb_surface_t   *dst,
                            cairo_pattern_t            *src_pattern,
                            cairo_surface_t            *src,
                            cairo_surface_attributes_t *src_attr)
{
    if (src != dst->color)
         _cairo_pattern_release_surface (src_pattern, src, src_attr);
}
#endif /* DFB_COMPOSITE || DFB_COMPOSITE_TRAPEZOIDS */        

#if DFB_COMPOSITE
static DFBAccelerationMask
_directfb_categorize_operation (cairo_surface_attributes_t *src_attr)
{
    cairo_matrix_t *m = &src_attr->matrix;
    
    if (m->xy != 0 || m->yx != 0 || m->xx < 0 || m->yy < 0) {
        if (src_attr->extend != CAIRO_EXTEND_NONE)
            return DFXL_NONE;
        return DFXL_TEXTRIANGLES;
    }
        
    if (m->xx != 1 || m->yy != 1) {
        if (src_attr->extend != CAIRO_EXTEND_NONE)
            return DFXL_NONE;
        return DFXL_STRETCHBLIT;
    }
     
    if (src_attr->extend != CAIRO_EXTEND_NONE &&
        src_attr->extend != CAIRO_EXTEND_REPEAT)
        return DFXL_NONE;
           
    return DFXL_BLIT;
}

static cairo_int_status_t
_cairo_directfb_surface_composite (cairo_operator_t  op,
                                   cairo_pattern_t  *src_pattern,
                                   cairo_pattern_t  *mask_pattern,
                                   void             *abstract_dst,
                                   int  src_x,  int  src_y,
                                   int  mask_x, int  mask_y,
                                   int  dst_x,  int  dst_y,
                                   unsigned int      width,
                                   unsigned int      height)
{
    cairo_directfb_surface_t   *dst = abstract_dst;
    cairo_directfb_surface_t   *src;
    cairo_surface_attributes_t  src_attr;
    DFBAccelerationMask         accel, mask;
    cairo_int_status_t          ret;
    
    D_DEBUG_AT (Cairo_DirectFB,
                "%s( op=%d, src_pattern=%p, mask_pattern=%p, dst=%p,"
                   " src_x=%d, src_y=%d, mask_x=%d, mask_y=%d, dst_x=%d,"
                   " dst_y=%d, width=%u, height=%u ).\n",
                __FUNCTION__, op, src_pattern, mask_pattern, dst, 
                src_x, src_y, mask_x, mask_y, dst_x, dst_y, width, height);
    
    ret = _directfb_prepare_composite (dst, src_pattern, mask_pattern, op,
                                       &src_x, &src_y, &mask_x, &mask_y, 
                                       width, height, &src, &src_attr);
    if (ret)
        return ret;
        
    accel = _directfb_categorize_operation (&src_attr);
    
    dst->dfbsurface->GetAccelerationMask (dst->dfbsurface, src->dfbsurface, &mask);
    if (!(mask & accel)) {
        D_DEBUG_AT (Cairo_DirectFB, "No acceleration (%08x)!\n", accel);
        if (accel != DFXL_BLIT) {
            _directfb_finish_composite (dst, src_pattern, &src->base, &src_attr);
            return CAIRO_INT_STATUS_UNSUPPORTED;
        }
    }
    
    src_x += src_attr.x_offset;
    src_y += src_attr.y_offset;
    
    switch (accel) {
        case DFXL_BLIT: {
            DFBRectangle sr;
        
            sr.x = src_x + _cairo_lround (src_attr.matrix.x0);
            sr.y = src_y + _cairo_lround (src_attr.matrix.y0);
            sr.w = width;
            sr.h = height;
        
            if (src_attr.extend == CAIRO_EXTEND_NONE) {
                D_DEBUG_AT (Cairo_DirectFB, "Running Blit().\n");
        
                RUN_CLIPPED( dst, NULL,
                             dst->dfbsurface->Blit (dst->dfbsurface,
                                            src->dfbsurface, &sr, dst_x, dst_y));
            }
            else if (src_attr.extend == CAIRO_EXTEND_REPEAT) {
                DFBRegion clip;
            
                clip.x1 = dst_x;
                clip.y1 = dst_y;
                clip.x2 = dst_x + width  - 1;
                clip.y2 = dst_y + height - 1;
            
                D_DEBUG_AT (Cairo_DirectFB, "Running TileBlit().\n");
            
                RUN_CLIPPED( dst, &clip,
                             dst->dfbsurface->TileBlit (dst->dfbsurface, 
                                            src->dfbsurface, &sr, dst_x, dst_y));
            }
        }   break;
        
        case DFXL_STRETCHBLIT: {
            DFBRectangle sr, dr;
            double       x1, y1, x2, y2;
            
            TRANSFORM_POINT2X (src_attr.matrix, 
                               src_x, src_y, x1, y1);
            TRANSFORM_POINT2X (src_attr.matrix, 
                               src_x+width, src_y+height, x2, y2);
                               
            sr.x = _cairo_lround (x1);
            sr.y = _cairo_lround (y1);
            sr.w = _cairo_lround (x2-x1);
            sr.h = _cairo_lround (y2-y1);
            
            dr.x = dst_x;
            dr.y = dst_y;
            dr.w = width;
            dr.h = height;
            
            D_DEBUG_AT (Cairo_DirectFB, "Running StretchBlit().\n");

            RUN_CLIPPED (dst, NULL,
                         dst->dfbsurface->StretchBlit (dst->dfbsurface, 
                                                       src->dfbsurface, &sr, &dr));
        }   break;
        
        case DFXL_TEXTRIANGLES: {
            DFBRegion clip;
            DFBVertex v[4];
            float     x1, y1, x2, y2;
            int       w, h;
            
            if (cairo_matrix_invert (&src_attr.matrix) != CAIRO_STATUS_SUCCESS) {
                ret = CAIRO_INT_STATUS_UNSUPPORTED;
                break;
            }
            
            x1 = src_attr.x_offset;
            y1 = src_attr.y_offset;
            x2 = src->width  - x1;
            y2 = src->height - y1;
            
            src->dfbsurface->GetSize (src->dfbsurface, &w, &h);
            
            TRANSFORM_POINT3X (src_attr.matrix, 
                               x1, y1, v[0].x, v[0].y);
            v[0].z = 0;
            v[0].w = 1;
            v[0].s = x1 / w;
            v[0].t = y1 / h;
        
            TRANSFORM_POINT3X (src_attr.matrix, 
                               x2, y1, v[1].x, v[1].y);
            v[1].z = 0;
            v[1].w = 1;
            v[1].s = x2 / w;
            v[1].t = y1 / h;
        
            TRANSFORM_POINT3X (src_attr.matrix,
                               x2, y2, v[2].x, v[2].y);
            v[2].z = 0;
            v[2].w = 1;
            v[2].s = x2 / w;
            v[2].t = y2 / h;
        
            TRANSFORM_POINT3X (src_attr.matrix, 
                               x1, y2, v[3].x, v[3].y);
            v[3].z = 0;
            v[3].w = 1;
            v[3].s = x1 / w;
            v[3].t = y2 / h;
            
            clip.x1 = dst_x;
            clip.y1 = dst_y;
            clip.x2 = dst_x + width  - 1;
            clip.y2 = dst_y + height - 1;
        
            D_DEBUG_AT (Cairo_DirectFB, "Running TextureTriangles().\n");
            
            RUN_CLIPPED (dst, &clip,
                         dst->dfbsurface->TextureTriangles (dst->dfbsurface, 
                                            src->dfbsurface, v, NULL, 4, DTTF_FAN));
        }   break;
        
        default:
            D_BUG ("Unexpected operation");
            break;
    }

    _directfb_finish_composite (dst, src_pattern, &src->base, &src_attr);
    
    return ret;
}
#endif /* DFB_COMPOSITE */

#if DFB_RECTANGLES
static cairo_int_status_t
_cairo_directfb_surface_fill_rectangles (void                  *abstract_surface,
                                         cairo_operator_t       op,
                                         const cairo_color_t   *color,
                                         cairo_rectangle_int_t *rects,
                                         int                    n_rects)
{
    cairo_directfb_surface_t *dst   = abstract_surface;
    DFBSurfaceDrawingFlags    flags;
    DFBSurfaceBlendFunction   sblend;
    DFBSurfaceBlendFunction   dblend;
    DFBRectangle              r[n_rects];
    int                       i;
    
    D_DEBUG_AT (Cairo_DirectFB, 
                "%s( dst=%p, op=%d, color=%p, rects=%p, n_rects=%d ).\n",
                __FUNCTION__, dst, op, color, rects, n_rects);

    if (_directfb_get_operator (op, &sblend, &dblend))
        return CAIRO_INT_STATUS_UNSUPPORTED;
        
    if (color->alpha_short >= 0xff00) {
        if (sblend == DSBF_SRCALPHA)
            sblend = DSBF_ONE;
        else if (sblend == DSBF_INVSRCALPHA)
            sblend = DSBF_ZERO;
            
        if (dblend == DSBF_SRCALPHA)
            dblend = DSBF_ONE;
        else if (dblend == DSBF_INVSRCALPHA)
            dblend = DSBF_ZERO;
    }
    if (dst->content == CAIRO_CONTENT_COLOR) {
        if (sblend == DSBF_DESTALPHA)
            sblend = DSBF_ONE;
        else if (sblend == DSBF_INVDESTALPHA)
            sblend = DSBF_ZERO;
         
        if (dblend == DSBF_DESTALPHA)
            dblend = DSBF_ONE;
        else if (dblend == DSBF_INVDESTALPHA)
            dblend = DSBF_ZERO;   
    }
    
    flags = (sblend == DSBF_ONE && dblend == DSBF_ZERO) ? DSDRAW_NOFX : DSDRAW_BLEND;
    dst->dfbsurface->SetDrawingFlags (dst->dfbsurface, flags);
    if (flags & DSDRAW_BLEND) {
        dst->dfbsurface->SetSrcBlendFunction (dst->dfbsurface, sblend);
        dst->dfbsurface->SetDstBlendFunction (dst->dfbsurface, dblend);
    }    
    
    dst->dfbsurface->SetColor (dst->dfbsurface, 
                               color->red_short   >> 8,
                               color->green_short >> 8, 
                               color->blue_short  >> 8, 
                               color->alpha_short >> 8 );
       
    for (i = 0; i < n_rects; i++) {
        r[i].x = rects[i].x;
        r[i].y = rects[i].y;
        r[i].w = rects[i].width;
        r[i].h = rects[i].height;
    }
     
    RUN_CLIPPED (dst, NULL,
                 dst->dfbsurface->FillRectangles (dst->dfbsurface, r, n_rects));
    
    return CAIRO_STATUS_SUCCESS;
}
#endif

#if DFB_COMPOSITE_TRAPEZOIDS
static cairo_int_status_t
_cairo_directfb_surface_composite_trapezoids (cairo_operator_t   op,
                                              cairo_pattern_t   *pattern,
                                              void              *abstract_dst,
                                              cairo_antialias_t  antialias,
                                              int  src_x,   int  src_y,
                                              int  dst_x,   int  dst_y,
                                              unsigned int       width,
                                              unsigned int       height,
                                              cairo_trapezoid_t *traps,
                                              int                num_traps )
{
    cairo_directfb_surface_t   *dst = abstract_dst;
    cairo_directfb_surface_t   *src;
    cairo_surface_attributes_t  src_attr;
    cairo_status_t              ret;
    DFBAccelerationMask         accel;
    
    D_DEBUG_AT (Cairo_DirectFB,
                "%s( op=%d, pattern=%p, dst=%p, antialias=%d,"
                   " src_x=%d, src_y=%d, dst_x=%d, dst_y=%d,"
                   " width=%u, height=%u, traps=%p, num_traps=%d ).\n",
                __FUNCTION__, op, pattern, dst, antialias,
                src_x, src_y, dst_x, dst_y, width, height, traps, num_traps);

    if (antialias != CAIRO_ANTIALIAS_NONE)
        return CAIRO_INT_STATUS_UNSUPPORTED;
        
    /* Textures are not supported yet. */
    if (pattern->type != CAIRO_PATTERN_TYPE_SOLID)
        return CAIRO_INT_STATUS_UNSUPPORTED;
    
    ret = _directfb_prepare_composite (dst, pattern, NULL, op,
                                       &src_x, &src_y, NULL, NULL, 
                                       width, height, &src, &src_attr);
    if (ret)
        return ret;
   
    dst->dfbsurface->GetAccelerationMask (dst->dfbsurface, src->dfbsurface, &accel);
    
    ret = CAIRO_INT_STATUS_UNSUPPORTED;
    
    if (accel & DFXL_TEXTRIANGLES) {
        DFBVertex  vertex[6*num_traps];
        DFBVertex *v = &vertex[0];
        int        n;
      
#define ADD_TRI(id, x1, y1, s1, t1, x2, y2, s2, t2, x3, y3, s3, t3) {\
    const int p = (id)*3;\
    v[p+0].x=(x1); v[p+0].y=(y1); v[p+0].z=0; v[p+0].w=1; v[p+0].s=(s1); v[p+0].t=(t1);\
    v[p+1].x=(x2); v[p+1].y=(y2); v[p+1].z=0; v[p+1].w=1; v[p+1].s=(s2); v[p+1].t=(t2);\
    v[p+2].x=(x3); v[p+2].y=(y3); v[p+2].z=0; v[p+2].w=1; v[p+2].s=(s3); v[p+2].t=(t3);\
}         
       
        for (n = 0; num_traps; num_traps--) {
            float lx1, ly1, lx2, ly2;
            float rx1, ry1, rx2, ry2;
            
            /* XXX: Do we need to validate the trapezoid? */
 
            lx1 = traps->left.p1.x/65536.0;
            ly1 = traps->left.p1.y/65536.0;
            lx2 = traps->left.p2.x/65536.0;
            ly2 = traps->left.p2.y/65536.0;
            rx1 = traps->right.p1.x/65536.0;
            ry1 = traps->right.p1.y/65536.0;
            rx2 = traps->right.p2.x/65536.0;
            ry2 = traps->right.p2.y/65536.0;
            
            if (traps->left.p1.y < traps->top) {
                float y = traps->top/65536.0;
                if (lx2 != lx1)
                    lx1 = (y - ly1) * (lx2 - lx1) / (ly2 - ly1) + lx1;
                ly1 = y;
            } 
            if (traps->left.p2.y > traps->bottom) {
                float y = traps->bottom/65536.0;
                if (lx2 != lx1)
                    lx2 = (y - ly1) * (lx2 - lx1) / (ly2 - ly1) + lx1;
                ly2 = y;
            }
            
            if (traps->right.p1.y < traps->top) {
                float y = traps->top/65536.0;
                if (rx2 != rx1)
                    rx1 = (y - ry1) * (rx2 - rx1) / (ry2 - ry1) + rx1;
                ry1 = y;
            }
            if (traps->right.p2.y > traps->bottom) {
                float y = traps->bottom/65536.0;
                if (rx2 != rx1)
                    rx2 = (y - ry1) * (rx2 - rx1) / (ry2 - ry1) + rx1;
                ry2 = y;
            }
             
            if (lx1 == rx1 && ly1 == ry1) {
                ADD_TRI (0, lx2, ly2, 0, 0,
                            lx1, ly1, 0, 0,
                            rx2, ry2, 0, 0 );
                v += 3;
                n += 3;
            }
            else if (lx2 == rx2 && ly2 == ry2) {
                ADD_TRI (0, lx1, ly1, 0, 0,
                            lx2, ly2, 0, 0,
                            rx1, ry1, 0, 0 );
                v += 3;
                n += 3;
            }
            else {
                ADD_TRI (0, lx1, ly1, 0, 0, 
                            rx1, ry1, 0, 0,
                            lx2, ly2, 0, 0);
                ADD_TRI (1, lx2, ly2, 0, 0,
                            rx1, ry1, 0, 0,
                            rx2, ry2, 0, 0);            
                v += 6;
                n += 6;
            }
            
            traps++;
        }
        
#undef ADD_TRI
              
        D_DEBUG_AT (Cairo_DirectFB, "Running TextureTriangles().\n");
            
        RUN_CLIPPED (dst, NULL,
                     dst->dfbsurface->TextureTriangles (dst->dfbsurface, src->dfbsurface, 
                                                    vertex, NULL, n, DTTF_LIST));
                                            
        ret = CAIRO_STATUS_SUCCESS;
    }
    
    _directfb_finish_composite (dst, pattern, &src->base, &src_attr);

    return ret;
}
#endif /* DFB_COMPOSITE_TRAPEZOIDS */

static cairo_int_status_t
_cairo_directfb_surface_set_clip_region (void           *abstract_surface,
                                         cairo_region_t *region)
{
    cairo_directfb_surface_t *surface = abstract_surface;
    
    D_DEBUG_AT (Cairo_DirectFB, 
                "%s( surface=%p, region=%p ).\n",
                __FUNCTION__, surface, region);
    
    if (region) {
        cairo_box_int_t *boxes;
        int              n_boxes;
        cairo_status_t   status;
        int              i;

        status = _cairo_region_get_boxes (region, &n_boxes, &boxes);
        if (status)
            return status;
        
        if (surface->n_clips != n_boxes) {
            if (surface->clips)
                free (surface->clips);
            
            surface->clips = _cairo_malloc_ab (n_boxes, sizeof(DFBRegion));
            if (!surface->clips) {
                surface->n_clips = 0;
                _cairo_region_boxes_fini (region, boxes);
                return _cairo_error (CAIRO_STATUS_NO_MEMORY);
            }
        
            surface->n_clips = n_boxes;
        }
        
        for (i = 0; i < n_boxes; i++) {
            surface->clips[i].x1 = boxes[i].p1.x;
            surface->clips[i].y1 = boxes[i].p1.y;
            surface->clips[i].x2 = boxes[i].p2.x;
            surface->clips[i].y2 = boxes[i].p2.y;
        }

        _cairo_region_boxes_fini (region, boxes);
    }
    else {
        if (surface->clips) {
            free (surface->clips);
            surface->clips = NULL;
            surface->n_clips = 0;
        }
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_directfb_abstract_surface_get_extents (void                  *abstract_surface,
                                              cairo_rectangle_int_t *rectangle)
{
    cairo_directfb_surface_t *surface = abstract_surface;
    
    D_DEBUG_AT (Cairo_DirectFB,
                "%s( surface=%p, rectangle=%p ).\n",
                __FUNCTION__, surface, rectangle);
    
    if (rectangle) {
        if (!surface->local) {
            surface->dfbsurface->GetSize (surface->dfbsurface,
                                          &surface->width, &surface->height);
        }   
        rectangle->x = 0;
        rectangle->y = 0;
        rectangle->width  = surface->width;
        rectangle->height = surface->height;
    }
    
    return CAIRO_STATUS_SUCCESS;
}

#if DFB_SHOW_GLYPHS
static cairo_directfb_font_cache_t*
_directfb_allocate_font_cache (IDirectFB *dfb, int width, int height)
{
    cairo_directfb_font_cache_t *cache;

    cache = calloc (1, sizeof(cairo_directfb_font_cache_t));
    if (!cache)
        return NULL;

    cache->dfb = dfb;
    cache->dfbsurface = _directfb_buffer_surface_create (dfb, 
                            _directfb_argb_font ? DSPF_ARGB : DSPF_A8, width, height);
    if (!cache->dfbsurface) {
        free (cache);
        return NULL;
    }
    cache->width  = width;
    cache->height = height;

    return cache;
}

static void
_directfb_destroy_font_cache (cairo_directfb_font_cache_t *cache)
{
    cache->dfbsurface->Release (cache->dfbsurface);
    free (cache);
}

static cairo_status_t
_directfb_acquire_font_cache (cairo_directfb_surface_t     *surface,
                              cairo_scaled_font_t          *scaled_font,
                              const cairo_glyph_t          *glyphs,
                              int                           num_glyphs,
                              cairo_directfb_font_cache_t **ret_cache,
                              DFBRectangle                 *rects,
                              DFBPoint                     *points,
                              int                          *ret_num )
{
    cairo_status_t               ret;
    cairo_scaled_glyph_t        *chars[num_glyphs];
    int                          num_chars = 0;
    cairo_directfb_font_cache_t *cache     = NULL; 
    int                          n         = 0;
    int                          x         = 0;
    int                          y         = 0;
    int                          w         = 8;
    int                          h         = 8;
    int                          i;
    
    if (scaled_font->surface_private) {
        cache = scaled_font->surface_private;
        x = cache->x;
        y = cache->y;
    }
    
    for (i = 0; i < num_glyphs; i++) {
        cairo_scaled_glyph_t  *scaled_glyph;
        cairo_image_surface_t *img;
        
        ret = _cairo_scaled_glyph_lookup (scaled_font, glyphs[i].index,
                                          CAIRO_SCALED_GLYPH_INFO_SURFACE,
                                          &scaled_glyph);
        if (ret)
            return ret;
        
        img = scaled_glyph->surface;
        switch (img->format) {
            case CAIRO_FORMAT_A1:
            case CAIRO_FORMAT_A8:
            case CAIRO_FORMAT_ARGB32:
                break;
            default:
                D_DEBUG_AT (Cairo_DirectFB,
                            "Unsupported font format %d!\n", img->format);
                return CAIRO_INT_STATUS_UNSUPPORTED;
        }
        
        points[n].x = _cairo_lround (glyphs[i].x - img->base.device_transform.x0);
        points[n].y = _cairo_lround (glyphs[i].y - img->base.device_transform.y0);
        
        if (points[n].x >= surface->width  ||
            points[n].y >= surface->height ||
            points[n].x+img->width  <= 0   ||
            points[n].y+img->height <= 0)
            continue;
        
        if (!scaled_glyph->surface_private) {
            DFBRectangle *rect;
            
            if (x+img->width > 2048) {
                x = 0;
                y = h;
                h = 0;
            }
        
            rects[n].x = x;
            rects[n].y = y;
            rects[n].w = img->width;
            rects[n].h = img->height;
 
            x += img->width;
            h  = MAX (h, img->height);
            w  = MAX (w, x);
            
            /* Remember glyph location */ 
            rect = malloc (sizeof(DFBRectangle));
            if (!rect)
                return _cairo_error (CAIRO_STATUS_NO_MEMORY);
            *rect = rects[n];
            
            scaled_glyph->surface_private = rect;
            chars[num_chars++] = scaled_glyph;
            
            /*D_DEBUG_AT (Cairo_DirectFB, 
                        "Glyph %lu will be loaded at (%d,%d).\n",
                        glyphs[i].index, rects[n].x, rects[n].y);*/
        }
        else {
            rects[n] = *((DFBRectangle *)scaled_glyph->surface_private);
            
            /*D_DEBUG_AT (Cairo_DirectFB, 
                        "Glyph %lu already loaded at (%d,%d).\n",
                        glyphs[i].index, rects[n].x, rects[n].y);*/
        }
            
        n++;
    }
    
    if (!n)
        return CAIRO_INT_STATUS_NOTHING_TO_DO;
    
    h += y;
    w = MAX (w, 8);
    h = MAX (h, 8);
     
    if (cache) {
        if (cache->width < w || cache->height < h) {
            cairo_directfb_font_cache_t *new_cache;
            
            w = MAX (w, cache->width);
            h = MAX (h, cache->height);
            
            D_DEBUG_AT (Cairo_DirectFB,
                        "Reallocating font cache (%dx%d).\n", w, h);
            
            new_cache = _directfb_allocate_font_cache (surface->dfb, w, h);
            if (!new_cache)
                return _cairo_error (CAIRO_STATUS_NO_MEMORY);
            
            new_cache->dfbsurface->Blit (new_cache->dfbsurface,
                                         cache->dfbsurface, NULL, 0, 0);
            
            _directfb_destroy_font_cache (cache);
            scaled_font->surface_private = cache = new_cache;
        }
    }
    else {
        D_DEBUG_AT (Cairo_DirectFB,
                    "Allocating font cache (%dx%d).\n", w, h);
        
        cache = _directfb_allocate_font_cache (surface->dfb, w, h);
        if (!cache)
            return _cairo_error (CAIRO_STATUS_NO_MEMORY);
            
        scaled_font->surface_backend = &cairo_directfb_surface_backend;
        scaled_font->surface_private = cache;
    }
    
    if (num_chars) {
        unsigned char *data;
        int            pitch;
    
        if (cache->dfbsurface->Lock (cache->dfbsurface, 
                                     DSLF_WRITE, (void *)&data, &pitch))
            return _cairo_error (CAIRO_STATUS_NO_MEMORY);
            
        for (i = 0; i < num_chars; i++) {
            cairo_image_surface_t *img  = chars[i]->surface;
            DFBRectangle          *rect = chars[i]->surface_private;
            unsigned char         *dst  = data;
            unsigned char         *src  = img->data;
            int                    j;
            
            dst += rect->y * pitch + (_directfb_argb_font ? (rect->x<<2) : rect->x);
                        
            if (img->format == CAIRO_FORMAT_A1) {
                for (h = rect->h; h; h--) {
                    if (_directfb_argb_font) {
                        for (j = 0; j < rect->w; j++)
                            ((uint32_t *)dst)[j] = (src[j>>3] & (1 << (j&7))) ? 0xffffffff : 0;
                    }
                    else {
                        for (j = 0; j < rect->w; j++)
                            dst[j] = (src[j>>3] & (1 << (j&7))) ? 0xff : 0;
                    }
                    
                    dst += pitch;
                    src += img->stride;
                }
            }
            else if (img->format == CAIRO_FORMAT_A8) {
                for (h = rect->h; h; h--) {
                    if (_directfb_argb_font) {
                        for (j = 0; j < rect->w; j++)
                            ((uint32_t *)dst)[j] = src[j] * 0x01010101;
                    }
                    else {
                        direct_memcpy (dst, src, rect->w);
                    }
                    
                    dst += pitch;
                    src += img->stride;
                }
            }
            else { /* ARGB32 */
                for (h = rect->h; h; h--) {
                    if (_directfb_argb_font) {
                        direct_memcpy (dst, src, rect->w<<2);
                    }
                    else {
                        for (j = 0; j < rect->w; j++)
                            dst[j] = ((uint32_t *)src)[j] >> 24;
                    }
                    
                    dst += pitch;
                    src += img->stride;
                }
            }
        }
        
        cache->dfbsurface->Unlock (cache->dfbsurface);
    }

    cache->x = x;
    cache->y = y;
    
    *ret_cache = cache;
    *ret_num   = n;

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_directfb_surface_scaled_font_fini (cairo_scaled_font_t *scaled_font)
{
    cairo_directfb_font_cache_t *cache = scaled_font->surface_private;
    
    D_DEBUG_AT (Cairo_DirectFB,
                "%s( scaled_font=%p ).\n", __FUNCTION__, scaled_font);
    
    if (cache) {
        _directfb_destroy_font_cache (cache);
        scaled_font->surface_private = NULL;
    }
}

static void
_cairo_directfb_surface_scaled_glyph_fini (cairo_scaled_glyph_t *scaled_glyph,
                                           cairo_scaled_font_t  *scaled_font)
{
     D_DEBUG_AT (Cairo_DirectFB,
                 "%s( scaled_glyph=%p, scaled_font=%p ).\n",
                 __FUNCTION__, scaled_glyph, scaled_font);

     if (scaled_glyph->surface_private) {
          free (scaled_glyph->surface_private);
          scaled_glyph->surface_private = NULL;
     }
}

static cairo_int_status_t
_cairo_directfb_surface_show_glyphs (void                *abstract_dst,
                                     cairo_operator_t     op,
                                     cairo_pattern_t     *pattern,
                                     cairo_glyph_t       *glyphs,
                                     int                  num_glyphs,
                                     cairo_scaled_font_t *scaled_font)
{
    cairo_directfb_surface_t    *dst = abstract_dst;
    cairo_directfb_font_cache_t *cache;
    cairo_status_t               ret;
    DFBSurfaceBlittingFlags      flags;
    DFBSurfaceBlendFunction      sblend;
    DFBSurfaceBlendFunction      dblend;
    DFBColor                     color;
    DFBRectangle                 rects[num_glyphs];
    DFBPoint                     points[num_glyphs];
    int                          num;
    
    D_DEBUG_AT (Cairo_DirectFB,
                "%s( dst=%p, op=%d, pattern=%p, glyphs=%p, num_glyphs=%d, scaled_font=%p ).\n",
                __FUNCTION__, dst, op, pattern, glyphs, num_glyphs, scaled_font);
                            
    if (pattern->type != CAIRO_PATTERN_TYPE_SOLID)
        return CAIRO_INT_STATUS_UNSUPPORTED;
        
    if (_directfb_get_operator (op, &sblend, &dblend) ||
        sblend == DSBF_DESTALPHA || sblend == DSBF_INVDESTALPHA) 
        return CAIRO_INT_STATUS_UNSUPPORTED;
        
    ret = _directfb_acquire_font_cache (dst, scaled_font, glyphs, num_glyphs,
                                        &cache, &rects[0], &points[0], &num);
    if (ret) {
        if (ret == CAIRO_INT_STATUS_NOTHING_TO_DO)
            ret = CAIRO_STATUS_SUCCESS;
        return ret;
    }
        
    color.a = ((cairo_solid_pattern_t *)pattern)->color.alpha_short >> 8;
    color.r = ((cairo_solid_pattern_t *)pattern)->color.red_short   >> 8;
    color.g = ((cairo_solid_pattern_t *)pattern)->color.green_short >> 8;
    color.b = ((cairo_solid_pattern_t *)pattern)->color.blue_short  >> 8;
    
    flags = DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_COLORIZE;
    if (color.a != 0xff)
        flags |= DSBLIT_BLEND_COLORALPHA;
        
    if (!_directfb_argb_font) {
        if (sblend == DSBF_ONE) {
            sblend = DSBF_SRCALPHA;
            if (dblend == DSBF_ZERO)
                dblend = DSBF_INVSRCALPHA;
        }
    } 
        
    dst->dfbsurface->SetBlittingFlags (dst->dfbsurface, flags);
    dst->dfbsurface->SetSrcBlendFunction (dst->dfbsurface, sblend);
    dst->dfbsurface->SetDstBlendFunction (dst->dfbsurface, dblend);
    dst->dfbsurface->SetColor (dst->dfbsurface, color.r, color.g, color.b, color.a);
    
    D_DEBUG_AT (Cairo_DirectFB, "Running BatchBlit().\n");
        
    RUN_CLIPPED (dst, NULL,
                 dst->dfbsurface->BatchBlit (dst->dfbsurface,
                                         cache->dfbsurface, rects, points, num));
        
    return CAIRO_STATUS_SUCCESS;
}
#endif /* DFB_SHOW_GLYPHS */


static cairo_bool_t
_cairo_directfb_surface_is_similar (void *surface_a, void *surface_b, cairo_content_t content)
{
    cairo_directfb_surface_t *a = (cairo_directfb_surface_t *) surface_a;
    cairo_directfb_surface_t *b = (cairo_directfb_surface_t *) surface_b;

    return a->dfb == b->dfb;
}

static cairo_surface_backend_t cairo_directfb_surface_backend = {
         CAIRO_SURFACE_TYPE_DIRECTFB, /*type*/
        _cairo_directfb_surface_create_similar,/*create_similar*/
        _cairo_directfb_surface_finish, /*finish*/
        _cairo_directfb_surface_acquire_source_image,/*acquire_source_image*/
        _cairo_directfb_surface_release_source_image,/*release_source_image*/
        _cairo_directfb_surface_acquire_dest_image,/*acquire_dest_image*/
        _cairo_directfb_surface_release_dest_image,/*release_dest_image*/ 
        _cairo_directfb_surface_clone_similar,/*clone_similar*/
#if DFB_COMPOSITE
        _cairo_directfb_surface_composite,/*composite*/
#else
        NULL,/*composite*/
#endif
#if DFB_RECTANGLES
        _cairo_directfb_surface_fill_rectangles,/*fill_rectangles*/
#else
        NULL,/*fill_rectangles*/
#endif
#if DFB_COMPOSITE_TRAPEZOIDS
        _cairo_directfb_surface_composite_trapezoids,/*composite_trapezoids*/
#else
        NULL,/*composite_trapezoids*/
#endif
        NULL, /* copy_page */
        NULL, /* show_page */
        _cairo_directfb_surface_set_clip_region,/* set_clip_region */
        NULL, /* intersect_clip_path */
        _cairo_directfb_abstract_surface_get_extents,/* get_extents */
        NULL, /* old_show_glyphs */
        NULL, /* get_font_options */
        NULL, /* flush */
        NULL, /* mark_dirty_rectangle */
#if DFB_SHOW_GLYPHS
        _cairo_directfb_surface_scaled_font_fini,/* scaled_font_fini */
        _cairo_directfb_surface_scaled_glyph_fini,/* scaled_glyph_fini */
#else
        NULL,
        NULL,
#endif
        NULL, /* paint */
        NULL, /* mask */
        NULL, /* stroke */
        NULL, /* fill */
#if DFB_SHOW_GLYPHS
        _cairo_directfb_surface_show_glyphs,/* show_glyphs */
#else
        NULL, /* show_glyphs */
#endif
        NULL, /* snapshot */
        _cairo_directfb_surface_is_similar,
        NULL /* reset */
};


static void
cairo_directfb_surface_backend_init (IDirectFB *dfb)
{
    static int done = 0;
    
    if (done)
        return;
        
    if (getenv ("CAIRO_DIRECTFB_NO_ACCEL")) {
#if DFB_RECTANGLES
        cairo_directfb_surface_backend.fill_rectangles = NULL;
#endif
#if DFB_COMPOSITE
        cairo_directfb_surface_backend.composite = NULL;
#endif
#if DFB_COMPOSITE_TRAPEZOIDS
        cairo_directfb_surface_backend.composite_trapezoids = NULL;
#endif
#if DFB_SHOW_GLYPHS
        cairo_directfb_surface_backend.scaled_font_fini = NULL;
        cairo_directfb_surface_backend.scaled_glyph_fini = NULL;
        cairo_directfb_surface_backend.show_glyphs = NULL;
#endif
        D_DEBUG_AT (Cairo_DirectFB, "Acceleration disabled.\n");
    }
    else {
        DFBGraphicsDeviceDescription dsc;
        
        dfb->GetDeviceDescription (dfb, &dsc);
    
#if DFB_COMPOSITE
        if (!(dsc.acceleration_mask & DFXL_BLIT))
            cairo_directfb_surface_backend.composite = NULL;
#endif

#if DFB_COMPOSITE_TRAPEZOIDS
        if (!(dsc.acceleration_mask & DFXL_TEXTRIANGLES))
            cairo_directfb_surface_backend.composite_trapezoids = NULL;
#endif
    }
    
    if (getenv ("CAIRO_DIRECTFB_ARGB_FONT")) {
        _directfb_argb_font = 1;
        D_DEBUG_AT (Cairo_DirectFB, "Using ARGB fonts.\n");
    }

    done = 1;
}   


cairo_surface_t *
cairo_directfb_surface_create (IDirectFB *dfb, IDirectFBSurface *dfbsurface) 
{
    cairo_directfb_surface_t *surface;
    DFBSurfacePixelFormat     format;
   
    D_ASSERT (dfb != NULL);
    D_ASSERT (dfbsurface != NULL);

    cairo_directfb_surface_backend_init (dfb);
        
    surface = calloc (1, sizeof(cairo_directfb_surface_t));
    if (!surface)
        return NULL;
     
    dfbsurface->AddRef (dfbsurface);   
    dfbsurface->GetPixelFormat (dfbsurface, &format);
    dfbsurface->GetSize (dfbsurface, &surface->width, &surface->height);
    surface->dfb = dfb;
    surface->dfbsurface = dfbsurface;  
    surface->format = _directfb_to_cairo_format (format);
    surface->content = _directfb_format_to_content (format);

    _cairo_surface_init (&surface->base, 
                         &cairo_directfb_surface_backend, surface->content);  

    return &surface->base;
}

