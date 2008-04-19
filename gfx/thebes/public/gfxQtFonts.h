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

class gfxQtFont : public gfxFont {
public:
     gfxQtFont (const nsAString& aName,
                const gfxFontStyle *aFontStyle);
     virtual ~gfxQtFont ();

     virtual nsString GetUniqueName ();

     virtual PRUint32 GetSpaceGlyph ()
     {

        NS_ASSERTION (GetStyle ()->size != 0,
        "forgot to short-circuit a text run with zero-sized font?");
        GetMetrics ();
       return mSpaceGlyph;
     }

    virtual const gfxFont::Metrics& GetMetrics();

    void* GetQFont() { if (!mQFont) RealizeQFont(); return mQFont; }

protected:
    void *mQFont;
    cairo_scaled_font_t *mCairoFont;

    PRBool mHasMetrics;
    PRUint32 mSpaceGlyph;
    Metrics mMetrics;
    gfxFloat mAdjustedSize;

    virtual PRBool SetupCairoFont(gfxContext *aContext);
	void RealizeQFont();
};

class THEBES_API gfxQtFontGroup : public gfxFontGroup {
public:
    gfxQtFontGroup (const nsAString& families,
                    const gfxFontStyle *aStyle);
    virtual ~gfxQtFontGroup ();

    virtual gfxFontGroup *Copy(const gfxFontStyle *aStyle);

    // Create and initialize a textrun using Pango
    virtual gfxTextRun *MakeTextRun(const PRUnichar *aString, PRUint32 aLength,
                                    const Parameters *aParams, PRUint32 aFlags);
    virtual gfxTextRun *MakeTextRun(const PRUint8 *aString, PRUint32 aLength,
                                    const Parameters *aParams, PRUint32 aFlags);

    gfxQtFont * GetFontAt (PRInt32 i) {
        return static_cast < gfxQtFont * >(static_cast < gfxFont * >(mFonts[i]));
    }

protected:
    void InitTextRun (gfxTextRun * aTextRun, const char * aUTF8Text,
                      PRUint32 aUTF8Length, PRUint32 aUTF8HeaderLength,
                      PRBool aTake8BitPath);

/*
    void CreateGlyphRunsItemizing (gfxTextRun * aTextRun,
                                   const char * aUTF8, PRUint32 aUTF8Length,
                                   PRUint32 aUTF8HeaderLength);
*/
#if defined(ENABLE_FAST_PATH_8BIT) || defined(ENABLE_FAST_PATH_ALWAYS)
    PRBool CanTakeFastPath (PRUint32 aFlags);
    nsresult CreateGlyphRunsFast (gfxTextRun * aTextRun,
                                  const char * aUTF8, PRUint32 aUTF8Length);
#endif


    static PRBool FontCallback (const nsAString & fontName, const nsACString & genericName, void *closure);

};

#endif /* GFX_QTFONTS_H */

