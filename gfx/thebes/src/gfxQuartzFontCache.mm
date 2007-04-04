/* -*- Mode: ObjC; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: BSD
 *
 * Copyright (C) 2006 Mozilla Corporation.  All rights reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
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

// _atsFontID is private; add it in our new category to NSFont
@interface NSFont (MozillaCategory)
- (ATSUFontID)_atsFontID;
@end

#define NON_WEIGHT_TRAITS_MASK (0 \
    | NSCompressedFontMask \
    | NSCondensedFontMask \
    | NSExpandedFontMask \
    | NSItalicFontMask \
    | NSNarrowFontMask \
    | NSPosterFontMask \
    | NSSmallCapsFontMask \
    | NSFixedPitchFontMask \
)

#define IMPORTANT_TRAITS_MASK (NON_WEIGHT_TRAITS_MASK | NSBoldFontMask)

#define SAME_TRAITS(a,b,mask) (((a) & mask) == ((b) & mask))

#define DESIRED_WEIGHT 5

#define APPLE_BOLD_WEIGHT 9

#define CSS_NORMAL_WEIGHT_BASE 4
#define CSS_BOLD_WEIGHT_BASE 7

#define SHOULD_BE_BOLD_IN_THIS_CSS_WEIGHT(w)  ((w) >= 550)
#define SHOULD_BE_LIGHT_IN_THIS_CSS_WEIGHT(w) ((w) < 350)

#define INDEX_FONT_POSTSCRIPT_NAME 0
#define INDEX_FONT_EXTRA_NAME 1
#define INDEX_FONT_WEIGHT 2
#define INDEX_FONT_TRAITS 3

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

/* FontEntry */

NSFont*
FontEntry::GetNSFont(float aSize)
{
    NSString *name = GetNSStringForString(mName);
    return [NSFont fontWithName:name size:aSize];
}

PRBool
FontEntry::IsFixedPitch()
{
    return Traits() & NSFixedPitchFontMask ? PR_TRUE : PR_FALSE;
}

PRBool
FontEntry::IsItalicStyle()
{
    return Traits() & NSItalicFontMask ? PR_TRUE : PR_FALSE;
}

PRBool
FontEntry::IsBold()
{
    return Weight() >= APPLE_BOLD_WEIGHT ? PR_TRUE : PR_FALSE;
}

void
FontEntry::RealizeWeightAndTraits()
{
    NSFont *font = GetNSFont(10.0);
    NSFontManager *fontManager = [NSFontManager sharedFontManager];
    mWeight = [fontManager weightOfFont:font];
    mTraits = [fontManager traitsOfFont:font];
}

void
FontEntry::GetStringForNSString(const NSString *aSrc, nsAString& aDist)
{
    ::GetStringForNSString(aSrc, aDist);
}

NSString*
FontEntry::GetNSStringForString(const nsAString& aSrc)
{
    return ::GetNSStringForString(aSrc);
}

/* gfxQuartzFontCache */

gfxQuartzFontCache *gfxQuartzFontCache::sSharedFontCache = nsnull;

gfxQuartzFontCache::gfxQuartzFontCache()
{
    mCache.Init();
    mFamilies.Init(30);
    mAppleFamilyNames.Init(60);
    mAllFamilyNames.Init(60);
    mPostscriptFonts.Init(60);
    mFontIDTable.Init(60);
    mAllFontNames.Init(100);
    InitFontList();
    ::ATSFontNotificationSubscribe(ATSNotification,
                                   kATSFontNotifyOptionDefault,
                                   (void*)this, nsnull);
}

void
gfxQuartzFontCache::GenerateFontListKey(const nsAString& aKeyName,
                                        nsAString& aResult)
{
    aResult = aKeyName;
    ToLowerCase(aResult);
}

static TextEncoding sUTF8Encoding =
    ::CreateTextEncoding(kTextEncodingUnicodeDefault,
                         kTextEncodingDefaultVariant,
                         kUnicodeUTF8Format);

