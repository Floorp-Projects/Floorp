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

#include "gfxPlatform.h"

#if defined(XP_WIN)
#include "gfxWindowsPlatform.h"
#elif defined(XP_MACOSX)
#include "gfxPlatformMac.h"
#include "gfxQuartzFontCache.h"
#elif defined(MOZ_WIDGET_GTK2)
#include "gfxPlatformGtk.h"
#elif defined(MOZ_WIDGET_QT)
#include "gfxQtPlatform.h"
#elif defined(XP_BEOS)
#include "gfxBeOSPlatform.h"
#elif defined(XP_OS2)
#include "gfxOS2Platform.h"
#endif

#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "gfxTextRunCache.h"
#include "gfxTextRunWordCache.h"
#include "gfxUserFontSet.h"

#include "nsIPref.h"
#include "nsServiceManagerUtils.h"

#include "nsWeakReference.h"

#include "cairo.h"
#include "lcms.h"

#include "plstr.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranch2.h"

gfxPlatform *gPlatform = nsnull;

// These two may point to the same profile
static cmsHPROFILE gCMSOutputProfile = nsnull;
static cmsHPROFILE gCMSsRGBProfile = nsnull;

static cmsHTRANSFORM gCMSRGBTransform = nsnull;
static cmsHTRANSFORM gCMSInverseRGBTransform = nsnull;
static cmsHTRANSFORM gCMSRGBATransform = nsnull;

static PRBool gCMSInitialized = PR_FALSE;
static eCMSMode gCMSMode = eCMSMode_Off;
static int gCMSIntent = -2;

static const char *CMPrefName = "gfx.color_management.mode";
static const char *CMPrefNameOld = "gfx.color_management.enabled";
static const char *CMIntentPrefName = "gfx.color_management.rendering_intent";
static const char *CMProfilePrefName = "gfx.color_management.display_profile";
static const char *CMForceSRGBPrefName = "gfx.color_management.force_srgb";

static void ShutdownCMS();
static void MigratePrefs();

/* Class to listen for pref changes so that chrome code can dynamically
   force sRGB as an output profile. See Bug #452125. */
class SRGBOverrideObserver : public nsIObserver,
                             public nsSupportsWeakReference
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
};

NS_IMPL_ISUPPORTS2(SRGBOverrideObserver, nsIObserver, nsISupportsWeakReference)

NS_IMETHODIMP
SRGBOverrideObserver::Observe(nsISupports *aSubject,
                              const char *aTopic,
                              const PRUnichar *someData)
{
    NS_ASSERTION(NS_strcmp(someData,
                   NS_LITERAL_STRING("gfx.color_mangement.force_srgb").get()),
                 "Restarting CMS on wrong pref!");
    ShutdownCMS();
    return NS_OK;
}


// this needs to match the list of pref font.default.xx entries listed in all.js!
// the order *must* match the order in eFontPrefLang
static const char *gPrefLangNames[] = {
    "x-western",
    "x-central-euro",
    "ja",
    "zh-TW",
    "zh-CN",
    "zh-HK",
    "ko",
    "x-cyrillic",
    "x-baltic",
    "el",
    "tr",
    "th",
    "he",
    "ar",
    "x-devanagari",
    "x-tamil",
    "x-armn",
    "x-beng",
    "x-cans",
    "x-ethi",
    "x-geor",
    "x-gujr",
    "x-guru",
    "x-khmr",
    "x-mlym",
    "x-orya",
    "x-telu",
    "x-knda",
    "x-sinh",
    "x-unicode",
    "x-user-def"
};


gfxPlatform*
gfxPlatform::GetPlatform()
{
    return gPlatform;
}

