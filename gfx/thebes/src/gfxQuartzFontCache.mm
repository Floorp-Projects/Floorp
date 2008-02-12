/* -*- Mode: ObjC; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: BSD
 *
 * Copyright (C) 2006 Mozilla Corporation.  All rights reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   John Daggett <jdaggett@mozilla.com>
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

#include <Carbon.h>

#import <AppKit/AppKit.h>

#include "gfxPlatformMac.h"
#include "gfxQuartzFontCache.h"
#include "gfxAtsuiFonts.h"

#include "nsIPref.h"  // for pref changes callback notification
#include "nsServiceManagerUtils.h"

// _atsFontID is private; add it in our new category to NSFont
@interface NSFont (MozillaCategory)
- (ATSUFontID)_atsFontID;
@end

#define INDEX_FONT_POSTSCRIPT_NAME 0
#define INDEX_FONT_FACE_NAME 1
#define INDEX_FONT_WEIGHT 2
#define INDEX_FONT_TRAITS 3
 
static const int kAppleMaxWeight = 14;

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

void
gfxQuartzFontCache::GenerateFontListKey(const nsAString& aKeyName, nsAString& aResult)
{
    aResult = aKeyName;
    ToLowerCase(aResult);
}

/* MacOSFontEntry */
#pragma mark-

MacOSFontEntry::MacOSFontEntry(const nsAString& aPostscriptName, 
                                PRInt32 aAppleWeight, PRUint32 aTraits, MacOSFamilyEntry *aFamily)
    : mPostscriptName(aPostscriptName), mTraits(aTraits), mFamily(aFamily), mATSUFontID(0),
        mCmapInitialized(0), mATSUIDInitialized(0)
{
    mWeight = gfxQuartzFontCache::AppleWeightToCSSWeight(aAppleWeight);
}

const nsString& 
MacOSFontEntry::FamilyName()
{
    return mFamily->Name();
}

PRBool MacOSFontEntry::IsFixedPitch() {
    return mTraits & NSFixedPitchFontMask;
}

PRBool MacOSFontEntry::IsItalicStyle() {
    return mTraits & NSItalicFontMask;
}

PRBool MacOSFontEntry::IsBold() {
    return mTraits & NSBoldFontMask;
}

ATSUFontID MacOSFontEntry::GetFontID() 
{
    if (!mATSUIDInitialized) {
        mATSUIDInitialized = PR_TRUE;
        NSString *psname = GetNSStringForString(mPostscriptName);
        NSFont *font = [NSFont fontWithName:psname size:0.0];
        if (font) mATSUFontID = [font _atsFontID];
    }
    return mATSUFontID; 
}

