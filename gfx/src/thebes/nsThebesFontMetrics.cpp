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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * mozilla.org.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@pavlov.net>
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

#include "nsThebesFontMetrics.h"    
#include "nsFont.h"

#include "nsString.h"
#include "nsAutoBuffer.h"
#include <stdio.h>
#include <malloc.h>

NS_IMPL_ISUPPORTS1(nsThebesFontMetrics, nsIFontMetrics)

#include <stdlib.h>

#if defined(XP_WIN)
#include "gfxWindowsFonts.h"
#elif defined(MOZ_ENABLE_GTK2)
#include "gfxPangoFonts.h"
#endif

nsThebesFontMetrics::nsThebesFontMetrics()
{
    mFontStyle = nsnull;
    mFontGroup = nsnull;
}

nsThebesFontMetrics::~nsThebesFontMetrics()
{
    delete mFontStyle;
    delete mFontGroup;
}

NS_IMETHODIMP
nsThebesFontMetrics::Init(const nsFont& aFont, nsIAtom* aLangGroup,
                          nsIDeviceContext *aContext)
{
    mFont = aFont;
    mLangGroup = aLangGroup;
    mDeviceContext = (nsThebesDeviceContext*)aContext;
    mDev2App = aContext->DevUnitsToAppUnits();

    // work around layout giving us 0 sized fonts...
    double size = aFont.size * mDeviceContext->AppUnitsToDevUnits();
    if (size == 0.0)
        size = 1.0;

    nsCString langGroup;
    if (aLangGroup) {
        const char* lg;
        mLangGroup->GetUTF8String(&lg);
        langGroup.Assign(lg);
    }

    mFontStyle = new gfxFontStyle(aFont.style, aFont.variant,
                                  aFont.weight, aFont.decorations,
                                  size, langGroup, aFont.sizeAdjust,
                                  aFont.systemFont, aFont.familyNameQuirks);

#if defined(XP_WIN)
    mFontGroup = new gfxWindowsFontGroup(aFont.name, mFontStyle, (HDC)mDeviceContext->GetHDC());
#elif defined(MOZ_ENABLE_GTK2)
    mFontGroup = new gfxPangoFontGroup(aFont.name, mFontStyle);
#else
#error implement me
#endif

    return NS_OK;
}

NS_IMETHODIMP
nsThebesFontMetrics::Destroy()
{
    return NS_OK;
}

#define ROUND_TO_TWIPS(x) NSToCoordRound((x) * mDev2App)

gfxFont::Metrics nsThebesFontMetrics::GetMetrics()
{
    return mFontGroup->GetFontList()[0]->GetMetrics();
}

NS_IMETHODIMP
nsThebesFontMetrics::GetXHeight(nscoord& aResult)
{
    aResult = ROUND_TO_TWIPS(GetMetrics().xHeight);
    return NS_OK;
}

NS_IMETHODIMP
nsThebesFontMetrics::GetSuperscriptOffset(nscoord& aResult)
{
    aResult = ROUND_TO_TWIPS(GetMetrics().superscriptOffset);
    return NS_OK;
}

NS_IMETHODIMP
nsThebesFontMetrics::GetSubscriptOffset(nscoord& aResult)
{
    aResult = ROUND_TO_TWIPS(GetMetrics().subscriptOffset);
    return NS_OK;
}

NS_IMETHODIMP
nsThebesFontMetrics::GetStrikeout(nscoord& aOffset, nscoord& aSize)
{
    aOffset = ROUND_TO_TWIPS(GetMetrics().strikeoutOffset);
    aSize = ROUND_TO_TWIPS(GetMetrics().strikeoutSize);
    return NS_OK;
}

NS_IMETHODIMP
nsThebesFontMetrics::GetUnderline(nscoord& aOffset, nscoord& aSize)
{
    aOffset = ROUND_TO_TWIPS(GetMetrics().underlineOffset);
    aSize = ROUND_TO_TWIPS(GetMetrics().underlineSize);

    return NS_OK;
}

