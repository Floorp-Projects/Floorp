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

#include <Carbon/Carbon.h>

#import <AppKit/AppKit.h>

#include "gfxPlatformMac.h"
#include "gfxMacPlatformFontList.h"
#include "gfxMacFont.h"
#include "gfxUserFontSet.h"

#include "nsServiceManagerUtils.h"
#include "nsTArray.h"

#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsISimpleEnumerator.h"

#include <unistd.h>
#include <time.h>

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

// font info loader constants
static const PRUint32 kDelayBeforeLoadingCmaps = 8 * 1000; // 8secs
static const PRUint32 kIntervalBetweenLoadingCmaps = 150; // 150ms
static const PRUint32 kNumFontsPerSlice = 10; // read in info 10 fonts at a time

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
    [aSrc getCharacters:aDist.BeginWriting()];
}

static NSString* GetNSStringForString(const nsAString& aSrc)
{
    return [NSString stringWithCharacters:aSrc.BeginReading()
                     length:aSrc.Length()];
}

#ifdef PR_LOGGING
static PRLogModuleInfo *gFontInfoLog = PR_NewLogModule("fontInfoLog");
#endif /* PR_LOGGING */

#define LOG(args) PR_LOG(gFontInfoLog, PR_LOG_DEBUG, args)
#define LOG_ENABLED() PR_LOG_TEST(gFontInfoLog, PR_LOG_DEBUG)

/* MacOSFontEntry */
#pragma mark-

MacOSFontEntry::MacOSFontEntry(const nsAString& aPostscriptName,
                               PRInt32 aWeight,
                               gfxFontFamily *aFamily,
                               PRBool aIsStandardFace)
    : gfxFontEntry(aPostscriptName, aFamily, aIsStandardFace),
      mATSFontRef(0),
      mATSFontRefInitialized(PR_FALSE),
      mRequiresAAT(PR_FALSE),
      mIsCFF(PR_FALSE),
      mIsCFFInitialized(PR_FALSE)
{
    mWeight = aWeight;
}

MacOSFontEntry::MacOSFontEntry(const nsAString& aPostscriptName, ATSFontRef aFontRef,
                               PRUint16 aWeight, PRUint16 aStretch, PRUint32 aItalicStyle,
                               gfxUserFontData *aUserFontData)
    : gfxFontEntry(aPostscriptName),
      mATSFontRef(aFontRef),
      mATSFontRefInitialized(PR_TRUE),
      mRequiresAAT(PR_FALSE),
      mIsCFFInitialized(PR_FALSE)
{
    // xxx - stretch is basically ignored for now

    mUserFontData = aUserFontData;
    mWeight = aWeight;
    mStretch = aStretch;
    mFixedPitch = PR_FALSE; // xxx - do we need this for downloaded fonts?
    mItalic = (aItalicStyle & (FONT_STYLE_ITALIC | FONT_STYLE_OBLIQUE)) != 0;
    mIsUserFont = aUserFontData != nsnull;
}

ATSFontRef
MacOSFontEntry::GetFontRef()
{
    if (!mATSFontRefInitialized) {
        mATSFontRefInitialized = PR_TRUE;
        NSString *psname = GetNSStringForString(mName);
        mATSFontRef = ::ATSFontFindFromPostScriptName(CFStringRef(psname),
                                                      kATSOptionFlagsDefault);
    }
    return mATSFontRef;
}

// ATSUI requires AAT-enabled fonts to render complex scripts correctly.
// For now, simple clear out the cmap codepoints for fonts that have
// codepoints for complex scripts. (Bug 361986)
// Core Text is similar, but can render Arabic using OpenType fonts as well.

enum eComplexScript {
    eComplexScriptArabic,
    eComplexScriptIndic,
    eComplexScriptTibetan
};

struct ScriptRange {
    eComplexScript   script;
    PRUint32         rangeStart;
    PRUint32         rangeEnd;
};

