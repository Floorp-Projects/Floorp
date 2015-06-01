/* -*- Mode: ObjC; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: BSD
 *
 * Copyright (C) 2006-2009 Mozilla Corporation.  All rights reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   John Daggett <jdaggett@mozilla.com>
 *   Jonathan Kew <jfkthame@gmail.com>
 *
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozilla/Logging.h"

#include <algorithm>

#import <AppKit/AppKit.h>

#include "gfxPlatformMac.h"
#include "gfxMacPlatformFontList.h"
#include "gfxMacFont.h"
#include "gfxUserFontSet.h"
#include "harfbuzz/hb.h"

#include "nsServiceManagerUtils.h"
#include "nsTArray.h"

#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsISimpleEnumerator.h"
#include "nsCharTraits.h"
#include "nsCocoaFeatures.h"
#include "gfxFontConstants.h"

#include "mozilla/MemoryReporting.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"
#include "mozilla/gfx/2D.h"

#include <unistd.h>
#include <time.h>

using namespace mozilla;

class nsAutoreleasePool {
public:
    nsAutoreleasePool()
    {
        mLocalPool = [[NSAutoreleasePool alloc] init];
    }
    ~nsAutoreleasePool()
    {
        [mLocalPool release];
    }
private:
    NSAutoreleasePool *mLocalPool;
};

// indexes into the NSArray objects that the Cocoa font manager returns
// as the available members of a family
#define INDEX_FONT_POSTSCRIPT_NAME 0
#define INDEX_FONT_FACE_NAME 1
#define INDEX_FONT_WEIGHT 2
#define INDEX_FONT_TRAITS 3

static const int kAppleMaxWeight = 14;
static const int kAppleExtraLightWeight = 3;
static const int kAppleUltraLightWeight = 2;

static const int gAppleWeightToCSSWeight[] = {
    0,
    1, // 1.
    1, // 2.  W1, ultralight
    2, // 3.  W2, extralight
    3, // 4.  W3, light
    4, // 5.  W4, semilight
    5, // 6.  W5, medium
    6, // 7.
    6, // 8.  W6, semibold
    7, // 9.  W7, bold
    8, // 10. W8, extrabold
    8, // 11.
    9, // 12. W9, ultrabold
    9, // 13
    9  // 14
};

// cache Cocoa's "shared font manager" for performance
static NSFontManager *sFontManager;

static void GetStringForNSString(const NSString *aSrc, nsAString& aDist)
{
    aDist.SetLength([aSrc length]);
    [aSrc getCharacters:reinterpret_cast<unichar*>(aDist.BeginWriting())];
}

static NSString* GetNSStringForString(const nsAString& aSrc)
{
    return [NSString stringWithCharacters:reinterpret_cast<const unichar*>(aSrc.BeginReading())
                                   length:aSrc.Length()];
}

#define LOG_FONTLIST(args) MOZ_LOG(gfxPlatform::GetLog(eGfxLog_fontlist), \
                               mozilla::LogLevel::Debug, args)
#define LOG_FONTLIST_ENABLED() MOZ_LOG_TEST( \
                                   gfxPlatform::GetLog(eGfxLog_fontlist), \
                                   mozilla::LogLevel::Debug)
#define LOG_CMAPDATA_ENABLED() MOZ_LOG_TEST( \
                                   gfxPlatform::GetLog(eGfxLog_cmapdata), \
                                   mozilla::LogLevel::Debug)

#pragma mark-

// Complex scripts will not render correctly unless appropriate AAT or OT
// layout tables are present.
// For OpenType, we also check that the GSUB table supports the relevant
// script tag, to avoid using things like Arial Unicode MS for Lao (it has
// the characters, but lacks OpenType support).

// TODO: consider whether we should move this to gfxFontEntry and do similar
// cmap-masking on other platforms to avoid using fonts that won't shape
// properly.

nsresult
MacOSFontEntry::ReadCMAP(FontInfoData *aFontInfoData)
{
    // attempt this once, if errors occur leave a blank cmap
    if (mCharacterMap) {
        return NS_OK;
    }

    nsRefPtr<gfxCharacterMap> charmap;
    nsresult rv;
    bool symbolFont;

    if (aFontInfoData && (charmap = GetCMAPFromFontInfo(aFontInfoData,
                                                        mUVSOffset,
                                                        symbolFont))) {
        rv = NS_OK;
    } else {
        uint32_t kCMAP = TRUETYPE_TAG('c','m','a','p');
        charmap = new gfxCharacterMap();
        AutoTable cmapTable(this, kCMAP);

        if (cmapTable) {
            bool unicodeFont = false, symbolFont = false; // currently ignored
            uint32_t cmapLen;
            const uint8_t* cmapData =
                reinterpret_cast<const uint8_t*>(hb_blob_get_data(cmapTable,
                                                                  &cmapLen));
            rv = gfxFontUtils::ReadCMAP(cmapData, cmapLen,
                                        *charmap, mUVSOffset,
                                        unicodeFont, symbolFont);
        } else {
            rv = NS_ERROR_NOT_AVAILABLE;
        }
    }

    if (NS_SUCCEEDED(rv) && !HasGraphiteTables()) {
        // We assume a Graphite font knows what it's doing,
        // and provides whatever shaping is needed for the
        // characters it supports, so only check/clear the
        // complex-script ranges for non-Graphite fonts

        // for layout support, check for the presence of mort/morx and/or
        // opentype layout tables
        bool hasAATLayout = HasFontTable(TRUETYPE_TAG('m','o','r','x')) ||
                            HasFontTable(TRUETYPE_TAG('m','o','r','t'));
        bool hasGSUB = HasFontTable(TRUETYPE_TAG('G','S','U','B'));
        bool hasGPOS = HasFontTable(TRUETYPE_TAG('G','P','O','S'));
        if (hasAATLayout && !(hasGSUB || hasGPOS)) {
            mRequiresAAT = true; // prefer CoreText if font has no OTL tables
        }

        for (const ScriptRange* sr = gfxPlatformFontList::sComplexScriptRanges;
             sr->rangeStart; sr++) {
            // check to see if the cmap includes complex script codepoints
            if (charmap->TestRange(sr->rangeStart, sr->rangeEnd)) {
                if (hasAATLayout) {
                    // prefer CoreText for Apple's complex-script fonts,
                    // even if they also have some OpenType tables
                    // (e.g. Geeza Pro Bold on 10.6; see bug 614903)
                    mRequiresAAT = true;
                    // and don't mask off complex-script ranges, we assume
                    // the AAT tables will provide the necessary shaping
                    continue;
                }

                // We check for GSUB here, as GPOS alone would not be ok.
                if (hasGSUB && SupportsScriptInGSUB(sr->tags)) {
                    continue;
                }

                charmap->ClearRange(sr->rangeStart, sr->rangeEnd);
            }
        }
    }

    mHasCmapTable = NS_SUCCEEDED(rv);
    if (mHasCmapTable) {
        gfxPlatformFontList *pfl = gfxPlatformFontList::PlatformFontList();
        mCharacterMap = pfl->FindCharMap(charmap);
    } else {
        // if error occurred, initialize to null cmap
        mCharacterMap = new gfxCharacterMap();
    }

    LOG_FONTLIST(("(fontlist-cmap) name: %s, size: %d hash: %8.8x%s\n",
                  NS_ConvertUTF16toUTF8(mName).get(),
                  charmap->SizeOfIncludingThis(moz_malloc_size_of),
                  charmap->mHash, mCharacterMap == charmap ? " new" : ""));
    if (LOG_CMAPDATA_ENABLED()) {
        char prefix[256];
        sprintf(prefix, "(cmapdata) name: %.220s",
                NS_ConvertUTF16toUTF8(mName).get());
        charmap->Dump(prefix, eGfxLog_cmapdata);
    }

    return rv;
}

gfxFont*
MacOSFontEntry::CreateFontInstance(const gfxFontStyle *aFontStyle, bool aNeedsBold)
{
    return new gfxMacFont(this, aFontStyle, aNeedsBold);
}

bool
MacOSFontEntry::IsCFF()
{
    if (!mIsCFFInitialized) {
        mIsCFFInitialized = true;
        mIsCFF = HasFontTable(TRUETYPE_TAG('C','F','F',' '));
    }

    return mIsCFF;
}

MacOSFontEntry::MacOSFontEntry(const nsAString& aPostscriptName,
                               int32_t aWeight,
                               bool aIsStandardFace)
    : gfxFontEntry(aPostscriptName, aIsStandardFace),
      mFontRef(NULL),
      mFontRefInitialized(false),
      mRequiresAAT(false),
      mIsCFF(false),
      mIsCFFInitialized(false)
{
    mWeight = aWeight;
}

MacOSFontEntry::MacOSFontEntry(const nsAString& aPostscriptName,
                               CGFontRef aFontRef,
                               uint16_t aWeight, uint16_t aStretch,
                               uint32_t aItalicStyle,
                               bool aIsDataUserFont,
                               bool aIsLocalUserFont)
    : gfxFontEntry(aPostscriptName, false),
      mFontRef(NULL),
      mFontRefInitialized(false),
      mRequiresAAT(false),
      mIsCFF(false),
      mIsCFFInitialized(false)
{
    mFontRef = aFontRef;
    mFontRefInitialized = true;
    ::CFRetain(mFontRef);

    mWeight = aWeight;
    mStretch = aStretch;
    mFixedPitch = false; // xxx - do we need this for downloaded fonts?
    mItalic = (aItalicStyle & (NS_FONT_STYLE_ITALIC | NS_FONT_STYLE_OBLIQUE)) != 0;

    NS_ASSERTION(!(aIsDataUserFont && aIsLocalUserFont),
                 "userfont is either a data font or a local font");
    mIsDataUserFont = aIsDataUserFont;
    mIsLocalUserFont = aIsLocalUserFont;
}

CGFontRef
MacOSFontEntry::GetFontRef()
{
    if (!mFontRefInitialized) {
        mFontRefInitialized = true;
        NSString *psname = GetNSStringForString(mName);
        mFontRef = ::CGFontCreateWithFontName(CFStringRef(psname));
    }
    return mFontRef;
}

// For a logging build, we wrap the CFDataRef in a FontTableRec so that we can
// use the MOZ_COUNT_[CD]TOR macros in it. A release build without logging
// does not get this overhead.
class FontTableRec {
public:
    explicit FontTableRec(CFDataRef aDataRef)
        : mDataRef(aDataRef)
    {
        MOZ_COUNT_CTOR(FontTableRec);
    }

    ~FontTableRec() {
        MOZ_COUNT_DTOR(FontTableRec);
        ::CFRelease(mDataRef);
    }

private:
    CFDataRef mDataRef;
};

/*static*/ void
MacOSFontEntry::DestroyBlobFunc(void* aUserData)
{
#ifdef NS_BUILD_REFCNT_LOGGING
    FontTableRec *ftr = static_cast<FontTableRec*>(aUserData);
    delete ftr;
#else
    ::CFRelease((CFDataRef)aUserData);
#endif
}