// ATSUI requires AAT-enabled fonts to render complex scripts correctly.
// For now, simple clear out the cmap codepoints for fonts that have
// codepoints for complex scripts. (Bug 361986)

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
    
    if (mCmapInitialized) return NS_OK;
    ATSUFontID fontID = GetFontID();
    
    // attempt this once, if errors occur leave a blank cmap
    mCmapInitialized = PR_TRUE;
    
    status = ATSFontGetTable(fontID, 'cmap', 0, 0, 0, &size);
    //printf( "cmap size: %s %d\n", NS_ConvertUTF16toUTF8(mName).get(), size );
    NS_ENSURE_TRUE(status == noErr, NS_ERROR_FAILURE);

    nsAutoTArray<PRUint8,16384> buffer;
    if (!buffer.AppendElements(size))
        return NS_ERROR_OUT_OF_MEMORY;
    PRUint8 *cmap = buffer.Elements();

    status = ATSFontGetTable(fontID, 'cmap', 0, size, cmap, &size);
    NS_ENSURE_TRUE(status == noErr, NS_ERROR_FAILURE);

    nsresult rv = NS_ERROR_FAILURE;
    PRPackedBool  unicodeFont, symbolFont; // currently ignored
    rv = gfxFontUtils::ReadCMAP(cmap, size, mCharacterMap, mUnicodeRanges, unicodeFont, symbolFont);

    // for complex scripts, check for the presence of mort/morx
    PRBool checkedForMorphTable = PR_FALSE, hasMorphTable = PR_FALSE;
    
    PRUint32 s, numScripts = sizeof(gScriptsThatRequireShaping) / sizeof(ScriptRange);
    
    for (s = 0; s < numScripts; s++) {
        eComplexScript  whichScript = gScriptsThatRequireShaping[s].script;
        
        // check to see if the cmap includes complex script codepoints
        if (mCharacterMap.TestRange(gScriptsThatRequireShaping[s].rangeStart, gScriptsThatRequireShaping[s].rangeEnd)) {
            
            // check for mort/morx table, if haven't already
            if (!checkedForMorphTable) {
                status = ATSFontGetTable(fontID, 'morx', 0, 0, 0, &size);
                if ( status == noErr ) {
                    checkedForMorphTable = PR_TRUE;
                    hasMorphTable = PR_TRUE;
                } else {
                    // check for a mort table
                    status = ATSFontGetTable(fontID, 'mort', 0, 0, 0, &size);
                    checkedForMorphTable = PR_TRUE;
                    if ( status == noErr ) {
                        hasMorphTable = PR_TRUE;
                    }
                }
            }
            
            // rude hack - the Chinese STxxx fonts on 10.4 contain morx tables and Arabic glyphs but 
            // lack the proper info for shaping Arabic, so exclude explicitly, ick
            if (whichScript == eComplexScriptArabic && hasMorphTable) {
                if (mPostscriptName.CharAt(0) == 'S' && mPostscriptName.CharAt(1) == 'T') {
                    mCharacterMap.ClearRange(gScriptsThatRequireShaping[s].rangeStart, gScriptsThatRequireShaping[s].rangeEnd);
                }
            }

            // general exclusion - if no morph table, exclude codepoints
            if (!hasMorphTable) {
                mCharacterMap.ClearRange(gScriptsThatRequireShaping[s].rangeStart, gScriptsThatRequireShaping[s].rangeEnd);
            }
        }
    }

    return rv;
}


/* MacOSFamilyEntry */
#pragma mark-

static const PRUint32 kTraits_NonNormalWidthMask = NSNarrowFontMask | NSExpandedFontMask | 
                            NSCondensedFontMask | NSCompressedFontMask | NSFixedPitchFontMask;

MacOSFontEntry*
MacOSFamilyEntry::FindFont(const gfxFontStyle* aStyle)
{
    // short-circuit the single face per family case
    if (mAvailableFonts.Length() == 1) return mAvailableFonts[0];
    
    PRBool found = PR_FALSE;
    PRBool isItalic = (aStyle->style == FONT_STYLE_ITALIC || aStyle->style == FONT_STYLE_OBLIQUE);
    MacOSFontEntry* fontsWithTraits[10];
    
    memset(fontsWithTraits, 0, sizeof(fontsWithTraits));
    
    // match italic faces
    if ( isItalic ) {    
        // first search for italic normal width fonts
        found = FindFontsWithTraits(fontsWithTraits, NSItalicFontMask, kTraits_NonNormalWidthMask);
        
        // if not found, italic any width ones
        if (!found) {
            found = FindFontsWithTraits(fontsWithTraits, NSItalicFontMask, 0);        
        }
    }
    
    // match non-italic faces, if no italic faces fall through here
    if (!found) {
        // look for normal width fonts
        found = FindFontsWithTraits(fontsWithTraits, NSUnitalicFontMask, kTraits_NonNormalWidthMask);
        
        // if not found, any face will do
        if (!found) {
            found = FindFontsWithTraits(fontsWithTraits, NSUnitalicFontMask, 0);        
        } 
    }
    
    // still not found?!? family must only contain italic fonts when looking for a normal 
    // face, just use the whole list
    if (!found) {
        found = FindFontsWithTraits(fontsWithTraits, 0, 0);
    }
    NS_ASSERTION(found, "Font family containing no faces");
    if (!found) return nsnull;
    
    MacOSFontEntry* chosenFont = FindFontWeight(fontsWithTraits, aStyle);
    NS_ASSERTION(chosenFont, "Somehow selected a null font entry when choosing based on font weight");
    return chosenFont;
}

