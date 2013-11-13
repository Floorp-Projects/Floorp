/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxPlatformMac.h"

#include "gfxImageSurface.h"
#include "gfxQuartzSurface.h"
#include "gfxQuartzImageSurface.h"
#include "mozilla/gfx/2D.h"

#include "gfxMacPlatformFontList.h"
#include "gfxMacFont.h"
#include "gfxCoreTextShaper.h"
#include "gfxUserFontSet.h"

#include "nsTArray.h"
#include "mozilla/Preferences.h"
#include "qcms.h"
#include "gfx2DGlue.h"

#include <dlfcn.h>

using namespace mozilla;
using namespace mozilla::gfx;

// cribbed from CTFontManager.h
enum {
   kAutoActivationDisabled = 1
};
typedef uint32_t AutoActivationSetting;

// bug 567552 - disable auto-activation of fonts

static void 
DisableFontActivation()
{
    // get the main bundle identifier
    CFBundleRef mainBundle = ::CFBundleGetMainBundle();
    CFStringRef mainBundleID = nullptr;

    if (mainBundle) {
        mainBundleID = ::CFBundleGetIdentifier(mainBundle);
    }

    // if possible, fetch CTFontManagerSetAutoActivationSetting
    void (*CTFontManagerSetAutoActivationSettingPtr)
            (CFStringRef, AutoActivationSetting);
    CTFontManagerSetAutoActivationSettingPtr =
        (void (*)(CFStringRef, AutoActivationSetting))
        dlsym(RTLD_DEFAULT, "CTFontManagerSetAutoActivationSetting");

    // bug 567552 - disable auto-activation of fonts
    if (CTFontManagerSetAutoActivationSettingPtr) {
        CTFontManagerSetAutoActivationSettingPtr(mainBundleID,
                                                 kAutoActivationDisabled);
    }
}

gfxPlatformMac::gfxPlatformMac()
{
    mOSXVersion = 0;
    OSXVersion();

    DisableFontActivation();
    mFontAntiAliasingThreshold = ReadAntiAliasingThreshold();

    uint32_t canvasMask = (1 << BACKEND_CAIRO) | (1 << BACKEND_SKIA) | (1 << BACKEND_COREGRAPHICS);
    uint32_t contentMask = (1 << BACKEND_COREGRAPHICS);
    InitBackendPrefs(canvasMask, BACKEND_COREGRAPHICS,
                     contentMask, BACKEND_COREGRAPHICS);
}

gfxPlatformMac::~gfxPlatformMac()
{
    gfxCoreTextShaper::Shutdown();
}

gfxPlatformFontList*
gfxPlatformMac::CreatePlatformFontList()
{
    gfxPlatformFontList* list = new gfxMacPlatformFontList();
    if (NS_SUCCEEDED(list->InitFontList())) {
        return list;
    }
    gfxPlatformFontList::Shutdown();
    return nullptr;
}

already_AddRefed<gfxASurface>
gfxPlatformMac::CreateOffscreenSurface(const gfxIntSize& size,
                                       gfxContentType contentType)
{
    nsRefPtr<gfxASurface> newSurface =
      new gfxQuartzSurface(size, OptimalFormatForContent(contentType));
    return newSurface.forget();
}

already_AddRefed<gfxASurface>
gfxPlatformMac::CreateOffscreenImageSurface(const gfxIntSize& aSize,
                                            gfxContentType aContentType)
{
    nsRefPtr<gfxASurface> surface = CreateOffscreenSurface(aSize, aContentType);
#ifdef DEBUG
    nsRefPtr<gfxImageSurface> imageSurface = surface->GetAsImageSurface();
    NS_ASSERTION(imageSurface, "Surface cannot be converted to a gfxImageSurface");
#endif
    return surface.forget();
}


already_AddRefed<gfxASurface>
gfxPlatformMac::OptimizeImage(gfxImageSurface *aSurface,
                              gfxImageFormat format)
{
    const gfxIntSize& surfaceSize = aSurface->GetSize();
    nsRefPtr<gfxImageSurface> isurf = aSurface;

    if (format != aSurface->Format()) {
        isurf = new gfxImageSurface (surfaceSize, format);
        if (!isurf->CopyFrom (aSurface)) {
            // don't even bother doing anything more
            nsRefPtr<gfxASurface> ret = aSurface;
            return ret.forget();
        }
    }

    nsRefPtr<gfxASurface> ret = new gfxQuartzImageSurface(isurf);
    return ret.forget();
}