hb_blob_t *
MacOSFontEntry::GetFontTable(uint32_t aTag)
{
    CGFontRef fontRef = GetFontRef();
    if (!fontRef) {
        return nullptr;
    }

    CFDataRef dataRef = ::CGFontCopyTableForTag(fontRef, aTag);
    if (dataRef) {
        return hb_blob_create((const char*)::CFDataGetBytePtr(dataRef),
                              ::CFDataGetLength(dataRef),
                              HB_MEMORY_MODE_READONLY,
#ifdef NS_BUILD_REFCNT_LOGGING
                              new FontTableRec(dataRef),
#else
                              (void*)dataRef,
#endif
                              DestroyBlobFunc);
    }

    return nullptr;
}

bool
MacOSFontEntry::HasFontTable(uint32_t aTableTag)
{
    nsAutoreleasePool localPool;

    CGFontRef fontRef = GetFontRef();
    if (!fontRef) {
        return false;
    }

    CFDataRef tableData = ::CGFontCopyTableForTag(fontRef, aTableTag);
    if (!tableData) {
        return false;
    }

    ::CFRelease(tableData);
    return true;
}

void
MacOSFontEntry::AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf,
                                       FontListSizes* aSizes) const
{
    aSizes->mFontListSize += aMallocSizeOf(this);
    AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
}

