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

protected:
    PangoFontDescription *mPangoFontDesc;
    PangoContext *mPangoCtx;

    XftFont *mXftFont;

    PRBool mHasMetrics;
    Metrics mMetrics;
    gfxFloat mAdjustedSize;

    void RealizeFont(PRBool force = PR_FALSE);
    void RealizeXftFont(PRBool force = PR_FALSE);
    void GetSize(const char *aString, PRUint32 aLength, gfxSize& inkSize, gfxSize& logSize);
};

class THEBES_API gfxPangoFontGroup : public gfxFontGroup {
public:
    gfxPangoFontGroup (const nsAString& families,
                       const gfxFontStyle *aStyle);
    virtual ~gfxPangoFontGroup ();

    virtual gfxFontGroup *Copy(const gfxFontStyle *aStyle);

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

    struct SpecialStringData {
        PangoGlyphString* mGlyphs;
        gfxFloat          mAdvance;
        
        SpecialStringData() { mGlyphs = nsnull; }
        ~SpecialStringData() { if (mGlyphs) pango_glyph_string_free(mGlyphs); }
    };
    SpecialStringData mSpecialStrings[gfxTextRun::STRING_MAX + 1];

protected:
    static PRBool FontCallback (const nsAString& fontName,
                                const nsACString& genericName,
                                void *closure);

private:
    nsDataHashtable<nsStringHashKey, nsRefPtr<gfxPangoFont> > mFontCache;
    nsTArray<gfxFontStyle> mAdditionalStyles;
};

struct TextSegment;

class THEBES_API gfxPangoTextRun : public gfxTextRun {
public:
    gfxPangoTextRun(gfxPangoFontGroup *aGroup,
                    const PRUint8 *aString, PRUint32 aLength,
                    gfxTextRunFactory::Parameters *aParams);
    gfxPangoTextRun(gfxPangoFontGroup *aGroup,
                    const PRUnichar *aString, PRUint32 aLength,
                    gfxTextRunFactory::Parameters *aParams);
    ~gfxPangoTextRun();

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
    virtual void DrawSpecialString(gfxContext *aContext, gfxPoint aPt,
                                   SpecialString aString);
    virtual Metrics MeasureText(PRUint32 aStart, PRUint32 aLength,
                                PRBool aTightBoundingBox,
                                PropertyProvider *aBreakProvider);
    virtual void SetLineBreaks(PRUint32 aStart, PRUint32 aLength,
                               PRBool aLineBreakBefore, PRBool aLineBreakAfter,
                               TextProvider *aProvider,
                               gfxFloat *aAdvanceWidthDelta);
    virtual Metrics MeasureTextSpecialString(SpecialString aString,
                                             PRBool aTightBoundingBox);
    virtual gfxFloat GetAdvanceWidth(PRUint32 aStart, PRUint32 aLength,
                                     PropertyProvider *aBreakProvider);
    virtual gfxFloat GetAdvanceWidthSpecialString(SpecialString aString);
    virtual gfxFont::Metrics GetDecorationMetrics();
    virtual PRUint32 BreakAndMeasureText(PRUint32 aStart, PRUint32 aMaxLength,
                                         PRBool aLineBreakBefore, gfxFloat aWidth,
                                         PropertyProvider *aBreakProvider,
                                         PRBool aSuppressInitialBreak,
                                         Metrics *aMetrics, PRBool aTightBoundingBox,
                                         PRBool *aUsedHyphenation,
                                         PRUint32 *aLastBreak);
    virtual void FlushSpacingCache(PRUint32 aStart);

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
            // Indicates that a cluster starts at this character and is rendered
            // using one or more glyphs which cannot be represented here.
            // Look up the DetailedGlyph table instead.
            TAG_COMPLEX_CLUSTER       = 0x00U,
            // Indicates that a cluster starts at this character but is rendered
            // as part of a ligature starting in a previous cluster.
            // NOTE: we divide up the ligature's width by the number of clusters
            // to get the width assigned to each cluster.
            TAG_LIGATURE_CONTINUATION = 0x01U,
            
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

        // Returns true if the glyph ID aGlyph fits into the compressed representation
        static PRBool IsSimpleGlyphID(PRUint32 aGlyph) {
            return (aGlyph & GLYPH_MASK) == aGlyph;
        }
        // Returns true if the advance aAdvance fits into the compressed representation
        static PRBool IsSimpleAdvance(PRUint32 aAdvance) {
            return (aAdvance & (ADVANCE_MASK >> ADVANCE_SHIFT)) == aAdvance;
        }