TemporaryRef<ScaledFont>
gfxPlatformMac::GetScaledFontForFont(DrawTarget* aTarget, gfxFont *aFont)
{
    gfxMacFont *font = static_cast<gfxMacFont*>(aFont);
    return font->GetScaledFont(aTarget);
}

nsresult
gfxPlatformMac::ResolveFontName(const nsAString& aFontName,
                                FontResolverCallback aCallback,
                                void *aClosure, bool& aAborted)
{
    nsAutoString resolvedName;
    if (!gfxPlatformFontList::PlatformFontList()->
             ResolveFontName(aFontName, resolvedName)) {
        aAborted = false;
        return NS_OK;
    }
    aAborted = !(*aCallback)(resolvedName, aClosure);
    return NS_OK;
}

nsresult
gfxPlatformMac::GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName)
{
    gfxPlatformFontList::PlatformFontList()->GetStandardFamilyName(aFontName, aFamilyName);
    return NS_OK;
}

gfxFontGroup *
gfxPlatformMac::CreateFontGroup(const nsAString &aFamilies,
                                const gfxFontStyle *aStyle,
                                gfxUserFontSet *aUserFontSet)
{
    return new gfxFontGroup(aFamilies, aStyle, aUserFontSet);
}

// these will move to gfxPlatform once all platforms support the fontlist
gfxFontEntry* 
gfxPlatformMac::LookupLocalFont(const gfxProxyFontEntry *aProxyEntry,
                                const nsAString& aFontName)
{
    return gfxPlatformFontList::PlatformFontList()->LookupLocalFont(aProxyEntry, 
                                                                    aFontName);
}

gfxFontEntry* 
gfxPlatformMac::MakePlatformFont(const gfxProxyFontEntry *aProxyEntry,
                                 const uint8_t *aFontData, uint32_t aLength)
{
    // Ownership of aFontData is received here, and passed on to
    // gfxPlatformFontList::MakePlatformFont(), which must ensure the data
    // is released with NS_Free when no longer needed
    return gfxPlatformFontList::PlatformFontList()->MakePlatformFont(aProxyEntry,
                                                                     aFontData,
                                                                     aLength);
}

bool
gfxPlatformMac::IsFontFormatSupported(nsIURI *aFontURI, uint32_t aFormatFlags)
{
    // check for strange format flags
    NS_ASSERTION(!(aFormatFlags & gfxUserFontSet::FLAG_FORMAT_NOT_USED),
                 "strange font format hint set");

    // accept supported formats
    if (aFormatFlags & (gfxUserFontSet::FLAG_FORMAT_WOFF     |
                        gfxUserFontSet::FLAG_FORMAT_OPENTYPE | 
                        gfxUserFontSet::FLAG_FORMAT_TRUETYPE | 
                        gfxUserFontSet::FLAG_FORMAT_TRUETYPE_AAT)) {
        return true;
    }

    // reject all other formats, known and unknown
    if (aFormatFlags != 0) {
        return false;
    }

    // no format hint set, need to look at data
    return true;
}

// these will also move to gfxPlatform once all platforms support the fontlist
nsresult
gfxPlatformMac::GetFontList(nsIAtom *aLangGroup,
                            const nsACString& aGenericFamily,
                            nsTArray<nsString>& aListOfFonts)
{
    gfxPlatformFontList::PlatformFontList()->GetFontList(aLangGroup, aGenericFamily, aListOfFonts);
    return NS_OK;
}

nsresult
gfxPlatformMac::UpdateFontList()
{
    gfxPlatformFontList::PlatformFontList()->UpdateFontList();
    return NS_OK;
}

static const char kFontArialUnicodeMS[] = "Arial Unicode MS";
static const char kFontAppleBraille[] = "Apple Braille";
static const char kFontAppleSymbols[] = "Apple Symbols";
static const char kFontGeneva[] = "Geneva";
static const char kFontGeezaPro[] = "Geeza Pro";
static const char kFontHiraginoKakuGothic[] = "Hiragino Kaku Gothic ProN";
static const char kFontLucidaGrande[] = "Lucida Grande";
static const char kFontMenlo[] = "Menlo";
static const char kFontPlantagenetCherokee[] = "Plantagenet Cherokee";
static const char kFontSTHeiti[] = "STHeiti";

