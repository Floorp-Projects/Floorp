/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFontMetrics.h"
#include <math.h>                       // for floor, ceil
#include <algorithm>                    // for max
#include "gfxFontConstants.h"           // for NS_FONT_SYNTHESIS_*
#include "gfxPlatform.h"                // for gfxPlatform
#include "gfxPoint.h"                   // for gfxPoint
#include "gfxRect.h"                    // for gfxRect
#include "gfxTypes.h"                   // for gfxFloat
#include "nsBoundingMetrics.h"          // for nsBoundingMetrics
#include "nsDebug.h"                    // for NS_ERROR
#include "nsDeviceContext.h"            // for nsDeviceContext
#include "nsIAtom.h"                    // for nsIAtom
#include "nsMathUtils.h"                // for NS_round
#include "nsRenderingContext.h"         // for nsRenderingContext
#include "nsString.h"                   // for nsString
#include "nsStyleConsts.h"              // for StyleHyphens::None
#include "mozilla/Assertions.h"         // for MOZ_ASSERT
#include "mozilla/UniquePtr.h"          // for UniquePtr

class gfxUserFontSet;
using namespace mozilla;

namespace {

class AutoTextRun {
public:
    typedef mozilla::gfx::DrawTarget DrawTarget;

    AutoTextRun(nsFontMetrics* aMetrics, DrawTarget* aDrawTarget,
                const char* aString, int32_t aLength)
    {
        mTextRun = aMetrics->GetThebesFontGroup()->MakeTextRun(
            reinterpret_cast<const uint8_t*>(aString), aLength,
            aDrawTarget,
            aMetrics->AppUnitsPerDevPixel(),
            ComputeFlags(aMetrics), nsTextFrameUtils::Flags(),
            nullptr);
    }

    AutoTextRun(nsFontMetrics* aMetrics, DrawTarget* aDrawTarget,
                const char16_t* aString, int32_t aLength)
    {
        mTextRun = aMetrics->GetThebesFontGroup()->MakeTextRun(
            aString, aLength,
            aDrawTarget,
            aMetrics->AppUnitsPerDevPixel(),
            ComputeFlags(aMetrics), nsTextFrameUtils::Flags(),
            nullptr);
    }

    gfxTextRun *get() { return mTextRun.get(); }
    gfxTextRun *operator->() { return mTextRun.get(); }

private:
    static gfx::ShapedTextFlags ComputeFlags(nsFontMetrics* aMetrics) {
        gfx::ShapedTextFlags flags = gfx::ShapedTextFlags();
        if (aMetrics->GetTextRunRTL()) {
            flags |= gfx::ShapedTextFlags::TEXT_IS_RTL;
        }
        if (aMetrics->GetVertical()) {
            switch (aMetrics->GetTextOrientation()) {
            case NS_STYLE_TEXT_ORIENTATION_MIXED:
                flags |= gfx::ShapedTextFlags::TEXT_ORIENT_VERTICAL_MIXED;
                break;
            case NS_STYLE_TEXT_ORIENTATION_UPRIGHT:
                flags |= gfx::ShapedTextFlags::TEXT_ORIENT_VERTICAL_UPRIGHT;
                break;
            case NS_STYLE_TEXT_ORIENTATION_SIDEWAYS:
                flags |= gfx::ShapedTextFlags::TEXT_ORIENT_VERTICAL_SIDEWAYS_RIGHT;
                break;
            }
        }
        return flags;
    }

