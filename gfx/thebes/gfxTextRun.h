/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_TEXTRUN_H
#define GFX_TEXTRUN_H

#include "gfxTypes.h"
#include "nsString.h"
#include "gfxPoint.h"
#include "gfxFont.h"
#include "nsTArray.h"
#include "gfxSkipChars.h"
#include "gfxPlatform.h"
#include "mozilla/MemoryReporting.h"
#include "DrawMode.h"
#include "harfbuzz/hb.h"
#include "nsUnicodeScriptCodes.h"

#ifdef DEBUG
#include <stdio.h>
#endif

class gfxContext;
class gfxFontGroup;
class gfxUserFontEntry;
class gfxUserFontSet;
class gfxTextContextPaint;
class nsIAtom;
class nsILanguageAtomService;
class gfxMissingFontRecorder;

/**
 * Callback for Draw() to use when drawing text with mode
 * DrawMode::GLYPH_PATH.
 */
struct gfxTextRunDrawCallbacks {

    /**
     * Constructs a new DrawCallbacks object.
     *
     * @param aShouldPaintSVGGlyphs If true, SVG glyphs will be painted.  If
     *   false, SVG glyphs will not be painted; fallback plain glyphs are not
     *   emitted either.
     */
    explicit gfxTextRunDrawCallbacks(bool aShouldPaintSVGGlyphs = false)
      : mShouldPaintSVGGlyphs(aShouldPaintSVGGlyphs)
    {
    }

    /**
     * Called when a path has been emitted to the gfxContext when
     * painting a text run.  This can be called any number of times,
     * due to partial ligatures and intervening SVG glyphs.
     */
    virtual void NotifyGlyphPathEmitted() = 0;

    bool mShouldPaintSVGGlyphs;
};

/**
 * gfxTextRun is an abstraction for drawing and measuring substrings of a run
 * of text. It stores runs of positioned glyph data, each run having a single
 * gfxFont. The glyphs are associated with a string of source text, and the
 * gfxTextRun APIs take parameters that are offsets into that source text.
 * 
 * gfxTextRuns are not refcounted. They should be deleted when no longer required.
 * 
 * gfxTextRuns are mostly immutable. The only things that can change are
 * inter-cluster spacing and line break placement. Spacing is always obtained
 * lazily by methods that need it, it is not cached. Line breaks are stored
 * persistently (insofar as they affect the shaping of glyphs; gfxTextRun does
 * not actually do anything to explicitly account for line breaks). Initially
 * there are no line breaks. The textrun can record line breaks before or after
 * any given cluster. (Line breaks specified inside clusters are ignored.)
 * 
 * It is important that zero-length substrings are handled correctly. This will
 * be on the test!
 */
class gfxTextRun : public gfxShapedText {
public:

    // Override operator delete to properly free the object that was
    // allocated via malloc.
    void operator delete(void* p) {
        free(p);
    }

    virtual ~gfxTextRun();

    typedef gfxFont::RunMetrics Metrics;

    // Public textrun API for general use

    bool IsClusterStart(uint32_t aPos) const {
        NS_ASSERTION(aPos < GetLength(), "aPos out of range");
        return mCharacterGlyphs[aPos].IsClusterStart();
    }
    bool IsLigatureGroupStart(uint32_t aPos) const {
        NS_ASSERTION(aPos < GetLength(), "aPos out of range");
        return mCharacterGlyphs[aPos].IsLigatureGroupStart();
    }
    bool CanBreakLineBefore(uint32_t aPos) const {
        return CanBreakBefore(aPos) == CompressedGlyph::FLAG_BREAK_TYPE_NORMAL;
    }
    bool CanHyphenateBefore(uint32_t aPos) const {
        return CanBreakBefore(aPos) == CompressedGlyph::FLAG_BREAK_TYPE_HYPHEN;
    }

    // Returns a gfxShapedText::CompressedGlyph::FLAG_BREAK_TYPE_* value
    // as defined in gfxFont.h (may be NONE, NORMAL or HYPHEN).
    uint8_t CanBreakBefore(uint32_t aPos) const {
        NS_ASSERTION(aPos < GetLength(), "aPos out of range");
        return mCharacterGlyphs[aPos].CanBreakBefore();
    }

    bool CharIsSpace(uint32_t aPos) const {
        NS_ASSERTION(aPos < GetLength(), "aPos out of range");
        return mCharacterGlyphs[aPos].CharIsSpace();
    }
    bool CharIsTab(uint32_t aPos) const {
        NS_ASSERTION(aPos < GetLength(), "aPos out of range");
        return mCharacterGlyphs[aPos].CharIsTab();
    }
    bool CharIsNewline(uint32_t aPos) const {
        NS_ASSERTION(aPos < GetLength(), "aPos out of range");
        return mCharacterGlyphs[aPos].CharIsNewline();
    }
    bool CharIsLowSurrogate(uint32_t aPos) const {
        NS_ASSERTION(aPos < GetLength(), "aPos out of range");
        return mCharacterGlyphs[aPos].CharIsLowSurrogate();
    }

    // All uint32_t aStart, uint32_t aLength ranges below are restricted to
    // grapheme cluster boundaries! All offsets are in terms of the string
    // passed into MakeTextRun.
    
    // All coordinates are in layout/app units

    /**
     * Set the potential linebreaks for a substring of the textrun. These are
     * the "allow break before" points. Initially, there are no potential
     * linebreaks.
     * 
     * This can change glyphs and/or geometry! Some textruns' shapes
     * depend on potential line breaks (e.g., title-case-converting textruns).
     * This function is virtual so that those textruns can reshape themselves.
     * 
     * @return true if this changed the linebreaks, false if the new line
     * breaks are the same as the old
     */
    virtual bool SetPotentialLineBreaks(uint32_t aStart, uint32_t aLength,
                                          uint8_t *aBreakBefore,
                                          gfxContext *aRefContext);

