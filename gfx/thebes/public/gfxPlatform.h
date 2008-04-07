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
#include "nsVoidArray.h"

#include "gfxTypes.h"
#include "gfxASurface.h"

#ifdef XP_OS2
#undef OS2EMX_PLAIN_CHAR
#endif

typedef void* cmsHPROFILE;
typedef void* cmsHTRANSFORM;

class gfxImageSurface;
class gfxFontGroup;
struct gfxFontStyle;

// pref lang id's for font prefs
// !!! needs to match the list of pref font.default.xx entries listed in all.js !!!

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

    eFontPrefLang_LangCount   = 29, // except Others and UserDefined.

    eFontPrefLang_Others      = 29, // x-unicode
    eFontPrefLang_UserDefined = 30,

    eFontPrefLang_CJKSet      = 31, // special code for CJK set
    eFontPrefLang_AllCount    = 32
};

// when searching through pref langs, max number of pref langs
const PRUint32 kMaxLenPrefLangList = 32;

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
     * Return PR_TRUE if we're to use Glitz for acceleration.
     */
    static PRBool UseGlitz();

    /**
     * Force the glitz state to on or off
     */
    static void SetUseGlitz(PRBool use);

    /**
     * Create an offscreen surface of the given dimensions
     * and image format.
     */
    virtual already_AddRefed<gfxASurface> CreateOffscreenSurface(const gfxIntSize& size,
                                                                 gfxASurface::gfxImageFormat imageFormat) = 0;


    virtual already_AddRefed<gfxASurface> OptimizeImage(gfxImageSurface *aSurface,
                                                        gfxASurface::gfxImageFormat format);

    /*
     * Font bits
     */

    /**
     * Fill aListOfFonts with the results of querying the list of font names
     * that correspond to the given language group or generic font family
     * (or both, or neither).
     */
    virtual nsresult GetFontList(const nsACString& aLangGroup,
                                 const nsACString& aGenericFamily,
                                 nsStringArray& aListOfFonts);

    /**
     * Rebuilds the any cached system font lists
     */
    virtual nsresult UpdateFontList();

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
                                          const gfxFontStyle *aStyle) = 0;

    void GetPrefFonts(const char *aLangGroup, nsString& array, PRBool aAppendUnicode = PR_TRUE);

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

    // convert a lang group string to enum constant (i.e. "zh-TW" ==> eFontPrefLang_ChineseTW)
    static eFontPrefLang GetFontPrefLangFor(const char* aLang);

    // convert a enum constant to lang group string (i.e. eFontPrefLang_ChineseTW ==> "zh-TW")
    static const char* GetPrefLangName(eFontPrefLang aLang);
   
    // returns true if a pref lang is CJK
    static PRBool IsLangCJK(eFontPrefLang aLang);
    
    // helper method to add a pref lang to an array, if not already in array
    static void AppendPrefLang(eFontPrefLang aPrefLangs[], PRUint32& aLen, eFontPrefLang aAddLang);
    
    /**
     * Are we going to try color management?
     */
    static PRBool IsCMSEnabled();

    /**
     * Return the output device ICC profile.
     */
    static cmsHPROFILE GetCMSOutputProfile();

    /**
     * Return sRGB -> output device transform.
     */
    static cmsHTRANSFORM GetCMSRGBTransform();

    /**
     * Return output -> sRGB device transform.
     */
    static cmsHTRANSFORM GetCMSInverseRGBTransform();

    /**
     * Return sRGBA -> output device transform.
     */
    static cmsHTRANSFORM GetCMSRGBATransform();

protected:
    gfxPlatform() { }
    virtual ~gfxPlatform();

private:
    virtual cmsHPROFILE GetPlatformCMSOutputProfile();
};

#endif /* GFX_PLATFORM_H */