nsresult
gfxPlatform::Init()
{
    NS_ASSERTION(!gPlatform, "Already started???");
#if defined(XP_WIN)
    gPlatform = new gfxWindowsPlatform;
#elif defined(XP_MACOSX)
    gPlatform = new gfxPlatformMac;
#elif defined(MOZ_WIDGET_GTK2)
    gPlatform = new gfxPlatformGtk;
#elif defined(MOZ_WIDGET_QT)
    gPlatform = new gfxQtPlatform;
#elif defined(XP_BEOS)
    gPlatform = new gfxBeOSPlatform;
#elif defined(XP_OS2)
    gPlatform = new gfxOS2Platform;
#endif
    if (!gPlatform)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;

#if defined(XP_MACOSX)
    rv = gfxQuartzFontCache::Init();
    if (NS_FAILED(rv)) {
        NS_ERROR("Could not initialize gfxQuartzFontCache");
        Shutdown();
        return rv;
    }
#endif

    rv = gfxFontCache::Init();
    if (NS_FAILED(rv)) {
        NS_ERROR("Could not initialize gfxFontCache");
        Shutdown();
        return rv;
    }

    rv = gfxTextRunWordCache::Init();
    if (NS_FAILED(rv)) {
        NS_ERROR("Could not initialize gfxTextRunWordCache");
        Shutdown();
        return rv;
    }

    rv = gfxTextRunCache::Init();
    if (NS_FAILED(rv)) {
        NS_ERROR("Could not initialize gfxTextRunCache");
        Shutdown();
        return rv;
    }

    /* Pref migration hook. */
    MigratePrefs();

    /* Create and register our CMS Override observer. */
    gPlatform->overrideObserver = new SRGBOverrideObserver();
    nsCOMPtr<nsIPrefBranch2> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (prefs)
        prefs->AddObserver(CMForceSRGBPrefName, gPlatform->overrideObserver, PR_TRUE);

    /* By default, LCMS calls exit() on error, which isn't what we want. If
       cms is enabled, change the error functionality. */
    if (GetCMSMode() != eCMSMode_Off) {
#ifdef DEBUG
        cmsErrorAction(LCMS_ERROR_SHOW);
#else
        cmsErrorAction(LCMS_ERROR_IGNORE);
#endif
    }

    return NS_OK;
}

void
gfxPlatform::Shutdown()
{
    // These may be called before the corresponding subsystems have actually
    // started up. That's OK, they can handle it.
    gfxTextRunCache::Shutdown();
    gfxTextRunWordCache::Shutdown();
    gfxFontCache::Shutdown();
#if defined(XP_MACOSX)
    gfxQuartzFontCache::Shutdown();
#endif

    // Free the various non-null transforms and loaded profiles
    ShutdownCMS();

    /* Unregister our CMS Override callback. */
    nsCOMPtr<nsIPrefBranch2> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (prefs)
        prefs->RemoveObserver(CMForceSRGBPrefName, gPlatform->overrideObserver);
    
    delete gPlatform;
    gPlatform = nsnull;
}

gfxPlatform::~gfxPlatform()
{
    // The cairo folks think we should only clean up in debug builds,
    // but we're generally in the habit of trying to shut down as
    // cleanly as possible even in production code, so call this
    // cairo_debug_* function unconditionally.
    //
    // because cairo can assert and thus crash on shutdown, don't do this in release builds
#if MOZ_TREE_CAIRO && (defined(DEBUG) || defined(NS_BUILD_REFCNT_LOGGING) || defined(NS_TRACE_MALLOC))
    cairo_debug_reset_static_data();
#endif

#if 0
    // It would be nice to do this (although it might need to be after
    // the cairo shutdown that happens in ~gfxPlatform).  It even looks
    // idempotent.  But it has fatal assertions that fire if stuff is
    // leaked, and we hit them.
    FcFini();
#endif
}

already_AddRefed<gfxASurface>
gfxPlatform::OptimizeImage(gfxImageSurface *aSurface,
                           gfxASurface::gfxImageFormat format)
{
    const gfxIntSize& surfaceSize = aSurface->GetSize();

    nsRefPtr<gfxASurface> optSurface = CreateOffscreenSurface(surfaceSize, format);
    if (!optSurface || optSurface->CairoStatus() != 0)
        return nsnull;

    gfxContext tmpCtx(optSurface);
    tmpCtx.SetOperator(gfxContext::OPERATOR_SOURCE);
    tmpCtx.SetSource(aSurface);
    tmpCtx.Paint();

    gfxASurface *ret = optSurface;
    NS_ADDREF(ret);
    return ret;
}

