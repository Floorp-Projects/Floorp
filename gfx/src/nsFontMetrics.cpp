/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFontMetrics.h"
#include <math.h>                       // for floor, ceil
#include <algorithm>                    // for max
#include "gfxPlatform.h"                // for gfxPlatform
#include "gfxPoint.h"                   // for gfxPoint
#include "gfxRect.h"                    // for gfxRect
#include "gfxTypes.h"                   // for gfxFloat
#include "nsBoundingMetrics.h"          // for nsBoundingMetrics
#include "nsDebug.h"                    // for NS_ERROR, NS_ABORT_IF_FALSE
#include "nsDeviceContext.h"            // for nsDeviceContext
#include "nsIAtom.h"                    // for nsIAtom
#include "nsMathUtils.h"                // for NS_round
#include "nsRenderingContext.h"         // for nsRenderingContext
#include "nsString.h"               // for nsString
#include "nsStyleConsts.h"              // for NS_STYLE_HYPHENS_NONE

class gfxUserFontSet;

namespace {

class AutoTextRun {
public:
    AutoTextRun(nsFontMetrics* aMetrics, nsRenderingContext* aRC,
                const char* aString, int32_t aLength)
    {
        mTextRun = aMetrics->GetThebesFontGroup()->MakeTextRun(
            reinterpret_cast<const uint8_t*>(aString), aLength,
            aRC->ThebesContext(),
            aMetrics->AppUnitsPerDevPixel(),
            ComputeFlags(aMetrics));
    }

    AutoTextRun(nsFontMetrics* aMetrics, nsRenderingContext* aRC,
                const char16_t* aString, int32_t aLength)
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
    static uint32_t ComputeFlags(nsFontMetrics* aMetrics) {
        uint32_t flags = 0;
        if (aMetrics->GetTextRunRTL()) {
            flags |= gfxTextRunFactory::TEXT_IS_RTL;
        }
        return flags;
    }

    nsAutoPtr<gfxTextRun> mTextRun;
};

class StubPropertyProvider : public gfxTextRun::PropertyProvider {
public:
    virtual void GetHyphenationBreaks(uint32_t aStart, uint32_t aLength,
                                      bool* aBreakBefore) {
        NS_ERROR("This shouldn't be called because we never call BreakAndMeasureText");
    }
    virtual int8_t GetHyphensOption() {
        NS_ERROR("This shouldn't be called because we never call BreakAndMeasureText");
        return NS_STYLE_HYPHENS_NONE;
    }
    virtual gfxFloat GetHyphenWidth() {
        NS_ERROR("This shouldn't be called because we never enable hyphens");
        return 0;
    }
    virtual already_AddRefed<gfxContext> GetContext() {
        NS_ERROR("This shouldn't be called because we never enable hyphens");
        return nullptr;
    }
    virtual uint32_t GetAppUnitsPerDevUnit() {
        NS_ERROR("This shouldn't be called because we never enable hyphens");
        return 60;
    }
    virtual void GetSpacing(uint32_t aStart, uint32_t aLength,
                            Spacing* aSpacing) {
        NS_ERROR("This shouldn't be called because we never enable spacing");
    }
};

} // anon namespace

nsFontMetrics::nsFontMetrics()
    : mDeviceContext(nullptr), mP2A(0), mTextRunRTL(false)
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
                    gfxUserFontSet *aUserFontSet,
                    gfxTextPerfMetrics *aTextPerf)
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
                       aFont.variant == NS_STYLE_FONT_VARIANT_SMALL_CAPS,
                       aFont.languageOverride);

    aFont.AddFontFeaturesToStyle(&style);

    mFontGroup = gfxPlatform::GetPlatform()->
        CreateFontGroup(aFont.name, &style, aUserFontSet);
    mFontGroup->SetTextPerfMetrics(aTextPerf);
    if (mFontGroup->FontListLength() < 1)
        return NS_ERROR_UNEXPECTED;

    return NS_OK;
}

void
nsFontMetrics::Destroy()
{
    mDeviceContext = nullptr;
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
    return std::max(minDescent, aMetrics.maxDescent);
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

int32_t
nsFontMetrics::GetMaxStringLength()
{
    const gfxFont::Metrics& m = GetMetrics();
    const double x = 32767.0 / m.maxAdvance;
    int32_t len = (int32_t)floor(x);
    return std::max(1, len);
}

nscoord
nsFontMetrics::GetWidth(const char* aString, uint32_t aLength,
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
nsFontMetrics::GetWidth(const char16_t* aString, uint32_t aLength,
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
nsFontMetrics::DrawString(const char *aString, uint32_t aLength,
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
    textRun->Draw(aContext->ThebesContext(), pt, DrawMode::GLYPH_FILL, 0, aLength,
                  &provider, nullptr, nullptr);
}

void
nsFontMetrics::DrawString(const char16_t* aString, uint32_t aLength,
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
    textRun->Draw(aContext->ThebesContext(), pt, DrawMode::GLYPH_FILL, 0, aLength,
                  &provider, nullptr, nullptr);
}

static nsBoundingMetrics
GetTextBoundingMetrics(nsFontMetrics* aMetrics, const char16_t *aString, uint32_t aLength,
                       nsRenderingContext *aContext, gfxFont::BoundingBoxType aType)
{
    if (aLength == 0)
        return nsBoundingMetrics();

    StubPropertyProvider provider;
    AutoTextRun textRun(aMetrics, aContext, aString, aLength);
    nsBoundingMetrics m;
    if (textRun.get()) {
        gfxTextRun::Metrics theMetrics =
            textRun->MeasureText(0, aLength,
                                 aType,
                                 aContext->ThebesContext(), &provider);

        m.leftBearing  = NSToCoordFloor( theMetrics.mBoundingBox.X());
        m.rightBearing = NSToCoordCeil(  theMetrics.mBoundingBox.XMost());
        m.ascent       = NSToCoordCeil( -theMetrics.mBoundingBox.Y());
        m.descent      = NSToCoordCeil(  theMetrics.mBoundingBox.YMost());
        m.width        = NSToCoordRound( theMetrics.mAdvanceWidth);
    }
    return m;
}

nsBoundingMetrics
nsFontMetrics::GetBoundingMetrics(const char16_t *aString, uint32_t aLength,
                                  nsRenderingContext *aContext)
{
  return GetTextBoundingMetrics(this, aString, aLength, aContext, gfxFont::TIGHT_HINTED_OUTLINE_EXTENTS);
  
}

nsBoundingMetrics
nsFontMetrics::GetInkBoundsForVisualOverflow(const char16_t *aString, uint32_t aLength,
                                             nsRenderingContext *aContext)
{
  return GetTextBoundingMetrics(this, aString, aLength, aContext, gfxFont::LOOSE_INK_EXTENTS);
}

