/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxTextRun.h"
#include "gfxGlyphExtents.h"
#include "gfxPlatformFontList.h"
#include "gfxUserFontSet.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/Sprintf.h"
#include "nsGkAtoms.h"
#include "nsILanguageAtomService.h"
#include "nsServiceManagerUtils.h"

#include "gfxContext.h"
#include "gfxFontConstants.h"
#include "gfxFontMissingGlyphs.h"
#include "gfxScriptItemizer.h"
#include "nsUnicodeProperties.h"
#include "nsUnicodeRange.h"
#include "nsStyleConsts.h"
#include "mozilla/Likely.h"
#include "gfx2DGlue.h"
#include "mozilla/gfx/Logging.h"        // for gfxCriticalError
#include "mozilla/UniquePtr.h"

#if defined(MOZ_WIDGET_GTK)
#include "gfxPlatformGtk.h" // xxx - for UseFcFontList
#endif

#ifdef XP_WIN
#include "gfxWindowsPlatform.h"
#endif

#include "cairo.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::unicode;
using mozilla::services::GetObserverService;

static const char16_t kEllipsisChar[] = { 0x2026, 0x0 };
static const char16_t kASCIIPeriodsChar[] = { '.', '.', '.', 0x0 };

#ifdef DEBUG_roc
#define DEBUG_TEXT_RUN_STORAGE_METRICS
#endif

#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
extern uint32_t gTextRunStorageHighWaterMark;
extern uint32_t gTextRunStorage;
extern uint32_t gFontCount;
extern uint32_t gGlyphExtentsCount;
extern uint32_t gGlyphExtentsWidthsTotalSize;
extern uint32_t gGlyphExtentsSetupEagerSimple;
extern uint32_t gGlyphExtentsSetupEagerTight;
extern uint32_t gGlyphExtentsSetupLazyTight;
extern uint32_t gGlyphExtentsSetupFallBackToTight;
#endif

bool
gfxTextRun::GlyphRunIterator::NextRun()  {
    if (mNextIndex >= mTextRun->mGlyphRuns.Length())
        return false;
    mGlyphRun = &mTextRun->mGlyphRuns[mNextIndex];
    if (mGlyphRun->mCharacterOffset >= mEndOffset)
        return false;

    mStringStart = std::max(mStartOffset, mGlyphRun->mCharacterOffset);
    uint32_t last = mNextIndex + 1 < mTextRun->mGlyphRuns.Length()
        ? mTextRun->mGlyphRuns[mNextIndex + 1].mCharacterOffset : mTextRun->GetLength();
    mStringEnd = std::min(mEndOffset, last);

    ++mNextIndex;
    return true;
}

#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
static void
AccountStorageForTextRun(gfxTextRun *aTextRun, int32_t aSign)
{
    // Ignores detailed glyphs... we don't know when those have been constructed
    // Also ignores gfxSkipChars dynamic storage (which won't be anything
    // for preformatted text)
    // Also ignores GlyphRun array, again because it hasn't been constructed
    // by the time this gets called. If there's only one glyphrun that's stored
    // directly in the textrun anyway so no additional overhead.
    uint32_t length = aTextRun->GetLength();
    int32_t bytes = length * sizeof(gfxTextRun::CompressedGlyph);
    bytes += sizeof(gfxTextRun);
    gTextRunStorage += bytes*aSign;
    gTextRunStorageHighWaterMark = std::max(gTextRunStorageHighWaterMark, gTextRunStorage);
}
#endif

static bool
NeedsGlyphExtents(gfxTextRun *aTextRun)
{
    if (aTextRun->GetFlags() & gfxTextRunFactory::TEXT_NEED_BOUNDING_BOX)
        return true;
    uint32_t numRuns;
    const gfxTextRun::GlyphRun *glyphRuns = aTextRun->GetGlyphRuns(&numRuns);
    for (uint32_t i = 0; i < numRuns; ++i) {
        if (glyphRuns[i].mFont->GetFontEntry()->IsUserFont())
            return true;
    }
    return false;
}

// Helper for textRun creation to preallocate storage for glyph records;
// this function returns a pointer to the newly-allocated glyph storage.
// Returns nullptr if allocation fails.
void *
gfxTextRun::AllocateStorageForTextRun(size_t aSize, uint32_t aLength)
{
    // Allocate the storage we need, returning nullptr on failure rather than
    // throwing an exception (because web content can create huge runs).
    void *storage = malloc(aSize + aLength * sizeof(CompressedGlyph));
    if (!storage) {
        NS_WARNING("failed to allocate storage for text run!");
        return nullptr;
    }

    // Initialize the glyph storage (beyond aSize) to zero
    memset(reinterpret_cast<char*>(storage) + aSize, 0,
           aLength * sizeof(CompressedGlyph));

    return storage;
}

already_AddRefed<gfxTextRun>
gfxTextRun::Create(const gfxTextRunFactory::Parameters *aParams,
                   uint32_t aLength, gfxFontGroup *aFontGroup, uint32_t aFlags)
{
    void *storage = AllocateStorageForTextRun(sizeof(gfxTextRun), aLength);
    if (!storage) {
        return nullptr;
    }

    RefPtr<gfxTextRun> result = new (storage) gfxTextRun(aParams, aLength,
                                                         aFontGroup, aFlags);
    return result.forget();
}

gfxTextRun::gfxTextRun(const gfxTextRunFactory::Parameters *aParams,
                       uint32_t aLength, gfxFontGroup *aFontGroup, uint32_t aFlags)
    : gfxShapedText(aLength, aFlags, aParams->mAppUnitsPerDevUnit)
    , mUserData(aParams->mUserData)
    , mFontGroup(aFontGroup)
    , mReleasedFontGroup(false)
    , mShapingState(eShapingState_Normal)
{
    NS_ASSERTION(mAppUnitsPerDevUnit > 0, "Invalid app unit scale");
    MOZ_COUNT_CTOR(gfxTextRun);
    NS_ADDREF(mFontGroup);

#ifndef RELEASE_OR_BETA
    gfxTextPerfMetrics *tp = aFontGroup->GetTextPerfMetrics();
    if (tp) {
        tp->current.textrunConst++;
    }
#endif

    mCharacterGlyphs = reinterpret_cast<CompressedGlyph*>(this + 1);

    if (aParams->mSkipChars) {
        mSkipChars.TakeFrom(aParams->mSkipChars);
    }

#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
    AccountStorageForTextRun(this, 1);
#endif

    mSkipDrawing = mFontGroup->ShouldSkipDrawing();
}

gfxTextRun::~gfxTextRun()
{
#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
    AccountStorageForTextRun(this, -1);
#endif
#ifdef DEBUG
    // Make it easy to detect a dead text run
    mFlags = 0xFFFFFFFF;
#endif

    // The cached ellipsis textrun (if any) in a fontgroup will have already
    // been told to release its reference to the group, so we mustn't do that
    // again here.
    if (!mReleasedFontGroup) {
#ifndef RELEASE_OR_BETA
        gfxTextPerfMetrics *tp = mFontGroup->GetTextPerfMetrics();
        if (tp) {
            tp->current.textrunDestr++;
        }
#endif
        NS_RELEASE(mFontGroup);
    }

    MOZ_COUNT_DTOR(gfxTextRun);
}

void
gfxTextRun::ReleaseFontGroup()
{
    NS_ASSERTION(!mReleasedFontGroup, "doubly released!");
    NS_RELEASE(mFontGroup);
    mReleasedFontGroup = true;
}

bool
gfxTextRun::SetPotentialLineBreaks(Range aRange, const uint8_t* aBreakBefore)
{
    NS_ASSERTION(aRange.end <= GetLength(), "Overflow");

    uint32_t changed = 0;
    CompressedGlyph* cg = mCharacterGlyphs + aRange.start;
    const CompressedGlyph* const end = cg + aRange.Length();
    while (cg < end) {
        uint8_t canBreak = *aBreakBefore++;
        if (canBreak && !cg->IsClusterStart()) {
            // XXX If we replace the line-breaker with one based more closely
            // on UAX#14 (e.g. using ICU), this may not be needed any more.
            // Avoid possible breaks inside a cluster, EXCEPT when the previous
            // character was a space (compare UAX#14 rules LB9, LB10).
            if (cg == mCharacterGlyphs || !(cg - 1)->CharIsSpace()) {
                canBreak = CompressedGlyph::FLAG_BREAK_TYPE_NONE;
            }
        }
        changed |= cg->SetCanBreakBefore(canBreak);
        ++cg;
    }
    return changed != 0;
}

gfxTextRun::LigatureData
gfxTextRun::ComputeLigatureData(Range aPartRange,
                                PropertyProvider *aProvider) const
{
    NS_ASSERTION(aPartRange.start < aPartRange.end,
                 "Computing ligature data for empty range");
    NS_ASSERTION(aPartRange.end <= GetLength(), "Character length overflow");
  
    LigatureData result;
    const CompressedGlyph *charGlyphs = mCharacterGlyphs;

    uint32_t i;
    for (i = aPartRange.start; !charGlyphs[i].IsLigatureGroupStart(); --i) {
        NS_ASSERTION(i > 0, "Ligature at the start of the run??");
    }
    result.mRange.start = i;
    for (i = aPartRange.start + 1;
         i < GetLength() && !charGlyphs[i].IsLigatureGroupStart(); ++i) {
    }
    result.mRange.end = i;

    int32_t ligatureWidth = GetAdvanceForGlyphs(result.mRange);
    // Count the number of started clusters we have seen
    uint32_t totalClusterCount = 0;
    uint32_t partClusterIndex = 0;
    uint32_t partClusterCount = 0;
    for (i = result.mRange.start; i < result.mRange.end; ++i) {
        // Treat the first character of the ligature as the start of a
        // cluster for our purposes of allocating ligature width to its
        // characters.
        if (i == result.mRange.start || charGlyphs[i].IsClusterStart()) {
            ++totalClusterCount;
            if (i < aPartRange.start) {
                ++partClusterIndex;
            } else if (i < aPartRange.end) {
                ++partClusterCount;
            }
        }
    }
    NS_ASSERTION(totalClusterCount > 0, "Ligature involving no clusters??");
    result.mPartAdvance = partClusterIndex * (ligatureWidth / totalClusterCount);
    result.mPartWidth = partClusterCount * (ligatureWidth / totalClusterCount);

    // Any rounding errors are apportioned to the final part of the ligature,
    // so that measuring all parts of a ligature and summing them is equal to
    // the ligature width.
    if (aPartRange.end == result.mRange.end) {
        gfxFloat allParts = totalClusterCount * (ligatureWidth / totalClusterCount);
        result.mPartWidth += ligatureWidth - allParts;
    }

    if (partClusterCount == 0) {
        // nothing to draw
        result.mClipBeforePart = result.mClipAfterPart = true;
    } else {
        // Determine whether we should clip before or after this part when
        // drawing its slice of the ligature.
        // We need to clip before the part if any cluster is drawn before
        // this part.
        result.mClipBeforePart = partClusterIndex > 0;
        // We need to clip after the part if any cluster is drawn after
        // this part.
        result.mClipAfterPart = partClusterIndex + partClusterCount < totalClusterCount;
    }

    if (aProvider && (mFlags & gfxTextRunFactory::TEXT_ENABLE_SPACING)) {
        gfxFont::Spacing spacing;
        if (aPartRange.start == result.mRange.start) {
            aProvider->GetSpacing(
                Range(aPartRange.start, aPartRange.start + 1), &spacing);
            result.mPartWidth += spacing.mBefore;
        }
        if (aPartRange.end == result.mRange.end) {
            aProvider->GetSpacing(
                Range(aPartRange.end - 1, aPartRange.end), &spacing);
            result.mPartWidth += spacing.mAfter;
        }
    }

    return result;
}

gfxFloat
gfxTextRun::ComputePartialLigatureWidth(Range aPartRange,
                                        PropertyProvider *aProvider) const
{
    if (aPartRange.start >= aPartRange.end)
        return 0;
    LigatureData data = ComputeLigatureData(aPartRange, aProvider);
    return data.mPartWidth;
}

int32_t
gfxTextRun::GetAdvanceForGlyphs(Range aRange) const
{
    int32_t advance = 0;
    for (auto i = aRange.start; i < aRange.end; ++i) {
        advance += GetAdvanceForGlyph(i);
    }
    return advance;
}

static void
GetAdjustedSpacing(const gfxTextRun *aTextRun, gfxTextRun::Range aRange,
                   gfxTextRun::PropertyProvider *aProvider,
                   gfxTextRun::PropertyProvider::Spacing *aSpacing)
{
    if (aRange.start >= aRange.end)
        return;

    aProvider->GetSpacing(aRange, aSpacing);

#ifdef DEBUG
    // Check to see if we have spacing inside ligatures

    const gfxTextRun::CompressedGlyph *charGlyphs = aTextRun->GetCharacterGlyphs();
    uint32_t i;

    for (i = aRange.start; i < aRange.end; ++i) {
        if (!charGlyphs[i].IsLigatureGroupStart()) {
            NS_ASSERTION(i == aRange.start ||
                         aSpacing[i - aRange.start].mBefore == 0,
                         "Before-spacing inside a ligature!");
            NS_ASSERTION(i - 1 <= aRange.start ||
                         aSpacing[i - 1 - aRange.start].mAfter == 0,
                         "After-spacing inside a ligature!");
        }
    }
#endif
}

bool
gfxTextRun::GetAdjustedSpacingArray(Range aRange, PropertyProvider *aProvider,
                                    Range aSpacingRange,
                                    nsTArray<PropertyProvider::Spacing>*
                                        aSpacing) const
{
    if (!aProvider || !(mFlags & gfxTextRunFactory::TEXT_ENABLE_SPACING))
        return false;
    if (!aSpacing->AppendElements(aRange.Length()))
        return false;
    auto spacingOffset = aSpacingRange.start - aRange.start;
    memset(aSpacing->Elements(), 0, sizeof(gfxFont::Spacing) * spacingOffset);
    GetAdjustedSpacing(this, aSpacingRange, aProvider,
                       aSpacing->Elements() + spacingOffset);
    memset(aSpacing->Elements() + aSpacingRange.end - aRange.start, 0,
           sizeof(gfxFont::Spacing) * (aRange.end - aSpacingRange.end));
    return true;
}

void
gfxTextRun::ShrinkToLigatureBoundaries(Range* aRange) const
{
    if (aRange->start >= aRange->end)
        return;
  
    const CompressedGlyph *charGlyphs = mCharacterGlyphs;

    while (aRange->start < aRange->end &&
           !charGlyphs[aRange->start].IsLigatureGroupStart()) {
        ++aRange->start;
    }
    if (aRange->end < GetLength()) {
        while (aRange->end > aRange->start &&
               !charGlyphs[aRange->end].IsLigatureGroupStart()) {
            --aRange->end;
        }
    }
}

void
gfxTextRun::DrawGlyphs(gfxFont *aFont, Range aRange, gfxPoint *aPt,
                       PropertyProvider *aProvider, Range aSpacingRange,
                       TextRunDrawParams& aParams, uint16_t aOrientation) const
{
    AutoTArray<PropertyProvider::Spacing,200> spacingBuffer;
    bool haveSpacing = GetAdjustedSpacingArray(aRange, aProvider,
                                               aSpacingRange, &spacingBuffer);
    aParams.spacing = haveSpacing ? spacingBuffer.Elements() : nullptr;
    aFont->Draw(this, aRange.start, aRange.end, aPt, aParams, aOrientation);
}