    /**
     * Layout provides PropertyProvider objects. These allow detection of
     * potential line break points and computation of spacing. We pass the data
     * this way to allow lazy data acquisition; for example BreakAndMeasureText
     * will want to only ask for properties of text it's actually looking at.
     * 
     * NOTE that requested spacing may not actually be applied, if the textrun
     * is unable to apply it in some context. Exception: spacing around a
     * whitespace character MUST always be applied.
     */
    class PropertyProvider {
    public:
        // Detect hyphenation break opportunities in the given range; breaks
        // not at cluster boundaries will be ignored.
        virtual void GetHyphenationBreaks(uint32_t aStart, uint32_t aLength,
                                          bool *aBreakBefore) = 0;

        // Returns the provider's hyphenation setting, so callers can decide
        // whether it is necessary to call GetHyphenationBreaks.
        // Result is an NS_STYLE_HYPHENS_* value.
        virtual int8_t GetHyphensOption() = 0;

        // Returns the extra width that will be consumed by a hyphen. This should
        // be constant for a given textrun.
        virtual gfxFloat GetHyphenWidth() = 0;

        typedef gfxFont::Spacing Spacing;

        /**
         * Get the spacing around the indicated characters. Spacing must be zero
         * inside clusters. In other words, if character i is not
         * CLUSTER_START, then character i-1 must have zero after-spacing and
         * character i must have zero before-spacing.
         */
        virtual void GetSpacing(uint32_t aStart, uint32_t aLength,
                                Spacing *aSpacing) = 0;

        // Returns a gfxContext that can be used to measure the hyphen glyph.
        // Only called if the hyphen width is requested.
        virtual already_AddRefed<gfxContext> GetContext() = 0;

        // Return the appUnitsPerDevUnit value to be used when measuring.
        // Only called if the hyphen width is requested.
        virtual uint32_t GetAppUnitsPerDevUnit() = 0;
    };

    class ClusterIterator {
    public:
        explicit ClusterIterator(gfxTextRun *aTextRun);

        void Reset();

        bool NextCluster();

        uint32_t Position() const {
            return mCurrentChar;
        }

        uint32_t ClusterLength() const;

        gfxFloat ClusterAdvance(PropertyProvider *aProvider) const;

    private:
        gfxTextRun *mTextRun;
        uint32_t    mCurrentChar;
    };

    /**
     * Draws a substring. Uses only GetSpacing from aBreakProvider.
     * The provided point is the baseline origin on the left of the string
     * for LTR, on the right of the string for RTL.
     * @param aAdvanceWidth if non-null, the advance width of the substring
     * is returned here.
     * 
     * Drawing should respect advance widths in the sense that for LTR runs,
     * Draw(ctx, pt, offset1, length1, dirty, &provider, &advance) followed by
     * Draw(ctx, gfxPoint(pt.x + advance, pt.y), offset1 + length1, length2,
     *      dirty, &provider, nullptr) should have the same effect as
     * Draw(ctx, pt, offset1, length1+length2, dirty, &provider, nullptr).
     * For RTL runs the rule is:
     * Draw(ctx, pt, offset1 + length1, length2, dirty, &provider, &advance) followed by
     * Draw(ctx, gfxPoint(pt.x + advance, pt.y), offset1, length1,
     *      dirty, &provider, nullptr) should have the same effect as
     * Draw(ctx, pt, offset1, length1+length2, dirty, &provider, nullptr).
     * 
     * Glyphs should be drawn in logical content order, which can be significant
     * if they overlap (perhaps due to negative spacing).
     */
    void Draw(gfxContext *aContext, gfxPoint aPt,
              DrawMode aDrawMode,
              uint32_t aStart, uint32_t aLength,
              PropertyProvider *aProvider,
              gfxFloat *aAdvanceWidth, gfxTextContextPaint *aContextPaint,
              gfxTextRunDrawCallbacks *aCallbacks = nullptr);

    /**
     * Computes the ReflowMetrics for a substring.
     * Uses GetSpacing from aBreakProvider.
     * @param aBoundingBoxType which kind of bounding box (loose/tight)
     */
    Metrics MeasureText(uint32_t aStart, uint32_t aLength,
                        gfxFont::BoundingBoxType aBoundingBoxType,
                        gfxContext *aRefContextForTightBoundingBox,
                        PropertyProvider *aProvider);

    /**
     * Computes just the advance width for a substring.
     * Uses GetSpacing from aBreakProvider.
     * If aSpacing is not null, the spacing attached before and after
     * the substring would be returned in it. NOTE: the spacing is
     * included in the advance width.
     */
    gfxFloat GetAdvanceWidth(uint32_t aStart, uint32_t aLength,
                             PropertyProvider *aProvider,
                             PropertyProvider::Spacing* aSpacing = nullptr);

    /**
     * Clear all stored line breaks for the given range (both before and after),
     * and then set the line-break state before aStart to aBreakBefore and
     * after the last cluster to aBreakAfter.
     * 
     * We require that before and after line breaks be consistent. For clusters
     * i and i+1, we require that if there is a break after cluster i, a break
     * will be specified before cluster i+1. This may be temporarily violated
     * (e.g. after reflowing line L and before reflowing line L+1); to handle
     * these temporary violations, we say that there is a break betwen i and i+1
     * if a break is specified after i OR a break is specified before i+1.
     * 
     * This can change textrun geometry! The existence of a linebreak can affect
     * the advance width of the cluster before the break (when kerning) or the
     * geometry of one cluster before the break or any number of clusters
     * after the break. (The one-cluster-before-the-break limit is somewhat
     * arbitrary; if some scripts require breaking it, then we need to
     * alter nsTextFrame::TrimTrailingWhitespace, perhaps drastically becase
     * it could affect the layout of frames before it...)
     * 
     * We return true if glyphs or geometry changed, false otherwise. This
     * function is virtual so that gfxTextRun subclasses can reshape
     * properly.
     * 
     * @param aAdvanceWidthDelta if non-null, returns the change in advance
     * width of the given range.
     */
    virtual bool SetLineBreaks(uint32_t aStart, uint32_t aLength,
                                 bool aLineBreakBefore, bool aLineBreakAfter,
                                 gfxFloat *aAdvanceWidthDelta,
                                 gfxContext *aRefContext);

