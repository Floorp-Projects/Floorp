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
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#ifndef GFX_PLATFORM_H
#define GFX_PLATFORM_H

#include "prtypes.h"
#include "nsTArray.h"

#include "nsIObserver.h"

#include "gfxTypes.h"
#include "gfxASurface.h"
#include "gfxColor.h"

#include "qcms.h"
#ifdef XP_OS2
#undef OS2EMX_PLAIN_CHAR
#endif

class gfxImageSurface;
class gfxFont;
class gfxFontGroup;
struct gfxFontStyle;
class gfxUserFontSet;
class gfxFontEntry;
class gfxProxyFontEntry;
class gfxPlatformFontList;
class gfxTextRun;
class nsIURI;
class nsIAtom;
class nsIPrefBranch;

// pref lang id's for font prefs
// !!! needs to match the list of pref font.default.xx entries listed in all.js !!!
// !!! don't use as bit mask, this may grow larger !!!

enum eFontPrefLang {
    eFontPrefLang_Western     =  0,
    eFontPrefLang_CentEuro    =  1,
    eFontPrefLang_Japanese    =  2,
    eFontPrefLang_ChineseTW   =  3,
    eFontPrefLang_ChineseCN   =  4,
    eFontPrefLang_ChineseHK   =  5,
    eFontPrefLang_Korean      =  6,
    eFontPrefLang_Cyrillic    =  7,
    eFontPrefLang_Baltic      =  8,
    eFontPrefLang_Greek       =  9,
    eFontPrefLang_Turkish     = 10,
    eFontPrefLang_Thai        = 11,
    eFontPrefLang_Hebrew      = 12,
    eFontPrefLang_Arabic      = 13,
    eFontPrefLang_Devanagari  = 14,
    eFontPrefLang_Tamil       = 15,
    eFontPrefLang_Armenian    = 16,
    eFontPrefLang_Bengali     = 17,
    eFontPrefLang_Canadian    = 18,
    eFontPrefLang_Ethiopic    = 19,
    eFontPrefLang_Georgian    = 20,
    eFontPrefLang_Gujarati    = 21,
    eFontPrefLang_Gurmukhi    = 22,
    eFontPrefLang_Khmer       = 23,
    eFontPrefLang_Malayalam   = 24,
    eFontPrefLang_Oriya       = 25,
    eFontPrefLang_Telugu      = 26,
    eFontPrefLang_Kannada     = 27,
    eFontPrefLang_Sinhala     = 28,
    eFontPrefLang_Tibetan     = 29,

    eFontPrefLang_LangCount   = 30, // except Others and UserDefined.

    eFontPrefLang_Others      = 30, // x-unicode
    eFontPrefLang_UserDefined = 31,

    eFontPrefLang_CJKSet      = 32, // special code for CJK set
    eFontPrefLang_AllCount    = 33
};

enum eCMSMode {
    eCMSMode_Off          = 0,     // No color management
    eCMSMode_All          = 1,     // Color manage everything
    eCMSMode_TaggedOnly   = 2,     // Color manage tagged Images Only
    eCMSMode_AllCount     = 3
};

// when searching through pref langs, max number of pref langs
const PRUint32 kMaxLenPrefLangList = 32;

#define UNINITIALIZED_VALUE  (-1)

class THEBES_API gfxPlatform {
public:
    /**
     * Return a pointer to the current active platform.
     * This is a singleton; it contains mostly convenience
     * functions to obtain platform-specific objects.
     */
    static gfxPlatform *GetPlatform();

    /**
     * Start up Thebes. This can fail.
     */
    static nsresult Init();

    /**
     * Clean up static objects to shut down thebes.
     */
    static void Shutdown();

    /**
     * Create an offscreen surface of the given dimensions
     * and image format.
     */
    virtual already_AddRefed<gfxASurface> CreateOffscreenSurface(const gfxIntSize& size,
                                                                 gfxASurface::gfxContentType contentType) = 0;


    virtual already_AddRefed<gfxASurface> OptimizeImage(gfxImageSurface *aSurface,
                                                        gfxASurface::gfxImageFormat format);

    /*
     * Font bits
     */

    virtual void SetupClusterBoundaries(gfxTextRun *aTextRun, const PRUnichar *aString);

