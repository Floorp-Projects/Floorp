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

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif
#include "prlog.h"

#include <Carbon/Carbon.h>
#include <algorithm>

#import <AppKit/AppKit.h>

#include "gfxPlatformMac.h"
#include "gfxMacPlatformFontList.h"
#include "gfxMacFont.h"
#include "gfxUserFontSet.h"
#include "harfbuzz/hb.h"
#include "harfbuzz/hb-ot.h"

#include "nsServiceManagerUtils.h"
#include "nsTArray.h"

#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsISimpleEnumerator.h"
#include "nsCharTraits.h"
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

#ifdef PR_LOGGING

#define LOG_FONTLIST(args) PR_LOG(gfxPlatform::GetLog(eGfxLog_fontlist), \
                               PR_LOG_DEBUG, args)
#define LOG_FONTLIST_ENABLED() PR_LOG_TEST( \
                                   gfxPlatform::GetLog(eGfxLog_fontlist), \
                                   PR_LOG_DEBUG)
#define LOG_CMAPDATA_ENABLED() PR_LOG_TEST( \
                                   gfxPlatform::GetLog(eGfxLog_cmapdata), \
                                   PR_LOG_DEBUG)

#endif // PR_LOGGING

#pragma mark-

// Complex scripts will not render correctly unless appropriate AAT or OT
// layout tables are present.
// For OpenType, we also check that the GSUB table supports the relevant
// script tag, to avoid using things like Arial Unicode MS for Lao (it has
// the characters, but lacks OpenType support).

// TODO: consider whether we should move this to gfxFontEntry and do similar
// cmap-masking on other platforms to avoid using fonts that won't shape
// properly.

struct ScriptRange {
    uint32_t         rangeStart;
    uint32_t         rangeEnd;
    hb_tag_t         tags[3]; // one or two OpenType script tags to check,
                              // plus a NULL terminator
};

static const ScriptRange sComplexScripts[] = {
    // Actually, now that harfbuzz supports presentation-forms shaping for
    // Arabic, we can render it without layout tables. So maybe we don't
    // want to mask the basic Arabic block here?
    // This affects the arabic-fallback-*.html reftests, which rely on
    // loading a font that *doesn't* have any GSUB table.
    { 0x0600, 0x06FF, { TRUETYPE_TAG('a','r','a','b'), 0, 0 } },
    { 0x0700, 0x074F, { TRUETYPE_TAG('s','y','r','c'), 0, 0 } },
    { 0x0750, 0x077F, { TRUETYPE_TAG('a','r','a','b'), 0, 0 } },
    { 0x08A0, 0x08FF, { TRUETYPE_TAG('a','r','a','b'), 0, 0 } },
    { 0x0900, 0x097F, { TRUETYPE_TAG('d','e','v','2'),
                        TRUETYPE_TAG('d','e','v','a'), 0 } },
    { 0x0980, 0x09FF, { TRUETYPE_TAG('b','n','g','2'),
                        TRUETYPE_TAG('b','e','n','g'), 0 } },
    { 0x0A00, 0x0A7F, { TRUETYPE_TAG('g','u','r','2'),
                        TRUETYPE_TAG('g','u','r','u'), 0 } },
    { 0x0A80, 0x0AFF, { TRUETYPE_TAG('g','j','r','2'),
                        TRUETYPE_TAG('g','u','j','r'), 0 } },
    { 0x0B00, 0x0B7F, { TRUETYPE_TAG('o','r','y','2'),
                        TRUETYPE_TAG('o','r','y','a'), 0 } },
    { 0x0B80, 0x0BFF, { TRUETYPE_TAG('t','m','l','2'),
                        TRUETYPE_TAG('t','a','m','l'), 0 } },
    { 0x0C00, 0x0C7F, { TRUETYPE_TAG('t','e','l','2'),
                        TRUETYPE_TAG('t','e','l','u'), 0 } },
    { 0x0C80, 0x0CFF, { TRUETYPE_TAG('k','n','d','2'),
                        TRUETYPE_TAG('k','n','d','a'), 0 } },
    { 0x0D00, 0x0D7F, { TRUETYPE_TAG('m','l','m','2'),
                        TRUETYPE_TAG('m','l','y','m'), 0 } },
    { 0x0D80, 0x0DFF, { TRUETYPE_TAG('s','i','n','h'), 0, 0 } },
    { 0x0E80, 0x0EFF, { TRUETYPE_TAG('l','a','o',' '), 0, 0 } },
    { 0x0F00, 0x0FFF, { TRUETYPE_TAG('t','i','b','t'), 0, 0 } },
    { 0x1000, 0x109f, { TRUETYPE_TAG('m','y','m','r'),
                        TRUETYPE_TAG('m','y','m','2'), 0 } },
    { 0x1780, 0x17ff, { TRUETYPE_TAG('k','h','m','r'), 0, 0 } },
    // Khmer Symbols (19e0..19ff) don't seem to need any special shaping
    { 0xaa60, 0xaa7f, { TRUETYPE_TAG('m','y','m','r'),
                        TRUETYPE_TAG('m','y','m','2'), 0 } },
    // Thai seems to be "renderable" without AAT morphing tables
};

