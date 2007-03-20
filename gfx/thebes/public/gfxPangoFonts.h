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

    virtual gfxTextRun::Metrics Measure(gfxTextRun *aTextRun,
                                        PRUint32 aStart, PRUint32 aEnd,
                                        PRBool aTightBoundingBox,
                                        Spacing *aSpacing);

    virtual nsString GetUniqueName();

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

    virtual void SetupCairoFont(cairo_t *aCR);
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

protected:
    friend class FontSelector;

    // ****** Textrun glyph conversion helpers ******

    // If aUTF16Text is null, then the string contains no characters >= 0x100
    void InitTextRun(gfxTextRun *aTextRun, const gchar *aUTF8Text,
                     PRUint32 aUTF8Length, PRUint32 aUTF8HeaderLength,
                     const PRUnichar *aUTF16Text, PRUint32 aUTF16Length);
    // Returns NS_ERROR_FAILURE if there's a missing glyph
    nsresult SetGlyphs(gfxTextRun *aTextRun, const gchar *aUTF8,
                       PRUint32 aUTF8Length,
                       PRUint32 *aUTF16Offset, PangoGlyphString *aGlyphs,
                       PangoGlyphUnit aOverrideSpaceWidth,
                       PRBool aAbortOnMissingGlyph);
    // If aUTF16Text is null, then the string contains no characters >= 0x100.
    // Returns NS_ERROR_FAILURE if we require the itemizing path
    nsresult CreateGlyphRunsFast(gfxTextRun *aTextRun,
                                 const gchar *aUTF8, PRUint32 aUTF8Length,
                                 const PRUnichar *aUTF16Text, PRUint32 aUTF16Length);
    void CreateGlyphRunsItemizing(gfxTextRun *aTextRun,
                                  const gchar *aUTF8, PRUint32 aUTF8Length,
                                  PRUint32 aUTF8HeaderLength);
#if defined(ENABLE_XFT_FAST_PATH_8BIT) || defined(ENABLE_XFT_FAST_PATH_ALWAYS)
    void CreateGlyphRunsXft(gfxTextRun *aTextRun,
                            const gchar *aUTF8, PRUint32 aUTF8Length);
#endif

    static PRBool FontCallback (const nsAString& fontName,
                                const nsACString& genericName,
                                void *closure);

private:
    nsDataHashtable<nsStringHashKey, nsRefPtr<gfxPangoFont> > mFontCache;
    nsTArray<gfxFontStyle> mAdditionalStyles;
};

#endif /* GFX_PANGOFONTS_H */