    /**
     * Fill aListOfFonts with the results of querying the list of font names
     * that correspond to the given language group or generic font family
     * (or both, or neither).
     */
    virtual nsresult GetFontList(nsIAtom *aLangGroup,
                                 const nsACString& aGenericFamily,
                                 nsTArray<nsString>& aListOfFonts);

    /**
     * Rebuilds the any cached system font lists
     */
    virtual nsresult UpdateFontList();

    /**
     * Create the platform font-list object (gfxPlatformFontList concrete subclass)
     */
    virtual gfxPlatformFontList *CreatePlatformFontList() {
        NS_NOTREACHED("oops, this platform doesn't have a gfxPlatformFontList implementation");
        return nsnull;
    }

    /**
     * Font name resolver, this returns actual font name(s) by the callback
     * function. If the font doesn't exist, the callback function is not called.
     * If the callback function returns PR_FALSE, the aAborted value is set to
     * PR_TRUE, otherwise, PR_FALSE.
     */
    typedef PRBool (*FontResolverCallback) (const nsAString& aName,
                                            void *aClosure);
    virtual nsresult ResolveFontName(const nsAString& aFontName,
                                     FontResolverCallback aCallback,
                                     void *aClosure,
                                     PRBool& aAborted) = 0;

    /**
     * Resolving a font name to family name. The result MUST be in the result of GetFontList().
     * If the name doesn't in the system, aFamilyName will be empty string, but not failed.
     */
    virtual nsresult GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName) = 0;

    /**
     * Create the appropriate platform font group
     */
    virtual gfxFontGroup *CreateFontGroup(const nsAString& aFamilies,
                                          const gfxFontStyle *aStyle,
                                          gfxUserFontSet *aUserFontSet) = 0;
                                          
                                          
    /**
     * Look up a local platform font using the full font face name.
     * (Needed to support @font-face src local().)
     * Ownership of the returned gfxFontEntry is passed to the caller,
     * who must either AddRef() or delete.
     */
    virtual gfxFontEntry* LookupLocalFont(const gfxProxyFontEntry *aProxyEntry,
                                          const nsAString& aFontName)
    { return nsnull; }

    /**
     * Activate a platform font.  (Needed to support @font-face src url().)
     * aFontData is a NS_Malloc'ed block that must be freed by this function
     * (or responsibility passed on) when it is no longer needed; the caller
     * will NOT free it.
     * Ownership of the returned gfxFontEntry is passed to the caller,
     * who must either AddRef() or delete.
     */
    virtual gfxFontEntry* MakePlatformFont(const gfxProxyFontEntry *aProxyEntry,
                                           const PRUint8 *aFontData,
                                           PRUint32 aLength);

    /**
     * Whether to allow downloadable fonts via @font-face rules
     */
    PRBool DownloadableFontsEnabled();

    /**
     * Whether to sanitize downloaded fonts using the OTS library
     */
    PRBool SanitizeDownloadedFonts();

    /**
     * Whether to preserve OpenType layout tables when sanitizing
     */
    PRBool PreserveOTLTablesWhenSanitizing();

    /**
     * Whether to use the harfbuzz shaper (depending on script complexity).
     *
     * This allows harfbuzz to be enabled selectively via the preferences.
     * Current "harfbuzz level" options:
     * <= 0 will never use the harfbuzz shaper;
     *  = 1 will use it for "simple" scripts (Latin, Cyrillic, CJK, etc);
     * >= 2 will use it for all scripts, including those requiring complex
     *      shaping for correct rendering (Arabic, Indic, etc).
     *
     * Depending how harfbuzz complex-script support evolves, we may want to
     * update this mechanism - e.g., separating complex-bidi from Indic,
     * or other distinctions.
     */
    PRInt8 UseHarfBuzzLevel();

    // check whether format is supported on a platform or not (if unclear, returns true)
    virtual PRBool IsFontFormatSupported(nsIURI *aFontURI, PRUint32 aFormatFlags) { return PR_FALSE; }

    void GetPrefFonts(nsIAtom *aLanguage, nsString& array, PRBool aAppendUnicode = PR_TRUE);

    // in some situations, need to make decisions about ambiguous characters, may need to look at multiple pref langs
    void GetLangPrefs(eFontPrefLang aPrefLangs[], PRUint32 &aLen, eFontPrefLang aCharLang, eFontPrefLang aPageLang);
    
    /**
     * Iterate over pref fonts given a list of lang groups.  For a single lang
     * group, multiple pref fonts are possible.  If error occurs, returns PR_FALSE,
     * PR_TRUE otherwise.  Callback returns PR_FALSE to abort process.
     */
    typedef PRBool (*PrefFontCallback) (eFontPrefLang aLang, const nsAString& aName,
                                        void *aClosure);
    static PRBool ForEachPrefFont(eFontPrefLang aLangArray[], PRUint32 aLangArrayLen,
                                  PrefFontCallback aCallback,
                                  void *aClosure);

    // convert a lang group to enum constant (i.e. "zh-TW" ==> eFontPrefLang_ChineseTW)
    static eFontPrefLang GetFontPrefLangFor(const char* aLang);

    // convert a lang group atom to enum constant
    static eFontPrefLang GetFontPrefLangFor(nsIAtom *aLang);

    // convert a enum constant to lang group string (i.e. eFontPrefLang_ChineseTW ==> "zh-TW")
    static const char* GetPrefLangName(eFontPrefLang aLang);
   
    // map a Unicode range (based on char code) to a font language for Preferences
    static eFontPrefLang GetFontPrefLangFor(PRUint8 aUnicodeRange);

    // returns true if a pref lang is CJK
    static PRBool IsLangCJK(eFontPrefLang aLang);
    
    // helper method to add a pref lang to an array, if not already in array
    static void AppendPrefLang(eFontPrefLang aPrefLangs[], PRUint32& aLen, eFontPrefLang aAddLang);
    
    /**
     * Are we going to try color management?
     */
    static eCMSMode GetCMSMode();

    /**
     * Determines the rendering intent for color management.
     *
     * If the value in the pref gfx.color_management.rendering_intent is a
     * valid rendering intent as defined in gfx/qcms/qcms.h, that
     * value is returned. Otherwise, -1 is returned and the embedded intent
     * should be used.
     *
     * See bug 444014 for details.
     */
    static int GetRenderingIntent();

    /**
     * Convert a pixel using a cms transform in an endian-aware manner.
     *
     * Sets 'out' to 'in' if transform is NULL.
     */
    static void TransformPixel(const gfxRGBA& in, gfxRGBA& out, qcms_transform *transform);

    /**
     * Return the output device ICC profile.
     */
    static qcms_profile* GetCMSOutputProfile();

    /**
     * Return the sRGB ICC profile.
     */
    static qcms_profile* GetCMSsRGBProfile();

    /**
     * Return sRGB -> output device transform.
     */
    static qcms_transform* GetCMSRGBTransform();

    /**
     * Return output -> sRGB device transform.
     */
    static qcms_transform* GetCMSInverseRGBTransform();

    /**
     * Return sRGBA -> output device transform.
     */
    static qcms_transform* GetCMSRGBATransform();

    virtual void FontsPrefsChanged(nsIPrefBranch *aPrefBranch, const char *aPref);

    /**
     * Returns a 1x1 surface that can be used to create graphics contexts
     * for measuring text etc as if they will be rendered to the screen
     */
    gfxASurface* ScreenReferenceSurface() { return mScreenReferenceSurface; }

protected:
    gfxPlatform();
    virtual ~gfxPlatform();

    static PRBool GetBoolPref(const char *aPref, PRBool aDefault);

    void AppendCJKPrefLangs(eFontPrefLang aPrefLangs[], PRUint32 &aLen, 
                            eFontPrefLang aCharLang, eFontPrefLang aPageLang);
                                               
    PRBool  mAllowDownloadableFonts;
    PRBool  mDownloadableFontsSanitize;
    PRBool  mSanitizePreserveOTLTables;

    // whether to use the HarfBuzz layout engine
    PRInt8  mUseHarfBuzzLevel;

private:
    virtual qcms_profile* GetPlatformCMSOutputProfile();

    nsRefPtr<gfxASurface> mScreenReferenceSurface;
    nsTArray<PRUint32> mCJKPrefLangs;
    nsCOMPtr<nsIObserver> overrideObserver;
};

#endif /* GFX_PLATFORM_H */
