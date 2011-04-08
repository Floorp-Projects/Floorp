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

#include "nsFontMetrics.h"
#include "nsBoundingMetrics.h"
#include "nsRenderingContext.h"
#include "nsThebesDeviceContext.h"

class AutoTextRun {
public:
    AutoTextRun(nsFontMetrics* aMetrics, nsRenderingContext* aRC,
                const char* aString, PRInt32 aLength) {
        mTextRun = gfxTextRunCache::MakeTextRun(
            reinterpret_cast<const PRUint8*>(aString), aLength,
            aMetrics->GetThebesFontGroup(), aRC->ThebesContext(),
            aMetrics->AppUnitsPerDevPixel(),
            ComputeFlags(aMetrics));
    }
    AutoTextRun(nsFontMetrics* aMetrics, nsRenderingContext* aRC,
                const PRUnichar* aString, PRInt32 aLength) {
        mTextRun = gfxTextRunCache::MakeTextRun(
            aString, aLength, aMetrics->GetThebesFontGroup(),
            aRC->ThebesContext(),
            aMetrics->AppUnitsPerDevPixel(),
            ComputeFlags(aMetrics));
    }
    gfxTextRun* operator->() { return mTextRun.get(); }
    gfxTextRun* get() { return mTextRun.get(); }

private:
    gfxTextRunCache::AutoTextRun mTextRun;

    static PRUint32 ComputeFlags(nsFontMetrics* aMetrics) {
        PRUint32 flags = 0;
        if (aMetrics->GetRightToLeftTextRunMode()) {
            flags |= gfxTextRunFactory::TEXT_IS_RTL;
        }
        return flags;
    }
};

nsFontMetrics::nsFontMetrics()
{
    mFontStyle = nsnull;
    mFontGroup = nsnull;
}

nsFontMetrics::~nsFontMetrics()
{
    if (mDeviceContext)
        mDeviceContext->FontMetricsDeleted(this);
    delete mFontStyle;
}

nsresult
nsFontMetrics::Init(const nsFont& aFont, nsIAtom* aLanguage,
                    nsIDeviceContext *aContext,
                    gfxUserFontSet *aUserFontSet)
{
    mFont = aFont;
    mLanguage = aLanguage;
    mDeviceContext = (nsThebesDeviceContext*)aContext;
    mP2A = mDeviceContext->AppUnitsPerDevPixel();
    mIsRightToLeft = PR_FALSE;
    mTextRunRTL = PR_FALSE;

    gfxFloat size = gfxFloat(aFont.size) / mP2A;

    PRBool printerFont = mDeviceContext->IsPrinterSurface();
    mFontStyle = new gfxFontStyle(aFont.style, aFont.weight, aFont.stretch,
                                  size, aLanguage,
                                  aFont.sizeAdjust, aFont.systemFont,
                                  printerFont,
                                  aFont.featureSettings,
                                  aFont.languageOverride);

    mFontGroup =
        gfxPlatform::GetPlatform()->CreateFontGroup(aFont.name, mFontStyle,
                                                    aUserFontSet);
    if (mFontGroup->FontListLength() < 1)
        return NS_ERROR_UNEXPECTED;

    return NS_OK;
}

nsresult
nsFontMetrics::Destroy()
{
    mDeviceContext = nsnull;
    return NS_OK;
}

// XXXTODO get rid of this macro
#define ROUND_TO_TWIPS(x) (nscoord)floor(((x) * mP2A) + 0.5)
#define CEIL_TO_TWIPS(x) (nscoord)NS_ceil((x) * mP2A)

const gfxFont::Metrics& nsFontMetrics::GetMetrics() const
{
    return mFontGroup->GetFontAt(0)->GetMetrics();
}

nsresult
nsFontMetrics::GetXHeight(nscoord& aResult)
{
    aResult = ROUND_TO_TWIPS(GetMetrics().xHeight);
    return NS_OK;
}

nsresult
nsFontMetrics::GetSuperscriptOffset(nscoord& aResult)
{
    aResult = ROUND_TO_TWIPS(GetMetrics().superscriptOffset);
    return NS_OK;
}

nsresult
nsFontMetrics::GetSubscriptOffset(nscoord& aResult)
{
    aResult = ROUND_TO_TWIPS(GetMetrics().subscriptOffset);
    return NS_OK;
}

nsresult
nsFontMetrics::GetStrikeout(nscoord& aOffset, nscoord& aSize)
{
    aOffset = ROUND_TO_TWIPS(GetMetrics().strikeoutOffset);
    aSize = ROUND_TO_TWIPS(GetMetrics().strikeoutSize);
    return NS_OK;
}