    enum SuppressBreak {
      eNoSuppressBreak,
      // Measure the range of text as if there is no break before it.
      eSuppressInitialBreak,
      // Measure the range of text as if it contains no break
      eSuppressAllBreaks
    };

    /**
     * Finds the longest substring that will fit into the given width.
     * Uses GetHyphenationBreaks and GetSpacing from aBreakProvider.
     * Guarantees the following:
     * -- 0 <= result <= aMaxLength
     * -- result is the maximal value of N such that either
     *       N < aMaxLength && line break at N && GetAdvanceWidth(aStart, N) <= aWidth
     *   OR  N < aMaxLength && hyphen break at N && GetAdvanceWidth(aStart, N) + GetHyphenWidth() <= aWidth
     *   OR  N == aMaxLength && GetAdvanceWidth(aStart, N) <= aWidth
     * where GetAdvanceWidth assumes the effect of
     * SetLineBreaks(aStart, N, aLineBreakBefore, N < aMaxLength, aProvider)
     * -- if no such N exists, then result is the smallest N such that
     *       N < aMaxLength && line break at N
     *   OR  N < aMaxLength && hyphen break at N
     *   OR  N == aMaxLength
     *
     * The call has the effect of
     * SetLineBreaks(aStart, result, aLineBreakBefore, result < aMaxLength, aProvider)
     * and the returned metrics and the invariants above reflect this.
     *
     * @param aMaxLength this can be UINT32_MAX, in which case the length used
     * is up to the end of the string
     * @param aLineBreakBefore set to true if and only if there is an actual
     * line break at the start of this string.
     * @param aSuppressBreak what break should be suppressed.
     * @param aTrimWhitespace if non-null, then we allow a trailing run of
     * spaces to be trimmed; the width of the space(s) will not be included in
     * the measured string width for comparison with the limit aWidth, and
     * trimmed spaces will not be included in returned metrics. The width
     * of the trimmed spaces will be returned in aTrimWhitespace.
     * Trimmed spaces are still counted in the "characters fit" result.
     * @param aMetrics if non-null, we fill this in for the returned substring.
     * If a hyphenation break was used, the hyphen is NOT included in the returned metrics.
     * @param aBoundingBoxType whether to make the bounding box in aMetrics tight
     * @param aRefContextForTightBoundingBox a reference context to get the
     * tight bounding box, if requested
     * @param aUsedHyphenation if non-null, records if we selected a hyphenation break
     * @param aLastBreak if non-null and result is aMaxLength, we set this to
     * the maximal N such that
     *       N < aMaxLength && line break at N && GetAdvanceWidth(aStart, N) <= aWidth
     *   OR  N < aMaxLength && hyphen break at N && GetAdvanceWidth(aStart, N) + GetHyphenWidth() <= aWidth
     * or UINT32_MAX if no such N exists, where GetAdvanceWidth assumes
     * the effect of
     * SetLineBreaks(aStart, N, aLineBreakBefore, N < aMaxLength, aProvider)
     *
     * @param aCanWordWrap true if we can break between any two grapheme
     * clusters. This is set by word-wrap: break-word
     *
     * @param aBreakPriority in/out the priority of the break opportunity
     * saved in the line. If we are prioritizing break opportunities, we will
     * not set a break with a lower priority. @see gfxBreakPriority.
     * 
     * Note that negative advance widths are possible especially if negative
     * spacing is provided.
     */
    uint32_t BreakAndMeasureText(uint32_t aStart, uint32_t aMaxLength,
                                 bool aLineBreakBefore, gfxFloat aWidth,
                                 PropertyProvider *aProvider,
                                 SuppressBreak aSuppressBreak,
                                 gfxFloat *aTrimWhitespace,
                                 Metrics *aMetrics,
                                 gfxFont::BoundingBoxType aBoundingBoxType,
                                 gfxContext *aRefContextForTightBoundingBox,
                                 bool *aUsedHyphenation,
                                 uint32_t *aLastBreak,
                                 bool aCanWordWrap,
                                 gfxBreakPriority *aBreakPriority);

    /**
     * Update the reference context.
     * XXX this is a hack. New text frame does not call this. Use only
     * temporarily for old text frame.
     */
    void SetContext(gfxContext *aContext) {}

    // Utility getters

    void *GetUserData() const { return mUserData; }
    void SetUserData(void *aUserData) { mUserData = aUserData; }

    void SetFlagBits(uint32_t aFlags) {
      NS_ASSERTION(!(aFlags & ~gfxTextRunFactory::SETTABLE_FLAGS),
                   "Only user flags should be mutable");
      mFlags |= aFlags;
    }
    void ClearFlagBits(uint32_t aFlags) {
      NS_ASSERTION(!(aFlags & ~gfxTextRunFactory::SETTABLE_FLAGS),
                   "Only user flags should be mutable");
      mFlags &= ~aFlags;
    }
    const gfxSkipChars& GetSkipChars() const { return mSkipChars; }
    gfxFontGroup *GetFontGroup() const { return mFontGroup; }


    // Call this, don't call "new gfxTextRun" directly. This does custom
    // allocation and initialization
    static gfxTextRun *Create(const gfxTextRunFactory::Parameters *aParams,
                              uint32_t aLength, gfxFontGroup *aFontGroup,
                              uint32_t aFlags);

    // The text is divided into GlyphRuns as necessary
    struct GlyphRun {
        nsRefPtr<gfxFont> mFont;   // never null
        uint32_t          mCharacterOffset; // into original UTF16 string
        uint8_t           mMatchType;
        uint16_t          mOrientation; // gfxTextRunFactory::TEXT_ORIENT_* value
    };

    class GlyphRunIterator {
    public:
        GlyphRunIterator(gfxTextRun *aTextRun, uint32_t aStart, uint32_t aLength)
          : mTextRun(aTextRun), mStartOffset(aStart), mEndOffset(aStart + aLength) {
            mNextIndex = mTextRun->FindFirstGlyphRunContaining(aStart);
        }
        bool NextRun();
        GlyphRun *GetGlyphRun() { return mGlyphRun; }
        uint32_t GetStringStart() { return mStringStart; }
        uint32_t GetStringEnd() { return mStringEnd; }
    private:
        gfxTextRun *mTextRun;
        GlyphRun   *mGlyphRun;
        uint32_t    mStringStart;
        uint32_t    mStringEnd;
        uint32_t    mNextIndex;
        uint32_t    mStartOffset;
        uint32_t    mEndOffset;
    };