        PRBool IsSimpleGlyph() { return (mValue & FLAG_IS_SIMPLE_GLYPH) != 0; }
        PRBool IsComplex(PRUint32 aTag) { return (mValue & (FLAG_IS_SIMPLE_GLYPH|TAG_MASK))  == aTag; }
        PRBool IsComplexCluster() { return IsComplex(TAG_COMPLEX_CLUSTER); }
        PRBool IsLigatureContinuation() { return IsComplex(TAG_LIGATURE_CONTINUATION); }
        PRBool IsLowSurrogate() { return IsComplex(TAG_LOW_SURROGATE); }
        PRBool IsClusterStart() { return (mValue & (FLAG_IS_SIMPLE_GLYPH|0x80U)) != 0x80U; }

        PRUint32 GetSimpleAdvance() { return (mValue & ADVANCE_MASK) >> ADVANCE_SHIFT; }
        PRUint32 GetSimpleGlyph() { return mValue & GLYPH_MASK; }

        PRUint32 GetComplexTag() { return mValue & TAG_MASK; }

        PRBool CanBreakBefore() { return (mValue & FLAG_CAN_BREAK_BEFORE) != 0; }
        // Returns FLAG_CAN_BREAK_BEFORE if the setting changed, 0 otherwise
        PRUint32 SetCanBreakBefore(PRBool aCanBreakBefore) {
            PRUint32 breakMask = aCanBreakBefore*FLAG_CAN_BREAK_BEFORE;
            PRUint32 toggle = breakMask ^ (mValue & FLAG_CAN_BREAK_BEFORE);
            mValue ^= toggle;
            return toggle;
        }

