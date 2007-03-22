
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
 * The Original Code is Thebes gfx.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#include "gfxAlphaRecovery.h"

#include "gfxImageSurface.h"


struct gfxAlphaRecoveryResult {
    gfxAlphaRecoveryResult()
        : uniformColor(PR_FALSE),
          uniformAlpha(PR_FALSE)
    { }
    PRBool uniformColor;
    PRBool uniformAlpha;
    gfxFloat alpha;
    gfxFloat r, g, b;
};

static void _compute_alpha_values (unsigned int *black_data,
                                   unsigned int *white_data,
                                   gfxIntSize dimensions,
                                   gfxAlphaRecoveryResult *result);

already_AddRefed<gfxImageSurface>
gfxAlphaRecovery::RecoverAlpha (gfxImageSurface *blackSurf,
                                gfxImageSurface *whiteSurf,
                                gfxIntSize dimensions)
{

    nsRefPtr<gfxImageSurface> resultSurf;
    resultSurf = new gfxImageSurface(dimensions, gfxASurface::ImageFormatARGB32);

    // copy blackSurf into resultSurf
    nsRefPtr<gfxContext> ctx = new gfxContext(resultSurf);
    ctx->SetSource(blackSurf);
    ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
    ctx->Paint();
    ctx = nsnull;

    gfxAlphaRecoveryResult result;
    _compute_alpha_values ((unsigned int*) resultSurf->Data(),
                           (unsigned int*) whiteSurf->Data(),
                           dimensions,
                           &result);

    // XX use result, maybe return pattern, etc.

    NS_ADDREF(resultSurf.get());
    return resultSurf.get();
}

/** from cairo-xlib-utils.c, modified */
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

#define SET_ALPHA(v, a) (((v) & ~(0xFF << 24)) | ((a) << 24))
#define GREEN_OF(v) (((v) >> 8) & 0xFF)

static void
_compute_alpha_values (unsigned int *black_data,
                       unsigned int *white_data,
                       gfxIntSize dimensions,
                       gfxAlphaRecoveryResult *result)
{
    int num_pixels = dimensions.width * dimensions.height;
    int i;
    unsigned int first;
    unsigned int deltas = 0;
    unsigned char first_alpha;
  
    if (num_pixels == 0) {
        if (result) {
            result->uniformAlpha = PR_TRUE;
            result->uniformColor = PR_TRUE;
            /* whatever we put here will be true */
            result->alpha = 1.0;
            result->r = result->g = result->b = 0.0;
        }
        return;
    }
  
    first_alpha = 255 - (GREEN_OF(*white_data) - GREEN_OF(*black_data));
    /* set the alpha value of 'first' */
    first = SET_ALPHA(*black_data, first_alpha);
  
    for (i = 0; i < num_pixels; ++i) {
        unsigned int black = *black_data;
        unsigned int white = *white_data;
        unsigned char pixel_alpha = 255 - (GREEN_OF(white) - GREEN_OF(black));
        
        black = SET_ALPHA(black, pixel_alpha);
        *black_data = black;
        deltas |= (first ^ black);
        
        black_data++;
        white_data++;
    }
    
    if (result) {
        result->uniformAlpha = (deltas >> 24) == 0;
        if (result->uniformAlpha) {
            result->alpha = first_alpha/255.0;
            /* we only set uniformColor when the alpha is already uniform.
               it's only useful in that case ... and if the alpha was nonuniform
               then computing whether the color is uniform would require unpremultiplying
               every pixel */
            result->uniformColor = (deltas & ~(0xFF << 24)) == 0;
            if (result->uniformColor) {
                if (first_alpha == 0) {
                    /* can't unpremultiply, this is OK */
                    result->r = result->g = result->b = 0.0;
                } else {
                    double d_first_alpha = first_alpha;
                    result->r = (first & 0xFF)/d_first_alpha;
                    result->g = ((first >> 8) & 0xFF)/d_first_alpha;
                    result->b = ((first >> 16) & 0xFF)/d_first_alpha;
                }
            }
        }
    }
}