/* gfxMacFontFamily */
#pragma mark-

class gfxMacFontFamily : public gfxFontFamily
{
public:
    explicit gfxMacFontFamily(nsAString& aName) :
        gfxFontFamily(aName)
    {}

    virtual ~gfxMacFontFamily() {}

    virtual void LocalizedName(nsAString& aLocalizedName);

    virtual void FindStyleVariations(FontInfoData *aFontInfoData = nullptr);
};

void
gfxMacFontFamily::LocalizedName(nsAString& aLocalizedName)
{
    nsAutoreleasePool localPool;

    if (!HasOtherFamilyNames()) {
        aLocalizedName = mName;
        return;
    }

    NSString *family = GetNSStringForString(mName);
    NSString *localized = [sFontManager
                           localizedNameForFamily:family
                                             face:nil];

    if (localized) {
        GetStringForNSString(localized, aLocalizedName);
        return;
    }

    // failed to get localized name, just use the canonical one
    aLocalizedName = mName;
}

// Return the CSS weight value to use for the given face, overriding what
// AppKit gives us (used to adjust families with bad weight values, see
// bug 931426).
// A return value of 0 indicates no override - use the existing weight.
static inline int
GetWeightOverride(const nsAString& aPSName)
{
    nsAutoCString prefName("font.weight-override.");
    // The PostScript name is required to be ASCII; if it's not, the font is
    // broken anyway, so we really don't care that this is lossy.
    LossyAppendUTF16toASCII(aPSName, prefName);
    return Preferences::GetInt(prefName.get(), 0);
}