void
MacOSFamilyEntry::FindFontForChar(FontSearch *aMatchData)
{
    PRUint32 numFonts, i;
    
    // xxx - optimization point - keep a bit vector with the union of supported unicode ranges
    // by all fonts for this family and bail immediately if the character is not in any of
    // this family's cmaps
    
    // iterate over fonts
    numFonts = mAvailableFonts.Length();
    for (i = 0; i < numFonts; i++) {
        MacOSFontEntry *fe = mAvailableFonts[i];
        PRInt32 rank = 0;
    
        if (fe->TestCharacterMap(aMatchData->ch)) {
            rank += 20;
        }
    
        // if we didn't match any characters don't bother wasting more time.
        if (rank == 0)
            return;
            
        // omitting from original windows code -- family name, lang group, pitch
        // not available in current FontEntry implementation
    
        if (aMatchData->fontToMatch) { 
            const gfxFontStyle *style = aMatchData->fontToMatch->GetStyle();
            
            // italics
            if (fe->IsItalicStyle() && 
                    (style->style == FONT_STYLE_ITALIC || style->style == FONT_STYLE_ITALIC)) {
                rank += 5;
            }
            
            // weight
            PRInt8 baseWeight, weightDistance;
            style->ComputeWeightAndOffset(&baseWeight, &weightDistance);
    
            // xxx - not entirely correct, the one unit of weight distance reflects 
            // the "next bolder/lighter face"
            PRUint32 targetWeight = (baseWeight * 100) + (weightDistance * 100);
    
            PRUint32 entryWeight = fe->Weight() * 100;
            if (entryWeight == targetWeight) {
                rank += 5;
            } else {
                PRUint32 diffWeight = abs(entryWeight - targetWeight);
                if (diffWeight <= 100)  // favor faces close in weight
                    rank += 2;
            }
        } else {
            // if no font to match, prefer non-bold, non-italic fonts
            if (!fe->IsItalicStyle() && !fe->IsBold())
                rank += 5;
        }
        
        // xxx - add whether AAT font with morphing info for specific lang groups
        
        if (rank > aMatchData->matchRank
            || (rank == aMatchData->matchRank && Compare(fe->Name(), aMatchData->bestMatch->Name()) > 0)) 
        {
            aMatchData->bestMatch = fe;
            aMatchData->matchRank = rank;
        }
    
    }
}

PRBool
MacOSFamilyEntry::FindFontsWithTraits(MacOSFontEntry* aFontsForWeights[], PRUint32 aPosTraitsMask, 
                                        PRUint32 aNegTraitsMask)
{
    PRUint32 numFonts, i;
    PRBool found = PR_FALSE;
    
    // iterate over fonts
    numFonts = mAvailableFonts.Length();
    for (i = 0; i < numFonts; i++) {
        MacOSFontEntry *fe = mAvailableFonts[i];
        
        // if traits match, add to list of fonts
        PRUint32 traits = fe->Traits();
        
        // aPosTraitsMask == 0 ==> match all
        if ( (!aPosTraitsMask || (traits & aPosTraitsMask)) && !(traits & aNegTraitsMask)) {
            PRInt32 weight = fe->Weight();
            
            // always prefer the first font for a given weight, helps deal a bit with 
            // families with lots of faces (e.g. Minion Pro)
            if (!aFontsForWeights[weight]) {
                aFontsForWeights[weight] = fe;
                found = PR_TRUE;
            }
        }
    }
    return found;
}

