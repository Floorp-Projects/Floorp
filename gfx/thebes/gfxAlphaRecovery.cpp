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
 * The Initial Developer of the Original Code is Mozilla Foundation.
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

/* static */ PRBool
gfxAlphaRecovery::RecoverAlpha(gfxImageSurface* blackSurf,
                               const gfxImageSurface* whiteSurf,
                               Analysis* analysis)
{
    gfxIntSize size = blackSurf->GetSize();

    if (size != whiteSurf->GetSize() ||
        (blackSurf->Format() != gfxASurface::ImageFormatARGB32 &&
         blackSurf->Format() != gfxASurface::ImageFormatRGB24) ||
        (whiteSurf->Format() != gfxASurface::ImageFormatARGB32 &&
         whiteSurf->Format() != gfxASurface::ImageFormatRGB24))
        return PR_FALSE;

    if (size.width == 0 || size.height == 0) {
        if (analysis) {
            analysis->uniformAlpha = PR_TRUE;
            analysis->uniformColor = PR_TRUE;
            /* whatever we put here will be true */
            analysis->alpha = 1.0;
            analysis->r = analysis->g = analysis->b = 0.0;
        }
        return PR_TRUE;
    }
  
    unsigned char* blackData = blackSurf->Data();
    unsigned char* whiteData = whiteSurf->Data();
    if (!blackData || !whiteData)
        return PR_FALSE;

    blackSurf->Flush();
    whiteSurf->Flush();

    PRUint32 black = *reinterpret_cast<PRUint32*>(blackData);
    PRUint32 white = *reinterpret_cast<PRUint32*>(whiteData);
    unsigned char first_alpha =
        255 - (GREEN_OF(white) - GREEN_OF(black));
    /* set the alpha value of 'first' */
    PRUint32 first = SET_ALPHA(black, first_alpha);

    PRUint32 deltas = 0;
    for (PRInt32 i = 0; i < size.height; ++i) {
        PRUint32* blackPixel = reinterpret_cast<PRUint32*>(blackData);
        const PRUint32* whitePixel = reinterpret_cast<PRUint32*>(whiteData);
        for (PRInt32 j = 0; j < size.width; ++j) {
            black = blackPixel[j];
            white = whitePixel[j];
            unsigned char pixel_alpha =
                255 - (GREEN_OF(white) - GREEN_OF(black));
        
            black = SET_ALPHA(black, pixel_alpha);
            blackPixel[j] = black;
            deltas |= (first ^ black);
        }
        blackData += blackSurf->Stride();
        whiteData += whiteSurf->Stride();
    }

    blackSurf->MarkDirty();
    
    if (analysis) {
        analysis->uniformAlpha = (deltas >> 24) == 0;
        analysis->uniformColor = PR_FALSE;
        if (analysis->uniformAlpha) {
            analysis->alpha = first_alpha/255.0;
            /* we only set uniformColor when the alpha is already uniform.
               it's only useful in that case ... and if the alpha was nonuniform
               then computing whether the color is uniform would require unpremultiplying
               every pixel */
            analysis->uniformColor = (deltas & ~(0xFF << 24)) == 0;
            if (analysis->uniformColor) {
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

    return PR_TRUE;
}