static void
ClipPartialLigature(const gfxTextRun* aTextRun,
                    gfxFloat *aStart, gfxFloat *aEnd,
                    gfxFloat aOrigin,
                    gfxTextRun::LigatureData *aLigature)
{
    if (aLigature->mClipBeforePart) {
        if (aTextRun->IsRightToLeft()) {
            *aEnd = std::min(*aEnd, aOrigin);
        } else {
            *aStart = std::max(*aStart, aOrigin);
        }
    }
    if (aLigature->mClipAfterPart) {
        gfxFloat endEdge =
            aOrigin + aTextRun->GetDirection() * aLigature->mPartWidth;
        if (aTextRun->IsRightToLeft()) {
            *aStart = std::max(*aStart, endEdge);
        } else {
            *aEnd = std::min(*aEnd, endEdge);
        }
    }    
}

void
gfxTextRun::DrawPartialLigature(gfxFont *aFont, Range aRange,
                                gfxPoint *aPt, PropertyProvider *aProvider,
                                TextRunDrawParams& aParams,
                                uint16_t aOrientation) const
{
    if (aRange.start >= aRange.end) {
        return;
    }

    // Draw partial ligature. We hack this by clipping the ligature.
    LigatureData data = ComputeLigatureData(aRange, aProvider);
    gfxRect clipExtents = aParams.context->GetClipExtents();
    gfxFloat start, end;
    if (aParams.isVerticalRun) {
        start = clipExtents.Y() * mAppUnitsPerDevUnit;
        end = clipExtents.YMost() * mAppUnitsPerDevUnit;
        ClipPartialLigature(this, &start, &end, aPt->y, &data);
    } else {
        start = clipExtents.X() * mAppUnitsPerDevUnit;
        end = clipExtents.XMost() * mAppUnitsPerDevUnit;
        ClipPartialLigature(this, &start, &end, aPt->x, &data);
    }

    {
      // use division here to ensure that when the rect is aligned on multiples
      // of mAppUnitsPerDevUnit, we clip to true device unit boundaries.
      // Also, make sure we snap the rectangle to device pixels.
      Rect clipRect = aParams.isVerticalRun ?
          Rect(clipExtents.X(), start / mAppUnitsPerDevUnit,
               clipExtents.Width(), (end - start) / mAppUnitsPerDevUnit) :
          Rect(start / mAppUnitsPerDevUnit, clipExtents.Y(),
               (end - start) / mAppUnitsPerDevUnit, clipExtents.Height());
      MaybeSnapToDevicePixels(clipRect, *aParams.dt, true);

      aParams.context->Save();
      aParams.context->Clip(clipRect);
    }

    gfxPoint pt;
    if (aParams.isVerticalRun) {
        pt = gfxPoint(aPt->x, aPt->y - aParams.direction * data.mPartAdvance);
    } else {
        pt = gfxPoint(aPt->x - aParams.direction * data.mPartAdvance, aPt->y);
    }

    DrawGlyphs(aFont, data.mRange, &pt,
               aProvider, aRange, aParams, aOrientation);
    aParams.context->Restore();

    if (aParams.isVerticalRun) {
        aPt->y += aParams.direction * data.mPartWidth;
    } else {
        aPt->x += aParams.direction * data.mPartWidth;
    }
}

// Returns true if a glyph run is using a font with synthetic bolding enabled,
// or a color font (COLR/SVG/sbix/CBDT), false otherwise. This is used to
// check whether the text run needs to be explicitly composited in order to
// support opacity.
static bool
HasSyntheticBoldOrColor(const gfxTextRun *aRun, gfxTextRun::Range aRange)
{
    gfxTextRun::GlyphRunIterator iter(aRun, aRange);
    while (iter.NextRun()) {
        gfxFont *font = iter.GetGlyphRun()->mFont;
        if (font) {
            if (font->IsSyntheticBold()) {
                return true;
            }
            gfxFontEntry* fe = font->GetFontEntry();
            if (fe->TryGetSVGData(font) || fe->TryGetColorGlyphs()) {
                return true;
            }
#if defined(XP_MACOSX) // sbix fonts only supported via Core Text
            if (fe->HasFontTable(TRUETYPE_TAG('s', 'b', 'i', 'x'))) {
                return true;
            }
#endif
        }
    }
    return false;
}

// Returns true if color is neither opaque nor transparent (i.e. alpha is not 0
// or 1), and false otherwise. If true, aCurrentColorOut is set on output.
static bool
HasNonOpaqueNonTransparentColor(gfxContext *aContext, Color& aCurrentColorOut)
{
    if (aContext->GetDeviceColor(aCurrentColorOut)) {
        if (0.f < aCurrentColorOut.a && aCurrentColorOut.a < 1.f) {
            return true;
        }
    }
    return false;
}

// helper class for double-buffering drawing with non-opaque color
struct BufferAlphaColor {
    explicit BufferAlphaColor(gfxContext *aContext)
        : mContext(aContext)
    {

    }

    ~BufferAlphaColor() {}

    void PushSolidColor(const gfxRect& aBounds, const Color& aAlphaColor, uint32_t appsPerDevUnit)
    {
        mContext->Save();
        mContext->NewPath();
        mContext->Rectangle(gfxRect(aBounds.X() / appsPerDevUnit,
                    aBounds.Y() / appsPerDevUnit,
                    aBounds.Width() / appsPerDevUnit,
                    aBounds.Height() / appsPerDevUnit), true);
        mContext->Clip();
        mContext->SetColor(Color(aAlphaColor.r, aAlphaColor.g, aAlphaColor.b));
        mContext->PushGroupForBlendBack(gfxContentType::COLOR_ALPHA, aAlphaColor.a);
    }

    void PopAlpha()
    {
        // pop the text, using the color alpha as the opacity
        mContext->PopGroupAndBlend();
        mContext->Restore();
    }

    gfxContext *mContext;
};

void
gfxTextRun::Draw(Range aRange, gfxPoint aPt, const DrawParams& aParams) const
{
    NS_ASSERTION(aRange.end <= GetLength(), "Substring out of range");
    NS_ASSERTION(aParams.drawMode == DrawMode::GLYPH_PATH ||
                 !(aParams.drawMode & DrawMode::GLYPH_PATH),
                 "GLYPH_PATH cannot be used with GLYPH_FILL, GLYPH_STROKE or GLYPH_STROKE_UNDERNEATH");
    NS_ASSERTION(aParams.drawMode == DrawMode::GLYPH_PATH || !aParams.callbacks,
                 "callback must not be specified unless using GLYPH_PATH");

    bool skipDrawing = mSkipDrawing;
    if (aParams.drawMode & DrawMode::GLYPH_FILL) {
        Color currentColor;
        if (aParams.context->GetDeviceColor(currentColor) &&
            currentColor.a == 0) {
            skipDrawing = true;
        }
    }

    gfxFloat direction = GetDirection();

    if (skipDrawing) {
        // We don't need to draw anything;
        // but if the caller wants advance width, we need to compute it here
        if (aParams.advanceWidth) {
            gfxTextRun::Metrics metrics = MeasureText(
                aRange, gfxFont::LOOSE_INK_EXTENTS,
                aParams.context->GetDrawTarget(), aParams.provider);
            *aParams.advanceWidth = metrics.mAdvanceWidth * direction;
        }

        // return without drawing
        return;
    }

    // synthetic bolding draws glyphs twice ==> colors with opacity won't draw
    // correctly unless first drawn without alpha
    BufferAlphaColor syntheticBoldBuffer(aParams.context);
    Color currentColor;
    bool needToRestore = false;

    if (aParams.drawMode & DrawMode::GLYPH_FILL &&
        HasNonOpaqueNonTransparentColor(aParams.context, currentColor) &&
        HasSyntheticBoldOrColor(this, aRange)) {
        needToRestore = true;
        // measure text, use the bounding box
        gfxTextRun::Metrics metrics = MeasureText(
            aRange, gfxFont::LOOSE_INK_EXTENTS,
            aParams.context->GetDrawTarget(), aParams.provider);
        metrics.mBoundingBox.MoveBy(aPt);
        syntheticBoldBuffer.PushSolidColor(metrics.mBoundingBox, currentColor,
                                           GetAppUnitsPerDevUnit());
    }

    // Set up parameters that will be constant across all glyph runs we need
    // to draw, regardless of the font used.
    TextRunDrawParams params;
    params.context = aParams.context;
    params.devPerApp = 1.0 / double(GetAppUnitsPerDevUnit());
    params.isVerticalRun = IsVertical();
    params.isRTL = IsRightToLeft();
    params.direction = direction;
    params.strokeOpts = aParams.strokeOpts;
    params.textStrokeColor = aParams.textStrokeColor;
    params.textStrokePattern = aParams.textStrokePattern;
    params.drawOpts = aParams.drawOpts;
    params.drawMode = aParams.drawMode;
    params.callbacks = aParams.callbacks;
    params.runContextPaint = aParams.contextPaint;
    params.paintSVGGlyphs = !aParams.callbacks ||
        aParams.callbacks->mShouldPaintSVGGlyphs;
    params.dt = aParams.context->GetDrawTarget();
    params.fontSmoothingBGColor =
        aParams.context->GetFontSmoothingBackgroundColor();

    GlyphRunIterator iter(this, aRange);
    gfxFloat advance = 0.0;

    while (iter.NextRun()) {
        gfxFont *font = iter.GetGlyphRun()->mFont;
        uint32_t start = iter.GetStringStart();
        uint32_t end = iter.GetStringEnd();
        Range ligatureRange(start, end);
        ShrinkToLigatureBoundaries(&ligatureRange);

        bool drawPartial = (aParams.drawMode & DrawMode::GLYPH_FILL) ||
                           (aParams.drawMode == DrawMode::GLYPH_PATH &&
                            aParams.callbacks);
        gfxPoint origPt = aPt;

        if (drawPartial) {
            DrawPartialLigature(font, Range(start, ligatureRange.start),
                                &aPt, aParams.provider, params,
                                iter.GetGlyphRun()->mOrientation);
        }

        DrawGlyphs(font, ligatureRange, &aPt,
                   aParams.provider, ligatureRange, params,
                   iter.GetGlyphRun()->mOrientation);

        if (drawPartial) {
            DrawPartialLigature(font, Range(ligatureRange.end, end),
                                &aPt, aParams.provider, params,
                                iter.GetGlyphRun()->mOrientation);
        }

        if (params.isVerticalRun) {
            advance += (aPt.y - origPt.y) * params.direction;
        } else {
            advance += (aPt.x - origPt.x) * params.direction;
        }
    }

    // composite result when synthetic bolding used
    if (needToRestore) {
        syntheticBoldBuffer.PopAlpha();
    }

    if (aParams.advanceWidth) {
        *aParams.advanceWidth = advance;
    }
}

// This method is mostly parallel to Draw().
void
gfxTextRun::DrawEmphasisMarks(gfxContext *aContext, gfxTextRun* aMark,
                              gfxFloat aMarkAdvance, gfxPoint aPt,
                              Range aRange, PropertyProvider* aProvider) const
{
    MOZ_ASSERT(aRange.end <= GetLength());

    EmphasisMarkDrawParams params;
    params.context = aContext;
    params.mark = aMark;
    params.advance = aMarkAdvance;
    params.direction = GetDirection();
    params.isVertical = IsVertical();

    gfxFloat& inlineCoord = params.isVertical ? aPt.y : aPt.x;
    gfxFloat direction = params.direction;

    GlyphRunIterator iter(this, aRange);
    while (iter.NextRun()) {
        gfxFont* font = iter.GetGlyphRun()->mFont;
        uint32_t start = iter.GetStringStart();
        uint32_t end = iter.GetStringEnd();
        Range ligatureRange(start, end);
        ShrinkToLigatureBoundaries(&ligatureRange);

        inlineCoord += direction * ComputePartialLigatureWidth(
            Range(start, ligatureRange.start), aProvider);

        AutoTArray<PropertyProvider::Spacing, 200> spacingBuffer;
        bool haveSpacing = GetAdjustedSpacingArray(
            ligatureRange, aProvider, ligatureRange, &spacingBuffer);
        params.spacing = haveSpacing ? spacingBuffer.Elements() : nullptr;
        font->DrawEmphasisMarks(this, &aPt, ligatureRange.start,
                                ligatureRange.Length(), params);

        inlineCoord += direction * ComputePartialLigatureWidth(
            Range(ligatureRange.end, end), aProvider);
    }
}

void
gfxTextRun::AccumulateMetricsForRun(gfxFont *aFont, Range aRange,
                                    gfxFont::BoundingBoxType aBoundingBoxType,
                                    DrawTarget* aRefDrawTarget,
                                    PropertyProvider *aProvider,
                                    Range aSpacingRange,
                                    uint16_t aOrientation,
                                    Metrics *aMetrics) const
{
    AutoTArray<PropertyProvider::Spacing,200> spacingBuffer;
    bool haveSpacing = GetAdjustedSpacingArray(aRange, aProvider,
                                               aSpacingRange, &spacingBuffer);
    Metrics metrics = aFont->Measure(this, aRange.start, aRange.end,
                                     aBoundingBoxType, aRefDrawTarget,
                                     haveSpacing ? spacingBuffer.Elements() : nullptr,
                                     aOrientation);
    aMetrics->CombineWith(metrics, IsRightToLeft());
}

void
gfxTextRun::AccumulatePartialLigatureMetrics(gfxFont *aFont, Range aRange,
    gfxFont::BoundingBoxType aBoundingBoxType, DrawTarget* aRefDrawTarget,
    PropertyProvider *aProvider, uint16_t aOrientation,
    Metrics *aMetrics) const
{
    if (aRange.start >= aRange.end)
        return;

    // Measure partial ligature. We hack this by clipping the metrics in the
    // same way we clip the drawing.
    LigatureData data = ComputeLigatureData(aRange, aProvider);

    // First measure the complete ligature
    Metrics metrics;
    AccumulateMetricsForRun(aFont, data.mRange,
                            aBoundingBoxType, aRefDrawTarget,
                            aProvider, aRange, aOrientation, &metrics);

    // Clip the bounding box to the ligature part
    gfxFloat bboxLeft = metrics.mBoundingBox.X();
    gfxFloat bboxRight = metrics.mBoundingBox.XMost();
    // Where we are going to start "drawing" relative to our left baseline origin
    gfxFloat origin = IsRightToLeft() ? metrics.mAdvanceWidth - data.mPartAdvance : 0;
    ClipPartialLigature(this, &bboxLeft, &bboxRight, origin, &data);
    metrics.mBoundingBox.x = bboxLeft;
    metrics.mBoundingBox.width = bboxRight - bboxLeft;

    // mBoundingBox is now relative to the left baseline origin for the entire
    // ligature. Shift it left.
    metrics.mBoundingBox.x -=
        IsRightToLeft() ? metrics.mAdvanceWidth - (data.mPartAdvance + data.mPartWidth)
            : data.mPartAdvance;    
    metrics.mAdvanceWidth = data.mPartWidth;

    aMetrics->CombineWith(metrics, IsRightToLeft());
}

