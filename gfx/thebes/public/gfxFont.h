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
 *   Stuart Parmenter <stuart@mozilla.com>
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

#ifndef GFX_FONT_H
#define GFX_FONT_H

#include "prtypes.h"
#include "gfxTypes.h"
#include "nsString.h"
#include "gfxPoint.h"
#include "nsTArray.h"
#include "gfxSkipChars.h"
#include "gfxRect.h"

class gfxContext;
class gfxTextRun;
class nsIAtom;
class gfxFontGroup;
typedef struct _cairo cairo_t;

#define FONT_STYLE_NORMAL              0
#define FONT_STYLE_ITALIC              1
#define FONT_STYLE_OBLIQUE             2

#define FONT_VARIANT_NORMAL            0
#define FONT_VARIANT_SMALL_CAPS        1

#define FONT_DECORATION_NONE           0x0
#define FONT_DECORATION_UNDERLINE      0x1
#define FONT_DECORATION_OVERLINE       0x2
#define FONT_DECORATION_STRIKEOUT      0x4

#define FONT_WEIGHT_NORMAL             400
#define FONT_WEIGHT_BOLD               700

struct THEBES_API gfxFontStyle {
    gfxFontStyle(PRUint8 aStyle, PRUint8 aVariant,
                 PRUint16 aWeight, PRUint8 aDecoration, gfxFloat aSize,
                 const nsACString& aLangGroup,
                 float aSizeAdjust, PRPackedBool aSystemFont,
                 PRPackedBool aFamilyNameQuirks);

    // The style of font (normal, italic, oblique)
    PRUint8 style : 7;

    // Say that this font is a system font and therefore does not
    // require certain fixup that we do for fonts from untrusted
    // sources.
    PRPackedBool systemFont : 1;

    // The variant of the font (normal, small-caps)
    PRUint8 variant : 7;

    // True if the character set quirks (for treatment of "Symbol",
    // "Wingdings", etc.) should be applied.
    PRPackedBool familyNameQuirks : 1;
    
    // The weight of the font.  100, 200, ... 900 are the weights, and
    // single integer offsets request the next bolder/lighter font
    // available.  For example, for a font available in weights 200,
    // 400, 700, and 900, a weight of 898 should lead to the weight 400
    // font being used, since it is two weights lighter than 900.
    PRUint16 weight;

    // The decorations on the font (underline, overline,
    // line-through). The decorations can be binary or'd together.
    PRUint8 decorations;

    // The logical size of the font, in pixels
    gfxFloat size;

    // the language group
    nsCString langGroup;

    // The aspect-value (ie., the ratio actualsize:actualxheight) that any
    // actual physical font created from this font structure must have when
    // rendering or measuring a string. A value of 0 means no adjustment
    // needs to be done.
    float sizeAdjust;

    void ComputeWeightAndOffset(PRInt8 *outBaseWeight,
                                PRInt8 *outOffset) const;

    PRBool Equals(const gfxFontStyle& other) const {
        return (size == other.size) &&
            (style == other.style) &&
            (systemFont == other.systemFont) &&
            (variant == other.variant) &&
            (familyNameQuirks == other.familyNameQuirks) &&
            (weight == other.weight) &&
            (decorations == other.decorations) &&
            (langGroup.Equals(other.langGroup)) &&
            (sizeAdjust == other.sizeAdjust);
    }
};



/* a SPECIFIC single font family */
class THEBES_API gfxFont {
    THEBES_INLINE_DECL_REFCOUNTING(gfxFont)

public:
    gfxFont(const nsAString &aName, const gfxFontStyle *aFontGroup);
    virtual ~gfxFont() {}

    const nsString& GetName() const { return mName; }
    const gfxFontStyle *GetStyle() const { return mStyle; }

    virtual nsString GetUniqueName() = 0;

    // Font metrics
    struct Metrics {
        gfxFloat xHeight;
        gfxFloat superscriptOffset;
        gfxFloat subscriptOffset;
        gfxFloat strikeoutSize;
        gfxFloat strikeoutOffset;
        gfxFloat underlineSize;
        gfxFloat underlineOffset;
        gfxFloat height;

        gfxFloat internalLeading;
        gfxFloat externalLeading;