    class GlyphRunOffsetComparator {
    public:
        bool Equals(const GlyphRun& a,
                      const GlyphRun& b) const
        {
            return a.mCharacterOffset == b.mCharacterOffset;
        }

        bool LessThan(const GlyphRun& a,
                        const GlyphRun& b) const
        {
            return a.mCharacterOffset < b.mCharacterOffset;
        }
    };

    friend class GlyphRunIterator;
    friend class FontSelector;

    // API for setting up the textrun glyphs. Should only be called by
    // things that construct textruns.
    /**
     * We've found a run of text that should use a particular font. Call this
     * only during initialization when font substitution has been computed.
     * Call it before setting up the glyphs for the characters in this run;
     * SetMissingGlyph requires that the correct glyphrun be installed.
     *
     * If aForceNewRun, a new glyph run will be added, even if the
     * previously added run uses the same font.  If glyph runs are
     * added out of strictly increasing aStartCharIndex order (via
     * force), then SortGlyphRuns must be called after all glyph runs
     * are added before any further operations are performed with this
     * TextRun.
     */
    nsresult AddGlyphRun(gfxFont *aFont, uint8_t aMatchType,
                         uint32_t aStartCharIndex, bool aForceNewRun,
                         uint16_t aOrientation);
    void ResetGlyphRuns() { mGlyphRuns.Clear(); }
    void SortGlyphRuns();
    void SanitizeGlyphRuns();

    CompressedGlyph* GetCharacterGlyphs() {
        NS_ASSERTION(mCharacterGlyphs, "failed to initialize mCharacterGlyphs");
        return mCharacterGlyphs;
    }

    // clean out results from shaping in progress, used for fallback scenarios
    void ClearGlyphsAndCharacters();

    void SetSpaceGlyph(gfxFont *aFont, gfxContext *aContext, uint32_t aCharIndex,
                       uint16_t aOrientation);

    // Set the glyph data for the given character index to the font's
    // space glyph, IF this can be done as a "simple" glyph record
    // (not requiring a DetailedGlyph entry). This avoids the need to call
    // the font shaper and go through the shaped-word cache for most spaces.
    //
    // The parameter aSpaceChar is the original character code for which
    // this space glyph is being used; if this is U+0020, we need to record
    // that it could be trimmed at a run edge, whereas other kinds of space
    // (currently just U+00A0) would not be trimmable/breakable.
    //
    // Returns true if it was able to set simple glyph data for the space;
    // if it returns false, the caller needs to fall back to some other
    // means to create the necessary (detailed) glyph data.
    bool SetSpaceGlyphIfSimple(gfxFont *aFont, gfxContext *aContext,
                               uint32_t aCharIndex, char16_t aSpaceChar,
                               uint16_t aOrientation);

    // Record the positions of specific characters that layout may need to
    // detect in the textrun, even though it doesn't have an explicit copy
    // of the original text. These are recorded using flag bits in the
    // CompressedGlyph record; if necessary, we convert "simple" glyph records
    // to "complex" ones as the Tab and Newline flags are not present in
    // simple CompressedGlyph records.
    void SetIsTab(uint32_t aIndex) {
        CompressedGlyph *g = &mCharacterGlyphs[aIndex];
        if (g->IsSimpleGlyph()) {
            DetailedGlyph *details = AllocateDetailedGlyphs(aIndex, 1);
            details->mGlyphID = g->GetSimpleGlyph();
            details->mAdvance = g->GetSimpleAdvance();
            details->mXOffset = details->mYOffset = 0;
            SetGlyphs(aIndex, CompressedGlyph().SetComplex(true, true, 1), details);
        }
        g->SetIsTab();
    }
    void SetIsNewline(uint32_t aIndex) {
        CompressedGlyph *g = &mCharacterGlyphs[aIndex];
        if (g->IsSimpleGlyph()) {
            DetailedGlyph *details = AllocateDetailedGlyphs(aIndex, 1);
            details->mGlyphID = g->GetSimpleGlyph();
            details->mAdvance = g->GetSimpleAdvance();
            details->mXOffset = details->mYOffset = 0;
            SetGlyphs(aIndex, CompressedGlyph().SetComplex(true, true, 1), details);
        }
        g->SetIsNewline();
    }
    void SetIsLowSurrogate(uint32_t aIndex) {
        SetGlyphs(aIndex, CompressedGlyph().SetComplex(false, false, 0), nullptr);
        mCharacterGlyphs[aIndex].SetIsLowSurrogate();
    }

    /**
     * Prefetch all the glyph extents needed to ensure that Measure calls
     * on this textrun not requesting tight boundingBoxes will succeed. Note
     * that some glyph extents might not be fetched due to OOM or other
     * errors.
     */
    void FetchGlyphExtents(gfxContext *aRefContext);

    uint32_t CountMissingGlyphs();
    const GlyphRun *GetGlyphRuns(uint32_t *aNumGlyphRuns) {
        *aNumGlyphRuns = mGlyphRuns.Length();
        return mGlyphRuns.Elements();
    }
    // Returns the index of the GlyphRun containing the given offset.
    // Returns mGlyphRuns.Length() when aOffset is mCharacterCount.
    uint32_t FindFirstGlyphRunContaining(uint32_t aOffset);

    // Copy glyph data from a ShapedWord into this textrun.
    void CopyGlyphDataFrom(gfxShapedWord *aSource, uint32_t aStart);

    // Copy glyph data for a range of characters from aSource to this
    // textrun.
    void CopyGlyphDataFrom(gfxTextRun *aSource, uint32_t aStart,
                           uint32_t aLength, uint32_t aDest);

    nsExpirationState *GetExpirationState() { return &mExpirationState; }