MacOSFontEntry* 
MacOSFamilyEntry::FindFontWeight(MacOSFontEntry* aFontsForWeights[], const gfxFontStyle* aStyle)
{
    // calculate the desired weight from the style
    PRInt32 w, direction, offset, baseMatch;
    PRInt8 baseWeight, weightDistance;

    aStyle->ComputeWeightAndOffset(&baseWeight, &weightDistance);
    NS_ASSERTION(baseWeight != 0, "Style with font weight 0 (whacked)");
    
    // choose the weight that matches the base weight using CSS Fonts spec rules
    
    // have the desired weight ==> use it
    baseMatch = 0;
    if (aFontsForWeights[baseWeight]) {
    
        baseMatch = baseWeight;
        
    } else {
    
        // CSS2.1 and draft CSS3 Fonts specs are ambiguous about how to handle missing 400 weight face
        // substitute 400 and 500 for each other (example: Futura family that ships with Mac OS X)
        if (baseWeight == 4 && aFontsForWeights[5]) {
            baseMatch = 5;
        } else if (baseWeight == 5 && aFontsForWeights[4]) {
            baseMatch = 4;
        } else {
        
            // otherwise, use explicit CSS rules
            // weights above 400 ==> look up in weights, then down, otherwise look down, then up
            direction = (baseWeight > 4 ? 1 : -1);
            
            // search in one direction
            for (w = baseWeight + direction; w >= 1 && w <= 9; w += direction) {
                if (aFontsForWeights[w]) {
                    baseMatch = w;
                    break;
                }
            }
            
            // not found? switch direction and search the remaining entries
            if (!baseMatch) {
                direction = -direction;
                for (w = baseWeight + direction; w >= 1 && w <= 9; w += direction) {
                    if (aFontsForWeights[w]) {
                        baseMatch = w;
                        break;
                    }
                }
            }
        }
    }
    
    // at this point, should have found an entry matching the base weight
    NS_ASSERTION(baseMatch, "Somehow didn't find matching weight");
                
    // handle weight offsets
    if (weightDistance) {
        direction = (weightDistance < 0 ? -1 : 1);
        offset = abs(weightDistance);
        
        // look for bolder/lighter face [offset] number of faces away from the base face
        // e.g. weight = 698 with Helvetica Neue ==> offset = 2, direction = -1, 
        //      baseMatch starts at 7 (Bold), then 4 (Regular), then 2 (Light)
        for (w = baseMatch + direction; w >= 1 && w <= 9 && offset; w += direction) {
            if (aFontsForWeights[w]) {
                baseMatch = w;
                offset--;
            }
        }
    }
    
    NS_ASSERTION(aFontsForWeights[baseMatch], "Chose a weight without a corresponding face");
    return aFontsForWeights[baseMatch];
}

/* gfxQuartzFontCache */
#pragma mark-

gfxQuartzFontCache *gfxQuartzFontCache::sSharedFontCache = nsnull;

gfxQuartzFontCache::gfxQuartzFontCache()
{
    mFontFamilies.Init(100);
    mLocalizedFamilies.Init(30);
    mPrefFonts.Init(10);

    InitFontList();
    ::ATSFontNotificationSubscribe(ATSNotification,
                                   kATSFontNotifyOptionDefault,
                                   (void*)this, nsnull);

    // pref changes notification setup
    nsCOMPtr<nsIPref> pref = do_GetService(NS_PREF_CONTRACTID);
    pref->RegisterCallback("font.", PrefChangedCallback, this);
    pref->RegisterCallback("font.name-list.", PrefChangedCallback, this);
    pref->RegisterCallback("intl.accept_languages", PrefChangedCallback, this);  // hmmmm...
    
}

static NSString* CreateNameFromBuffer(const UInt8 *aBuf, ByteCount aLength, 
        FontPlatformCode aPlatformCode, FontScriptCode aScriptCode, FontLanguageCode aLangCode )
{
    CFStringRef outName = NULL;
    
    if (aPlatformCode == kFontMacintoshPlatform) {
        TextEncoding encoding;
        OSStatus err = GetTextEncodingFromScriptInfo(aScriptCode, aLangCode, 
                                                        kTextRegionDontCare, &encoding);
        if (err) {
            // some fonts are sloppy about the language code (e.g Arial Hebrew, Corsiva Hebrew)
            // try again without the lang code to avoid bad combinations
            OSStatus err = GetTextEncodingFromScriptInfo(aScriptCode, kTextLanguageDontCare, 
                                                        kTextRegionDontCare, &encoding);
            if (err) return nil;
        }
        outName = CFStringCreateWithBytes(kCFAllocatorDefault, aBuf, 
                                            aLength, (CFStringEncoding) encoding, false);
    } else if (aPlatformCode == kFontUnicodePlatform) {
        outName = CFStringCreateWithCharacters(kCFAllocatorDefault, (UniChar*)aBuf, aLength/2);    
    } else if (aPlatformCode == kFontMicrosoftPlatform) {
        if ( aScriptCode == 0 ) {
            outName = CFStringCreateWithBytes(kCFAllocatorDefault, aBuf, 
                                                aLength, kCFStringEncodingUTF16BE, false);
        } else {
            outName = CFStringCreateWithCharacters(kCFAllocatorDefault, (UniChar*)aBuf, aLength/2);    
        }
    }

    return (NSString*) outName;
}


// 10.4 headers only define TT/OT name table id's up to the license id (14) but 10.5 does, so use our own enum
enum {
  kMozillaFontPreferredFamilyName            = 16,
};