        gfxFloat emHeight;
        gfxFloat emAscent;
        gfxFloat emDescent;
        gfxFloat maxHeight;
        gfxFloat maxAscent;
        gfxFloat maxDescent;
        gfxFloat maxAdvance;

        gfxFloat aveCharWidth;
        gfxFloat spaceWidth;
    };
    virtual const gfxFont::Metrics& GetMetrics() = 0;

    /**
     * We let layout specify spacing on either side of any
     * character. We need to specify both before and after
     * spacing so that substring measurement can do the right things.
     * These values are in appunits. They're always an integral number of
     * appunits, but we specify them in floats in case very large spacing
     * values are required.
     */
    struct Spacing {
        gfxFloat mBefore;
        gfxFloat mAfter;
    };
    /**
     * Metrics for a particular string
     */
    struct RunMetrics {
        RunMetrics() {
            mAdvanceWidth = mAscent = mDescent = 0.0;
            mBoundingBox = gfxRect(0,0,0,0);
            mClusterCount = 0;
        }

        void CombineWith(const RunMetrics& aOtherOnRight) {
            mAscent = PR_MAX(mAscent, aOtherOnRight.mAscent);
            mDescent = PR_MAX(mDescent, aOtherOnRight.mDescent);
            mBoundingBox =
                mBoundingBox.Union(aOtherOnRight.mBoundingBox + gfxPoint(mAdvanceWidth, 0));
            mAdvanceWidth += aOtherOnRight.mAdvanceWidth;
            mClusterCount += aOtherOnRight.mClusterCount;
        }

        // can be negative (partly due to negative spacing).
        // Advance widths should be additive: the advance width of the
        // (offset1, length1) plus the advance width of (offset1 + length1,
        // length2) should be the advance width of (offset1, length1 + length2)
        gfxFloat mAdvanceWidth;
        
        // For zero-width substrings, these must be zero!
        gfxFloat mAscent;  // always non-negative
        gfxFloat mDescent; // always non-negative
        
        // Bounding box that is guaranteed to include everything drawn.
        // If aTightBoundingBox was set to true when these metrics were
        // generated, this will tightly wrap the glyphs, otherwise it is
        // "loose" and may be larger than the true bounding box.
        // Coordinates are relative to the baseline left origin, so typically
        // mBoundingBox.y == -mAscent
        gfxRect  mBoundingBox;
        
        // Count of the number of grapheme clusters. Layout needs
        // this to compute tab offsets. For SpecialStrings, this is always 1.
        PRInt32  mClusterCount;
    };

    /**
     * Draw a series of glyphs to aContext. The direction of aTextRun must
     * be honoured.
     * @param aStart the first character to draw
     * @param aEnd draw characters up to here
     * @param aBaselineOrigin the baseline origin; the left end of the baseline
     * for LTR textruns, the right end of the baseline for RTL textruns. On return,
     * this should be updated to the other end of the baseline. In application
     * units, really!
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
     * 
     * The default implementation builds a cairo glyph array and
     * calls cairo_show_glyphs or cairo_glyph_path.
     */
    virtual void Draw(gfxTextRun *aTextRun, PRUint32 aStart, PRUint32 aEnd,
                      gfxContext *aContext, PRBool aDrawToPath, gfxPoint *aBaselineOrigin,
                      Spacing *aSpacing);
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
     * 
     * The default implementation just uses font metrics and aTextRun's
     * advances, and assumes no characters fall outside the font box. In
     * general this is insufficient, because that assumption is not always true.
     */
    virtual RunMetrics Measure(gfxTextRun *aTextRun,
                               PRUint32 aStart, PRUint32 aEnd,
                               PRBool aTightBoundingBox,
                               Spacing *aSpacing);
    /**
     * Line breaks have been changed at the beginning and/or end of a substring
     * of the text. Reshaping may be required; glyph updating is permitted.
     * @return true if anything was changed, false otherwise
     */
    PRBool NotifyLineBreaksChanged(gfxTextRun *aTextRun,
                                   PRUint32 aStart, PRUint32 aLength)
    { return PR_FALSE; }

protected:
    // The family name of the font
    nsString mName;

    // This is called by the default Draw() implementation above.
    virtual void SetupCairoFont(cairo_t *aCR) = 0;

    const gfxFontStyle *mStyle;
};