gfxTextRun::Metrics
gfxTextRun::MeasureText(Range aRange,
                        gfxFont::BoundingBoxType aBoundingBoxType,
                        DrawTarget* aRefDrawTarget,
                        PropertyProvider *aProvider) const
{
    NS_ASSERTION(aRange.end <= GetLength(), "Substring out of range");

    Metrics accumulatedMetrics;
    GlyphRunIterator iter(this, aRange);
    while (iter.NextRun()) {
        gfxFont *font = iter.GetGlyphRun()->mFont;
        uint32_t start = iter.GetStringStart();
        uint32_t end = iter.GetStringEnd();
        Range ligatureRange(start, end);
        ShrinkToLigatureBoundaries(&ligatureRange);

        AccumulatePartialLigatureMetrics(
            font, Range(start, ligatureRange.start),
            aBoundingBoxType, aRefDrawTarget, aProvider,
            iter.GetGlyphRun()->mOrientation, &accumulatedMetrics);

        // XXX This sucks. We have to get glyph extents just so we can detect
        // glyphs outside the font box, even when aBoundingBoxType is LOOSE,
        // even though in almost all cases we could get correct results just
        // by getting some ascent/descent from the font and using our stored
        // advance widths.
        AccumulateMetricsForRun(font,
            ligatureRange, aBoundingBoxType,
            aRefDrawTarget, aProvider, ligatureRange,
            iter.GetGlyphRun()->mOrientation, &accumulatedMetrics);

        AccumulatePartialLigatureMetrics(
            font, Range(ligatureRange.end, end),
            aBoundingBoxType, aRefDrawTarget, aProvider,
            iter.GetGlyphRun()->mOrientation, &accumulatedMetrics);
    }

    return accumulatedMetrics;
}

#define MEASUREMENT_BUFFER_SIZE 100

uint32_t
gfxTextRun::BreakAndMeasureText(uint32_t aStart, uint32_t aMaxLength,
                                bool aLineBreakBefore, gfxFloat aWidth,
                                PropertyProvider *aProvider,
                                SuppressBreak aSuppressBreak,
                                gfxFloat *aTrimWhitespace,
                                bool aWhitespaceCanHang,
                                Metrics *aMetrics,
                                gfxFont::BoundingBoxType aBoundingBoxType,
                                DrawTarget* aRefDrawTarget,
                                bool *aUsedHyphenation,
                                uint32_t *aLastBreak,
                                bool aCanWordWrap,
                                gfxBreakPriority *aBreakPriority)
{
    aMaxLength = std::min(aMaxLength, GetLength() - aStart);

    NS_ASSERTION(aStart + aMaxLength <= GetLength(), "Substring out of range");

    Range bufferRange(aStart, aStart +
        std::min<uint32_t>(aMaxLength, MEASUREMENT_BUFFER_SIZE));
    PropertyProvider::Spacing spacingBuffer[MEASUREMENT_BUFFER_SIZE];
    bool haveSpacing = aProvider && (mFlags & gfxTextRunFactory::TEXT_ENABLE_SPACING) != 0;
    if (haveSpacing) {
        GetAdjustedSpacing(this, bufferRange, aProvider, spacingBuffer);
    }
    bool hyphenBuffer[MEASUREMENT_BUFFER_SIZE];
    bool haveHyphenation = aProvider &&
        (aProvider->GetHyphensOption() == NS_STYLE_HYPHENS_AUTO ||
         (aProvider->GetHyphensOption() == NS_STYLE_HYPHENS_MANUAL &&
          (mFlags & gfxTextRunFactory::TEXT_ENABLE_HYPHEN_BREAKS) != 0));
    if (haveHyphenation) {
        aProvider->GetHyphenationBreaks(bufferRange, hyphenBuffer);
    }

    gfxFloat width = 0;
    gfxFloat advance = 0;
    // The number of space characters that can be trimmed or hang at a soft-wrap
    uint32_t trimmableChars = 0;
    // The amount of space removed by ignoring trimmableChars
    gfxFloat trimmableAdvance = 0;
    int32_t lastBreak = -1;
    int32_t lastBreakTrimmableChars = -1;
    gfxFloat lastBreakTrimmableAdvance = -1;
    bool aborted = false;
    uint32_t end = aStart + aMaxLength;
    bool lastBreakUsedHyphenation = false;

    Range ligatureRange(aStart, end);
    ShrinkToLigatureBoundaries(&ligatureRange);

    uint32_t i;
    for (i = aStart; i < end; ++i) {
        if (i >= bufferRange.end) {
            // Fetch more spacing and hyphenation data
            bufferRange.start = i;
            bufferRange.end = std::min(aStart + aMaxLength,
                                       i + MEASUREMENT_BUFFER_SIZE);
            if (haveSpacing) {
                GetAdjustedSpacing(this, bufferRange, aProvider, spacingBuffer);
            }
            if (haveHyphenation) {
                aProvider->GetHyphenationBreaks(bufferRange, hyphenBuffer);
            }
        }

        // There can't be a word-wrap break opportunity at the beginning of the
        // line: if the width is too small for even one character to fit, it 
        // could be the first and last break opportunity on the line, and that
        // would trigger an infinite loop.
        if (aSuppressBreak != eSuppressAllBreaks &&
            (aSuppressBreak != eSuppressInitialBreak || i > aStart)) {
            bool atNaturalBreak = mCharacterGlyphs[i].CanBreakBefore() == 1;
            bool atHyphenationBreak = !atNaturalBreak &&
                haveHyphenation && hyphenBuffer[i - bufferRange.start];
            bool atBreak = atNaturalBreak || atHyphenationBreak;
            bool wordWrapping =
                aCanWordWrap && mCharacterGlyphs[i].IsClusterStart() &&
                *aBreakPriority <= gfxBreakPriority::eWordWrapBreak;

            if (atBreak || wordWrapping) {
                gfxFloat hyphenatedAdvance = advance;
                if (atHyphenationBreak) {
                    hyphenatedAdvance += aProvider->GetHyphenWidth();
                }
            
                if (lastBreak < 0 || width + hyphenatedAdvance - trimmableAdvance <= aWidth) {
                    // We can break here.
                    lastBreak = i;
                    lastBreakTrimmableChars = trimmableChars;
                    lastBreakTrimmableAdvance = trimmableAdvance;
                    lastBreakUsedHyphenation = atHyphenationBreak;
                    *aBreakPriority = atBreak ? gfxBreakPriority::eNormalBreak
                                              : gfxBreakPriority::eWordWrapBreak;
                }

                width += advance;
                advance = 0;
                if (width - trimmableAdvance > aWidth) {
                    // No more text fits. Abort
                    aborted = true;
                    break;
                }
            }
        }
        
        gfxFloat charAdvance;
        if (i >= ligatureRange.start && i < ligatureRange.end) {
            charAdvance = GetAdvanceForGlyphs(Range(i, i + 1));
            if (haveSpacing) {
                PropertyProvider::Spacing *space =
                    &spacingBuffer[i - bufferRange.start];
                charAdvance += space->mBefore + space->mAfter;
            }
        } else {
            charAdvance =
                ComputePartialLigatureWidth(Range(i, i + 1), aProvider);
        }
        
        advance += charAdvance;
        if (aTrimWhitespace || aWhitespaceCanHang) {
            if (mCharacterGlyphs[i].CharIsSpace()) {
                ++trimmableChars;
                trimmableAdvance += charAdvance;
            } else {
                trimmableAdvance = 0;
                trimmableChars = 0;
            }
        }
    }

    if (!aborted) {
        width += advance;
    }

    // There are three possibilities:
    // 1) all the text fit (width <= aWidth)
    // 2) some of the text fit up to a break opportunity (width > aWidth && lastBreak >= 0)
    // 3) none of the text fits before a break opportunity (width > aWidth && lastBreak < 0)
    uint32_t charsFit;
    bool usedHyphenation = false;
    if (width - trimmableAdvance <= aWidth) {
        charsFit = aMaxLength;
    } else if (lastBreak >= 0) {
        charsFit = lastBreak - aStart;
        trimmableChars = lastBreakTrimmableChars;
        trimmableAdvance = lastBreakTrimmableAdvance;
        usedHyphenation = lastBreakUsedHyphenation;
    } else {
        charsFit = aMaxLength;
    }

    if (aMetrics) {
        auto fitEnd = aStart + charsFit;
        // Initially, measure everything, so that our bounding box includes
        // any trimmable or hanging whitespace.
        *aMetrics = MeasureText(Range(aStart, fitEnd),
                                aBoundingBoxType, aRefDrawTarget,
                                aProvider);
        if (aTrimWhitespace || aWhitespaceCanHang) {
            // Measure trailing whitespace that is to be trimmed/hung.
            Metrics trimOrHangMetrics =
                MeasureText(Range(fitEnd - trimmableChars, fitEnd),
                            aBoundingBoxType, aRefDrawTarget,
                            aProvider);
            if (aTrimWhitespace) {
                aMetrics->mAdvanceWidth -= trimOrHangMetrics.mAdvanceWidth;
            } else if (aMetrics->mAdvanceWidth > aWidth) {
                // Restrict width of hanging whitespace so it doesn't overflow.
                aMetrics->mAdvanceWidth =
                    std::max(aWidth, aMetrics->mAdvanceWidth -
                                     trimOrHangMetrics.mAdvanceWidth);
            }
        }
    }
    if (aTrimWhitespace) {
        *aTrimWhitespace = trimmableAdvance;
    }
    if (aUsedHyphenation) {
        *aUsedHyphenation = usedHyphenation;
    }
    if (aLastBreak && charsFit == aMaxLength) {
        if (lastBreak < 0) {
            *aLastBreak = UINT32_MAX;
        } else {
            *aLastBreak = lastBreak - aStart;
        }
    }

    return charsFit;
}

gfxFloat
gfxTextRun::GetAdvanceWidth(Range aRange, PropertyProvider *aProvider,
                            PropertyProvider::Spacing* aSpacing) const
{
    NS_ASSERTION(aRange.end <= GetLength(), "Substring out of range");

    Range ligatureRange = aRange;
    ShrinkToLigatureBoundaries(&ligatureRange);

    gfxFloat result =
        ComputePartialLigatureWidth(Range(aRange.start, ligatureRange.start),
                                    aProvider) +
        ComputePartialLigatureWidth(Range(ligatureRange.end, aRange.end),
                                    aProvider);

    if (aSpacing) {
        aSpacing->mBefore = aSpacing->mAfter = 0;
    }

    // Account for all remaining spacing here. This is more efficient than
    // processing it along with the glyphs.
    if (aProvider && (mFlags & gfxTextRunFactory::TEXT_ENABLE_SPACING)) {
        uint32_t i;
        AutoTArray<PropertyProvider::Spacing,200> spacingBuffer;
        if (spacingBuffer.AppendElements(aRange.Length())) {
            GetAdjustedSpacing(this, ligatureRange, aProvider,
                               spacingBuffer.Elements());
            for (i = 0; i < ligatureRange.Length(); ++i) {
                PropertyProvider::Spacing *space = &spacingBuffer[i];
                result += space->mBefore + space->mAfter;
            }
            if (aSpacing) {
                aSpacing->mBefore = spacingBuffer[0].mBefore;
                aSpacing->mAfter = spacingBuffer.LastElement().mAfter;
            }
        }
    }

    return result + GetAdvanceForGlyphs(ligatureRange);
}

bool
gfxTextRun::SetLineBreaks(Range aRange,
                          bool aLineBreakBefore, bool aLineBreakAfter,
                          gfxFloat *aAdvanceWidthDelta)
{
    // Do nothing because our shaping does not currently take linebreaks into
    // account. There is no change in advance width.
    if (aAdvanceWidthDelta) {
        *aAdvanceWidthDelta = 0;
    }
    return false;
}

uint32_t
gfxTextRun::FindFirstGlyphRunContaining(uint32_t aOffset) const
{
    NS_ASSERTION(aOffset <= GetLength(), "Bad offset looking for glyphrun");
    NS_ASSERTION(GetLength() == 0 || mGlyphRuns.Length() > 0,
                 "non-empty text but no glyph runs present!");
    if (aOffset == GetLength())
        return mGlyphRuns.Length();
    uint32_t start = 0;
    uint32_t end = mGlyphRuns.Length();
    while (end - start > 1) {
        uint32_t mid = (start + end)/2;
        if (mGlyphRuns[mid].mCharacterOffset <= aOffset) {
            start = mid;
        } else {
            end = mid;
        }
    }
    NS_ASSERTION(mGlyphRuns[start].mCharacterOffset <= aOffset,
                 "Hmm, something went wrong, aOffset should have been found");
    return start;
}

nsresult
gfxTextRun::AddGlyphRun(gfxFont *aFont, uint8_t aMatchType,
                        uint32_t aUTF16Offset, bool aForceNewRun,
                        uint16_t aOrientation)
{
    NS_ASSERTION(aFont, "adding glyph run for null font!");
    NS_ASSERTION(aOrientation != gfxTextRunFactory::TEXT_ORIENT_VERTICAL_MIXED,
                 "mixed orientation should have been resolved");
    if (!aFont) {
        return NS_OK;
    }    
    uint32_t numGlyphRuns = mGlyphRuns.Length();
    if (!aForceNewRun && numGlyphRuns > 0) {
        GlyphRun *lastGlyphRun = &mGlyphRuns[numGlyphRuns - 1];

        NS_ASSERTION(lastGlyphRun->mCharacterOffset <= aUTF16Offset,
                     "Glyph runs out of order (and run not forced)");

        // Don't append a run if the font is already the one we want
        if (lastGlyphRun->mFont == aFont &&
            lastGlyphRun->mMatchType == aMatchType &&
            lastGlyphRun->mOrientation == aOrientation)
        {
            return NS_OK;
        }

        // If the offset has not changed, avoid leaving a zero-length run
        // by overwriting the last entry instead of appending...
        if (lastGlyphRun->mCharacterOffset == aUTF16Offset) {

            // ...except that if the run before the last entry had the same
            // font as the new one wants, merge with it instead of creating
            // adjacent runs with the same font
            if (numGlyphRuns > 1 &&
                mGlyphRuns[numGlyphRuns - 2].mFont == aFont &&
                mGlyphRuns[numGlyphRuns - 2].mMatchType == aMatchType &&
                mGlyphRuns[numGlyphRuns - 2].mOrientation == aOrientation)
            {
                mGlyphRuns.TruncateLength(numGlyphRuns - 1);
                return NS_OK;
            }

            lastGlyphRun->mFont = aFont;
            lastGlyphRun->mMatchType = aMatchType;
            lastGlyphRun->mOrientation = aOrientation;
            return NS_OK;
        }
    }

    NS_ASSERTION(aForceNewRun || numGlyphRuns > 0 || aUTF16Offset == 0,
                 "First run doesn't cover the first character (and run not forced)?");

    GlyphRun *glyphRun = mGlyphRuns.AppendElement();
    if (!glyphRun)
        return NS_ERROR_OUT_OF_MEMORY;
    glyphRun->mFont = aFont;
    glyphRun->mCharacterOffset = aUTF16Offset;
    glyphRun->mMatchType = aMatchType;
    glyphRun->mOrientation = aOrientation;
    return NS_OK;
}

void
gfxTextRun::SortGlyphRuns()
{
    if (mGlyphRuns.Length() <= 1)
        return;

    nsTArray<GlyphRun> runs(mGlyphRuns);
    GlyphRunOffsetComparator comp;
    runs.Sort(comp);

    // Now copy back, coalescing adjacent glyph runs that have the same font
    mGlyphRuns.Clear();
    uint32_t i, count = runs.Length();
    for (i = 0; i < count; ++i) {
        // a GlyphRun with the same font and orientation as the previous can
        // just be skipped; the last GlyphRun will cover its character range.
        if (i == 0 || runs[i].mFont != runs[i - 1].mFont ||
            runs[i].mOrientation != runs[i - 1].mOrientation) {
            mGlyphRuns.AppendElement(runs[i]);
            // If two fonts have the same character offset, Sort() will have
            // randomized the order.
            NS_ASSERTION(i == 0 ||
                         runs[i].mCharacterOffset !=
                         runs[i - 1].mCharacterOffset,
                         "Two fonts for the same run, glyph indices may not match the font");
        }
    }
}