// xxx - rather than use ATSUI calls, probably faster to load name table directly, 
// this avoids copying around strings that are of no interest

static void ReadLocalizedNames(NSString *familyName, NSArray *faceArray, 
                                int faceIndex, NSMutableArray *localizedNames)
{
    OSStatus err;
    ItemCount i, nameCount;
    ATSUFontID fontID;
    
    NSArray *face = [faceArray objectAtIndex:faceIndex];
    NSString *psname = [face objectAtIndex:INDEX_FONT_POSTSCRIPT_NAME];
    NSFont *font = [NSFont fontWithName:psname size:0.0];
    fontID = [font _atsFontID];
    
    err = ATSUCountFontNames(fontID, &nameCount);
    for (i = 0; i < nameCount; i++) {

        FontNameCode nameCode;
        FontPlatformCode platformCode;
        FontScriptCode scriptCode;
        FontLanguageCode langCode;
        const ByteCount kBufLength = 2048;
        char buf[kBufLength];
        ByteCount len;

        err = ATSUGetIndFontName(fontID, i, kBufLength, buf, &len, &nameCode, &platformCode, &scriptCode, &langCode);
        if (!(nameCode == kFontFamilyName || nameCode == kMozillaFontPreferredFamilyName)) continue; 
        if (len >= kBufLength) continue; 
        buf[len] = 0;
        
        NSString *name = CreateNameFromBuffer((UInt8*)buf, len, platformCode, scriptCode, langCode);
        NSString *foundName = nil;
        
        // add if not same as canonical family name or already in list of names
        if (name) {

            if (![name isEqualToString:familyName]) {
                
                // search other localized names
                int j, lnameCount = [localizedNames count];
                NSString *lname;
                for (j = 0; j < lnameCount; j++) {
                    lname = [localizedNames objectAtIndex:j];
                    if ([lname isEqualToString:name]) {
                        foundName = lname;
                        break;
                    }
                }
                
                // didn't find it in the list? add it
                if (!foundName)
                    [localizedNames addObject:name];
            }
    
            [name release];
        }
    }

}

const int kIndexNormalFace_NotFound = -1;
const PRUint32 kNonNormalTraits = NSItalicFontMask | NSBoldFontMask | NSNarrowFontMask | NSExpandedFontMask | NSCondensedFontMask | NSCompressedFontMask;