const ScriptRange gScriptsThatRequireShaping[] = {
    { eComplexScriptArabic, 0x0600, 0x077F },   // Basic Arabic and Arabic Supplement
    { eComplexScriptIndic, 0x0900, 0x0D7F },     // Indic scripts - Devanagari, Bengali, ..., Malayalam
    { eComplexScriptTibetan, 0x0F00, 0x0FFF }     // Tibetan
    // Thai seems to be "renderable" without AAT morphing tables
    // xxx - Lao, Khmer?
};

nsresult
MacOSFontEntry::ReadCMAP()
{
    ByteCount size;

    // attempt this once, if errors occur leave a blank cmap
    if (mCmapInitialized)
        return NS_OK;
    mCmapInitialized = PR_TRUE;

    PRUint32 kCMAP = TRUETYPE_TAG('c','m','a','p');

    nsAutoTArray<PRUint8,16384> cmap;
    if (GetFontTable(kCMAP, cmap) != NS_OK)
        return NS_ERROR_FAILURE;

    PRPackedBool  unicodeFont, symbolFont; // currently ignored
    nsresult rv = gfxFontUtils::ReadCMAP(cmap.Elements(), cmap.Length(),
                                         mCharacterMap, mUVSOffset,
                                         unicodeFont, symbolFont);
    if (NS_FAILED(rv)) {
        mCharacterMap.reset();
        return rv;
    }
    mHasCmapTable = PR_TRUE;

    ATSFontRef fontRef = GetFontRef();

    // for layout support, check for the presence of mort/morx and GSUB/GPOS
    PRBool hasAATLayout =
        (::ATSFontGetTable(fontRef, TRUETYPE_TAG('m','o','r','x'),
                           0, 0, 0, &size) == noErr) ||
        (::ATSFontGetTable(fontRef, TRUETYPE_TAG('m','o','r','t'),
                           0, 0, 0, &size) == noErr);

    PRBool hasOTLayout =
        (::ATSFontGetTable(fontRef, TRUETYPE_TAG('G','S','U','B'),
                           0, 0, 0, &size) == noErr) ||
        (::ATSFontGetTable(fontRef, TRUETYPE_TAG('G','P','O','S'),
                           0, 0, 0, &size) == noErr);

    if (hasAATLayout && !hasOTLayout) {
        mRequiresAAT = PR_TRUE;
    }

    PRUint32 numScripts =
        sizeof(gScriptsThatRequireShaping) / sizeof(ScriptRange);

    for (PRUint32 s = 0; s < numScripts; s++) {
        eComplexScript  whichScript = gScriptsThatRequireShaping[s].script;

        // check to see if the cmap includes complex script codepoints
        if (mCharacterMap.TestRange(gScriptsThatRequireShaping[s].rangeStart,
                                    gScriptsThatRequireShaping[s].rangeEnd)) {
            PRBool omitRange = PR_TRUE;

            if (hasAATLayout) {
                omitRange = PR_FALSE;
            } else if (whichScript == eComplexScriptArabic) {
                // special-case for Arabic:
                // even if there's no morph table, CoreText can shape Arabic
                // using OpenType layout
                if (hasOTLayout) {
                    // TODO: to be really thorough, we could check that the
                    // GSUB table actually supports the 'arab' script tag.
                    omitRange = PR_FALSE;
                }
            }

            if (omitRange) {
                mCharacterMap.ClearRange(gScriptsThatRequireShaping[s].rangeStart,
                                         gScriptsThatRequireShaping[s].rangeEnd);
            }
        }
    }

    PR_LOG(gFontInfoLog, PR_LOG_DEBUG, ("(fontinit-cmap) psname: %s, size: %d\n",
                                        NS_ConvertUTF16toUTF8(mName).get(),
                                        mCharacterMap.GetSize()));

    return rv;
}

nsresult
MacOSFontEntry::GetFontTable(PRUint32 aTableTag, nsTArray<PRUint8>& aBuffer)
{
    nsAutoreleasePool localPool;

    ATSFontRef fontRef = GetFontRef();
    if (fontRef == (ATSFontRef)kATSUInvalidFontID) {
        return NS_ERROR_FAILURE;
    }

    ByteCount dataLength;
    OSStatus status = ::ATSFontGetTable(fontRef, aTableTag, 0, 0, 0, &dataLength);
    if (status != noErr) {
        return NS_ERROR_FAILURE;
    }

    if (!aBuffer.AppendElements(dataLength)) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    PRUint8 *dataPtr = aBuffer.Elements();

    status = ::ATSFontGetTable(fontRef, aTableTag, 0, dataLength, dataPtr, &dataLength);
    NS_ENSURE_TRUE(status == noErr, NS_ERROR_FAILURE);

    return NS_OK;
}