void
gfxMacFontFamily::FindStyleVariations(FontInfoData *aFontInfoData)
{
    if (mHasStyles)
        return;

    nsAutoreleasePool localPool;

    NSString *family = GetNSStringForString(mName);

    // create a font entry for each face
    NSArray *fontfaces = [sFontManager
                          availableMembersOfFontFamily:family];  // returns an array of [psname, style name, weight, traits] elements, goofy api
    int faceCount = [fontfaces count];
    int faceIndex;

    for (faceIndex = 0; faceIndex < faceCount; faceIndex++) {
        NSArray *face = [fontfaces objectAtIndex:faceIndex];
        NSString *psname = [face objectAtIndex:INDEX_FONT_POSTSCRIPT_NAME];
        int32_t appKitWeight = [[face objectAtIndex:INDEX_FONT_WEIGHT] unsignedIntValue];
        uint32_t macTraits = [[face objectAtIndex:INDEX_FONT_TRAITS] unsignedIntValue];
        NSString *facename = [face objectAtIndex:INDEX_FONT_FACE_NAME];
        bool isStandardFace = false;

        if (appKitWeight == kAppleExtraLightWeight) {
            // if the facename contains UltraLight, set the weight to the ultralight weight value
            NSRange range = [facename rangeOfString:@"ultralight" options:NSCaseInsensitiveSearch];
            if (range.location != NSNotFound) {
                appKitWeight = kAppleUltraLightWeight;
            }
        }

        // make a nsString
        nsAutoString postscriptFontName;
        GetStringForNSString(psname, postscriptFontName);

        int32_t cssWeight = GetWeightOverride(postscriptFontName);
        if (cssWeight) {
            // scale down and clamp, to get a value from 1..9
            cssWeight = ((cssWeight + 50) / 100);
            cssWeight = std::max(1, std::min(cssWeight, 9));
        } else {
            cssWeight =
                gfxMacPlatformFontList::AppleWeightToCSSWeight(appKitWeight);
        }
        cssWeight *= 100; // scale up to CSS values

        if ([facename isEqualToString:@"Regular"] ||
            [facename isEqualToString:@"Bold"] ||
            [facename isEqualToString:@"Italic"] ||
            [facename isEqualToString:@"Oblique"] ||
            [facename isEqualToString:@"Bold Italic"] ||
            [facename isEqualToString:@"Bold Oblique"])
        {
            isStandardFace = true;
        }

        // create a font entry
        MacOSFontEntry *fontEntry =
            new MacOSFontEntry(postscriptFontName, cssWeight, isStandardFace);
        if (!fontEntry) {
            break;
        }

        // set additional properties based on the traits reported by Cocoa
        if (macTraits & (NSCondensedFontMask | NSNarrowFontMask | NSCompressedFontMask)) {
            fontEntry->mStretch = NS_FONT_STRETCH_CONDENSED;
        } else if (macTraits & NSExpandedFontMask) {
            fontEntry->mStretch = NS_FONT_STRETCH_EXPANDED;
        }
        // Cocoa fails to set the Italic traits bit for HelveticaLightItalic,
        // at least (see bug 611855), so check for style name endings as well
        if ((macTraits & NSItalicFontMask) ||
            [facename hasSuffix:@"Italic"] ||
            [facename hasSuffix:@"Oblique"])
        {
            fontEntry->mItalic = true;
        }
        if (macTraits & NSFixedPitchFontMask) {
            fontEntry->mFixedPitch = true;
        }

        if (LOG_FONTLIST_ENABLED()) {
            LOG_FONTLIST(("(fontlist) added (%s) to family (%s)"
                 " with style: %s weight: %d stretch: %d"
                 " (apple-weight: %d macTraits: %8.8x)",
                 NS_ConvertUTF16toUTF8(fontEntry->Name()).get(), 
                 NS_ConvertUTF16toUTF8(Name()).get(), 
                 fontEntry->IsItalic() ? "italic" : "normal",
                 cssWeight, fontEntry->Stretch(),
                 appKitWeight, macTraits));
        }

        // insert into font entry array of family
        AddFontEntry(fontEntry);
    }

    SortAvailableFonts();
    SetHasStyles(true);

    if (mIsBadUnderlineFamily) {
        SetBadUnderlineFonts();
    }
}


/* gfxSingleFaceMacFontFamily */
#pragma mark-

class gfxSingleFaceMacFontFamily : public gfxFontFamily
{
public:
    explicit gfxSingleFaceMacFontFamily(nsAString& aName) :
        gfxFontFamily(aName)
    {
        mFaceNamesInitialized = true; // omit from face name lists
    }

    virtual ~gfxSingleFaceMacFontFamily() {}

    virtual void LocalizedName(nsAString& aLocalizedName);

    virtual void ReadOtherFamilyNames(gfxPlatformFontList *aPlatformFontList);
};

void
gfxSingleFaceMacFontFamily::LocalizedName(nsAString& aLocalizedName)
{
    nsAutoreleasePool localPool;

    if (!HasOtherFamilyNames()) {
        aLocalizedName = mName;
        return;
    }

    gfxFontEntry *fe = mAvailableFonts[0];
    NSFont *font = [NSFont fontWithName:GetNSStringForString(fe->Name())
                                   size:0.0];
    if (font) {
        NSString *localized = [font displayName];
        if (localized) {
            GetStringForNSString(localized, aLocalizedName);
            return;
        }
    }

    // failed to get localized name, just use the canonical one
    aLocalizedName = mName;
}

void
gfxSingleFaceMacFontFamily::ReadOtherFamilyNames(gfxPlatformFontList *aPlatformFontList)
{
    if (mOtherFamilyNamesInitialized) {
        return;
    }

    gfxFontEntry *fe = mAvailableFonts[0];
    if (!fe) {
        return;
    }

    const uint32_t kNAME = TRUETYPE_TAG('n','a','m','e');

    gfxFontEntry::AutoTable nameTable(fe, kNAME);
    if (!nameTable) {
        return;
    }

    mHasOtherFamilyNames = ReadOtherFamilyNamesForFace(aPlatformFontList,
                                                       nameTable,
                                                       true);

    mOtherFamilyNamesInitialized = true;
}