    // Tell the textrun to release its reference to its creating gfxFontGroup
    // immediately, rather than on destruction. This is used for textruns
    // that are actually owned by a gfxFontGroup, so that they don't keep it
    // permanently alive due to a circular reference. (The caller of this is
    // taking responsibility for ensuring the textrun will not outlive its
    // mFontGroup.)
    void ReleaseFontGroup();

    struct LigatureData {
        // textrun offsets of the start and end of the containing ligature
        uint32_t mLigatureStart;
        uint32_t mLigatureEnd;
        // appunits advance to the start of the ligature part within the ligature;
        // never includes any spacing
        gfxFloat mPartAdvance;
        // appunits width of the ligature part; includes before-spacing
        // when the part is at the start of the ligature, and after-spacing
        // when the part is as the end of the ligature
        gfxFloat mPartWidth;
        
        bool mClipBeforePart;
        bool mClipAfterPart;
    };
    
    // return storage used by this run, for memory reporter;
    // nsTransformedTextRun needs to override this as it holds additional data
    virtual size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf)
      MOZ_MUST_OVERRIDE;
    virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
      MOZ_MUST_OVERRIDE;

    // Get the size, if it hasn't already been gotten, marking as it goes.
    size_t MaybeSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)  {
        if (mFlags & gfxTextRunFactory::TEXT_RUN_SIZE_ACCOUNTED) {
            return 0;
        }
        mFlags |= gfxTextRunFactory::TEXT_RUN_SIZE_ACCOUNTED;
        return SizeOfIncludingThis(aMallocSizeOf);
    }
    void ResetSizeOfAccountingFlags() {
        mFlags &= ~gfxTextRunFactory::TEXT_RUN_SIZE_ACCOUNTED;
    }

    // shaping state - for some font features, fallback is required that
    // affects the entire run. for example, fallback for one script/font
    // portion of a textrun requires fallback to be applied to the entire run

    enum ShapingState {
        eShapingState_Normal,                 // default state
        eShapingState_ShapingWithFeature,     // have shaped with feature
        eShapingState_ShapingWithFallback,    // have shaped with fallback
        eShapingState_Aborted,                // abort initial iteration
        eShapingState_ForceFallbackFeature    // redo with fallback forced on
    };

    ShapingState GetShapingState() const { return mShapingState; }
    void SetShapingState(ShapingState aShapingState) {
        mShapingState = aShapingState;
    }

#ifdef DEBUG
    void Dump(FILE* aOutput);
#endif

protected:
    /**
     * Create a textrun, and set its mCharacterGlyphs to point immediately
     * after the base object; this is ONLY used in conjunction with placement
     * new, after allocating a block large enough for the glyph records to
     * follow the base textrun object.
     */
    gfxTextRun(const gfxTextRunFactory::Parameters *aParams,
               uint32_t aLength, gfxFontGroup *aFontGroup, uint32_t aFlags);

    /**
     * Helper for the Create() factory method to allocate the required
     * glyph storage for a textrun object with the basic size aSize,
     * plus room for aLength glyph records.
     */
    static void* AllocateStorageForTextRun(size_t aSize, uint32_t aLength);

    // Pointer to the array of CompressedGlyph records; must be initialized
    // when the object is constructed.
    CompressedGlyph *mCharacterGlyphs;

private:
    // **** general helpers **** 

    // Get the total advance for a range of glyphs.
    int32_t GetAdvanceForGlyphs(uint32_t aStart, uint32_t aEnd);

    // Spacing for characters outside the range aSpacingStart/aSpacingEnd
    // is assumed to be zero; such characters are not passed to aProvider.
    // This is useful to protect aProvider from being passed character indices
    // it is not currently able to handle.
    bool GetAdjustedSpacingArray(uint32_t aStart, uint32_t aEnd,
                                   PropertyProvider *aProvider,
                                   uint32_t aSpacingStart, uint32_t aSpacingEnd,
                                   nsTArray<PropertyProvider::Spacing> *aSpacing);

    //  **** ligature helpers ****
    // (Platforms do the actual ligaturization, but we need to do a bunch of stuff
    // to handle requests that begin or end inside a ligature)

    // if aProvider is null then mBeforeSpacing and mAfterSpacing are set to zero
    LigatureData ComputeLigatureData(uint32_t aPartStart, uint32_t aPartEnd,
                                     PropertyProvider *aProvider);
    gfxFloat ComputePartialLigatureWidth(uint32_t aPartStart, uint32_t aPartEnd,
                                         PropertyProvider *aProvider);
    void DrawPartialLigature(gfxFont *aFont, uint32_t aStart, uint32_t aEnd,
                             gfxPoint *aPt, PropertyProvider *aProvider,
                             TextRunDrawParams& aParams, uint16_t aOrientation);
    // Advance aStart to the start of the nearest ligature; back up aEnd
    // to the nearest ligature end; may result in *aStart == *aEnd
    void ShrinkToLigatureBoundaries(uint32_t *aStart, uint32_t *aEnd);
    // result in appunits
    gfxFloat GetPartialLigatureWidth(uint32_t aStart, uint32_t aEnd, PropertyProvider *aProvider);
    void AccumulatePartialLigatureMetrics(gfxFont *aFont,
                                          uint32_t aStart, uint32_t aEnd,
                                          gfxFont::BoundingBoxType aBoundingBoxType,
                                          gfxContext *aRefContext,
                                          PropertyProvider *aProvider,
                                          uint16_t aOrientation,
                                          Metrics *aMetrics);

    // **** measurement helper ****
    void AccumulateMetricsForRun(gfxFont *aFont, uint32_t aStart, uint32_t aEnd,
                                 gfxFont::BoundingBoxType aBoundingBoxType,
                                 gfxContext *aRefContext,
                                 PropertyProvider *aProvider,
                                 uint32_t aSpacingStart, uint32_t aSpacingEnd,
                                 uint16_t aOrientation,
                                 Metrics *aMetrics);

    // **** drawing helper ****
    void DrawGlyphs(gfxFont *aFont, uint32_t aStart, uint32_t aEnd,
                    gfxPoint *aPt, PropertyProvider *aProvider,
                    uint32_t aSpacingStart, uint32_t aSpacingEnd,
                    TextRunDrawParams& aParams, uint16_t aOrientation);

    // XXX this should be changed to a GlyphRun plus a maybe-null GlyphRun*,
    // for smaller size especially in the super-common one-glyphrun case
    nsAutoTArray<GlyphRun,1>        mGlyphRuns;

    void             *mUserData;
    gfxFontGroup     *mFontGroup; // addrefed on creation, but our reference
                                  // may be released by ReleaseFontGroup()
    gfxSkipChars      mSkipChars;
    nsExpirationState mExpirationState;

    bool              mSkipDrawing; // true if the font group we used had a user font
                                    // download that's in progress, so we should hide text
                                    // until the download completes (or timeout fires)
    bool              mReleasedFontGroup; // we already called NS_RELEASE on
                                          // mFontGroup, so don't do it again

    // shaping state for handling variant fallback features
    // such as subscript/superscript variant glyphs
    ShapingState      mShapingState;
};