class THEBES_API gfxTextRunFactory {
    THEBES_INLINE_DECL_REFCOUNTING(gfxTextRunFactory)

public:
    // Flags in the mask 0xFFFF0000 are reserved for textrun clients
    // Flags in the mask 0x0000F000 are reserved for per-platform fonts
    // Flags in the mask 0x00000FFF are set by the textrun creator.
    enum {
        /**
         * When set, the text string pointer used to create the text run
         * is guaranteed to be available during the lifetime of the text run.
         */
        TEXT_IS_PERSISTENT           = 0x0001,
        /**
         * When set, the text is known to be all-ASCII (< 128).
         */
        TEXT_IS_ASCII                = 0x0002,
        /**
         * When set, the text is RTL.
         */
        TEXT_IS_RTL                  = 0x0004,
        /**
         * When set, spacing is enabled and the textrun needs to call GetSpacing
         * on the spacing provider.
         */
        TEXT_ENABLE_SPACING          = 0x0008,
        /**
         * When set, GetSpacing can return negative spacing.
         */
        TEXT_ENABLE_NEGATIVE_SPACING = 0x0010,
        /**
         * When set, mAfter spacing for a character already includes the character
         * width. Otherwise, it does not include the character width.
         */
        TEXT_ABSOLUTE_SPACING        = 0x0020,
        /**
         * When set, GetHyphenationBreaks may return true for some character
         * positions, otherwise it will always return false for all characters.
         */
        TEXT_ENABLE_HYPHEN_BREAKS    = 0x0040,
        /**
         * When set, the text has no characters above 255.
         */
        TEXT_IS_8BIT                 = 0x0080,
        /**
         * When set, the text may have UTF16 surrogate pairs, otherwise it
         * doesn't.
         */
        TEXT_HAS_SURROGATES          = 0x0100,
        /**
         * When set, the RunMetrics::mBoundingBox field will be initialized
         * properly based on glyph extents, in particular, glyph extents that
         * overflow the standard font-box (the box defined by the ascent, descent
         * and advance width of the glyph). When not set, it may just be the
         * standard font-box even if glyphs overflow.
         */
        TEXT_NEED_BOUNDING_BOX       = 0x0200
    };

    /**
     * This record contains all the parameters needed to initialize a textrun.
     */
    struct Parameters {
        // A reference context suggesting where the textrun will be rendered
        gfxContext   *mContext;
        // Pointer to arbitrary user data (which should outlive the textrun)
        void         *mUserData;
        // The language of the text, or null if not known
        nsIAtom      *mLangGroup;
        // A description of which characters have been stripped from the original
        // DOM string to produce the characters in the textrun. May be null
        // if that information is not relevant.
        gfxSkipChars *mSkipChars;
        // A list of where linebreaks are currently placed in the textrun. May
        // be null if mInitialBreakCount is zero.
        PRUint32     *mInitialBreaks;
        PRUint32      mInitialBreakCount;
        // The ratio to use to convert device pixels to application layout units
        PRUint32      mAppUnitsPerDevUnit;
        // Flags --- see above
        PRUint32      mFlags;
    };

    virtual ~gfxTextRunFactory() {}

    /**
     * Create a gfxTextRun from Unicode text. The length is obtained from
     * aParams->mSkipChars->GetCharCount().
     */
    virtual gfxTextRun *MakeTextRun(const PRUnichar *aString, PRUint32 aLength,
                                    Parameters *aParams) = 0;
    /**
     * Create a gfxTextRun from 8-bit Unicode (UCS1?) text. The length is
     * obtained from aParams->mSkipChars->GetCharCount().
     */
    virtual gfxTextRun *MakeTextRun(const PRUint8 *aString, PRUint32 aLength,
                                    Parameters *aParams) = 0;
};

