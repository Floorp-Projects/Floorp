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
#include <vector>

class gfxContext;

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

struct NS_EXPORT gfxFontStyle {
    gfxFontStyle(PRUint8 aStyle, PRUint8 aVariant,
                 PRUint16 aWeight, PRUint8 aDecoration, gfxFloat aSize,
                 float aSizeAdjust=0.0f) :
        style(aStyle), variant(aVariant), weight(aWeight),
        decorations(aDecoration), size(aSize), sizeAdjust(aSizeAdjust) {}

    // The style of font (normal, italic, oblique)
    PRUint8 style : 7;

    // Force this font to not be considered a 'generic' font, even if
    // the name is the same as a CSS generic font family.
    PRPackedBool systemFont : 1;

    // The variant of the font (normal, small-caps)
    PRUint8 variant : 7;

    // True if the character set quirks (for treatment of "Symbol",
    // "Wingdings", etc.) should be applied.
    PRPackedBool familyNameQuirks : 1;
    
    // The weight of the font (0-999)
    PRUint16 weight;

    // The decorations on the font (underline, overline,
    // line-through). The decorations can be binary or'd together.
    PRUint8 decorations;

    // The logical size of the font, in nscoord units
    gfxFloat size;

    // The aspect-value (ie., the ratio actualsize:actualxheight) that any
    // actual physical font created from this font structure must have when
    // rendering or measuring a string. A value of 0 means no adjustment
    // needs to be done.
    float sizeAdjust;
};



/* a SPECIFIC single font family */
class NS_EXPORT gfxFont {
public:
    virtual ~gfxFont() {}

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
    virtual const gfxFont::Metrics& GetMetrics() const = 0;

protected:
    // The family name of the font
    nsString mName;

    // points to the group's style
    const gfxFontGroup *mGroup;
    const gfxFontStyle *mStyle;
};

class NS_EXPORT gfxFontGroup {
public:
    gfxFontGroup(const nsAString& aFamilies, const gfxFontStyle *aStyle) : mFamilies(aFamilies), mStyle(*aStyle) {}
    virtual ~gfxFontGroup() {
        for (std::vector<gfxFont*>::iterator it = mFonts.begin(); it!=mFonts.end(); ++it)
            delete *it;
    }

    std::vector<gfxFont*> &GetFontList() { return mFonts; } // XXX this should really be const..
    const gfxFontStyle *GetStyle() const { return &mStyle; }

    virtual void DrawString(gfxContext *aContext, const nsAString& aString, gfxPoint pt) = 0;
    virtual gfxFloat MeasureText(gfxContext *aContext, const nsAString& aString) = 0; // returns length in pixels
    // need something here for "text dimensions" similar to what we had before
    // XXX need to handle spacing...

protected:
    /* data gets passed in to the font when it is created */
    void Init();
    virtual gfxFont *MakeFont(const nsAString& aName) = 0;

    PRBool FillFontArray();
    nsString mFamilies;
    gfxFontStyle mStyle;
    std::vector<gfxFont*> mFonts;

    void *mData; // XXXA
};


#endif

