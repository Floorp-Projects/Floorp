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
#include "nsDeviceContext.h"
#include "nsStyleConsts.h"

namespace {

class AutoTextRun {
public:
    AutoTextRun(nsFontMetrics* aMetrics, nsRenderingContext* aRC,
                const char* aString, PRInt32 aLength)
    {
        mTextRun = aMetrics->GetThebesFontGroup()->MakeTextRun(
            reinterpret_cast<const PRUint8*>(aString), aLength,
            aRC->ThebesContext(),
            aMetrics->AppUnitsPerDevPixel(),
            ComputeFlags(aMetrics));
    }

    AutoTextRun(nsFontMetrics* aMetrics, nsRenderingContext* aRC,
                const PRUnichar* aString, PRInt32 aLength)
    {
        mTextRun = aMetrics->GetThebesFontGroup()->MakeTextRun(
            aString, aLength,
            aRC->ThebesContext(),
            aMetrics->AppUnitsPerDevPixel(),
            ComputeFlags(aMetrics));
    }

    gfxTextRun *get() { return mTextRun; }
    gfxTextRun *operator->() { return mTextRun; }

private:
    static PRUint32 ComputeFlags(nsFontMetrics* aMetrics) {
        PRUint32 flags = 0;
        if (aMetrics->GetTextRunRTL()) {
            flags |= gfxTextRunFactory::TEXT_IS_RTL;
        }
        return flags;
    }

