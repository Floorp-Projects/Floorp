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

class gfxFontGroup;

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

    virtual nsString GetUniqueName() { return GetName(); }

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

protected:
    // The family name of the font
    nsString mName;

    const gfxFontStyle *mStyle;
};

class THEBES_API gfxTextRunFactory {
    THEBES_INLINE_DECL_REFCOUNTING(gfxTextRunFactory)

public:
    // Flags >= 0x10000 are reserved for textrun clients
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
        TEXT_HAS_SURROGATES          = 0x0100
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
        // DOM string to produce the characters in the textrun
        gfxSkipChars *mSkipChars;
        // A list of where linebreaks are currently placed in the textrun
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

protected:
    nsString mFamilies;
    gfxFontStyle mStyle;
    nsTArray< nsRefPtr<gfxFont> > mFonts;

    static PRBool ForEachFontInternal(const nsAString& aFamilies,
                                      const nsACString& aLangGroup,
                                      PRBool aResolveGeneric,
                                      FontCreationCallback fc,
                                      void *closure);

    static PRBool FontResolverProc(const nsAString& aName, void *aClosure);
};

/**
 * gfxTextRun is an abstraction for drawing and measuring substrings of a run
 * of text.
 * 
 * \r and \n characters must be ignored completely. Substring operations
 * will not normally include these characters.
 * 
 * gfxTextRuns are not refcounted. They should be deleted when no longer required.
 * 
 * gfxTextRuns are mostly immutable. The only things that can change are
 * inter-cluster spacing and line break placement. Spacing is always obtained
 * lazily by methods that need it; cached spacing is flushed via
 * FlushSpacingCache(). Line breaks are stored persistently (insofar
 * as they affect the shaping of glyphs; gfxTextRun does not actually do anything
 * to explicitly account for line breaks). Initially there are no line breaks.
 * The textrun can record line breaks before or after any given cluster. (Line
 * breaks specified inside clusters are ignored.)
 * 
 * gfxTextRuns don't need to remember their text ... often it's enough just to
 * convert text to glyphs and store glyphs plus some geometry data in packed
 * form. However sometimes gfxTextRuns will want to get the original text back
 * to handle some unusual situation. So gfxTextRuns have two modes: "not
 * remembering text" (initial state) and "remembering text". A call to
 * gfxTextRun::RememberText forces a transition from the former to latter state.
 * The text is never forgotten. A gfxTextRun call that receives a TextProvider
 * object may call ForceRememberText to request a transition to "remembering text".
 * 
 * It is important that zero-length substrings are handled correctly. This will
 * be on the test!
 */
class THEBES_API gfxTextRun {
public:
    virtual ~gfxTextRun() {}

    enum {
        // character is the start of a grapheme cluster
        CLUSTER_START = 0x01,
        // character is a cluster start but is part of a ligature started
        // in a previous cluster.
        CONTINUES_LIGATURE = 0x02,
        // line break opportunity before this character
        LINE_BREAK_BEFORE = 0x04
    };
    virtual void GetCharFlags(PRUint32 aStart, PRUint32 aLength,
                              PRUint8 *aFlags) = 0;
    virtual PRUint8 GetCharFlags(PRUint32 aStart) = 0;

    virtual PRUint32 GetLength() = 0;

    // All PRUint32 aStart, PRUint32 aLength ranges below are restricted to
    // grapheme cluster boundaries! All offsets are in terms of the string
    // passed into MakeTextRun.
    
    // All coordinates are in layout/app units

    /**
     * This can be called to force gfxTextRun to remember the text used
     * to create it and *never* call TextProvider::GetText again.
     * 
     * The default implementation is to not remember anything. If you want
     * to be able to recover text from the gfxTextRun user you need to override
     * these.
     */
    virtual void RememberText(const PRUnichar *aText, PRUint32 aLength) {}
    virtual void RememberText(const PRUint8 *aText, PRUint32 aLength) {}

    /**
     * Set the potential linebreaks for a substring of the textrun. These are
     * the "allow break before" points. Initially, there are no potential
     * linebreaks.
     * 
     * @return true if this changed the linebreaks, false if the new line
     * breaks are the same as the old
     */
    virtual PRBool SetPotentialLineBreaks(PRUint32 aStart, PRUint32 aLength,
                                          PRPackedBool *aBreakBefore) = 0;

    /**
     * This is provided so a textrun can (re)obtain the original text used to
     * construct it, if necessary.
     */
    class TextProvider {
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
    };

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
    class PropertyProvider : public TextProvider {
    public:
        // Detect hyphenation break opportunities in the given range; breaks
        // not at cluster boundaries will be ignored.
        virtual void GetHyphenationBreaks(PRUint32 aStart, PRUint32 aLength,
                                          PRPackedBool *aBreakBefore) = 0;

        // Returns the extra width that will be consumed by a hyphen. This should
        // be constant for a given textrun.
        virtual gfxFloat GetHyphenWidth() = 0;