class gfxFontGroup : public gfxTextRunFactory {
public:
    static void Shutdown(); // platform must call this to release the languageAtomService

    gfxFontGroup(const mozilla::FontFamilyList& aFontFamilyList,
                 const gfxFontStyle* aStyle,
                 gfxTextPerfMetrics* aTextPerf,
                 gfxUserFontSet* aUserFontSet = nullptr);

    virtual ~gfxFontGroup();

    // Returns first valid font in the fontlist or default font.
    // Initiates userfont loads if userfont not loaded
    virtual gfxFont* GetFirstValidFont(uint32_t aCh = 0x20);

    // Returns the first font in the font-group that has an OpenType MATH table,
    // or null if no such font is available. The GetMathConstant methods may be
    // called on the returned font.
    gfxFont *GetFirstMathFont();

    const gfxFontStyle *GetStyle() const { return &mStyle; }

    virtual gfxFontGroup *Copy(const gfxFontStyle *aStyle);

    /**
     * The listed characters should be treated as invisible and zero-width
     * when creating textruns.
     */
    static bool IsInvalidChar(uint8_t ch);
    static bool IsInvalidChar(char16_t ch);

    /**
     * Make a textrun for a given string.
     * If aText is not persistent (aFlags & TEXT_IS_PERSISTENT), the
     * textrun will copy it.
     * This calls FetchGlyphExtents on the textrun.
     */
    virtual gfxTextRun *MakeTextRun(const char16_t *aString, uint32_t aLength,
                                    const Parameters *aParams, uint32_t aFlags,
                                    gfxMissingFontRecorder *aMFR);
    /**
     * Make a textrun for a given string.
     * If aText is not persistent (aFlags & TEXT_IS_PERSISTENT), the
     * textrun will copy it.
     * This calls FetchGlyphExtents on the textrun.
     */
    virtual gfxTextRun *MakeTextRun(const uint8_t *aString, uint32_t aLength,
                                    const Parameters *aParams, uint32_t aFlags,
                                    gfxMissingFontRecorder *aMFR);

    /**
     * Textrun creation helper for clients that don't want to pass
     * a full Parameters record.
     */
    template<typename T>
    gfxTextRun *MakeTextRun(const T *aString, uint32_t aLength,
                            gfxContext *aRefContext,
                            int32_t aAppUnitsPerDevUnit,
                            uint32_t aFlags,
                            gfxMissingFontRecorder *aMFR)
    {
        gfxTextRunFactory::Parameters params = {
            aRefContext, nullptr, nullptr, nullptr, 0, aAppUnitsPerDevUnit
        };
        return MakeTextRun(aString, aLength, &params, aFlags, aMFR);
    }

    /**
     * Get the (possibly-cached) width of the hyphen character.
     * The aCtx and aAppUnitsPerDevUnit parameters will be used only if
     * needed to initialize the cached hyphen width; otherwise they are
     * ignored.
     */
    gfxFloat GetHyphenWidth(gfxTextRun::PropertyProvider* aProvider);

    /**
     * Make a text run representing a single hyphen character.
     * This will use U+2010 HYPHEN if available in the first font,
     * otherwise fall back to U+002D HYPHEN-MINUS.
     * The caller is responsible for deleting the returned text run
     * when no longer required.
     */
    gfxTextRun *MakeHyphenTextRun(gfxContext *aCtx,
                                  uint32_t aAppUnitsPerDevUnit);

    /**
     * Check whether a given font (specified by its gfxFontEntry)
     * is already in the fontgroup's list of actual fonts
     */
    bool HasFont(const gfxFontEntry *aFontEntry);

    // This returns the preferred underline for this font group.
    // Some CJK fonts have wrong underline offset in its metrics.
    // If this group has such "bad" font, each platform's gfxFontGroup
    // initialized mUnderlineOffset. The value should be lower value of
    // first font's metrics and the bad font's metrics. Otherwise, this
    // returns from first font's metrics.
    enum { UNDERLINE_OFFSET_NOT_SET = INT16_MAX };
    virtual gfxFloat GetUnderlineOffset();

    virtual already_AddRefed<gfxFont>
        FindFontForChar(uint32_t ch, uint32_t prevCh, uint32_t aNextCh,
                        int32_t aRunScript, gfxFont *aPrevMatchedFont,
                        uint8_t *aMatchType);

    gfxUserFontSet* GetUserFontSet();

    // With downloadable fonts, the composition of the font group can change as fonts are downloaded
    // for each change in state of the user font set, the generation value is bumped to avoid picking up
    // previously created text runs in the text run word cache.  For font groups based on stylesheets
    // with no @font-face rule, this always returns 0.
    uint64_t GetGeneration();

    // generation of the latest fontset rebuild, 0 when no fontset present
    uint64_t GetRebuildGeneration();

    // used when logging text performance
    gfxTextPerfMetrics *GetTextPerfMetrics() { return mTextPerf; }

    // This will call UpdateUserFonts() if the user font set is changed.
    void SetUserFontSet(gfxUserFontSet *aUserFontSet);

    // If there is a user font set, check to see whether the font list or any
    // caches need updating.
    virtual void UpdateUserFonts();

    // search for a specific userfont in the list of fonts
    bool ContainsUserFont(const gfxUserFontEntry* aUserFont);

