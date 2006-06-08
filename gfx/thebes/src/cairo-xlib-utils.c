/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Novell code.
 *
 * The Initial Developer of the Original Code is Novell.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   rocallahan@novell.com
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "cairo-xlib-utils.h"

#include "cairo-xlib.h"
#include <stdlib.h>

#if   HAVE_STDINT_H
#include <stdint.h>
#elif HAVE_INTTYPES_H
#include <inttypes.h>
#elif HAVE_SYS_INT_TYPES_H
#include <sys/int_types.h>
#endif

#if 0
#include <stdio.h>
#define CAIRO_XLIB_DRAWING_NOTE(m) fprintf(stderr, m)
#else
#define CAIRO_XLIB_DRAWING_NOTE(m) do {} while (0)
#endif

static cairo_user_data_key_t pixmap_free_key;

typedef struct {
    Display *dpy;
    Pixmap   pixmap;
} pixmap_free_struct;

static void pixmap_free_func (void *data)
{
    pixmap_free_struct *pfs = (pixmap_free_struct *) data;
    XFreePixmap (pfs->dpy, pfs->pixmap);
    free (pfs);
}

/* We have three basic strategies available:
   1) 'direct': cr targets an xlib surface, and other conditions are met: we can
      pass the underlying drawable directly to the callback
   2) 'opaque': the image is opaque: we can create a temporary cairo xlib surface,
      pass its underlying drawable to the callback, and paint the result
      using cairo
   3) 'default': create a temporary cairo xlib surface, fill with black, pass its
      underlying drawable to the callback, copy the results to a cairo
      image surface, repeat with a white background, update the on-black
      image alpha values by comparing the two images, then paint the on-black
      image using cairo
   Sure would be nice to have an X extension to do 3 for us on the server...
*/

static cairo_bool_t
_convert_coord_to_short (double coord, short *v)
{
    *v = (short)coord;
    /* XXX allow some tolerance here? */
    return *v == coord;
}

static cairo_bool_t
_convert_coord_to_unsigned_short (double coord, unsigned short *v)
{
    *v = (unsigned short)coord;
    /* XXX allow some tolerance here? */
    return *v == coord;
}

static cairo_bool_t
_intersect_interval (double a_begin, double a_end, double b_begin, double b_end,
                     double *out_begin, double *out_end)
{
    *out_begin = a_begin;
    if (*out_begin < b_begin) {
        *out_begin = b_begin;
    }
    *out_end = a_end;
    if (*out_end > b_end) {
        *out_end = b_end;
    }
    return *out_begin < *out_end;
}

#define MAX_STATIC_CLIP_RECTANGLES 50
static cairo_bool_t
_get_rectangular_clip (cairo_t *cr,
                       int bounds_x, int bounds_y,
                       int bounds_width, int bounds_height,
                       cairo_bool_t *need_clip,
                       XRectangle *rectangles, int max_rectangles,
                       int *num_rectangles)
{
    cairo_clip_rect_t clips[MAX_STATIC_CLIP_RECTANGLES];
    int count;
    int i;
    double b_x = bounds_x;
    double b_y = bounds_y;
    double b_x_most = bounds_x + bounds_width;
    double b_y_most = bounds_y + bounds_height;
    int rect_count = 0;
    
    if (!cairo_has_clip (cr)) {
        *need_clip = False;
        return True;
    }

    if (!cairo_extract_clip_rectangles (cr, MAX_STATIC_CLIP_RECTANGLES, clips, &count))
        return False;

    for (i = 0; i < count; ++i) {
        double intersect_x, intersect_y, intersect_x_most, intersect_y_most;
        
        /* the clip is always in surface backend coordinates (i.e. native backend coords) */
        if (b_x >= clips[i].x && b_x_most <= clips[i].x + clips[i].width &&
            b_y >= clips[i].y && b_y_most <= clips[i].y + clips[i].height) {
            /* the bounds are entirely inside the clip region so we don't need to clip. */
            *need_clip = False;
            return True;
        }
        
        if (_intersect_interval (b_x, b_x_most, clips[i].x, clips[i].x + clips[i].width,
                                 &intersect_x, &intersect_x_most) &&
            _intersect_interval (b_y, b_y_most, clips[i].y, clips[i].y + clips[i].height,
                                 &intersect_y, &intersect_y_most)) {
            XRectangle *rect = &rectangles[rect_count];

            if (rect_count >= max_rectangles)
              return False;

            if (!_convert_coord_to_short (intersect_x, &rect->x) ||
                !_convert_coord_to_short (intersect_y, &rect->y) ||
                !_convert_coord_to_unsigned_short (intersect_x_most - intersect_x, &rect->width) ||
                !_convert_coord_to_unsigned_short (intersect_y_most - intersect_y, &rect->height))
              return False;

            ++rect_count;
        }
    }
  
    *need_clip = True;
    *num_rectangles = rect_count;
    return True;
}

