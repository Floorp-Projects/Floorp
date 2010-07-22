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
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Karl Tomlinson <karlt+@karlt.net>, Mozilla Corporation
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

#include "gfxXlibNativeRenderer.h"

#include "gfxXlibSurface.h"
#include "gfxContext.h"
#include "cairo-xlib.h"
#include "cairo-xlib-xrender.h"
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
#define NATIVE_DRAWING_NOTE(m) fprintf(stderr, m)
#else
#define NATIVE_DRAWING_NOTE(m) do {} while (0)
#endif

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
_convert_coord_to_int (double coord, PRInt32 *v)
{
    *v = (PRInt32)coord;
    /* XXX allow some tolerance here? */
    return *v == coord;
}

static PRBool
_get_rectangular_clip (cairo_t *cr,
                       const nsIntRect& bounds,
                       PRBool *need_clip,
                       nsIntRect *rectangles, int max_rectangles,
                       int *num_rectangles)
{
    cairo_rectangle_list_t *cliplist;
    cairo_rectangle_t *clips;
    int i;
    PRBool retval = PR_TRUE;

    cliplist = cairo_copy_clip_rectangle_list (cr);
    if (cliplist->status != CAIRO_STATUS_SUCCESS) {
        retval = PR_FALSE;
        NATIVE_DRAWING_NOTE("TAKING SLOW PATH: non-rectangular clip\n");
        goto FINISH;
    }

    /* the clip is always in surface backend coordinates (i.e. native backend coords) */
    clips = cliplist->rectangles;

    for (i = 0; i < cliplist->num_rectangles; ++i) {
        
        nsIntRect rect;
        if (!_convert_coord_to_int (clips[i].x, &rect.x) ||
            !_convert_coord_to_int (clips[i].y, &rect.y) ||
            !_convert_coord_to_int (clips[i].width, &rect.width) ||
            !_convert_coord_to_int (clips[i].height, &rect.height))
        {
            retval = PR_FALSE;
            NATIVE_DRAWING_NOTE("TAKING SLOW PATH: non-integer clip\n");
            goto FINISH;
        }

        if (rect == bounds) {
            /* the bounds are entirely inside the clip region so we don't need to clip. */
            *need_clip = PR_FALSE;
            goto FINISH;
        }            

        NS_ASSERTION(bounds.Contains(rect),
                     "Was expecting to be clipped to bounds");

        if (i >= max_rectangles) {
            retval = PR_FALSE;
            NATIVE_DRAWING_NOTE("TAKING SLOW PATH: unsupported clip rectangle count\n");
            goto FINISH;
        }

        rectangles[i] = rect;
    }
  
    *need_clip = PR_TRUE;
    *num_rectangles = cliplist->num_rectangles;

FINISH:
    cairo_rectangle_list_destroy (cliplist);

    return retval;
}

#define MAX_STATIC_CLIP_RECTANGLES 50

/**
 * Try the direct path.
 * @return True if we took the direct path
 */
