/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is mozilla.org.
 * Portions created by the Initial Developer are
 * Copyright (C) 2004 the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Vladimir Vukicevic <vladimir@pobox.com>
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
 */

#include "nsMemory.h"

#include "nsCairoBlender.h"
#include "nsCairoDrawingSurface.h"

NS_IMPL_ISUPPORTS1(nsCairoBlender, nsIBlender)

nsCairoBlender::nsCairoBlender()
    : mCairoDC(nsnull)
{
}

nsCairoBlender::~nsCairoBlender()
{
}

NS_IMETHODIMP
nsCairoBlender::Init(nsIDeviceContext *aContext)
{
    mCairoDC = NS_STATIC_CAST(nsCairoDeviceContext*, aContext);
    return NS_OK;
}

NS_IMETHODIMP
nsCairoBlender::Blend(PRInt32 aSX, PRInt32 aSY, PRInt32 aWidth, PRInt32 aHeight,
                      nsIDrawingSurface* aSrc, nsIDrawingSurface* aDest,
                      PRInt32 aDX, PRInt32 aDY,
                      float aSrcOpacity,
                      nsIDrawingSurface* aSecondSrc,
                      nscolor aSrcBackColor, nscolor aSecondSrcBackColor)
{
    nsCairoDrawingSurface *cSrc = NS_STATIC_CAST(nsCairoDrawingSurface *, aSrc);
    nsCairoDrawingSurface *cDest = NS_STATIC_CAST(nsCairoDrawingSurface *, aDest);

    cairo_surface_t *src_surf = cSrc->GetCairoSurface();
    cairo_surface_t *dst_surf = cDest->GetCairoSurface();

    cairo_t *cairo = cairo_create();
    cairo_set_target_surface(cairo, dst_surf);

    cairo_pattern_t *src_pat = cairo_pattern_create_for_surface (src_surf);

    cairo_matrix_t *mat = cairo_matrix_create();
    cairo_matrix_translate (mat, aDX - aSX, aDY - aSY);
    cairo_pattern_set_matrix (src_pat, mat);

    cairo_set_pattern (cairo, src_pat);
    cairo_set_alpha (cairo, (double) aSrcOpacity);

    cairo_rectangle (cairo, aDX, aDY, aWidth, aHeight);
    cairo_fill (cairo);

    cairo_matrix_destroy (mat);
    cairo_pattern_destroy (src_pat);
    cairo_destroy(cairo);

    return NS_OK;
}

NS_IMETHODIMP
nsCairoBlender::Blend(PRInt32 aSX, PRInt32 aSY, PRInt32 aWidth, PRInt32 aHeight,
                      nsIRenderingContext *aSrc, nsIRenderingContext *aDest,
                      PRInt32 aDX, PRInt32 aDY, float aSrcOpacity,
                      nsIRenderingContext *aSecondSrc, nscolor aSrcBackColor,
                      nscolor aSecondSrcBackColor)
{
    nsIDrawingSurface* srcSurface, *destSurface, *secondSrcSurface = nsnull;
    aSrc->GetDrawingSurface(&srcSurface);
    aDest->GetDrawingSurface(&destSurface);
    if (aSecondSrc != nsnull)
        aSecondSrc->GetDrawingSurface(&secondSrcSurface);
    return Blend(aSX, aSY, aWidth, aHeight, srcSurface, destSurface,
                 aDX, aDY, aSrcOpacity, secondSrcSurface, aSrcBackColor,
                 aSecondSrcBackColor);
}


NS_IMETHODIMP
nsCairoBlender::GetAlphas(const nsRect& aRect, nsIDrawingSurface* aBlack,
                          nsIDrawingSurface* aWhite, PRUint8** aAlphas)
{
    *aAlphas = (PRUint8*) nsMemory::Alloc(aRect.width * aRect.height);
    memset(*aAlphas, 0, aRect.width * aRect.height);
    return NS_OK;
}