gfxFont*
MacOSFontEntry::CreateFontInstance(const gfxFontStyle *aFontStyle, PRBool aNeedsBold)
{
    return new gfxMacFont(this, aFontStyle, aNeedsBold);
}

PRBool
MacOSFontEntry::IsCFF()
{
    if (!mIsCFFInitialized) {
        mIsCFFInitialized = PR_TRUE;
        ATSFontRef fontRef = GetFontRef();
        if (fontRef != (ATSFontRef)kATSUInvalidFontID) {
            ByteCount dataLength;
            OSStatus status = ::ATSFontGetTable(fontRef,
                                                TRUETYPE_TAG('C','F','F',' '),
                                                0, 0, 0, &dataLength);
            if (status == noErr && dataLength > 0) {
                mIsCFF = PR_TRUE;
            }
        }
    }

    return mIsCFF;
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

    void EliminateDuplicateFaces();
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

    // Bug 420981 - under 10.5, UltraLight and Light have the same weight value
    PRBool needToCheckLightFaces =
        (gfxPlatformMac::GetPlatform()->OSXVersion() >= MAC_OS_X_VERSION_10_5_HEX);

    for (faceIndex = 0; faceIndex < faceCount; faceIndex++) {
        NSArray *face = [fontfaces objectAtIndex:faceIndex];
        NSString *psname = [face objectAtIndex:INDEX_FONT_POSTSCRIPT_NAME];
        PRInt32 appKitWeight = [[face objectAtIndex:INDEX_FONT_WEIGHT] unsignedIntValue];
        PRUint32 macTraits = [[face objectAtIndex:INDEX_FONT_TRAITS] unsignedIntValue];
        NSString *facename = [face objectAtIndex:INDEX_FONT_FACE_NAME];
        PRBool isStandardFace = PR_FALSE;

        if (needToCheckLightFaces && appKitWeight == kAppleExtraLightWeight) {
            // if the facename contains UltraLight, set the weight to the ultralight weight value
            NSRange range = [facename rangeOfString:@"ultralight" options:NSCaseInsensitiveSearch];
            if (range.location != NSNotFound) {
                appKitWeight = kAppleUltraLightWeight;
            }
        }

        PRInt32 cssWeight = gfxMacPlatformFontList::AppleWeightToCSSWeight(appKitWeight) * 100;

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
            isStandardFace = PR_TRUE;
        }

        // create a font entry
        MacOSFontEntry *fontEntry = new MacOSFontEntry(postscriptFontName,
                                                       cssWeight, this, isStandardFace);
        if (!fontEntry) break;

        // set additional properties based on the traits reported by Cocoa
        if (macTraits & (NSCondensedFontMask | NSNarrowFontMask | NSCompressedFontMask)) {
            fontEntry->mStretch = NS_FONT_STRETCH_CONDENSED;
        } else if (macTraits & NSExpandedFontMask) {
            fontEntry->mStretch = NS_FONT_STRETCH_EXPANDED;
        }
        if (macTraits & NSItalicFontMask) {
            fontEntry->mItalic = PR_TRUE;
        }
        if (macTraits & NSFixedPitchFontMask) {
            fontEntry->mFixedPitch = PR_TRUE;
        }

#ifdef PR_LOGGING
        if (LOG_ENABLED()) {
            LOG(("(fontinit) added (%s) to family (%s)"
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
    SetHasStyles(PR_TRUE);

    if (mIsBadUnderlineFamily) {
        SetBadUnderlineFonts();
    }
}

void
gfxMacFontFamily::EliminateDuplicateFaces()
{
    PRUint32 i, bold, numFonts, italicIndex;
    MacOSFontEntry *italic, *nonitalic;

    FindStyleVariations();

    // if normal and italic have the same ATS font ref, delete italic
    // if bold and bold-italic have the same ATS font ref, delete bold-italic

    // two iterations, one for normal, one for bold
    for (bold = 0; bold < 2; bold++) {
        numFonts = mAvailableFonts.Length();

        // find the non-italic face
        nonitalic = nsnull;
        for (i = 0; i < numFonts; i++) {
            if ((mAvailableFonts[i]->IsBold() == (bold == 1)) &&
                !mAvailableFonts[i]->IsItalic()) {
                nonitalic = static_cast<MacOSFontEntry*>(mAvailableFonts[i].get());
                break;
            }
        }

        // find the italic face
        if (nonitalic) {
            italic = nsnull;
            for (i = 0; i < numFonts; i++) {
                if ((mAvailableFonts[i]->IsBold() == (bold == 1)) &&
                     mAvailableFonts[i]->IsItalic()) {
                    italic = static_cast<MacOSFontEntry*>(mAvailableFonts[i].get());
                    italicIndex = i;
                    break;
                }
            }

            // if italic face and non-italic face have matching ATS refs,
            // or if the italic returns 0 rather than an actual ATSFontRef,
            // then the italic face is bogus so remove it
            if (italic && (italic->GetFontRef() == 0 ||
                           italic->GetFontRef() == nonitalic->GetFontRef())) {
                mAvailableFonts.RemoveElementAt(italicIndex);
            }
        }
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
    if (mOtherFamilyNamesInitialized)
        return;

    gfxFontEntry *fe = mAvailableFonts[0];
    if (!fe)
        return;

    const PRUint32 kNAME = TRUETYPE_TAG('n','a','m','e');
    nsAutoTArray<PRUint8,8192> buffer;

    if (fe->GetFontTable(kNAME, buffer) != NS_OK)
        return;

    mHasOtherFamilyNames = ReadOtherFamilyNamesForFace(aPlatformFontList,
                                                       buffer,
                                                       PR_TRUE);
    mOtherFamilyNamesInitialized = PR_TRUE;
}


/* gfxMacPlatformFontList */
#pragma mark-

gfxMacPlatformFontList::gfxMacPlatformFontList() :
    gfxPlatformFontList(PR_FALSE), mATSGeneration(PRUint32(kATSGenerationInitial))
{
    ::ATSFontNotificationSubscribe(ATSNotification,
                                   kATSFontNotifyOptionDefault,
                                   (void*)this, nsnull);

    // this should always be available (though we won't actually fail if it's missing,
    // we'll just end up doing a search and then caching the new result instead)
    mReplacementCharFallbackFamily = NS_LITERAL_STRING("Lucida Grande");

    // cache this in a static variable so that MacOSFontFamily objects
    // don't have to repeatedly look it up
    sFontManager = [NSFontManager sharedFontManager];
}

void
gfxMacPlatformFontList::InitFontList()
{
    nsAutoreleasePool localPool;

    ATSGeneration currentGeneration = ::ATSGetGeneration();

    // need to ignore notifications after adding each font
    if (mATSGeneration == currentGeneration)
        return;

    mATSGeneration = currentGeneration;
    PR_LOG(gFontInfoLog, PR_LOG_DEBUG, ("(fontinit) updating to generation: %d", mATSGeneration));

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

    // clean up various minor 10.4 font problems for specific fonts
    if (gfxPlatformMac::GetPlatform()->OSXVersion() < MAC_OS_X_VERSION_10_5_HEX) {
        // Cocoa calls report that italic faces exist for Courier and Helvetica,
        // even though only bold faces exist so test for this using ATS font refs (10.5 has proper faces)
        EliminateDuplicateFaces(NS_LITERAL_STRING("Courier"));
        EliminateDuplicateFaces(NS_LITERAL_STRING("Helvetica"));

        // Cocoa reports that Courier and Monaco are not fixed-pitch fonts
        // so explicitly tweak these settings
        SetFixedPitch(NS_LITERAL_STRING("Courier"));
        SetFixedPitch(NS_LITERAL_STRING("Monaco"));
    }

    // start the delayed cmap loader
    StartLoader(kDelayBeforeLoadingCmaps, kIntervalBetweenLoadingCmaps);
}

void
gfxMacPlatformFontList::InitSingleFaceList()
{
    nsAutoTArray<nsString, 10> singleFaceFonts;
    gfxFontUtils::GetPrefsFontList("font.single-face-list", singleFaceFonts);

    PRUint32 numFonts = singleFaceFonts.Length();
    for (PRUint32 i = 0; i < numFonts; i++) {
        PR_LOG(gFontInfoLog, PR_LOG_DEBUG, ("(fontlist-singleface) face name: %s\n",
                                            NS_ConvertUTF16toUTF8(singleFaceFonts[i]).get()));
        gfxFontEntry *fontEntry = LookupLocalFont(nsnull, singleFaceFonts[i]);
        if (fontEntry) {
            nsAutoString familyName, key;
            familyName = singleFaceFonts[i];
            GenerateFontListKey(familyName, key);
            PR_LOG(gFontInfoLog, PR_LOG_DEBUG, ("(fontlist-singleface) family name: %s, key: %s\n",
                   NS_ConvertUTF16toUTF8(familyName).get(), NS_ConvertUTF16toUTF8(key).get()));

            // add only if doesn't exist already
            PRBool found;
            gfxFontFamily *familyEntry;
            if (!(familyEntry = mFontFamilies.GetWeak(key, &found))) {
                familyEntry = new gfxSingleFaceMacFontFamily(familyName);
                familyEntry->AddFontEntry(fontEntry);
                familyEntry->SetHasStyles(PR_TRUE);
                mFontFamilies.Put(key, familyEntry);
                fontEntry->mFamily = familyEntry;
                PR_LOG(gFontInfoLog, PR_LOG_DEBUG, ("(fontlist-singleface) added new family\n",
                       NS_ConvertUTF16toUTF8(familyName).get(), NS_ConvertUTF16toUTF8(key).get()));
            }
        }
    }
}

void
gfxMacPlatformFontList::EliminateDuplicateFaces(const nsAString& aFamilyName)
{
    gfxMacFontFamily *family =
        static_cast<gfxMacFontFamily*>(FindFamily(aFamilyName));

    if (family)
        family->EliminateDuplicateFaces();
}

PRBool
gfxMacPlatformFontList::GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName)
{
    gfxFontFamily *family = FindFamily(aFontName);
    if (family) {
        family->LocalizedName(aFamilyName);
        return PR_TRUE;
    }

    // Gecko 1.8 used Quickdraw font api's which produce a slightly different set of "family"
    // names.  Try to resolve based on these names, in case this is stored in an old profile
    // 1.8: "Futura", "Futura Condensed" ==> 1.9: "Futura"

    // convert the name to a Pascal-style Str255 to try as Quickdraw name
    Str255 qdname;
    NS_ConvertUTF16toUTF8 utf8name(aFontName);
    qdname[0] = PR_MAX(255, strlen(utf8name.get()));
    memcpy(&qdname[1], utf8name.get(), qdname[0]);

    // look up the Quickdraw name
    ATSFontFamilyRef atsFamily = ::ATSFontFamilyFindFromQuickDrawName(qdname);
    if (atsFamily == (ATSFontFamilyRef)kInvalidFontFamily) {
        return PR_FALSE;
    }

    // if we found a family, get its ATS name
    CFStringRef cfName;
    OSStatus status = ::ATSFontFamilyGetName(atsFamily, kATSOptionFlagsDefault, &cfName);
    if (status != noErr) {
        return PR_FALSE;
    }

    // then use this to locate the family entry and retrieve its localized name
    nsAutoString familyName;
    GetStringForNSString((const NSString*)cfName, familyName);
    ::CFRelease(cfName);

    family = FindFamily(familyName);
    if (family) {
        family->LocalizedName(aFamilyName);
        return PR_TRUE;
    }

    return PR_FALSE;
}

void
gfxMacPlatformFontList::ATSNotification(ATSFontNotificationInfoRef aInfo,
                                    void* aUserArg)
{
    // xxx - should be carefully pruning the list of fonts, not rebuilding it from scratch
    gfxMacPlatformFontList *qfc = (gfxMacPlatformFontList*)aUserArg;
    qfc->UpdateFontList();
}

gfxFontEntry*
gfxMacPlatformFontList::GetDefaultFont(const gfxFontStyle* aStyle, PRBool& aNeedsBold)
{
    nsAutoreleasePool localPool;

    NSString *defaultFamily = [[NSFont userFontOfSize:aStyle->size] familyName];
    nsAutoString familyName;

    GetStringForNSString(defaultFamily, familyName);
    return FindFontForFamily(familyName, aStyle, aNeedsBold);
}

PRInt32
gfxMacPlatformFontList::AppleWeightToCSSWeight(PRInt32 aAppleWeight)
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

    // first lookup a single face based on postscript name
    ATSFontRef fontRef = ::ATSFontFindFromPostScriptName(CFStringRef(faceName),
                                                         kATSOptionFlagsDefault);

    // if not found, lookup using full font name
    if (fontRef == kInvalidFont)
        fontRef = ::ATSFontFindFromName(CFStringRef(faceName),
                                        kATSOptionFlagsDefault);

    // not found
    if (fontRef == kInvalidFont)
        return nsnull;

    MacOSFontEntry *newFontEntry;
    if (aProxyEntry) {
        PRUint16 w = aProxyEntry->mWeight;
        NS_ASSERTION(w >= 100 && w <= 900, "bogus font weight value!");

        newFontEntry =
            new MacOSFontEntry(aFontName, fontRef,
                               w, aProxyEntry->mStretch,
                               aProxyEntry->mItalic ?
                                   FONT_STYLE_ITALIC : FONT_STYLE_NORMAL,
                               nsnull);
    } else {
        newFontEntry =
            new MacOSFontEntry(aFontName, fontRef,
                               400, 0, FONT_STYLE_NORMAL, nsnull);
    }

    return newFontEntry;
}

// grumble, another non-publised Apple API dependency (found in Webkit code)
// activated with this value, font will not be found via system lookup routines
// it can only be used via the created ATSFontRef
// needed to prevent one doc from finding a font used in a separate doc

enum {
    kPrivateATSFontContextPrivate = 3
};

class MacOSUserFontData : public gfxUserFontData {
public:
    MacOSUserFontData(ATSFontContainerRef aContainerRef)
        : mContainerRef(aContainerRef)
    { }

    virtual ~MacOSUserFontData()
    {
        // deactivate font
        if (mContainerRef)
            ::ATSFontDeactivate(mContainerRef, NULL, kATSOptionFlagsDefault);
    }

    ATSFontContainerRef     mContainerRef;
};

gfxFontEntry*
gfxMacPlatformFontList::MakePlatformFont(const gfxProxyFontEntry *aProxyEntry,
                                         const PRUint8 *aFontData,
                                         PRUint32 aLength)
{
    OSStatus err;

    NS_ASSERTION(aFontData, "MakePlatformFont called with null data");

    // MakePlatformFont is responsible for deleting the font data with NS_Free
    // so we set up a stack object to ensure it is freed even if we take an
    // early exit
    struct FontDataDeleter {
        FontDataDeleter(const PRUint8 *aFontData)
            : mFontData(aFontData) { }
        ~FontDataDeleter() { NS_Free((void*)mFontData); }
        const PRUint8 *mFontData;
    };
    FontDataDeleter autoDelete(aFontData);

    ATSFontRef fontRef;
    ATSFontContainerRef containerRef;

    // we get occasional failures when multiple fonts are activated in quick succession
    // if the ATS font cache is damaged; to work around this, we can retry the activation
    const PRUint32 kMaxRetries = 3;
    PRUint32 retryCount = 0;
    while (retryCount++ < kMaxRetries) {
        err = ::ATSFontActivateFromMemory(const_cast<PRUint8*>(aFontData), aLength,
                                          kPrivateATSFontContextPrivate,
                                          kATSFontFormatUnspecified,
                                          NULL,
                                          kATSOptionFlagsDoNotNotify,
                                          &containerRef);
        mATSGeneration = ::ATSGetGeneration();

        if (err != noErr) {
#if DEBUG
            char warnBuf[1024];
            sprintf(warnBuf, "downloaded font error, ATSFontActivateFromMemory err: %d for (%s)",
                    PRInt32(err),
                    NS_ConvertUTF16toUTF8(aProxyEntry->mFamily->Name()).get());
            NS_WARNING(warnBuf);
#endif
            return nsnull;
        }

        // ignoring containers with multiple fonts, use the first face only for now
        err = ::ATSFontFindFromContainer(containerRef, kATSOptionFlagsDefault, 1,
                                         &fontRef, NULL);
        if (err != noErr) {
#if DEBUG
            char warnBuf[1024];
            sprintf(warnBuf, "downloaded font error, ATSFontFindFromContainer err: %d for (%s)",
                    PRInt32(err),
                    NS_ConvertUTF16toUTF8(aProxyEntry->mFamily->Name()).get());
            NS_WARNING(warnBuf);
#endif
            ::ATSFontDeactivate(containerRef, NULL, kATSOptionFlagsDefault);
            return nsnull;
        }

        // now lookup the Postscript name; this may fail if the font cache is bad
        OSStatus err;
        NSString *psname = NULL;
        err = ::ATSFontGetPostScriptName(fontRef, kATSOptionFlagsDefault, (CFStringRef*) (&psname));
        if (err == noErr) {
            [psname release];
        } else {
#ifdef DEBUG
            char warnBuf[1024];
            sprintf(warnBuf, "ATSFontGetPostScriptName err = %d for (%s), retries = %d", (PRInt32)err,
                    NS_ConvertUTF16toUTF8(aProxyEntry->mFamily->Name()).get(), retryCount);
            NS_WARNING(warnBuf);
#endif
            ::ATSFontDeactivate(containerRef, NULL, kATSOptionFlagsDefault);
            // retry the activation a couple of times if this fails
            // (may be a transient failure due to ATS font cache issues)
            continue;
        }

        // font entry will own this
        MacOSUserFontData *userFontData = new MacOSUserFontData(containerRef);

        if (!userFontData) {
            ::ATSFontDeactivate(containerRef, NULL, kATSOptionFlagsDefault);
            return nsnull;
        }

        PRUint16 w = aProxyEntry->mWeight;
        NS_ASSERTION(w >= 100 && w <= 900, "bogus font weight value!");

        // create the font entry
        nsAutoString uniqueName;

        nsresult rv = gfxFontUtils::MakeUniqueUserFontName(uniqueName);
        if (NS_FAILED(rv)) {
            delete userFontData;
            return nsnull;
        }

        MacOSFontEntry *newFontEntry =
            new MacOSFontEntry(uniqueName,
                               fontRef,
                               w, aProxyEntry->mStretch,
                               aProxyEntry->mItalic ?
                                   FONT_STYLE_ITALIC : FONT_STYLE_NORMAL,
                               userFontData);

        if (!newFontEntry) {
            delete userFontData;
            return nsnull;
        }

        // if succeeded and font cmap is good, return the new font
        if (newFontEntry->mIsValid && NS_SUCCEEDED(newFontEntry->ReadCMAP()))
            return newFontEntry;

        // if something is funky about this font, delete immediately
#if DEBUG
        char warnBuf[1024];
        sprintf(warnBuf, "downloaded font not loaded properly, removed face for (%s)",
                NS_ConvertUTF16toUTF8(aProxyEntry->mFamily->Name()).get());
        NS_WARNING(warnBuf);
#endif
        delete newFontEntry;

        // We don't retry from here; the ATS font cache issue would have caused failure earlier
        // so if we get here, there's something else bad going on within our font data structures.
        // Currently, there should be no way to reach here, as fontentry creation cannot fail
        // except by memory allocation failure.
        NS_WARNING("invalid font entry for a newly activated font");
        break;
    }

    // if we get here, the activation failed (even with possible retries); can't use this font
    return nsnull;
}

