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
 *    Stuart Parmenter <pavlov@pavlov.net>
 *    Joe Hewitt <hewitt@netscape.com>
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

#ifndef NSCAIROIMAGE__H__
#define NSCAIROIMAGE__H__

#include "nsIImage.h"

#include <cairo.h>

class nsCairoImage : public nsIImage
{
public:
    nsCairoImage();
    ~nsCairoImage();

    NS_DECL_ISUPPORTS

    virtual nsresult Init(PRInt32 aWidth, PRInt32 aHeight,
                          PRInt32 aDepth, nsMaskRequirements aMaskRequirements);
    virtual PRInt32 GetBytesPix();
    virtual PRBool GetIsRowOrderTopToBottom();
    virtual PRInt32 GetWidth();
    virtual PRInt32 GetHeight();
    virtual PRUint8 * GetBits();
    virtual PRInt32 GetLineStride();
    virtual PRBool GetHasAlphaMask();
    virtual PRUint8 * GetAlphaBits();
    virtual PRInt32 GetAlphaLineStride();
    virtual void ImageUpdated(nsIDeviceContext *aContext, PRUint8 aFlags, nsRect *aUpdateRect);
    virtual nsresult Optimize(nsIDeviceContext* aContext);
    virtual nsColorMap * GetColorMap();

    NS_IMETHOD Draw(nsIRenderingContext &aContext,
                    nsIDrawingSurface *aSurface,
                    PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
    NS_IMETHOD Draw(nsIRenderingContext &aContext,
                    nsIDrawingSurface *aSurface,
                    PRInt32 aSX, PRInt32 aSY, PRInt32 aSWidth, PRInt32 aSHeight,
                    PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight);
    NS_IMETHOD DrawTile(nsIRenderingContext &aContext,
                        nsIDrawingSurface *aSurface,
                        PRInt32 aSXOffset, PRInt32 aSYOffset,
                        PRInt32 aPadX, PRInt32 aPadY,
                        const nsRect &aTileRect);
    NS_IMETHOD DrawToImage(nsIImage* aDstImage,
                           PRInt32 aDX, PRInt32 aDY, PRInt32 aDWidth, PRInt32 aDHeight);

    virtual PRInt8 GetAlphaDepth();
    virtual void* GetBitInfo();
    NS_IMETHOD LockImagePixels(PRBool aMaskPixels);
    NS_IMETHOD UnlockImagePixels(PRBool aMaskPixels);

protected:
    PRInt32 mWidth;
    PRInt32 mHeight;

    cairo_surface_t *mImageSurface;
    char *mImageSurfaceData;
};

#endif // NSCAIROIMAGE__H__
