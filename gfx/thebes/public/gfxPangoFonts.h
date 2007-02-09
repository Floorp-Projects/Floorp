/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Foundation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@mozilla.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
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

#ifndef GFX_PANGOFONTS_H
#define GFX_PANGOFONTS_H

#include "cairo.h"
#include "gfxTypes.h"
#include "gfxFont.h"

#include <pango/pango.h>
#include <X11/Xft/Xft.h>

// Control when we use Xft directly, bypassing Pango
// Enable this to use Xft to glyph-convert 8bit-only textruns, but use Pango
// to shape any textruns with non-8bit characters
#define ENABLE_XFT_FAST_PATH_8BIT
// Enable this to use Xft to glyph-convert all textruns
// #define ENABLE_XFT_FAST_PATH_ALWAYS

#include "nsDataHashtable.h"

class FontSelector;

class gfxPangoTextRun;

class gfxPangoFont : public gfxFont {
public:
    gfxPangoFont (const nsAString& aName,
                  const gfxFontStyle *aFontStyle);
    virtual ~gfxPangoFont ();

    virtual const gfxFont::Metrics& GetMetrics();

    PangoFontDescription *GetPangoFontDescription() { RealizeFont(); return mPangoFontDesc; }
    PangoContext *GetPangoContext() { RealizeFont(); return mPangoCtx; }

    void GetMozLang(nsACString &aMozLang);
    void GetActualFontFamily(nsACString &aFamily);
    PangoFont *GetPangoFont();

    XftFont *GetXftFont () { RealizeXftFont (); return mXftFont; }
    gfxFloat GetAdjustedSize() { RealizeFont(); return mAdjustedSize; }

    // These APIs need to be moved up to gfxFont with gfxPangoTextRun switched
    // to gfxTextRun
    /**
     * Draw a series of glyphs to aContext. The direction of aTextRun must
     * be honoured.
     * @param aStart the first character to draw
     * @param aEnd draw characters up to here
     * @param aBaselineOrigin the baseline origin; the left end of the baseline
     * for LTR textruns, the right end of the baseline for RTL textruns. On return,
     * this should be updated to the other end of the baseline. In application units.
     * @param aSpacing spacing to insert before and after characters (for RTL
     * glyphs, before-spacing is inserted to the right of characters). There
     * are aEnd - aStart elements in this array, unless it's null to indicate
     * that there is no spacing.
     * @param aDrawToPath when true, add the glyph outlines to the current path
     * instead of drawing the glyphs
     * 
     * Callers guarantee:
     * -- aStart and aEnd are aligned to cluster and ligature boundaries
     * -- all glyphs use this font
     */
    void Draw(gfxPangoTextRun *aTextRun, PRUint32 aStart, PRUint32 aEnd,
              gfxContext *aContext, PRBool aDrawToPath, gfxPoint *aBaselineOrigin,
              gfxTextRun::PropertyProvider::Spacing *aSpacing);
    /**
     * Measure a run of characters. See gfxTextRun::Metrics.
     * @param aTight if false, then return the union of the glyph extents
     * with the font-box for the characters (the rectangle with x=0,width=
     * the advance width for the character run,y=-(font ascent), and height=
     * font ascent + font descent). Otherwise, we must return as tight as possible
     * an approximation to the area actually painted by glyphs.
     * @param aSpacing spacing to insert before and after glyphs. The bounding box
     * need not include the spacing itself, but the spacing affects the glyph
     * positions. null if there is no spacing.
     * 
     * Callers guarantee:
     * -- aStart and aEnd are aligned to cluster and ligature boundaries
     * -- all glyphs use this font
     */
    gfxTextRun::Metrics Measure(gfxPangoTextRun *aTextRun,
                                PRUint32 aStart, PRUint32 aEnd,
                                PRBool aTightBoundingBox,
                                gfxTextRun::PropertyProvider::Spacing *aSpacing);
    /**
     * Line breaks have been changed at the beginning and/or end of a substring
     * of the text. Reshaping may be required; glyph updating is permitted.
     * @return true if anything was changed, false otherwise
     */
    PRBool NotifyLineBreaksChanged(gfxPangoTextRun *aTextRun,
                                   PRUint32 aStart, PRUint32 aLength)
    { return PR_FALSE; }

protected:
    PangoFontDescription *mPangoFontDesc;
    PangoContext *mPangoCtx;

