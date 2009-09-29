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
#include "gfxAtsuiFonts.h"
#include "gfxUserFontSet.h"

#include "nsServiceManagerUtils.h"
#include "nsTArray.h"

#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsISimpleEnumerator.h"

#include <unistd.h>
#include <time.h>

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

static PRLogModuleInfo *gFontInfoLog = PR_NewLogModule("fontInfoLog");

/* MacOSFontEntry */
#pragma mark-

MacOSFontEntry::MacOSFontEntry(const nsAString& aPostscriptName, 
                               PRInt32 aWeight,
                               gfxFontFamily *aFamily,
                               PRBool aIsStandardFace)
    : gfxFontEntry(aPostscriptName, aFamily, aIsStandardFace),
      mATSFontRef(0),
      mATSFontRefInitialized(PR_FALSE)
{
    mWeight = aWeight;
}

MacOSFontEntry::MacOSFontEntry(const nsAString& aPostscriptName, ATSFontRef aFontRef,
                               PRUint16 aWeight, PRUint16 aStretch, PRUint32 aItalicStyle,
                               gfxUserFontData *aUserFontData)
    : gfxFontEntry(aPostscriptName),
      mATSFontRef(aFontRef),
      mATSFontRefInitialized(PR_TRUE)
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
    OSStatus status;
    ByteCount size;

    // attempt this once, if errors occur leave a blank cmap
    if (mCmapInitialized)
        return NS_OK;
    mCmapInitialized = PR_TRUE;

    PRUint32 kCMAP = TRUETYPE_TAG('c','m','a','p');
    
    nsAutoTArray<PRUint8,16384> buffer;
    if (GetFontTable(kCMAP, buffer) != NS_OK)
        return NS_ERROR_FAILURE;
    PRUint8 *cmap = buffer.Elements();

    PRPackedBool  unicodeFont, symbolFont; // currently ignored
    nsresult rv = gfxFontUtils::ReadCMAP(cmap, buffer.Length(),
                                         mCharacterMap, unicodeFont, symbolFont);
                                         
    if (NS_FAILED(rv)) {
        mCharacterMap.reset();
        return rv;
    }

    // for complex scripts, check for the presence of mort/morx
    PRBool checkedForMorphTable = PR_FALSE, hasMorphTable = PR_FALSE;

    ATSFontRef fontRef = GetFontRef();
    PRUint32 s, numScripts = sizeof(gScriptsThatRequireShaping) / sizeof(ScriptRange);

    for (s = 0; s < numScripts; s++) {
        eComplexScript  whichScript = gScriptsThatRequireShaping[s].script;
        
        // check to see if the cmap includes complex script codepoints
        if (mCharacterMap.TestRange(gScriptsThatRequireShaping[s].rangeStart,
                                    gScriptsThatRequireShaping[s].rangeEnd)) {
            PRBool omitRange = PR_TRUE;

            // check for mort/morx table, if haven't already
            if (!checkedForMorphTable) {
                status = ::ATSFontGetTable(fontRef, TRUETYPE_TAG('m','o','r','x'), 0, 0, 0, &size);
                if (status == noErr) {
                    checkedForMorphTable = PR_TRUE;
                    hasMorphTable = PR_TRUE;
                } else {
                    // check for a mort table
                    status = ::ATSFontGetTable(fontRef, TRUETYPE_TAG('m','o','r','t'), 0, 0, 0, &size);
                    checkedForMorphTable = PR_TRUE;
                    if (status == noErr) {
                        hasMorphTable = PR_TRUE;
                    }
                }
            }

            if (hasMorphTable) {
                omitRange = PR_FALSE;
            }

            // special-cases for Arabic:
            if (whichScript == eComplexScriptArabic) {
                // even if there's no morph table, CoreText can shape if there's GSUB support
                if (gfxPlatformMac::GetPlatform()->UsingCoreText()) {
                    // with CoreText, don't exclude Arabic if the font has GSUB
                    status = ::ATSFontGetTable(fontRef, TRUETYPE_TAG('G','S','U','B'), 0, 0, 0, &size);
                    if (status == noErr) {
                        // TODO: to be really thorough, we could check that the GSUB table
                        // actually supports the 'arab' script tag.
                        omitRange = PR_FALSE;
                    }
                }

                // rude hack - the Chinese STxxx fonts on 10.4 contain morx tables and Arabic glyphs but 
                // lack the proper info for shaping Arabic, so exclude explicitly, ick
                if (mName.CharAt(0) == 'S' && mName.CharAt(1) == 'T') {
                    omitRange = PR_TRUE;
                }
            }

            if (omitRange) {
                mCharacterMap.ClearRange(gScriptsThatRequireShaping[s].rangeStart,
                                         gScriptsThatRequireShaping[s].rangeEnd);
            }
        }
    }

    PR_LOG(gFontInfoLog, PR_LOG_DEBUG, ("(fontinit-cmap) psname: %s, size: %d\n", 
                                        NS_ConvertUTF16toUTF8(mName).get(), mCharacterMap.GetSize()));
                                        
    return rv;
}