// Note that SanitizeGlyphRuns scans all glyph runs in the textrun;
// therefore we only call it once, at the end of textrun construction,
// NOT incrementally as each glyph run is added (bug 680402).
void
gfxTextRun::SanitizeGlyphRuns()
{
    if (mGlyphRuns.Length() <= 1)
        return;

    // If any glyph run starts with ligature-continuation characters, we need to advance it
    // to the first "real" character to avoid drawing partial ligature glyphs from wrong font
    // (seen with U+FEFF in reftest 474417-1, as Core Text eliminates the glyph, which makes
    // it appear as if a ligature has been formed)
    int32_t i, lastRunIndex = mGlyphRuns.Length() - 1;
    const CompressedGlyph *charGlyphs = mCharacterGlyphs;
    for (i = lastRunIndex; i >= 0; --i) {
        GlyphRun& run = mGlyphRuns[i];
        while (charGlyphs[run.mCharacterOffset].IsLigatureContinuation() &&
               run.mCharacterOffset < GetLength()) {
            run.mCharacterOffset++;
        }
        // if the run has become empty, eliminate it
        if ((i < lastRunIndex &&
             run.mCharacterOffset >= mGlyphRuns[i+1].mCharacterOffset) ||
            (i == lastRunIndex && run.mCharacterOffset == GetLength())) {
            mGlyphRuns.RemoveElementAt(i);
            --lastRunIndex;
        }
    }
}

uint32_t
gfxTextRun::CountMissingGlyphs() const
{
    uint32_t i;
    uint32_t count = 0;
    for (i = 0; i < GetLength(); ++i) {
        if (mCharacterGlyphs[i].IsMissing()) {
            ++count;
        }
    }
    return count;
}

void
gfxTextRun::CopyGlyphDataFrom(gfxShapedWord *aShapedWord, uint32_t aOffset)
{
    uint32_t wordLen = aShapedWord->GetLength();
    NS_ASSERTION(aOffset + wordLen <= GetLength(),
                 "word overruns end of textrun!");

    CompressedGlyph *charGlyphs = GetCharacterGlyphs();
    const CompressedGlyph *wordGlyphs = aShapedWord->GetCharacterGlyphs();
    if (aShapedWord->HasDetailedGlyphs()) {
        for (uint32_t i = 0; i < wordLen; ++i, ++aOffset) {
            const CompressedGlyph& g = wordGlyphs[i];
            if (g.IsSimpleGlyph()) {
                charGlyphs[aOffset] = g;
            } else {
                const DetailedGlyph *details =
                    g.GetGlyphCount() > 0 ?
                        aShapedWord->GetDetailedGlyphs(i) : nullptr;
                SetGlyphs(aOffset, g, details);
            }
        }
    } else {
        memcpy(charGlyphs + aOffset, wordGlyphs,
               wordLen * sizeof(CompressedGlyph));
    }
}

void
gfxTextRun::CopyGlyphDataFrom(gfxTextRun *aSource, Range aRange, uint32_t aDest)
{
    NS_ASSERTION(aRange.end <= aSource->GetLength(),
                 "Source substring out of range");
    NS_ASSERTION(aDest + aRange.Length() <= GetLength(),
                 "Destination substring out of range");

    if (aSource->mSkipDrawing) {
        mSkipDrawing = true;
    }

    // Copy base glyph data, and DetailedGlyph data where present
    const CompressedGlyph *srcGlyphs = aSource->mCharacterGlyphs + aRange.start;
    CompressedGlyph *dstGlyphs = mCharacterGlyphs + aDest;
    for (uint32_t i = 0; i < aRange.Length(); ++i) {
        CompressedGlyph g = srcGlyphs[i];
        g.SetCanBreakBefore(!g.IsClusterStart() ?
            CompressedGlyph::FLAG_BREAK_TYPE_NONE :
            dstGlyphs[i].CanBreakBefore());
        if (!g.IsSimpleGlyph()) {
            uint32_t count = g.GetGlyphCount();
            if (count > 0) {
                DetailedGlyph *dst = AllocateDetailedGlyphs(i + aDest, count);
                if (dst) {
                    DetailedGlyph *src =
                        aSource->GetDetailedGlyphs(i + aRange.start);
                    if (src) {
                        ::memcpy(dst, src, count * sizeof(DetailedGlyph));
                    } else {
                        g.SetMissing(0);
                    }
                } else {
                    g.SetMissing(0);
                }
            }
        }
        dstGlyphs[i] = g;
    }

    // Copy glyph runs
    GlyphRunIterator iter(aSource, aRange);
#ifdef DEBUG
    const GlyphRun *prevRun = nullptr;
#endif
    while (iter.NextRun()) {
        gfxFont *font = iter.GetGlyphRun()->mFont;
        NS_ASSERTION(!prevRun || prevRun->mFont != iter.GetGlyphRun()->mFont ||
                     prevRun->mMatchType != iter.GetGlyphRun()->mMatchType ||
                     prevRun->mOrientation != iter.GetGlyphRun()->mOrientation,
                     "Glyphruns not coalesced?");
#ifdef DEBUG
        prevRun = iter.GetGlyphRun();
        uint32_t end = iter.GetStringEnd();
#endif
        uint32_t start = iter.GetStringStart();

        // These used to be NS_ASSERTION()s, but WARNING is more appropriate.
        // Although it's unusual (and not desirable), it's possible for us to assign
        // different fonts to a base character and a following diacritic.
        // Example on OSX 10.5/10.6 with default fonts installed:
        //     data:text/html,<p style="font-family:helvetica, arial, sans-serif;">
        //                    &%23x043E;&%23x0486;&%23x20;&%23x043E;&%23x0486;
        // This means the rendering of the cluster will probably not be very good,
        // but it's the best we can do for now if the specified font only covered the
        // initial base character and not its applied marks.
        NS_WARNING_ASSERTION(
          aSource->IsClusterStart(start),
          "Started font run in the middle of a cluster");
        NS_WARNING_ASSERTION(
          end == aSource->GetLength() || aSource->IsClusterStart(end),
          "Ended font run in the middle of a cluster");

        nsresult rv = AddGlyphRun(font, iter.GetGlyphRun()->mMatchType,
                                  start - aRange.start + aDest, false,
                                  iter.GetGlyphRun()->mOrientation);
        if (NS_FAILED(rv))
            return;
    }
}

void
gfxTextRun::ClearGlyphsAndCharacters()
{
    ResetGlyphRuns();
    memset(reinterpret_cast<char*>(mCharacterGlyphs), 0,
           mLength * sizeof(CompressedGlyph));
    mDetailedGlyphs = nullptr;
}

void
gfxTextRun::SetSpaceGlyph(gfxFont* aFont, DrawTarget* aDrawTarget,
                          uint32_t aCharIndex, uint16_t aOrientation)
{
    if (SetSpaceGlyphIfSimple(aFont, aCharIndex, ' ', aOrientation)) {
        return;
    }

    aFont->InitWordCache();
    static const uint8_t space = ' ';
    uint32_t flags = gfxTextRunFactory::TEXT_IS_8BIT |
                     gfxTextRunFactory::TEXT_IS_ASCII |
                     gfxTextRunFactory::TEXT_IS_PERSISTENT |
                     aOrientation;
    bool vertical =
        (GetFlags() & gfxTextRunFactory::TEXT_ORIENT_VERTICAL_UPRIGHT) != 0;
    gfxShapedWord* sw = aFont->GetShapedWord(aDrawTarget,
                                             &space, 1,
                                             gfxShapedWord::HashMix(0, ' '), 
                                             Script::LATIN,
                                             vertical,
                                             mAppUnitsPerDevUnit,
                                             flags,
                                             nullptr);
    if (sw) {
        AddGlyphRun(aFont, gfxTextRange::kFontGroup, aCharIndex, false,
                    aOrientation);
        CopyGlyphDataFrom(sw, aCharIndex);
    }
}

bool
gfxTextRun::SetSpaceGlyphIfSimple(gfxFont* aFont, uint32_t aCharIndex,
                                  char16_t aSpaceChar, uint16_t aOrientation)
{
    uint32_t spaceGlyph = aFont->GetSpaceGlyph();
    if (!spaceGlyph || !CompressedGlyph::IsSimpleGlyphID(spaceGlyph)) {
        return false;
    }

    gfxFont::Orientation fontOrientation =
        (aOrientation & gfxTextRunFactory::TEXT_ORIENT_VERTICAL_UPRIGHT) ?
            gfxFont::eVertical : gfxFont::eHorizontal;
    uint32_t spaceWidthAppUnits =
        NS_lroundf(aFont->GetMetrics(fontOrientation).spaceWidth *
                   mAppUnitsPerDevUnit);
    if (!CompressedGlyph::IsSimpleAdvance(spaceWidthAppUnits)) {
        return false;
    }

    AddGlyphRun(aFont, gfxTextRange::kFontGroup, aCharIndex, false,
                aOrientation);
    CompressedGlyph g;
    g.SetSimpleGlyph(spaceWidthAppUnits, spaceGlyph);
    if (aSpaceChar == ' ') {
        g.SetIsSpace();
    }
    GetCharacterGlyphs()[aCharIndex] = g;
    return true;
}

void
gfxTextRun::FetchGlyphExtents(DrawTarget* aRefDrawTarget)
{
    bool needsGlyphExtents = NeedsGlyphExtents(this);
    if (!needsGlyphExtents && !mDetailedGlyphs)
        return;

    uint32_t i, runCount = mGlyphRuns.Length();
    CompressedGlyph *charGlyphs = mCharacterGlyphs;
    for (i = 0; i < runCount; ++i) {
        const GlyphRun& run = mGlyphRuns[i];
        gfxFont *font = run.mFont;
        if (MOZ_UNLIKELY(font->GetStyle()->size == 0) ||
            MOZ_UNLIKELY(font->GetStyle()->sizeAdjust == 0.0f)) {
            continue;
        }

        uint32_t start = run.mCharacterOffset;
        uint32_t end = i + 1 < runCount ?
            mGlyphRuns[i + 1].mCharacterOffset : GetLength();
        bool fontIsSetup = false;
        uint32_t j;
        gfxGlyphExtents *extents = font->GetOrCreateGlyphExtents(mAppUnitsPerDevUnit);
  
        for (j = start; j < end; ++j) {
            const gfxTextRun::CompressedGlyph *glyphData = &charGlyphs[j];
            if (glyphData->IsSimpleGlyph()) {
                // If we're in speed mode, don't set up glyph extents here; we'll
                // just return "optimistic" glyph bounds later
                if (needsGlyphExtents) {
                    uint32_t glyphIndex = glyphData->GetSimpleGlyph();
                    if (!extents->IsGlyphKnown(glyphIndex)) {
                        if (!fontIsSetup) {
                            if (!font->SetupCairoFont(aRefDrawTarget)) {
                                NS_WARNING("failed to set up font for glyph extents");
                                break;
                            }
                            fontIsSetup = true;
                        }
#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
                        ++gGlyphExtentsSetupEagerSimple;
#endif
                        font->SetupGlyphExtents(aRefDrawTarget,
                                                glyphIndex, false, extents);
                    }
                }
            } else if (!glyphData->IsMissing()) {
                uint32_t glyphCount = glyphData->GetGlyphCount();
                if (glyphCount == 0) {
                    continue;
                }
                const gfxTextRun::DetailedGlyph *details = GetDetailedGlyphs(j);
                if (!details) {
                    continue;
                }
                for (uint32_t k = 0; k < glyphCount; ++k, ++details) {
                    uint32_t glyphIndex = details->mGlyphID;
                    if (!extents->IsGlyphKnownWithTightExtents(glyphIndex)) {
                        if (!fontIsSetup) {
                            if (!font->SetupCairoFont(aRefDrawTarget)) {
                                NS_WARNING("failed to set up font for glyph extents");
                                break;
                            }
                            fontIsSetup = true;
                        }
#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
                        ++gGlyphExtentsSetupEagerTight;
#endif
                        font->SetupGlyphExtents(aRefDrawTarget,
                                                glyphIndex, true, extents);
                    }
                }
            }
        }
    }
}


size_t
gfxTextRun::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf)
{
    // The second arg is how much gfxTextRun::AllocateStorage would have
    // allocated.
    size_t total = mGlyphRuns.ShallowSizeOfExcludingThis(aMallocSizeOf);

    if (mDetailedGlyphs) {
        total += mDetailedGlyphs->SizeOfIncludingThis(aMallocSizeOf);
    }

    return total;
}

size_t
gfxTextRun::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf)
{
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}


#ifdef DEBUG
void
gfxTextRun::Dump(FILE* aOutput) {
    if (!aOutput) {
        aOutput = stdout;
    }

    uint32_t i;
    fputc('[', aOutput);
    for (i = 0; i < mGlyphRuns.Length(); ++i) {
        if (i > 0) {
            fputc(',', aOutput);
        }
        gfxFont* font = mGlyphRuns[i].mFont;
        const gfxFontStyle* style = font->GetStyle();
        NS_ConvertUTF16toUTF8 fontName(font->GetName());
        nsAutoCString lang;
        style->language->ToUTF8String(lang);
        fprintf(aOutput, "%d: %s %f/%d/%d/%s", mGlyphRuns[i].mCharacterOffset,
                fontName.get(), style->size,
                style->weight, style->style, lang.get());
    }
    fputc(']', aOutput);
}
#endif

gfxFontGroup::gfxFontGroup(const FontFamilyList& aFontFamilyList,
                           const gfxFontStyle *aStyle,
                           gfxTextPerfMetrics* aTextPerf,
                           gfxUserFontSet *aUserFontSet,
                           gfxFloat aDevToCssSize)
    : mFamilyList(aFontFamilyList)
    , mStyle(*aStyle)
    , mUnderlineOffset(UNDERLINE_OFFSET_NOT_SET)
    , mHyphenWidth(-1)
    , mDevToCssSize(aDevToCssSize)
    , mUserFontSet(aUserFontSet)
    , mTextPerf(aTextPerf)
    , mLastPrefLang(eFontPrefLang_Western)
    , mPageLang(gfxPlatformFontList::GetFontPrefLangFor(aStyle->language))
    , mLastPrefFirstFont(false)
    , mSkipDrawing(false)
    , mSkipUpdateUserFonts(false)
{
    // We don't use SetUserFontSet() here, as we want to unconditionally call
    // BuildFontList() rather than only do UpdateUserFonts() if it changed.
    mCurrGeneration = GetGeneration();
    BuildFontList();
}

gfxFontGroup::~gfxFontGroup()
{
}