    XftFont *mXftFont;
    cairo_scaled_font_t *mCairoFont;

    PRBool mHasMetrics;
    Metrics mMetrics;
    gfxFloat mAdjustedSize;

    void RealizeFont(PRBool force = PR_FALSE);
    void RealizeXftFont(PRBool force = PR_FALSE);
    void GetSize(const char *aString, PRUint32 aLength, gfxSize& inkSize, gfxSize& logSize);
    void SetupCairoFont(cairo_t *aCR);
};

class FontSelector;

class THEBES_API gfxPangoFontGroup : public gfxFontGroup {
public:
    gfxPangoFontGroup (const nsAString& families,
                       const gfxFontStyle *aStyle);
    virtual ~gfxPangoFontGroup ();

    virtual gfxFontGroup *Copy(const gfxFontStyle *aStyle);

    // Create and initialize a textrun using Pango (or Xft)
    virtual gfxTextRun *MakeTextRun(const PRUnichar *aString, PRUint32 aLength,
                                    Parameters *aParams);
    virtual gfxTextRun *MakeTextRun(const PRUint8 *aString, PRUint32 aLength,
                                    Parameters *aParams);

    gfxPangoFont *GetFontAt(PRInt32 i) {
        return NS_STATIC_CAST(gfxPangoFont*, 
                              NS_STATIC_CAST(gfxFont*, mFonts[i]));
    }

    gfxPangoFont *GetCachedFont(const nsAString& aName) const {
        nsRefPtr<gfxPangoFont> font;
        if (mFontCache.Get(aName, &font))
            return font;
        return nsnull;
    }

    void PutCachedFont(const nsAString& aName, gfxPangoFont *aFont) {
        mFontCache.Put(aName, aFont);
    }

    virtual gfxTextRun *GetSpecialStringTextRun(SpecialString aString,
                                                gfxTextRun *aTemplate);

protected:
    friend class FontSelector;

    // ****** Textrun glyph conversion helpers ******

    // If aUTF16Text is null, then the string contains no characters >= 0x100
    void InitTextRun(gfxPangoTextRun *aTextRun, const gchar *aUTF8Text,
                     PRUint32 aUTF8Length, PRUint32 aUTF8HeaderLength,
                     const PRUnichar *aUTF16Text, PRUint32 aUTF16Length);
    // Returns NS_ERROR_FAILURE if there's a missing glyph
    nsresult SetGlyphs(gfxPangoTextRun *aTextRun, const gchar *aUTF8,
                       PRUint32 aUTF8Length,
                       PRUint32 *aUTF16Offset, PangoGlyphString *aGlyphs,
                       PangoGlyphUnit aOverrideSpaceWidth,
                       PRBool aAbortOnMissingGlyph);
    // If aUTF16Text is null, then the string contains no characters >= 0x100.
    // Returns NS_ERROR_FAILURE if we require the itemizing path
    nsresult CreateGlyphRunsFast(gfxPangoTextRun *aTextRun,
                                 const gchar *aUTF8, PRUint32 aUTF8Length,
                                 const PRUnichar *aUTF16Text, PRUint32 aUTF16Length);
    void CreateGlyphRunsItemizing(gfxPangoTextRun *aTextRun,
                                  const gchar *aUTF8, PRUint32 aUTF8Length,
                                  PRUint32 aUTF8HeaderLength);
#if defined(ENABLE_XFT_FAST_PATH_8BIT) || defined(ENABLE_XFT_FAST_PATH_ALWAYS)
    void CreateGlyphRunsXft(gfxPangoTextRun *aTextRun,
                            const gchar *aUTF8, PRUint32 aUTF8Length);
#endif
    void SetupClusterBoundaries(gfxPangoTextRun *aTextRun,
                                const gchar *aUTF8, PRUint32 aUTF8Length,
                                PRUint32 aUTF16Offset, PangoAnalysis *aAnalysis);