        void SetSimpleGlyph(PRUint32 aAdvance, PRUint32 aGlyph) {
            NS_ASSERTION(IsSimpleAdvance(aAdvance), "Advance overflow");
            NS_ASSERTION(IsSimpleGlyphID(aGlyph), "Glyph overflow");
            mValue = (mValue & FLAG_CAN_BREAK_BEFORE) | FLAG_IS_SIMPLE_GLYPH |
              (aAdvance << ADVANCE_SHIFT) | aGlyph;
        }
        void SetComplex(PRUint32 aTag) { mValue = (mValue & FLAG_CAN_BREAK_BEFORE) | aTag; }
        void SetComplexCluster() { SetComplex(TAG_COMPLEX_CLUSTER); }
        void SetLowSurrogate() { SetComplex(TAG_LOW_SURROGATE); }
        void SetLigatureContinuation() { SetComplex(TAG_LIGATURE_CONTINUATION); }
        void SetClusterContinuation() { SetComplex(TAG_CLUSTER_CONTINUATION); }
    private:
        PRUint32 mValue;
    };
    struct DetailedGlyph {
        PRUint32 mIsLastGlyph:1;
        // mGlyphID == 2^31 means "missing glyph"
        PRUint32 mGlyphID:31;
        float    mAdvance, mXOffset, mYOffset;
                    
        // This is the ID of a missing glyph in a details record. All "missing" glyphs
        // are converted to a detailed glyph record with this glyph ID.
        enum {
            DETAILED_MISSING_GLYPH = 1U << 30
        };
    };
    // The text is divided into GlyphRuns at Pango item and text segment boundaries
    struct GlyphRun {
        PangoFont           *mPangoFont;       // strong ref; can't be null
        cairo_scaled_font_t *mCairoFont;       // could be null
        PRUint32             mCharacterOffset; // into original UTF16 string

        ~GlyphRun() {
            if (mCairoFont) {
                cairo_scaled_font_destroy(mCairoFont);
            }
            if (mPangoFont) {
                g_object_unref(mPangoFont);
            }
        }
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

    // **** Initialization helpers **** 

private:
    // If aUTF16Text is null, then the string contains no characters >= 0x100
    void Init(gfxTextRunFactory::Parameters *aParams, const gchar *aUTF8Text,
              PRUint32 aUTF8Length, PRUint32 aUTF8HeaderLength, const PRUnichar *aUTF16Text,
              PRUint32 aUTF16Length);
    void SetupClusterBoundaries(const gchar *aUTF8, PRUint32 aUTF8Length,
                                PRUint32 aUTF16Offset, PangoAnalysis *aAnalysis);
    nsresult AddGlyphRun(PangoFont *aFont, PRUint32 aUTF16Offset);
    DetailedGlyph *AllocateDetailedGlyphs(PRUint32 aIndex, PRUint32 aCount);
    // Returns NS_ERROR_FAILURE if there's a missing glyph
    nsresult SetGlyphs(const gchar *aUTF8, PRUint32 aUTF8Length,
                       PRUint32 *aUTF16Offset, PangoGlyphString *aGlyphs,
                       PangoGlyphUnit aOverrideSpaceWidth,
                       PRBool aAbortOnMissingGlyph);
    // If aUTF16Text is null, then the string contains no characters >= 0x100.
    // Returns NS_ERROR_FAILURE if we require the itemizing path
    nsresult CreateGlyphRunsFast(const gchar *aUTF8, PRUint32 aUTF8Length,
                                 const PRUnichar *aUTF16Text, PRUint32 aUTF16Length);
    void CreateGlyphRunsItemizing(const gchar *aUTF8, PRUint32 aUTF8Length,
                                  PRUint32 aUTF8HeaderLength);
#if defined(ENABLE_XFT_FAST_PATH_8BIT) || defined(ENABLE_XFT_FAST_PATH_ALWAYS)
    void CreateGlyphRunsXft(const gchar *aUTF8, PRUint32 aUTF8Length);
#endif
    // **** general helpers **** 

    void SetupPangoContextDirection();
    static void SetupCairoFont(cairo_t *aCR, GlyphRun *aGlyphRun);
    // Returns the index of the GlyphRun containing the given offset.
    // Returns mGlyphRuns.Length() when aOffset is mCharacterCount.
    PRUint32 FindFirstGlyphRunContaining(PRUint32 aOffset);
    // Computes the x-advance for a given cluster starting at aClusterOffset. Does
    // not include any spacing. Result is in device pixels.
    gfxFloat ComputeClusterAdvance(PRUint32 aClusterOffset);

    //  **** ligature helpers ****
    // (Pango does the actual ligaturization, but we need to do a bunch of stuff
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
    void DrawPartialLigature(gfxContext *aCtx, PRUint32 aOffset,
                             const gfxRect *aDirtyRect, gfxPoint *aPt,
                             PropertyProvider *aProvider);
    // result in appunits
    void ShrinkToLigatureBoundaries(PRUint32 *aStart, PRUint32 *aEnd);
    gfxFloat GetPartialLigatureWidth(PRUint32 aStart, PRUint32 aEnd, PropertyProvider *aProvider);
    void AccumulatePartialLigatureMetrics(PangoFont *aPangoFont,
                                          PRUint32 aOffset, PropertyProvider *aProvider,
                                          Metrics *aMetrics);

    // **** measurement helper ****
    void AccumulatePangoMetricsForRun(PangoFont *aPangoFont, PRUint32 aStart,
                                      PRUint32 aEnd, PropertyProvider *aProvider,
                                      Metrics *aMetrics);

    // **** drawing helpers ****

    typedef void (* CairoGlyphProcessorCallback)
        (void *aClosure, cairo_glyph_t *aGlyphs, int aNumGlyphs);
    void ProcessCairoGlyphsWithSpacing(CairoGlyphProcessorCallback aCB, void *aClosure,
                                       gfxPoint *aPt, PRUint32 aStart, PRUint32 aEnd,
                                       PropertyProvider::Spacing *aSpacing);
    void ProcessCairoGlyphs(CairoGlyphProcessorCallback aCB, void *aClosure,
                            gfxPoint *aPt, PRUint32 aStart, PRUint32 aEnd,
                            PropertyProvider *aProvider);
    static void CairoShowGlyphs(void *aClosure, cairo_glyph_t *aGlyphs, int aNumGlyphs);
    static void CairoGlyphsToPath(void *aClosure, cairo_glyph_t *aGlyphs, int aNumGlyphs);

    nsRefPtr<gfxPangoFontGroup>     mFontGroup;
    // All our glyph data is in logical order, not visual
    nsAutoArrayPtr<CompressedGlyph> mCharacterGlyphs;
    nsAutoArrayPtr<nsAutoArrayPtr<DetailedGlyph> > mDetailedGlyphs; // only non-null if needed
    nsAutoTArray<GlyphRun,1>        mGlyphRuns;
    PRUint32                        mCharacterCount;
};

#endif /* GFX_PANGOFONTS_H */
