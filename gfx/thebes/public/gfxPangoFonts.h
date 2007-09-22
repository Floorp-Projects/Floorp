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
#include "nsClassHashtable.h"

class FontSelector;

class gfxPangoTextRun;

class gfxPangoFont : public gfxFont {
public:
    gfxPangoFont (const nsAString& aName,
                  const gfxFontStyle *aFontStyle);
    virtual ~gfxPangoFont ();

    static void Shutdown();

    virtual const gfxFont::Metrics& GetMetrics();

    PangoFontDescription *GetPangoFontDescription() { RealizeFont(); return mPangoFontDesc; }
    PangoContext *GetPangoContext() { RealizeFont(); return mPangoCtx; }

    void GetMozLang(nsACString &aMozLang);
    void GetActualFontFamily(nsACString &aFamily);

    XftFont *GetXftFont () { RealizeXftFont (); return mXftFont; }
    PangoFont *GetPangoFont() { RealizePangoFont(); return mPangoFont; }
    gfxFloat GetAdjustedSize() { RealizeFont(); return mAdjustedSize; }

    PRBool HasGlyph(const PRUint32 aChar);
    PRUint32 GetGlyph(const PRUint32 aChar);

    virtual nsString GetUniqueName();

    // Get the glyphID of a space
    virtual PRUint32 GetSpaceGlyph() {
        GetMetrics();
        return mSpaceGlyph;
    }

protected:
    PangoFontDescription *mPangoFontDesc;
    PangoContext *mPangoCtx;

    XftFont *mXftFont;
    PangoFont *mPangoFont;
    PangoFont *mGlyphTestingFont;
    cairo_scaled_font_t *mCairoFont;

    PRBool   mHasMetrics;
    PRUint32 mSpaceGlyph;
    Metrics  mMetrics;
    gfxFloat mAdjustedSize;

    void RealizeFont(PRBool force = PR_FALSE);
    void RealizeXftFont(PRBool force = PR_FALSE);
    void RealizePangoFont(PRBool aForce = PR_FALSE);
    void GetCharSize(const char aChar, gfxSize& aInkSize, gfxSize& aLogSize,
                     PRUint32 *aGlyphID = nsnull);

    virtual PRBool SetupCairoFont(cairo_t *aCR);
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
                                    const Parameters *aParams, PRUint32 aFlags);
    virtual gfxTextRun *MakeTextRun(const PRUint8 *aString, PRUint32 aLength,
                                    const Parameters *aParams, PRUint32 aFlags);

    gfxPangoFont *GetFontAt(PRInt32 i) {
        return static_cast<gfxPangoFont*>(static_cast<gfxFont*>(mFonts[i]));
    }

protected:
    friend class FontSelector;

    // ****** Textrun glyph conversion helpers ******

    /**
     * Fill in the glyph-runs for the textrun.
     * @param aTake8BitPath the text contains only characters below 0x100
     * (TEXT_IS_8BIT can return false when the characters are all below 0x100
     * but stored in UTF16 format)
     */
    void InitTextRun(gfxTextRun *aTextRun, const gchar *aUTF8Text,
                     PRUint32 aUTF8Length, PRUint32 aUTF8HeaderLength,
                     PRBool aTake8BitPath);
    // Returns NS_ERROR_FAILURE if there's a missing glyph
    nsresult SetGlyphs(gfxTextRun *aTextRun, gfxPangoFont *aFont,
                       const gchar *aUTF8, PRUint32 aUTF8Length,
                       PRUint32 *aUTF16Offset, PangoGlyphString *aGlyphs,
                       PangoGlyphUnit aOverrideSpaceWidth,
                       PRBool aAbortOnMissingGlyph);
    nsresult SetMissingGlyphs(gfxTextRun *aTextRun,
                              const gchar *aUTF8, PRUint32 aUTF8Length,
                              PRUint32 *aUTF16Offset);
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
    nsTArray<gfxFontStyle> mAdditionalStyles;
};

class gfxPangoFontWrapper {
public:
    gfxPangoFontWrapper(PangoFont *aFont) {
        mFont = aFont;
        g_object_ref(mFont);
    }
    ~gfxPangoFontWrapper() {
        if (mFont)
            g_object_unref(mFont);
    }
    PangoFont* Get() { return mFont; }
private:
    PangoFont *mFont;
};

class gfxPangoFontCache
{
public:
    gfxPangoFontCache();
    ~gfxPangoFontCache();

    static gfxPangoFontCache* GetPangoFontCache() {
        if (!sPangoFontCache)
            sPangoFontCache = new gfxPangoFontCache();
        return sPangoFontCache;
    }
    static void Shutdown() {
        if (sPangoFontCache)
            delete sPangoFontCache;
        sPangoFontCache = nsnull;
    }

    void Put(const PangoFontDescription *aFontDesc, PangoFont *aPangoFont);
    PangoFont* Get(const PangoFontDescription *aFontDesc);
private:
    static gfxPangoFontCache *sPangoFontCache;
    nsClassHashtable<nsUint32HashKey,  gfxPangoFontWrapper> mPangoFonts;
};

// XXX we should remove this class, because this class is used only in |HasGlyph| of gfxPangoFont.
// But it can use fontconfig directly after bug 366664.
class gfxPangoFontNameMap
{
public:
    gfxPangoFontNameMap();
    ~gfxPangoFontNameMap();

    static gfxPangoFontNameMap* GetPangoFontNameMap() {
        if (!sPangoFontNameMap)
            sPangoFontNameMap = new gfxPangoFontNameMap();
        return sPangoFontNameMap;
    }
    static void Shutdown() {
        if (sPangoFontNameMap)
            delete sPangoFontNameMap;
        sPangoFontNameMap = nsnull;
    }

    void Put(const nsACString &aName, PangoFont *aPangoFont);
    PangoFont* Get(const nsACString &aName);

private:
    static gfxPangoFontNameMap *sPangoFontNameMap;
    nsClassHashtable<nsCStringHashKey, gfxPangoFontWrapper> mPangoFonts;
};
#endif /* GFX_PANGOFONTS_H */