void
gfxQuartzFontCache::InitFontList()
{
    mFontFamilies.Clear();
    mLocalizedFamilies.Clear();
    mPrefFonts.Clear();
    mCodepointsWithNoFonts.reset();
    
    // iterate over available families
    NSFontManager *fontManager = [NSFontManager sharedFontManager];
    NSEnumerator *families = [[fontManager availableFontFamilies] objectEnumerator];  // returns "canonical", non-localized family name
    
    NSMutableArray* localizedNames = [[NSMutableArray alloc] init];
    nsAutoString availableFamilyName, postscriptFontName, localizedName;
   
    NSString *availableFamily = nil;
    while ((availableFamily = [families nextObject])) {

        // make a nsString
        GetStringForNSString(availableFamily, availableFamilyName);
        
        // create a family entry
        nsRefPtr<MacOSFamilyEntry> familyEntry = new MacOSFamilyEntry(availableFamilyName);
        if (!familyEntry) break;
        
        // create a font entry for each face
        NSArray *fontfaces = [fontManager availableMembersOfFontFamily:availableFamily];  // returns an array of [psname, style name, weight, traits] elements, goofy api
        int faceCount = [fontfaces count];
        int normalFaceIndex = kIndexNormalFace_NotFound;
        int faceIndex;

        for (faceIndex = 0; faceIndex < faceCount; faceIndex++) {
            NSArray *face = [fontfaces objectAtIndex:faceIndex];
            NSString *psname = [face objectAtIndex:INDEX_FONT_POSTSCRIPT_NAME];
            PRInt32 weight = [[face objectAtIndex:INDEX_FONT_WEIGHT] unsignedIntValue];
            PRUint32 traits = [[face objectAtIndex:INDEX_FONT_TRAITS] unsignedIntValue];
            
            // 10.5 doesn't set NSUnitalicFontMask and NSUnboldFontMask - manually set these for consistency 
            if (!(traits & NSBoldFontMask))
                traits |= NSUnboldFontMask;
            if (!(traits & NSItalicFontMask))
                traits |= NSUnitalicFontMask;
            
            PR_LOG(gFontInfoLog, PR_LOG_DEBUG, ("(init) family: %s, psname: %s, face: %s, apple-weight: %d, css-weight: %d, traits: %8.8x\n", 
                [availableFamily UTF8String], [psname UTF8String], [[face objectAtIndex:INDEX_FONT_FACE_NAME] UTF8String], weight, gfxQuartzFontCache::AppleWeightToCSSWeight(weight), traits ));

            // make a nsString
            GetStringForNSString(psname, postscriptFontName);
        
            // create a font entry
            nsRefPtr<MacOSFontEntry> fontEntry = new MacOSFontEntry(postscriptFontName, weight, traits, familyEntry);
            if (!fontEntry) break;            
            
            // insert into font entry array of family
            familyEntry->AddFontEntry(fontEntry);
            
            // if normal face not found yet, check the traits for this one
            if ( normalFaceIndex == kIndexNormalFace_NotFound && !(traits & kNonNormalTraits))
                normalFaceIndex = faceIndex;
        }
        
        // if no normal face, just use the first face in the array of available ones
        if ( normalFaceIndex == kIndexNormalFace_NotFound )
            normalFaceIndex = 0;
        
        // read the name table for the normal face; if localized names exist for that face, scan all font entries for more localized names
        [localizedNames removeAllObjects];
        ReadLocalizedNames(availableFamily, fontfaces, normalFaceIndex, localizedNames);
        if ([localizedNames count] != 0) {
            for (faceIndex = 0; faceIndex < faceCount; faceIndex++) {
                if (faceIndex == normalFaceIndex) continue;
                ReadLocalizedNames(availableFamily, fontfaces, faceIndex, localizedNames);
            }
        }
        
        // add the family entry to the hash table
        ToLowerCase(availableFamilyName);
        mFontFamilies.Put(availableFamilyName, familyEntry);
        
        // add in an entry for each of the localized names
        int lnameIndex, lnameCount = [localizedNames count];
        for (lnameIndex = 0; lnameIndex < lnameCount; lnameIndex++) {
            GetStringForNSString([localizedNames objectAtIndex:lnameIndex], localizedName);
            ToLowerCase(localizedName);
            mLocalizedFamilies.Put(localizedName, familyEntry);
        }
        
    }
    
    // xxx - deal with quirks (e.g. Osaka-Mono)
    
    // xxx - need to remove bogus Helvetica/Courier italic faces (Cocoa inanity!)
    
    // initialize ranges of characters for which system-wide font search should be skipped
    mCodepointsWithNoFonts.SetRange(0,0x1f);     // C0 controls
    mCodepointsWithNoFonts.SetRange(0x7f,0x9f);  // C1 controls
    mCodepointsWithNoFonts.set(0xfffd);          // unknown
                               
    [localizedNames release];
}

PRBool 
gfxQuartzFontCache::ResolveFontName(const nsAString& aFontName, nsAString& aResolvedFontName)
{
    nsAutoString key;
    nsRefPtr<MacOSFamilyEntry> familyEntry;
    GenerateFontListKey(aFontName, key);
    
    if (mFontFamilies.Get(key, &familyEntry) || mLocalizedFamilies.Get(key, &familyEntry)) {
        aResolvedFontName = familyEntry->Name();
        return PR_TRUE;
    }
    return PR_FALSE;
}

void
gfxQuartzFontCache::ATSNotification(ATSFontNotificationInfoRef aInfo,
                                    void* aUserArg)
{
    // xxx - should be carefully pruning the list of fonts, not rebuilding it from scratch
    gfxQuartzFontCache *qfc = (gfxQuartzFontCache*)aUserArg;
    qfc->UpdateFontList();
}

int PR_CALLBACK
gfxQuartzFontCache::PrefChangedCallback(const char *aPrefName, void *closure)
{
    // XXX this could be made to only clear out the cache for the prefs that were changed
    // but it probably isn't that big a deal.
    gfxQuartzFontCache *qfc = static_cast<gfxQuartzFontCache *>(closure);
    qfc->mPrefFonts.Clear();
    return 0;
}

MacOSFontEntry*
gfxQuartzFontCache::GetDefaultFont(const gfxFontStyle* aStyle)
{
    NSString *defaultFamily = [[NSFont userFontOfSize:aStyle->size] familyName];
    nsAutoString familyName;
    
    GetStringForNSString(defaultFamily, familyName);
    return FindFontForFamily(familyName, aStyle);
}