void
gfxFontGroup::BuildFontList()
{
    bool enumerateFonts = true;

#if defined(MOZ_WIDGET_GTK)
    // xxx - eliminate this once gfxPangoFontGroup is no longer needed
    enumerateFonts = gfxPlatformGtk::UseFcFontList();
#endif
    if (!enumerateFonts) {
        return;
    }

    // initialize fonts in the font family list
    AutoTArray<gfxFontFamily*,10> fonts;
    gfxPlatformFontList *pfl = gfxPlatformFontList::PlatformFontList();

    // lookup fonts in the fontlist
    for (const FontFamilyName& name : mFamilyList.GetFontlist()) {
        if (name.IsNamed()) {
            AddPlatformFont(name.mName, fonts);
        } else {
            pfl->AddGenericFonts(name.mType, mStyle.language, fonts);
            if (mTextPerf) {
                mTextPerf->current.genericLookups++;
            }
        }
    }

    // if necessary, append default generic onto the end
    if (mFamilyList.GetDefaultFontType() != eFamily_none &&
        !mFamilyList.HasDefaultGeneric()) {
        pfl->AddGenericFonts(mFamilyList.GetDefaultFontType(),
                             mStyle.language, fonts);
        if (mTextPerf) {
            mTextPerf->current.genericLookups++;
        }
    }

    // build the fontlist from the specified families
    for (gfxFontFamily* fontFamily : fonts) {
        AddFamilyToFontList(fontFamily);
    }
}

void
gfxFontGroup::AddPlatformFont(const nsAString& aName,
                              nsTArray<gfxFontFamily*>& aFamilyList)
{
    // First, look up in the user font set...
    // If the fontSet matches the family, we must not look for a platform
    // font of the same name, even if we fail to actually get a fontEntry
    // here; we'll fall back to the next name in the CSS font-family list.
    if (mUserFontSet) {
        // Add userfonts to the fontlist whether already loaded
        // or not. Loading is initiated during font matching.
        gfxFontFamily* family = mUserFontSet->LookupFamily(aName);
        if (family) {
            aFamilyList.AppendElement(family);
            return;
        }
    }

    // Not known in the user font set ==> check system fonts
    gfxPlatformFontList::PlatformFontList()
        ->FindAndAddFamilies(aName, &aFamilyList, &mStyle, mDevToCssSize);
}

void
gfxFontGroup::AddFamilyToFontList(gfxFontFamily* aFamily)
{
    NS_ASSERTION(aFamily, "trying to add a null font family to fontlist");
    AutoTArray<gfxFontEntry*,4> fontEntryList;
    bool needsBold;
    aFamily->FindAllFontsForStyle(mStyle, fontEntryList, needsBold);
    // add these to the fontlist
    for (gfxFontEntry* fe : fontEntryList) {
        if (!HasFont(fe)) {
            FamilyFace ff(aFamily, fe, needsBold);
            if (fe->mIsUserFontContainer) {
                ff.CheckState(mSkipDrawing);
            }
            mFonts.AppendElement(ff);
        }
    }
    // for a family marked as "check fallback faces", only mark the last
    // entry so that fallbacks for a family are only checked once
    if (aFamily->CheckForFallbackFaces() &&
        !fontEntryList.IsEmpty() && !mFonts.IsEmpty()) {
        mFonts.LastElement().SetCheckForFallbackFaces();
    }
}

bool
gfxFontGroup::HasFont(const gfxFontEntry *aFontEntry)
{
    uint32_t count = mFonts.Length();
    for (uint32_t i = 0; i < count; ++i) {
        if (mFonts[i].FontEntry() == aFontEntry) {
            return true;
        }
    }
    return false;
}

gfxFont*
gfxFontGroup::GetFontAt(int32_t i, uint32_t aCh)
{
    if (uint32_t(i) >= mFonts.Length()) {
        return nullptr;
    }

    FamilyFace& ff = mFonts[i];
    if (ff.IsInvalid() || ff.IsLoading()) {
        return nullptr;
    }

    RefPtr<gfxFont> font = ff.Font();
    if (!font) {
        gfxFontEntry* fe = mFonts[i].FontEntry();
        gfxCharacterMap* unicodeRangeMap = nullptr;
        if (fe->mIsUserFontContainer) {
            gfxUserFontEntry* ufe = static_cast<gfxUserFontEntry*>(fe);
            if (ufe->LoadState() == gfxUserFontEntry::STATUS_NOT_LOADED &&
                ufe->CharacterInUnicodeRange(aCh) &&
                !FontLoadingForFamily(ff.Family(), aCh)) {
                ufe->Load();
                ff.CheckState(mSkipDrawing);
            }
            fe = ufe->GetPlatformFontEntry();
            if (!fe) {
                return nullptr;
            }
            unicodeRangeMap = ufe->GetUnicodeRangeMap();
        }
        font = fe->FindOrMakeFont(&mStyle, mFonts[i].NeedsBold(),
                                  unicodeRangeMap);
        if (!font || !font->Valid()) {
            ff.SetInvalid();
            return nullptr;
        }
        mFonts[i].SetFont(font);
    }
    return font.get();
}

void
gfxFontGroup::FamilyFace::CheckState(bool& aSkipDrawing)
{
    gfxFontEntry* fe = FontEntry();
    if (fe->mIsUserFontContainer) {
        gfxUserFontEntry* ufe = static_cast<gfxUserFontEntry*>(fe);
        gfxUserFontEntry::UserFontLoadState state = ufe->LoadState();
        switch (state) {
            case gfxUserFontEntry::STATUS_LOADING:
                SetLoading(true);
                break;
            case gfxUserFontEntry::STATUS_FAILED:
                SetInvalid();
                // fall-thru to the default case
                MOZ_FALLTHROUGH;
            default:
                SetLoading(false);
        }
        if (ufe->WaitForUserFont()) {
            aSkipDrawing = true;
        }
    }
}

bool
gfxFontGroup::FamilyFace::EqualsUserFont(const gfxUserFontEntry* aUserFont) const
{
    gfxFontEntry* fe = FontEntry();
    // if there's a font, the entry is the underlying platform font
    if (mFontCreated) {
        gfxFontEntry* pfe = aUserFont->GetPlatformFontEntry();
        if (pfe == fe) {
            return true;
        }
    } else if (fe == aUserFont) {
        return true;
    }
    return false;
}

bool
gfxFontGroup::FontLoadingForFamily(gfxFontFamily* aFamily, uint32_t aCh) const
{
    uint32_t count = mFonts.Length();
    for (uint32_t i = 0; i < count; ++i) {
        const FamilyFace& ff = mFonts[i];
        if (ff.IsLoading() && ff.Family() == aFamily) {
            const gfxUserFontEntry* ufe =
                static_cast<gfxUserFontEntry*>(ff.FontEntry());
            if (ufe->CharacterInUnicodeRange(aCh)) {
                return true;
            }
        }
    }
    return false;
}

gfxFont*
gfxFontGroup::GetDefaultFont()
{
    if (mDefaultFont) {
        return mDefaultFont.get();
    }

    bool needsBold;
    gfxPlatformFontList *pfl = gfxPlatformFontList::PlatformFontList();
    gfxFontFamily *defaultFamily = pfl->GetDefaultFont(&mStyle);
    NS_ASSERTION(defaultFamily,
                 "invalid default font returned by GetDefaultFont");

    if (defaultFamily) {
        gfxFontEntry *fe = defaultFamily->FindFontForStyle(mStyle,
                                                           needsBold);
        if (fe) {
            mDefaultFont = fe->FindOrMakeFont(&mStyle, needsBold);
        }
    }

    uint32_t numInits, loaderState;
    pfl->GetFontlistInitInfo(numInits, loaderState);
    NS_ASSERTION(numInits != 0,
                 "must initialize system fontlist before getting default font!");

    uint32_t numFonts = 0;
    if (!mDefaultFont) {
        // Try for a "font of last resort...."
        // Because an empty font list would be Really Bad for later code
        // that assumes it will be able to get valid metrics for layout,
        // just look for the first usable font and put in the list.
        // (see bug 554544)
        AutoTArray<RefPtr<gfxFontFamily>,200> familyList;
        pfl->GetFontFamilyList(familyList);
        numFonts = familyList.Length();
        for (uint32_t i = 0; i < numFonts; ++i) {
            gfxFontEntry *fe = familyList[i]->FindFontForStyle(mStyle,
                                                               needsBold);
            if (fe) {
                mDefaultFont = fe->FindOrMakeFont(&mStyle, needsBold);
                if (mDefaultFont) {
                    break;
                }
            }
        }
    }

    if (!mDefaultFont) {
        // an empty font list at this point is fatal; we're not going to
        // be able to do even the most basic layout operations

        // annotate crash report with fontlist info
        nsAutoCString fontInitInfo;
        fontInitInfo.AppendPrintf("no fonts - init: %d fonts: %d loader: %d",
                                  numInits, numFonts, loaderState);
#ifdef XP_WIN
        bool dwriteEnabled = gfxWindowsPlatform::GetPlatform()->DWriteEnabled();
        double upTime = (double) GetTickCount();
        fontInitInfo.AppendPrintf(" backend: %s system-uptime: %9.3f sec",
                                  dwriteEnabled ? "directwrite" : "gdi", upTime/1000);
#endif
        gfxCriticalError() << fontInitInfo.get();

        char msg[256]; // CHECK buffer length if revising message below
        nsAutoString familiesString;
        mFamilyList.ToString(familiesString);
        SprintfLiteral(msg, "unable to find a usable font (%.220s)",
                       NS_ConvertUTF16toUTF8(familiesString).get());
        NS_RUNTIMEABORT(msg);
    }

    return mDefaultFont.get();
}

gfxFont*
gfxFontGroup::GetFirstValidFont(uint32_t aCh)
{
    uint32_t count = mFonts.Length();
    for (uint32_t i = 0; i < count; ++i) {
        FamilyFace& ff = mFonts[i];
        if (ff.IsInvalid()) {
            continue;
        }

        // already have a font?
        gfxFont* font = ff.Font();
        if (font) {
            return font;
        }

        // Need to build a font, loading userfont if not loaded. In
        // cases where unicode range might apply, use the character
        // provided.
        if (ff.IsUserFontContainer()) {
            gfxUserFontEntry* ufe =
                static_cast<gfxUserFontEntry*>(mFonts[i].FontEntry());
            bool inRange = ufe->CharacterInUnicodeRange(aCh);
            if (ufe->LoadState() == gfxUserFontEntry::STATUS_NOT_LOADED &&
                inRange && !FontLoadingForFamily(ff.Family(), aCh)) {
                ufe->Load();
                ff.CheckState(mSkipDrawing);
            }
            if (ufe->LoadState() != gfxUserFontEntry::STATUS_LOADED ||
                !inRange) {
                continue;
            }
        }

        font = GetFontAt(i, aCh);
        if (font) {
            return font;
        }
    }
    return GetDefaultFont();
}

gfxFont *
gfxFontGroup::GetFirstMathFont()
{
    uint32_t count = mFonts.Length();
    for (uint32_t i = 0; i < count; ++i) {
        gfxFont* font = GetFontAt(i);
        if (font && font->TryGetMathTable()) {
            return font;
        }
    }
    return nullptr;
}

gfxFontGroup *
gfxFontGroup::Copy(const gfxFontStyle *aStyle)
{
    gfxFontGroup *fg =
        new gfxFontGroup(mFamilyList, aStyle, mTextPerf,
                         mUserFontSet, mDevToCssSize);
    return fg;
}

bool 
gfxFontGroup::IsInvalidChar(uint8_t ch)
{
    return ((ch & 0x7f) < 0x20 || ch == 0x7f);
}

bool 
gfxFontGroup::IsInvalidChar(char16_t ch)
{
    // All printable 7-bit ASCII values are OK
    if (ch >= ' ' && ch < 0x7f) {
        return false;
    }
    // No point in sending non-printing control chars through font shaping
    if (ch <= 0x9f) {
        return true;
    }
    return (((ch & 0xFF00) == 0x2000 /* Unicode control character */ &&
             (ch == 0x200B/*ZWSP*/ || ch == 0x2028/*LSEP*/ || ch == 0x2029/*PSEP*/)) ||
            IsBidiControl(ch));
}

already_AddRefed<gfxTextRun>
gfxFontGroup::MakeEmptyTextRun(const Parameters *aParams, uint32_t aFlags)
{
    aFlags |= TEXT_IS_8BIT | TEXT_IS_ASCII | TEXT_IS_PERSISTENT;
    return gfxTextRun::Create(aParams, 0, this, aFlags);
}

already_AddRefed<gfxTextRun>
gfxFontGroup::MakeSpaceTextRun(const Parameters *aParams, uint32_t aFlags)
{
    aFlags |= TEXT_IS_8BIT | TEXT_IS_ASCII | TEXT_IS_PERSISTENT;

    RefPtr<gfxTextRun> textRun =
        gfxTextRun::Create(aParams, 1, this, aFlags);
    if (!textRun) {
        return nullptr;
    }

    uint16_t orientation = aFlags & TEXT_ORIENT_MASK;
    if (orientation == TEXT_ORIENT_VERTICAL_MIXED) {
        orientation = TEXT_ORIENT_VERTICAL_SIDEWAYS_RIGHT;
    }

    gfxFont *font = GetFirstValidFont();
    if (MOZ_UNLIKELY(GetStyle()->size == 0) ||
        MOZ_UNLIKELY(GetStyle()->sizeAdjust == 0.0f)) {
        // Short-circuit for size-0 fonts, as Windows and ATSUI can't handle
        // them, and always create at least size 1 fonts, i.e. they still
        // render something for size 0 fonts.
        textRun->AddGlyphRun(font, gfxTextRange::kFontGroup, 0, false,
                             orientation);
    }
    else {
        if (font->GetSpaceGlyph()) {
            // Normally, the font has a cached space glyph, so we can avoid
            // the cost of calling FindFontForChar.
            textRun->SetSpaceGlyph(font, aParams->mDrawTarget, 0, orientation);
        } else {
            // In case the primary font doesn't have <space> (bug 970891),
            // find one that does.
            uint8_t matchType;
            RefPtr<gfxFont> spaceFont =
                FindFontForChar(' ', 0, 0, Script::LATIN, nullptr,
                                &matchType);
            if (spaceFont) {
                textRun->SetSpaceGlyph(spaceFont, aParams->mDrawTarget, 0,
                                       orientation);
            }
        }
    }

    // Note that the gfxGlyphExtents glyph bounds storage for the font will
    // always contain an entry for the font's space glyph, so we don't have
    // to call FetchGlyphExtents here.
    return textRun.forget();
}

already_AddRefed<gfxTextRun>
gfxFontGroup::MakeBlankTextRun(uint32_t aLength,
                               const Parameters *aParams, uint32_t aFlags)
{
    RefPtr<gfxTextRun> textRun =
        gfxTextRun::Create(aParams, aLength, this, aFlags);
    if (!textRun) {
        return nullptr;
    }

    uint16_t orientation = aFlags & TEXT_ORIENT_MASK;
    if (orientation == TEXT_ORIENT_VERTICAL_MIXED) {
        orientation = TEXT_ORIENT_VERTICAL_UPRIGHT;
    }
    textRun->AddGlyphRun(GetFirstValidFont(), gfxTextRange::kFontGroup, 0, false,
                         orientation);
    return textRun.forget();
}

already_AddRefed<gfxTextRun>
gfxFontGroup::MakeHyphenTextRun(DrawTarget* aDrawTarget,
                                uint32_t aAppUnitsPerDevUnit)
{
    // only use U+2010 if it is supported by the first font in the group;
    // it's better to use ASCII '-' from the primary font than to fall back to
    // U+2010 from some other, possibly poorly-matching face
    static const char16_t hyphen = 0x2010;
    gfxFont *font = GetFirstValidFont(uint32_t(hyphen));
    if (font->HasCharacter(hyphen)) {
        return MakeTextRun(&hyphen, 1, aDrawTarget, aAppUnitsPerDevUnit,
                           gfxFontGroup::TEXT_IS_PERSISTENT, nullptr);
    }

    static const uint8_t dash = '-';
    return MakeTextRun(&dash, 1, aDrawTarget, aAppUnitsPerDevUnit,
                       gfxFontGroup::TEXT_IS_PERSISTENT, nullptr);
}