        /**
         * We let the property provider specify spacing on either side of any
         * character. We need to specify both before and after
         * spacing so that substring measurement can do the right things.
         */
        struct Spacing {
            gfxFloat mBefore;
            gfxFloat mAfter;
        };
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
    virtual void Draw(gfxContext *aContext, gfxPoint aPt,
                      PRUint32 aStart, PRUint32 aLength,
                      const gfxRect *aDirtyRect,
                      PropertyProvider *aProvider,
                      gfxFloat *aAdvanceWidth) = 0;

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
    virtual void DrawToPath(gfxContext *aContext, gfxPoint aPt,
                            PRUint32 aStart, PRUint32 aLength,
                            PropertyProvider *aBreakProvider,
                            gfxFloat *aAdvanceWidth) = 0;

    /**
     * Special strings are strings that we might need to draw/measure but aren't
     * actually substrings of the given text. They never have extra spacing and
     * aren't involved in breaking.
     */
    enum SpecialString {
        STRING_ELLIPSIS,
        STRING_HYPHEN,
        STRING_SPACE,
        STRING_MAX = STRING_SPACE
    };
    /**
     * Draw special string.
     * The provided point is the baseline origin on the left of the string
     * for LTR, on the right of the string for RTL.
     */
    virtual void DrawSpecialString(gfxContext *aContext, gfxPoint aPt,
                                   SpecialString aString) = 0;

    // Metrics needed by reflow
    struct Metrics {
        Metrics() {
            mAdvanceWidth = mAscent = mDescent = 0.0;
            mBoundingBox = gfxRect(0,0,0,0);
            mClusterCount = 0;
        }

        void CombineWith(const Metrics& aOtherOnRight) {
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
     * Computes the ReflowMetrics for a substring.
     * Uses GetSpacing from aBreakProvider.
     * @param aTightBoundingBox if true, we make the bounding box tight
     */
    virtual Metrics MeasureText(PRUint32 aStart, PRUint32 aLength,
                                PRBool aTightBoundingBox,
                                PropertyProvider *aProvider) = 0;

    /**
     * Computes the ReflowMetrics for a special string.
     * Uses GetSpacing from aBreakProvider.
     * @param aTightBoundingBox if true, we make the bounding box tight
     */
    virtual Metrics MeasureTextSpecialString(SpecialString aString,
                                             PRBool aTightBoundingBox) = 0;

    /**
     * Computes just the advance width for a substring.
     * Uses GetSpacing from aBreakProvider.
     */
    virtual gfxFloat GetAdvanceWidth(PRUint32 aStart, PRUint32 aLength,
                                     PropertyProvider *aProvider) = 0;

    /**
     * Computes the advance width for a special string.
     */
    virtual gfxFloat GetAdvanceWidthSpecialString(SpecialString aString) = 0;

    /**
     * Get the font metrics that we should use for drawing text decorations.
     * Overriden here so that transforming gfxTextRuns such as smallcaps
     * can do something special if they want to.
     */
    virtual gfxFont::Metrics GetDecorationMetrics() = 0;

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
     * @param aAdvanceWidthDelta if non-null, returns the change in advance
     * width of the given range.
     */
    virtual void SetLineBreaks(PRUint32 aStart, PRUint32 aLength,
                               PRBool aLineBreakBefore, PRBool aLineBreakAfter,
                               TextProvider *aProvider,
                               gfxFloat *aAdvanceWidthDelta) = 0;

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
    virtual PRUint32 BreakAndMeasureText(PRUint32 aStart, PRUint32 aMaxLength,
                                         PRBool aLineBreakBefore, gfxFloat aWidth,
                                         PropertyProvider *aProvider,
                                         PRBool aSuppressInitialBreak,
                                         Metrics *aMetrics, PRBool aTightBoundingBox,
                                         PRBool *aUsedHyphenation,
                                         PRUint32 *aLastBreak) = 0;

    /**
     * Update the reference context.
     * XXX this is a hack. New text frame does not call this. Use only
     * temporarily for old text frame.
     */
    virtual void SetContext(gfxContext *aContext) {}

    /**
     * Flush cached spacing data for the characters at and after aStart.
     */
    virtual void FlushSpacingCache(PRUint32 aStart) = 0;

    // Utility getters

    PRBool IsRightToLeft() const { return (mFlags & gfxTextRunFactory::TEXT_IS_RTL) != 0; }
    gfxFloat GetDirection() const { return (mFlags & gfxTextRunFactory::TEXT_IS_RTL) ? -1.0 : 1.0; }
    void *GetUserData() const { return mUserData; }
    PRUint32 GetFlags() const { return mFlags; }
    const gfxSkipChars& GetSkipChars() const { return mSkipChars; }
    float GetAppUnitsPerDevUnit() { return mAppUnitsPerDevUnit; }

protected:
    gfxTextRun(gfxTextRunFactory::Parameters *aParams, PRBool aIs8Bit)
        : mUserData(aParams->mUserData),
          mAppUnitsPerDevUnit(aParams->mAppUnitsPerDevUnit),
          mFlags(aParams->mFlags)
    {
        mSkipChars.TakeFrom(aParams->mSkipChars);
        if (aIs8Bit) {
            mFlags |= gfxTextRunFactory::TEXT_IS_8BIT;
        }
    }

    void *       mUserData;
    gfxSkipChars mSkipChars;
    // This is actually an integer, but we keep it in float form to reduce
    // the conversions required
    float        mAppUnitsPerDevUnit;
    PRUint32     mFlags;
};
#endif