PRBool
gfxQuartzFontCache::AppendFontFamily(NSFontManager *aFontManager,
                                     NSString *aName,
                                     PRBool aNameIsPostscriptName)
{
    nsAutoString displayName, key;
    if (aNameIsPostscriptName) {
        // we should use display name that is localized the family name and
        // extra name. (e.g., "mono")
        NSFont *font = [NSFont fontWithName:aName size:10.0];
        GetStringForNSString([font displayName], displayName);
    } else {
        // we should use localized simple family name.
        NSString *name = [aFontManager localizedNameForFamily:aName face:nil];
        GetStringForNSString(name, displayName);
    }

    GenerateFontListKey(displayName, key);
    nsRefPtr<FamilyEntry> fe;
    if (mFamilies.Get(key, &fe))
        return PR_FALSE;
    fe = new FamilyEntry(displayName);
    mFamilies.Put(key, fe);

    return PR_TRUE;
}

static PRBool GetFontNameFromBuffer(const char *aBuf,
                                    ByteCount aLength,
                                    FontPlatformCode aPlatformCode,
                                    FontScriptCode aScriptCode,
                                    FontLanguageCode aLangCode,
                                    TECObjectRef &aConverter,
                                    TextEncoding &aEncodingOfConverter,
                                    nsAString &aResult)
{
    if (aPlatformCode == kFontMacintoshPlatform) {
        TextEncoding encoding;
        OSStatus err = ::GetTextEncodingFromScriptInfo(aScriptCode, aLangCode,
                                                       kTextRegionDontCare,
                                                       &encoding);
        if (err != noErr)
            return PR_FALSE;
        if (!aConverter || aEncodingOfConverter != encoding) {
            if (aConverter)
                ::TECDisposeConverter(aConverter);
            err = ::TECCreateConverter(&aConverter, encoding, sUTF8Encoding);
            if (err != noErr)
                return PR_FALSE;
            aEncodingOfConverter = encoding;
        }
        const ByteCount kBufLength = 1024;
        char name[kBufLength];
        ByteCount actualInputLength, actualOutputLength;
        err = ::TECConvertText(aConverter,
                               (UInt8*)aBuf, aLength, &actualInputLength,
                               (UInt8*)name, kBufLength, &actualOutputLength);
        if (err != noErr)
            return PR_FALSE;
        name[actualOutputLength] = '\0';
        aResult = NS_ConvertUTF8toUTF16(name);
    } else if (aPlatformCode == kFontUnicodePlatform ||
               aPlatformCode == kFontMicrosoftPlatform) {
        aResult.Assign((PRUnichar*)aBuf, aLength / sizeof(PRUnichar));
    } else {
        return PR_FALSE;
    }
    return !aResult.IsEmpty();
}