    static PRBool FontCallback (const nsAString& fontName,
                                const nsACString& genericName,
                                void *closure);

private:
    nsDataHashtable<nsStringHashKey, nsRefPtr<gfxPangoFont> > mFontCache;
    nsTArray<gfxFontStyle> mAdditionalStyles;
    nsAutoPtr<gfxTextRun> mSpecialStrings[STRING_MAX + 1];
};

/**
 * This is not really Pango-specific anymore. We'll merge this into gfxTextRun
 * soon. At that point uses of gfxPangoFontGroup, gfxPangoFont and gfxPangoTextRun
 * will be converted to use the cross-platform superclasses.
 */
class THEBES_API gfxPangoTextRun : public gfxTextRun {
public:
    gfxPangoTextRun(gfxTextRunFactory::Parameters *aParams, PRUint32 aLength);
    // The caller is responsible for initializing our glyphs. Initially
    // all glyphs are such that GetCharacterGlyphs()[i].IsMissing() is true.

    // gfxTextRun public API
    virtual void GetCharFlags(PRUint32 aStart, PRUint32 aLength,
                              PRUint8 *aFlags);
    virtual PRUint8 GetCharFlags(PRUint32 aOffset);
    virtual PRUint32 GetLength();
    virtual PRBool SetPotentialLineBreaks(PRUint32 aStart, PRUint32 aLength,
                                          PRPackedBool *aBreakBefore);
    virtual void Draw(gfxContext *aContext, gfxPoint aPt,
                      PRUint32 aStart, PRUint32 aLength,
                      const gfxRect *aDirtyRect,
                      PropertyProvider *aBreakProvider,
                      gfxFloat *aAdvanceWidth);
    virtual void DrawToPath(gfxContext *aContext, gfxPoint aPt,
                            PRUint32 aStart, PRUint32 aLength,
                            PropertyProvider *aBreakProvider,
                            gfxFloat *aAdvanceWidth);
    virtual Metrics MeasureText(PRUint32 aStart, PRUint32 aLength,
                                PRBool aTightBoundingBox,
                                PropertyProvider *aBreakProvider);
    virtual void SetLineBreaks(PRUint32 aStart, PRUint32 aLength,
                               PRBool aLineBreakBefore, PRBool aLineBreakAfter,
                               TextProvider *aProvider,
                               gfxFloat *aAdvanceWidthDelta);
    virtual gfxFloat GetAdvanceWidth(PRUint32 aStart, PRUint32 aLength,
                                     PropertyProvider *aBreakProvider);
    virtual PRUint32 BreakAndMeasureText(PRUint32 aStart, PRUint32 aMaxLength,
                                         PRBool aLineBreakBefore, gfxFloat aWidth,
                                         PropertyProvider *aBreakProvider,
                                         PRBool aSuppressInitialBreak,
                                         Metrics *aMetrics, PRBool aTightBoundingBox,
                                         PRBool *aUsedHyphenation,
                                         PRUint32 *aLastBreak);

    /**
     * This class records the information associated with a character in the
     * input string. It's optimized for the case where there is one glyph
     * representing that character alone.
     */
    class CompressedGlyph {
    public:
        CompressedGlyph() { mValue = 0; }

        enum {
            // Indicates that a cluster starts at this character and can be
            // rendered using a single glyph with a reasonable advance offset
            // and no special glyph offset. A "reasonable" advance offset is
            // one that is a) a multiple of a pixel and b) fits in the available
            // bits (currently 14). We should revisit this, especially a),
            // if we want to support subpixel-aligned text.
            FLAG_IS_SIMPLE_GLYPH  = 0x80000000U,
            // Indicates that a linebreak is allowed before this character
            FLAG_CAN_BREAK_BEFORE = 0x40000000U,