nsresult
gfxPlatform::GetFontList(const nsACString& aLangGroup,
                         const nsACString& aGenericFamily,
                         nsStringArray& aListOfFonts)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
gfxPlatform::UpdateFontList()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

#define GFX_DOWNLOADABLE_FONTS_ENABLED "gfx.downloadable_fonts.enabled"

PRBool
gfxPlatform::DownloadableFontsEnabled()
{
    static PRBool initialized = PR_FALSE;
    static PRBool allowDownloadableFonts = PR_FALSE;

    if (initialized == PR_FALSE) {
        initialized = PR_TRUE;
        nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
        if (prefs) {
            PRBool allow;
            nsresult rv = prefs->GetBoolPref(GFX_DOWNLOADABLE_FONTS_ENABLED, &allow);
            if (NS_SUCCEEDED(rv))
                allowDownloadableFonts = allow;
        }
    }

    return allowDownloadableFonts;
}


static void
AppendGenericFontFromPref(nsString& aFonts, const char *aLangGroup, const char *aGenericName)
{
    nsresult rv;

    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
    if (!prefs)
        return;

    nsCAutoString prefName;
    nsXPIDLString nameValue, nameListValue;

    nsXPIDLString genericName;
    if (aGenericName) {
        genericName = NS_ConvertASCIItoUTF16(aGenericName);
    } else {
        prefName.AssignLiteral("font.default.");
        prefName.Append(aLangGroup);
        prefs->CopyUnicharPref(prefName.get(), getter_Copies(genericName));
    }

    nsCAutoString genericDotLang;
    genericDotLang.Assign(NS_ConvertUTF16toUTF8(genericName));
    genericDotLang.AppendLiteral(".");
    genericDotLang.Append(aLangGroup);

    // fetch font.name.xxx value                   
    prefName.AssignLiteral("font.name.");
    prefName.Append(genericDotLang);
    rv = prefs->CopyUnicharPref(prefName.get(), getter_Copies(nameValue));
    if (NS_SUCCEEDED(rv)) {
        if (!aFonts.IsEmpty())
            aFonts.AppendLiteral(", ");
        aFonts.Append(nameValue);
    }

    // fetch font.name-list.xxx value                   
    prefName.AssignLiteral("font.name-list.");
    prefName.Append(genericDotLang);
    rv = prefs->CopyUnicharPref(prefName.get(), getter_Copies(nameListValue));
    if (NS_SUCCEEDED(rv) && !nameListValue.Equals(nameValue)) {
        if (!aFonts.IsEmpty())
            aFonts.AppendLiteral(", ");
        aFonts.Append(nameListValue);
    }
}

void
gfxPlatform::GetPrefFonts(const char *aLangGroup, nsString& aFonts, PRBool aAppendUnicode)
{
    aFonts.Truncate();

    AppendGenericFontFromPref(aFonts, aLangGroup, nsnull);
    if (aAppendUnicode)
        AppendGenericFontFromPref(aFonts, "x-unicode", nsnull);
}

PRBool gfxPlatform::ForEachPrefFont(eFontPrefLang aLangArray[], PRUint32 aLangArrayLen, PrefFontCallback aCallback,
                                    void *aClosure)
{
    nsresult rv;

    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
    if (!prefs)
        return PR_FALSE;

    PRUint32    i;
    
    for (i = 0; i < aLangArrayLen; i++) {
        eFontPrefLang prefLang = aLangArray[i];
        const char *langGroup = GetPrefLangName(prefLang);
        
        nsCAutoString prefName;
        nsXPIDLString nameValue, nameListValue;
    
        nsXPIDLString genericName;
        prefName.AssignLiteral("font.default.");
        prefName.Append(langGroup);
        prefs->CopyUnicharPref(prefName.get(), getter_Copies(genericName));
    
        nsCAutoString genericDotLang;
        genericDotLang.Assign(NS_ConvertUTF16toUTF8(genericName));
        genericDotLang.AppendLiteral(".");
        genericDotLang.Append(langGroup);
    
        // fetch font.name.xxx value                   
        prefName.AssignLiteral("font.name.");
        prefName.Append(genericDotLang);
        rv = prefs->CopyUnicharPref(prefName.get(), getter_Copies(nameValue));
        if (NS_SUCCEEDED(rv)) {
            if (!aCallback(prefLang, nameValue, aClosure))
                return PR_FALSE;
        }
    
        // fetch font.name-list.xxx value                   
        prefName.AssignLiteral("font.name-list.");
        prefName.Append(genericDotLang);
        rv = prefs->CopyUnicharPref(prefName.get(), getter_Copies(nameListValue));
        if (NS_SUCCEEDED(rv) && !nameListValue.Equals(nameValue)) {
            if (!aCallback(prefLang, nameListValue, aClosure))
                return PR_FALSE;
        }
    }

    return PR_TRUE;
}