/**
 * Try the direct path.
 * @param status the status returned by the callback, if we took the direct path
 * @return True if we took the direct path
 */
static cairo_bool_t
_draw_with_xlib_direct (cairo_t *cr,
                        Display *default_display,
                        cairo_xlib_drawing_callback callback,
                        void *closure,
                        int bounds_width, int bounds_height,
                        cairo_xlib_drawing_support_t capabilities)
{
    cairo_surface_t *target;
    Drawable d;
    cairo_matrix_t matrix;
    short offset_x, offset_y;
    cairo_bool_t needs_clip;
    XRectangle rectangles[MAX_STATIC_CLIP_RECTANGLES];
    int rect_count;
    double device_offset_x, device_offset_y;
    int max_rectangles;
    Display *dpy;
    Visual *visual;

    target = cairo_get_group_target (cr);
    cairo_surface_get_device_offset (target, &device_offset_x, &device_offset_y);
    d = cairo_xlib_surface_get_drawable (target);

    cairo_get_matrix (cr, &matrix);
    
    /* Check that the matrix is a pure translation */
    /* XXX test some approximation to == 1.0 here? */
    if (matrix.xx != 1.0 || matrix.yy != 1.0 || matrix.xy != 0.0 || matrix.yx != 0.0) {
        CAIRO_XLIB_DRAWING_NOTE("TAKING SLOW PATH: matrix not a pure translation\n");
        return False;
    }
    /* Check that the matrix translation offsets (adjusted for
       device offset) are integers */
    if (!_convert_coord_to_short (matrix.x0 + device_offset_x, &offset_x) ||
        !_convert_coord_to_short (matrix.y0 + device_offset_y, &offset_y)) {
        CAIRO_XLIB_DRAWING_NOTE("TAKING SLOW PATH: non-integer offset\n");
        return False;
    }
    
    max_rectangles = 0;
    if (capabilities & CAIRO_XLIB_DRAWING_SUPPORTS_CLIP_RECT) {
      max_rectangles = 1;
    }
    if (capabilities & CAIRO_XLIB_DRAWING_SUPPORTS_CLIP_LIST) {
      max_rectangles = MAX_STATIC_CLIP_RECTANGLES;
    }
    
    /* Check that the clip is rectangular and aligned on unit boundaries */
    if (!_get_rectangular_clip (cr,
                                offset_x, offset_y, bounds_width, bounds_height,
                                &needs_clip,
                                rectangles, max_rectangles, &rect_count)) {
        CAIRO_XLIB_DRAWING_NOTE("TAKING SLOW PATH: unsupported clip\n");
        return False;
    }
  
    /* Stop now if everything is clipped out */
    if (needs_clip && rect_count == 0) {
        CAIRO_XLIB_DRAWING_NOTE("TAKING FAST PATH: all clipped\n");
        return True;
    }
      
    /* Check that the operator is OVER */
    if (cairo_get_operator (cr) != CAIRO_OPERATOR_OVER) {
        CAIRO_XLIB_DRAWING_NOTE("TAKING SLOW PATH: non-OVER operator\n");
        return False;
    }
    
    /* Check that the offset is supported */  
    if (!(capabilities & CAIRO_XLIB_DRAWING_SUPPORTS_OFFSET) &&
        (offset_x != 0 || offset_y != 0)) {
        CAIRO_XLIB_DRAWING_NOTE("TAKING SLOW PATH: unsupported offset\n");
        return False;
    }
    
    /* Check that the target surface is an xlib surface. Do this late because
       we might complete early above when when the object to be drawn is
       completely clipped out. */
    if (!d) {
        CAIRO_XLIB_DRAWING_NOTE("TAKING SLOW PATH: non-X surface\n");
        return False;
    }
    
    /* Check that the display is supported */  
    dpy = cairo_xlib_surface_get_display (target);
    if (!(capabilities & CAIRO_XLIB_DRAWING_SUPPORTS_ALTERNATE_DISPLAY) &&
        dpy != default_display) {
        CAIRO_XLIB_DRAWING_NOTE("TAKING SLOW PATH: non-default display\n");
        return False;
    }
        
    /* Check that the visual is supported */  
    visual = cairo_xlib_surface_get_visual (target);
    if (!(capabilities & CAIRO_XLIB_DRAWING_SUPPORTS_NONDEFAULT_VISUAL) &&
        DefaultVisual (dpy, DefaultScreen (dpy)) != visual) {
        CAIRO_XLIB_DRAWING_NOTE("TAKING SLOW PATH: non-default visual\n");
        return False;
    }
  
    /* we're good to go! */
    CAIRO_XLIB_DRAWING_NOTE("TAKING FAST PATH\n");
    cairo_surface_flush (target);
    callback (closure, dpy, d, visual, offset_x, offset_y, rectangles,
              needs_clip ? rect_count : 0);
    cairo_surface_mark_dirty (target);
    return True;
}