            ADVANCE_MASK  = 0x3FFF0000U,
            ADVANCE_SHIFT = 16,

            GLYPH_MASK = 0x0000FFFFU,

            // Non-simple glyphs have the following tags
            
            TAG_MASK                  = 0x000000FFU,
            // Indicates that this character corresponds to a missing glyph
            // and should be skipped (or possibly, render the character
            // Unicode value in some special way)
            TAG_MISSING               = 0x00U,
            // Indicates that a cluster starts at this character and is rendered
            // using one or more glyphs which cannot be represented here.
            // Look up the DetailedGlyph table instead.
            TAG_COMPLEX_CLUSTER       = 0x01U,
            // Indicates that a cluster starts at this character but is rendered
            // as part of a ligature starting in a previous cluster.
            // NOTE: we divide up the ligature's width by the number of clusters
            // to get the width assigned to each cluster.
            TAG_LIGATURE_CONTINUATION = 0x21U,
            
            // Values where the upper 28 bits equal 0x80 are reserved for
            // non-cluster-start characters (see IsClusterStart below)
            
            // Indicates that a cluster does not start at this character, this is
            // a low UTF16 surrogate
            TAG_LOW_SURROGATE         = 0x80U,
            // Indicates that a cluster does not start at this character, this is
            // part of a cluster starting with an earlier character (but not
            // a low surrogate).
            TAG_CLUSTER_CONTINUATION  = 0x81U
        };

        // "Simple glyphs" have a simple glyph ID, simple advance and their
        // x and y offsets are zero. These cases are optimized to avoid storing
        // DetailedGlyphs.
        
        // Returns true if the glyph ID aGlyph fits into the compressed representation
        static PRBool IsSimpleGlyphID(PRUint32 aGlyph) {
            return (aGlyph & GLYPH_MASK) == aGlyph;
        }
        // Returns true if the advance aAdvance fits into the compressed representation.
        // aAdvance is in pixels.
        static PRBool IsSimpleAdvancePixels(PRUint32 aAdvance) {
            return (aAdvance & (ADVANCE_MASK >> ADVANCE_SHIFT)) == aAdvance;
        }

        PRBool IsSimpleGlyph() const { return (mValue & FLAG_IS_SIMPLE_GLYPH) != 0; }
        PRBool IsComplex(PRUint32 aTag) const { return (mValue & (FLAG_IS_SIMPLE_GLYPH|TAG_MASK))  == aTag; }
        PRBool IsMissing() const { return IsComplex(TAG_MISSING); }
        PRBool IsComplexCluster() const { return IsComplex(TAG_COMPLEX_CLUSTER); }
        PRBool IsLigatureContinuation() const { return IsComplex(TAG_LIGATURE_CONTINUATION); }
        PRBool IsLowSurrogate() const { return IsComplex(TAG_LOW_SURROGATE); }
        PRBool IsClusterStart() const { return (mValue & (FLAG_IS_SIMPLE_GLYPH|0x80U)) != 0x80U; }

        PRUint32 GetSimpleAdvance() const { return (mValue & ADVANCE_MASK) >> ADVANCE_SHIFT; }
        PRUint32 GetSimpleGlyph() const { return mValue & GLYPH_MASK; }

        PRUint32 GetComplexTag() const { return mValue & TAG_MASK; }

        PRBool CanBreakBefore() const { return (mValue & FLAG_CAN_BREAK_BEFORE) != 0; }
        // Returns FLAG_CAN_BREAK_BEFORE if the setting changed, 0 otherwise
        PRUint32 SetCanBreakBefore(PRBool aCanBreakBefore) {
            PRUint32 breakMask = aCanBreakBefore*FLAG_CAN_BREAK_BEFORE;
            PRUint32 toggle = breakMask ^ (mValue & FLAG_CAN_BREAK_BEFORE);
            mValue ^= toggle;
            return toggle;
        }

