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

class gfxContext;
class gfxTextRun;

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

    void ComputeWeightAndOffset(PRInt16 *outBaseWeight,
                                PRInt16 *outOffset) const;
};



/* a SPECIFIC single font family */
class THEBES_API gfxFont {
    THEBES_DECL_REFCOUNTING_ABSTRACT

public:
    gfxFont(const nsAString &aName, const gfxFontStyle *aFontGroup);
    virtual ~gfxFont() {}

    const nsString& GetName() const { return mName; }
    const gfxFontStyle *GetStyle() const { return mStyle; }

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


class THEBES_API gfxFontGroup {
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

    const gfxFontStyle *GetStyle() const { return &mStyle; }

    /* unicode method */
    virtual gfxTextRun *MakeTextRun(const nsAString& aString) = 0;
    /* ASCII text only, not UTF-8 */
    virtual gfxTextRun *MakeTextRun(const nsACString& aString) = 0;

    /* helper function for splitting font families on commas and
     * calling a function for each family to fill the mFonts array
     */
    typedef PRBool (*FontCreationCallback) (const nsAString& aName, const nsACString& aGenericName, void *closure);
    static PRBool ForEachFont(const nsAString& aFamilies,
                              const nsACString& aLangGroup,
                              FontCreationCallback fc,
                              void *closure);
    PRBool ForEachFont(FontCreationCallback fc, void *closure);

    /* this will call back fc with the a generic font based on the style's langgroup */
    void FindGenericFontFromStyle(FontCreationCallback fc, void *closure);

protected:
    nsString mFamilies;
    gfxFontStyle mStyle;
    nsTArray< nsRefPtr<gfxFont> > mFonts;
};


// these do not copy the text
class THEBES_API gfxTextRun { 
    THEBES_DECL_REFCOUNTING_ABSTRACT

public:
    virtual void Draw(gfxContext *aContext, gfxPoint pt) = 0;
    // returns length in pixels
    virtual gfxFloat Measure(gfxContext *aContext) = 0;

    virtual void SetSpacing(const nsTArray<gfxFloat> &spacing) = 0;
    virtual const nsTArray<gfxFloat> *const GetSpacing() const = 0;

    // defaults to FALSE
    virtual void SetRightToLeft(PRBool aIsRTL) { mIsRTL = aIsRTL; }
    virtual PRBool IsRightToLeft() const { return mIsRTL; }

private:
    PRBool mIsRTL;
};
#endif