    bool ShouldSkipDrawing() const {
        return mSkipDrawing;
    }

    class LazyReferenceContextGetter {
    public:
      virtual already_AddRefed<gfxContext> GetRefContext() = 0;
    };
    // The gfxFontGroup keeps ownership of this textrun.
    // It is only guaranteed to exist until the next call to GetEllipsisTextRun
    // (which might use a different appUnitsPerDev value or flags) for the font
    // group, or until UpdateUserFonts is called, or the fontgroup is destroyed.
    // Get it/use it/forget it :) - don't keep a reference that might go stale.
    gfxTextRun* GetEllipsisTextRun(int32_t aAppUnitsPerDevPixel, uint32_t aFlags,
                                   LazyReferenceContextGetter& aRefContextGetter);

    // helper method for resolving generic font families
    static void
    ResolveGenericFontNames(mozilla::FontFamilyType aGenericType,
                            nsIAtom *aLanguage,
                            nsTArray<nsString>& aGenericFamilies);

protected:
    // search through pref fonts for a character, return nullptr if no matching pref font
    already_AddRefed<gfxFont> WhichPrefFontSupportsChar(uint32_t aCh);

    already_AddRefed<gfxFont>
        WhichSystemFontSupportsChar(uint32_t aCh, uint32_t aNextCh,
                                    int32_t aRunScript);

    template<typename T>
    void ComputeRanges(nsTArray<gfxTextRange>& mRanges,
                       const T *aString, uint32_t aLength,
                       int32_t aRunScript, uint16_t aOrientation);

    class FamilyFace {
    public:
        FamilyFace() : mFamily(nullptr), mFontEntry(nullptr),
                       mNeedsBold(false), mFontCreated(false),
                       mLoading(false), mInvalid(false)
        { }

        FamilyFace(gfxFontFamily* aFamily, gfxFont* aFont)
            : mFamily(aFamily), mNeedsBold(false), mFontCreated(true),
              mLoading(false), mInvalid(false)
        {
            NS_ASSERTION(aFont, "font pointer must not be null");
            NS_ASSERTION(!aFamily ||
                         aFamily->ContainsFace(aFont->GetFontEntry()),
                         "font is not a member of the given family");
            mFont = aFont;
            NS_ADDREF(aFont);
        }

        FamilyFace(gfxFontFamily* aFamily, gfxFontEntry* aFontEntry,
                   bool aNeedsBold)
            : mFamily(aFamily), mNeedsBold(aNeedsBold), mFontCreated(false),
              mLoading(false), mInvalid(false)
        {
            NS_ASSERTION(aFontEntry, "font entry pointer must not be null");
            NS_ASSERTION(!aFamily ||
                         aFamily->ContainsFace(aFontEntry),
                         "font is not a member of the given family");
            mFontEntry = aFontEntry;
            NS_ADDREF(aFontEntry);
        }

        FamilyFace(const FamilyFace& aOtherFamilyFace)
            : mFamily(aOtherFamilyFace.mFamily),
              mNeedsBold(aOtherFamilyFace.mNeedsBold),
              mFontCreated(aOtherFamilyFace.mFontCreated),
              mLoading(aOtherFamilyFace.mLoading),
              mInvalid(aOtherFamilyFace.mInvalid)
        {
            if (mFontCreated) {
                mFont = aOtherFamilyFace.mFont;
                NS_ADDREF(mFont);
            } else {
                mFontEntry = aOtherFamilyFace.mFontEntry;
                NS_IF_ADDREF(mFontEntry);
            }
        }

        ~FamilyFace()
        {
            if (mFontCreated) {
                NS_RELEASE(mFont);
            } else {
                NS_IF_RELEASE(mFontEntry);
            }
        }

        FamilyFace& operator=(const FamilyFace& aOther)
        {
            if (mFontCreated) {
                NS_RELEASE(mFont);
            } else {
                NS_IF_RELEASE(mFontEntry);
            }

            mFamily = aOther.mFamily;
            mNeedsBold = aOther.mNeedsBold;
            mFontCreated = aOther.mFontCreated;
            mLoading = aOther.mLoading;
            mInvalid = aOther.mInvalid;

            if (mFontCreated) {
                mFont = aOther.mFont;
                NS_ADDREF(mFont);
            } else {
                mFontEntry = aOther.mFontEntry;
                NS_IF_ADDREF(mFontEntry);
            }

            return *this;
        }

        gfxFontFamily* Family() const { return mFamily.get(); }
        gfxFont* Font() const {
            return mFontCreated ? mFont : nullptr;
        }

        gfxFontEntry* FontEntry() const {
            return mFontCreated ? mFont->GetFontEntry() : mFontEntry;
        }

        bool NeedsBold() const { return mNeedsBold; }
        bool IsUserFontContainer() const {
            return FontEntry()->mIsUserFontContainer;
        }
        bool IsLoading() const { return mLoading; }
        bool IsInvalid() const { return mInvalid; }
        void CheckState(bool& aSkipDrawing);
        void SetLoading(bool aIsLoading) { mLoading = aIsLoading; }
        void SetInvalid() { mInvalid = true; }

        void SetFont(gfxFont* aFont)
        {
            NS_ASSERTION(aFont, "font pointer must not be null");
            NS_ADDREF(aFont);
            if (mFontCreated) {
                NS_RELEASE(mFont);
            } else {
                NS_IF_RELEASE(mFontEntry);
            }
            mFont = aFont;
            mFontCreated = true;
            mLoading = false;
        }

        bool EqualsUserFont(const gfxUserFontEntry* aUserFont) const;

    private:
        nsRefPtr<gfxFontFamily> mFamily;
        // either a font or a font entry exists
        union {
            gfxFont*            mFont;
            gfxFontEntry*       mFontEntry;
        };
        bool                    mNeedsBold   : 1;
        bool                    mFontCreated : 1;
        bool                    mLoading     : 1;
        bool                    mInvalid     : 1;
    };

    // List of font families, either named or generic.
    // Generic names map to system pref fonts based on language.
    mozilla::FontFamilyList mFamilyList;