        CompressedGlyph& SetSimpleGlyph(PRUint32 aAdvancePixels, PRUint32 aGlyph) {
            NS_ASSERTION(IsSimpleAdvancePixels(aAdvancePixels), "Advance overflow");
            NS_ASSERTION(IsSimpleGlyphID(aGlyph), "Glyph overflow");
            mValue = (mValue & FLAG_CAN_BREAK_BEFORE) | FLAG_IS_SIMPLE_GLYPH |
                (aAdvancePixels << ADVANCE_SHIFT) | aGlyph;
            return *this;
        }
        CompressedGlyph& SetComplex(PRUint32 aTag) {
            mValue = (mValue & FLAG_CAN_BREAK_BEFORE) | aTag;
            return *this;
        }
        CompressedGlyph& SetMissing() { return SetComplex(TAG_MISSING); }
        CompressedGlyph& SetComplexCluster() { return SetComplex(TAG_COMPLEX_CLUSTER); }
        CompressedGlyph& SetLowSurrogate() { return SetComplex(TAG_LOW_SURROGATE); }
        CompressedGlyph& SetLigatureContinuation() { return SetComplex(TAG_LIGATURE_CONTINUATION); }
        CompressedGlyph& SetClusterContinuation() { return SetComplex(TAG_CLUSTER_CONTINUATION); }
    private:
        PRUint32 mValue;
    };
    struct DetailedGlyph {
        PRUint32 mIsLastGlyph:1;
        PRUint32 mGlyphID:31;
        float    mAdvance, mXOffset, mYOffset;
    };
    // The text is divided into GlyphRuns at Pango item and text segment boundaries
    struct GlyphRun {
        nsRefPtr<gfxPangoFont> mFont;   // never null
        PRUint32               mCharacterOffset; // into original UTF16 string
    };

    class GlyphRunIterator {
    public:
        GlyphRunIterator(gfxPangoTextRun *aTextRun, PRUint32 aStart, PRUint32 aLength)
          : mTextRun(aTextRun), mStartOffset(aStart), mEndOffset(aStart + aLength) {
            mNextIndex = mTextRun->FindFirstGlyphRunContaining(aStart);
        }
        PRBool NextRun();
        GlyphRun *GetGlyphRun() { return mGlyphRun; }
        PRUint32 GetStringStart() { return mStringStart; }
        PRUint32 GetStringEnd() { return mStringEnd; }
    private:
        gfxPangoTextRun *mTextRun;
        GlyphRun        *mGlyphRun;
        PRUint32         mStringStart;
        PRUint32         mStringEnd;
        PRUint32         mNextIndex;
        PRUint32         mStartOffset;
        PRUint32         mEndOffset;
    };

    friend class GlyphRunIterator;
    friend class FontSelector;

    // API for setting up the textrun glyphs

    /**
     * We've found a run of text that should use a particular font. Call this
     * only during initialization when font substitution has been computed.
     */
    nsresult AddGlyphRun(gfxPangoFont *aFont, PRUint32 aStartCharIndex);
    // Call the following glyph-setters during initialization or during reshaping
    // only. It is OK to overwrite existing data for a character.
    /**
     * Set the glyph for a character. Also allows you to set low surrogates,
     * cluster and ligature continuations.
     */
    void SetCharacterGlyph(PRUint32 aCharIndex, CompressedGlyph aGlyph) {
        if (mCharacterGlyphs) {
            mCharacterGlyphs[aCharIndex] = aGlyph;
        }
        if (mDetailedGlyphs) {
            mDetailedGlyphs[aCharIndex] = nsnull;
        }
    }
    /**
     * Set some detailed glyphs for a character. The data is copied from aGlyphs,
     * the caller retains ownership.
     */
    void SetDetailedGlyphs(PRUint32 aCharIndex, DetailedGlyph *aGlyphs,
                           PRUint32 aNumGlyphs);