static cairo_surface_t *
_create_temp_xlib_surface (cairo_t *cr, Display *dpy, int width, int height,
                           cairo_xlib_drawing_support_t capabilities)
{
    /* base the temp surface on the *screen* surface, not any intermediate
     * group surface, because the screen surface is more likely to have
     * characteristics that the xlib-using code is likely to be happy with */
    cairo_surface_t *target = cairo_get_target (cr);
    Drawable target_drawable = cairo_xlib_surface_get_drawable (target);

    int screen_index = DefaultScreen (dpy);
    Drawable drawable = RootWindow (dpy, screen_index);
    Screen *screen = DefaultScreenOfDisplay (dpy);
    Visual *visual = DefaultVisual (dpy, screen_index);
    int depth = DefaultDepth (dpy, screen_index);

    Pixmap pixmap;
    cairo_surface_t *result;
    pixmap_free_struct *pfs;
    
    /* make the temporary surface match target_drawable to the extent supported
       by the native rendering code */
    if (target_drawable) {
        Display *target_display = cairo_xlib_surface_get_display (target);
        Visual *target_visual = cairo_xlib_surface_get_visual (target);
        if ((target_display == dpy ||
             (capabilities & CAIRO_XLIB_DRAWING_SUPPORTS_ALTERNATE_DISPLAY)) &&
            (target_visual == visual ||
             (capabilities & CAIRO_XLIB_DRAWING_SUPPORTS_NONDEFAULT_VISUAL))) {
            drawable = target_drawable;
            dpy = target_display;
            visual = target_visual;
            screen = cairo_xlib_surface_get_screen (target);
            depth = cairo_xlib_surface_get_depth (target);
        }
    }
    
    pfs = malloc (sizeof(pixmap_free_struct));
    if (pfs == NULL)
        return NULL;
        
    pixmap = XCreatePixmap (dpy, drawable, width, height, depth);
    if (!pixmap) {
        free (pfs);
        return NULL;
    }
    pfs->dpy = dpy;
    pfs->pixmap = pixmap;
  
    result = cairo_xlib_surface_create (dpy, pixmap, visual, width, height);
    if (cairo_surface_status (result) != CAIRO_STATUS_SUCCESS) {
        pixmap_free_func (pfs);
        return NULL;
    }
    
    cairo_surface_set_user_data (result, &pixmap_free_key, pfs, pixmap_free_func);
    return result;
}

static cairo_bool_t
_draw_onto_temp_xlib_surface (cairo_surface_t *temp_xlib_surface,
                              cairo_xlib_drawing_callback callback,
                              void *closure,
                              double background_gray_value)
{
    Drawable d = cairo_xlib_surface_get_drawable (temp_xlib_surface);
    Display *dpy = cairo_xlib_surface_get_display (temp_xlib_surface);
    Visual *visual = cairo_xlib_surface_get_visual (temp_xlib_surface);
    cairo_bool_t result;
    cairo_t *cr = cairo_create (temp_xlib_surface);
    cairo_set_source_rgb (cr, background_gray_value, background_gray_value,
                          background_gray_value);
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint (cr);
    cairo_destroy (cr);
    
    cairo_surface_flush (temp_xlib_surface);
    /* no clipping is needed because the callback can't draw outside the native
       surface anyway */
    result = callback (closure, dpy, d, visual, 0, 0, NULL, 0);
    cairo_surface_mark_dirty (temp_xlib_surface);
    return result;
}