NS_IMETHODIMP
nsThebesFontMetrics::GetHeight(nscoord &aHeight)
{
    aHeight = ROUND_TO_TWIPS(GetMetrics().maxHeight);
    return NS_OK;
}

NS_IMETHODIMP
nsThebesFontMetrics::GetInternalLeading(nscoord &aLeading)
{
    aLeading = ROUND_TO_TWIPS(GetMetrics().internalLeading);
    return NS_OK;
}

NS_IMETHODIMP
nsThebesFontMetrics::GetExternalLeading(nscoord &aLeading)
{
    aLeading = ROUND_TO_TWIPS(GetMetrics().externalLeading);
    return NS_OK;
}

NS_IMETHODIMP
nsThebesFontMetrics::GetEmHeight(nscoord &aHeight)
{
    aHeight = ROUND_TO_TWIPS(GetMetrics().emHeight);
    return NS_OK;
}

NS_IMETHODIMP
nsThebesFontMetrics::GetEmAscent(nscoord &aAscent)
{
    aAscent = ROUND_TO_TWIPS(GetMetrics().emAscent);
    return NS_OK;
}

NS_IMETHODIMP
nsThebesFontMetrics::GetEmDescent(nscoord &aDescent)
{
    aDescent = ROUND_TO_TWIPS(GetMetrics().emDescent);
    return NS_OK;
}

NS_IMETHODIMP
nsThebesFontMetrics::GetMaxHeight(nscoord &aHeight)
{
    aHeight = ROUND_TO_TWIPS(GetMetrics().maxHeight);
    return NS_OK;
}

NS_IMETHODIMP
nsThebesFontMetrics::GetMaxAscent(nscoord &aAscent)
{
    aAscent = ROUND_TO_TWIPS(GetMetrics().maxAscent);
    return NS_OK;
}

NS_IMETHODIMP
nsThebesFontMetrics::GetMaxDescent(nscoord &aDescent)
{
    aDescent = ROUND_TO_TWIPS(GetMetrics().maxDescent);
    return NS_OK;
}

NS_IMETHODIMP
nsThebesFontMetrics::GetMaxAdvance(nscoord &aAdvance)
{
    aAdvance = ROUND_TO_TWIPS(GetMetrics().maxAdvance);
    return NS_OK;
}