    nsAutoPtr<gfxTextRun> mTextRun;
};

class StubPropertyProvider : public gfxTextRun::PropertyProvider {
public:
    virtual void GetHyphenationBreaks(PRUint32 aStart, PRUint32 aLength,
                                      bool* aBreakBefore) {
        NS_ERROR("This shouldn't be called because we never call BreakAndMeasureText");
    }
    virtual PRInt8 GetHyphensOption() {
        NS_ERROR("This shouldn't be called because we never call BreakAndMeasureText");
        return NS_STYLE_HYPHENS_NONE;
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

} // anon namespace

nsFontMetrics::nsFontMetrics()
    : mDeviceContext(nsnull), mP2A(0), mTextRunRTL(false)
{
}

nsFontMetrics::~nsFontMetrics()
{
    if (mDeviceContext)
        mDeviceContext->FontMetricsDeleted(this);
}

nsresult
nsFontMetrics::Init(const nsFont& aFont, nsIAtom* aLanguage,
                    nsDeviceContext *aContext,
                    gfxUserFontSet *aUserFontSet)
{
    NS_ABORT_IF_FALSE(mP2A == 0, "already initialized");

    mFont = aFont;
    mLanguage = aLanguage;
    mDeviceContext = aContext;
    mP2A = mDeviceContext->AppUnitsPerDevPixel();

    gfxFontStyle style(aFont.style,
                       aFont.weight,
                       aFont.stretch,
                       gfxFloat(aFont.size) / mP2A,
                       aLanguage,
                       aFont.sizeAdjust,
                       aFont.systemFont,
                       mDeviceContext->IsPrinterSurface(),
                       aFont.featureSettings,
                       aFont.languageOverride);

    mFontGroup = gfxPlatform::GetPlatform()->
        CreateFontGroup(aFont.name, &style, aUserFontSet);
    if (mFontGroup->FontListLength() < 1)
        return NS_ERROR_UNEXPECTED;

    return NS_OK;
}

void
nsFontMetrics::Destroy()
{
    mDeviceContext = nsnull;
}

// XXXTODO get rid of this macro
#define ROUND_TO_TWIPS(x) (nscoord)floor(((x) * mP2A) + 0.5)
#define CEIL_TO_TWIPS(x) (nscoord)ceil((x) * mP2A)

const gfxFont::Metrics& nsFontMetrics::GetMetrics() const
{
    return mFontGroup->GetFontAt(0)->GetMetrics();
}

nscoord
nsFontMetrics::XHeight()
{
    return ROUND_TO_TWIPS(GetMetrics().xHeight);
}

nscoord
nsFontMetrics::SuperscriptOffset()
{
    return ROUND_TO_TWIPS(GetMetrics().superscriptOffset);
}

nscoord
nsFontMetrics::SubscriptOffset()
{
    return ROUND_TO_TWIPS(GetMetrics().subscriptOffset);
}

void
nsFontMetrics::GetStrikeout(nscoord& aOffset, nscoord& aSize)
{
    aOffset = ROUND_TO_TWIPS(GetMetrics().strikeoutOffset);
    aSize = ROUND_TO_TWIPS(GetMetrics().strikeoutSize);
}

void
nsFontMetrics::GetUnderline(nscoord& aOffset, nscoord& aSize)
{
    aOffset = ROUND_TO_TWIPS(mFontGroup->GetUnderlineOffset());
    aSize = ROUND_TO_TWIPS(GetMetrics().underlineSize);
}

// GetMaxAscent/GetMaxDescent/GetMaxHeight must contain the
// text-decoration lines drawable area. See bug 421353.
// BE CAREFUL for rounding each values. The logic MUST be same as
// nsCSSRendering::GetTextDecorationRectInternal's.

static gfxFloat ComputeMaxDescent(const gfxFont::Metrics& aMetrics,
                                  gfxFontGroup* aFontGroup)
{
    gfxFloat offset = floor(-aFontGroup->GetUnderlineOffset() + 0.5);
    gfxFloat size = NS_round(aMetrics.underlineSize);
    gfxFloat minDescent = floor(offset + size + 0.5);
    return NS_MAX(minDescent, aMetrics.maxDescent);
}

static gfxFloat ComputeMaxAscent(const gfxFont::Metrics& aMetrics)
{
    return floor(aMetrics.maxAscent + 0.5);
}

nscoord
nsFontMetrics::InternalLeading()
{
    return ROUND_TO_TWIPS(GetMetrics().internalLeading);
}

nscoord
nsFontMetrics::ExternalLeading()
{
    return ROUND_TO_TWIPS(GetMetrics().externalLeading);
}

nscoord
nsFontMetrics::EmHeight()
{
    return ROUND_TO_TWIPS(GetMetrics().emHeight);
}

nscoord
nsFontMetrics::EmAscent()
{
    return ROUND_TO_TWIPS(GetMetrics().emAscent);
}

nscoord
nsFontMetrics::EmDescent()
{
    return ROUND_TO_TWIPS(GetMetrics().emDescent);
}

nscoord
nsFontMetrics::MaxHeight()
{
    return CEIL_TO_TWIPS(ComputeMaxAscent(GetMetrics())) +
        CEIL_TO_TWIPS(ComputeMaxDescent(GetMetrics(), mFontGroup));
}

nscoord
nsFontMetrics::MaxAscent()
{
    return CEIL_TO_TWIPS(ComputeMaxAscent(GetMetrics()));
}

nscoord
nsFontMetrics::MaxDescent()
{
    return CEIL_TO_TWIPS(ComputeMaxDescent(GetMetrics(), mFontGroup));
}

nscoord
nsFontMetrics::MaxAdvance()
{
    return CEIL_TO_TWIPS(GetMetrics().maxAdvance);
}

nscoord
nsFontMetrics::AveCharWidth()
{
    // Use CEIL instead of ROUND for consistency with GetMaxAdvance
    return CEIL_TO_TWIPS(GetMetrics().aveCharWidth);
}

nscoord
nsFontMetrics::SpaceWidth()
{
    return CEIL_TO_TWIPS(GetMetrics().spaceWidth);
}

PRInt32
nsFontMetrics::GetMaxStringLength()
{
    const gfxFont::Metrics& m = GetMetrics();
    const double x = 32767.0 / m.maxAdvance;
    PRInt32 len = (PRInt32)floor(x);
    return NS_MAX(1, len);
}

nscoord
nsFontMetrics::GetWidth(const char* aString, PRUint32 aLength,
                        nsRenderingContext *aContext)
{
    if (aLength == 0)
        return 0;

    if (aLength == 1 && aString[0] == ' ')
        return SpaceWidth();

    StubPropertyProvider provider;
    AutoTextRun textRun(this, aContext, aString, aLength);
    return textRun.get() ?
        NSToCoordRound(textRun->GetAdvanceWidth(0, aLength, &provider)) : 0;
}

nscoord
nsFontMetrics::GetWidth(const PRUnichar* aString, PRUint32 aLength,
                        nsRenderingContext *aContext)
{
    if (aLength == 0)
        return 0;

    if (aLength == 1 && aString[0] == ' ')
        return SpaceWidth();

    StubPropertyProvider provider;
    AutoTextRun textRun(this, aContext, aString, aLength);
    return textRun.get() ?
        NSToCoordRound(textRun->GetAdvanceWidth(0, aLength, &provider)) : 0;
}

// Draw a string using this font handle on the surface passed in.
void
nsFontMetrics::DrawString(const char *aString, PRUint32 aLength,
                          nscoord aX, nscoord aY,
                          nsRenderingContext *aContext)
{
    if (aLength == 0)
        return;

    StubPropertyProvider provider;
    AutoTextRun textRun(this, aContext, aString, aLength);
    if (!textRun.get()) {
        return;
    }
    gfxPoint pt(aX, aY);
    if (mTextRunRTL) {
        pt.x += textRun->GetAdvanceWidth(0, aLength, &provider);
    }
    textRun->Draw(aContext->ThebesContext(), pt, gfxFont::GLYPH_FILL, 0, aLength,
                  &provider, nsnull, nsnull);
}

void
nsFontMetrics::DrawString(const PRUnichar* aString, PRUint32 aLength,
                          nscoord aX, nscoord aY,
                          nsRenderingContext *aContext,
                          nsRenderingContext *aTextRunConstructionContext)
{
    if (aLength == 0)
        return;

    StubPropertyProvider provider;
    AutoTextRun textRun(this, aTextRunConstructionContext, aString, aLength);
    if (!textRun.get()) {
        return;
    }
    gfxPoint pt(aX, aY);
    if (mTextRunRTL) {
        pt.x += textRun->GetAdvanceWidth(0, aLength, &provider);
    }
    textRun->Draw(aContext->ThebesContext(), pt, gfxFont::GLYPH_FILL, 0, aLength,
                  &provider, nsnull, nsnull);
}

nsBoundingMetrics
nsFontMetrics::GetBoundingMetrics(const PRUnichar *aString, PRUint32 aLength,
                                  nsRenderingContext *aContext)
{
    if (aLength == 0)
        return nsBoundingMetrics();

    StubPropertyProvider provider;
    AutoTextRun textRun(this, aContext, aString, aLength);
    nsBoundingMetrics m;
    if (textRun.get()) {
        gfxTextRun::Metrics theMetrics =
            textRun->MeasureText(0, aLength,
                                 gfxFont::TIGHT_HINTED_OUTLINE_EXTENTS,
                                 aContext->ThebesContext(), &provider);

        m.leftBearing  = NSToCoordFloor( theMetrics.mBoundingBox.X());
        m.rightBearing = NSToCoordCeil(  theMetrics.mBoundingBox.XMost());
        m.ascent       = NSToCoordCeil( -theMetrics.mBoundingBox.Y());
        m.descent      = NSToCoordCeil(  theMetrics.mBoundingBox.YMost());
        m.width        = NSToCoordRound( theMetrics.mAdvanceWidth);
    }
    return m;
}