    // API for access to the raw glyph data, needed by gfxPangoFont::Draw
    // and gfxPangoFont::GetBoundingBox
    const CompressedGlyph *GetCharacterGlyphs() { return mCharacterGlyphs; }
    const DetailedGlyph *GetDetailedGlyphs(PRUint32 aCharIndex) {
        NS_ASSERTION(mDetailedGlyphs && mDetailedGlyphs[aCharIndex],
                     "Requested detailed glyphs when there aren't any, "
                     "I think I'll go and have a lie down...");
        return mDetailedGlyphs[aCharIndex];
    }
    PRUint32 CountMissingGlyphs();

private:
    // **** general helpers **** 

    // Returns the index of the GlyphRun containing the given offset.
    // Returns mGlyphRuns.Length() when aOffset is mCharacterCount.
    PRUint32 FindFirstGlyphRunContaining(PRUint32 aOffset);
    // Computes the x-advance for a given cluster starting at aClusterOffset. Does
    // not include any spacing. Result is in device pixels.
    gfxFloat ComputeClusterAdvance(PRUint32 aClusterOffset);

    //  **** ligature helpers ****
    // (Platforms do the actual ligaturization, but we need to do a bunch of stuff
    // to handle requests that begin or end inside a ligature)

    struct LigatureData {
        PRUint32 mStartOffset;
        PRUint32 mEndOffset;
        PRUint32 mClusterCount;
        PRUint32 mPartClusterIndex;
        gfxFloat mLigatureWidth;  // appunits
        gfxFloat mBeforeSpacing;  // appunits
        gfxFloat mAfterSpacing;   // appunits
    };
    // if aProvider is null then mBeforeSpacing and mAfterSpacing are set to zero
    LigatureData ComputeLigatureData(PRUint32 aPartOffset, PropertyProvider *aProvider);
    void GetAdjustedSpacing(PRUint32 aStart, PRUint32 aEnd,
                            PropertyProvider *aProvider, PropertyProvider::Spacing *aSpacing);
    PRBool GetAdjustedSpacingArray(PRUint32 aStart, PRUint32 aEnd,
                                   PropertyProvider *aProvider,
                                   nsTArray<PropertyProvider::Spacing> *aSpacing);
    void DrawPartialLigature(gfxPangoFont *aFont, gfxContext *aCtx, PRUint32 aOffset,
                             const gfxRect *aDirtyRect, gfxPoint *aPt,
                             PropertyProvider *aProvider);
    // result in appunits
    void ShrinkToLigatureBoundaries(PRUint32 *aStart, PRUint32 *aEnd);
    gfxFloat GetPartialLigatureWidth(PRUint32 aStart, PRUint32 aEnd, PropertyProvider *aProvider);
    void AccumulatePartialLigatureMetrics(gfxPangoFont *aFont,
                                          PRUint32 aOffset, PRBool aTight,
                                          PropertyProvider *aProvider,
                                          Metrics *aMetrics);

    // **** measurement helper ****
    void AccumulateMetricsForRun(gfxPangoFont *aFont, PRUint32 aStart,
                                 PRUint32 aEnd, PRBool aTight,
                                 PropertyProvider *aProvider,
                                 Metrics *aMetrics);

    // **** drawing helper ****
    void DrawGlyphs(gfxPangoFont *aFont, gfxContext *aContext, PRBool aDrawToPath,
                    gfxPoint *aPt, PRUint32 aStart, PRUint32 aEnd,
                    PropertyProvider *aProvider);

    // All our glyph data is in logical order, not visual
    nsAutoArrayPtr<CompressedGlyph> mCharacterGlyphs;
    nsAutoArrayPtr<nsAutoArrayPtr<DetailedGlyph> > mDetailedGlyphs; // only non-null if needed
    // XXX this should be changed to a GlyphRun plus a maybe-null GlyphRun*,
    // for smaller size especially in the super-common one-glyphrun case
    nsAutoTArray<GlyphRun,1>        mGlyphRuns;
    PRUint32                        mCharacterCount;
};

#endif /* GFX_PANGOFONTS_H */