struct FontListData {
    FontListData(const nsACString& aLangGroup,
                 const nsACString& aGenericFamily,
                 nsStringArray& aListOfFonts) :
        mLangGroup(aLangGroup), mGenericFamily(aGenericFamily),
        mListOfFonts(aListOfFonts) {}
    const nsACString& mLangGroup;
    const nsACString& mGenericFamily;
    nsStringArray& mListOfFonts;
};

PLDHashOperator PR_CALLBACK
gfxQuartzFontCache::HashEnumFuncForFamilies(nsStringHashKey::KeyType aKey,
                                            nsRefPtr<MacOSFamilyEntry>& aFamilyEntry,
                                            void* aUserArg)
{
    FontListData *data = (FontListData*)aUserArg;
    data->mListOfFonts.AppendString(aFamilyEntry->Name());
    return PL_DHASH_NEXT;
}

void
gfxQuartzFontCache::GetFontList (const nsACString& aLangGroup,
                                 const nsACString& aGenericFamily,
                                 nsStringArray& aListOfFonts)
{
    FontListData data(aLangGroup, aGenericFamily, aListOfFonts);

    mFontFamilies.Enumerate(gfxQuartzFontCache::HashEnumFuncForFamilies, &data);

    aListOfFonts.Sort();
    aListOfFonts.Compact();
}

MacOSFontEntry*  
gfxQuartzFontCache::FindFontForChar(const PRUint32 aCh, gfxAtsuiFont *aPrevFont)
{
    // is codepoint with no matching font? return null immediately
    if (mCodepointsWithNoFonts.test(aCh)) {
        return nsnull;
    }

    FontSearch data(aCh, aPrevFont);

    // iterate over all font families to find a font that support the character
    mFontFamilies.Enumerate(gfxQuartzFontCache::FindFontForCharProc, &data);

    // no match? add to set of non-matching codepoints
    if (!data.bestMatch) {
        mCodepointsWithNoFonts.set(aCh);
    }
    
    return data.bestMatch;
}

PLDHashOperator PR_CALLBACK 
gfxQuartzFontCache::FindFontForCharProc(nsStringHashKey::KeyType aKey, nsRefPtr<MacOSFamilyEntry>& aFamilyEntry,
     void* userArg)
{
    FontSearch *data = (FontSearch*)userArg;

    // evaluate all fonts in this family for a match
    aFamilyEntry->FindFontForChar(data);
    return PL_DHASH_NEXT;
}

MacOSFamilyEntry* 
gfxQuartzFontCache::FindFamily(const nsAString& aFamily)
{
    nsAutoString key;
    nsRefPtr<MacOSFamilyEntry> familyEntry;
    GenerateFontListKey(aFamily, key);
    
    if (mFontFamilies.Get(key, &familyEntry) || mLocalizedFamilies.Get(key, &familyEntry)) {
        return familyEntry;
    }
    return nsnull;
}
    
MacOSFontEntry*
gfxQuartzFontCache::FindFontForFamily(const nsAString& aFamily, const gfxFontStyle* aStyle)
{
    MacOSFamilyEntry *familyEntry = FindFamily(aFamily);
    
    if (familyEntry)
        return familyEntry->FindFont(aStyle);

    return nsnull;
}

PRInt32 
gfxQuartzFontCache::AppleWeightToCSSWeight(PRInt32 aAppleWeight)
{
    if (aAppleWeight < 1)
        aAppleWeight = 1;
    else if (aAppleWeight > kAppleMaxWeight)
        aAppleWeight = kAppleMaxWeight;
    return gAppleWeightToCSSWeight[aAppleWeight];
}

PRBool
gfxQuartzFontCache::GetPrefFontFamilyEntries(eFontPrefLang aLangGroup, nsTArray<nsRefPtr<MacOSFamilyEntry> > *array)
{
    return mPrefFonts.Get(PRUint32(aLangGroup), array);
}

void
gfxQuartzFontCache::SetPrefFontFamilyEntries(eFontPrefLang aLangGroup, nsTArray<nsRefPtr<MacOSFamilyEntry> >& array)
{
    mPrefFonts.Put(PRUint32(aLangGroup), array);
}