void
gfxQuartzFontCache::InitFontList()
{
    mCache.Clear();
    mFamilies.Clear();
    mAllFamilyNames.Clear();
    mPostscriptFonts.Clear();
    mFontIDTable.Clear();
    mAllFontNames.Clear();
    mNonExistingFonts.Clear();

    nsAutoString key;

    // We should use cocoa for listing up the font families.
    NSFontManager *fontManager = [NSFontManager sharedFontManager];
    NSArray *families = [fontManager availableFontFamilies];
    PRUint32 familiesCount = [families count];
    for (PRUint32 i = 0; i < familiesCount; i++) {
        NSString *name = [families objectAtIndex:i];
        if (!AppendFontFamily(fontManager, name, PR_FALSE))
            continue;
        // If this family has both variable pitch font and
        // fixed pitch font, we should append the font to family.
        // Because CSS cannot specify the pitch in the same family. 
        NSFont *font = [NSFont fontWithName:name size:10.0];
        PRBool isFixedPitch =
            [fontManager traitsOfFont:font] & NSFixedPitchFontMask ? PR_TRUE :
                                                                     PR_FALSE;
        NSArray *members = [fontManager availableMembersOfFontFamily:name];
        PRUint32 membersCount = [members count];
        for (PRUint32 j = 0; j < membersCount; j++) {
            NSArray *member = [members objectAtIndex:j];
            PRUint32 newTraits =
                [[member objectAtIndex:INDEX_FONT_TRAITS] unsignedIntValue];
            if ((isFixedPitch && !(newTraits & NSFixedPitchFontMask)) ||
                (!isFixedPitch && (newTraits & NSFixedPitchFontMask))) {
                AppendFontFamily(fontManager,
                    [member objectAtIndex:INDEX_FONT_POSTSCRIPT_NAME], PR_TRUE);
                break;
            }
        }
    }

    // We should cache for all alias for all families for resloving the given
    // font name to postscript name. But if we cache all font names here,
    // the start up time is slowly, so we should only cache the minimum list.
    ItemCount fontCount;
    OSStatus err = ::ATSUFontCount(&fontCount);
    if (err != noErr)
        return;
    ATSUFontID fontIDs[fontCount];
    ItemCount arraySize = fontCount;
    err = ::ATSUGetFontIDs(fontIDs, arraySize, &fontCount);
    if (err != noErr)
        return;

    TextEncoding encoding = 0;
    TECObjectRef converter = nsnull;

    // In principle, we should get actual length of font name,
    // but we can skip it by the enough buffer length.
    ByteCount len;
    ItemCount index;
    const ByteCount kBufLength = 1024;
    char buf[kBufLength];
    nsAutoString basicFamilyName, postscriptName, name;

    for (ItemCount i = 0; i < fontCount; i++) {
        // Get postscript name, first.
        err = ATSUFindFontName(fontIDs[i], kFontPostscriptName,
                               kFontNoPlatformCode, kFontNoScriptCode,
                               kFontNoLanguageCode, kBufLength,
                               buf, &len, &index);
        if (err != noErr || len == 0)
            continue;

        FontNameCode nameCode;
        FontPlatformCode platformCode;
        FontScriptCode scriptCode;
        FontLanguageCode langCode;
        err = ::ATSUGetIndFontName(fontIDs[i], index, kBufLength, buf, &len,
                                   &nameCode, &platformCode,
                                   &scriptCode, &langCode);
        if (err != noErr || len == 0)
            continue;

        postscriptName.Truncate();
        if (!GetFontNameFromBuffer(buf, len, platformCode, scriptCode,
                                   langCode, converter, encoding,
                                   postscriptName))
            continue;

        // We should get family name from mFamilies for checking the first
        // character of the name.
        nsRefPtr<FontEntry> psFont = new FontEntry(postscriptName);
        NSFont *font = psFont->GetNSFont(10.0);
        basicFamilyName.Truncate();
        GetStringForNSString([font familyName], basicFamilyName);
        NS_ASSERTION(!basicFamilyName.IsEmpty(),
                     "fail to get basic family name");

        PRBool ignoreThisFont = PR_FALSE;
        if (basicFamilyName[0] == PRUnichar('.') ||
            basicFamilyName[0] == PRUnichar('%'))
            ignoreThisFont = PR_TRUE;

        // Append postscript to mPostscriptFonts
        GenerateFontListKey(postscriptName, key);
        if (!ignoreThisFont) {
            mPostscriptFonts.Put(key, psFont);
            mFontIDTable.Put(PRUint32(fontIDs[i]), psFont);
        } else {
            mNonExistingFonts.AppendString(key);
        }

        ItemCount nameCount;
        err = ::ATSUCountFontNames(fontIDs[i], &nameCount);
        if (err != noErr || nameCount == 0)
            continue;

        for (ItemCount j = 0; j < nameCount; j++) {
            err = ::ATSUGetIndFontName(fontIDs[i], j, kBufLength, buf, &len,
                                       &nameCode, &platformCode,
                                       &scriptCode, &langCode);
            if (err != noErr || len == 0)
                continue;

            if (nameCode != kFontFamilyName && nameCode != kFontFullName)
                continue;

            // XXX the mac platform font names cannot be found by
            // ATSUFindFontFromName with UTF16 string. So, we MUST cache the
            // names here, but we can get the other names later.
            if (!ignoreThisFont && platformCode != kFontMacintoshPlatform)
                continue;

            name.Truncate();
            if (!GetFontNameFromBuffer(buf, len, platformCode, scriptCode,
                                       langCode, converter, encoding, name))
                continue;

            GenerateFontListKey(name, key);
            if (ignoreThisFont) {
                if (mNonExistingFonts.IndexOf(key) >= 0)
                    mNonExistingFonts.AppendString(key);
                continue;
            }
            switch (nameCode) {
                case kFontFullName:
                    mAllFontNames.Put(key, psFont->Name());
                    break;
                case kFontFamilyName:
                    // XXX we cannot resolve to postscript name, here.
                    // Don't use fontName of NSFont that is buggy in some cases.
                    mAppleFamilyNames.Put(key, basicFamilyName);
                    break;
            }
        }
    }
    if (converter)
        ::TECDisposeConverter(converter);
}