/* gfxMacPlatformFontList */
#pragma mark-

gfxMacPlatformFontList::gfxMacPlatformFontList() :
    gfxPlatformFontList(false),
    mDefaultFont(nullptr)
{
#ifdef MOZ_BUNDLED_FONTS
    ActivateBundledFonts();
#endif

    ::CFNotificationCenterAddObserver(::CFNotificationCenterGetLocalCenter(),
                                      this,
                                      RegisteredFontsChangedNotificationCallback,
                                      kCTFontManagerRegisteredFontsChangedNotification,
                                      0,
                                      CFNotificationSuspensionBehaviorDeliverImmediately);

    // cache this in a static variable so that MacOSFontFamily objects
    // don't have to repeatedly look it up
    sFontManager = [NSFontManager sharedFontManager];
}

gfxMacPlatformFontList::~gfxMacPlatformFontList()
{
    if (mDefaultFont) {
        ::CFRelease(mDefaultFont);
    }
}

nsresult
gfxMacPlatformFontList::InitFontList()
{
    nsAutoreleasePool localPool;

    Telemetry::AutoTimer<Telemetry::MAC_INITFONTLIST_TOTAL> timer;

    // reset font lists
    gfxPlatformFontList::InitFontList();
    mSystemFontFamilies.Clear();
    
    // iterate over available families

    CFArrayRef familyNames = CTFontManagerCopyAvailableFontFamilyNames();

    // iterate over families
    uint32_t i, numFamilies;

    numFamilies = CFArrayGetCount(familyNames);
    for (i = 0; i < numFamilies; i++) {
        CFStringRef family = (CFStringRef)CFArrayGetValueAtIndex(familyNames, i);

        // CTFontManager includes weird internal family names and
        // LastResort, skip over those
        if (!family ||
            CFStringCompare(family, CFSTR("LastResort"),
                            kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
            continue;
        }

        bool hiddenSystemFont = false;
        if (::CFStringHasPrefix(family, CFSTR("."))) {
            hiddenSystemFont = true;
        }

        nsAutoTArray<UniChar, 1024> buffer;
        CFIndex len = ::CFStringGetLength(family);
        buffer.SetLength(len+1);
        ::CFStringGetCharacters(family, ::CFRangeMake(0, len),
                                buffer.Elements());
        buffer[len] = 0;
        nsAutoString familyName(reinterpret_cast<char16_t*>(buffer.Elements()), len);

        // create a family entry
        gfxFontFamily *familyEntry = new gfxMacFontFamily(familyName);
        if (!familyEntry) break;

        // add the family entry to the hash table
        ToLowerCase(familyName);
        if (!hiddenSystemFont) {
            mFontFamilies.Put(familyName, familyEntry);
        } else {
            mSystemFontFamilies.Put(familyName, familyEntry);
        }

        // check the bad underline blacklist
        if (mBadUnderlineFamilyNames.Contains(familyName))
            familyEntry->SetBadUnderlineFamily();
    }

    CFRelease(familyNames);

    InitSingleFaceList();

    // to avoid full search of font name tables, seed the other names table with localized names from
    // some of the prefs fonts which are accessed via their localized names.  changes in the pref fonts will only cause
    // a font lookup miss earlier. this is a simple optimization, it's not required for correctness
    PreloadNamesList();

    // start the delayed cmap loader
    GetPrefsAndStartLoader();

    return NS_OK;
}

void
gfxMacPlatformFontList::InitSingleFaceList()
{
    nsAutoTArray<nsString, 10> singleFaceFonts;
    gfxFontUtils::GetPrefsFontList("font.single-face-list", singleFaceFonts);

    uint32_t numFonts = singleFaceFonts.Length();
    for (uint32_t i = 0; i < numFonts; i++) {
        LOG_FONTLIST(("(fontlist-singleface) face name: %s\n",
                      NS_ConvertUTF16toUTF8(singleFaceFonts[i]).get()));
        gfxFontEntry *fontEntry = LookupLocalFont(singleFaceFonts[i],
                                                  400, 0,
                                                  NS_FONT_STYLE_NORMAL);
        if (fontEntry) {
            nsAutoString familyName, key;
            familyName = singleFaceFonts[i];
            GenerateFontListKey(familyName, key);
            LOG_FONTLIST(("(fontlist-singleface) family name: %s, key: %s\n",
                          NS_ConvertUTF16toUTF8(familyName).get(),
                          NS_ConvertUTF16toUTF8(key).get()));

            // add only if doesn't exist already
            if (!mFontFamilies.GetWeak(key)) {
                gfxFontFamily *familyEntry =
                    new gfxSingleFaceMacFontFamily(familyName);
                // LookupLocalFont sets this, need to clear
                fontEntry->mIsLocalUserFont = false;
                familyEntry->AddFontEntry(fontEntry);
                familyEntry->SetHasStyles(true);
                mFontFamilies.Put(key, familyEntry);
                LOG_FONTLIST(("(fontlist-singleface) added new family\n",
                              NS_ConvertUTF16toUTF8(familyName).get(),
                              NS_ConvertUTF16toUTF8(key).get()));
            }
        }
    }
}

bool
gfxMacPlatformFontList::GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName)
{
    gfxFontFamily *family = FindFamily(aFontName);
    if (family) {
        family->LocalizedName(aFamilyName);
        return true;
    }

    return false;
}

