/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 *  Vladimir Vukicevic <vladimir@pobox.com>
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCairoDrawingSurface.h"

#include "nsMemory.h"

NS_IMPL_ISUPPORTS1(nsCairoDrawingSurface, nsIDrawingSurface)

nsCairoDrawingSurface::nsCairoDrawingSurface()
    : mSurface(nsnull), mSurfaceData(nsnull), mOwnsData(PR_FALSE)
{
}

nsCairoDrawingSurface::~nsCairoDrawingSurface()
{
    if (mSurface)
        cairo_surface_destroy (mSurface);
    if (mOwnsData && mSurfaceData)
        nsMemory::Free(mSurfaceData);
}

nsresult
nsCairoDrawingSurface::Init(PRUint32 aWidth, PRUint32 aHeight)
{
    NS_ASSERTION(mSurface == nsnull, "Surface already initialized!");
    NS_ASSERTION(aWidth > 0 && aHeight > 0, "Invalid surface dimensions!");

    mSurfaceData = (PRUint8*) nsMemory::Alloc(aWidth * aHeight * 4);

    mSurface = cairo_image_surface_create_for_data ((char*) mSurfaceData, CAIRO_FORMAT_ARGB32,
                                                    aWidth, aHeight, aWidth * 4);
    mWidth = aWidth;
    mHeight = aHeight;
    mStride = mWidth * 4;
    mDepth = 32;
    mOwnsData = PR_TRUE;

    return NS_OK;
}

nsresult
nsCairoDrawingSurface::Init (cairo_surface_t *aSurface, PRBool aIsOffscreen,
                             PRUint32 aWidth, PRUint32 aHeight)
{
    mSurface = aSurface;
    cairo_surface_reference (mSurface);
    if (aIsOffscreen) {
        char *data;
        int width, height, depth, stride;
        if (cairo_image_surface_get_data (mSurface, &data, &width, &height, &stride, &depth) == 0) {
            mSurfaceData = (PRUint8*) data;
            mWidth = width;
            mHeight = height;
            mStride = stride;
            mDepth = depth;
            mOwnsData = PR_FALSE;
            mIsOffscreen = PR_TRUE;
        } else {
            NS_WARNING("nsCairoDrawingSurface::Init with non-image offscreen surface");
        }
    } else {
        mWidth = aWidth;
        mHeight = aHeight;
        mOwnsData = PR_FALSE;
        mIsOffscreen = PR_FALSE;
    }
}

NS_IMETHODIMP
nsCairoDrawingSurface::Lock (PRInt32 aX, PRInt32 aY, PRUint32 aWidth, PRUint32 aHeight,
                             void **aBits, PRInt32 *aStride, PRInt32 *aWidthBytes,
                             PRUint32 aFlags)
{
    NS_ASSERTION(aX + aWidth <= mWidth, "Invalid aX/aWidth");
    NS_ASSERTION(aY + aHeight <= mHeight, "Inavlid aY/aHeight");

    if (mIsOffscreen) {
        *aBits = mSurfaceData + ((aWidth * 4) * aY) + (aX * 4);
        *aStride = mStride;
        *aWidthBytes = mWidth * 4;
    } else {
        NS_WARNING("nsCairoDrawingSurface::Lock with non-offscreen surface!");
    }

    return NS_OK;
}

NS_IMETHODIMP
nsCairoDrawingSurface::Unlock (void)
{
    return NS_OK;
}

NS_IMETHODIMP
nsCairoDrawingSurface::GetDimensions (PRUint32 *aWidth, PRUint32 *aHeight)
{
    *aWidth = mWidth;
    *aHeight = mHeight;
    return NS_OK;
}

NS_IMETHODIMP
nsCairoDrawingSurface::IsOffscreen(PRBool *aOffScreen)
{
    *aOffScreen = mIsOffscreen;
    return NS_OK;
}

NS_IMETHODIMP
nsCairoDrawingSurface::IsPixelAddressable(PRBool *aAddressable)
{
    *aAddressable = mIsOffscreen;
    return NS_OK;
}

NS_IMETHODIMP
nsCairoDrawingSurface::GetPixelFormat(nsPixelFormat *aFormat)
{
    aFormat->mRedMask = 0x000000ff;
    aFormat->mGreenMask = 0x0000ff00;
    aFormat->mBlueMask = 0x00ff0000;
    aFormat->mAlphaMask = 0xff000000;

    aFormat->mRedCount = 8;
    aFormat->mGreenCount = 8;
    aFormat->mBlueCount = 8;
    aFormat->mAlphaCount = 8;

    aFormat->mRedShift = 0;
    aFormat->mGreenShift = 8;
    aFormat->mBlueShift = 16;
    aFormat->mAlphaCount = 24;

    return NS_OK;
}