static cairo_surface_t *
_copy_xlib_surface_to_image (cairo_surface_t *temp_xlib_surface,
                             cairo_format_t format,
                             int width, int height,
                             unsigned char **data_out)
{
    unsigned char *data;
    cairo_surface_t *result;
    cairo_t *cr;
    
    *data_out = data = (unsigned char*)malloc (width*height*4);
    if (!data)
        return NULL;
  
    result = cairo_image_surface_create_for_data (data, format, width, height, width*4);
    cr = cairo_create (result);
    cairo_set_source_surface (cr, temp_xlib_surface, 0, 0);
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint (cr);
    cairo_destroy (cr);
    return result;
}

#define SET_ALPHA(v, a) (((v) & ~(0xFF << 24)) | ((a) << 24))
#define GREEN_OF(v) (((v) >> 8) & 0xFF)

/**
 * Given the RGB data for two image surfaces, one a source image composited
 * with OVER onto a black background, and one a source image composited with 
 * OVER onto a white background, reconstruct the original image data into
 * black_data.
 * 
 * Consider a single color channel and a given pixel. Suppose the original
 * premultiplied color value was C and the alpha value was A. Let the final
 * on-black color be B and the final on-white color be W. All values range
 * over 0-255.
 * Then B=C and W=(255*(255 - A) + C*255)/255. Solving for A, we get
 * A=255 - (W - C). Therefore it suffices to leave the black_data color
 * data alone and set the alpha values using that simple formula. It shouldn't
 * matter what color channel we pick for the alpha computation, but we'll
 * pick green because if we went through a color channel downsample the green
 * bits are likely to be the most accurate.
 */
static void
_compute_alpha_values (uint32_t *black_data,
                       uint32_t *white_data,
                       int width, int height,
                       cairo_xlib_drawing_result_t *analysis)
{
    int num_pixels = width*height;
    int i;
    uint32_t first;
    uint32_t deltas = 0;
    unsigned char first_alpha;
  
    if (num_pixels == 0) {
        if (analysis) {
            analysis->uniform_alpha = True;
            analysis->uniform_color = True;
            /* whatever we put here will be true */
            analysis->alpha = 1.0;
            analysis->r = analysis->g = analysis->b = 0.0;
        }
        return;
    }
  
    first_alpha = 255 - (GREEN_OF(*white_data) - GREEN_OF(*black_data));
    /* set the alpha value of 'first' */
    first = SET_ALPHA(*black_data, first_alpha);
  
    for (i = 0; i < num_pixels; ++i) {
        uint32_t black = *black_data;
        uint32_t white = *white_data;
        unsigned char pixel_alpha = 255 - (GREEN_OF(white) - GREEN_OF(black));
        
        black = SET_ALPHA(black, pixel_alpha);
        *black_data = black;
        deltas |= (first ^ black);
        
        black_data++;
        white_data++;
    }
    
    if (analysis) {
        analysis->uniform_alpha = (deltas >> 24) == 0;
        if (analysis->uniform_alpha) {
            analysis->alpha = first_alpha/255.0;
            /* we only set uniform_color when the alpha is already uniform.
               it's only useful in that case ... and if the alpha was nonuniform
               then computing whether the color is uniform would require unpremultiplying
               every pixel */
            analysis->uniform_color = (deltas & ~(0xFF << 24)) == 0;
            if (analysis->uniform_color) {
                if (first_alpha == 0) {
                    /* can't unpremultiply, this is OK */
                    analysis->r = analysis->g = analysis->b = 0.0;
                } else {
                    double d_first_alpha = first_alpha;
                    analysis->r = (first & 0xFF)/d_first_alpha;
                    analysis->g = ((first >> 8) & 0xFF)/d_first_alpha;
                    analysis->b = ((first >> 16) & 0xFF)/d_first_alpha;
                }
            }
        }
    }
}