void
gfxMacPlatformFontList::RegisteredFontsChangedNotificationCallback(CFNotificationCenterRef center,
                                                                   void *observer,
                                                                   CFStringRef name,
                                                                   const void *object,
                                                                   CFDictionaryRef userInfo)
{
    if (!::CFEqual(name, kCTFontManagerRegisteredFontsChangedNotification)) {
        return;
    }

    gfxMacPlatformFontList* fl = static_cast<gfxMacPlatformFontList*>(observer);

    // xxx - should be carefully pruning the list of fonts, not rebuilding it from scratch
    fl->UpdateFontList();

    // modify a preference that will trigger reflow everywhere
    fl->ForceGlobalReflow();
}

gfxFontEntry*
gfxMacPlatformFontList::GlobalFontFallback(const uint32_t aCh,
                                           int32_t aRunScript,
                                           const gfxFontStyle* aMatchStyle,
                                           uint32_t& aCmapCount,
                                           gfxFontFamily** aMatchedFamily)
{
    bool useCmaps = gfxPlatform::GetPlatform()->UseCmapsDuringSystemFallback();

    if (useCmaps) {
        return gfxPlatformFontList::GlobalFontFallback(aCh,
                                                       aRunScript,
                                                       aMatchStyle,
                                                       aCmapCount,
                                                       aMatchedFamily);
    }

    CFStringRef str;
    UniChar ch[2];
    CFIndex len = 1;

    if (IS_IN_BMP(aCh)) {
        ch[0] = aCh;
        str = ::CFStringCreateWithCharactersNoCopy(kCFAllocatorDefault, ch, 1,
                                                   kCFAllocatorNull);
    } else {
        ch[0] = H_SURROGATE(aCh);
        ch[1] = L_SURROGATE(aCh);
        str = ::CFStringCreateWithCharactersNoCopy(kCFAllocatorDefault, ch, 2,
                                                   kCFAllocatorNull);
        if (!str) {
            return nullptr;
        }
        len = 2;
    }

    // use CoreText to find the fallback family

    gfxFontEntry *fontEntry = nullptr;
    CTFontRef fallback;
    bool cantUseFallbackFont = false;

    if (!mDefaultFont) {
        mDefaultFont = ::CTFontCreateWithName(CFSTR("LucidaGrande"), 12.f,
                                              NULL);
    }

    fallback = ::CTFontCreateForString(mDefaultFont, str,
                                       ::CFRangeMake(0, len));

    if (fallback) {
        CFStringRef familyName = ::CTFontCopyFamilyName(fallback);
        ::CFRelease(fallback);

        if (familyName &&
            ::CFStringCompare(familyName, CFSTR("LastResort"),
                              kCFCompareCaseInsensitive) != kCFCompareEqualTo)
        {
            nsAutoTArray<UniChar, 1024> buffer;
            CFIndex len = ::CFStringGetLength(familyName);
            buffer.SetLength(len+1);
            ::CFStringGetCharacters(familyName, ::CFRangeMake(0, len),
                                    buffer.Elements());
            buffer[len] = 0;
            nsDependentString familyName(reinterpret_cast<char16_t*>(buffer.Elements()), len);

            bool needsBold;  // ignored in the system fallback case

            gfxFontFamily *family = FindFamily(familyName);
            if (family) {
                fontEntry = family->FindFontForStyle(*aMatchStyle, needsBold);
                if (fontEntry) {
                    if (fontEntry->HasCharacter(aCh)) {
                        *aMatchedFamily = family;
                    } else {
                        fontEntry = nullptr;
                        cantUseFallbackFont = true;
                    }
                }
            }
        }

        if (familyName) {
            ::CFRelease(familyName);
        }
    }

    if (cantUseFallbackFont) {
        Telemetry::Accumulate(Telemetry::BAD_FALLBACK_FONT, cantUseFallbackFont);
    }

    ::CFRelease(str);

    return fontEntry;
}

gfxFontFamily*
gfxMacPlatformFontList::GetDefaultFont(const gfxFontStyle* aStyle)
{
    nsAutoreleasePool localPool;

    NSString *defaultFamily = [[NSFont userFontOfSize:aStyle->size] familyName];
    nsAutoString familyName;

    GetStringForNSString(defaultFamily, familyName);
    return FindFamily(familyName);
}