PRBool
gfxXlibNativeRenderer::DrawDirect(gfxContext *ctx, nsIntSize size,
                                  PRUint32 flags,
                                  Screen *screen, Visual *visual)
{
    cairo_t *cr = ctx->GetCairo();

    /* Check that the operator is OVER */
    if (cairo_get_operator (cr) != CAIRO_OPERATOR_OVER) {
        NATIVE_DRAWING_NOTE("TAKING SLOW PATH: non-OVER operator\n");
        return PR_FALSE;
    }
    
    /* Check that the target surface is an xlib surface. */
    cairo_surface_t *target = cairo_get_group_target (cr);
    if (cairo_surface_get_type (target) != CAIRO_SURFACE_TYPE_XLIB) {
        NATIVE_DRAWING_NOTE("TAKING SLOW PATH: non-X surface\n");
        return PR_FALSE;
    }
    
    /* Check that the screen is supported.
       Visuals belong to screens, so, if alternate visuals are not supported,
       then alternate screens cannot be supported. */  
    PRBool supports_alternate_visual =
        (flags & DRAW_SUPPORTS_ALTERNATE_VISUAL) != 0;
    PRBool supports_alternate_screen = supports_alternate_visual
        && (flags & DRAW_SUPPORTS_ALTERNATE_SCREEN);
    if (!supports_alternate_screen &&
        cairo_xlib_surface_get_screen (target) != screen) {
        NATIVE_DRAWING_NOTE("TAKING SLOW PATH: non-default screen\n");
        return PR_FALSE;
    }
        
    /* Check that there is a visual */
    Visual *target_visual = cairo_xlib_surface_get_visual (target);
    if (!target_visual) {
        NATIVE_DRAWING_NOTE("TAKING SLOW PATH: no Visual for surface\n");
        return PR_FALSE;
    }        
    /* Check that the visual is supported */
    if (!supports_alternate_visual && target_visual != visual) {
        // Only the format of the visual is important (not the GLX properties)
        // for Xlib or XRender drawing.
        XRenderPictFormat *target_format =
            cairo_xlib_surface_get_xrender_format (target);
        if (!target_format ||
            (target_format !=
             XRenderFindVisualFormat (DisplayOfScreen(screen), visual))) {
            NATIVE_DRAWING_NOTE("TAKING SLOW PATH: unsupported Visual\n");
            return PR_FALSE;
        }
    }
  
    cairo_matrix_t matrix;
    cairo_get_matrix (cr, &matrix);
    double device_offset_x, device_offset_y;
    cairo_surface_get_device_offset (target, &device_offset_x, &device_offset_y);

    /* Draw() checked that the matrix contained only a very-close-to-integer
       translation.  Here (and in several other places and thebes) device
       offsets are assumed to be integer. */
    NS_ASSERTION(PRUint32(device_offset_x) == device_offset_x &&
                 PRUint32(device_offset_y) == device_offset_y,
                 "Expected integer device offsets");
    nsIntPoint offset(NS_lroundf(matrix.x0 + device_offset_x),
                      NS_lroundf(matrix.y0 + device_offset_y));
    
    int max_rectangles = 0;
    if (flags & DRAW_SUPPORTS_CLIP_RECT) {
      max_rectangles = 1;
    }
    if (flags & DRAW_SUPPORTS_CLIP_LIST) {
      max_rectangles = MAX_STATIC_CLIP_RECTANGLES;
    }

    /* The client won't draw outside the surface so consider this when
       analysing clip rectangles. */
    nsIntRect bounds(offset, size);
    bounds.IntersectRect(bounds,
                         nsIntRect(0, 0,
                                   cairo_xlib_surface_get_width(target),
                                   cairo_xlib_surface_get_height(target)));

    PRBool needs_clip;
    nsIntRect rectangles[MAX_STATIC_CLIP_RECTANGLES];
    int rect_count;

    /* Check that the clip is rectangular and aligned on unit boundaries. */
    /* Temporarily set the matrix for _get_rectangular_clip. It's basically
       the identity matrix, but we must adjust for the fact that our
       offset-rect is in device coordinates. */
    cairo_identity_matrix (cr);
    cairo_translate (cr, -device_offset_x, -device_offset_y);
    PRBool have_rectangular_clip =
        _get_rectangular_clip (cr, bounds, &needs_clip,
                               rectangles, max_rectangles, &rect_count);
    cairo_set_matrix (cr, &matrix);
    if (!have_rectangular_clip)
        return PR_FALSE;

    /* Draw only calls this function when the clip region is not empty. */
    NS_ASSERTION(!needs_clip || rect_count != 0,
                 "Where did the clip region go?");
      
    /* we're good to go! */
    NATIVE_DRAWING_NOTE("TAKING FAST PATH\n");
    cairo_surface_flush (target);
    nsRefPtr<gfxASurface> surface = gfxASurface::Wrap(target);
    nsresult rv = DrawWithXlib(static_cast<gfxXlibSurface*>(surface.get()),
                               offset, rectangles,
                               needs_clip ? rect_count : 0);
    if (NS_SUCCEEDED(rv)) {
        cairo_surface_mark_dirty (target);
        return PR_TRUE;
    }
    return PR_FALSE;
}

static PRBool
FormatHasAlpha(const XRenderPictFormat *format) {
    if (!format)
        return false;

    if (format->type != PictTypeDirect)
        return false;

    return format->direct.alphaMask != 0;
}