nsresult
nsFontMetrics::GetUnderline(nscoord& aOffset, nscoord& aSize)
{
    aOffset = ROUND_TO_TWIPS(mFontGroup->GetUnderlineOffset());
    aSize = ROUND_TO_TWIPS(GetMetrics().underlineSize);

    return NS_OK;
}

// GetMaxAscent/GetMaxDescent/GetMaxHeight must contain the
// text-decoration lines drawable area. See bug 421353.
// BE CAREFUL for rounding each values. The logic MUST be same as
// nsCSSRendering::GetTextDecorationRectInternal's.

static gfxFloat ComputeMaxDescent(const gfxFont::Metrics& aMetrics,
                                  gfxFontGroup* aFontGroup)
{
    gfxFloat offset = NS_floor(-aFontGroup->GetUnderlineOffset() + 0.5);
    gfxFloat size = NS_round(aMetrics.underlineSize);
    gfxFloat minDescent = NS_floor(offset + size + 0.5);
    return PR_MAX(minDescent, aMetrics.maxDescent);
}

static gfxFloat ComputeMaxAscent(const gfxFont::Metrics& aMetrics)
{
    return NS_floor(aMetrics.maxAscent + 0.5);
}

nsresult
nsFontMetrics::GetInternalLeading(nscoord &aLeading)
{
    aLeading = ROUND_TO_TWIPS(GetMetrics().internalLeading);
    return NS_OK;
}

nsresult
nsFontMetrics::GetExternalLeading(nscoord &aLeading)
{
    aLeading = ROUND_TO_TWIPS(GetMetrics().externalLeading);
    return NS_OK;
}

nsresult
nsFontMetrics::GetEmHeight(nscoord &aHeight)
{
    aHeight = ROUND_TO_TWIPS(GetMetrics().emHeight);
    return NS_OK;
}

nsresult
nsFontMetrics::GetEmAscent(nscoord &aAscent)
{
    aAscent = ROUND_TO_TWIPS(GetMetrics().emAscent);
    return NS_OK;
}

nsresult
nsFontMetrics::GetEmDescent(nscoord &aDescent)
{
    aDescent = ROUND_TO_TWIPS(GetMetrics().emDescent);
    return NS_OK;
}

nsresult
nsFontMetrics::GetMaxHeight(nscoord &aHeight)
{
    aHeight = CEIL_TO_TWIPS(ComputeMaxAscent(GetMetrics())) +
        CEIL_TO_TWIPS(ComputeMaxDescent(GetMetrics(), mFontGroup));
    return NS_OK;
}

nsresult
nsFontMetrics::GetMaxAscent(nscoord &aAscent)
{
    aAscent = CEIL_TO_TWIPS(ComputeMaxAscent(GetMetrics()));
    return NS_OK;
}

nsresult
nsFontMetrics::GetMaxDescent(nscoord &aDescent)
{
    aDescent = CEIL_TO_TWIPS(ComputeMaxDescent(GetMetrics(), mFontGroup));
    return NS_OK;
}

nsresult
nsFontMetrics::GetMaxAdvance(nscoord &aAdvance)
{
    aAdvance = CEIL_TO_TWIPS(GetMetrics().maxAdvance);
    return NS_OK;
}

nsresult
nsFontMetrics::GetLanguage(nsIAtom** aLanguage)
{
    *aLanguage = mLanguage;
    NS_IF_ADDREF(*aLanguage);
    return NS_OK;
}

nsresult
nsFontMetrics::GetAveCharWidth(nscoord& aAveCharWidth)
{
    // Use CEIL instead of ROUND for consistency with GetMaxAdvance
    aAveCharWidth = CEIL_TO_TWIPS(GetMetrics().aveCharWidth);
    return NS_OK;
}

nsresult
nsFontMetrics::GetSpaceWidth(nscoord& aSpaceCharWidth)
{
    aSpaceCharWidth = CEIL_TO_TWIPS(GetMetrics().spaceWidth);
    return NS_OK;
}

PRInt32
nsFontMetrics::GetMaxStringLength()
{
    const gfxFont::Metrics& m = GetMetrics();
    const double x = 32767.0 / m.maxAdvance;
    PRInt32 len = (PRInt32)floor(x);
    return PR_MAX(1, len);
}

class StubPropertyProvider : public gfxTextRun::PropertyProvider {
public:
    virtual void GetHyphenationBreaks(PRUint32 aStart, PRUint32 aLength,
                                      PRPackedBool* aBreakBefore) {
        NS_ERROR("This shouldn't be called because we never call BreakAndMeasureText");
    }
    virtual gfxFloat GetHyphenWidth() {
        NS_ERROR("This shouldn't be called because we never enable hyphens");
        return 0;
    }
    virtual void GetSpacing(PRUint32 aStart, PRUint32 aLength,
                            Spacing* aSpacing) {
        NS_ERROR("This shouldn't be called because we never enable spacing");
    }
};