/**
 * gfxTextRun is an abstraction for drawing and measuring substrings of a run
 * of text. It stores runs of positioned glyph data, each run having a single
 * gfxFont. The glyphs are associated with a string of source text, and the
 * gfxTextRun APIs take parameters that are offsets into that source text.
 * 
 * \r and \n characters must be ignored completely. Substring operations
 * will not normally include these characters.
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
 * gfxTextRuns don't need to remember their text ... often it's enough just to
 * convert text to glyphs and store glyphs plus some geometry data in packed
 * form. However sometimes gfxTextRuns will want to get the original text back
 * to handle some unusual situation. So gfxTextRuns have two modes: "not
 * remembering text" (initial state) and "remembering text". A call to
 * gfxTextRun::RememberText forces a transition from the former to latter state.
 * The text is never forgotten. A gfxTextRun call that receives a PropertyProvider
 * object may call ForceRememberText to request a transition to "remembering text".
 * 
 * It is important that zero-length substrings are handled correctly. This will
 * be on the test!
 * 
 * gfxTextRun stores a list of zero or more glyphs for each character. For each
 * glyph we store the glyph ID, the advance, and possibly an xoffset and yoffset.
 * The idea is that a string is rendered by a loop that draws each glyph
 * at its designated offset from the current point, then advances the current
 * point by the glyph's advance in the direction of the textrun (LTR or RTL).
 * Each glyph advance is always rounded to the nearest appunit; this ensures
 * consistent results when dividing the text in a textrun into multiple text
 * frames (frame boundaries are always aligned to appunits). We optimize
 * for the case where a character has a single glyph and zero xoffset and yoffset,
 * and the glyph ID and advance are in a reasonable range so we can pack all
 * necessary data into 32 bits.
 */
class THEBES_API gfxTextRun {
public:
    ~gfxTextRun() {}

    typedef gfxFont::RunMetrics Metrics;

    // Public textrun API for general use

    PRBool IsClusterStart(PRUint32 aPos) {
        NS_ASSERTION(0 <= aPos && aPos < mCharacterCount, "aPos out of range");
        return mCharacterGlyphs[aPos].IsClusterStart();
    }
    PRBool IsLigatureContinuation(PRUint32 aPos) {
        NS_ASSERTION(0 <= aPos && aPos < mCharacterCount, "aPos out of range");
        return mCharacterGlyphs[aPos].IsLigatureContinuation();
    }
    PRBool CanBreakLineBefore(PRUint32 aPos) {
        NS_ASSERTION(0 <= aPos && aPos < mCharacterCount, "aPos out of range");
        return mCharacterGlyphs[aPos].CanBreakBefore();
    }    

    PRUint32 GetLength() { return mCharacterCount; }

    // All PRUint32 aStart, PRUint32 aLength ranges below are restricted to
    // grapheme cluster boundaries! All offsets are in terms of the string
    // passed into MakeTextRun.
    
    // All coordinates are in layout/app units

    /**
     * This can be called to force gfxTextRun to remember the text used
     * to create it and *never* call PropertyProvider::GetText again.
     * 
     * Right now we don't implement these.
     */
    void RememberText(const PRUnichar *aText, PRUint32 aLength) {}
    void RememberText(const PRUint8 *aText, PRUint32 aLength) {}

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
    virtual PRBool SetPotentialLineBreaks(PRUint32 aStart, PRUint32 aLength,
                                          PRPackedBool *aBreakBefore);

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
        /**
         * Recover the text originally used to build the textrun. This should
         * only be requested infrequently as it may be slow. If you need to
         * call it a lot you should probably be saving the text in the text run
         * itself. It just forces the textrun user to call RememberText on the
         * text run. If you call this and RememberText doesn't get called,
         * then something has failed and you should handle it.
         */
        virtual void ForceRememberText() = 0;