gfxFloat
gfxFontGroup::GetHyphenWidth(gfxTextRun::PropertyProvider *aProvider)
{
    if (mHyphenWidth < 0) {
        RefPtr<DrawTarget> dt(aProvider->GetDrawTarget());
        if (dt) {
            RefPtr<gfxTextRun>
                hyphRun(MakeHyphenTextRun(dt,
                                          aProvider->GetAppUnitsPerDevUnit()));
            mHyphenWidth = hyphRun.get() ? hyphRun->GetAdvanceWidth() : 0;
        }
    }
    return mHyphenWidth;
}

already_AddRefed<gfxTextRun>
gfxFontGroup::MakeTextRun(const uint8_t *aString, uint32_t aLength,
                          const Parameters *aParams, uint32_t aFlags,
                          gfxMissingFontRecorder *aMFR)
{
    if (aLength == 0) {
        return MakeEmptyTextRun(aParams, aFlags);
    }
    if (aLength == 1 && aString[0] == ' ') {
        return MakeSpaceTextRun(aParams, aFlags);
    }

    aFlags |= TEXT_IS_8BIT;

    if (MOZ_UNLIKELY(GetStyle()->size == 0) ||
        MOZ_UNLIKELY(GetStyle()->sizeAdjust == 0.0f)) {
        // Short-circuit for size-0 fonts, as Windows and ATSUI can't handle
        // them, and always create at least size 1 fonts, i.e. they still
        // render something for size 0 fonts.
        return MakeBlankTextRun(aLength, aParams, aFlags);
    }

    RefPtr<gfxTextRun> textRun = gfxTextRun::Create(aParams, aLength, this,
                                                    aFlags);
    if (!textRun) {
        return nullptr;
    }

    InitTextRun(aParams->mDrawTarget, textRun.get(), aString, aLength, aMFR);

    textRun->FetchGlyphExtents(aParams->mDrawTarget);

    return textRun.forget();
}

already_AddRefed<gfxTextRun>
gfxFontGroup::MakeTextRun(const char16_t *aString, uint32_t aLength,
                          const Parameters *aParams, uint32_t aFlags,
                          gfxMissingFontRecorder *aMFR)
{
    if (aLength == 0) {
        return MakeEmptyTextRun(aParams, aFlags);
    }
    if (aLength == 1 && aString[0] == ' ') {
        return MakeSpaceTextRun(aParams, aFlags);
    }
    if (MOZ_UNLIKELY(GetStyle()->size == 0) ||
        MOZ_UNLIKELY(GetStyle()->sizeAdjust == 0.0f)) {
        return MakeBlankTextRun(aLength, aParams, aFlags);
    }

    RefPtr<gfxTextRun> textRun = gfxTextRun::Create(aParams, aLength, this,
                                                    aFlags);
    if (!textRun) {
        return nullptr;
    }

    InitTextRun(aParams->mDrawTarget, textRun.get(), aString, aLength, aMFR);

    textRun->FetchGlyphExtents(aParams->mDrawTarget);

    return textRun.forget();
}

template<typename T>
void
gfxFontGroup::InitTextRun(DrawTarget* aDrawTarget,
                          gfxTextRun *aTextRun,
                          const T *aString,
                          uint32_t aLength,
                          gfxMissingFontRecorder *aMFR)
{
    NS_ASSERTION(aLength > 0, "don't call InitTextRun for a zero-length run");

    // we need to do numeral processing even on 8-bit text,
    // in case we're converting Western to Hindi/Arabic digits
    int32_t numOption = gfxPlatform::GetPlatform()->GetBidiNumeralOption();
    UniquePtr<char16_t[]> transformedString;
    if (numOption != IBMBIDI_NUMERAL_NOMINAL) {
        // scan the string for numerals that may need to be transformed;
        // if we find any, we'll make a local copy here and use that for
        // font matching and glyph generation/shaping
        bool prevIsArabic =
            (aTextRun->GetFlags() & gfxTextRunFactory::TEXT_INCOMING_ARABICCHAR) != 0;
        for (uint32_t i = 0; i < aLength; ++i) {
            char16_t origCh = aString[i];
            char16_t newCh = HandleNumberInChar(origCh, prevIsArabic, numOption);
            if (newCh != origCh) {
                if (!transformedString) {
                    transformedString = MakeUnique<char16_t[]>(aLength);
                    if (sizeof(T) == sizeof(char16_t)) {
                        memcpy(transformedString.get(), aString, i * sizeof(char16_t));
                    } else {
                        for (uint32_t j = 0; j < i; ++j) {
                            transformedString[j] = aString[j];
                        }
                    }
                }
            }
            if (transformedString) {
                transformedString[i] = newCh;
            }
            prevIsArabic = IS_ARABIC_CHAR(newCh);
        }
    }

    LogModule* log = mStyle.systemFont
                   ? gfxPlatform::GetLog(eGfxLog_textrunui)
                   : gfxPlatform::GetLog(eGfxLog_textrun);

    // variant fallback handling may end up passing through this twice
    bool redo;
    do {
        redo = false;

        if (sizeof(T) == sizeof(uint8_t) && !transformedString) {

            if (MOZ_UNLIKELY(MOZ_LOG_TEST(log, LogLevel::Warning))) {
                nsAutoCString lang;
                mStyle.language->ToUTF8String(lang);
                nsAutoString families;
                mFamilyList.ToString(families);
                nsAutoCString str((const char*)aString, aLength);
                MOZ_LOG(log, LogLevel::Warning,\
                       ("(%s) fontgroup: [%s] default: %s lang: %s script: %d "
                        "len %d weight: %d width: %d style: %s size: %6.2f %d-byte "
                        "TEXTRUN [%s] ENDTEXTRUN\n",
                        (mStyle.systemFont ? "textrunui" : "textrun"),
                        NS_ConvertUTF16toUTF8(families).get(),
                        (mFamilyList.GetDefaultFontType() == eFamily_serif ?
                         "serif" :
                         (mFamilyList.GetDefaultFontType() == eFamily_sans_serif ?
                          "sans-serif" : "none")),
                        lang.get(), Script::LATIN, aLength,
                        uint32_t(mStyle.weight), uint32_t(mStyle.stretch),
                        (mStyle.style & NS_FONT_STYLE_ITALIC ? "italic" :
                        (mStyle.style & NS_FONT_STYLE_OBLIQUE ? "oblique" :
                                                                "normal")),
                        mStyle.size,
                        sizeof(T),
                        str.get()));
            }

            // the text is still purely 8-bit; bypass the script-run itemizer
            // and treat it as a single Latin run
            InitScriptRun(aDrawTarget, aTextRun, aString,
                          0, aLength, Script::LATIN, aMFR);
        } else {
            const char16_t *textPtr;
            if (transformedString) {
                textPtr = transformedString.get();
            } else {
                // typecast to avoid compilation error for the 8-bit version,
                // even though this is dead code in that case
                textPtr = reinterpret_cast<const char16_t*>(aString);
            }

            // split into script runs so that script can potentially influence
            // the font matching process below
            gfxScriptItemizer scriptRuns(textPtr, aLength);

            uint32_t runStart = 0, runLimit = aLength;
            Script runScript = Script::LATIN;
            while (scriptRuns.Next(runStart, runLimit, runScript)) {

                if (MOZ_UNLIKELY(MOZ_LOG_TEST(log, LogLevel::Warning))) {
                    nsAutoCString lang;
                    mStyle.language->ToUTF8String(lang);
                    nsAutoString families;
                    mFamilyList.ToString(families);
                    uint32_t runLen = runLimit - runStart;
                    MOZ_LOG(log, LogLevel::Warning,\
                           ("(%s) fontgroup: [%s] default: %s lang: %s script: %d "
                            "len %d weight: %d width: %d style: %s size: %6.2f "
                            "%d-byte TEXTRUN [%s] ENDTEXTRUN\n",
                            (mStyle.systemFont ? "textrunui" : "textrun"),
                            NS_ConvertUTF16toUTF8(families).get(),
                            (mFamilyList.GetDefaultFontType() == eFamily_serif ?
                             "serif" :
                             (mFamilyList.GetDefaultFontType() == eFamily_sans_serif ?
                              "sans-serif" : "none")),
                            lang.get(), runScript, runLen,
                            uint32_t(mStyle.weight), uint32_t(mStyle.stretch),
                            (mStyle.style & NS_FONT_STYLE_ITALIC ? "italic" :
                            (mStyle.style & NS_FONT_STYLE_OBLIQUE ? "oblique" :
                                                                    "normal")),
                            mStyle.size,
                            sizeof(T),
                            NS_ConvertUTF16toUTF8(textPtr + runStart, runLen).get()));
                }

                InitScriptRun(aDrawTarget, aTextRun, textPtr + runStart,
                              runStart, runLimit - runStart, runScript, aMFR);
            }
        }

        // if shaping was aborted due to lack of feature support, clear out
        // glyph runs and redo shaping with fallback forced on
        if (aTextRun->GetShapingState() == gfxTextRun::eShapingState_Aborted) {
            redo = true;
            aTextRun->SetShapingState(
                gfxTextRun::eShapingState_ForceFallbackFeature);
            aTextRun->ClearGlyphsAndCharacters();
        }

    } while (redo);

    if (sizeof(T) == sizeof(char16_t) && aLength > 0) {
        gfxTextRun::CompressedGlyph *glyph = aTextRun->GetCharacterGlyphs();
        if (!glyph->IsSimpleGlyph()) {
            glyph->SetClusterStart(true);
        }
    }

    // It's possible for CoreText to omit glyph runs if it decides they contain
    // only invisibles (e.g., U+FEFF, see reftest 474417-1). In this case, we
    // need to eliminate them from the glyph run array to avoid drawing "partial
    // ligatures" with the wrong font.
    // We don't do this during InitScriptRun (or gfxFont::InitTextRun) because
    // it will iterate back over all glyphruns in the textrun, which leads to
    // pathologically-bad perf in the case where a textrun contains many script
    // changes (see bug 680402) - we'd end up re-sanitizing all the earlier runs
    // every time a new script subrun is processed.
    aTextRun->SanitizeGlyphRuns();

    aTextRun->SortGlyphRuns();
}

static inline bool
IsPUA(uint32_t aUSV)
{
    // We could look up the General Category of the codepoint here,
    // but it's simpler to check PUA codepoint ranges.
    return (aUSV >= 0xE000 && aUSV <= 0xF8FF) || (aUSV >= 0xF0000);
}

template<typename T>
void
gfxFontGroup::InitScriptRun(DrawTarget* aDrawTarget,
                            gfxTextRun *aTextRun,
                            const T *aString, // text for this script run,
                                              // not the entire textrun
                            uint32_t aOffset, // position of the script run
                                              // within the textrun
                            uint32_t aLength, // length of the script run
                            Script aRunScript,
                            gfxMissingFontRecorder *aMFR)
{
    NS_ASSERTION(aLength > 0, "don't call InitScriptRun for a 0-length run");
    NS_ASSERTION(aTextRun->GetShapingState() != gfxTextRun::eShapingState_Aborted,
                 "don't call InitScriptRun with aborted shaping state");

    // confirm the load state of userfonts in the list
    if (!mSkipUpdateUserFonts && mUserFontSet &&
        mCurrGeneration != mUserFontSet->GetGeneration()) {
        UpdateUserFonts();
    }

    gfxFont *mainFont = GetFirstValidFont();

    uint32_t runStart = 0;
    AutoTArray<gfxTextRange,3> fontRanges;
    ComputeRanges(fontRanges, aString, aLength, aRunScript,
                  aTextRun->GetFlags() & gfxTextRunFactory::TEXT_ORIENT_MASK);
    uint32_t numRanges = fontRanges.Length();
    bool missingChars = false;

    for (uint32_t r = 0; r < numRanges; r++) {
        const gfxTextRange& range = fontRanges[r];
        uint32_t matchedLength = range.Length();
        gfxFont *matchedFont = range.font;
        bool vertical =
            range.orientation == gfxTextRunFactory::TEXT_ORIENT_VERTICAL_UPRIGHT;
        // create the glyph run for this range
        if (matchedFont && mStyle.noFallbackVariantFeatures) {
            // common case - just do glyph layout and record the
            // resulting positioned glyphs
            aTextRun->AddGlyphRun(matchedFont, range.matchType,
                                  aOffset + runStart, (matchedLength > 0),
                                  range.orientation);
            if (!matchedFont->SplitAndInitTextRun(aDrawTarget, aTextRun,
                                                  aString + runStart,
                                                  aOffset + runStart,
                                                  matchedLength,
                                                  aRunScript,
                                                  vertical)) {
                // glyph layout failed! treat as missing glyphs
                matchedFont = nullptr;
            }
        } else if (matchedFont) {
            // shape with some variant feature that requires fallback handling
            bool petiteToSmallCaps = false;
            bool syntheticLower = false;
            bool syntheticUpper = false;

            if (mStyle.variantSubSuper != NS_FONT_VARIANT_POSITION_NORMAL &&
                (aTextRun->GetShapingState() ==
                     gfxTextRun::eShapingState_ForceFallbackFeature ||
                 !matchedFont->SupportsSubSuperscript(mStyle.variantSubSuper,
                                                      aString, aLength,
                                                      aRunScript)))
            {
                // fallback for subscript/superscript variant glyphs

                // if the feature was already used, abort and force
                // fallback across the entire textrun
                gfxTextRun::ShapingState ss = aTextRun->GetShapingState();

                if (ss == gfxTextRun::eShapingState_Normal) {
                    aTextRun->SetShapingState(gfxTextRun::eShapingState_ShapingWithFallback);
                } else if (ss == gfxTextRun::eShapingState_ShapingWithFeature) {
                    aTextRun->SetShapingState(gfxTextRun::eShapingState_Aborted);
                    return;
                }

                RefPtr<gfxFont> subSuperFont =
                    matchedFont->GetSubSuperscriptFont(aTextRun->GetAppUnitsPerDevUnit());
                aTextRun->AddGlyphRun(subSuperFont, range.matchType,
                                      aOffset + runStart, (matchedLength > 0),
                                      range.orientation);
                if (!subSuperFont->SplitAndInitTextRun(aDrawTarget, aTextRun,
                                                       aString + runStart,
                                                       aOffset + runStart,
                                                       matchedLength,
                                                       aRunScript,
                                                       vertical)) {
                    // glyph layout failed! treat as missing glyphs
                    matchedFont = nullptr;
                }
            } else if (mStyle.variantCaps != NS_FONT_VARIANT_CAPS_NORMAL &&
                       !matchedFont->SupportsVariantCaps(aRunScript,
                                                         mStyle.variantCaps,
                                                         petiteToSmallCaps,
                                                         syntheticLower,
                                                         syntheticUpper))
            {
                // fallback for small-caps variant glyphs
                if (!matchedFont->InitFakeSmallCapsRun(aDrawTarget, aTextRun,
                                                       aString + runStart,
                                                       aOffset + runStart,
                                                       matchedLength,
                                                       range.matchType,
                                                       range.orientation,
                                                       aRunScript,
                                                       syntheticLower,
                                                       syntheticUpper)) {
                    matchedFont = nullptr;
                }
            } else {
                // shape normally with variant feature enabled
                gfxTextRun::ShapingState ss = aTextRun->GetShapingState();

                // adjust the shaping state if necessary
                if (ss == gfxTextRun::eShapingState_Normal) {
                    aTextRun->SetShapingState(gfxTextRun::eShapingState_ShapingWithFeature);
                } else if (ss == gfxTextRun::eShapingState_ShapingWithFallback) {
                    // already have shaping results using fallback, need to redo
                    aTextRun->SetShapingState(gfxTextRun::eShapingState_Aborted);
                    return;
                }

                // do glyph layout and record the resulting positioned glyphs
                aTextRun->AddGlyphRun(matchedFont, range.matchType,
                                      aOffset + runStart, (matchedLength > 0),
                                      range.orientation);
                if (!matchedFont->SplitAndInitTextRun(aDrawTarget, aTextRun,
                                                      aString + runStart,
                                                      aOffset + runStart,
                                                      matchedLength,
                                                      aRunScript,
                                                      vertical)) {
                    // glyph layout failed! treat as missing glyphs
                    matchedFont = nullptr;
                }
            }
        } else {
            aTextRun->AddGlyphRun(mainFont, gfxTextRange::kFontGroup,
                                  aOffset + runStart, (matchedLength > 0),
                                  range.orientation);
        }

        if (!matchedFont) {
            // We need to set cluster boundaries (and mark spaces) so that
            // surrogate pairs, combining characters, etc behave properly,
            // even if we don't have glyphs for them
            aTextRun->SetupClusterBoundaries(aOffset + runStart, aString + runStart,
                                             matchedLength);

            // various "missing" characters may need special handling,
            // so we check for them here
            uint32_t runLimit = runStart + matchedLength;
            for (uint32_t index = runStart; index < runLimit; index++) {
                T ch = aString[index];

                // tab and newline are not to be displayed as hexboxes,
                // but do need to be recorded in the textrun
                if (ch == '\n') {
                    aTextRun->SetIsNewline(aOffset + index);
                    continue;
                }
                if (ch == '\t') {
                    aTextRun->SetIsTab(aOffset + index);
                    continue;
                }

                // for 16-bit textruns only, check for surrogate pairs and
                // special Unicode spaces; omit these checks in 8-bit runs
                if (sizeof(T) == sizeof(char16_t)) {
                    if (NS_IS_HIGH_SURROGATE(ch) &&
                        index + 1 < aLength &&
                        NS_IS_LOW_SURROGATE(aString[index + 1]))
                    {
                        uint32_t usv =
                            SURROGATE_TO_UCS4(ch, aString[index + 1]);
                        aTextRun->SetMissingGlyph(aOffset + index,
                                                  usv,
                                                  mainFont);
                        index++;
                        if (!mSkipDrawing && !IsPUA(usv)) {
                            missingChars = true;
                        }
                        continue;
                    }

                    // check if this is a known Unicode whitespace character that
                    // we can render using the space glyph with a custom width
                    gfxFloat wid = mainFont->SynthesizeSpaceWidth(ch);
                    if (wid >= 0.0) {
                        nscoord advance =
                            aTextRun->GetAppUnitsPerDevUnit() * floor(wid + 0.5);
                        if (gfxShapedText::CompressedGlyph::IsSimpleAdvance(advance)) {
                            aTextRun->GetCharacterGlyphs()[aOffset + index].
                                SetSimpleGlyph(advance,
                                               mainFont->GetSpaceGlyph());
                        } else {
                            gfxTextRun::DetailedGlyph detailedGlyph;
                            detailedGlyph.mGlyphID = mainFont->GetSpaceGlyph();
                            detailedGlyph.mAdvance = advance;
                            detailedGlyph.mXOffset = detailedGlyph.mYOffset = 0;
                            gfxShapedText::CompressedGlyph g;
                            g.SetComplex(true, true, 1);
                            aTextRun->SetGlyphs(aOffset + index,
                                                g, &detailedGlyph);
                        }
                        continue;
                    }
                }

                if (IsInvalidChar(ch)) {
                    // invalid chars are left as zero-width/invisible
                    continue;
                }

                // record char code so we can draw a box with the Unicode value
                aTextRun->SetMissingGlyph(aOffset + index, ch, mainFont);
                if (!mSkipDrawing && !IsPUA(ch)) {
                    missingChars = true;
                }
            }
        }

        runStart += matchedLength;
    }

    if (aMFR && missingChars) {
        aMFR->RecordScript(aRunScript);
    }
}

