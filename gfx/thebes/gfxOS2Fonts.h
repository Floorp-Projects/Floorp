/* vim: set sw=4 sts=4 et cin: */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is OS/2 code in Thebes.
 *
 * The Initial Developer of the Original Code is
 * Peter Weilbacher <mozilla@Weilbacher.org>.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef GFX_OS2_FONTS_H
#define GFX_OS2_FONTS_H

#include "gfxTypes.h"
#include "gfxFont.h"
#include "gfxMatrix.h"
#include "nsDataHashtable.h"

#define INCL_GPI
#include <os2.h>
#include <cairo-os2.h>
#include "cairo-ft.h" // includes fontconfig.h, too
#include <freetype/tttables.h>

#include "nsICharsetConverterManager.h"

class gfxOS2FontEntry : public gfxFontEntry {
public:
    gfxOS2FontEntry(const nsAString& aName) : gfxFontEntry(aName) {}
    ~gfxOS2FontEntry() {}
};

class gfxOS2Font : public gfxFont {
public:
    gfxOS2Font(gfxOS2FontEntry *aFontEntry, const gfxFontStyle *aFontStyle);
    virtual ~gfxOS2Font();

    virtual const gfxFont::Metrics& GetMetrics();
    cairo_font_face_t *CairoFontFace();
    cairo_scaled_font_t *CairoScaledFont();

    // Get the glyphID of a space
    virtual PRUint32 GetSpaceGlyph() {
        if (!mMetrics)
            GetMetrics();
        return mSpaceGlyph;
    }

    static already_AddRefed<gfxOS2Font> GetOrMakeFont(const nsAString& aName,
                                                      const gfxFontStyle *aStyle);

protected:
    virtual bool SetupCairoFont(gfxContext *aContext);

    virtual FontType GetType() const { return FONT_TYPE_OS2; }

private:
    cairo_font_face_t *mFontFace;
    Metrics *mMetrics;
    gfxFloat mAdjustedSize;
    PRUint32 mSpaceGlyph;
    int mHinting;
    bool mAntialias;
};


class THEBES_API gfxOS2FontGroup : public gfxFontGroup {
public:
    gfxOS2FontGroup(const nsAString& aFamilies, const gfxFontStyle* aStyle, gfxUserFontSet *aUserFontSet);
    virtual ~gfxOS2FontGroup();

    virtual gfxFontGroup *Copy(const gfxFontStyle *aStyle);

    // create and initialize the textRun using FreeType font
    virtual gfxTextRun *MakeTextRun(const PRUnichar* aString, PRUint32 aLength,
                                    const Parameters* aParams, PRUint32 aFlags);
    virtual gfxTextRun *MakeTextRun(const PRUint8* aString, PRUint32 aLength,
                                    const Parameters* aParams, PRUint32 aFlags);

    gfxOS2Font *GetFontAt(PRInt32 i) {
        // If it turns out to be hard for all clients that cache font
        // groups to call UpdateFontList at appropriate times, we could
        // instead consider just calling UpdateFontList from someplace
        // more central (such as here).
        NS_ASSERTION(!mUserFontSet || mCurrGeneration == GetGeneration(),
                     "Whoever was caching this font group should have "
                     "called UpdateFontList on it");

#ifdef DEBUG_thebes_2
        printf("gfxOS2FontGroup[%#x]::GetFontAt(%d), %#x, %#x\n",
               (unsigned)this, i, (unsigned)&mFonts, (unsigned)&mFonts[i]);
#endif
        return static_cast<gfxOS2Font*>(static_cast<gfxFont*>(mFonts[i]));
    }

protected:
    void InitTextRun(gfxTextRun *aTextRun, const PRUint8 *aUTF8Text,
                     PRUint32 aUTF8Length, PRUint32 aUTF8HeaderLength);
    void CreateGlyphRunsFT(gfxTextRun *aTextRun, const PRUint8 *aUTF8,
                           PRUint32 aUTF8Length);
    static bool FontCallback(const nsAString& aFontName,
                               const nsACString& aGenericName,
                               bool aUseFontSet,
                               void *aClosure);

private:
    bool mEnableKerning;
};

#endif /* GFX_OS2_FONTS_H */