    RefPtr<gfxTextRun> mTextRun;
};

class StubPropertyProvider final : public gfxTextRun::PropertyProvider {
public:
    void GetHyphenationBreaks(gfxTextRun::Range aRange,
                              gfxTextRun::HyphenType* aBreakBefore) const {
        NS_ERROR("This shouldn't be called because we never call BreakAndMeasureText");
    }
    mozilla::StyleHyphens GetHyphensOption() const {
        NS_ERROR("This shouldn't be called because we never call BreakAndMeasureText");
        return mozilla::StyleHyphens::None;
    }
    gfxFloat GetHyphenWidth() const {
        NS_ERROR("This shouldn't be called because we never enable hyphens");
        return 0;
    }
    already_AddRefed<mozilla::gfx::DrawTarget> GetDrawTarget() const {
        NS_ERROR("This shouldn't be called because we never enable hyphens");
        return nullptr;
    }
    uint32_t GetAppUnitsPerDevUnit() const {
        NS_ERROR("This shouldn't be called because we never enable hyphens");
        return 60;
    }
    void GetSpacing(gfxTextRun::Range aRange, Spacing* aSpacing) const {
        NS_ERROR("This shouldn't be called because we never enable spacing");
    }
};

} // namespace

nsFontMetrics::nsFontMetrics(const nsFont& aFont, const Params& aParams,
                             nsDeviceContext *aContext)
    : mFont(aFont)
    , mLanguage(aParams.language)
    , mDeviceContext(aContext)
    , mP2A(aContext->AppUnitsPerDevPixel())
    , mOrientation(aParams.orientation)
    , mTextRunRTL(false)
    , mVertical(false)
    , mTextOrientation(0)
{
    gfxFontStyle style(aFont.style,
                       aFont.weight,
                       aFont.stretch,
                       gfxFloat(aFont.size) / mP2A,
                       aParams.language,
                       aParams.explicitLanguage,
                       aFont.sizeAdjust,
                       aFont.systemFont,
                       mDeviceContext->IsPrinterContext(),
                       aFont.synthesis & NS_FONT_SYNTHESIS_WEIGHT,
                       aFont.synthesis & NS_FONT_SYNTHESIS_STYLE,
                       aFont.languageOverride);

    aFont.AddFontFeaturesToStyle(&style);
    aFont.AddFontVariationsToStyle(&style);

    gfxFloat devToCssSize = gfxFloat(mP2A) /
        gfxFloat(mDeviceContext->AppUnitsPerCSSPixel());
    mFontGroup = gfxPlatform::GetPlatform()->
        CreateFontGroup(aFont.fontlist, &style, aParams.textPerf,
                        aParams.userFontSet, devToCssSize);
}

nsFontMetrics::~nsFontMetrics()
{
    // Should not be dropped by stylo
    MOZ_ASSERT(NS_IsMainThread());
    if (mDeviceContext) {
        mDeviceContext->FontMetricsDeleted(this);
    }
}

void
nsFontMetrics::Destroy()
{
    mDeviceContext = nullptr;
}

// XXXTODO get rid of this macro
#define ROUND_TO_TWIPS(x) (nscoord)floor(((x) * mP2A) + 0.5)
#define CEIL_TO_TWIPS(x) (nscoord)ceil((x) * mP2A)

const gfxFont::Metrics&
nsFontMetrics::GetMetrics(gfxFont::Orientation aOrientation) const
{
    return mFontGroup->GetFirstValidFont()->GetMetrics(aOrientation);
}

nscoord
nsFontMetrics::XHeight()
{
    return ROUND_TO_TWIPS(GetMetrics().xHeight);
}

nscoord
nsFontMetrics::CapHeight()
{
    return ROUND_TO_TWIPS(GetMetrics().capHeight);
}

nscoord
nsFontMetrics::SuperscriptOffset()
{
    return ROUND_TO_TWIPS(GetMetrics().emHeight *
                          NS_FONT_SUPERSCRIPT_OFFSET_RATIO);
}

