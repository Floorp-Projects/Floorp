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

#include "nsAutoRef.h"
#include "nsTArray.h"

#include <pango/pango.h>

// Control when we bypass Pango
// Enable this to use FreeType to glyph-convert 8bit-only textruns, but use Pango
// to shape any textruns with non-8bit characters
// XXX
#define ENABLE_FAST_PATH_8BIT
// Enable this to bypass Pango shaping for all textruns.  Don't expect
// anything other than simple Latin work though!
//#define ENABLE_FAST_PATH_ALWAYS

class gfxFcPangoFontSet;
class gfxProxyFontEntry;
typedef struct _FcPattern FcPattern;
typedef struct FT_FaceRec_* FT_Face;

class THEBES_API gfxPangoFontGroup : public gfxFontGroup {
public:
    gfxPangoFontGroup (const nsAString& families,
                       const gfxFontStyle *aStyle,
                       gfxUserFontSet *aUserFontSet);
    virtual ~gfxPangoFontGroup ();

    virtual gfxFontGroup *Copy(const gfxFontStyle *aStyle);

    // Create and initialize a textrun using Pango
    virtual gfxTextRun *MakeTextRun(const PRUnichar *aString, PRUint32 aLength,
                                    const Parameters *aParams, PRUint32 aFlags);
    virtual gfxTextRun *MakeTextRun(const PRUint8 *aString, PRUint32 aLength,
                                    const Parameters *aParams, PRUint32 aFlags);

    virtual gfxFont *GetFontAt(PRInt32 i);

    virtual void UpdateFontList();

    static void Shutdown();

    // Used for @font-face { src: local(); }
    static gfxFontEntry *NewFontEntry(const gfxProxyFontEntry &aProxyEntry,
                                      const nsAString &aFullname);
    // Used for @font-face { src: url(); }
    static gfxFontEntry *NewFontEntry(const gfxProxyFontEntry &aProxyEntry,
                                      const PRUint8 *aFontData,
                                      PRUint32 aLength);

    // Interfaces used internally
    // (but public so that they can be accessed from non-member functions):

    // The FontGroup holds the reference to the PangoFont (through the FontSet).
    PangoFont *GetBasePangoFont();

    // A language guessed from the gfxFontStyle
    PangoLanguage *GetPangoLanguage() { return mPangoLanguage; }

    // @param aLang [in] language to use for pref fonts and system default font
    //        selection, or NULL for the language guessed from the gfxFontStyle.
    // The FontGroup holds a reference to this set.
    gfxFcPangoFontSet *GetFontSet(PangoLanguage *aLang = NULL);

protected:
    class FontSetByLangEntry {
    public:
        FontSetByLangEntry(PangoLanguage *aLang, gfxFcPangoFontSet *aFontSet);
        PangoLanguage *mLang;
        nsRefPtr<gfxFcPangoFontSet> mFontSet;
    };
    // There is only one of entry in this array unless characters from scripts
    // of other languages are measured.
    nsAutoTArray<FontSetByLangEntry,1> mFontSets;

    gfxFloat mSizeAdjustFactor;
    PangoLanguage *mPangoLanguage;

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
    nsresult SetGlyphs(gfxTextRun *aTextRun,
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
#if defined(ENABLE_FAST_PATH_8BIT) || defined(ENABLE_FAST_PATH_ALWAYS)
    PRBool CanTakeFastPath(PRUint32 aFlags);
    nsresult CreateGlyphRunsFast(gfxTextRun *aTextRun,
                                 const gchar *aUTF8, PRUint32 aUTF8Length);
#endif

    void GetFcFamilies(nsTArray<nsString> *aFcFamilyList,
                       const nsACString& aLangGroup);

    // @param aLang [in] language to use for pref fonts and system font
    //        resolution, or NULL to guess a language from the gfxFontStyle.
    // @param aMatchPattern [out] if non-NULL, will return the pattern used.
    already_AddRefed<gfxFcPangoFontSet>
    MakeFontSet(PangoLanguage *aLang, gfxFloat aSizeAdjustFactor,
                nsAutoRef<FcPattern> *aMatchPattern = NULL);

    gfxFcPangoFontSet *GetBaseFontSet();

    gfxFloat GetSizeAdjustFactor()
    {
        if (mFontSets.Length() == 0)
            GetBaseFontSet();
        return mSizeAdjustFactor;
    }
};

#endif /* GFX_PANGOFONTS_H */