static bool
SupportsScriptInGSUB(gfxFontEntry* aFontEntry, const hb_tag_t* aScriptTags)
{
    hb_face_t *face = aFontEntry->GetHBFace();
    if (!face) {
        return false;
    }

    unsigned int index;
    hb_tag_t     chosenScript;
    bool found =
        hb_ot_layout_table_choose_script(face, TRUETYPE_TAG('G','S','U','B'),
                                         aScriptTags, &index, &chosenScript);
    hb_face_destroy(face);

    return found && chosenScript != TRUETYPE_TAG('D','F','L','T');
}

nsresult
MacOSFontEntry::ReadCMAP()
{
    // attempt this once, if errors occur leave a blank cmap
    if (mCharacterMap) {
        return NS_OK;
    }

    nsRefPtr<gfxCharacterMap> charmap = new gfxCharacterMap();

    uint32_t kCMAP = TRUETYPE_TAG('c','m','a','p');

    AutoTable cmapTable(this, kCMAP);
    nsresult rv;

    if (cmapTable) {
        bool unicodeFont = false, symbolFont = false; // currently ignored

        uint32_t cmapLen;
        const char* cmapData = hb_blob_get_data(cmapTable, &cmapLen);
        rv = gfxFontUtils::ReadCMAP((const uint8_t*)cmapData, cmapLen,
                                    *charmap, mUVSOffset,
                                    unicodeFont, symbolFont);
    } else {
        rv = NS_ERROR_NOT_AVAILABLE;
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

        uint32_t numScripts = ArrayLength(sComplexScripts);

        for (uint32_t s = 0; s < numScripts; s++) {
            // check to see if the cmap includes complex script codepoints
            const ScriptRange& sr = sComplexScripts[s];
            if (charmap->TestRange(sr.rangeStart, sr.rangeEnd)) {
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
                if (hasGSUB && SupportsScriptInGSUB(this, sr.tags)) {
                    continue;
                }

                charmap->ClearRange(sr.rangeStart, sr.rangeEnd);
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

#ifdef PR_LOGGING
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
#endif

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
                               bool aIsUserFont, bool aIsLocal)
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
    mIsUserFont = aIsUserFont;
    mIsLocalUserFont = aIsLocal;
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

/*static*/ void
MacOSFontEntry::DestroyBlobFunc(void* aUserData)
{
    ::CFRelease((CFDataRef)aUserData);
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
                              (void*)dataRef, DestroyBlobFunc);
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
    gfxMacFontFamily(nsAString& aName) :
        gfxFontFamily(aName)
    {}

    virtual ~gfxMacFontFamily() {}

    virtual void LocalizedName(nsAString& aLocalizedName);

    virtual void FindStyleVariations();
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

void
gfxMacFontFamily::FindStyleVariations()
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

        int32_t cssWeight = gfxMacPlatformFontList::AppleWeightToCSSWeight(appKitWeight) * 100;

        // make a nsString
        nsAutoString postscriptFontName;
        GetStringForNSString(psname, postscriptFontName);

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

#ifdef PR_LOGGING
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
#endif

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
    gfxSingleFaceMacFontFamily(nsAString& aName) :
        gfxFontFamily(aName)
    {}

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
    gfxPlatformFontList(false), mATSGeneration(uint32_t(kATSGenerationInitial)),
    mDefaultFont(nullptr)
{
    ::ATSFontNotificationSubscribe(ATSNotification,
                                   kATSFontNotifyOptionDefault,
                                   (void*)this, nullptr);

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

    ATSGeneration currentGeneration = ::ATSGetGeneration();

    // need to ignore notifications after adding each font
    if (mATSGeneration == currentGeneration)
        return NS_OK;

    Telemetry::AutoTimer<Telemetry::MAC_INITFONTLIST_TOTAL> timer;

    mATSGeneration = currentGeneration;
#ifdef PR_LOGGING
    LOG_FONTLIST(("(fontlist) updating to generation: %d", mATSGeneration));
#endif

    // reset font lists
    gfxPlatformFontList::InitFontList();
    
    // iterate over available families
    NSEnumerator *families = [[sFontManager availableFontFamilies]
                              objectEnumerator];  // returns "canonical", non-localized family name

    nsAutoString availableFamilyName;

    NSString *availableFamily = nil;
    while ((availableFamily = [families nextObject])) {

        // make a nsString
        GetStringForNSString(availableFamily, availableFamilyName);

        // create a family entry
        gfxFontFamily *familyEntry = new gfxMacFontFamily(availableFamilyName);
        if (!familyEntry) break;

        // add the family entry to the hash table
        ToLowerCase(availableFamilyName);
        mFontFamilies.Put(availableFamilyName, familyEntry);

        // check the bad underline blacklist
        if (mBadUnderlineFamilyNames.Contains(availableFamilyName))
            familyEntry->SetBadUnderlineFamily();
    }

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
#ifdef PR_LOGGING
        LOG_FONTLIST(("(fontlist-singleface) face name: %s\n",
                      NS_ConvertUTF16toUTF8(singleFaceFonts[i]).get()));
#endif
        gfxFontEntry *fontEntry = LookupLocalFont(nullptr, singleFaceFonts[i]);
        if (fontEntry) {
            nsAutoString familyName, key;
            familyName = singleFaceFonts[i];
            GenerateFontListKey(familyName, key);
#ifdef PR_LOGGING
            LOG_FONTLIST(("(fontlist-singleface) family name: %s, key: %s\n",
                          NS_ConvertUTF16toUTF8(familyName).get(),
                          NS_ConvertUTF16toUTF8(key).get()));
#endif

            // add only if doesn't exist already
            if (!mFontFamilies.GetWeak(key)) {
                gfxFontFamily *familyEntry =
                    new gfxSingleFaceMacFontFamily(familyName);
                familyEntry->AddFontEntry(fontEntry);
                familyEntry->SetHasStyles(true);
                mFontFamilies.Put(key, familyEntry);
#ifdef PR_LOGGING
                LOG_FONTLIST(("(fontlist-singleface) added new family\n",
                              NS_ConvertUTF16toUTF8(familyName).get(),
                              NS_ConvertUTF16toUTF8(key).get()));
#endif
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
gfxMacPlatformFontList::ATSNotification(ATSFontNotificationInfoRef aInfo,
                                        void* aUserArg)
{
    // xxx - should be carefully pruning the list of fonts, not rebuilding it from scratch
    static_cast<gfxMacPlatformFontList*>(aUserArg)->UpdateFontList();

    // modify a preference that will trigger reflow everywhere
    static const char kPrefName[] = "font.internaluseonly.changed";
    bool fontInternalChange = Preferences::GetBool(kPrefName, false);
    Preferences::SetBool(kPrefName, !fontInternalChange);
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
        mDefaultFont = ::CTFontCreateWithName(CFSTR("Lucida Grande"), 12.f,
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
            nsDependentString familyName(reinterpret_cast<PRUnichar*>(buffer.Elements()), len);

            bool needsBold;  // ignored in the system fallback case

            gfxFontFamily *family = FindFamily(familyName);
            if (family) {
                fontEntry = family->FindFontForStyle(*aMatchStyle, needsBold);
                if (fontEntry) {
                    if (fontEntry->TestCharacterMap(aCh)) {
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
gfxMacPlatformFontList::LookupLocalFont(const gfxProxyFontEntry *aProxyEntry,
                                        const nsAString& aFontName)
{
    nsAutoreleasePool localPool;

    NSString *faceName = GetNSStringForString(aFontName);
    MacOSFontEntry *newFontEntry;

    // lookup face based on postscript or full name
    CGFontRef fontRef = ::CGFontCreateWithFontName(CFStringRef(faceName));
    if (!fontRef) {
        return nullptr;
    }

    if (aProxyEntry) {
        uint16_t w = aProxyEntry->mWeight;
        NS_ASSERTION(w >= 100 && w <= 900, "bogus font weight value!");

        newFontEntry =
            new MacOSFontEntry(aFontName, fontRef,
                               w, aProxyEntry->mStretch,
                               aProxyEntry->mItalic ?
                                   NS_FONT_STYLE_ITALIC : NS_FONT_STYLE_NORMAL,
                               true, true);
    } else {
        newFontEntry =
            new MacOSFontEntry(aFontName, fontRef,
                               400, 0, NS_FONT_STYLE_NORMAL,
                               false, false);
    }
    ::CFRelease(fontRef);

    return newFontEntry;
}

static void ReleaseData(void *info, const void *data, size_t size)
{
    NS_Free((void*)data);
}

gfxFontEntry*
gfxMacPlatformFontList::MakePlatformFont(const gfxProxyFontEntry *aProxyEntry,
                                         const uint8_t *aFontData,
                                         uint32_t aLength)
{
    NS_ASSERTION(aFontData, "MakePlatformFont called with null data");

    uint16_t w = aProxyEntry->mWeight;
    NS_ASSERTION(w >= 100 && w <= 900, "bogus font weight value!");

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
        newFontEntry(new MacOSFontEntry(uniqueName, fontRef, w,
                                        aProxyEntry->mStretch,
                                        aProxyEntry->mItalic ?
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