void
gfxQuartzFontCache::ATSNotification(ATSFontNotificationInfoRef aInfo,
                                    void* aUserArg)
{
    gfxQuartzFontCache *qfc = (gfxQuartzFontCache*)aUserArg;
    qfc->UpdateFontList();
}

// see docs for NSFontManager-availableMembersOfFontFamily:
static const int CSSWeightToAppleWeight[] = {
    // 0 invalid
    0,
    2,
    3,
    4,
    5,
    6,
    8,
    9,
    10,
    12
};

static const int AppleWeightToCSSWeight[] = {
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

// from the given CSS Weight, how many steps do we need to take in Apple Weights
// to get to the next CSS weight
static const int AppleWeightStepsToNextCSSWeight[] = {
    // 0 is invalid
    0,
    1,
    1,
    1,
    1,
    2,
    1,
    1,
    2,
    2
};

NSFont*
gfxQuartzFontCache::FindFontWeight(NSFontManager *aFontManager,
                                   FontEntry *aOriginalFont,
                                   NSFont *aCurrentFont,
                                   const gfxFontStyle *aStyle)
{
    // Assume that the weight of gfxFontStyle is a relative weight for the
    // specified font, e.g., if a font has three font they are light, normal,
    // and bold, when the light font is specified, we should use normal font
    // for the bold style of light font insted of the bold font.
    PRInt8 baseCSSWeight, weightOffset;
    aStyle->ComputeWeightAndOffset(&baseCSSWeight, &weightOffset);
    PRInt32 fontBaseCSSWeight =
        PR_MIN(PR_MAX(AppleWeightToCSSWeight[aOriginalFont->Weight()], 1), 9);
    PRInt32 desiredCSSWeight =
        PR_MIN(PR_MAX(fontBaseCSSWeight - CSS_NORMAL_WEIGHT_BASE + baseCSSWeight, 1), 9);
    PRInt32 desiredWeight = CSSWeightToAppleWeight[desiredCSSWeight];

    PRInt32 currentWeight = [aFontManager weightOfFont:aCurrentFont];
    if (currentWeight == desiredWeight && weightOffset == 0)
        return aCurrentFont;

    PRUint32 traits = [aFontManager traitsOfFont:aCurrentFont];
    NSFont *newFont = aCurrentFont;
    if (currentWeight != desiredWeight) {
        newFont = [aFontManager fontWithFamily:[aCurrentFont familyName]
                                traits:traits weight:desiredWeight
                                size:aStyle->size];
        if (!SAME_TRAITS(traits, [aFontManager traitsOfFont:newFont],
                         NON_WEIGHT_TRAITS_MASK))
            newFont = aCurrentFont;
    }

    // The last resort. Some fonts cannot be bold by "font-weight: bold;".
    // E.g., "Hiragino Kaku Gothic Pro".
    // Maybe, we can remove this, if cairo supports the bold and italic font
    // dynamic generating.
    if (SHOULD_BE_LIGHT_IN_THIS_CSS_WEIGHT(aStyle->weight) &&
        [aFontManager weightOfFont:newFont] >= aOriginalFont->Weight()) {
        newFont = FindAnotherWeightMemberFont(aFontManager, aOriginalFont,
                                              aCurrentFont, aStyle, PR_FALSE);
    } else if (SHOULD_BE_BOLD_IN_THIS_CSS_WEIGHT(aStyle->weight) &&
              [aFontManager weightOfFont:newFont] <= aOriginalFont->Weight()) {
        newFont = FindAnotherWeightMemberFont(aFontManager, aOriginalFont,
                                              aCurrentFont, aStyle, PR_TRUE);        
    }

    if (weightOffset < 0) {
        while (weightOffset != 0) {
            NSFont *font = [aFontManager convertWeight:NO ofFont:newFont];
            if (newFont == font)
                break;
            if (!SAME_TRAITS(traits, [aFontManager traitsOfFont:font],
                             NON_WEIGHT_TRAITS_MASK))
                break;
            newFont = font;
            weightOffset++;
        }
    } else if (weightOffset > 0) {
        while (weightOffset != 0) {
            NSFont *font = [aFontManager convertWeight:YES ofFont:newFont];
            if (newFont == font)
                break;
            if (!SAME_TRAITS(traits, [aFontManager traitsOfFont:font],
                             NON_WEIGHT_TRAITS_MASK))
                break;
            newFont = font;
            weightOffset--;
        }
    }

    return newFont;
}

NSFont*
gfxQuartzFontCache::FindAnotherWeightMemberFont(NSFontManager *aFontManager,
                                                FontEntry *aOriginalFont,
                                                NSFont *aCurrentFont,
                                                const gfxFontStyle *aStyle,
                                                PRBool aBolder)
{
    NSArray *fonts =
        [aFontManager availableMembersOfFontFamily:[aCurrentFont familyName]];
    PRUint32 count = [fonts count];
    PRInt32 baseWeight = aOriginalFont->Weight();
    PRUint32 baseTraits = [aFontManager traitsOfFont:aCurrentFont];

    PRInt32 newWeight = 0;
    NSString *newFont;
    for (PRUint32 i = 0; i < count; i++) {
        NSArray *member = [fonts objectAtIndex:i];
        PRInt32 weight = [[member objectAtIndex:INDEX_FONT_WEIGHT] intValue];
        if ((aBolder && weight <= baseWeight) || (!aBolder && weight >= baseWeight))
            continue;

        PRUint32 traits =
            [[member objectAtIndex:INDEX_FONT_TRAITS] unsignedIntValue];
        if (!SAME_TRAITS(traits, baseTraits, NON_WEIGHT_TRAITS_MASK))
            continue;

        // we should use lightest weight font at finding the bolder font,
        // otherwise, we should use boldest weight font.
        if (!newWeight ||
            (aBolder && newWeight > weight) ||
            (!aBolder && newWeight < weight)) {
            newFont = [member objectAtIndex:INDEX_FONT_POSTSCRIPT_NAME];
            newWeight = weight;
        }
    }
    if (!newWeight)
        return aCurrentFont;
    return [NSFont fontWithName:newFont size:aStyle->size];
}

ATSUFontID
gfxQuartzFontCache::FindATSUFontIDForFamilyAndStyle (const nsAString& aFamily,
                                                     const gfxFontStyle *aStyle)
{
    FontAndFamilyContainer key(aFamily, *aStyle);
    ATSUFontID fid;

    if (mCache.Get(key, &fid))
        return fid;

    // Prevent this from getting too big.  This is an arbitrary number,
    // but it's not worth doing a more complex eviction policy for this.
    if (mCache.Count() > 5000)
        mCache.Clear();

    fid = FindFromSystem(aFamily, aStyle);
    mCache.Put(key, fid);

    return fid;
}

ATSUFontID
gfxQuartzFontCache::FindFromSystem (const nsAString& aFamily,
                                    const gfxFontStyle *aStyle)
{
    nsAutoString key, fontName;
    nsRefPtr<FontEntry> fe;
    GenerateFontListKey(aFamily, key);
    if (!mPostscriptFonts.Get(key, &fe)) {
        if (!ResolveFontName(aFamily, fontName))
            return kATSUInvalidFontID;
        GenerateFontListKey(fontName, key);
        if (!mPostscriptFonts.Get(key, &fe))
            return kATSUInvalidFontID;
    }
    NSFont *font = fe->GetNSFont(aStyle->size);

    PRUint32 desiredTraits = 0;
    // Oblique should really be synthesized italic; fix that later.
    if (fe->IsItalicStyle() || aStyle->style & FONT_STYLE_ITALIC ||
                               aStyle->style & FONT_STYLE_OBLIQUE)
        desiredTraits |= NSItalicFontMask;

    if (fe->IsFixedPitch())
        desiredTraits |= NSFixedPitchFontMask;

    NSFontManager *fontManager = [NSFontManager sharedFontManager];

    NSFont *newFont = font;
    while (desiredTraits) {
        newFont = [fontManager convertFont:font toHaveTrait:desiredTraits];

        PRInt32 newTraits = [fontManager traitsOfFont:newFont];
        if (newTraits & desiredTraits == desiredTraits)
            break;

        newFont = font;

        if (desiredTraits & NSItalicFontMask)
            desiredTraits &= ~NSItalicFontMask;
        else
            break;
    }
    newFont = FindFontWeight(fontManager, fe, newFont, aStyle);
    return [newFont _atsFontID];
}

ATSUFontID
gfxQuartzFontCache::GetDefaultATSUFontID (const gfxFontStyle* aStyle)
{
    return [[NSFont userFontOfSize:aStyle->size] _atsFontID];
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
                                            nsRefPtr<FamilyEntry>& aFamilyEntry,
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

    mFamilies.Enumerate(gfxQuartzFontCache::HashEnumFuncForFamilies, &data);

    aListOfFonts.Sort();
    aListOfFonts.Compact();
}

PRBool
gfxQuartzFontCache::ResolveFontName(const nsAString& aFontName,
                                    nsAString& aResolvedFontName)
{
    nsAutoString name, key;
    nsRefPtr<FontEntry> fe;
    GenerateFontListKey(aFontName, key);
    if (mNonExistingFonts.IndexOf(key) >= 0)
        return PR_FALSE;
    if (mPostscriptFonts.Get(key, &fe)) {
        aResolvedFontName = fe->Name();
        return PR_TRUE;
    }

    // try to find from font names.
    if (mAllFontNames.Get(key, &name)) {
        aResolvedFontName = name;
        return PR_TRUE;
    }
    ATSUFontID fontID;
    OSStatus err;
    err = ATSUFindFontFromName(aFontName.BeginReading(),
                               aFontName.Length() * sizeof(PRUnichar),
                               kFontFullName,
                               kFontNoPlatformCode,
                               kFontNoScriptCode,
                               kFontNoLanguageCode,
                               &fontID);
    if (err == noErr && fontID != kATSUInvalidFontID) {
        if (!mFontIDTable.Get(PRUint32(fontID), &fe)) {
            mNonExistingFonts.AppendString(key);
            return PR_FALSE;
        }
        mAllFontNames.Put(key, fe->Name());
        aResolvedFontName = fe->Name();
        return PR_TRUE;
    }

    // try to find from family names.
    if (mAllFamilyNames.Get(key, &name)) {
        aResolvedFontName = name;
        return PR_TRUE;
    }
    // try to find from apple family names.
    if (mAppleFamilyNames.Get(key, &name)) {
        NSString *familyName = GetNSStringForString(name);        
        NSFont *font = [NSFont fontWithName:familyName size:10.0];
        // XXX Don't use fontName of NSFont. It is buggy in some cases.
        if (mFontIDTable.Get(PRUint32([font _atsFontID]), &fe)) {
            mAllFamilyNames.Put(key, fe->Name());
            aResolvedFontName = fe->Name();
            return PR_TRUE;
        }
        NS_ERROR("There is a font family, but cannot find the actual font");
        mNonExistingFonts.AppendString(key);
        return PR_FALSE;
    }
    err = ATSUFindFontFromName(aFontName.BeginReading(),
                               aFontName.Length() * sizeof(PRUnichar),
                               kFontFamilyName,
                               kFontNoPlatformCode,
                               kFontNoScriptCode,
                               kFontNoLanguageCode,
                               &fontID);
    if (err != noErr || fontID == kATSUInvalidFontID ||
        !mFontIDTable.Get(PRUint32(fontID), &fe)) {
        mNonExistingFonts.AppendString(key);
        return PR_FALSE;
    }
    mAllFamilyNames.Put(key, fe->Name());
    aResolvedFontName = fe->Name();
    return PR_TRUE;
}

const nsString&
gfxQuartzFontCache::GetPostscriptNameForFontID(ATSUFontID fid)
{
    nsRefPtr<FontEntry> fe;

    if (!mFontIDTable.Get(PRUint32(fid), &fe)) {
        NS_WARNING("Invalid font");
        return NS_LITERAL_STRING("INVALID_FONT");
    }

    return fe->Name();
}