    // Fontlist containing a font entry for each family found. gfxFont objects
    // are created as needed and userfont loads are initiated when needed.
    // Code should be careful about addressing this array directly.
    nsTArray<FamilyFace> mFonts;

    nsRefPtr<gfxFont> mDefaultFont;
    gfxFontStyle mStyle;

    gfxFloat mUnderlineOffset;
    gfxFloat mHyphenWidth;

    nsRefPtr<gfxUserFontSet> mUserFontSet;
    uint64_t mCurrGeneration;  // track the current user font set generation, rebuild font list if needed

    gfxTextPerfMetrics *mTextPerf;

    // Cache a textrun representing an ellipsis (useful for CSS text-overflow)
    // at a specific appUnitsPerDevPixel size and orientation
    nsAutoPtr<gfxTextRun>   mCachedEllipsisTextRun;

    // cache the most recent pref font to avoid general pref font lookup
    nsRefPtr<gfxFontFamily> mLastPrefFamily;
    nsRefPtr<gfxFont>       mLastPrefFont;
    eFontPrefLang           mLastPrefLang;       // lang group for last pref font
    eFontPrefLang           mPageLang;
    bool                    mLastPrefFirstFont;  // is this the first font in the list of pref fonts for this lang group?

    bool                    mSkipDrawing; // hide text while waiting for a font
                                          // download to complete (or fallback
                                          // timer to fire)

    // xxx - gfxPangoFontGroup skips UpdateUserFonts
    bool                    mSkipUpdateUserFonts;

    /**
     * Textrun creation short-cuts for special cases where we don't need to
     * call a font shaper to generate glyphs.
     */
    gfxTextRun *MakeEmptyTextRun(const Parameters *aParams, uint32_t aFlags);
    gfxTextRun *MakeSpaceTextRun(const Parameters *aParams, uint32_t aFlags);
    gfxTextRun *MakeBlankTextRun(uint32_t aLength,
                                 const Parameters *aParams, uint32_t aFlags);

    // Initialize the list of fonts
    void BuildFontList();

    // Get the font at index i within the fontlist.
    // Will initiate userfont load if not already loaded.
    // May return null if userfont not loaded or if font invalid
    virtual gfxFont* GetFontAt(int32_t i, uint32_t aCh = 0x20);

    // Whether there's a font loading for a given family in the fontlist
    // for a given character
    bool FontLoadingForFamily(gfxFontFamily* aFamily, uint32_t aCh) const;

    // will always return a font or force a shutdown
    gfxFont* GetDefaultFont();

    // Init this font group's font metrics. If there no bad fonts, you don't need to call this.
    // But if there are one or more bad fonts which have bad underline offset,
    // you should call this with the *first* bad font.
    void InitMetricsForBadFont(gfxFont* aBadFont);

    // Set up the textrun glyphs for an entire text run:
    // find script runs, and then call InitScriptRun for each
    template<typename T>
    void InitTextRun(gfxContext *aContext,
                     gfxTextRun *aTextRun,
                     const T *aString,
                     uint32_t aLength,
                     gfxMissingFontRecorder *aMFR);

    // InitTextRun helper to handle a single script run, by finding font ranges
    // and calling each font's InitTextRun() as appropriate
    template<typename T>
    void InitScriptRun(gfxContext *aContext,
                       gfxTextRun *aTextRun,
                       const T *aString,
                       uint32_t aScriptRunStart,
                       uint32_t aScriptRunEnd,
                       int32_t aRunScript,
                       gfxMissingFontRecorder *aMFR);

    // Helper for font-matching:
    // When matching the italic case, allow use of the regular face
    // if it supports a character but the italic one doesn't.
    // Return null if regular face doesn't support aCh
    already_AddRefed<gfxFont>
    FindNonItalicFaceForChar(gfxFontFamily* aFamily, uint32_t aCh);

    // helper methods for looking up fonts

    // iterate over the fontlist, lookup names and expand generics
    void EnumerateFontList(nsIAtom *aLanguage);

    // expand a generic to a list of specific names based on prefs
    void FindGenericFonts(mozilla::FontFamilyType aGenericType,
                          nsIAtom *aLanguage);

    // lookup and add a font with a given name (i.e. *not* a generic!)
    void FindPlatformFont(const nsAString& aName, bool aUseFontSet);

    static nsILanguageAtomService* gLangService;
};

// A "missing font recorder" is to be used during text-run creation to keep
// a record of any scripts encountered for which font coverage was lacking;
// when Flush() is called, it sends a notification that front-end code can use
// to download fonts on demand (or whatever else it wants to do).

#define GFX_MISSING_FONTS_NOTIFY_PREF "gfx.missing_fonts.notify"

class gfxMissingFontRecorder {
public:
    gfxMissingFontRecorder()
    {
        MOZ_COUNT_CTOR(gfxMissingFontRecorder);
        memset(&mMissingFonts, 0, sizeof(mMissingFonts));
    }

    ~gfxMissingFontRecorder()
    {
#ifdef DEBUG
        for (uint32_t i = 0; i < kNumScriptBitsWords; i++) {
            NS_ASSERTION(mMissingFonts[i] == 0,
                         "failed to flush the missing-font recorder");
        }
#endif
        MOZ_COUNT_DTOR(gfxMissingFontRecorder);
    }

    // record this script code in our mMissingFonts bitset
    void RecordScript(int32_t aScriptCode)
    {
        mMissingFonts[uint32_t(aScriptCode) >> 5] |=
            (1 << (uint32_t(aScriptCode) & 0x1f));
    }

    // send a notification of any missing-scripts that have been
    // recorded, and clear the mMissingFonts set for re-use
    void Flush();

    // forget any missing-scripts that have been recorded up to now;
    // called before discarding a recorder we no longer care about
    void Clear()
    {
        memset(&mMissingFonts, 0, sizeof(mMissingFonts));
    }

private:
    // Number of 32-bit words needed for the missing-script flags
    static const uint32_t kNumScriptBitsWords =
        ((MOZ_NUM_SCRIPT_CODES + 31) / 32);
    uint32_t mMissingFonts[kNumScriptBitsWords];
};

#endif