void
gfxPlatformMac::GetCommonFallbackFonts(const uint32_t aCh,
                                       int32_t aRunScript,
                                       nsTArray<const char*>& aFontList)
{
    aFontList.AppendElement(kFontLucidaGrande);

    if (!IS_IN_BMP(aCh)) {
        uint32_t p = aCh >> 16;
        if (p == 1) {
            aFontList.AppendElement(kFontAppleSymbols);
            aFontList.AppendElement(kFontGeneva);
        }
    } else {
        uint32_t b = (aCh >> 8) & 0xff;

        switch (b) {
        case 0x03:
        case 0x05:
            aFontList.AppendElement(kFontGeneva);
            break;
        case 0x07:
            aFontList.AppendElement(kFontGeezaPro);
            break;
        case 0x10:
            aFontList.AppendElement(kFontMenlo);
            break;
        case 0x13:  // Cherokee
            aFontList.AppendElement(kFontPlantagenetCherokee);
            break;
        case 0x18:  // Mongolian
            aFontList.AppendElement(kFontSTHeiti);
            break;
        case 0x1d:
        case 0x1e:
            aFontList.AppendElement(kFontGeneva);
            break;
        case 0x20:  // Symbol ranges
        case 0x21:
        case 0x22:
        case 0x23:
        case 0x24:
        case 0x25:
        case 0x26:
        case 0x27:
        case 0x29:
        case 0x2a:
        case 0x2b:
        case 0x2e:
            aFontList.AppendElement(kFontAppleSymbols);
            aFontList.AppendElement(kFontMenlo);
            aFontList.AppendElement(kFontGeneva);
            aFontList.AppendElement(kFontHiraginoKakuGothic);
            break;
        case 0x2c:
        case 0x2d:
            aFontList.AppendElement(kFontGeneva);
            break;
        case 0x28:  // Braille
            aFontList.AppendElement(kFontAppleBraille);
            break;
        case 0x4d:
            aFontList.AppendElement(kFontAppleSymbols);
            break;
        case 0xa0:  // Yi
        case 0xa1:
        case 0xa2:
        case 0xa3:
        case 0xa4:
            aFontList.AppendElement(kFontSTHeiti);
            break;
        case 0xa6:
        case 0xa7:
            aFontList.AppendElement(kFontGeneva);
            aFontList.AppendElement(kFontAppleSymbols);
            break;
        case 0xfc:
        case 0xff:
            aFontList.AppendElement(kFontAppleSymbols);
            break;
        default:
            break;
        }
    }

    // Arial Unicode MS has lots of glyphs for obscure, use it as a last resort
    aFontList.AppendElement(kFontArialUnicodeMS);
}


int32_t 
gfxPlatformMac::OSXVersion()
{
    if (!mOSXVersion) {
        // minor version is not accurate, use gestaltSystemVersionMajor, gestaltSystemVersionMinor, gestaltSystemVersionBugFix for these
        OSErr err = ::Gestalt(gestaltSystemVersion, reinterpret_cast<SInt32*>(&mOSXVersion));
        if (err != noErr) {
            //This should probably be changed when our minimum version changes
            NS_ERROR("Couldn't determine OS X version, assuming 10.6");
            mOSXVersion = MAC_OS_X_VERSION_10_6_HEX;
        }
    }
    return mOSXVersion;
}

uint32_t
gfxPlatformMac::ReadAntiAliasingThreshold()
{
    uint32_t threshold = 0;  // default == no threshold
    
    // first read prefs flag to determine whether to use the setting or not
    bool useAntiAliasingThreshold = Preferences::GetBool("gfx.use_text_smoothing_setting", false);

    // if the pref setting is disabled, return 0 which effectively disables this feature
    if (!useAntiAliasingThreshold)
        return threshold;
        
    // value set via Appearance pref panel, "Turn off text smoothing for font sizes xxx and smaller"
    CFNumberRef prefValue = (CFNumberRef)CFPreferencesCopyAppValue(CFSTR("AppleAntiAliasingThreshold"), kCFPreferencesCurrentApplication);

    if (prefValue) {
        if (!CFNumberGetValue(prefValue, kCFNumberIntType, &threshold)) {
            threshold = 0;
        }
        CFRelease(prefValue);
    }

    return threshold;
}