gfxTextRun *
gfxFontGroup::GetEllipsisTextRun(int32_t aAppUnitsPerDevPixel, uint32_t aFlags,
                                 LazyReferenceDrawTargetGetter& aRefDrawTargetGetter)
{
    MOZ_ASSERT(!(aFlags & ~TEXT_ORIENT_MASK),
               "flags here should only be used to specify orientation");
    if (mCachedEllipsisTextRun &&
        (mCachedEllipsisTextRun->GetFlags() & TEXT_ORIENT_MASK) == aFlags &&
        mCachedEllipsisTextRun->GetAppUnitsPerDevUnit() == aAppUnitsPerDevPixel) {
        return mCachedEllipsisTextRun.get();
    }

    // Use a Unicode ellipsis if the font supports it,
    // otherwise use three ASCII periods as fallback.
    gfxFont* firstFont = GetFirstValidFont(uint32_t(kEllipsisChar[0]));
    nsString ellipsis = firstFont->HasCharacter(kEllipsisChar[0])
        ? nsDependentString(kEllipsisChar,
                            ArrayLength(kEllipsisChar) - 1)
        : nsDependentString(kASCIIPeriodsChar,
                            ArrayLength(kASCIIPeriodsChar) - 1);

    RefPtr<DrawTarget> refDT = aRefDrawTargetGetter.GetRefDrawTarget();
    Parameters params = {
        refDT, nullptr, nullptr, nullptr, 0, aAppUnitsPerDevPixel
    };
    mCachedEllipsisTextRun =
        MakeTextRun(ellipsis.get(), ellipsis.Length(), &params,
                    aFlags | TEXT_IS_PERSISTENT, nullptr);
    if (!mCachedEllipsisTextRun) {
        return nullptr;
    }
    // don't let the presence of a cached ellipsis textrun prolong the
    // fontgroup's life
    mCachedEllipsisTextRun->ReleaseFontGroup();
    return mCachedEllipsisTextRun.get();
}

already_AddRefed<gfxFont>
gfxFontGroup::FindFallbackFaceForChar(gfxFontFamily* aFamily, uint32_t aCh,
                                      Script aRunScript)
{
    GlobalFontMatch data(aCh, aRunScript, &mStyle);
    aFamily->SearchAllFontsForChar(&data);
    gfxFontEntry* fe = data.mBestMatch;
    if (!fe) {
        return nullptr;
    }

    bool needsBold = mStyle.weight >= 600 && !fe->IsBold() &&
                     mStyle.allowSyntheticWeight;
    RefPtr<gfxFont> font = fe->FindOrMakeFont(&mStyle, needsBold);
    return font.forget();
}

gfxFloat
gfxFontGroup::GetUnderlineOffset()
{
    if (mUnderlineOffset == UNDERLINE_OFFSET_NOT_SET) {
        // if the fontlist contains a bad underline font, make the underline
        // offset the min of the first valid font and bad font underline offsets
        uint32_t len = mFonts.Length();
        for (uint32_t i = 0; i < len; i++) {
            FamilyFace& ff = mFonts[i];
            if (!ff.IsUserFontContainer() &&
                !ff.FontEntry()->IsUserFont() &&
                ff.Family() &&
                ff.Family()->IsBadUnderlineFamily()) {
                RefPtr<gfxFont> font = GetFontAt(i);
                if (!font) {
                    continue;
                }
                gfxFloat bad = font->GetMetrics(gfxFont::eHorizontal).
                                         underlineOffset;
                gfxFloat first =
                    GetFirstValidFont()->GetMetrics(gfxFont::eHorizontal).
                                             underlineOffset;
                mUnderlineOffset = std::min(first, bad);
                return mUnderlineOffset;
            }
        }

        // no bad underline fonts, use the first valid font's metric
        mUnderlineOffset = GetFirstValidFont()->
            GetMetrics(gfxFont::eHorizontal).underlineOffset;
    }

    return mUnderlineOffset;
}

already_AddRefed<gfxFont>
gfxFontGroup::FindFontForChar(uint32_t aCh, uint32_t aPrevCh, uint32_t aNextCh,
                              Script aRunScript, gfxFont *aPrevMatchedFont,
                              uint8_t *aMatchType)
{
    // If the char is a cluster extender, we want to use the same font as the
    // preceding character if possible. This is preferable to using the font
    // group because it avoids breaks in shaping within a cluster.
    if (aPrevMatchedFont && IsClusterExtender(aCh) &&
        aPrevMatchedFont->HasCharacter(aCh)) {
        RefPtr<gfxFont> ret = aPrevMatchedFont;
        return ret.forget();
    }

    // Special cases for NNBSP (as used in Mongolian):
    const uint32_t NARROW_NO_BREAK_SPACE = 0x202f;
    if (aCh == NARROW_NO_BREAK_SPACE) {
        // If there is no preceding character, try the font that we'd use
        // for the next char (unless it's just another NNBSP; we don't try
        // to look ahead through a whole run of them).
        if (!aPrevCh && aNextCh && aNextCh != NARROW_NO_BREAK_SPACE) {
            RefPtr<gfxFont> nextFont =
                FindFontForChar(aNextCh, 0, 0, aRunScript, aPrevMatchedFont,
                                aMatchType);
            if (nextFont && nextFont->HasCharacter(aCh)) {
                return nextFont.forget();
            }
        }
        // Otherwise, treat NNBSP like a cluster extender (as above) and try
        // to continue the preceding font run.
        if (aPrevMatchedFont && aPrevMatchedFont->HasCharacter(aCh)) {
            RefPtr<gfxFont> ret = aPrevMatchedFont;
            return ret.forget();
        }
    }

    // To optimize common cases, try the first font in the font-group
    // before going into the more detailed checks below
    uint32_t nextIndex = 0;
    bool isJoinControl = gfxFontUtils::IsJoinControl(aCh);
    bool wasJoinCauser = gfxFontUtils::IsJoinCauser(aPrevCh);
    bool isVarSelector = gfxFontUtils::IsVarSelector(aCh);

    if (!isJoinControl && !wasJoinCauser && !isVarSelector) {
        RefPtr<gfxFont> firstFont = GetFontAt(0, aCh);
        if (firstFont) {
            if (firstFont->HasCharacter(aCh)) {
                *aMatchType = gfxTextRange::kFontGroup;
                return firstFont.forget();
            }

            RefPtr<gfxFont> font;
            if (mFonts[0].CheckForFallbackFaces()) {
                font = FindFallbackFaceForChar(mFonts[0].Family(), aCh,
                                               aRunScript);
            } else if (!firstFont->GetFontEntry()->IsUserFont()) {
                // For platform fonts (but not userfonts), we may need to do
                // fallback within the family to handle cases where some faces
                // such as Italic or Black have reduced character sets compared
                // to the family's Regular face.
                gfxFontEntry* fe = firstFont->GetFontEntry();
                if (!fe->IsUpright() ||
                    fe->Weight() != NS_FONT_WEIGHT_NORMAL ||
                    fe->Stretch() != NS_FONT_STRETCH_NORMAL) {
                    // If style/weight/stretch was not Normal, see if we can
                    // fall back to a next-best face (e.g. Arial Black -> Bold,
                    // or Arial Narrow -> Regular).
                    font = FindFallbackFaceForChar(mFonts[0].Family(), aCh,
                                                   aRunScript);
                }
            }
            if (font) {
                *aMatchType = gfxTextRange::kFontGroup;
                return font.forget();
            }
        }

        // we don't need to check the first font again below
        ++nextIndex;
    }

    if (aPrevMatchedFont) {
        // Don't switch fonts for control characters, regardless of
        // whether they are present in the current font, as they won't
        // actually be rendered (see bug 716229)
        if (isJoinControl ||
            GetGeneralCategory(aCh) == HB_UNICODE_GENERAL_CATEGORY_CONTROL) {
            RefPtr<gfxFont> ret = aPrevMatchedFont;
            return ret.forget();
        }

        // if previous character was a join-causer (ZWJ),
        // use the same font as the previous range if we can
        if (wasJoinCauser) {
            if (aPrevMatchedFont->HasCharacter(aCh)) {
                RefPtr<gfxFont> ret = aPrevMatchedFont;
                return ret.forget();
            }
        }
    }

    // if this character is a variation selector,
    // use the previous font regardless of whether it supports VS or not.
    // otherwise the text run will be divided.
    if (isVarSelector) {
        if (aPrevMatchedFont) {
            RefPtr<gfxFont> ret = aPrevMatchedFont;
            return ret.forget();
        }
        // VS alone. it's meaningless to search different fonts
        return nullptr;
    }

    // 1. check remaining fonts in the font group
    uint32_t fontListLength = mFonts.Length();
    for (uint32_t i = nextIndex; i < fontListLength; i++) {
        FamilyFace& ff = mFonts[i];
        if (ff.IsInvalid() || ff.IsLoading()) {
            continue;
        }

        // if available, use already made gfxFont and check for character
        RefPtr<gfxFont> font = ff.Font();
        if (font) {
            if (font->HasCharacter(aCh)) {
                return font.forget();
            }
            continue;
        }

        // don't have a gfxFont yet, test before building
        gfxFontEntry *fe = ff.FontEntry();
        if (fe->mIsUserFontContainer) {
            // for userfonts, need to test both the unicode range map and
            // the cmap of the platform font entry
            gfxUserFontEntry* ufe = static_cast<gfxUserFontEntry*>(fe);

            // never match a character outside the defined unicode range
            if (!ufe->CharacterInUnicodeRange(aCh)) {
                continue;
            }

            // load if not already loaded but only if no other font in similar
            // range within family is loading
            if (ufe->LoadState() == gfxUserFontEntry::STATUS_NOT_LOADED &&
                !FontLoadingForFamily(ff.Family(), aCh)) {
                ufe->Load();
                ff.CheckState(mSkipDrawing);
            }
            gfxFontEntry* pfe = ufe->GetPlatformFontEntry();
            if (pfe && pfe->HasCharacter(aCh)) {
                font = GetFontAt(i, aCh);
                if (font) {
                    *aMatchType = gfxTextRange::kFontGroup;
                    return font.forget();
                }
            }
        } else if (fe->HasCharacter(aCh)) {
            // for normal platform fonts, after checking the cmap
            // build the font via GetFontAt
            font = GetFontAt(i, aCh);
            if (font) {
                *aMatchType = gfxTextRange::kFontGroup;
                return font.forget();
            }
        }

        // check other family faces if needed
        if (ff.CheckForFallbackFaces()) {
            NS_ASSERTION(i == 0 ? true :
                         !mFonts[i-1].CheckForFallbackFaces() ||
                         !mFonts[i-1].Family()->Name().Equals(ff.Family()->Name()),
                         "should only do fallback once per font family");
            font = FindFallbackFaceForChar(ff.Family(), aCh, aRunScript);
            if (font) {
                *aMatchType = gfxTextRange::kFontGroup;
                return font.forget();
            }
        } else {
            // For platform fonts, but not user fonts, consider intra-family
            // fallback to handle styles with reduced character sets (see
            // also above).
            fe = ff.FontEntry();
            if (!fe->mIsUserFontContainer && !fe->IsUserFont() &&
                (!fe->IsUpright() ||
                 fe->Weight() != NS_FONT_WEIGHT_NORMAL ||
                 fe->Stretch() != NS_FONT_STRETCH_NORMAL)) {
                font = FindFallbackFaceForChar(ff.Family(), aCh, aRunScript);
                if (font) {
                    *aMatchType = gfxTextRange::kFontGroup;
                    return font.forget();
                }
            }
        }
    }

    if (fontListLength == 0) {
        RefPtr<gfxFont> defaultFont = GetDefaultFont();
        if (defaultFont->HasCharacter(aCh)) {
            *aMatchType = gfxTextRange::kFontGroup;
            return defaultFont.forget();
        }
    }

    // if character is in Private Use Area, don't do matching against pref or system fonts
    if ((aCh >= 0xE000  && aCh <= 0xF8FF) || (aCh >= 0xF0000 && aCh <= 0x10FFFD))
        return nullptr;

    // 2. search pref fonts
    RefPtr<gfxFont> font = WhichPrefFontSupportsChar(aCh);
    if (font) {
        *aMatchType = gfxTextRange::kPrefsFallback;
        return font.forget();
    }

    // 3. use fallback fonts
    // -- before searching for something else check the font used for the previous character
    if (aPrevMatchedFont && aPrevMatchedFont->HasCharacter(aCh)) {
        *aMatchType = gfxTextRange::kSystemFallback;
        RefPtr<gfxFont> ret = aPrevMatchedFont;
        return ret.forget();
    }

    // for known "space" characters, don't do a full system-fallback search;
    // we'll synthesize appropriate-width spaces instead of missing-glyph boxes
    if (GetGeneralCategory(aCh) ==
            HB_UNICODE_GENERAL_CATEGORY_SPACE_SEPARATOR &&
        GetFirstValidFont()->SynthesizeSpaceWidth(aCh) >= 0.0)
    {
        return nullptr;
    }

    // -- otherwise look for other stuff
    *aMatchType = gfxTextRange::kSystemFallback;
    font = WhichSystemFontSupportsChar(aCh, aNextCh, aRunScript);
    return font.forget();
}