void
cairo_draw_with_xlib (cairo_t *cr,
                      cairo_xlib_drawing_callback callback,
                      void *closure,
                      Display *dpy,
                      unsigned int width, unsigned int height,
                      cairo_xlib_drawing_opacity_t is_opaque,
                      cairo_xlib_drawing_support_t capabilities,
                      cairo_xlib_drawing_result_t *result)
{
    cairo_surface_t *temp_xlib_surface;
    cairo_surface_t *black_image_surface;
    cairo_surface_t *white_image_surface;
    unsigned char *black_data;
    unsigned char *white_data;
  
    if (result) {
        result->surface = NULL;
        result->uniform_alpha = False;
        result->uniform_color = False;
    }
    
    /* exit early if there's no work to do. This is actually important
       because we'll die with an X error if we try to create an empty temporary
       pixmap */
    if (width == 0 || height == 0)
        return;

    if (_draw_with_xlib_direct (cr, dpy, callback, closure, width, height,
                                capabilities))
        return;
  
    temp_xlib_surface = _create_temp_xlib_surface (cr, dpy, width, height,
                                                   capabilities);
    if (temp_xlib_surface == NULL)
        return;
    /* update 'dpy' to refer to the display that actually got used. Might
       be different now, if cr was referring to an xlib surface on a different display */
    dpy = cairo_xlib_surface_get_display (temp_xlib_surface);
  
    if (!_draw_onto_temp_xlib_surface (temp_xlib_surface, callback, closure, 0.0)) {
        cairo_surface_destroy (temp_xlib_surface);
        return;
    }
  
    if (is_opaque == CAIRO_XLIB_DRAWING_OPAQUE) {
        cairo_set_source_surface (cr, temp_xlib_surface, 0.0, 0.0);
        cairo_paint (cr);
        if (result) {
            result->surface = temp_xlib_surface;
            /* fill in the result with what we know, which is really just what our
               assumption was */
            result->uniform_alpha = True;
            result->alpha = 1.0;
        } else {
            cairo_surface_destroy (temp_xlib_surface);
        }
        return;
    }
    
    black_image_surface =
        _copy_xlib_surface_to_image (temp_xlib_surface, CAIRO_FORMAT_ARGB32,
                                     width, height, &black_data);
    
    _draw_onto_temp_xlib_surface (temp_xlib_surface, callback, closure, 1.0);
    white_image_surface =
        _copy_xlib_surface_to_image (temp_xlib_surface, CAIRO_FORMAT_RGB24,
                                     width, height, &white_data);
  
    cairo_surface_destroy (temp_xlib_surface);
    
    if (black_image_surface && white_image_surface &&
        cairo_surface_status (black_image_surface) == CAIRO_STATUS_SUCCESS &&
        cairo_surface_status (white_image_surface) == CAIRO_STATUS_SUCCESS &&
        black_data != NULL && white_data != NULL) {
        cairo_surface_flush (black_image_surface);
        cairo_surface_flush (white_image_surface);
        _compute_alpha_values ((uint32_t*)black_data, (uint32_t*)white_data, width, height, result);
        cairo_surface_mark_dirty (black_image_surface);
        
        cairo_set_source_surface (cr, black_image_surface, 0.0, 0.0);
        /* if the caller wants to retrieve the rendered image, put it into
           a 'similar' surface, and use that as the source for the drawing right
           now. This means we always return a surface similar to the surface
           used for 'cr', which is ideal if it's going to be cached and reused.
           We do not return an image if the result has uniform color and alpha. */
        if (result && (!result->uniform_alpha || !result->uniform_color)) {
            cairo_surface_t *target = cairo_get_group_target (cr);
            cairo_surface_t *similar_surface =
                cairo_surface_create_similar (target, CAIRO_CONTENT_COLOR_ALPHA,
                                              width, height);
            cairo_t *copy_cr = cairo_create (similar_surface);
            cairo_set_source_surface (copy_cr, black_image_surface, 0.0, 0.0);
            cairo_set_operator (copy_cr, CAIRO_OPERATOR_SOURCE);
            cairo_paint (copy_cr);
            cairo_destroy (copy_cr);
      
            cairo_set_source_surface (cr, similar_surface, 0.0, 0.0);
            
            result->surface = similar_surface;
        }
        
        cairo_paint (cr);
    }
    
    if (black_image_surface) {
        cairo_surface_destroy (black_image_surface);
    }
    if (white_image_surface) {
        cairo_surface_destroy (white_image_surface);
    }
    free (black_data);
    free (white_data);
}