NS_IMETHODIMP
nsThebesFontMetrics::GetLangGroup(nsIAtom** aLangGroup)
{
    *aLangGroup = mLangGroup;
    NS_IF_ADDREF(*aLangGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsThebesFontMetrics::GetFontHandle(nsFontHandle &aHandle)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsThebesFontMetrics::GetAveCharWidth(nscoord& aAveCharWidth)
{
    aAveCharWidth = ROUND_TO_TWIPS(GetMetrics().aveCharWidth);
    return NS_OK;
}

NS_IMETHODIMP
nsThebesFontMetrics::GetSpaceWidth(nscoord& aSpaceCharWidth)
{
    aSpaceCharWidth = ROUND_TO_TWIPS(GetMetrics().spaceWidth);

    return NS_OK;
}

NS_IMETHODIMP
nsThebesFontMetrics::GetLeading(nscoord& aLeading)
{
    aLeading = ROUND_TO_TWIPS(GetMetrics().internalLeading);
    return NS_OK;
}

NS_IMETHODIMP
nsThebesFontMetrics::GetNormalLineHeight(nscoord& aLineHeight)
{
    const gfxFont::Metrics &m = GetMetrics();
    aLineHeight = ROUND_TO_TWIPS(m.emHeight + m.internalLeading);
    return NS_OK;
}

nsresult 
nsThebesFontMetrics::GetWidth(const char* aString, PRUint32 aLength, nscoord& aWidth,
                           nsThebesRenderingContext *aContext)
{
    PRInt32 aFontID = 0;
    return GetWidth(PromiseFlatString(NS_ConvertUTF8toUTF16(aString, aLength)).get(),
                    aLength, aWidth, &aFontID, aContext);
}

nsresult
nsThebesFontMetrics::GetWidth(const PRUnichar* aString, PRUint32 aLength,
                           nscoord& aWidth, PRInt32 *aFontID,
                           nsThebesRenderingContext *aContext)
{
    nsRefPtr<gfxTextRun> textrun = mFontGroup->MakeTextRun(nsDependentSubstring(aString, aString+aLength));

    aWidth = ROUND_TO_TWIPS(textrun->MeasureString(aContext->Thebes()));

    return NS_OK;

}

// Get the text dimensions for this string
nsresult
nsThebesFontMetrics::GetTextDimensions(const PRUnichar* aString,
                                    PRUint32 aLength,
                                    nsTextDimensions& aDimensions, 
                                    PRInt32* aFontID)
{
    return NS_OK;
}

nsresult
nsThebesFontMetrics::GetTextDimensions(const char*         aString,
                                   PRInt32             aLength,
                                   PRInt32             aAvailWidth,
                                   PRInt32*            aBreaks,
                                   PRInt32             aNumBreaks,
                                   nsTextDimensions&   aDimensions,
                                   PRInt32&            aNumCharsFit,
                                   nsTextDimensions&   aLastWordDimensions,
                                   PRInt32*            aFontID)
{
    return NS_OK;
}
nsresult
nsThebesFontMetrics::GetTextDimensions(const PRUnichar*    aString,
                                   PRInt32             aLength,
                                   PRInt32             aAvailWidth,
                                   PRInt32*            aBreaks,
                                   PRInt32             aNumBreaks,
                                   nsTextDimensions&   aDimensions,
                                   PRInt32&            aNumCharsFit,
                                   nsTextDimensions&   aLastWordDimensions,
                                   PRInt32*            aFontID)
{
    return NS_OK;
}

// Draw a string using this font handle on the surface passed in.  
nsresult
nsThebesFontMetrics::DrawString(const char *aString, PRUint32 aLength,
                            nscoord aX, nscoord aY,
                            const nscoord* aSpacing,
                            nsThebesRenderingContext *aContext)
{
    return DrawString(PromiseFlatString(NS_ConvertUTF8toUTF16(aString, aLength)).get(),
                      aLength, aX, aY, 0, aSpacing, aContext);
}

// aCachedOffset will be updated with a new offset.
nsresult
nsThebesFontMetrics::DrawString(const PRUnichar* aString, PRUint32 aLength,
                            nscoord aX, nscoord aY,
                            PRInt32 aFontID,
                            const nscoord* aSpacing,
                            nsThebesRenderingContext *aContext)
{
    float app2dev = mDeviceContext->AppUnitsToDevUnits();

    nsRefPtr<gfxTextRun> textrun = mFontGroup->MakeTextRun(nsDependentSubstring(aString, aString+aLength));

    textrun->DrawString(aContext->Thebes(), gfxPoint(NSToIntRound(aX * app2dev), NSToIntRound(aY * app2dev)));

    return NS_OK;
}


#ifdef MOZ_MATHML
// These two functions get the bounding metrics for this handle,
// updating the aBoundingMetrics in Points.  This means that the
// caller will have to update them to twips before passing it
// back.
nsresult
nsThebesFontMetrics::GetBoundingMetrics(const char *aString, PRUint32 aLength,
                                    nsBoundingMetrics &aBoundingMetrics)
{
    return NS_OK;
}

// aCachedOffset will be updated with a new offset.
nsresult
nsThebesFontMetrics::GetBoundingMetrics(const PRUnichar *aString,
                                    PRUint32 aLength,
                                    nsBoundingMetrics &aBoundingMetrics,
                                    PRInt32 *aFontID)
{
    return NS_OK;
}

#endif /* MOZ_MATHML */

// Set the direction of the text rendering
nsresult
nsThebesFontMetrics::SetRightToLeftText(PRBool aIsRTL)
{
    // this doesn't seem to ever get called
    NS_WARNING("nsThebesFontMetrics::SetRightToLeftText called!");
    return NS_OK;
}