eFontPrefLang
gfxPlatform::GetFontPrefLangFor(const char* aLang)
{
    if (!aLang || !aLang[0])
        return eFontPrefLang_Others;
    for (PRUint32 i = 0; i < PRUint32(eFontPrefLang_LangCount); ++i) {
        if (!PL_strcasecmp(gPrefLangNames[i], aLang))
            return eFontPrefLang(i);
    }
    return eFontPrefLang_Others;
}

const char*
gfxPlatform::GetPrefLangName(eFontPrefLang aLang)
{
    if (PRUint32(aLang) < PRUint32(eFontPrefLang_AllCount))
        return gPrefLangNames[PRUint32(aLang)];
    return nsnull;
}

const PRUint32 kFontPrefLangCJKMask = (1 << (PRUint32) eFontPrefLang_Japanese) | (1 << (PRUint32) eFontPrefLang_ChineseTW)
                                      | (1 << (PRUint32) eFontPrefLang_ChineseCN) | (1 << (PRUint32) eFontPrefLang_ChineseHK)
                                      | (1 << (PRUint32) eFontPrefLang_Korean) | (1 << (PRUint32) eFontPrefLang_CJKSet);
PRBool 
gfxPlatform::IsLangCJK(eFontPrefLang aLang)
{
    return kFontPrefLangCJKMask & (1 << (PRUint32) aLang);
}

void 
gfxPlatform::AppendPrefLang(eFontPrefLang aPrefLangs[], PRUint32& aLen, eFontPrefLang aAddLang)
{
    if (aLen >= kMaxLenPrefLangList) return;
    
    // make sure
    PRUint32  i = 0;
    while (i < aLen && aPrefLangs[i] != aAddLang) {
        i++;
    }
    
    if (i == aLen) {
        aPrefLangs[aLen] = aAddLang;
        aLen++;
    }
}

eCMSMode
gfxPlatform::GetCMSMode()
{
    if (gCMSInitialized == PR_FALSE) {
        gCMSInitialized = PR_TRUE;
        nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
        if (prefs) {
            PRInt32 mode;
            nsresult rv =
                prefs->GetIntPref(CMPrefName, &mode);
            if (NS_SUCCEEDED(rv) && (mode >= 0) && (mode < eCMSMode_AllCount)) {
                gCMSMode = static_cast<eCMSMode>(mode);
            }
        }
    }
    return gCMSMode;
}

/* Chris Murphy (CM consultant) suggests this as a default in the event that we
cannot reproduce relative + Black Point Compensation.  BPC brings an
unacceptable performance overhead, so we go with perceptual. */
#define INTENT_DEFAULT INTENT_PERCEPTUAL

PRBool
gfxPlatform::GetRenderingIntent()
{
    if (gCMSIntent == -2) {

        /* Try to query the pref system for a rendering intent. */
        nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
        if (prefs) {
            PRInt32 pIntent;
            nsresult rv = prefs->GetIntPref(CMIntentPrefName, &pIntent);
            if (NS_SUCCEEDED(rv)) {
              
                /* If the pref is within range, use it as an override. */
                if ((pIntent >= INTENT_MIN) && (pIntent <= INTENT_MAX))
                    gCMSIntent = pIntent;

                /* If the pref is out of range, use embedded profile. */
                else
                    gCMSIntent = -1;
            }
        }

        /* If we didn't get a valid intent from prefs, use the default. */
        if (gCMSIntent == -2) 
            gCMSIntent = INTENT_DEFAULT;
    }
    return gCMSIntent;
}