nsresult
nsFontMetrics::GetWidth(const char* aString, PRUint32 aLength, nscoord& aWidth,
                        nsRenderingContext *aContext)
{
    if (aLength == 0) {
        aWidth = 0;
        return NS_OK;
    }

    // callers that hit this should not be so stupid
    if ((aLength == 1) && (aString[0] == ' '))
        return GetSpaceWidth(aWidth);

    StubPropertyProvider provider;
    AutoTextRun textRun(this, aContext, aString, aLength);
    if (!textRun.get())
        return NS_ERROR_FAILURE;

    aWidth = NSToCoordRound(textRun->GetAdvanceWidth(0, aLength, &provider));

    return NS_OK;
}

nsresult
nsFontMetrics::GetWidth(const PRUnichar* aString, PRUint32 aLength,
                        nscoord& aWidth, PRInt32 *aFontID,
                        nsRenderingContext *aContext)
{
    if (aLength == 0) {
        aWidth = 0;
        return NS_OK;
    }

    // callers that hit this should not be so stupid
    if ((aLength == 1) && (aString[0] == ' '))
        return GetSpaceWidth(aWidth);

    StubPropertyProvider provider;
    AutoTextRun textRun(this, aContext, aString, aLength);
    if (!textRun.get())
        return NS_ERROR_FAILURE;

    aWidth = NSToCoordRound(textRun->GetAdvanceWidth(0, aLength, &provider));

    return NS_OK;
}

// Draw a string using this font handle on the surface passed in.
nsresult
nsFontMetrics::DrawString(const char *aString, PRUint32 aLength,
                          nscoord aX, nscoord aY,
                          const nscoord* aSpacing,
                          nsRenderingContext *aContext)
{
    if (aLength == 0)
        return NS_OK;

    NS_ASSERTION(!aSpacing, "Spacing not supported here");
    StubPropertyProvider provider;
    AutoTextRun textRun(this, aContext, aString, aLength);
    if (!textRun.get())
        return NS_ERROR_FAILURE;
    gfxPoint pt(aX, aY);
    if (mTextRunRTL) {
        pt.x += textRun->GetAdvanceWidth(0, aLength, &provider);
    }
    textRun->Draw(aContext->ThebesContext(), pt, 0, aLength,
                  &provider, nsnull);
    return NS_OK;
}

nsresult
nsFontMetrics::DrawString(const PRUnichar* aString, PRUint32 aLength,
                          nscoord aX, nscoord aY,
                          nsRenderingContext *aContext,
                          nsRenderingContext *aTextRunConstructionContext)
{
    if (aLength == 0)
        return NS_OK;

    StubPropertyProvider provider;
    AutoTextRun textRun(this, aTextRunConstructionContext, aString, aLength);
    if (!textRun.get())
        return NS_ERROR_FAILURE;
    gfxPoint pt(aX, aY);
    if (mTextRunRTL) {
        pt.x += textRun->GetAdvanceWidth(0, aLength, &provider);
    }
    textRun->Draw(aContext->ThebesContext(), pt, 0, aLength,
                  &provider, nsnull);
    return NS_OK;
}

#ifdef MOZ_MATHML
nsresult
nsFontMetrics::GetBoundingMetrics(const PRUnichar *aString, PRUint32 aLength,
                                  nsRenderingContext *aContext,
                                  nsBoundingMetrics &aBoundingMetrics)
{
    if (aLength == 0) {
        aBoundingMetrics = nsBoundingMetrics();
        return NS_OK;
    }

    AutoTextRun textRun(this, aContext, aString, aLength);
    if (!textRun.get())
        return NS_ERROR_FAILURE;

    // note that TIGHT_HINTED_OUTLINE_EXTENTS can be expensive (on Windows)
    // but this is only used for MathML positioning so it's not critical
    StubPropertyProvider provider;
    gfxTextRun::Metrics theMetrics =
        textRun->MeasureText(0, aLength,
                             gfxFont::TIGHT_HINTED_OUTLINE_EXTENTS,
                             aContext->ThebesContext(), &provider);

    aBoundingMetrics.leftBearing = NSToCoordFloor(theMetrics.mBoundingBox.X());
    aBoundingMetrics.rightBearing
        = NSToCoordCeil(theMetrics.mBoundingBox.XMost());
    aBoundingMetrics.width = NSToCoordRound(theMetrics.mAdvanceWidth);
    aBoundingMetrics.ascent = NSToCoordCeil(- theMetrics.mBoundingBox.Y());
    aBoundingMetrics.descent = NSToCoordCeil(theMetrics.mBoundingBox.YMost());

    return NS_OK;
}
#endif /* MOZ_MATHML */
