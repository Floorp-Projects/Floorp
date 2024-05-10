/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/core/SkTypes.h"
#if defined(SK_BUILD_FOR_MAC) || defined(SK_BUILD_FOR_IOS)

#include "src/utils/mac/SkCTFontCreateExactCopy.h"

#include "src/ports/SkTypeface_mac_ct.h"
#include "src/utils/mac/SkUniqueCFRef.h"

#if defined(MOZ_SKIA) && defined(XP_MACOSX)
#include "nsCocoaFeatures.h"
#endif

// In macOS 10.12 and later any variation on the CGFont which has default axis value will be
// dropped when creating the CTFont. Unfortunately, in macOS 10.15 the priority of setting
// the optical size (and opsz variation) is
// 1. the value of kCTFontOpticalSizeAttribute in the CTFontDescriptor (undocumented)
// 2. the opsz axis default value if kCTFontOpticalSizeAttribute is 'none' (undocumented)
// 3. the opsz variation on the nascent CTFont from the CGFont (was dropped if default)
// 4. the opsz variation in kCTFontVariationAttribute in CTFontDescriptor (crashes 10.10)
// 5. the size requested (can fudge in SkTypeface but not SkScalerContext)
// The first one which is found will be used to set the opsz variation (after clamping).
static void add_opsz_attr(CFMutableDictionaryRef attr, double opsz) {
    SkUniqueCFRef<CFNumberRef> opszValueNumber(
        CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &opsz));
    // Avoid using kCTFontOpticalSizeAttribute directly
    CFStringRef SkCTFontOpticalSizeAttribute = CFSTR("NSCTFontOpticalSizeAttribute");
    CFDictionarySetValue(attr, SkCTFontOpticalSizeAttribute, opszValueNumber.get());
}

// This turns off application of the 'trak' table to advances, but also all other tracking.
static void add_notrak_attr(CFMutableDictionaryRef attr) {
    int zero = 0;
    SkUniqueCFRef<CFNumberRef> unscaledTrackingNumber(
        CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &zero));
    CFStringRef SkCTFontUnscaledTrackingAttribute = CFSTR("NSCTFontUnscaledTrackingAttribute");
    CFDictionarySetValue(attr, SkCTFontUnscaledTrackingAttribute, unscaledTrackingNumber.get());
}

SkUniqueCFRef<CTFontRef> SkCTFontCreateExactCopy(CTFontRef baseFont, CGFloat textSize,
                                                 OpszVariation opszVariation)
{
    SkUniqueCFRef<CFMutableDictionaryRef> attr(
    CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                              &kCFTypeDictionaryKeyCallBacks,
                              &kCFTypeDictionaryValueCallBacks));

    if (opszVariation.isSet) {
        add_opsz_attr(attr.get(), opszVariation.value);
#ifdef MOZ_SKIA
    }
#else
    } else {
        // On (at least) 10.10 though 10.14 the default system font was SFNSText/SFNSDisplay.
        // The CTFont is backed by both; optical size < 20 means SFNSText else SFNSDisplay.
        // On at least 10.11 the glyph ids in these fonts became non-interchangable.
        // To keep glyph ids stable over size changes, preserve the optical size.
        // In 10.15 this was replaced with use of variable fonts with an opsz axis.
        // A CTFont backed by multiple fonts picked by opsz where the multiple backing fonts are
        // variable fonts with opsz axis and non-interchangeable glyph ids would break the
        // opsz.isSet branch above, but hopefully that never happens.
        // See https://crbug.com/524646 .
        CFStringRef SkCTFontOpticalSizeAttribute = CFSTR("NSCTFontOpticalSizeAttribute");
        SkUniqueCFRef<CFTypeRef> opsz(CTFontCopyAttribute(baseFont, SkCTFontOpticalSizeAttribute));
        double opsz_val;
        if (!opsz ||
            CFGetTypeID(opsz.get()) != CFNumberGetTypeID() ||
            !CFNumberGetValue(static_cast<CFNumberRef>(opsz.get()),kCFNumberDoubleType,&opsz_val) ||
            opsz_val <= 0)
        {
            opsz_val = CTFontGetSize(baseFont);
        }
        add_opsz_attr(attr.get(), opsz_val);
    }
    add_notrak_attr(attr.get());