void 
gfxPlatform::TransformPixel(const gfxRGBA& in, gfxRGBA& out, cmsHTRANSFORM transform)
{

    if (transform) {
#ifdef IS_LITTLE_ENDIAN
        PRUint32 packed = in.Packed(gfxRGBA::PACKED_ABGR);
        cmsDoTransform(transform,
                       (PRUint8 *)&packed, (PRUint8 *)&packed,
                       1);
        out.~gfxRGBA();
        new (&out) gfxRGBA(packed, gfxRGBA::PACKED_ABGR);
#else
        PRUint32 packed = in.Packed(gfxRGBA::PACKED_ARGB);
        cmsDoTransform(transform,
                       (PRUint8 *)&packed + 1, (PRUint8 *)&packed + 1,
                       1);
        out.~gfxRGBA();
        new (&out) gfxRGBA(packed, gfxRGBA::PACKED_ARGB);
#endif
    }

    else if (&out != &in)
        out = in;
}

cmsHPROFILE
gfxPlatform::GetPlatformCMSOutputProfile()
{
    return nsnull;
}

cmsHPROFILE
gfxPlatform::GetCMSOutputProfile()
{
    if (!gCMSOutputProfile) {

        nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
        if (prefs) {

            nsresult rv;

            /* Determine if we're using the internal override to force sRGB as
               an output profile for reftests. See Bug 452125. */
            PRBool hasSRGBOverride, doSRGBOverride;
            rv = prefs->PrefHasUserValue(CMForceSRGBPrefName, &hasSRGBOverride);
            if (NS_SUCCEEDED(rv) && hasSRGBOverride) {
                rv = prefs->GetBoolPref(CMForceSRGBPrefName, &doSRGBOverride);
                if (NS_SUCCEEDED(rv) && doSRGBOverride)
                    gCMSOutputProfile = GetCMSsRGBProfile();
            }

            if (!gCMSOutputProfile) {

                nsXPIDLCString fname;
                rv = prefs->GetCharPref(CMProfilePrefName,
                                        getter_Copies(fname));
                if (NS_SUCCEEDED(rv) && !fname.IsEmpty()) {
                    gCMSOutputProfile = cmsOpenProfileFromFile(fname, "r");
                }
            }
        }

        if (!gCMSOutputProfile) {
            gCMSOutputProfile =
                gfxPlatform::GetPlatform()->GetPlatformCMSOutputProfile();
        }

        /* Determine if the profile looks bogus. If so, close the profile
         * and use sRGB instead. See bug 460629, */
        if (gCMSOutputProfile && cmsProfileIsBogus(gCMSOutputProfile)) {
            NS_ASSERTION(gCMSOutputProfile != GetCMSsRGBProfile(),
                         "Builtin sRGB profile tagged as bogus!!!");
            cmsCloseProfile(gCMSOutputProfile);
            gCMSOutputProfile = nsnull;
        }

        if (!gCMSOutputProfile) {
            gCMSOutputProfile = GetCMSsRGBProfile();
        }

        /* Precache the LUT16 Interpolations for the output profile. See 
           bug 444661 for details. */
        cmsPrecacheProfile(gCMSOutputProfile, CMS_PRECACHE_LI168_REVERSE);
    }

    return gCMSOutputProfile;
}

cmsHPROFILE
gfxPlatform::GetCMSsRGBProfile()
{
    if (!gCMSsRGBProfile) {

        /* Create the profile using lcms. */
        gCMSsRGBProfile = cmsCreate_sRGBProfile();

        /* Precache the Fixed-point Interpolations for sRGB as an input
           profile. See bug 444661 for details. */
        cmsPrecacheProfile(gCMSsRGBProfile, CMS_PRECACHE_LI16F_FORWARD);
    }
    return gCMSsRGBProfile;
}