        // Detect hyphenation break opportunities in the given range; breaks
        // not at cluster boundaries will be ignored.
        virtual void GetHyphenationBreaks(PRUint32 aStart, PRUint32 aLength,
                                          PRPackedBool *aBreakBefore) = 0;

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
        virtual void GetSpacing(PRUint32 aStart, PRUint32 aLength,
                                Spacing *aSpacing) = 0;
    };

    /**
     * Draws a substring. Uses only GetSpacing from aBreakProvider.
     * The provided point is the baseline origin on the left of the string
     * for LTR, on the right of the string for RTL.
     * @param aDirtyRect if non-null, drawing outside of the rectangle can be
     * (but does not need to be) dropped. Note that if this is null, we cannot
     * draw partial ligatures and we will assert if partial ligatures
     * are detected.
     * @param aAdvanceWidth if non-null, the advance width of the substring
     * is returned here.
     * 
     * Drawing should respect advance widths in the sense that for LTR runs,
     * Draw(ctx, pt, offset1, length1, dirty, &provider, &advance) followed by
     * Draw(ctx, gfxPoint(pt.x + advance, pt.y), offset1 + length1, length2,
     *      dirty, &provider, nsnull) should have the same effect as
     * Draw(ctx, pt, offset1, length1+length2, dirty, &provider, nsnull).
     * For RTL runs the rule is:
     * Draw(ctx, pt, offset1 + length1, length2, dirty, &provider, &advance) followed by
     * Draw(ctx, gfxPoint(pt.x + advance, pt.y), offset1, length1,
     *      dirty, &provider, nsnull) should have the same effect as
     * Draw(ctx, pt, offset1, length1+length2, dirty, &provider, nsnull).
     * 
     * Glyphs should be drawn in logical content order, which can be significant
     * if they overlap (perhaps due to negative spacing).
     */
    void Draw(gfxContext *aContext, gfxPoint aPt,
              PRUint32 aStart, PRUint32 aLength,
              const gfxRect *aDirtyRect,
              PropertyProvider *aProvider,
              gfxFloat *aAdvanceWidth);

    /**
     * Renders a substring to a path. Uses only GetSpacing from aBreakProvider.
     * The provided point is the baseline origin on the left of the string
     * for LTR, on the right of the string for RTL.
     * @param aAdvanceWidth if non-null, the advance width of the substring
     * is returned here.
     * 
     * Drawing should respect advance widths in the way that Draw above does.
     * 
     * Glyphs should be drawn in logical content order.
     * 
     * UNLIKE Draw above, this cannot be used to render substrings that start or
     * end inside a ligature.
     */
    void DrawToPath(gfxContext *aContext, gfxPoint aPt,
                    PRUint32 aStart, PRUint32 aLength,
                    PropertyProvider *aBreakProvider,
                    gfxFloat *aAdvanceWidth);

    /**
     * Computes the ReflowMetrics for a substring.
     * Uses GetSpacing from aBreakProvider.
     * @param aTightBoundingBox if true, we make the bounding box tight
     */
    Metrics MeasureText(PRUint32 aStart, PRUint32 aLength,
                        PRBool aTightBoundingBox,
                        PropertyProvider *aProvider);

    /**
     * Computes just the advance width for a substring.
     * Uses GetSpacing from aBreakProvider.
     */
    gfxFloat GetAdvanceWidth(PRUint32 aStart, PRUint32 aLength,
                             PropertyProvider *aProvider);

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
    virtual PRBool SetLineBreaks(PRUint32 aStart, PRUint32 aLength,
                                 PRBool aLineBreakBefore, PRBool aLineBreakAfter,
                                 PropertyProvider *aProvider,
                                 gfxFloat *aAdvanceWidthDelta);

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
     * @param aMaxLength this can be PR_UINT32_MAX, in which case the length used
     * is up to the end of the string
     * @param aLineBreakBefore set to true if and only if there is an actual
     * line break at the start of this string.
     * @param aSuppressInitialBreak if true, then we assume there is no possible
     * linebreak before aStart. If false, then we will check the internal
     * line break opportunity state before deciding whether to return 0 as the
     * character to break before.
     * @param aMetrics if non-null, we fill this in for the returned substring.
     * If a hyphenation break was used, the hyphen is NOT included in the returned metrics.
     * @param aTightBoundingBox if true, we make the bounding box in aMetrics tight
     * @param aUsedHyphenation if non-null, records if we selected a hyphenation break
     * @param aLastBreak if non-null and result is aMaxLength, we set this to
     * the maximal N such that
     *       N < aMaxLength && line break at N && GetAdvanceWidth(aStart, N) <= aWidth
     *   OR  N < aMaxLength && hyphen break at N && GetAdvanceWidth(aStart, N) + GetHyphenWidth() <= aWidth
     * or PR_UINT32_MAX if no such N exists, where GetAdvanceWidth assumes
     * the effect of
     * SetLineBreaks(aStart, N, aLineBreakBefore, N < aMaxLength, aProvider)
     * 
     * Note that negative advance widths are possible especially if negative
     * spacing is provided.
     */
    PRUint32 BreakAndMeasureText(PRUint32 aStart, PRUint32 aMaxLength,
                                 PRBool aLineBreakBefore, gfxFloat aWidth,
                                 PropertyProvider *aProvider,
                                 PRBool aSuppressInitialBreak,
                                 Metrics *aMetrics, PRBool aTightBoundingBox,
                                 PRBool *aUsedHyphenation,
                                 PRUint32 *aLastBreak);

    /**
     * Update the reference context.
     * XXX this is a hack. New text frame does not call this. Use only
     * temporarily for old text frame.
     */
    void SetContext(gfxContext *aContext) {}

    // Utility getters

    PRBool IsRightToLeft() const { return (mFlags & gfxTextRunFactory::TEXT_IS_RTL) != 0; }
    gfxFloat GetDirection() const { return (mFlags & gfxTextRunFactory::TEXT_IS_RTL) ? -1.0 : 1.0; }
    void *GetUserData() const { return mUserData; }
    PRUint32 GetFlags() const { return mFlags; }
    const gfxSkipChars& GetSkipChars() const { return mSkipChars; }
    PRUint32 GetAppUnitsPerDevUnit() const { return mAppUnitsPerDevUnit; }

    // The caller is responsible for initializing our glyphs after construction.
    // Initially all glyphs are such that GetCharacterGlyphs()[i].IsMissing() is true.
    gfxTextRun(gfxTextRunFactory::Parameters *aParams, PRUint32 aLength);

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

            // The advance is stored in appunits
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
        // x and y offsets are zero. Also the glyph extents do not overflow
        // the font-box defined by the font ascent, descent and glyph advance width.
        // These case is optimized to avoid storing DetailedGlyphs.

        // Returns true if the glyph ID aGlyph fits into the compressed representation
        static PRBool IsSimpleGlyphID(PRUint32 aGlyph) {
            return (aGlyph & GLYPH_MASK) == aGlyph;
        }
        // Returns true if the advance aAdvance fits into the compressed representation.
        // aAdvance is in pixels.
        static PRBool IsSimpleAdvance(PRUint32 aAdvance) {
            return (aAdvance & (ADVANCE_MASK >> ADVANCE_SHIFT)) == aAdvance;
        }

        PRBool IsSimpleGlyph() const { return (mValue & FLAG_IS_SIMPLE_GLYPH) != 0; }
        PRBool IsComplex(PRUint32 aTag) const { return (mValue & (FLAG_IS_SIMPLE_GLYPH|TAG_MASK))  == aTag; }
        PRBool IsMissing() const { return IsComplex(TAG_MISSING); }
        PRBool IsComplexCluster() const { return IsComplex(TAG_COMPLEX_CLUSTER); }
        PRBool IsLigatureContinuation() const { return IsComplex(TAG_LIGATURE_CONTINUATION); }
        PRBool IsClusterContinuation() const { return IsComplex(TAG_CLUSTER_CONTINUATION); }
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
            NS_ASSERTION(IsSimpleAdvance(aAdvancePixels), "Advance overflow");
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

    /**
     * When the glyphs for a character don't fit into a CompressedGlyph record
     * in SimpleGlyph format, we use an array of DetailedGlyphs instead.
     */
    struct DetailedGlyph {
        /** This is true for the last DetailedGlyph in the array. This lets
         * us track the length of the array. */
        PRUint32 mIsLastGlyph:1;
        PRUint32 mGlyphID:31;
        // The advance, x-offset and y-offset of the glyph, in appunits
        PRInt32  mAdvance;
        float    mXOffset, mYOffset;
    };

    // The text is divided into GlyphRuns as necessary
    struct GlyphRun {
        nsRefPtr<gfxFont> mFont;   // never null
        PRUint32          mCharacterOffset; // into original UTF16 string
    };

    class GlyphRunIterator {
    public:
        GlyphRunIterator(gfxTextRun *aTextRun, PRUint32 aStart, PRUint32 aLength)
          : mTextRun(aTextRun), mStartOffset(aStart), mEndOffset(aStart + aLength) {
            mNextIndex = mTextRun->FindFirstGlyphRunContaining(aStart);
        }
        PRBool NextRun();
        GlyphRun *GetGlyphRun() { return mGlyphRun; }
        PRUint32 GetStringStart() { return mStringStart; }
        PRUint32 GetStringEnd() { return mStringEnd; }
    private:
        gfxTextRun *mTextRun;
        GlyphRun   *mGlyphRun;
        PRUint32    mStringStart;
        PRUint32    mStringEnd;
        PRUint32    mNextIndex;
        PRUint32    mStartOffset;
        PRUint32    mEndOffset;
    };

    friend class GlyphRunIterator;
    friend class FontSelector;

    // API for setting up the textrun glyphs. Should only be called by
    // things that construct textruns.
    /**
     * Record every character that is the second half of a surrogate pair.
     * This should be called after creating a Unicode textrun.
     */
    void RecordSurrogates(const PRUnichar *aString);
    /**
     * We've found a run of text that should use a particular font. Call this
     * only during initialization when font substitution has been computed.
     */
    nsresult AddGlyphRun(gfxFont *aFont, PRUint32 aStartCharIndex);
    void ResetGlyphRuns() { mGlyphRuns.Clear(); }
    // Call the following glyph-setters during initialization or during reshaping
    // only. It is OK to overwrite existing data for a character.
    /**
     * Set the glyph for a character. Also allows you to set low surrogates,
     * cluster and ligature continuations.
     */
    void SetCharacterGlyph(PRUint32 aCharIndex, CompressedGlyph aGlyph) {
        NS_ASSERTION(aCharIndex > 0 ||
                     (aGlyph.IsClusterStart() && !aGlyph.IsLigatureContinuation()),
                     "First character must be the start of a cluster and can't be a ligature continuation!");
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
    void SetDetailedGlyphs(PRUint32 aCharIndex, const DetailedGlyph *aGlyphs,
                           PRUint32 aNumGlyphs);

    // API for access to the raw glyph data, needed by gfxFont::Draw
    // and gfxFont::GetBoundingBox
    const CompressedGlyph *GetCharacterGlyphs() { return mCharacterGlyphs; }
    const DetailedGlyph *GetDetailedGlyphs(PRUint32 aCharIndex) {
        NS_ASSERTION(mDetailedGlyphs && mDetailedGlyphs[aCharIndex],
                     "Requested detailed glyphs when there aren't any, "
                     "I think I'll go and have a lie down...");
        return mDetailedGlyphs[aCharIndex];
    }
    PRUint32 CountMissingGlyphs();
    const GlyphRun *GetGlyphRuns(PRUint32 *aNumGlyphRuns) {
        *aNumGlyphRuns = mGlyphRuns.Length();
        return mGlyphRuns.Elements();
    }

private:
    // **** general helpers **** 

    // Returns the index of the GlyphRun containing the given offset.
    // Returns mGlyphRuns.Length() when aOffset is mCharacterCount.
    PRUint32 FindFirstGlyphRunContaining(PRUint32 aOffset);
    // Computes the x-advance for a given cluster starting at aClusterOffset. Does
    // not include any spacing. Result is in appunits.
    PRInt32 ComputeClusterAdvance(PRUint32 aClusterOffset);

    //  **** ligature helpers ****
    // (Platforms do the actual ligaturization, but we need to do a bunch of stuff
    // to handle requests that begin or end inside a ligature)

    struct LigatureData {
        PRUint32 mStartOffset;
        PRUint32 mEndOffset;
        PRUint32 mClusterCount;
        PRUint32 mPartClusterIndex;
        PRInt32  mLigatureWidth;  // appunits
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
    void DrawPartialLigature(gfxFont *aFont, gfxContext *aCtx, PRUint32 aOffset,
                             const gfxRect *aDirtyRect, gfxPoint *aPt,
                             PropertyProvider *aProvider);
    void ShrinkToLigatureBoundaries(PRUint32 *aStart, PRUint32 *aEnd);
    // result in appunits
    gfxFloat GetPartialLigatureWidth(PRUint32 aStart, PRUint32 aEnd, PropertyProvider *aProvider);
    void AccumulatePartialLigatureMetrics(gfxFont *aFont,
                                          PRUint32 aOffset, PRBool aTight,
                                          PropertyProvider *aProvider,
                                          Metrics *aMetrics);

    // **** measurement helper ****
    void AccumulateMetricsForRun(gfxFont *aFont, PRUint32 aStart,
                                 PRUint32 aEnd, PRBool aTight,
                                 PropertyProvider *aProvider,
                                 Metrics *aMetrics);

    // **** drawing helper ****
    void DrawGlyphs(gfxFont *aFont, gfxContext *aContext, PRBool aDrawToPath,
                    gfxPoint *aPt, PRUint32 aStart, PRUint32 aEnd,
                    PropertyProvider *aProvider);

    // All our glyph data is in logical order, not visual
    nsAutoArrayPtr<CompressedGlyph>                mCharacterGlyphs;
    nsAutoArrayPtr<nsAutoArrayPtr<DetailedGlyph> > mDetailedGlyphs; // only non-null if needed
    // XXX this should be changed to a GlyphRun plus a maybe-null GlyphRun*,
    // for smaller size especially in the super-common one-glyphrun case
    nsAutoTArray<GlyphRun,1>                       mGlyphRuns;

    void        *mUserData;
    gfxSkipChars mSkipChars;
    // This is actually an integer, but we keep it in float form to reduce
    // the conversions required
    PRUint32     mAppUnitsPerDevUnit;
    PRUint32     mFlags;
    PRUint32     mCharacterCount;
};

class THEBES_API gfxFontGroup : public gfxTextRunFactory {
public:
    gfxFontGroup(const nsAString& aFamilies, const gfxFontStyle *aStyle);

    virtual ~gfxFontGroup() {
        mFonts.Clear();
    }

    gfxFont *GetFontAt(PRInt32 i) {
        return NS_STATIC_CAST(gfxFont*, mFonts[i]);
    }
    PRUint32 FontListLength() const {
        return mFonts.Length();
    }

    PRBool Equals(const gfxFontGroup& other) const {
        return mFamilies.Equals(other.mFamilies) &&
            mStyle.Equals(other.mStyle);
    }

    const gfxFontStyle *GetStyle() const { return &mStyle; }

    virtual gfxFontGroup *Copy(const gfxFontStyle *aStyle) = 0;

    // These need to be repeated from gfxTextRunFactory because of C++'s
    // hiding rules!
    virtual gfxTextRun *MakeTextRun(const PRUnichar* aString, PRUint32 aLength,
                                    Parameters* aParams) = 0;
    // This function should set TEXT_IS_8BIT in aParams->mFlags.
    virtual gfxTextRun *MakeTextRun(const PRUint8* aString, PRUint32 aLength,
                                    Parameters* aParams) = 0;

    /* helper function for splitting font families on commas and
     * calling a function for each family to fill the mFonts array
     */
    typedef PRBool (*FontCreationCallback) (const nsAString& aName,
                                            const nsACString& aGenericName,
                                            void *closure);
    static PRBool ForEachFont(const nsAString& aFamilies,
                              const nsACString& aLangGroup,
                              FontCreationCallback fc,
                              void *closure);
    PRBool ForEachFont(FontCreationCallback fc, void *closure);

    /* this will call back fc with the a generic font based on the style's langgroup */
    void FindGenericFontFromStyle(FontCreationCallback fc, void *closure);

    const nsString& GetFamilies() { return mFamilies; }

    /**
     * Special strings are strings that we might need to draw/measure but aren't
     * actually substrings of DOM text. They never have extra spacing and
     * aren't involved in breaking.
     */
    enum SpecialString {
        STRING_ELLIPSIS,
        STRING_HYPHEN,
        STRING_SPACE,
        STRING_MAX = STRING_SPACE
    };

    // Get the textrun for the given special string. Ownership remains
    // with this gfxFontGroup. Copy relevant parameters from the template textrun.
    // SpecialString textruns do not use spacing and it's fine to pass null
    // as the PropertyProvider* in their methods.
    // This is stubbed out until we have full textrun support on all platforms.
    gfxTextRun *GetSpecialStringTextRun(SpecialString aString,
                                        gfxTextRun *aTemplate);

protected:
    nsString mFamilies;
    gfxFontStyle mStyle;
    nsTArray< nsRefPtr<gfxFont> > mFonts;
    nsAutoPtr<gfxTextRun> mSpecialStrings[STRING_MAX + 1];

    static PRBool ForEachFontInternal(const nsAString& aFamilies,
                                      const nsACString& aLangGroup,
                                      PRBool aResolveGeneric,
                                      FontCreationCallback fc,
                                      void *closure);

    static PRBool FontResolverProc(const nsAString& aName, void *aClosure);
};
#endif