nsresult
MacOSFontEntry::GetFontTable(PRUint32 aTableTag, nsTArray<PRUint8>& aBuffer)
{
    ATSFontRef fontRef = GetFontRef();
    if (fontRef == (ATSFontRef)kATSUInvalidFontID)
        return NS_ERROR_FAILURE;

    ByteCount dataLength;
    OSStatus status = ::ATSFontGetTable(fontRef, aTableTag, 0, 0, 0, &dataLength);
    NS_ENSURE_TRUE(status == noErr, NS_ERROR_FAILURE);

    if (!aBuffer.AppendElements(dataLength))
        return NS_ERROR_OUT_OF_MEMORY;
    PRUint8 *dataPtr = aBuffer.Elements();

    status = ::ATSFontGetTable(fontRef, aTableTag, 0, dataLength, dataPtr, &dataLength);
    NS_ENSURE_TRUE(status == noErr, NS_ERROR_FAILURE);

    return NS_OK;
}
 

/* gfxMacPlatformFontList */
#pragma mark-

gfxMacPlatformFontList::gfxMacPlatformFontList()
{
    mATSGeneration = PRUint32(kATSGenerationInitial);

    ::ATSFontNotificationSubscribe(ATSNotification,
                                   kATSFontNotifyOptionDefault,
                                   (void*)this, nsnull);

    // this should always be available (though we won't actually fail if it's missing,
    // we'll just end up doing a search and then caching the new result instead)
    mReplacementCharFallbackFamily = NS_LITERAL_STRING("Lucida Grande");
}

void
gfxMacPlatformFontList::InitFontList()
{
    ATSGeneration currentGeneration = ::ATSGetGeneration();
    
    // need to ignore notifications after adding each font
    if (mATSGeneration == currentGeneration)
        return;

    mATSGeneration = currentGeneration;
    PR_LOG(gFontInfoLog, PR_LOG_DEBUG, ("(fontinit) updating to generation: %d", mATSGeneration));                                         
    
    // Bug 420981 - under 10.5, UltraLight and Light have the same weight value
    PRBool needToCheckLightFaces = PR_FALSE;
    if (gfxPlatformMac::GetPlatform()->OSXVersion() >= MAC_OS_X_VERSION_10_5_HEX) {
        needToCheckLightFaces = PR_TRUE;
    }
    
    mFontFamilies.Clear();
    mOtherFamilyNames.Clear();
    mOtherFamilyNamesInitialized = PR_FALSE;
    mPrefFonts.Clear();
    mCodepointsWithNoFonts.reset();
    CancelLoader();

    // iterate over available families
    NSFontManager *fontManager = [NSFontManager sharedFontManager];
    NSEnumerator *families = [[fontManager availableFontFamilies] objectEnumerator];  // returns "canonical", non-localized family name

    nsAutoString availableFamilyName, postscriptFontName;
   
    NSString *availableFamily = nil;
    while ((availableFamily = [families nextObject])) {

        // make a nsString
        GetStringForNSString(availableFamily, availableFamilyName);
        
        // create a family entry
        gfxFontFamily *familyEntry = new gfxFontFamily(availableFamilyName);
        if (!familyEntry) break;
        
        // create a font entry for each face
        NSArray *fontfaces = [fontManager availableMembersOfFontFamily:availableFamily];  // returns an array of [psname, style name, weight, traits] elements, goofy api
        int faceCount = [fontfaces count];
        int faceIndex;

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
            
            PR_LOG(gFontInfoLog, PR_LOG_DEBUG, ("(fontinit) family: %s, psname: %s, face: %s, apple-weight: %d, css-weight: %d, macTraits: %8.8x\n", 
                [availableFamily UTF8String], [psname UTF8String], [facename UTF8String], appKitWeight, cssWeight, macTraits));

            // make a nsString
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
                                                           cssWeight, familyEntry, isStandardFace);
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

            // insert into font entry array of family
            familyEntry->AddFontEntry(fontEntry);
        }

        familyEntry->SortAvailableFonts();
        familyEntry->SetHasStyles(PR_TRUE);

        // add the family entry to the hash table
        ToLowerCase(availableFamilyName);
        mFontFamilies.Put(availableFamilyName, familyEntry);
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

    // initialize ranges of characters for which system-wide font search should be skipped
    mCodepointsWithNoFonts.SetRange(0,0x1f);     // C0 controls
    mCodepointsWithNoFonts.SetRange(0x7f,0x9f);  // C1 controls

    InitBadUnderlineList();

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
                familyEntry = new gfxFontFamily(familyName);
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
    gfxFontFamily *family = FindFamily(aFamilyName);
    if (!family) return;

    nsTArray<nsRefPtr<gfxFontEntry> >& fontlist = family->GetFontList();

    PRUint32 i, bold, numFonts, italicIndex;
    MacOSFontEntry *italic, *nonitalic;

    // if normal and italic have the same ATS font ref, delete italic
    // if bold and bold-italic have the same ATS font ref, delete bold-italic

    // two iterations, one for normal, one for bold
    for (bold = 0; bold < 2; bold++) {
        numFonts = fontlist.Length();

        // find the non-italic face
        nonitalic = nsnull;
        for (i = 0; i < numFonts; i++) {
            if ((fontlist[i]->IsBold() == (bold == 1)) && !fontlist[i]->IsItalic()) {
                nonitalic = static_cast<MacOSFontEntry*>(fontlist[i].get());
                break;
            }
        }

        // find the italic face
        if (nonitalic) {
            italic = nsnull;
            for (i = 0; i < numFonts; i++) {
                if ((fontlist[i]->IsBold() == (bold == 1)) && fontlist[i]->IsItalic()) {
                    italic = static_cast<MacOSFontEntry*>(fontlist[i].get());
                    italicIndex = i;
                    break;
                }
            }

            // if italic face and non-italic face have matching ATS refs,
            // or if the italic returns 0 rather than an actual ATSFontRef,
            // then the italic face is bogus so remove it
            if (italic && (italic->GetFontRef() == 0 ||
                           italic->GetFontRef() == nonitalic->GetFontRef())) {
                fontlist.RemoveElementAt(italicIndex);
            }
        }
    }
}