cmsHTRANSFORM
gfxPlatform::GetCMSRGBTransform()
{
    if (!gCMSRGBTransform) {
        cmsHPROFILE inProfile, outProfile;
        outProfile = GetCMSOutputProfile();
        inProfile = GetCMSsRGBProfile();

        if (!inProfile || !outProfile)
            return nsnull;

        gCMSRGBTransform = cmsCreateTransform(inProfile, TYPE_RGB_8,
                                              outProfile, TYPE_RGB_8,
                                              INTENT_PERCEPTUAL, cmsFLAGS_FLOATSHAPER);
    }

    return gCMSRGBTransform;
}

cmsHTRANSFORM
gfxPlatform::GetCMSInverseRGBTransform()
{
    if (!gCMSInverseRGBTransform) {
        cmsHPROFILE inProfile, outProfile;
        inProfile = GetCMSOutputProfile();
        outProfile = GetCMSsRGBProfile();

        if (!inProfile || !outProfile)
            return nsnull;

        gCMSInverseRGBTransform = cmsCreateTransform(inProfile, TYPE_RGB_8,
                                                     outProfile, TYPE_RGB_8,
                                                     INTENT_PERCEPTUAL, cmsFLAGS_FLOATSHAPER);
    }

    return gCMSInverseRGBTransform;
}

cmsHTRANSFORM
gfxPlatform::GetCMSRGBATransform()
{
    if (!gCMSRGBATransform) {
        cmsHPROFILE inProfile, outProfile;
        outProfile = GetCMSOutputProfile();
        inProfile = GetCMSsRGBProfile();

        if (!inProfile || !outProfile)
            return nsnull;

        gCMSRGBATransform = cmsCreateTransform(inProfile, TYPE_RGBA_8,
                                               outProfile, TYPE_RGBA_8,
                                               INTENT_PERCEPTUAL, cmsFLAGS_FLOATSHAPER);
    }

    return gCMSRGBATransform;
}

/* Shuts down various transforms and profiles for CMS. */
static void ShutdownCMS()
{

    if (gCMSRGBTransform) {
        cmsDeleteTransform(gCMSRGBTransform);
        gCMSRGBTransform = nsnull;
    }
    if (gCMSInverseRGBTransform) {
        cmsDeleteTransform(gCMSInverseRGBTransform);
        gCMSInverseRGBTransform = nsnull;
    }
    if (gCMSRGBATransform) {
        cmsDeleteTransform(gCMSRGBATransform);
        gCMSRGBATransform = nsnull;
    }
    if (gCMSOutputProfile) {
        cmsCloseProfile(gCMSOutputProfile);

        // handle the aliased case
        if (gCMSsRGBProfile == gCMSOutputProfile)
            gCMSsRGBProfile = nsnull;
        gCMSOutputProfile = nsnull;
    }
    if (gCMSsRGBProfile) {
        cmsCloseProfile(gCMSsRGBProfile);
        gCMSsRGBProfile = nsnull;
    }

    // Reset the state variables
    gCMSIntent = -2;
    gCMSMode = eCMSMode_Off;
    gCMSInitialized = PR_FALSE;
}

static void MigratePrefs()
{

    /* Load the pref service. If we don't get it die quietly since this isn't
       critical code. */
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (!prefs)
        return;

    /* Migrate from the boolean color_management.enabled pref - we now use
       color_management.mode. */
    PRBool hasOldCMPref;
    nsresult rv =
        prefs->PrefHasUserValue(CMPrefNameOld, &hasOldCMPref);
    if (NS_SUCCEEDED(rv) && (hasOldCMPref == PR_TRUE)) {
        PRBool CMWasEnabled;
        rv = prefs->GetBoolPref(CMPrefNameOld, &CMWasEnabled);
        if (NS_SUCCEEDED(rv) && (CMWasEnabled == PR_TRUE))
            prefs->SetIntPref(CMPrefName, eCMSMode_All);
        prefs->ClearUserPref(CMPrefNameOld);
    }

}