#endif

    // To figure out if a font is installed locally or used from a @font-face
    // resource, we check whether its descriptor can provide a URL. This will
    // be present for installed fonts, but not for those activated from an
    // in-memory resource.
    auto IsInstalledFont = [](CTFontRef aFont) {
        CTFontDescriptorRef desc = CTFontCopyFontDescriptor(aFont);
        CFTypeRef attr = CTFontDescriptorCopyAttribute(desc, kCTFontURLAttribute);
        CFRelease(desc);
        bool result = false;
        if (attr) {
            result = true;
            CFRelease(attr);
        }
        return result;
    };

    SkUniqueCFRef<CGFontRef> baseCGFont;

    // If we have a system font we need to use the CGFont APIs to avoid having the
    // underlying font change for us when using CTFontCreateCopyWithAttributes.
    CFDictionaryRef variations = nullptr;
    if (IsInstalledFont(baseFont)) {
        baseCGFont.reset(CTFontCopyGraphicsFont(baseFont, nullptr));

        // The last parameter (CTFontDescriptorRef attributes) *must* be nullptr.
        // If non-nullptr then with fonts with variation axes, the copy will fail in
        // CGFontVariationFromDictCallback when it assumes kCGFontVariationAxisName is CFNumberRef
        // which it quite obviously is not.

        // Because we cannot setup the CTFont descriptor to match, the same restriction applies here
        // as other uses of CTFontCreateWithGraphicsFont which is that such CTFonts should not escape
        // the scaler context, since they aren't 'normal'.

        // Avoid calling potentially buggy variation APIs on pre-Sierra macOS
        // versions (see bug 1331683).
        //
        // And on HighSierra, CTFontCreateWithGraphicsFont properly carries over
        // variation settings from the CGFont to CTFont, so we don't need to do
        // the extra work here -- and this seems to avoid Core Text crashiness
        // seen in bug 1454094.
        //
        // However, for installed fonts it seems we DO need to copy the variations
        // explicitly even on 10.13, otherwise fonts fail to render (as in bug
        // 1455494) when non-default values are used. Fortunately, the crash
        // mentioned above occurs with data fonts, not (AFAICT) with system-
        // installed fonts.
        //
        // So we only need to do this "the hard way" on Sierra, and for installed
        // fonts on HighSierra+; otherwise, just let the standard CTFont function
        // do its thing.
        //
        // NOTE in case this ever needs further adjustment: there is similar logic
        // in four places in the tree (sadly):
        //    CreateCTFontFromCGFontWithVariations in gfxMacFont.cpp
        //    CreateCTFontFromCGFontWithVariations in ScaledFontMac.cpp
        //    CreateCTFontFromCGFontWithVariations in cairo-quartz-font.c
        //    ctfont_create_exact_copy in SkFontHost_mac.cpp

        // Not UniqueCFRef<> because CGFontCopyVariations can return null!
        variations = CGFontCopyVariations(baseCGFont.get());
        if (variations) {
            CFDictionarySetValue(attr.get(), kCTFontVariationAttribute, variations);
        }
    }

    SkUniqueCFRef<CTFontDescriptorRef> desc(CTFontDescriptorCreateWithAttributes(attr.get()));

    if (baseCGFont.get()) {
        auto ctFont = SkUniqueCFRef<CTFontRef>(
            CTFontCreateWithGraphicsFont(baseCGFont.get(), textSize, nullptr, desc.get()));
        if (variations) {
#if defined(MOZ_SKIA) && defined(XP_MACOSX)
            if (nsCocoaFeatures::OnVenturaOrLater()) {
                // On recent macOS versions, CTFontCreateWithGraphicsFont fails to apply
                // the variations from the descriptor, so to get the correct values we use
                // CTFontCreateCopyWithAttributes to re-apply them to the new instance.
                // (We don't do this on older versions to minimize the risk of regressing
                // something that has been working OK in the past.)
                SkUniqueCFRef<CFDictionaryRef> attrs(CFDictionaryCreate(
                    nullptr, (const void**)&kCTFontVariationAttribute,
                    (const void**)&variations, 1, &kCFTypeDictionaryKeyCallBacks,
                    &kCFTypeDictionaryValueCallBacks));
                // Get the original descriptor from the CTFont, then add the variations
                // attribute to it.
                SkUniqueCFRef<CTFontDescriptorRef> desc(CTFontCopyFontDescriptor(ctFont.get()));
                desc.reset(CTFontDescriptorCreateCopyWithAttributes(desc.get(), attrs.get()));
                // Return a copy of the font that has the variations added.
                ctFont.reset(CTFontCreateCopyWithAttributes(ctFont.get(), 0.0, nullptr, desc.get()));
            }
#endif
            CFRelease(variations);
        }
        return ctFont;
    }

    return SkUniqueCFRef<CTFontRef>(
            CTFontCreateCopyWithAttributes(baseFont, textSize, nullptr, desc.get()));
}

#endif

