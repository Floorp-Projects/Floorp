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
#include "gfxImageSurface.h"
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

/* We have four basic strategies available:

   1) 'direct': If the target is an xlib surface, and other conditions are met,
      we can pass the underlying drawable directly to the callback.

   2) 'simple': If the drawing is opaque, or we can draw to a surface with an
      alpha channel, then we can create a temporary xlib surface, pass its
      underlying drawable to the callback, and composite the result using
      cairo.

   3) 'copy-background': If the drawing is not opaque but the target is
      opaque, and we can draw to a surface with format such that pixel
      conversion to and from the target format is exact, we can create a
      temporary xlib surface, copy the background from the target, pass the
      underlying drawable to the callback, and copy back to the target.

      This strategy is not used if the pixel format conversion is not exact,
      because that would mean that drawing intended to be very transparent
      messes with other content.

      The strategy is prefered over simple for non-opaque drawing and opaque
      targets on the same screen as compositing without alpha is a simpler
      operation.

   4) 'alpha-extraction': create a temporary xlib surface, fill with black,
      pass its underlying drawable to the callback, copy the results to a
      cairo image surface, repeat with a white background, update the on-black
      image alpha values by comparing the two images, then paint the on-black
      image using cairo.

      Sure would be nice to have an X extension or GL to do this for us on the
      server...
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
        NATIVE_DRAWING_NOTE("FALLBACK: non-rectangular clip");
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
            NATIVE_DRAWING_NOTE("FALLBACK: non-integer clip");
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
            NATIVE_DRAWING_NOTE("FALLBACK: unsupported clip rectangle count");
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

    /* Check that the target surface is an xlib surface. */
    cairo_surface_t *target = cairo_get_group_target (cr);
    if (cairo_surface_get_type (target) != CAIRO_SURFACE_TYPE_XLIB) {
        NATIVE_DRAWING_NOTE("FALLBACK: non-X surface");
        return PR_FALSE;
    }
    
    /* Check that the screen is supported.
       Visuals belong to screens, so, if alternate visuals are not supported,
       then alternate screens cannot be supported. */  
    PRBool supports_alternate_visual =
        (flags & DRAW_SUPPORTS_ALTERNATE_VISUAL) != 0;
    PRBool supports_alternate_screen = supports_alternate_visual &&
        (flags & DRAW_SUPPORTS_ALTERNATE_SCREEN);
    if (!supports_alternate_screen &&
        cairo_xlib_surface_get_screen (target) != screen) {
        NATIVE_DRAWING_NOTE("FALLBACK: non-default screen");
        return PR_FALSE;
    }
        
    /* Check that there is a visual */
    Visual *target_visual = cairo_xlib_surface_get_visual (target);
    if (!target_visual) {
        NATIVE_DRAWING_NOTE("FALLBACK: no Visual for surface");
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
            NATIVE_DRAWING_NOTE("FALLBACK: unsupported Visual");
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
VisualHasAlpha(Screen *screen, Visual *visual) {
    // There may be some other visuals format with alpha but usually this is
    // the only one we care about.
    return visual->c_class == TrueColor &&
        visual->bits_per_rgb == 8 &&
        visual->red_mask == 0xff0000 &&
        visual->green_mask == 0xff00 &&
        visual->blue_mask == 0xff &&
        gfxXlibSurface::DepthOfVisual(screen, visual) == 32;
}

// Returns whether pixel conversion between visual and format is exact (in
// both directions).
static PRBool
FormatConversionIsExact(Screen *screen, Visual *visual, XRenderPictFormat *format) {
    if (!format ||
        visual->c_class != TrueColor ||
        format->type != PictTypeDirect ||
        gfxXlibSurface::DepthOfVisual(screen, visual) != format->depth)
        return PR_FALSE;

    XRenderPictFormat *visualFormat =
        XRenderFindVisualFormat(DisplayOfScreen(screen), visual);

    if (visualFormat->type != PictTypeDirect )
        return PR_FALSE;

    const XRenderDirectFormat& a = visualFormat->direct;
    const XRenderDirectFormat& b = format->direct;
    return a.redMask == b.redMask &&
        a.greenMask == b.greenMask &&
        a.blueMask == b.blueMask;
}

// The 3 non-direct strategies described above.
// The surface format and strategy are inter-dependent.
enum DrawingMethod {
    eSimple,
    eCopyBackground,
    eAlphaExtraction
};

static already_AddRefed<gfxXlibSurface>
CreateTempXlibSurface (gfxASurface *destination, nsIntSize size,
                       PRBool canDrawOverBackground,
                       PRUint32 flags, Screen *screen, Visual *visual,
                       DrawingMethod *method)
{
    PRBool drawIsOpaque = (flags & gfxXlibNativeRenderer::DRAW_IS_OPAQUE) != 0;
    PRBool supportsAlternateVisual =
        (flags & gfxXlibNativeRenderer::DRAW_SUPPORTS_ALTERNATE_VISUAL) != 0;
    PRBool supportsAlternateScreen = supportsAlternateVisual &&
        (flags & gfxXlibNativeRenderer::DRAW_SUPPORTS_ALTERNATE_SCREEN);

    cairo_surface_t *target = destination->CairoSurface();
    cairo_surface_type_t target_type = cairo_surface_get_type (target);
    cairo_content_t target_content = cairo_surface_get_content (target);

    Screen *target_screen = target_type == CAIRO_SURFACE_TYPE_XLIB ?
        cairo_xlib_surface_get_screen (target) : screen;

    // When the background has an alpha channel, we need to draw with an alpha
    // channel anyway, so there is no need to copy the background.  If
    // doCopyBackground is set here, we'll also need to check below that the
    // background can copied without any loss in format conversions.
    PRBool doCopyBackground = !drawIsOpaque && canDrawOverBackground &&
        target_content == CAIRO_CONTENT_COLOR;

    if (supportsAlternateScreen && screen != target_screen && drawIsOpaque) {
        // Prefer a visual on the target screen.
        // (If !drawIsOpaque, we'll need doCopyBackground or an alpha channel.)
        visual = DefaultVisualOfScreen(target_screen);
        screen = target_screen;

    } else if (doCopyBackground || (supportsAlternateVisual && drawIsOpaque)) {
        // Analyse the pixel formats either to check whether we can
        // doCopyBackground or to see if we can find a better visual for
        // opaque drawing.
        Visual *target_visual = NULL;
        XRenderPictFormat *target_format = NULL;
        switch (target_type) {
        case CAIRO_SURFACE_TYPE_XLIB:
            target_visual = cairo_xlib_surface_get_visual (target);
            target_format = cairo_xlib_surface_get_xrender_format (target);
            break;
        case CAIRO_SURFACE_TYPE_IMAGE: {
            gfxASurface::gfxImageFormat imageFormat =
                static_cast<gfxImageSurface*>(destination)->Format();
            target_visual = gfxXlibSurface::FindVisual(screen, imageFormat);
            Display *dpy = DisplayOfScreen(screen);
            if (target_visual) {
                target_format = XRenderFindVisualFormat(dpy, visual);
            } else {
                target_format =
                    gfxXlibSurface::FindRenderFormat(dpy, imageFormat);
            }                
            break;
        }
        default:
            break;
        }

        if (supportsAlternateVisual &&
            (supportsAlternateScreen || screen == target_screen)) {
            if (target_visual) {
                visual = target_visual;
                screen = target_screen;
            }
        }
        // Could try harder to match formats across screens for background
        // copying when !supportsAlternateScreen, if we cared.  Preferably
        // we'll find a visual below with an alpha channel anyway; if so, the
        // background won't need to be copied.

        if (doCopyBackground && visual != target_visual &&
            !FormatConversionIsExact(screen, visual, target_format)) {
            doCopyBackground = PR_FALSE;
        }
    }

    if (supportsAlternateVisual && !drawIsOpaque &&
        (screen != target_screen ||
         !(doCopyBackground || VisualHasAlpha(screen, visual)))) {
        // Try to find a visual with an alpha channel.
        Screen *visualScreen =
            supportsAlternateScreen ? target_screen : screen;
        Visual *argbVisual =
            gfxXlibSurface::FindVisual(visualScreen,
                                       gfxASurface::ImageFormatARGB32);
        if (argbVisual) {
            visual = argbVisual;
            screen = visualScreen;
        } else if (!doCopyBackground &&
                   gfxXlibSurface::DepthOfVisual(screen, visual) != 24) {
            // Will need to do alpha extraction; prefer a 24-bit visual.
            // No advantage in using the target screen.
            Visual *rgb24Visual =
                gfxXlibSurface::FindVisual(screen,
                                           gfxASurface::ImageFormatRGB24);
            if (rgb24Visual) {
                visual = rgb24Visual;
            }
        }
    }

    Drawable drawable =
        (screen == target_screen && target_type == CAIRO_SURFACE_TYPE_XLIB) ?
        cairo_xlib_surface_get_drawable (target) : RootWindowOfScreen(screen);

    nsRefPtr<gfxXlibSurface> surface =
        gfxXlibSurface::Create(screen, visual,
                               gfxIntSize(size.width, size.height),
                               drawable);

    if (drawIsOpaque ||
        surface->GetContentType() == gfxASurface::CONTENT_COLOR_ALPHA) {
        NATIVE_DRAWING_NOTE(drawIsOpaque ?
                            ", SIMPLE OPAQUE\n" : ", SIMPLE WITH ALPHA");
        *method = eSimple;
    } else if (doCopyBackground) {
        NATIVE_DRAWING_NOTE(", COPY BACKGROUND\n");
        *method = eCopyBackground;
    } else {
        NATIVE_DRAWING_NOTE(", SLOW ALPHA EXTRACTION\n");
        *method = eAlphaExtraction;
    }

    return surface.forget();
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

    PRBool drawIsOpaque = (flags & DRAW_IS_OPAQUE) != 0;
    gfxMatrix matrix = ctx->CurrentMatrix();

    // We can only draw direct or onto a copied background if pixels align and
    // native drawing is compatible with the current operator.  (The matrix is
    // actually also pixel-exact for flips and right-angle rotations, which
    // would permit copying the background but not drawing direct.)
    PRBool matrixIsIntegerTranslation = !matrix.HasNonIntegerTranslation();
    PRBool canDrawOverBackground = matrixIsIntegerTranslation &&
        ctx->CurrentOperator() == gfxContext::OPERATOR_OVER;

    // The padding of 0.5 for non-pixel-exact transformations used here is
    // the same as what _cairo_pattern_analyze_filter uses.
    const gfxFloat filterRadius = 0.5;
    gfxRect affectedRect(0.0, 0.0, size.width, size.height);
    if (!matrixIsIntegerTranslation) {
        // The filter footprint means that the affected rectangle is a
        // little larger than the drawingRect;
        affectedRect.Outset(filterRadius);

        NATIVE_DRAWING_NOTE("FALLBACK: matrix not integer translation");
    } else if (!canDrawOverBackground) {
        NATIVE_DRAWING_NOTE("FALLBACK: unsupported operator");
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

        if (canDrawOverBackground &&
            DrawDirect(ctx, size, flags, screen, visual))
            return;
    }

    nsIntRect drawingRect(nsIntPoint(0, 0), size);
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
    gfxPoint offset(drawingRect.x, drawingRect.y);

    DrawingMethod method;
    nsRefPtr<gfxASurface> target = ctx->CurrentSurface();
    nsRefPtr<gfxXlibSurface> tempXlibSurface = 
        CreateTempXlibSurface(target, drawingRect.Size(),
                              canDrawOverBackground, flags, screen, visual,
                              &method);
    if (!tempXlibSurface)
        return;
  
    if (drawingRect.Size() != size || method == eCopyBackground) {
        // Only drawing a portion, or copying background,
        // so won't return a result.
        result = NULL;
    }

    nsRefPtr<gfxContext> tmpCtx;
    if (!drawIsOpaque) {
        tmpCtx = new gfxContext(tempXlibSurface);
        if (method == eCopyBackground) {
            tmpCtx->SetOperator(gfxContext::OPERATOR_SOURCE);
            tmpCtx->SetSource(target, -(offset + matrix.GetTranslation()));
            // The copy from the tempXlibSurface to the target context should
            // use operator SOURCE, but that would need a mask to bound the
            // operation.  Here we only copy opaque backgrounds so operator
            // OVER will behave like SOURCE masked by the surface.
            NS_ASSERTION(tempXlibSurface->GetContentType()
                         == gfxASurface::CONTENT_COLOR,
                         "Don't copy background with a transparent surface");
        } else {
            tmpCtx->SetOperator(gfxContext::OPERATOR_CLEAR);
        }
        tmpCtx->Paint();
    }

    if (!DrawOntoTempSurface(tempXlibSurface, -drawingRect.TopLeft())) {
        return;
    }
  
    if (method != eAlphaExtraction) {
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
        
        cairo_t *cr = ctx->GetCairo();
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