nscoord
nsFontMetrics::SubscriptOffset()
{
    return ROUND_TO_TWIPS(GetMetrics().emHeight *
                          NS_FONT_SUBSCRIPT_OFFSET_RATIO);
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
    gfxFloat minDescent = offset + size;
    return floor(std::max(minDescent, aMetrics.maxDescent) + 0.5);
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
    // For vertical text with mixed or sideways orientation, we want the
    // width of a horizontal space (even if we're using vertical line-spacing
    // metrics, as with "writing-mode:vertical-*;text-orientation:mixed").
    return CEIL_TO_TWIPS(
        GetMetrics(mVertical &&
                   mTextOrientation == NS_STYLE_TEXT_ORIENTATION_UPRIGHT
                       ? gfxFont::eVertical
                       : gfxFont::eHorizontal).spaceWidth);
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
                        DrawTarget* aDrawTarget)
{
    if (aLength == 0)
        return 0;

    if (aLength == 1 && aString[0] == ' ')
        return SpaceWidth();

    StubPropertyProvider provider;
    AutoTextRun textRun(this, aDrawTarget, aString, aLength);
    if (textRun.get()) {
      return NSToCoordRound(
          textRun->GetAdvanceWidth(Range(0, aLength), &provider));
    }
    return 0;
}

nscoord
nsFontMetrics::GetWidth(const char16_t* aString, uint32_t aLength,
                        DrawTarget* aDrawTarget)
{
    if (aLength == 0)
        return 0;

    if (aLength == 1 && aString[0] == ' ')
        return SpaceWidth();

    StubPropertyProvider provider;
    AutoTextRun textRun(this, aDrawTarget, aString, aLength);
    if (textRun.get()) {
      return NSToCoordRound(
          textRun->GetAdvanceWidth(Range(0, aLength), &provider));
    }
    return 0;
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
    AutoTextRun textRun(this, aContext->GetDrawTarget(), aString, aLength);
    if (!textRun.get()) {
        return;
    }
    gfxPoint pt(aX, aY);
    Range range(0, aLength);
    if (mTextRunRTL) {
        if (mVertical) {
            pt.y += textRun->GetAdvanceWidth(range, &provider);
        } else {
            pt.x += textRun->GetAdvanceWidth(range, &provider);
        }
    }
    gfxTextRun::DrawParams params(aContext->ThebesContext());
    params.provider = &provider;
    textRun->Draw(range, pt, params);
}

void
nsFontMetrics::DrawString(const char16_t* aString, uint32_t aLength,
                          nscoord aX, nscoord aY,
                          nsRenderingContext *aContext,
                          DrawTarget* aTextRunConstructionDrawTarget)
{
    if (aLength == 0)
        return;

    StubPropertyProvider provider;
    AutoTextRun textRun(this, aTextRunConstructionDrawTarget, aString, aLength);
    if (!textRun.get()) {
        return;
    }
    gfxPoint pt(aX, aY);
    Range range(0, aLength);
    if (mTextRunRTL) {
        if (mVertical) {
            pt.y += textRun->GetAdvanceWidth(range, &provider);
        } else {
            pt.x += textRun->GetAdvanceWidth(range, &provider);
        }
    }
    gfxTextRun::DrawParams params(aContext->ThebesContext());
    params.provider = &provider;
    textRun->Draw(range, pt, params);
}

static nsBoundingMetrics
GetTextBoundingMetrics(nsFontMetrics* aMetrics, const char16_t* aString,
                       uint32_t aLength, mozilla::gfx::DrawTarget* aDrawTarget,
                       gfxFont::BoundingBoxType aType)
{
    if (aLength == 0)
        return nsBoundingMetrics();

    StubPropertyProvider provider;
    AutoTextRun textRun(aMetrics, aDrawTarget, aString, aLength);
    nsBoundingMetrics m;
    if (textRun.get()) {
        gfxTextRun::Metrics theMetrics = textRun->MeasureText(
            gfxTextRun::Range(0, aLength), aType, aDrawTarget, &provider);

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
                                  DrawTarget* aDrawTarget)
{
  return GetTextBoundingMetrics(this, aString, aLength, aDrawTarget,
                                gfxFont::TIGHT_HINTED_OUTLINE_EXTENTS);
}

nsBoundingMetrics
nsFontMetrics::GetInkBoundsForVisualOverflow(const char16_t *aString, uint32_t aLength,
                                             DrawTarget* aDrawTarget)
{
  return GetTextBoundingMetrics(this, aString, aLength, aDrawTarget,
                                gfxFont::LOOSE_INK_EXTENTS);
}