int32_t
gfxMacPlatformFontList::AppleWeightToCSSWeight(int32_t aAppleWeight)
{
    if (aAppleWeight < 1)
        aAppleWeight = 1;
    else if (aAppleWeight > kAppleMaxWeight)
        aAppleWeight = kAppleMaxWeight;
    return gAppleWeightToCSSWeight[aAppleWeight];
}

gfxFontEntry*
gfxMacPlatformFontList::LookupLocalFont(const nsAString& aFontName,
                                        uint16_t aWeight,
                                        int16_t aStretch,
                                        bool aItalic)
{
    nsAutoreleasePool localPool;

    NSString *faceName = GetNSStringForString(aFontName);
    MacOSFontEntry *newFontEntry;

    // lookup face based on postscript or full name
    CGFontRef fontRef = ::CGFontCreateWithFontName(CFStringRef(faceName));
    if (!fontRef) {
        return nullptr;
    }

    NS_ASSERTION(aWeight >= 100 && aWeight <= 900,
                 "bogus font weight value!");

    newFontEntry =
        new MacOSFontEntry(aFontName, fontRef,
                           aWeight, aStretch,
                           aItalic ?
                               NS_FONT_STYLE_ITALIC : NS_FONT_STYLE_NORMAL,
                           false, true);
    ::CFRelease(fontRef);

    return newFontEntry;
}

static void ReleaseData(void *info, const void *data, size_t size)
{
    free((void*)data);
}

gfxFontEntry*
gfxMacPlatformFontList::MakePlatformFont(const nsAString& aFontName,
                                         uint16_t aWeight,
                                         int16_t aStretch,
                                         bool aItalic,
                                         const uint8_t* aFontData,
                                         uint32_t aLength)
{
    NS_ASSERTION(aFontData, "MakePlatformFont called with null data");

    NS_ASSERTION(aWeight >= 100 && aWeight <= 900, "bogus font weight value!");

    // create the font entry
    nsAutoString uniqueName;

    nsresult rv = gfxFontUtils::MakeUniqueUserFontName(uniqueName);
    if (NS_FAILED(rv)) {
        return nullptr;
    }

    CGDataProviderRef provider =
        ::CGDataProviderCreateWithData(nullptr, aFontData, aLength,
                                       &ReleaseData);
    CGFontRef fontRef = ::CGFontCreateWithDataProvider(provider);
    ::CGDataProviderRelease(provider);

    if (!fontRef) {
        return nullptr;
    }

    nsAutoPtr<MacOSFontEntry>
        newFontEntry(new MacOSFontEntry(uniqueName, fontRef, aWeight,
                                        aStretch,
                                        aItalic ?
                                            NS_FONT_STYLE_ITALIC :
                                            NS_FONT_STYLE_NORMAL,
                                        true, false));
    ::CFRelease(fontRef);

    // if succeeded and font cmap is good, return the new font
    if (newFontEntry->mIsValid && NS_SUCCEEDED(newFontEntry->ReadCMAP())) {
        return newFontEntry.forget();
    }

    // if something is funky about this font, delete immediately

#if DEBUG
    NS_WARNING("downloaded font not loaded properly");
#endif

    return nullptr;
}

// used to load system-wide font info on off-main thread
class MacFontInfo : public FontInfoData {
public:
    MacFontInfo(bool aLoadOtherNames,
                bool aLoadFaceNames,
                bool aLoadCmaps) :
        FontInfoData(aLoadOtherNames, aLoadFaceNames, aLoadCmaps)
    {}

    virtual ~MacFontInfo() {}

    virtual void Load() {
        nsAutoreleasePool localPool;
        // bug 975460 - async font loader crashes sometimes under 10.6, disable
        if (nsCocoaFeatures::OnLionOrLater()) {
            FontInfoData::Load();
        }
    }

    // loads font data for all members of a given family
    virtual void LoadFontFamilyData(const nsAString& aFamilyName);
};

