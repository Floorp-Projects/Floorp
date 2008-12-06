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

#ifndef GFX_GTKDFBFONTS_H
#define GFX_GTKDFBFONTS_H

#include "cairo.h"
#include "gfxTypes.h"
#include "gfxFont.h"
#include "gfxContext.h"
#include "gfxFontUtils.h"

typedef struct FT_FaceRec_* FT_Face;

/**
 * FontFamily is a class that describes one of the fonts on the users system.  It holds
 * each FontEntry (maps more directly to a font face) which holds font type, charset info
 * and character map info.
 */
class FontEntry;
class FontFamily : public gfxFontFamily
{
public:
    FontFamily(const nsAString& aName) :
        gfxFontFamily(aName) { }

    FontEntry *FindFontEntry(const gfxFontStyle& aFontStyle);

public:
    nsTArray<nsRefPtr<FontEntry> > mFaces;
    nsString mName;
};

class FontEntry : public gfxFontEntry
{
public:
    FontEntry(const nsAString& aFaceName) :
        gfxFontEntry(aFaceName)
    {
        mFontFace = nsnull;
        mFTFontIndex = 0;
        mUnicodeFont = PR_FALSE;
        mSymbolFont = PR_FALSE;
    }

    FontEntry(const FontEntry& aFontEntry);
    ~FontEntry();

    const nsString& GetName() const {
        return mFaceName;
    }

    cairo_font_face_t *CairoFontFace();

    cairo_font_face_t *mFontFace;

    nsString mFaceName;
    nsCString mFilename;
    PRUint8 mFTFontIndex;

    PRPackedBool mTrueType    : 1;
    PRPackedBool mIsType1     : 1;
};



class gfxFT2Font : public gfxFont {
public: // new functions
    gfxFT2Font(FontEntry *aFontEntry,
               const gfxFontStyle *aFontStyle);
    virtual ~gfxFT2Font ();

    virtual const gfxFont::Metrics& GetMetrics();

    cairo_font_face_t *CairoFontFace();
    cairo_scaled_font_t *CairoScaledFont();

    virtual PRBool SetupCairoFont(gfxContext *aContext);
    virtual nsString GetUniqueName();
    virtual PRUint32 GetSpaceGlyph();

    FontEntry *GetFontEntry();
private:
    cairo_scaled_font_t *mScaledFont;

    PRBool mHasSpaceGlyph;
    PRUint32 mSpaceGlyph;
    PRBool mHasMetrics;
    Metrics mMetrics;
    gfxFloat mAdjustedSize;
};

class THEBES_API gfxFT2FontGroup : public gfxFontGroup {
public: // new functions
    gfxFT2FontGroup (const nsAString& families,
                    const gfxFontStyle *aStyle);
    virtual ~gfxFT2FontGroup ();

    inline gfxFT2Font *GetFontAt (PRInt32 i) {
        // If it turns out to be hard for all clients that cache font
        // groups to call UpdateFontList at appropriate times, we could
        // instead consider just calling UpdateFontList from someplace
        // more central (such as here).
        NS_ASSERTION(!mUserFontSet || mCurrGeneration == GetGeneration(),
                     "Whoever was caching this font group should have "
                     "called UpdateFontList on it");

        return static_cast <gfxFT2Font *>(static_cast <gfxFont *>(mFonts[i]));
    }

protected: // from gfxFontGroup
    virtual gfxTextRun *MakeTextRun(const PRUnichar *aString, 
                                    PRUint32 aLength,
                                    const Parameters *aParams, 
                                    PRUint32 aFlags);

    virtual gfxTextRun *MakeTextRun(const PRUint8 *aString, 
                                    PRUint32 aLength,
                                    const Parameters *aParams, 
                                    PRUint32 aFlags);

    virtual gfxFontGroup *Copy(const gfxFontStyle *aStyle);


protected: // new functions
    void InitTextRun(gfxTextRun *aTextRun);

    void CreateGlyphRunsFT(gfxTextRun *aTextRun);
    void AddRange(gfxTextRun *aTextRun, gfxFT2Font *font, const PRUnichar *str, PRUint32 len);

    static PRBool FontCallback (const nsAString & fontName, 
                                const nsACString & genericName, 
                                void *closure);
    PRBool mEnableKerning;

    gfxFT2Font *FindFontForChar(PRUint32 ch, PRUint32 prevCh, PRUint32 nextCh, gfxFT2Font *aFont);
    PRUint32 ComputeRanges();

    struct TextRange {
        TextRange(PRUint32 aStart,  PRUint32 aEnd) : start(aStart), end(aEnd) { }
        PRUint32 Length() const { return end - start; }
        nsRefPtr<gfxFT2Font> font;
        PRUint32 start, end;
    };

    nsTArray<TextRange> mRanges;
    nsString mString;
};

#endif /* GFX_GTKDFBFONTS_H */