PRBool
gfxMacPlatformFontList::GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName)
{
    gfxFontFamily *family = FindFamily(aFontName);
    if (family) {
        GetLocalizedFamilyName(family, aFamilyName);
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
        GetLocalizedFamilyName(family, aFamilyName);
        return PR_TRUE;
    }

    return PR_FALSE;
}

void
gfxMacPlatformFontList::GetLocalizedFamilyName(gfxFontFamily *aFamily,
                                           nsAString& aLocalizedName)
{
    if (!aFamily->HasOtherFamilyNames()) {
        aLocalizedName = aFamily->Name();
        return;
    }

    NSFontManager *fontManager = [NSFontManager sharedFontManager];

    // dig out the localized family name
    NSString *familyName = GetNSStringForString(aFamily->Name());
    NSString *localizedFamily = [fontManager localizedNameForFamily:familyName face:nil];

    if (localizedFamily) {
        GetStringForNSString(localizedFamily, aLocalizedName);
    } else {
        // failed to get a localized name, just use the canonical name
        aLocalizedName = aFamily->Name();
    }
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
gfxMacPlatformFontList::MakePlatformFont(const gfxFontEntry *aProxyEntry,
                                         const PRUint8 *aFontData,
                                         PRUint32 aLength)
{
    OSStatus err;
    
    NS_ASSERTION(aFontData, "MakePlatformFont called with null data");

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
            const gfxProxyFontEntry *proxyEntry = 
                static_cast<const gfxProxyFontEntry*> (aProxyEntry);
            sprintf(warnBuf, "downloaded font error, ATSFontActivateFromMemory err: %d for (%s)",
                    PRInt32(err),
                    NS_ConvertUTF16toUTF8(proxyEntry->mFamily->Name()).get());
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
            const gfxProxyFontEntry *proxyEntry = 
                static_cast<const gfxProxyFontEntry*> (aProxyEntry);
            sprintf(warnBuf, "downloaded font error, ATSFontFindFromContainer err: %d for (%s)",
                    PRInt32(err),
                    NS_ConvertUTF16toUTF8(proxyEntry->mFamily->Name()).get());
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
            const gfxProxyFontEntry *proxyEntry = 
                static_cast<const gfxProxyFontEntry*> (aProxyEntry);
            sprintf(warnBuf, "ATSFontGetPostScriptName err = %d for (%s), retries = %d", (PRInt32)err,
                    NS_ConvertUTF16toUTF8(proxyEntry->mFamily->Name()).get(), retryCount);
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
        const gfxProxyFontEntry *proxyEntry = 
            static_cast<const gfxProxyFontEntry*> (aProxyEntry);
        sprintf(warnBuf, "downloaded font not loaded properly, removed face for (%s)", 
                NS_ConvertUTF16toUTF8(proxyEntry->mFamily->Name()).get());
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