template<typename T>
void gfxFontGroup::ComputeRanges(nsTArray<gfxTextRange>& aRanges,
                                 const T *aString, uint32_t aLength,
                                 Script aRunScript, uint16_t aOrientation)
{
    NS_ASSERTION(aRanges.Length() == 0, "aRanges must be initially empty");
    NS_ASSERTION(aLength > 0, "don't call ComputeRanges for zero-length text");

    uint32_t prevCh = 0;
    uint32_t nextCh = aString[0];
    if (sizeof(T) == sizeof(char16_t)) {
        if (aLength > 1 && NS_IS_HIGH_SURROGATE(nextCh) &&
                           NS_IS_LOW_SURROGATE(aString[1])) {
            nextCh = SURROGATE_TO_UCS4(nextCh, aString[1]);
        }
    }
    int32_t lastRangeIndex = -1;

    // initialize prevFont to the group's primary font, so that this will be
    // used for string-initial control chars, etc rather than risk hitting font
    // fallback for these (bug 716229)
    gfxFont *prevFont = GetFirstValidFont();

    // if we use the initial value of prevFont, we treat this as a match from
    // the font group; fixes bug 978313
    uint8_t matchType = gfxTextRange::kFontGroup;

    for (uint32_t i = 0; i < aLength; i++) {

        const uint32_t origI = i; // save off in case we increase for surrogate

        // set up current ch
        uint32_t ch = nextCh;

        // Get next char (if any) so that FindFontForChar can look ahead
        // for a possible variation selector.

        if (sizeof(T) == sizeof(char16_t)) {
            // In 16-bit case only, check for surrogate pairs.
            if (ch > 0xffffu) {
                i++;
            }
            if (i < aLength - 1) {
                nextCh = aString[i + 1];
                if ((i + 2 < aLength) && NS_IS_HIGH_SURROGATE(nextCh) &&
                                         NS_IS_LOW_SURROGATE(aString[i + 2])) {
                    nextCh = SURROGATE_TO_UCS4(nextCh, aString[i + 2]);
                }
            } else {
                nextCh = 0;
            }
        } else {
            // 8-bit case is trivial.
            nextCh = i < aLength - 1 ? aString[i + 1] : 0;
        }

        if (ch == 0xa0) {
            ch = ' ';
        }

        // find the font for this char
        RefPtr<gfxFont> font =
            FindFontForChar(ch, prevCh, nextCh, aRunScript, prevFont,
                            &matchType);

#ifndef RELEASE_OR_BETA
        if (MOZ_UNLIKELY(mTextPerf)) {
            if (matchType == gfxTextRange::kPrefsFallback) {
                mTextPerf->current.fallbackPrefs++;
            } else if (matchType == gfxTextRange::kSystemFallback) {
                mTextPerf->current.fallbackSystem++;
            }
        }
#endif

        prevCh = ch;

        uint16_t orient = aOrientation;
        if (aOrientation == gfxTextRunFactory::TEXT_ORIENT_VERTICAL_MIXED) {
            // For CSS text-orientation:mixed, we need to resolve orientation
            // on a per-character basis using the UTR50 orientation property.
            switch (GetVerticalOrientation(ch)) {
            case VERTICAL_ORIENTATION_U:
            case VERTICAL_ORIENTATION_Tr:
            case VERTICAL_ORIENTATION_Tu:
                orient = TEXT_ORIENT_VERTICAL_UPRIGHT;
                break;
            case VERTICAL_ORIENTATION_R:
                orient = TEXT_ORIENT_VERTICAL_SIDEWAYS_RIGHT;
                break;
            }
        }

        if (lastRangeIndex == -1) {
            // first char ==> make a new range
            aRanges.AppendElement(gfxTextRange(0, 1, font, matchType, orient));
            lastRangeIndex++;
            prevFont = font;
        } else {
            // if font or orientation has changed, make a new range...
            // unless ch is a variation selector (bug 1248248)
            gfxTextRange& prevRange = aRanges[lastRangeIndex];
            if (prevRange.font != font || prevRange.matchType != matchType ||
                (prevRange.orientation != orient && !IsClusterExtender(ch))) {
                // close out the previous range
                prevRange.end = origI;
                aRanges.AppendElement(gfxTextRange(origI, i + 1,
                                                   font, matchType, orient));
                lastRangeIndex++;

                // update prevFont for the next match, *unless* we switched
                // fonts on a ZWJ, in which case propagating the changed font
                // is probably not a good idea (see bug 619511)
                if (sizeof(T) == sizeof(uint8_t) ||
                    !gfxFontUtils::IsJoinCauser(ch))
                {
                    prevFont = font;
                }
            }
        }
    }

    aRanges[lastRangeIndex].end = aLength;

#ifndef RELEASE_OR_BETA
    LogModule* log = mStyle.systemFont
                   ? gfxPlatform::GetLog(eGfxLog_textrunui)
                   : gfxPlatform::GetLog(eGfxLog_textrun);

    if (MOZ_UNLIKELY(MOZ_LOG_TEST(log, LogLevel::Debug))) {
        nsAutoCString lang;
        mStyle.language->ToUTF8String(lang);
        nsAutoString families;
        mFamilyList.ToString(families);

        // collect the font matched for each range
        nsAutoCString fontMatches;
        for (size_t i = 0, i_end = aRanges.Length(); i < i_end; i++) {
            const gfxTextRange& r = aRanges[i];
            fontMatches.AppendPrintf(" [%u:%u] %.200s (%s)", r.start, r.end,
                (r.font.get() ?
                 NS_ConvertUTF16toUTF8(r.font->GetName()).get() : "<null>"),
                (r.matchType == gfxTextRange::kFontGroup ?
                 "list" :
                 (r.matchType == gfxTextRange::kPrefsFallback) ?
                  "prefs" : "sys"));
        }
        MOZ_LOG(log, LogLevel::Debug,\
               ("(%s-fontmatching) fontgroup: [%s] default: %s lang: %s script: %d"
                "%s\n",
                (mStyle.systemFont ? "textrunui" : "textrun"),
                NS_ConvertUTF16toUTF8(families).get(),
                (mFamilyList.GetDefaultFontType() == eFamily_serif ?
                 "serif" :
                 (mFamilyList.GetDefaultFontType() == eFamily_sans_serif ?
                  "sans-serif" : "none")),
                lang.get(), aRunScript,
                fontMatches.get()));
    }
#endif
}

gfxUserFontSet*
gfxFontGroup::GetUserFontSet()
{
    return mUserFontSet;
}

void
gfxFontGroup::SetUserFontSet(gfxUserFontSet *aUserFontSet)
{
    if (aUserFontSet == mUserFontSet) {
        return;
    }
    mUserFontSet = aUserFontSet;
    mCurrGeneration = GetGeneration() - 1;
    UpdateUserFonts();
}

uint64_t
gfxFontGroup::GetGeneration()
{
    if (!mUserFontSet)
        return 0;
    return mUserFontSet->GetGeneration();
}

uint64_t
gfxFontGroup::GetRebuildGeneration()
{
    if (!mUserFontSet)
        return 0;
    return mUserFontSet->GetRebuildGeneration();
}

// note: gfxPangoFontGroup overrides UpdateUserFonts, such that
//       BuildFontList is never used
void
gfxFontGroup::UpdateUserFonts()
{
    if (mCurrGeneration < GetRebuildGeneration()) {
        // fonts in userfont set changed, need to redo the fontlist
        mFonts.Clear();
        ClearCachedData();
        BuildFontList();
        mCurrGeneration = GetGeneration();
    } else if (mCurrGeneration != GetGeneration()) {
        // load state change occurred, verify load state and validity of fonts
        ClearCachedData();

        uint32_t len = mFonts.Length();
        for (uint32_t i = 0; i < len; i++) {
            FamilyFace& ff = mFonts[i];
            if (ff.Font() || !ff.IsUserFontContainer()) {
                continue;
            }
            ff.CheckState(mSkipDrawing);
        }

        mCurrGeneration = GetGeneration();
    }
}

bool
gfxFontGroup::ContainsUserFont(const gfxUserFontEntry* aUserFont)
{
    UpdateUserFonts();
    // search through the fonts list for a specific user font
    uint32_t len = mFonts.Length();
    for (uint32_t i = 0; i < len; i++) {
        FamilyFace& ff = mFonts[i];
        if (ff.EqualsUserFont(aUserFont)) {
            return true;
        }
    }
    return false;
}

already_AddRefed<gfxFont>
gfxFontGroup::WhichPrefFontSupportsChar(uint32_t aCh)
{
    RefPtr<gfxFont> font;

    // get the pref font list if it hasn't been set up already
    uint32_t unicodeRange = FindCharUnicodeRange(aCh);
    gfxPlatformFontList* pfl = gfxPlatformFontList::PlatformFontList();
    eFontPrefLang charLang = pfl->GetFontPrefLangFor(unicodeRange);

    // if the last pref font was the first family in the pref list, no need to recheck through a list of families
    if (mLastPrefFont && charLang == mLastPrefLang &&
        mLastPrefFirstFont && mLastPrefFont->HasCharacter(aCh)) {
        font = mLastPrefFont;
        return font.forget();
    }

    // based on char lang and page lang, set up list of pref lang fonts to check
    eFontPrefLang prefLangs[kMaxLenPrefLangList];
    uint32_t i, numLangs = 0;

    pfl->GetLangPrefs(prefLangs, numLangs, charLang, mPageLang);

    for (i = 0; i < numLangs; i++) {
        eFontPrefLang currentLang = prefLangs[i];
        mozilla::FontFamilyType defaultGeneric =
            pfl->GetDefaultGeneric(currentLang);
        nsTArray<RefPtr<gfxFontFamily>>* families =
            pfl->GetPrefFontsLangGroup(defaultGeneric, currentLang);
        NS_ASSERTION(families, "no pref font families found");

        // find the first pref font that includes the character
        uint32_t  j, numPrefs;
        numPrefs = families->Length();
        for (j = 0; j < numPrefs; j++) {
            // look up the appropriate face
            gfxFontFamily *family = (*families)[j];
            if (!family) continue;

            // if a pref font is used, it's likely to be used again in the same text run.
            // the style doesn't change so the face lookup can be cached rather than calling
            // FindOrMakeFont repeatedly.  speeds up FindFontForChar lookup times for subsequent
            // pref font lookups
            if (family == mLastPrefFamily && mLastPrefFont->HasCharacter(aCh)) {
                font = mLastPrefFont;
                return font.forget();
            }

            bool needsBold;
            gfxFontEntry *fe = family->FindFontForStyle(mStyle, needsBold);
            // if ch in cmap, create and return a gfxFont
            if (fe && fe->HasCharacter(aCh)) {
                RefPtr<gfxFont> prefFont = fe->FindOrMakeFont(&mStyle, needsBold);
                if (!prefFont) continue;
                mLastPrefFamily = family;
                mLastPrefFont = prefFont;
                mLastPrefLang = charLang;
                mLastPrefFirstFont = (i == 0 && j == 0);
                return prefFont.forget();
            }

        }
    }

    return nullptr;
}

already_AddRefed<gfxFont>
gfxFontGroup::WhichSystemFontSupportsChar(uint32_t aCh, uint32_t aNextCh,
                                          Script aRunScript)
{
    gfxFontEntry *fe = 
        gfxPlatformFontList::PlatformFontList()->
            SystemFindFontForChar(aCh, aNextCh, aRunScript, &mStyle);
    if (fe) {
        bool wantBold = mStyle.ComputeWeight() >= 6;
        RefPtr<gfxFont> font =
            fe->FindOrMakeFont(&mStyle, wantBold && !fe->IsBold());
        return font.forget();
    }

    return nullptr;
}

/*static*/ void
gfxFontGroup::Shutdown()
{
    NS_IF_RELEASE(gLangService);
}

nsILanguageAtomService* gfxFontGroup::gLangService = nullptr;

void
gfxMissingFontRecorder::Flush()
{
    static bool mNotifiedFontsInitialized = false;
    static uint32_t mNotifiedFonts[gfxMissingFontRecorder::kNumScriptBitsWords];
    if (!mNotifiedFontsInitialized) {
        memset(&mNotifiedFonts, 0, sizeof(mNotifiedFonts));
        mNotifiedFontsInitialized = true;
    }

    nsAutoString fontNeeded;
    for (uint32_t i = 0; i < kNumScriptBitsWords; ++i) {
        mMissingFonts[i] &= ~mNotifiedFonts[i];
        if (!mMissingFonts[i]) {
            continue;
        }
        for (uint32_t j = 0; j < 32; ++j) {
            if (!(mMissingFonts[i] & (1 << j))) {
                continue;
            }
            mNotifiedFonts[i] |= (1 << j);
            if (!fontNeeded.IsEmpty()) {
                fontNeeded.Append(char16_t(','));
            }
            uint32_t sc = i * 32 + j;
            MOZ_ASSERT(sc < static_cast<uint32_t>(Script::NUM_SCRIPT_CODES),
                       "how did we set the bit for an invalid script code?");
            uint32_t tag = GetScriptTagForCode(static_cast<Script>(sc));
            fontNeeded.Append(char16_t(tag >> 24));
            fontNeeded.Append(char16_t((tag >> 16) & 0xff));
            fontNeeded.Append(char16_t((tag >> 8) & 0xff));
            fontNeeded.Append(char16_t(tag & 0xff));
        }
        mMissingFonts[i] = 0;
    }
    if (!fontNeeded.IsEmpty()) {
        nsCOMPtr<nsIObserverService> service = GetObserverService();
        service->NotifyObservers(nullptr, "font-needed", fontNeeded.get());
    }
}