static already_AddRefed<gfxXlibSurface>
_create_temp_xlib_surface (cairo_t *cr, nsIntSize size,
                           PRUint32 flags, Screen *screen, Visual *visual)
{
    Drawable drawable = None;

    // For opaque drawing, set up the temp surface for copying to the target.
    // For non-opaque drawing we read back anyway so just use the
    // prefered screen and visual.
    cairo_surface_t *target = cairo_get_group_target (cr);
    if ((flags & gfxXlibNativeRenderer::DRAW_IS_OPAQUE)
        && cairo_surface_get_type (target) == CAIRO_SURFACE_TYPE_XLIB) {

        Screen *target_screen = cairo_xlib_surface_get_screen (target);
        PRBool supports_alternate_visual =
            (flags & gfxXlibNativeRenderer::DRAW_SUPPORTS_ALTERNATE_VISUAL) != 0;
        PRBool supports_alternate_screen = supports_alternate_visual
            && (flags & gfxXlibNativeRenderer::DRAW_SUPPORTS_ALTERNATE_SCREEN);
        if (target_screen == screen || supports_alternate_screen) {

            if (supports_alternate_visual) {
                Visual *target_visual = cairo_xlib_surface_get_visual (target);
                if (target_visual &&
                    (!FormatHasAlpha(cairo_xlib_surface_get_xrender_format (target)))) {
                    visual = target_visual;
                } else if (target_screen != screen) {
                    visual = DefaultVisualOfScreen (target_screen);
                }
            }

            drawable = cairo_xlib_surface_get_drawable (target);
            screen = target_screen;
        }
    }

    if (!drawable) {
        drawable = RootWindowOfScreen (screen);
    }
    return gfxXlibSurface::Create(screen, visual,
                                  gfxIntSize(size.width, size.height),
                                  drawable);
}

PRBool
gfxXlibNativeRenderer::DrawOntoTempSurface(gfxXlibSurface *tempXlibSurface,
                                           nsIntPoint offset)
{
    cairo_surface_t *temp_xlib_surface = tempXlibSurface->CairoSurface();
    cairo_surface_flush (temp_xlib_surface);
    /* no clipping is needed because the callback can't draw outside the native
       surface anyway */
    nsresult rv = DrawWithXlib(tempXlibSurface, offset, NULL, 0);
    cairo_surface_mark_dirty (temp_xlib_surface);
    return NS_SUCCEEDED(rv);
}

