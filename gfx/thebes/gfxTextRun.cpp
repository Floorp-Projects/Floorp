/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxTextRun.h"
#include "gfxGlyphExtents.h"
#include "gfxPlatformFontList.h"
#include "gfxUserFontSet.h"
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

#include "cairo.h"

using namespace mozilla;
using namespace mozilla::unicode;

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
    void *storage = moz_malloc(aSize + aLength * sizeof(CompressedGlyph));
    if (!storage) {
        NS_WARNING("failed to allocate storage for text run!");
        return nullptr;
    }

    // Initialize the glyph storage (beyond aSize) to zero
    memset(reinterpret_cast<char*>(storage) + aSize, 0,
           aLength * sizeof(CompressedGlyph));

    return storage;
}

gfxTextRun *
gfxTextRun::Create(const gfxTextRunFactory::Parameters *aParams,
                   uint32_t aLength, gfxFontGroup *aFontGroup, uint32_t aFlags)
{
    void *storage = AllocateStorageForTextRun(sizeof(gfxTextRun), aLength);
    if (!storage) {
        return nullptr;
    }

    return new (storage) gfxTextRun(aParams, aLength, aFontGroup, aFlags);
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

#ifndef RELEASE_BUILD
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
#ifndef RELEASE_BUILD
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
gfxTextRun::SetPotentialLineBreaks(uint32_t aStart, uint32_t aLength,
                                   uint8_t *aBreakBefore,
                                   gfxContext *aRefContext)
{
    NS_ASSERTION(aStart + aLength <= GetLength(), "Overflow");

    uint32_t changed = 0;
    uint32_t i;
    CompressedGlyph *charGlyphs = mCharacterGlyphs + aStart;
    for (i = 0; i < aLength; ++i) {
        uint8_t canBreak = aBreakBefore[i];
        if (canBreak && !charGlyphs[i].IsClusterStart()) {
            // This can happen ... there is no guarantee that our linebreaking rules
            // align with the platform's idea of what constitutes a cluster.
            NS_WARNING("Break suggested inside cluster!");
            canBreak = CompressedGlyph::FLAG_BREAK_TYPE_NONE;
        }
        changed |= charGlyphs[i].SetCanBreakBefore(canBreak);
    }
    return changed != 0;
}

gfxTextRun::LigatureData
gfxTextRun::ComputeLigatureData(uint32_t aPartStart, uint32_t aPartEnd,
                                PropertyProvider *aProvider)
{
    NS_ASSERTION(aPartStart < aPartEnd, "Computing ligature data for empty range");
    NS_ASSERTION(aPartEnd <= GetLength(), "Character length overflow");
  
    LigatureData result;
    CompressedGlyph *charGlyphs = mCharacterGlyphs;

    uint32_t i;
    for (i = aPartStart; !charGlyphs[i].IsLigatureGroupStart(); --i) {
        NS_ASSERTION(i > 0, "Ligature at the start of the run??");
    }
    result.mLigatureStart = i;
    for (i = aPartStart + 1; i < GetLength() && !charGlyphs[i].IsLigatureGroupStart(); ++i) {
    }
    result.mLigatureEnd = i;

    int32_t ligatureWidth =
        GetAdvanceForGlyphs(result.mLigatureStart, result.mLigatureEnd);
    // Count the number of started clusters we have seen
    uint32_t totalClusterCount = 0;
    uint32_t partClusterIndex = 0;
    uint32_t partClusterCount = 0;
    for (i = result.mLigatureStart; i < result.mLigatureEnd; ++i) {
        // Treat the first character of the ligature as the start of a
        // cluster for our purposes of allocating ligature width to its
        // characters.
        if (i == result.mLigatureStart || charGlyphs[i].IsClusterStart()) {
            ++totalClusterCount;
            if (i < aPartStart) {
                ++partClusterIndex;
            } else if (i < aPartEnd) {
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
    if (aPartEnd == result.mLigatureEnd) {
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
        if (aPartStart == result.mLigatureStart) {
            aProvider->GetSpacing(aPartStart, 1, &spacing);
            result.mPartWidth += spacing.mBefore;
        }
        if (aPartEnd == result.mLigatureEnd) {
            aProvider->GetSpacing(aPartEnd - 1, 1, &spacing);
            result.mPartWidth += spacing.mAfter;
        }
    }

    return result;
}

gfxFloat
gfxTextRun::ComputePartialLigatureWidth(uint32_t aPartStart, uint32_t aPartEnd,
                                        PropertyProvider *aProvider)
{
    if (aPartStart >= aPartEnd)
        return 0;
    LigatureData data = ComputeLigatureData(aPartStart, aPartEnd, aProvider);
    return data.mPartWidth;
}

int32_t
gfxTextRun::GetAdvanceForGlyphs(uint32_t aStart, uint32_t aEnd)
{
    const CompressedGlyph *glyphData = mCharacterGlyphs + aStart;
    int32_t advance = 0;
    uint32_t i;
    for (i = aStart; i < aEnd; ++i, ++glyphData) {
        if (glyphData->IsSimpleGlyph()) {
            advance += glyphData->GetSimpleAdvance();   
        } else {
            uint32_t glyphCount = glyphData->GetGlyphCount();
            if (glyphCount == 0) {
                continue;
            }
            const DetailedGlyph *details = GetDetailedGlyphs(i);
            if (details) {
                uint32_t j;
                for (j = 0; j < glyphCount; ++j, ++details) {
                    advance += details->mAdvance;
                }
            }
        }
    }
    return advance;
}

static void
GetAdjustedSpacing(gfxTextRun *aTextRun, uint32_t aStart, uint32_t aEnd,
                   gfxTextRun::PropertyProvider *aProvider,
                   gfxTextRun::PropertyProvider::Spacing *aSpacing)
{
    if (aStart >= aEnd)
        return;

    aProvider->GetSpacing(aStart, aEnd - aStart, aSpacing);

#ifdef DEBUG
    // Check to see if we have spacing inside ligatures

    const gfxTextRun::CompressedGlyph *charGlyphs = aTextRun->GetCharacterGlyphs();
    uint32_t i;

    for (i = aStart; i < aEnd; ++i) {
        if (!charGlyphs[i].IsLigatureGroupStart()) {
            NS_ASSERTION(i == aStart || aSpacing[i - aStart].mBefore == 0,
                         "Before-spacing inside a ligature!");
            NS_ASSERTION(i - 1 <= aStart || aSpacing[i - 1 - aStart].mAfter == 0,
                         "After-spacing inside a ligature!");
        }
    }
#endif
}

bool
gfxTextRun::GetAdjustedSpacingArray(uint32_t aStart, uint32_t aEnd,
                                    PropertyProvider *aProvider,
                                    uint32_t aSpacingStart, uint32_t aSpacingEnd,
                                    nsTArray<PropertyProvider::Spacing> *aSpacing)
{
    if (!aProvider || !(mFlags & gfxTextRunFactory::TEXT_ENABLE_SPACING))
        return false;
    if (!aSpacing->AppendElements(aEnd - aStart))
        return false;
    memset(aSpacing->Elements(), 0, sizeof(gfxFont::Spacing)*(aSpacingStart - aStart));
    GetAdjustedSpacing(this, aSpacingStart, aSpacingEnd, aProvider,
                       aSpacing->Elements() + aSpacingStart - aStart);
    memset(aSpacing->Elements() + aSpacingEnd - aStart, 0, sizeof(gfxFont::Spacing)*(aEnd - aSpacingEnd));
    return true;
}

void
gfxTextRun::ShrinkToLigatureBoundaries(uint32_t *aStart, uint32_t *aEnd)
{
    if (*aStart >= *aEnd)
        return;
  
    CompressedGlyph *charGlyphs = mCharacterGlyphs;

    while (*aStart < *aEnd && !charGlyphs[*aStart].IsLigatureGroupStart()) {
        ++(*aStart);
    }
    if (*aEnd < GetLength()) {
        while (*aEnd > *aStart && !charGlyphs[*aEnd].IsLigatureGroupStart()) {
            --(*aEnd);
        }
    }
}

void
gfxTextRun::DrawGlyphs(gfxFont *aFont, uint32_t aStart, uint32_t aEnd,
                       gfxPoint *aPt, PropertyProvider *aProvider,
                       uint32_t aSpacingStart, uint32_t aSpacingEnd,
                       TextRunDrawParams& aParams)
{
    nsAutoTArray<PropertyProvider::Spacing,200> spacingBuffer;
    bool haveSpacing = GetAdjustedSpacingArray(aStart, aEnd, aProvider,
        aSpacingStart, aSpacingEnd, &spacingBuffer);
    aParams.spacing = haveSpacing ? spacingBuffer.Elements() : nullptr;
    aFont->Draw(this, aStart, aEnd, aPt, aParams);
}

static void
ClipPartialLigature(gfxTextRun *aTextRun, gfxFloat *aLeft, gfxFloat *aRight,
                    gfxFloat aXOrigin, gfxTextRun::LigatureData *aLigature)
{
    if (aLigature->mClipBeforePart) {
        if (aTextRun->IsRightToLeft()) {
            *aRight = std::min(*aRight, aXOrigin);
        } else {
            *aLeft = std::max(*aLeft, aXOrigin);
        }
    }
    if (aLigature->mClipAfterPart) {
        gfxFloat endEdge = aXOrigin + aTextRun->GetDirection()*aLigature->mPartWidth;
        if (aTextRun->IsRightToLeft()) {
            *aLeft = std::max(*aLeft, endEdge);
        } else {
            *aRight = std::min(*aRight, endEdge);
        }
    }    
}

void
gfxTextRun::DrawPartialLigature(gfxFont *aFont, uint32_t aStart, uint32_t aEnd,
                                gfxPoint *aPt, PropertyProvider *aProvider,
                                TextRunDrawParams& aParams)
{
    if (aStart >= aEnd)
        return;

    // Draw partial ligature. We hack this by clipping the ligature.
    LigatureData data = ComputeLigatureData(aStart, aEnd, aProvider);
    gfxRect clipExtents = aParams.context->GetClipExtents();
    gfxFloat left = clipExtents.X() * mAppUnitsPerDevUnit;
    gfxFloat right = clipExtents.XMost() * mAppUnitsPerDevUnit;
    ClipPartialLigature(this, &left, &right, aPt->x, &data);

    {
      // Need to preserve the path, otherwise this can break canvas text-on-path;
      // in general it seems like a good thing, as naive callers probably won't
      // expect gfxTextRun::Draw to implicitly destroy the current path.
      gfxContextPathAutoSaveRestore savePath(aParams.context);

      // use division here to ensure that when the rect is aligned on multiples
      // of mAppUnitsPerDevUnit, we clip to true device unit boundaries.
      // Also, make sure we snap the rectangle to device pixels.
      aParams.context->Save();
      aParams.context->NewPath();
      aParams.context->Rectangle(gfxRect(left / mAppUnitsPerDevUnit,
                                         clipExtents.Y(),
                                         (right - left) / mAppUnitsPerDevUnit,
                                         clipExtents.Height()), true);
      aParams.context->Clip();
    }

    gfxPoint pt(aPt->x - aParams.direction * data.mPartAdvance, aPt->y);
    DrawGlyphs(aFont, data.mLigatureStart, data.mLigatureEnd, &pt,
               aProvider, aStart, aEnd, aParams);
    aParams.context->Restore();

    aPt->x += aParams.direction * data.mPartWidth;
}

// returns true if a glyph run is using a font with synthetic bolding enabled, false otherwise
static bool
HasSyntheticBold(gfxTextRun *aRun, uint32_t aStart, uint32_t aLength)
{
    gfxTextRun::GlyphRunIterator iter(aRun, aStart, aLength);
    while (iter.NextRun()) {
        gfxFont *font = iter.GetGlyphRun()->mFont;
        if (font && font->IsSyntheticBold()) {
            return true;
        }
    }

    return false;
}

// returns true if color is non-opaque (i.e. alpha != 1.0) or completely transparent, false otherwise
// if true, color is set on output
static bool
HasNonOpaqueColor(gfxContext *aContext, gfxRGBA& aCurrentColor)
{
    if (aContext->GetDeviceColor(aCurrentColor)) {
        if (aCurrentColor.a < 1.0 && aCurrentColor.a > 0.0) {
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

    void PushSolidColor(const gfxRect& aBounds, const gfxRGBA& aAlphaColor, uint32_t appsPerDevUnit)
    {
        mContext->Save();
        mContext->NewPath();
        mContext->Rectangle(gfxRect(aBounds.X() / appsPerDevUnit,
                    aBounds.Y() / appsPerDevUnit,
                    aBounds.Width() / appsPerDevUnit,
                    aBounds.Height() / appsPerDevUnit), true);
        mContext->Clip();
        mContext->SetColor(gfxRGBA(aAlphaColor.r, aAlphaColor.g, aAlphaColor.b));
        mContext->PushGroup(gfxContentType::COLOR_ALPHA);
        mAlpha = aAlphaColor.a;
    }

    void PopAlpha()
    {
        // pop the text, using the color alpha as the opacity
        mContext->PopGroupToSource();
        mContext->SetOperator(gfxContext::OPERATOR_OVER);
        mContext->Paint(mAlpha);
        mContext->Restore();
    }

    gfxContext *mContext;
    gfxFloat mAlpha;
};

void
gfxTextRun::Draw(gfxContext *aContext, gfxPoint aPt, DrawMode aDrawMode,
                 uint32_t aStart, uint32_t aLength,
                 PropertyProvider *aProvider, gfxFloat *aAdvanceWidth,
                 gfxTextContextPaint *aContextPaint,
                 gfxTextRunDrawCallbacks *aCallbacks)
{
    NS_ASSERTION(aStart + aLength <= GetLength(), "Substring out of range");
    NS_ASSERTION(aDrawMode == DrawMode::GLYPH_PATH ||
                 !(int(aDrawMode) & int(DrawMode::GLYPH_PATH)),
                 "GLYPH_PATH cannot be used with GLYPH_FILL, GLYPH_STROKE or GLYPH_STROKE_UNDERNEATH");
    NS_ASSERTION(aDrawMode == DrawMode::GLYPH_PATH || !aCallbacks,
                 "callback must not be specified unless using GLYPH_PATH");

    bool skipDrawing = mSkipDrawing;
    if (aDrawMode == DrawMode::GLYPH_FILL) {
        gfxRGBA currentColor;
        if (aContext->GetDeviceColor(currentColor) && currentColor.a == 0) {
            skipDrawing = true;
        }
    }

    gfxFloat direction = GetDirection();

    if (skipDrawing) {
        // We don't need to draw anything;
        // but if the caller wants advance width, we need to compute it here
        if (aAdvanceWidth) {
            gfxTextRun::Metrics metrics = MeasureText(aStart, aLength,
                                                      gfxFont::LOOSE_INK_EXTENTS,
                                                      aContext, aProvider);
            *aAdvanceWidth = metrics.mAdvanceWidth * direction;
        }

        // return without drawing
        return;
    }

    // Set up parameters that will be constant across all glyph runs we need
    // to draw, regardless of the font used.
    TextRunDrawParams params;
    params.context = aContext;
    params.devPerApp = 1.0 / double(GetAppUnitsPerDevUnit());
    params.isRTL = IsRightToLeft();
    params.direction = direction;
    params.drawMode = aDrawMode;
    params.callbacks = aCallbacks;
    params.runContextPaint = aContextPaint;
    params.paintSVGGlyphs = !aCallbacks || aCallbacks->mShouldPaintSVGGlyphs;
    params.dt = aContext->GetDrawTarget();

    gfxPoint pt = aPt;

    // synthetic bolding draws glyphs twice ==> colors with opacity won't draw
    // correctly unless first drawn without alpha
    BufferAlphaColor syntheticBoldBuffer(aContext);
    gfxRGBA currentColor;
    bool needToRestore = false;

    if (aDrawMode == DrawMode::GLYPH_FILL &&
        HasNonOpaqueColor(aContext, currentColor) &&
        HasSyntheticBold(this, aStart, aLength)) {
        needToRestore = true;
        // measure text, use the bounding box
        gfxTextRun::Metrics metrics = MeasureText(aStart, aLength,
                                                  gfxFont::LOOSE_INK_EXTENTS,
                                                  aContext, aProvider);
        metrics.mBoundingBox.MoveBy(aPt);
        syntheticBoldBuffer.PushSolidColor(metrics.mBoundingBox, currentColor,
                                           GetAppUnitsPerDevUnit());
    }

    GlyphRunIterator iter(this, aStart, aLength);
    while (iter.NextRun()) {
        gfxFont *font = iter.GetGlyphRun()->mFont;
        uint32_t start = iter.GetStringStart();
        uint32_t end = iter.GetStringEnd();
        uint32_t ligatureRunStart = start;
        uint32_t ligatureRunEnd = end;
        ShrinkToLigatureBoundaries(&ligatureRunStart, &ligatureRunEnd);

        bool drawPartial = aDrawMode == DrawMode::GLYPH_FILL ||
                           (aDrawMode == DrawMode::GLYPH_PATH && aCallbacks);

        if (drawPartial) {
            DrawPartialLigature(font, start, ligatureRunStart, &pt,
                                aProvider, params);
        }

        DrawGlyphs(font, ligatureRunStart, ligatureRunEnd, &pt,
                   aProvider, ligatureRunStart, ligatureRunEnd, params);

        if (drawPartial) {
            DrawPartialLigature(font, ligatureRunEnd, end, &pt,
                                aProvider, params);
        }
    }

    // composite result when synthetic bolding used
    if (needToRestore) {
        syntheticBoldBuffer.PopAlpha();
    }

    if (aAdvanceWidth) {
        *aAdvanceWidth = (pt.x - aPt.x)*direction;
    }
}

void
gfxTextRun::AccumulateMetricsForRun(gfxFont *aFont,
                                    uint32_t aStart, uint32_t aEnd,
                                    gfxFont::BoundingBoxType aBoundingBoxType,
                                    gfxContext *aRefContext,
                                    PropertyProvider *aProvider,
                                    uint32_t aSpacingStart, uint32_t aSpacingEnd,
                                    Metrics *aMetrics)
{
    nsAutoTArray<PropertyProvider::Spacing,200> spacingBuffer;
    bool haveSpacing = GetAdjustedSpacingArray(aStart, aEnd, aProvider,
        aSpacingStart, aSpacingEnd, &spacingBuffer);
    Metrics metrics = aFont->Measure(this, aStart, aEnd, aBoundingBoxType, aRefContext,
                                     haveSpacing ? spacingBuffer.Elements() : nullptr);
    aMetrics->CombineWith(metrics, IsRightToLeft());
}

void
gfxTextRun::AccumulatePartialLigatureMetrics(gfxFont *aFont,
    uint32_t aStart, uint32_t aEnd,
    gfxFont::BoundingBoxType aBoundingBoxType, gfxContext *aRefContext,
    PropertyProvider *aProvider, Metrics *aMetrics)
{
    if (aStart >= aEnd)
        return;

    // Measure partial ligature. We hack this by clipping the metrics in the
    // same way we clip the drawing.
    LigatureData data = ComputeLigatureData(aStart, aEnd, aProvider);

    // First measure the complete ligature
    Metrics metrics;
    AccumulateMetricsForRun(aFont, data.mLigatureStart, data.mLigatureEnd,
                            aBoundingBoxType, aRefContext,
                            aProvider, aStart, aEnd, &metrics);

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
gfxTextRun::MeasureText(uint32_t aStart, uint32_t aLength,
                        gfxFont::BoundingBoxType aBoundingBoxType,
                        gfxContext *aRefContext,
                        PropertyProvider *aProvider)
{
    NS_ASSERTION(aStart + aLength <= GetLength(), "Substring out of range");

    Metrics accumulatedMetrics;
    GlyphRunIterator iter(this, aStart, aLength);
    while (iter.NextRun()) {
        gfxFont *font = iter.GetGlyphRun()->mFont;
        uint32_t start = iter.GetStringStart();
        uint32_t end = iter.GetStringEnd();
        uint32_t ligatureRunStart = start;
        uint32_t ligatureRunEnd = end;
        ShrinkToLigatureBoundaries(&ligatureRunStart, &ligatureRunEnd);

        AccumulatePartialLigatureMetrics(font, start, ligatureRunStart,
            aBoundingBoxType, aRefContext, aProvider, &accumulatedMetrics);

        // XXX This sucks. We have to get glyph extents just so we can detect
        // glyphs outside the font box, even when aBoundingBoxType is LOOSE,
        // even though in almost all cases we could get correct results just
        // by getting some ascent/descent from the font and using our stored
        // advance widths.
        AccumulateMetricsForRun(font,
            ligatureRunStart, ligatureRunEnd, aBoundingBoxType,
            aRefContext, aProvider, ligatureRunStart, ligatureRunEnd,
            &accumulatedMetrics);

        AccumulatePartialLigatureMetrics(font, ligatureRunEnd, end,
            aBoundingBoxType, aRefContext, aProvider, &accumulatedMetrics);
    }

    return accumulatedMetrics;
}

#define MEASUREMENT_BUFFER_SIZE 100

uint32_t
gfxTextRun::BreakAndMeasureText(uint32_t aStart, uint32_t aMaxLength,
                                bool aLineBreakBefore, gfxFloat aWidth,
                                PropertyProvider *aProvider,
                                bool aSuppressInitialBreak,
                                gfxFloat *aTrimWhitespace,
                                Metrics *aMetrics,
                                gfxFont::BoundingBoxType aBoundingBoxType,
                                gfxContext *aRefContext,
                                bool *aUsedHyphenation,
                                uint32_t *aLastBreak,
                                bool aCanWordWrap,
                                gfxBreakPriority *aBreakPriority)
{
    aMaxLength = std::min(aMaxLength, GetLength() - aStart);

    NS_ASSERTION(aStart + aMaxLength <= GetLength(), "Substring out of range");

    uint32_t bufferStart = aStart;
    uint32_t bufferLength = std::min<uint32_t>(aMaxLength, MEASUREMENT_BUFFER_SIZE);
    PropertyProvider::Spacing spacingBuffer[MEASUREMENT_BUFFER_SIZE];
    bool haveSpacing = aProvider && (mFlags & gfxTextRunFactory::TEXT_ENABLE_SPACING) != 0;
    if (haveSpacing) {
        GetAdjustedSpacing(this, bufferStart, bufferStart + bufferLength, aProvider,
                           spacingBuffer);
    }
    bool hyphenBuffer[MEASUREMENT_BUFFER_SIZE];
    bool haveHyphenation = aProvider &&
        (aProvider->GetHyphensOption() == NS_STYLE_HYPHENS_AUTO ||
         (aProvider->GetHyphensOption() == NS_STYLE_HYPHENS_MANUAL &&
          (mFlags & gfxTextRunFactory::TEXT_ENABLE_HYPHEN_BREAKS) != 0));
    if (haveHyphenation) {
        aProvider->GetHyphenationBreaks(bufferStart, bufferLength,
                                        hyphenBuffer);
    }

    gfxFloat width = 0;
    gfxFloat advance = 0;
    // The number of space characters that can be trimmed
    uint32_t trimmableChars = 0;
    // The amount of space removed by ignoring trimmableChars
    gfxFloat trimmableAdvance = 0;
    int32_t lastBreak = -1;
    int32_t lastBreakTrimmableChars = -1;
    gfxFloat lastBreakTrimmableAdvance = -1;
    bool aborted = false;
    uint32_t end = aStart + aMaxLength;
    bool lastBreakUsedHyphenation = false;

    uint32_t ligatureRunStart = aStart;
    uint32_t ligatureRunEnd = end;
    ShrinkToLigatureBoundaries(&ligatureRunStart, &ligatureRunEnd);

    uint32_t i;
    for (i = aStart; i < end; ++i) {
        if (i >= bufferStart + bufferLength) {
            // Fetch more spacing and hyphenation data
            bufferStart = i;
            bufferLength = std::min(aStart + aMaxLength, i + MEASUREMENT_BUFFER_SIZE) - i;
            if (haveSpacing) {
                GetAdjustedSpacing(this, bufferStart, bufferStart + bufferLength, aProvider,
                                   spacingBuffer);
            }
            if (haveHyphenation) {
                aProvider->GetHyphenationBreaks(bufferStart, bufferLength,
                                                hyphenBuffer);
            }
        }

        // There can't be a word-wrap break opportunity at the beginning of the
        // line: if the width is too small for even one character to fit, it 
        // could be the first and last break opportunity on the line, and that
        // would trigger an infinite loop.
        if (!aSuppressInitialBreak || i > aStart) {
            bool atNaturalBreak = mCharacterGlyphs[i].CanBreakBefore() == 1;
            bool atHyphenationBreak =
                !atNaturalBreak && haveHyphenation && hyphenBuffer[i - bufferStart];
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
        if (i >= ligatureRunStart && i < ligatureRunEnd) {
            charAdvance = GetAdvanceForGlyphs(i, i + 1);
            if (haveSpacing) {
                PropertyProvider::Spacing *space = &spacingBuffer[i - bufferStart];
                charAdvance += space->mBefore + space->mAfter;
            }
        } else {
            charAdvance = ComputePartialLigatureWidth(i, i + 1, aProvider);
        }
        
        advance += charAdvance;
        if (aTrimWhitespace) {
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
        *aMetrics = MeasureText(aStart, charsFit - trimmableChars,
            aBoundingBoxType, aRefContext, aProvider);
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
gfxTextRun::GetAdvanceWidth(uint32_t aStart, uint32_t aLength,
                            PropertyProvider *aProvider)
{
    NS_ASSERTION(aStart + aLength <= GetLength(), "Substring out of range");

    uint32_t ligatureRunStart = aStart;
    uint32_t ligatureRunEnd = aStart + aLength;
    ShrinkToLigatureBoundaries(&ligatureRunStart, &ligatureRunEnd);

    gfxFloat result = ComputePartialLigatureWidth(aStart, ligatureRunStart, aProvider) +
                      ComputePartialLigatureWidth(ligatureRunEnd, aStart + aLength, aProvider);

    // Account for all remaining spacing here. This is more efficient than
    // processing it along with the glyphs.
    if (aProvider && (mFlags & gfxTextRunFactory::TEXT_ENABLE_SPACING)) {
        uint32_t i;
        nsAutoTArray<PropertyProvider::Spacing,200> spacingBuffer;
        if (spacingBuffer.AppendElements(aLength)) {
            GetAdjustedSpacing(this, ligatureRunStart, ligatureRunEnd, aProvider,
                               spacingBuffer.Elements());
            for (i = 0; i < ligatureRunEnd - ligatureRunStart; ++i) {
                PropertyProvider::Spacing *space = &spacingBuffer[i];
                result += space->mBefore + space->mAfter;
            }
        }
    }

    return result + GetAdvanceForGlyphs(ligatureRunStart, ligatureRunEnd);
}

bool
gfxTextRun::SetLineBreaks(uint32_t aStart, uint32_t aLength,
                          bool aLineBreakBefore, bool aLineBreakAfter,
                          gfxFloat *aAdvanceWidthDelta,
                          gfxContext *aRefContext)
{
    // Do nothing because our shaping does not currently take linebreaks into
    // account. There is no change in advance width.
    if (aAdvanceWidthDelta) {
        *aAdvanceWidthDelta = 0;
    }
    return false;
}

uint32_t
gfxTextRun::FindFirstGlyphRunContaining(uint32_t aOffset)
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
gfxTextRun::CountMissingGlyphs()
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
gfxTextRun::CopyGlyphDataFrom(gfxTextRun *aSource, uint32_t aStart,
                              uint32_t aLength, uint32_t aDest)
{
    NS_ASSERTION(aStart + aLength <= aSource->GetLength(),
                 "Source substring out of range");
    NS_ASSERTION(aDest + aLength <= GetLength(),
                 "Destination substring out of range");

    if (aSource->mSkipDrawing) {
        mSkipDrawing = true;
    }

    // Copy base glyph data, and DetailedGlyph data where present
    const CompressedGlyph *srcGlyphs = aSource->mCharacterGlyphs + aStart;
    CompressedGlyph *dstGlyphs = mCharacterGlyphs + aDest;
    for (uint32_t i = 0; i < aLength; ++i) {
        CompressedGlyph g = srcGlyphs[i];
        g.SetCanBreakBefore(!g.IsClusterStart() ?
            CompressedGlyph::FLAG_BREAK_TYPE_NONE :
            dstGlyphs[i].CanBreakBefore());
        if (!g.IsSimpleGlyph()) {
            uint32_t count = g.GetGlyphCount();
            if (count > 0) {
                DetailedGlyph *dst = AllocateDetailedGlyphs(i + aDest, count);
                if (dst) {
                    DetailedGlyph *src = aSource->GetDetailedGlyphs(i + aStart);
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
    GlyphRunIterator iter(aSource, aStart, aLength);
#ifdef DEBUG
    gfxFont *lastFont = nullptr;
#endif
    while (iter.NextRun()) {
        gfxFont *font = iter.GetGlyphRun()->mFont;
        NS_ASSERTION(font != lastFont, "Glyphruns not coalesced?");
#ifdef DEBUG
        lastFont = font;
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
        NS_WARN_IF_FALSE(aSource->IsClusterStart(start),
                         "Started font run in the middle of a cluster");
        NS_WARN_IF_FALSE(end == aSource->GetLength() || aSource->IsClusterStart(end),
                         "Ended font run in the middle of a cluster");

        nsresult rv = AddGlyphRun(font, iter.GetGlyphRun()->mMatchType,
                                  start - aStart + aDest, false,
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
gfxTextRun::SetSpaceGlyph(gfxFont *aFont, gfxContext *aContext,
                          uint32_t aCharIndex, uint16_t aOrientation)
{
    if (SetSpaceGlyphIfSimple(aFont, aContext, aCharIndex, ' ',
                              aOrientation)) {
        return;
    }

    aFont->InitWordCache();
    static const uint8_t space = ' ';
    uint32_t flags = gfxTextRunFactory::TEXT_IS_8BIT |
                     gfxTextRunFactory::TEXT_IS_ASCII |
                     gfxTextRunFactory::TEXT_IS_PERSISTENT |
                     aOrientation;
    gfxShapedWord *sw = aFont->GetShapedWord(aContext,
                                             &space, 1,
                                             gfxShapedWord::HashMix(0, ' '), 
                                             MOZ_SCRIPT_LATIN,
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
gfxTextRun::SetSpaceGlyphIfSimple(gfxFont *aFont, gfxContext *aContext,
                                  uint32_t aCharIndex, char16_t aSpaceChar,
                                  uint16_t aOrientation)
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
gfxTextRun::FetchGlyphExtents(gfxContext *aRefContext)
{
    bool needsGlyphExtents = NeedsGlyphExtents(this);
    if (!needsGlyphExtents && !mDetailedGlyphs)
        return;

    uint32_t i, runCount = mGlyphRuns.Length();
    CompressedGlyph *charGlyphs = mCharacterGlyphs;
    for (i = 0; i < runCount; ++i) {
        const GlyphRun& run = mGlyphRuns[i];
        gfxFont *font = run.mFont;
        if (MOZ_UNLIKELY(font->GetStyle()->size == 0)) {
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
            gfxFont::Orientation orientation =
                IsVertical() ? gfxFont::eVertical : gfxFont::eHorizontal;
            if (glyphData->IsSimpleGlyph()) {
                // If we're in speed mode, don't set up glyph extents here; we'll
                // just return "optimistic" glyph bounds later
                if (needsGlyphExtents) {
                    uint32_t glyphIndex = glyphData->GetSimpleGlyph();
                    if (!extents->IsGlyphKnown(glyphIndex)) {
                        if (!fontIsSetup) {
                            if (!font->SetupCairoFont(aRefContext)) {
                                NS_WARNING("failed to set up font for glyph extents");
                                break;
                            }
                            fontIsSetup = true;
                        }
#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
                        ++gGlyphExtentsSetupEagerSimple;
#endif
                        font->SetupGlyphExtents(aRefContext, orientation,
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
                            if (!font->SetupCairoFont(aRefContext)) {
                                NS_WARNING("failed to set up font for glyph extents");
                                break;
                            }
                            fontIsSetup = true;
                        }
#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
                        ++gGlyphExtentsSetupEagerTight;
#endif
                        font->SetupGlyphExtents(aRefContext, orientation,
                                                glyphIndex, true, extents);
                    }
                }
            }
        }
    }
}


gfxTextRun::ClusterIterator::ClusterIterator(gfxTextRun *aTextRun)
    : mTextRun(aTextRun), mCurrentChar(uint32_t(-1))
{
}

void
gfxTextRun::ClusterIterator::Reset()
{
    mCurrentChar = uint32_t(-1);
}

bool
gfxTextRun::ClusterIterator::NextCluster()
{
    uint32_t len = mTextRun->GetLength();
    while (++mCurrentChar < len) {
        if (mTextRun->IsClusterStart(mCurrentChar)) {
            return true;
        }
    }

    mCurrentChar = uint32_t(-1);
    return false;
}

uint32_t
gfxTextRun::ClusterIterator::ClusterLength() const
{
    if (mCurrentChar == uint32_t(-1)) {
        return 0;
    }

    uint32_t i = mCurrentChar,
             len = mTextRun->GetLength();
    while (++i < len) {
        if (mTextRun->IsClusterStart(i)) {
            break;
        }
    }

    return i - mCurrentChar;
}

gfxFloat
gfxTextRun::ClusterIterator::ClusterAdvance(PropertyProvider *aProvider) const
{
    if (mCurrentChar == uint32_t(-1)) {
        return 0;
    }

    return mTextRun->GetAdvanceWidth(mCurrentChar, ClusterLength(), aProvider);
}

size_t
gfxTextRun::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf)
{
    // The second arg is how much gfxTextRun::AllocateStorage would have
    // allocated.
    size_t total = mGlyphRuns.SizeOfExcludingThis(aMallocSizeOf);

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
                           gfxUserFontSet *aUserFontSet)
    : mFamilyList(aFontFamilyList)
    , mStyle(*aStyle)
    , mUnderlineOffset(UNDERLINE_OFFSET_NOT_SET)
    , mHyphenWidth(-1)
    , mUserFontSet(aUserFontSet)
    , mTextPerf(nullptr)
    , mPageLang(gfxPlatform::GetFontPrefLangFor(aStyle->language))
    , mSkipDrawing(false)
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
gfxFontGroup::FindGenericFonts(FontFamilyType aGenericType,
                               nsIAtom *aLanguage,
                               void *aClosure)
{
    nsAutoTArray<nsString, 5> resolvedGenerics;
    ResolveGenericFontNames(aGenericType, aLanguage, resolvedGenerics);
    uint32_t g = 0, numGenerics = resolvedGenerics.Length();
    for (g = 0; g < numGenerics; g++) {
        FindPlatformFont(resolvedGenerics[g], false, aClosure);
    }
}

/* static */ void
gfxFontGroup::ResolveGenericFontNames(FontFamilyType aGenericType,
                                      nsIAtom *aLanguage,
                                      nsTArray<nsString>& aGenericFamilies)
{
    static const char kGeneric_serif[] = "serif";
    static const char kGeneric_sans_serif[] = "sans-serif";
    static const char kGeneric_monospace[] = "monospace";
    static const char kGeneric_cursive[] = "cursive";
    static const char kGeneric_fantasy[] = "fantasy";

    // treat -moz-fixed as monospace
    if (aGenericType == eFamily_moz_fixed) {
        aGenericType = eFamily_monospace;
    }

    // type should be standard generic type at this point
    NS_ASSERTION(aGenericType >= eFamily_serif &&
                 aGenericType <= eFamily_fantasy,
                 "standard generic font family type required");

    // create the lang string
    nsIAtom *langGroupAtom = nullptr;
    nsAutoCString langGroupString;
    if (aLanguage) {
        if (!gLangService) {
            CallGetService(NS_LANGUAGEATOMSERVICE_CONTRACTID, &gLangService);
        }
        if (gLangService) {
            nsresult rv;
            langGroupAtom = gLangService->GetLanguageGroup(aLanguage, &rv);
        }
    }
    if (!langGroupAtom) {
        langGroupAtom = nsGkAtoms::Unicode;
    }
    langGroupAtom->ToUTF8String(langGroupString);

    // map generic type to string
    const char *generic = nullptr;
    switch (aGenericType) {
        case eFamily_serif:
            generic = kGeneric_serif;
            break;
        case eFamily_sans_serif:
            generic = kGeneric_sans_serif;
            break;
        case eFamily_monospace:
            generic = kGeneric_monospace;
            break;
        case eFamily_cursive:
            generic = kGeneric_cursive;
            break;
        case eFamily_fantasy:
            generic = kGeneric_fantasy;
            break;
        default:
            break;
    }

    if (!generic) {
        return;
    }

    aGenericFamilies.Clear();

    // load family for "font.name.generic.lang"
    nsAutoCString prefFontName("font.name.");
    prefFontName.Append(generic);
    prefFontName.Append('.');
    prefFontName.Append(langGroupString);
    gfxFontUtils::AppendPrefsFontList(prefFontName.get(),
                                      aGenericFamilies);

    // if lang has pref fonts, also load fonts for "font.name-list.generic.lang"
    if (!aGenericFamilies.IsEmpty()) {
        nsAutoCString prefFontListName("font.name-list.");
        prefFontListName.Append(generic);
        prefFontListName.Append('.');
        prefFontListName.Append(langGroupString);
        gfxFontUtils::AppendPrefsFontList(prefFontListName.get(),
                                          aGenericFamilies);
    }

#if 0  // dump out generic mappings
    printf("%s ===> ", prefFontName.get());
    for (uint32_t k = 0; k < aGenericFamilies.Length(); k++) {
        if (k > 0) printf(", ");
        printf("%s", NS_ConvertUTF16toUTF8(aGenericFamilies[k]).get());
    }
    printf("\n");
#endif
}

void gfxFontGroup::EnumerateFontList(nsIAtom *aLanguage, void *aClosure)
{
    // initialize fonts in the font family list
    const nsTArray<FontFamilyName>& fontlist = mFamilyList.GetFontlist();

    // lookup fonts in the fontlist
    uint32_t i, numFonts = fontlist.Length();
    for (i = 0; i < numFonts; i++) {
        const FontFamilyName& name = fontlist[i];
        if (name.IsNamed()) {
            FindPlatformFont(name.mName, true, aClosure);
        } else {
            FindGenericFonts(name.mType, aLanguage, aClosure);
        }
    }

    // if necessary, append default generic onto the end
    if (mFamilyList.GetDefaultFontType() != eFamily_none &&
        !mFamilyList.HasDefaultGeneric()) {
        FindGenericFonts(mFamilyList.GetDefaultFontType(),
                         aLanguage,
                         aClosure);
    }
}

void
gfxFontGroup::BuildFontList()
{
// gfxPangoFontGroup behaves differently, so this method is a no-op on that platform
#if defined(XP_MACOSX) || defined(XP_WIN) || defined(ANDROID)
    EnumerateFontList(mStyle.language);
#endif
}

void
gfxFontGroup::FindPlatformFont(const nsAString& aName,
                               bool aUseFontSet,
                               void *aClosure)
{
    bool needsBold;
    gfxFontFamily *family = nullptr;
    gfxFontEntry *fe = nullptr;

    if (aUseFontSet) {
        // First, look up in the user font set...
        // If the fontSet matches the family, we must not look for a platform
        // font of the same name, even if we fail to actually get a fontEntry
        // here; we'll fall back to the next name in the CSS font-family list.
        if (mUserFontSet) {
            // add the userfont to the fontlist whether it's already been loaded
            // or not. loading is initiated during font matching.
            family = mUserFontSet->LookupFamily(aName);
            if (family) {
                fe = mUserFontSet->FindUserFontEntry(family, mStyle, needsBold);
            }
        }
    }

    // Not known in the user font set ==> check system fonts
    if (!family) {
        gfxPlatformFontList *fontList = gfxPlatformFontList::PlatformFontList();
        family = fontList->FindFamily(aName, mStyle.systemFont);
        if (family) {
            fe = family->FindFontForStyle(mStyle, needsBold);
        }
    }

    // add to the font group, unless it's already there
    if (fe && !HasFont(fe)) {
        mFonts.AppendElement(FamilyFace(family, fe, needsBold));
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
gfxFontGroup::GetFontAt(int32_t i)
{
    if (uint32_t(i) >= mFonts.Length()) {
        return nullptr;
    }

    FamilyFace& ff = mFonts[i];
    if (ff.IsInvalid() || ff.IsLoading()) {
        return nullptr;
    }

    nsRefPtr<gfxFont> font = ff.Font();
    if (!font) {
        gfxFontEntry *fe = mFonts[i].FontEntry();
        if (fe->mIsUserFontContainer) {
            gfxUserFontEntry* ufe = static_cast<gfxUserFontEntry*>(fe);
            if (ufe->LoadState() == gfxUserFontEntry::STATUS_NOT_LOADED) {
                ufe->Load();
                if (ufe->WaitForUserFont()) {
                    mSkipDrawing = true;
                }
            }
            fe = ufe->GetPlatformFontEntry();
            if (!fe) {
                return nullptr;
            }
        }
        font = fe->FindOrMakeFont(&mStyle, mFonts[i].NeedsBold());
        if (font && !font->Valid()) {
            ff.SetInvalid();
            return nullptr;
        }
        mFonts[i].SetFont(font);
    }
    return font.get();
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

    if (!mDefaultFont) {
        // Try for a "font of last resort...."
        // Because an empty font list would be Really Bad for later code
        // that assumes it will be able to get valid metrics for layout,
        // just look for the first usable font and put in the list.
        // (see bug 554544)
        nsAutoTArray<nsRefPtr<gfxFontFamily>,200> families;
        pfl->GetFontFamilyList(families);
        uint32_t count = families.Length();
        for (uint32_t i = 0; i < count; ++i) {
            gfxFontEntry *fe = families[i]->FindFontForStyle(mStyle,
                                                             needsBold);
            if (fe) {
                mDefaultFont = fe->FindOrMakeFont(&mStyle, needsBold);
            }
        }
    }

    if (!mDefaultFont) {
        // an empty font list at this point is fatal; we're not going to
        // be able to do even the most basic layout operations
        char msg[256]; // CHECK buffer length if revising message below
        nsAutoString families;
        mFamilyList.ToString(families);
        sprintf(msg, "unable to find a usable font (%.220s)",
                NS_ConvertUTF16toUTF8(families).get());
        NS_RUNTIMEABORT(msg);
    }

    return mDefaultFont.get();
}


gfxFont*
gfxFontGroup::GetFirstValidFont()
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

        // need to build a font, loading userfont if not loaded
        if (ff.IsUserFont()) {
            gfxUserFontEntry* ufe =
                static_cast<gfxUserFontEntry*>(mFonts[i].FontEntry());
            if (ufe->LoadState() == gfxUserFontEntry::STATUS_NOT_LOADED) {
                ufe->Load();
                if (ufe->WaitForUserFont()) {
                    mSkipDrawing = true;
                }
            }
            if (ufe->LoadState() != gfxUserFontEntry::STATUS_LOADED) {
                continue;
            }
        }

        font = GetFontAt(i);
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
        if (font && font->GetFontEntry()->TryGetMathTable()) {
            return font;
        }
    }
    return nullptr;
}

gfxFontGroup *
gfxFontGroup::Copy(const gfxFontStyle *aStyle)
{
    gfxFontGroup *fg = new gfxFontGroup(mFamilyList, aStyle, mUserFontSet);
    fg->SetTextPerfMetrics(mTextPerf);
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

gfxTextRun *
gfxFontGroup::MakeEmptyTextRun(const Parameters *aParams, uint32_t aFlags)
{
    aFlags |= TEXT_IS_8BIT | TEXT_IS_ASCII | TEXT_IS_PERSISTENT;
    return gfxTextRun::Create(aParams, 0, this, aFlags);
}

gfxTextRun *
gfxFontGroup::MakeSpaceTextRun(const Parameters *aParams, uint32_t aFlags)
{
    aFlags |= TEXT_IS_8BIT | TEXT_IS_ASCII | TEXT_IS_PERSISTENT;

    gfxTextRun *textRun = gfxTextRun::Create(aParams, 1, this, aFlags);
    if (!textRun) {
        return nullptr;
    }

    uint16_t orientation = aFlags & TEXT_ORIENT_MASK;
    if (orientation == TEXT_ORIENT_VERTICAL_MIXED) {
        orientation = TEXT_ORIENT_VERTICAL_UPRIGHT;
    }

    gfxFont *font = GetFirstValidFont();
    if (MOZ_UNLIKELY(GetStyle()->size == 0)) {
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
            textRun->SetSpaceGlyph(font, aParams->mContext, 0, orientation);
        } else {
            // In case the primary font doesn't have <space> (bug 970891),
            // find one that does.
            uint8_t matchType;
            nsRefPtr<gfxFont> spaceFont =
                FindFontForChar(' ', 0, 0, MOZ_SCRIPT_LATIN, nullptr,
                                &matchType);
            if (spaceFont) {
                textRun->SetSpaceGlyph(spaceFont, aParams->mContext, 0,
                                       orientation);
            }
        }
    }

    // Note that the gfxGlyphExtents glyph bounds storage for the font will
    // always contain an entry for the font's space glyph, so we don't have
    // to call FetchGlyphExtents here.
    return textRun;
}

gfxTextRun *
gfxFontGroup::MakeBlankTextRun(uint32_t aLength,
                               const Parameters *aParams, uint32_t aFlags)
{
    gfxTextRun *textRun =
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
    return textRun;
}

gfxTextRun *
gfxFontGroup::MakeHyphenTextRun(gfxContext *aCtx, uint32_t aAppUnitsPerDevUnit)
{
    // only use U+2010 if it is supported by the first font in the group;
    // it's better to use ASCII '-' from the primary font than to fall back to
    // U+2010 from some other, possibly poorly-matching face
    static const char16_t hyphen = 0x2010;
    gfxFont *font = GetFirstValidFont();
    if (font->HasCharacter(hyphen)) {
        return MakeTextRun(&hyphen, 1, aCtx, aAppUnitsPerDevUnit,
                           gfxFontGroup::TEXT_IS_PERSISTENT);
    }

    static const uint8_t dash = '-';
    return MakeTextRun(&dash, 1, aCtx, aAppUnitsPerDevUnit,
                       gfxFontGroup::TEXT_IS_PERSISTENT);
}

gfxFloat
gfxFontGroup::GetHyphenWidth(gfxTextRun::PropertyProvider *aProvider)
{
    if (mHyphenWidth < 0) {
        nsRefPtr<gfxContext> ctx(aProvider->GetContext());
        if (ctx) {
            nsAutoPtr<gfxTextRun>
                hyphRun(MakeHyphenTextRun(ctx,
                                          aProvider->GetAppUnitsPerDevUnit()));
            mHyphenWidth = hyphRun.get() ?
                hyphRun->GetAdvanceWidth(0, hyphRun->GetLength(), nullptr) : 0;
        }
    }
    return mHyphenWidth;
}

gfxTextRun *
gfxFontGroup::MakeTextRun(const uint8_t *aString, uint32_t aLength,
                          const Parameters *aParams, uint32_t aFlags)
{
    if (aLength == 0) {
        return MakeEmptyTextRun(aParams, aFlags);
    }
    if (aLength == 1 && aString[0] == ' ') {
        return MakeSpaceTextRun(aParams, aFlags);
    }

    aFlags |= TEXT_IS_8BIT;

    if (GetStyle()->size == 0) {
        // Short-circuit for size-0 fonts, as Windows and ATSUI can't handle
        // them, and always create at least size 1 fonts, i.e. they still
        // render something for size 0 fonts.
        return MakeBlankTextRun(aLength, aParams, aFlags);
    }

    gfxTextRun *textRun = gfxTextRun::Create(aParams, aLength,
                                             this, aFlags);
    if (!textRun) {
        return nullptr;
    }

    InitTextRun(aParams->mContext, textRun, aString, aLength);

    textRun->FetchGlyphExtents(aParams->mContext);

    return textRun;
}

gfxTextRun *
gfxFontGroup::MakeTextRun(const char16_t *aString, uint32_t aLength,
                          const Parameters *aParams, uint32_t aFlags)
{
    if (aLength == 0) {
        return MakeEmptyTextRun(aParams, aFlags);
    }
    if (aLength == 1 && aString[0] == ' ') {
        return MakeSpaceTextRun(aParams, aFlags);
    }
    if (GetStyle()->size == 0) {
        return MakeBlankTextRun(aLength, aParams, aFlags);
    }

    gfxTextRun *textRun = gfxTextRun::Create(aParams, aLength,
                                             this, aFlags);
    if (!textRun) {
        return nullptr;
    }

    InitTextRun(aParams->mContext, textRun, aString, aLength);

    textRun->FetchGlyphExtents(aParams->mContext);

    return textRun;
}

template<typename T>
void
gfxFontGroup::InitTextRun(gfxContext *aContext,
                          gfxTextRun *aTextRun,
                          const T *aString,
                          uint32_t aLength)
{
    NS_ASSERTION(aLength > 0, "don't call InitTextRun for a zero-length run");

    // we need to do numeral processing even on 8-bit text,
    // in case we're converting Western to Hindi/Arabic digits
    int32_t numOption = gfxPlatform::GetPlatform()->GetBidiNumeralOption();
    nsAutoArrayPtr<char16_t> transformedString;
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
                    transformedString = new char16_t[aLength];
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

#ifdef PR_LOGGING
    PRLogModuleInfo *log = (mStyle.systemFont ?
                            gfxPlatform::GetLog(eGfxLog_textrunui) :
                            gfxPlatform::GetLog(eGfxLog_textrun));
#endif

    // variant fallback handling may end up passing through this twice
    bool redo;
    do {
        redo = false;

        if (sizeof(T) == sizeof(uint8_t) && !transformedString) {

#ifdef PR_LOGGING
            if (MOZ_UNLIKELY(PR_LOG_TEST(log, PR_LOG_WARNING))) {
                nsAutoCString lang;
                mStyle.language->ToUTF8String(lang);
                nsAutoString families;
                mFamilyList.ToString(families);
                nsAutoCString str((const char*)aString, aLength);
                PR_LOG(log, PR_LOG_WARNING,\
                       ("(%s) fontgroup: [%s] default: %s lang: %s script: %d "
                        "len %d weight: %d width: %d style: %s size: %6.2f %d-byte "
                        "TEXTRUN [%s] ENDTEXTRUN\n",
                        (mStyle.systemFont ? "textrunui" : "textrun"),
                        NS_ConvertUTF16toUTF8(families).get(),
                        (mFamilyList.GetDefaultFontType() == eFamily_serif ?
                         "serif" :
                         (mFamilyList.GetDefaultFontType() == eFamily_sans_serif ?
                          "sans-serif" : "none")),
                        lang.get(), MOZ_SCRIPT_LATIN, aLength,
                        uint32_t(mStyle.weight), uint32_t(mStyle.stretch),
                        (mStyle.style & NS_FONT_STYLE_ITALIC ? "italic" :
                        (mStyle.style & NS_FONT_STYLE_OBLIQUE ? "oblique" :
                                                                "normal")),
                        mStyle.size,
                        sizeof(T),
                        str.get()));
            }
#endif

            // the text is still purely 8-bit; bypass the script-run itemizer
            // and treat it as a single Latin run
            InitScriptRun(aContext, aTextRun, aString,
                          0, aLength, MOZ_SCRIPT_LATIN);
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
            int32_t runScript = MOZ_SCRIPT_LATIN;
            while (scriptRuns.Next(runStart, runLimit, runScript)) {

    #ifdef PR_LOGGING
                if (MOZ_UNLIKELY(PR_LOG_TEST(log, PR_LOG_WARNING))) {
                    nsAutoCString lang;
                    mStyle.language->ToUTF8String(lang);
                    nsAutoString families;
                    mFamilyList.ToString(families);
                    uint32_t runLen = runLimit - runStart;
                    PR_LOG(log, PR_LOG_WARNING,\
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
    #endif

                InitScriptRun(aContext, aTextRun, textPtr + runStart,
                              runStart, runLimit - runStart, runScript);
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

template<typename T>
void
gfxFontGroup::InitScriptRun(gfxContext *aContext,
                            gfxTextRun *aTextRun,
                            const T *aString, // text for this script run,
                                              // not the entire textrun
                            uint32_t aOffset, // position of the script run
                                              // within the textrun
                            uint32_t aLength, // length of the script run
                            int32_t aRunScript)
{
    NS_ASSERTION(aLength > 0, "don't call InitScriptRun for a 0-length run");
    NS_ASSERTION(aTextRun->GetShapingState() != gfxTextRun::eShapingState_Aborted,
                 "don't call InitScriptRun with aborted shaping state");

#if defined(XP_MACOSX) || defined(XP_WIN) || defined(ANDROID)
    // non-linux platforms build the fontlist lazily and include userfonts
    // so need to confirm the load state of userfonts in the list
    if (mUserFontSet && mCurrGeneration != mUserFontSet->GetGeneration()) {
        UpdateUserFonts();
    }
#endif

    gfxFont *mainFont = GetFirstValidFont();

    uint32_t runStart = 0;
    nsAutoTArray<gfxTextRange,3> fontRanges;
    ComputeRanges(fontRanges, aString, aLength, aRunScript,
                  aTextRun->GetFlags() & gfxTextRunFactory::TEXT_ORIENT_MASK);
    uint32_t numRanges = fontRanges.Length();

    for (uint32_t r = 0; r < numRanges; r++) {
        const gfxTextRange& range = fontRanges[r];
        uint32_t matchedLength = range.Length();
        gfxFont *matchedFont = range.font;

        // create the glyph run for this range
        if (matchedFont && mStyle.noFallbackVariantFeatures) {
            // common case - just do glyph layout and record the
            // resulting positioned glyphs
            aTextRun->AddGlyphRun(matchedFont, range.matchType,
                                  aOffset + runStart, (matchedLength > 0),
                                  range.orientation);
            if (!matchedFont->SplitAndInitTextRun(aContext, aTextRun,
                                                  aString + runStart,
                                                  aOffset + runStart,
                                                  matchedLength,
                                                  aRunScript)) {
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

                nsRefPtr<gfxFont> subSuperFont =
                    matchedFont->GetSubSuperscriptFont(aTextRun->GetAppUnitsPerDevUnit());
                aTextRun->AddGlyphRun(subSuperFont, range.matchType,
                                      aOffset + runStart, (matchedLength > 0),
                                      range.orientation);
                if (!subSuperFont->SplitAndInitTextRun(aContext, aTextRun,
                                                       aString + runStart,
                                                       aOffset + runStart,
                                                       matchedLength,
                                                       aRunScript)) {
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
                if (!matchedFont->InitFakeSmallCapsRun(aContext, aTextRun,
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
                if (!matchedFont->SplitAndInitTextRun(aContext, aTextRun,
                                                      aString + runStart,
                                                      aOffset + runStart,
                                                      matchedLength,
                                                      aRunScript)) {
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
                        aTextRun->SetMissingGlyph(aOffset + index,
                                                  SURROGATE_TO_UCS4(ch,
                                                                    aString[index + 1]),
                                                  mainFont);
                        index++;
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
            }
        }

        runStart += matchedLength;
    }
}

gfxTextRun *
gfxFontGroup::GetEllipsisTextRun(int32_t aAppUnitsPerDevPixel,
                                 LazyReferenceContextGetter& aRefContextGetter)
{
    if (mCachedEllipsisTextRun &&
        mCachedEllipsisTextRun->GetAppUnitsPerDevUnit() == aAppUnitsPerDevPixel) {
        return mCachedEllipsisTextRun;
    }

    // Use a Unicode ellipsis if the font supports it,
    // otherwise use three ASCII periods as fallback.
    gfxFont* firstFont = GetFirstValidFont();
    nsString ellipsis = firstFont->HasCharacter(kEllipsisChar[0])
        ? nsDependentString(kEllipsisChar,
                            ArrayLength(kEllipsisChar) - 1)
        : nsDependentString(kASCIIPeriodsChar,
                            ArrayLength(kASCIIPeriodsChar) - 1);

    nsRefPtr<gfxContext> refCtx = aRefContextGetter.GetRefContext();
    Parameters params = {
        refCtx, nullptr, nullptr, nullptr, 0, aAppUnitsPerDevPixel
    };
    gfxTextRun* textRun =
        MakeTextRun(ellipsis.get(), ellipsis.Length(), &params, TEXT_IS_PERSISTENT);
    if (!textRun) {
        return nullptr;
    }
    mCachedEllipsisTextRun = textRun;
    textRun->ReleaseFontGroup(); // don't let the presence of a cached ellipsis
                                 // textrun prolong the fontgroup's life
    return textRun;
}

already_AddRefed<gfxFont>
gfxFontGroup::FindNonItalicFaceForChar(gfxFontFamily* aFamily, uint32_t aCh)
{
    NS_ASSERTION(mStyle.style != NS_FONT_STYLE_NORMAL,
                 "should only be called in the italic/oblique case");

    if (!aFamily->TestCharacterMap(aCh)) {
        return nullptr;
    }

    gfxFontStyle regularStyle = mStyle;
    regularStyle.style = NS_FONT_STYLE_NORMAL;
    bool needsBold;
    gfxFontEntry *fe = aFamily->FindFontForStyle(regularStyle, needsBold);
    NS_ASSERTION(!fe->mIsUserFontContainer,
                 "should only be searching platform fonts");
    if (!fe->HasCharacter(aCh)) {
        return nullptr;
    }

    nsRefPtr<gfxFont> font = fe->FindOrMakeFont(&mStyle, needsBold);
    if (!font->Valid()) {
        return nullptr;
    }
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
            if (!ff.IsUserFont() && ff.Family() &&
                ff.Family()->IsBadUnderlineFamily()) {
                nsRefPtr<gfxFont> font = GetFontAt(i);
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
                              int32_t aRunScript, gfxFont *aPrevMatchedFont,
                              uint8_t *aMatchType)
{
    // To optimize common cases, try the first font in the font-group
    // before going into the more detailed checks below
    uint32_t nextIndex = 0;
    bool isJoinControl = gfxFontUtils::IsJoinControl(aCh);
    bool wasJoinCauser = gfxFontUtils::IsJoinCauser(aPrevCh);
    bool isVarSelector = gfxFontUtils::IsVarSelector(aCh);

    if (!isJoinControl && !wasJoinCauser && !isVarSelector) {
        nsRefPtr<gfxFont> firstFont = GetFontAt(0);
        if (firstFont) {
            if (firstFont->HasCharacter(aCh)) {
                *aMatchType = gfxTextRange::kFontGroup;
                return firstFont.forget();
            }

            // If italic, test the regular face to see if it supports character.
            // Only do this for platform fonts, not userfonts.
            if (mStyle.style != NS_FONT_STYLE_NORMAL &&
                !firstFont->GetFontEntry()->IsUserFont()) {
                nsRefPtr<gfxFont> font =
                    FindNonItalicFaceForChar(mFonts[0].Family(), aCh);
                if (font) {
                    *aMatchType = gfxTextRange::kFontGroup;
                    return font.forget();
                }
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
            nsRefPtr<gfxFont> ret = aPrevMatchedFont;
            return ret.forget();
        }

        // if previous character was a join-causer (ZWJ),
        // use the same font as the previous range if we can
        if (wasJoinCauser) {
            if (aPrevMatchedFont->HasCharacter(aCh)) {
                nsRefPtr<gfxFont> ret = aPrevMatchedFont;
                return ret.forget();
            }
        }
    }

    // if this character is a variation selector,
    // use the previous font regardless of whether it supports VS or not.
    // otherwise the text run will be divided.
    if (isVarSelector) {
        if (aPrevMatchedFont) {
            nsRefPtr<gfxFont> ret = aPrevMatchedFont;
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

        nsRefPtr<gfxFont> font;

        // test the font entry, build font if needed
        gfxFontEntry *fe = ff.FontEntry();

        if (fe->mIsUserFontContainer) {
            // for userfonts, need to test the cmap of the platform font entry
            gfxUserFontEntry* ufe = static_cast<gfxUserFontEntry*>(fe);
            if (ufe->LoadState() == gfxUserFontEntry::STATUS_NOT_LOADED) {
                ufe->Load();
                if (ufe->WaitForUserFont()) {
                    mSkipDrawing = true;
                }
            }
            gfxFontEntry* pfe = ufe->GetPlatformFontEntry();
            if (pfe && pfe->HasCharacter(aCh)) {
                font = GetFontAt(i);
                if (font) {
                    *aMatchType = gfxTextRange::kFontGroup;
                    return font.forget();
                }
            }
        } else if (fe->HasCharacter(aCh)) {
            font = GetFontAt(i);
            if (font) {
                *aMatchType = gfxTextRange::kFontGroup;
                return font.forget();
            }
        }

        // If italic, test the regular face to see if it supports the character.
        // Only do this for platform fonts, not userfonts.
        if (mStyle.style != NS_FONT_STYLE_NORMAL && !ff.IsUserFont()) {
            font = FindNonItalicFaceForChar(mFonts[i].Family(), aCh);
            if (font) {
                *aMatchType = gfxTextRange::kFontGroup;
                return font.forget();
            }
        }
    }

    if (fontListLength == 0) {
        nsRefPtr<gfxFont> defaultFont = GetDefaultFont();
        if (defaultFont->HasCharacter(aCh)) {
            *aMatchType = gfxTextRange::kFontGroup;
            return defaultFont.forget();
        }
    }

    // if character is in Private Use Area, don't do matching against pref or system fonts
    if ((aCh >= 0xE000  && aCh <= 0xF8FF) || (aCh >= 0xF0000 && aCh <= 0x10FFFD))
        return nullptr;

    // 2. search pref fonts
    nsRefPtr<gfxFont> font = WhichPrefFontSupportsChar(aCh);
    if (font) {
        *aMatchType = gfxTextRange::kPrefsFallback;
        return font.forget();
    }

    // 3. use fallback fonts
    // -- before searching for something else check the font used for the previous character
    if (aPrevMatchedFont && aPrevMatchedFont->HasCharacter(aCh)) {
        *aMatchType = gfxTextRange::kSystemFallback;
        nsRefPtr<gfxFont> ret = aPrevMatchedFont;
        return ret.forget();
    }

    // never fall back for characters from unknown scripts
    if (aRunScript == HB_SCRIPT_UNKNOWN) {
        return nullptr;
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
                                 int32_t aRunScript, uint16_t aOrientation)
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
        nsRefPtr<gfxFont> font =
            FindFontForChar(ch, prevCh, nextCh, aRunScript, prevFont,
                            &matchType);

#ifndef RELEASE_BUILD
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
            // if font has changed, make a new range
            gfxTextRange& prevRange = aRanges[lastRangeIndex];
            if (prevRange.font != font || prevRange.matchType != matchType ||
                prevRange.orientation != orient) {
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
        mUnderlineOffset = UNDERLINE_OFFSET_NOT_SET;
        mSkipDrawing = false;
        BuildFontList();
        mCurrGeneration = GetGeneration();
        mCachedEllipsisTextRun = nullptr;
    } else if (mCurrGeneration != GetGeneration()) {
        // load state change occurred, verify load state and validity of fonts
        mSkipDrawing = false;
        mUnderlineOffset = UNDERLINE_OFFSET_NOT_SET;
        mCachedEllipsisTextRun = nullptr;

        uint32_t len = mFonts.Length();
        for (uint32_t i = 0; i < len; i++) {
            FamilyFace& ff = mFonts[i];
            if (ff.Font() || !ff.IsUserFont()) {
                continue;
            }

            // confirm status
            gfxUserFontEntry *ufe =
                static_cast<gfxUserFontEntry*>(mFonts[i].FontEntry());
            gfxUserFontEntry::UserFontLoadState state = ufe->LoadState();
            switch (state) {
                case gfxUserFontEntry::STATUS_LOADING:
                    ff.SetLoading(true);
                    break;
                case gfxUserFontEntry::STATUS_FAILED:
                    ff.SetInvalid();
                    // fall-thru to the default case
                default:
                    ff.SetLoading(false);
            }
            if (ufe->WaitForUserFont()) {
                mSkipDrawing = true;
            }
        }

        mCurrGeneration = GetGeneration();
    }
}

struct PrefFontCallbackData {
    explicit PrefFontCallbackData(nsTArray<nsRefPtr<gfxFontFamily> >& aFamiliesArray)
        : mPrefFamilies(aFamiliesArray)
    {}

    nsTArray<nsRefPtr<gfxFontFamily> >& mPrefFamilies;

    static bool AddFontFamilyEntry(eFontPrefLang aLang, const nsAString& aName, void *aClosure)
    {
        PrefFontCallbackData *prefFontData = static_cast<PrefFontCallbackData*>(aClosure);

        gfxFontFamily *family = gfxPlatformFontList::PlatformFontList()->FindFamily(aName);
        if (family) {
            prefFontData->mPrefFamilies.AppendElement(family);
        }
        return true;
    }
};

already_AddRefed<gfxFont>
gfxFontGroup::WhichPrefFontSupportsChar(uint32_t aCh)
{
    nsRefPtr<gfxFont> font;

    // get the pref font list if it hasn't been set up already
    uint32_t unicodeRange = FindCharUnicodeRange(aCh);
    eFontPrefLang charLang = gfxPlatform::GetPlatform()->GetFontPrefLangFor(unicodeRange);

    // if the last pref font was the first family in the pref list, no need to recheck through a list of families
    if (mLastPrefFont && charLang == mLastPrefLang &&
        mLastPrefFirstFont && mLastPrefFont->HasCharacter(aCh)) {
        font = mLastPrefFont;
        return font.forget();
    }

    // based on char lang and page lang, set up list of pref lang fonts to check
    eFontPrefLang prefLangs[kMaxLenPrefLangList];
    uint32_t i, numLangs = 0;

    gfxPlatform::GetPlatform()->GetLangPrefs(prefLangs, numLangs, charLang, mPageLang);

    for (i = 0; i < numLangs; i++) {
        nsAutoTArray<nsRefPtr<gfxFontFamily>, 5> families;
        eFontPrefLang currentLang = prefLangs[i];

        gfxPlatformFontList *fontList = gfxPlatformFontList::PlatformFontList();

        // get the pref families for a single pref lang
        if (!fontList->GetPrefFontFamilyEntries(currentLang, &families)) {
            eFontPrefLang prefLangsToSearch[1] = { currentLang };
            PrefFontCallbackData prefFontData(families);
            gfxPlatform::ForEachPrefFont(prefLangsToSearch, 1, PrefFontCallbackData::AddFontFamilyEntry,
                                           &prefFontData);
            fontList->SetPrefFontFamilyEntries(currentLang, families);
        }

        // find the first pref font that includes the character
        uint32_t  j, numPrefs;
        numPrefs = families.Length();
        for (j = 0; j < numPrefs; j++) {
            // look up the appropriate face
            gfxFontFamily *family = families[j];
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
            if (fe && fe->TestCharacterMap(aCh)) {
                nsRefPtr<gfxFont> prefFont = fe->FindOrMakeFont(&mStyle, needsBold);
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
                                          int32_t aRunScript)
{
    gfxFontEntry *fe = 
        gfxPlatformFontList::PlatformFontList()->
            SystemFindFontForChar(aCh, aNextCh, aRunScript, &mStyle);
    if (fe) {
        bool wantBold = mStyle.ComputeWeight() >= 6;
        nsRefPtr<gfxFont> font =
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
