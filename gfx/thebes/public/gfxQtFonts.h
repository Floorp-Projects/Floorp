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

#ifndef GFX_QTFONTS_H
#define GFX_QTFONTS_H

#include "cairo.h"
#include "gfxTypes.h"
#include "gfxFont.h"
#include "gfxContext.h"

#include "nsDataHashtable.h"
#include "nsClassHashtable.h"

#define ENABLE_FAST_PATH_8BIT 1
#define ENABLE_FAST_PATH_ALWAYS 1

class QFont;

class gfxQtFont : public gfxFont {
public: // new functions
    gfxQtFont (const nsAString& aName,
            const gfxFontStyle *aFontStyle);
    virtual ~gfxQtFont ();

    inline const QFont& GetQFont();

public: // from gfxFont
    virtual PRUint32 GetSpaceGlyph ();
    virtual const gfxFont::Metrics& GetMetrics();
    cairo_font_face_t *CairoFontFace(QFont *aFont = nsnull);

protected: // from gfxFont
    virtual nsString GetUniqueName ();
    virtual PRBool SetupCairoFont(gfxContext *aContext);

protected: // new functions
    cairo_scaled_font_t* CreateScaledFont(cairo_t *aCR, 
                                          cairo_matrix_t *aCTM, 
                                          QFont &aQFont);

protected: // data

    QFont* mQFont;
    cairo_scaled_font_t *mCairoFont;
    cairo_font_face_t *mFontFace;

    PRBool mHasSpaceGlyph;
    PRUint32 mSpaceGlyph;
    PRBool mHasMetrics;
    Metrics mMetrics;
    gfxFloat mAdjustedSize;

};

class THEBES_API gfxQtFontGroup : public gfxFontGroup {
public: // new functions
    gfxQtFontGroup (const nsAString& families,
                    const gfxFontStyle *aStyle);
    virtual ~gfxQtFontGroup ();

    inline gfxQtFont * GetFontAt (PRInt32 i);

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
    void InitTextRun(gfxTextRun *aTextRun, 
                     const PRUint8 *aUTF8Text,
                     PRUint32 aUTF8Length, 
                     PRUint32 aUTF8HeaderLength);

    void CreateGlyphRunsFT(gfxTextRun *aTextRun, 
                           const PRUint8 *aUTF8,
                           PRUint32 aUTF8Length);

    static PRBool FontCallback (const nsAString & fontName, 
                                const nsACString & genericName, 
                                void *closure);
    PRBool mEnableKerning;

};

inline const QFont& gfxQtFont::GetQFont()
{
    return *mQFont;
}

inline gfxQtFont * gfxQtFontGroup::GetFontAt (PRInt32 i) 
{
    return static_cast < gfxQtFont * >(static_cast < gfxFont * >(mFonts[i]));
}


#endif /* GFX_QTFONTS_H */