already_AddRefed<gfxASurface>
gfxPlatformMac::CreateThebesSurfaceAliasForDrawTarget_hack(mozilla::gfx::DrawTarget *aTarget)
{
  if (aTarget->GetType() == BACKEND_COREGRAPHICS) {
    CGContextRef cg = static_cast<CGContextRef>(aTarget->GetNativeSurface(NATIVE_SURFACE_CGCONTEXT));
    unsigned char* data = (unsigned char*)CGBitmapContextGetData(cg);
    size_t bpp = CGBitmapContextGetBitsPerPixel(cg);
    size_t stride = CGBitmapContextGetBytesPerRow(cg);
    gfxIntSize size(aTarget->GetSize().width, aTarget->GetSize().height);
    nsRefPtr<gfxImageSurface> imageSurface = new gfxImageSurface(data, size, stride, bpp == 2
                                                                                     ? gfxImageFormatRGB16_565
                                                                                     : gfxImageFormatARGB32);
    // Here we should return a gfxQuartzImageSurface but quartz will assumes that image surfaces
    // don't change which wont create a proper alias to the draw target, therefore we have to
    // return a plain image surface.
    return imageSurface.forget();
  } else {
    return GetThebesSurfaceForDrawTarget(aTarget);
  }
}

already_AddRefed<gfxASurface>
gfxPlatformMac::GetThebesSurfaceForDrawTarget(DrawTarget *aTarget)
{
  if (aTarget->GetType() == BACKEND_COREGRAPHICS_ACCELERATED) {
    RefPtr<SourceSurface> source = aTarget->Snapshot();
    RefPtr<DataSourceSurface> sourceData = source->GetDataSurface();
    unsigned char* data = sourceData->GetData();
    nsRefPtr<gfxImageSurface> surf = new gfxImageSurface(data, ThebesIntSize(sourceData->GetSize()), sourceData->Stride(),
                                                         gfxImageFormatARGB32);
    // We could fix this by telling gfxImageSurface it owns data.
    nsRefPtr<gfxImageSurface> cpy = new gfxImageSurface(ThebesIntSize(sourceData->GetSize()), gfxImageFormatARGB32);
    cpy->CopyFrom(surf);
    return cpy.forget();
  } else if (aTarget->GetType() == BACKEND_COREGRAPHICS) {
    CGContextRef cg = static_cast<CGContextRef>(aTarget->GetNativeSurface(NATIVE_SURFACE_CGCONTEXT));

    //XXX: it would be nice to have an implicit conversion from IntSize to gfxIntSize
    IntSize intSize = aTarget->GetSize();
    gfxIntSize size(intSize.width, intSize.height);

    nsRefPtr<gfxASurface> surf =
      new gfxQuartzSurface(cg, size);

    return surf.forget();
  }

  return gfxPlatform::GetThebesSurfaceForDrawTarget(aTarget);
}

bool
gfxPlatformMac::UseAcceleratedCanvas()
{
  // Lion or later is required
  return OSXVersion() >= 0x1070 && Preferences::GetBool("gfx.canvas.azure.accelerated", false);
}

bool
gfxPlatformMac::SupportsOffMainThreadCompositing()
{
  return true;
}

qcms_profile *
gfxPlatformMac::GetPlatformCMSOutputProfile()
{
    CGColorSpaceRef cspace = ::CGDisplayCopyColorSpace(::CGMainDisplayID());
    if (!cspace) {
        cspace = ::CGColorSpaceCreateDeviceRGB();
    }
    if (!cspace) {
        return nullptr;
    }

    CFDataRef iccp = ::CGColorSpaceCopyICCProfile(cspace);

    ::CFRelease(cspace);

    if (!iccp) {
        return nullptr;
    }

    qcms_profile* profile = qcms_profile_from_memory(::CFDataGetBytePtr(iccp), static_cast<size_t>(::CFDataGetLength(iccp)));

    ::CFRelease(iccp);

    return profile;
}
