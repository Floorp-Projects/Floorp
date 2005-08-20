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

#include "nsMemory.h"

#include "nsThebesBlender.h"
#include "nsThebesDrawingSurface.h"

NS_IMPL_ISUPPORTS1(nsThebesBlender, nsIBlender)

nsThebesBlender::nsThebesBlender()
    : mThebesDC(nsnull)
{
}

nsThebesBlender::~nsThebesBlender()
{
}

NS_IMETHODIMP
nsThebesBlender::Init(nsIDeviceContext *aContext)
{
    mThebesDC = NS_STATIC_CAST(nsThebesDeviceContext*, aContext);
    return NS_OK;
}

NS_IMETHODIMP
nsThebesBlender::Blend(PRInt32 aSX, PRInt32 aSY, PRInt32 aWidth, PRInt32 aHeight,
                       nsIDrawingSurface* aSrc, nsIDrawingSurface* aDest,
                       PRInt32 aDX, PRInt32 aDY,
                       float aSrcOpacity,
                       nsIDrawingSurface* aSecondSrc,
                       nscolor aSrcBackColor, nscolor aSecondSrcBackColor)
{
    NS_WARNING("Should be using Push/PopFilter instead");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsThebesBlender::Blend(PRInt32 aSX, PRInt32 aSY, PRInt32 aWidth, PRInt32 aHeight,
                       nsIRenderingContext *aSrc, nsIRenderingContext *aDest,
                       PRInt32 aDX, PRInt32 aDY, float aSrcOpacity,
                       nsIRenderingContext *aSecondSrc, nscolor aSrcBackColor,
                       nscolor aSecondSrcBackColor)
{
    NS_WARNING("Should be using Push/PopFilter instead");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsThebesBlender::GetAlphas(const nsRect& aRect, nsIDrawingSurface* aBlack,
                           nsIDrawingSurface* aWhite, PRUint8** aAlphas)
{
    NS_WARNING("This needs to be implemented somehow");
    return NS_ERROR_NOT_IMPLEMENTED;
}