static cairo_surface_t *
_copy_xlib_surface_to_image (gfxXlibSurface *tempXlibSurface,
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
    cairo_set_source_surface (cr, tempXlibSurface->CairoSurface(), 0, 0);
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
                       gfxXlibNativeRenderer::DrawOutput *analysis)
{
    int num_pixels = width*height;
    int i;
    uint32_t first;
    uint32_t deltas = 0;
    unsigned char first_alpha;
  
    if (num_pixels == 0) {
        if (analysis) {
            analysis->mUniformAlpha = True;
            analysis->mUniformColor = True;
            /* whatever we put here will be true */
            analysis->mColor = gfxRGBA(0.0, 0.0, 0.0, 1.0);
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
        analysis->mUniformAlpha = (deltas >> 24) == 0;
        if (analysis->mUniformAlpha) {
            analysis->mColor.a = first_alpha/255.0;
            /* we only set uniform_color when the alpha is already uniform.
               it's only useful in that case ... and if the alpha was nonuniform
               then computing whether the color is uniform would require unpremultiplying
               every pixel */
            analysis->mUniformColor = (deltas & ~(0xFF << 24)) == 0;
            if (analysis->mUniformColor) {
                if (first_alpha == 0) {
                    /* can't unpremultiply, this is OK */
                    analysis->mColor = gfxRGBA(0.0, 0.0, 0.0, 0.0);
                } else {
                    double d_first_alpha = first_alpha;
                    analysis->mColor.r = (first & 0xFF)/d_first_alpha;
                    analysis->mColor.g = ((first >> 8) & 0xFF)/d_first_alpha;
                    analysis->mColor.b = ((first >> 16) & 0xFF)/d_first_alpha;
                }
            }
        }
    }
}

void
gfxXlibNativeRenderer::Draw(gfxContext* ctx, nsIntSize size,
                            PRUint32 flags, Screen *screen, Visual *visual,
                            DrawOutput* result)
{
    cairo_surface_t *black_image_surface;
    cairo_surface_t *white_image_surface;
    unsigned char *black_data;
    unsigned char *white_data;
  
    if (result) {
        result->mSurface = NULL;
        result->mUniformAlpha = PR_FALSE;
        result->mUniformColor = PR_FALSE;
    }
    
    PRBool matrixIsIntegerTranslation =
        !ctx->CurrentMatrix().HasNonIntegerTranslation();

    // The padding of 0.5 for non-pixel-exact transformations used here is
    // the same as what _cairo_pattern_analyze_filter uses.
    const gfxFloat filterRadius = 0.5;
    gfxRect affectedRect(0.0, 0.0, size.width, size.height);
    if (!matrixIsIntegerTranslation) {
        // The filter footprint means that the affected rectangle is a
        // little larger than the drawingRect;
        affectedRect.Outset(filterRadius);

        NATIVE_DRAWING_NOTE("TAKING SLOW PATH: matrix not integer translation\n");
    }
    // Clipping to the region affected by drawing allows us to consider only
    // the portions of the clip region that will be affected by drawing.
    gfxRect clipExtents;
    {
        gfxContextAutoSaveRestore autoSR(ctx);
        ctx->Clip(affectedRect);

        clipExtents = ctx->GetClipExtents();
        if (clipExtents.IsEmpty())
            return; // nothing to do

        if (matrixIsIntegerTranslation &&
            DrawDirect(ctx, size, flags, screen, visual))
            return;
    }

    nsIntRect drawingRect(nsIntPoint(0, 0), size);
    PRBool drawIsOpaque = (flags & DRAW_IS_OPAQUE) != 0;
    if (drawIsOpaque || !result) {
        // Drawing need only be performed within the clip extents
        // (and padding for the filter).
        if (!matrixIsIntegerTranslation) {
            // The source surface may need to be a little larger than the clip
            // extents due to the filter footprint.  
            clipExtents.Outset(filterRadius);
        }
        clipExtents.RoundOut();

        nsIntRect intExtents(PRInt32(clipExtents.X()),
                             PRInt32(clipExtents.Y()),
                             PRInt32(clipExtents.Width()),
                             PRInt32(clipExtents.Height()));
        drawingRect.IntersectRect(drawingRect, intExtents);
    }
    gfxPoint offset(drawingRect.x, drawingRect.y);

    cairo_t *cr = ctx->GetCairo();
    nsRefPtr<gfxXlibSurface> tempXlibSurface = 
        _create_temp_xlib_surface (cr, drawingRect.Size(),
                                   flags, screen, visual);
    if (tempXlibSurface == NULL)
        return;
  
    nsRefPtr<gfxContext> tmpCtx;
    if (!drawIsOpaque) {
        tmpCtx = new gfxContext(tempXlibSurface);
        tmpCtx->SetOperator(gfxContext::OPERATOR_CLEAR);
        tmpCtx->Paint();
    }

    if (!DrawOntoTempSurface(tempXlibSurface, -drawingRect.TopLeft())) {
        return;
    }
  
    if (drawIsOpaque) {
        ctx->SetSource(tempXlibSurface, offset);
        ctx->Paint();
        if (result) {
            result->mSurface = tempXlibSurface;
            /* fill in the result with what we know, which is really just what our
               assumption was */
            result->mUniformAlpha = PR_TRUE;
            result->mColor.a = 1.0;
        }
        return;
    }
    
    int width = drawingRect.width;
    int height = drawingRect.height;
    black_image_surface =
        _copy_xlib_surface_to_image (tempXlibSurface, CAIRO_FORMAT_ARGB32,
                                     width, height, &black_data);
    
    tmpCtx->SetDeviceColor(gfxRGBA(1.0, 1.0, 1.0));
    tmpCtx->SetOperator(gfxContext::OPERATOR_SOURCE);
    tmpCtx->Paint();
    DrawOntoTempSurface(tempXlibSurface, -drawingRect.TopLeft());
    white_image_surface =
        _copy_xlib_surface_to_image (tempXlibSurface, CAIRO_FORMAT_RGB24,
                                     width, height, &white_data);
  
    if (black_image_surface && white_image_surface &&
        cairo_surface_status (black_image_surface) == CAIRO_STATUS_SUCCESS &&
        cairo_surface_status (white_image_surface) == CAIRO_STATUS_SUCCESS &&
        black_data != NULL && white_data != NULL) {
        cairo_surface_flush (black_image_surface);
        cairo_surface_flush (white_image_surface);
        _compute_alpha_values ((uint32_t*)black_data, (uint32_t*)white_data, width, height, result);
        cairo_surface_mark_dirty (black_image_surface);
        
        cairo_set_source_surface (cr, black_image_surface, offset.x, offset.y);
        /* if the caller wants to retrieve the rendered image, put it into
           a 'similar' surface, and use that as the source for the drawing right
           now. This means we always return a surface similar to the surface
           used for 'cr', which is ideal if it's going to be cached and reused.
           We do not return an image if the result has uniform color and alpha. */
        if (result && (!result->mUniformAlpha || !result->mUniformColor)) {
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
            
            result->mSurface = gfxASurface::Wrap(similar_surface);
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
