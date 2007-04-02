/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is thebes gfx
 *
 * The Initial Developer of the Original Code is
 * mozilla.org.
 * Portions created by the Initial Developer are Copyright (C) 2005
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

#ifndef _NSTHEBESIMAGE_H_
#define _NSTHEBESIMAGE_H_

#include "nsIImage.h"

#include "gfxColor.h"
#include "gfxASurface.h"
#include "gfxImageSurface.h"
#ifdef XP_WIN
#include "gfxWindowsSurface.h"
#endif

class nsThebesImage : public nsIImage
{
public:
    nsThebesImage();
    ~nsThebesImage();

    NS_DECL_ISUPPORTS

    virtual nsresult Init(PRInt32 aWidth, PRInt32 aHeight,
                          PRInt32 aDepth, nsMaskRequirements aMaskRequirements);
    virtual PRInt32 GetBytesPix();
    virtual PRBool GetIsRowOrderTopToBottom();
    virtual PRInt32 GetWidth();
    virtual PRInt32 GetHeight();
    virtual PRUint8 *GetBits();
    virtual PRInt32 GetLineStride();
    virtual PRBool GetHasAlphaMask();
    virtual PRUint8 *GetAlphaBits();
    virtual PRInt32 GetAlphaLineStride();
    virtual PRBool GetIsImageComplete();
    virtual void ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect);
    virtual nsresult Optimize(nsIDeviceContext* aContext);
    virtual nsColorMap *GetColorMap();

    NS_IMETHOD Draw(nsIRenderingContext &aContext,
                    const gfxRect &aSourceRect,
                    const gfxRect &aDestRect);
    NS_IMETHOD DrawTile(nsIRenderingContext &aContext,
                        nsIDrawingSurface *aSurface,
                        PRInt32 aSXOffset, PRInt32 aSYOffset,
                        PRInt32 aPadX, PRInt32 aPadY,
                        const nsRect &aTileRect);
    NS_IMETHOD DrawToImage(nsIImage* aDstImage,
                           PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight);

    nsresult ThebesDrawTile(gfxContext *thebesContext,
                            nsIDeviceContext* dx,
                            const gfxPoint& aOffset,
                            const gfxRect& aTileRect,
                            const PRInt32 aXPadding,
                            const PRInt32 aYPadding);

    virtual PRInt8 GetAlphaDepth();
    virtual void* GetBitInfo();
    NS_IMETHOD LockImagePixels(PRBool aMaskPixels);
    NS_IMETHOD UnlockImagePixels(PRBool aMaskPixels);

    NS_IMETHOD GetSurface(gfxASurface **aSurface) {
        *aSurface = ThebesSurface();
        NS_ADDREF(*aSurface);
        return NS_OK;
    }

    gfxASurface* ThebesSurface() {
        if (mOptSurface)
            return mOptSurface;

        return mImageSurface;
    }

protected:
    static PRBool AllowedImageSize(PRInt32 aWidth, PRInt32 aHeight) {
        NS_ASSERTION(aWidth > 0, "invalid image width");
        NS_ASSERTION(aHeight > 0, "invalid image height");

        // reject over-wide or over-tall images
        const PRInt32 k64KLimit = 0x0000FFFF;
        if (NS_UNLIKELY(aWidth > k64KLimit || aHeight > k64KLimit )) {
            NS_WARNING("image too big");
            return PR_FALSE;
        }
        
        // protect against division by zero - this really shouldn't happen
        // if our consumers were well behaved, but they aren't (bug 368427)
        if (NS_UNLIKELY(aHeight == 0)) {
            return PR_FALSE;
        }

        // check to make sure we don't overflow a 32-bit
        PRInt32 tmp = aWidth * aHeight;
        if (NS_UNLIKELY(tmp / aHeight != aWidth)) {
            NS_WARNING("width or height too large");
            return PR_FALSE;
        }
        tmp = tmp * 4;
        if (NS_UNLIKELY(tmp / 4 != aWidth * aHeight)) {
            NS_WARNING("width or height too large");
            return PR_FALSE;
        }
        return PR_TRUE;
    }

    gfxImageSurface::gfxImageFormat mFormat;
    PRInt32 mWidth;
    PRInt32 mHeight;
    PRInt32 mStride;
    nsRect mDecoded;
    PRPackedBool mImageComplete;
    PRPackedBool mSinglePixel;

    gfxRGBA mSinglePixelColor;

    nsRefPtr<gfxImageSurface> mImageSurface;
    nsRefPtr<gfxASurface> mOptSurface;
#ifdef XP_WIN
    nsRefPtr<gfxWindowsSurface> mWinSurface;
#endif

    PRUint8 mAlphaDepth;

    // this function should return true if
    // we should (temporarily) not allocate any
    // platform native surfaces and instead use
    // image surfaces for everything.
    static PRBool ShouldUseImageSurfaces();
};

#endif /* _NSTHEBESIMAGE_H_ */