void
MacFontInfo::LoadFontFamilyData(const nsAString& aFamilyName)
{
    // family name ==> CTFontDescriptor
    NSString *famName = GetNSStringForString(aFamilyName);
    CFStringRef family = CFStringRef(famName);

    CFMutableDictionaryRef attr =
        CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                  &kCFTypeDictionaryValueCallBacks);
    CFDictionaryAddValue(attr, kCTFontFamilyNameAttribute, family);
    CTFontDescriptorRef fd = CTFontDescriptorCreateWithAttributes(attr);
    CFRelease(attr);
    CFArrayRef matchingFonts =
        CTFontDescriptorCreateMatchingFontDescriptors(fd, NULL);
    CFRelease(fd);
    if (!matchingFonts) {
        return;
    }

    nsTArray<nsString> otherFamilyNames;
    bool hasOtherFamilyNames = true;

    // iterate over faces in the family
    int f, numFaces = (int) CFArrayGetCount(matchingFonts);
    for (f = 0; f < numFaces; f++) {
        mLoadStats.fonts++;

        CTFontDescriptorRef faceDesc =
            (CTFontDescriptorRef)CFArrayGetValueAtIndex(matchingFonts, f);
        if (!faceDesc) {
            continue;
        }
        CTFontRef fontRef = CTFontCreateWithFontDescriptor(faceDesc,
                                                           0.0, nullptr);
        if (!fontRef) {
            NS_WARNING("failed to create a CTFontRef");
            continue;
        }

        if (mLoadCmaps) {
            // face name
            CFStringRef faceName = (CFStringRef)
                CTFontDescriptorCopyAttribute(faceDesc, kCTFontNameAttribute);

            nsAutoTArray<UniChar, 1024> buffer;
            CFIndex len = CFStringGetLength(faceName);
            buffer.SetLength(len+1);
            CFStringGetCharacters(faceName, ::CFRangeMake(0, len),
                                    buffer.Elements());
            buffer[len] = 0;
            nsAutoString fontName(reinterpret_cast<char16_t*>(buffer.Elements()),
                                  len);

            // load the cmap data
            FontFaceData fontData;
            CFDataRef cmapTable = CTFontCopyTable(fontRef, kCTFontTableCmap,
                                                  kCTFontTableOptionNoOptions);

            if (cmapTable) {
                bool unicodeFont = false, symbolFont = false; // ignored
                const uint8_t *cmapData =
                    (const uint8_t*)CFDataGetBytePtr(cmapTable);
                uint32_t cmapLen = CFDataGetLength(cmapTable);
                nsRefPtr<gfxCharacterMap> charmap = new gfxCharacterMap();
                uint32_t offset;
                nsresult rv;

                rv = gfxFontUtils::ReadCMAP(cmapData, cmapLen, *charmap, offset,
                                            unicodeFont, symbolFont);
                if (NS_SUCCEEDED(rv)) {
                    fontData.mCharacterMap = charmap;
                    fontData.mUVSOffset = offset;
                    fontData.mSymbolFont = symbolFont;
                    mLoadStats.cmaps++;
                }
                CFRelease(cmapTable);
            }

            mFontFaceData.Put(fontName, fontData);
            CFRelease(faceName);
        }

        if (mLoadOtherNames && hasOtherFamilyNames) {
            CFDataRef nameTable = CTFontCopyTable(fontRef, kCTFontTableName,
                                                  kCTFontTableOptionNoOptions);

            if (nameTable) {
                const char *nameData = (const char*)CFDataGetBytePtr(nameTable);
                uint32_t nameLen = CFDataGetLength(nameTable);
                gfxFontFamily::ReadOtherFamilyNamesForFace(aFamilyName,
                                                           nameData, nameLen,
                                                           otherFamilyNames,
                                                           false);
                hasOtherFamilyNames = otherFamilyNames.Length() != 0;
                CFRelease(nameTable);
            }
        }

        CFRelease(fontRef);
    }
    CFRelease(matchingFonts);

    // if found other names, insert them in the hash table
    if (otherFamilyNames.Length() != 0) {
        mOtherFamilyNames.Put(aFamilyName, otherFamilyNames);
        mLoadStats.othernames += otherFamilyNames.Length();
    }
}

already_AddRefed<FontInfoData>
gfxMacPlatformFontList::CreateFontInfoData()
{
    bool loadCmaps = !UsesSystemFallback() ||
        gfxPlatform::GetPlatform()->UseCmapsDuringSystemFallback();

    nsRefPtr<MacFontInfo> fi =
        new MacFontInfo(true, NeedFullnamePostscriptNames(), loadCmaps);
    return fi.forget();
}

#ifdef MOZ_BUNDLED_FONTS

void
gfxMacPlatformFontList::ActivateBundledFonts()
{
    nsCOMPtr<nsIFile> localDir;
    nsresult rv = NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(localDir));
    if (NS_FAILED(rv)) {
        return;
    }
    if (NS_FAILED(localDir->Append(NS_LITERAL_STRING("fonts")))) {
        return;
    }
    bool isDir;
    if (NS_FAILED(localDir->IsDirectory(&isDir)) || !isDir) {
        return;
    }

    nsCOMPtr<nsISimpleEnumerator> e;
    rv = localDir->GetDirectoryEntries(getter_AddRefs(e));
    if (NS_FAILED(rv)) {
        return;
    }

    bool hasMore;
    while (NS_SUCCEEDED(e->HasMoreElements(&hasMore)) && hasMore) {
        nsCOMPtr<nsISupports> entry;
        if (NS_FAILED(e->GetNext(getter_AddRefs(entry)))) {
            break;
        }
        nsCOMPtr<nsIFile> file = do_QueryInterface(entry);
        if (!file) {
            continue;
        }
        nsCString path;
        if (NS_FAILED(file->GetNativePath(path))) {
            continue;
        }
        CFURLRef fontURL =
            ::CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
                                                      (uint8_t*)path.get(),
                                                      path.Length(),
                                                      false);
        if (fontURL) {
            CFErrorRef error = nullptr;
            ::CTFontManagerRegisterFontsForURL(fontURL,
                                               kCTFontManagerScopeProcess,
                                               &error);
            ::CFRelease(fontURL);
        }
    }
}

#endif
