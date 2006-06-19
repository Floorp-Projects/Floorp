/* -*- Mode: ObjC; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: BSD
 *
 * Copyright (C) 2006 Mozilla Corporation.  All rights reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

gfxQuartzFontCache *gfxQuartzFontCache::sSharedFontCache = nsnull;

gfxQuartzFontCache::gfxQuartzFontCache() {
}

#define SYNTHESIZED_FONT_TRAITS (NSBoldFontMask | NSItalicFontMask)


#define IMPORTANT_FONT_TRAITS (0 \
    | NSBoldFontMask       \
    | NSCompressedFontMask \
    | NSCondensedFontMask \
    | NSExpandedFontMask \
    | NSItalicFontMask \
    | NSNarrowFontMask \
    | NSPosterFontMask \
    | NSSmallCapsFontMask \
)

#define DESIRED_WEIGHT 5

#define APPLE_BOLD_WEIGHT 9

#define CSS_NORMAL_WEIGHT_BASE 4
#define CSS_BOLD_WEIGHT_BASE 7

#define INDEX_FONT_POSTSCRIPT_NAME 0
#define INDEX_FONT_EXTRA_NAME 1
#define INDEX_FONT_WEIGHT 2
#define INDEX_FONT_TRAITS 3

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

/*
 * Return PR_TRUE if the candidate font is acceptable for
 * the given desired traits.
 */
static PRBool
acceptableChoice (NSFontTraitMask desiredTraits, int desiredWeight,
                  NSFontTraitMask candidateTraits, int candidateWeight)
{
    // remove the traits we can synthesize, and compare the others
    desiredTraits &= ~SYNTHESIZED_FONT_TRAITS;
    return (candidateTraits & desiredTraits) == desiredTraits;
}

/*
 * Return PR_TRUE if the candidate font is better than the chosen font,
 * for the given desired traits
 */
static PRBool
betterChoice(NSFontTraitMask desiredTraits, int desiredWeight,
             NSFontTraitMask chosenTraits, int chosenWeight,
             NSFontTraitMask candidateTraits, int candidateWeight)
{
    if (!acceptableChoice(desiredTraits, desiredWeight, candidateTraits, candidateWeight))
        return PR_FALSE;

    // A list of the traits we care about.
    // The top item in the list is the worst trait to mismatch; if a font has this
    // and we didn't ask for it, we'd prefer any other font in the family.
    const NSFontTraitMask masks[] = {
        NSPosterFontMask,
        NSSmallCapsFontMask,
        NSItalicFontMask,
        NSCompressedFontMask,
        NSCondensedFontMask,
        NSExpandedFontMask,
        NSNarrowFontMask,
        NSBoldFontMask,
        0 };

    int i = 0;
    NSFontTraitMask mask;
    while ((mask = masks[i++])) {
        BOOL desired = (desiredTraits & mask) != 0;
        BOOL chosenHasUnwantedTrait = (desired != ((chosenTraits & mask) != 0));
        BOOL candidateHasUnwantedTrait = (desired != ((candidateTraits & mask) != 0));
        if (!candidateHasUnwantedTrait && chosenHasUnwantedTrait)
            return PR_TRUE;
        if (!chosenHasUnwantedTrait && candidateHasUnwantedTrait)
            return PR_FALSE;
    }

    int chosenWeightDelta = chosenWeight - desiredWeight;
    int candidateWeightDelta = candidateWeight - desiredWeight;

    int chosenWeightDeltaMagnitude = ABS(chosenWeightDelta);
    int candidateWeightDeltaMagnitude = ABS(candidateWeightDelta);

    // Smaller magnitude wins.
    // If both have same magnitude, tie breaker is that the smaller weight wins.
    // Otherwise, first font in the array wins (should almost never happen).
    if (candidateWeightDeltaMagnitude < chosenWeightDeltaMagnitude)
        return PR_TRUE;

    if (candidateWeightDeltaMagnitude == chosenWeightDeltaMagnitude && candidateWeight < chosenWeight)
        return PR_TRUE;

    return PR_FALSE;
}

NSFont*
FindFontWeight (NSFontManager *fontManager, NSFont *font, int desiredWeight, int weightSteps, float size)
{
    NSFont *newFont;

    int currentWeight = [fontManager weightOfFont:font];
    if (currentWeight != desiredWeight) {
        //fprintf (stderr, "desiredWeight: %d currentWeight: %d\n", desiredWeight, currentWeight);
        newFont = [fontManager fontWithFamily:[font familyName] traits:[fontManager traitsOfFont:font] weight:desiredWeight size:size];
        if (newFont) {
            font = newFont;
            currentWeight = [fontManager weightOfFont:font];
            //fprintf (stderr, "picked new font, weight: %d\n", currentWeight);
        }
    }

    if (weightSteps == 0) {
        return font;
    } else if (weightSteps < 0) {
        while (weightSteps != 0) {
            newFont = [fontManager convertWeight:NO ofFont:font];
            if (newFont == font)
                return font;
            font = newFont;
            weightSteps++;
        }
    } else if (weightSteps > 0) {
        while (weightSteps != 0) {
            newFont = [fontManager convertWeight:YES ofFont:font];
            if (newFont == font)
                return font;
            font = newFont;
            weightSteps--;
        }
    }

    return font;
}

ATSUFontID
gfxQuartzFontCache::FindATSUFontIDForFamilyAndStyle (const nsAString& aFamily,
                                                     const gfxFontStyle *aStyle)
{
    NSString *desiredFamily = [NSString stringWithCharacters:aFamily.BeginReading() length:aFamily.Length()];
    NSFontTraitMask desiredTraits = 0;
    int desiredWeight;
    PRInt8 baseCSSWeight, weightOffset;

    //printf ("FindATSUFontIDForFamilyAndStyle: %s\n", [desiredFamily cString]);

    aStyle->ComputeWeightAndOffset(&baseCSSWeight, &weightOffset);
    desiredWeight = CSSWeightToAppleWeight[PR_MIN(PR_MAX(baseCSSWeight, 1), 9)];

    // Oblique should really be synthesized italic; fix that later.
    if (aStyle->style & FONT_STYLE_ITALIC || aStyle->style & FONT_STYLE_OBLIQUE)
        desiredTraits |= NSItalicFontMask;

    // we should really do the right thing with the offsets here, but that's harder.
    if (desiredWeight >= APPLE_BOLD_WEIGHT)
        desiredTraits |= NSBoldFontMask;

    NSFontManager *fontManager = [NSFontManager sharedFontManager];
    NSFont *font = nsnull;

    // Look for an exact match first.
    NSEnumerator *availableFonts = [[fontManager availableFonts] objectEnumerator];
    NSString *availableFont;
    while ((availableFont = [availableFonts nextObject])) {
        if ([desiredFamily caseInsensitiveCompare:availableFont] == NSOrderedSame) {
            NSFont *nameMatchedFont = [NSFont fontWithName:availableFont size:aStyle->size];
            NSFontTraitMask traits = [fontManager traitsOfFont:nameMatchedFont];

            if ((traits & desiredTraits) == desiredTraits) {
                font = [fontManager convertFont:nameMatchedFont toHaveTrait:desiredTraits];
                font = FindFontWeight (fontManager, font, desiredWeight, weightOffset, aStyle->size);
                //fprintf (stderr, "Exact match found; weight: %d\n", [fontManager weightOfFont:font]);
                return [font _atsFontID];
            }

            break;
        }
    }

    // Do a simple case insensitive search for a matching font family.
    // NSFontManager requires exact name matches.
    // This addresses the problem of matching arial to Arial, etc., but perhaps not all the issues.
    NSEnumerator *e = [[fontManager availableFontFamilies] objectEnumerator];
    NSString *availableFamily;
    while ((availableFamily = [e nextObject])) {
        if ([desiredFamily caseInsensitiveCompare:availableFamily] == NSOrderedSame) {
            break;
        }
    }

    // If we can't find a match for this at all, then bail
    if (availableFamily == nsnull)
        return kATSUInvalidFontID;

    // Found a family, now figure out what weight and traits to use.
    BOOL choseFont = false;
    int chosenWeight = 0;
    NSFontTraitMask chosenTraits = 0;

    NSArray *fonts = [fontManager availableMembersOfFontFamily:availableFamily];
    unsigned n = [fonts count];
    unsigned i;
    for (i = 0; i < n; i++) {
        NSArray *fontInfo = [fonts objectAtIndex:i];

        // Array indices must be hard coded because of lame AppKit API.
        int fontWeight = [[fontInfo objectAtIndex:INDEX_FONT_WEIGHT] intValue];
        NSFontTraitMask fontTraits = [[fontInfo objectAtIndex:INDEX_FONT_TRAITS] unsignedIntValue];

        BOOL newWinner;
        if (!choseFont)
            newWinner = acceptableChoice(desiredTraits, desiredWeight, fontTraits, fontWeight);
        else
            newWinner = betterChoice(desiredTraits, desiredWeight, chosenTraits, chosenWeight, fontTraits, fontWeight);

        if (newWinner) {
            choseFont = YES;
            chosenWeight = fontWeight;
            chosenTraits = fontTraits;

            if (chosenWeight == desiredWeight &&
                (chosenTraits & IMPORTANT_FONT_TRAITS) == (desiredTraits & IMPORTANT_FONT_TRAITS))
            {
                // this one is good enough; don't bother looking for something better
                break;
            }
        }
    }

    // we couldn't find anything that was valid in the family
    if (!choseFont)
        return kATSUInvalidFontID;

    // grab the actual font
    font = [fontManager fontWithFamily:availableFamily traits:chosenTraits weight:chosenWeight size:aStyle->size];
    if (!font)
        return kATSUInvalidFontID;

    //fprintf (stderr, "No exact match found; basic match weight: %d\n", [fontManager weightOfFont:font]);
    font = FindFontWeight (fontManager, font, desiredWeight, weightOffset, aStyle->size);
    //fprintf (stderr, "   after FindFontWeight: %d\n", [fontManager weightOfFont:font]);
    chosenWeight = [fontManager weightOfFont:font];

    // Figure out whether we need to synthesize
    NSFontTraitMask actualTraits = 0;
    if (desiredTraits & (NSItalicFontMask | NSBoldFontMask))
        actualTraits = [fontManager traitsOfFont:font];

    bool syntheticBold = (baseCSSWeight >= CSS_BOLD_WEIGHT_BASE && weightOffset <= 0) && (chosenWeight < APPLE_BOLD_WEIGHT);
    bool syntheticOblique = (desiredTraits & NSItalicFontMask) && !(actualTraits & NSItalicFontMask);

    // There are some malformed fonts that will be correctly returned
    // by -fontWithFamily:traits:weight:size: as a match for a
    // particular trait, though -[NSFontManager traitsOfFont:]
    // incorrectly claims the font does not have the specified
    // trait. This could result in applying synthetic bold on top of
    // an already-bold font, as reported in
    // <http://bugzilla.opendarwin.org/show_bug.cgi?id=6146>. To work
    // around this problem, if we got an apparent exact match, but the
    // requested traits aren't present in the matched font, we'll try
    // to get a font from the same family without those traits (to
    // apply the synthetic traits to later).
    NSFontTraitMask nonSyntheticTraits = desiredTraits;

    if (syntheticBold)
        nonSyntheticTraits &= ~NSBoldFontMask;

    if (syntheticOblique)
        nonSyntheticTraits &= ~NSItalicFontMask;

    if (nonSyntheticTraits != desiredTraits) {
        NSFont *fontWithoutSyntheticTraits = [fontManager fontWithFamily:availableFamily traits:nonSyntheticTraits weight:chosenWeight size:aStyle->size];
        if (fontWithoutSyntheticTraits)
            font = fontWithoutSyntheticTraits;
    }

    //printf ("Font: %s -> %d\n", [availableFamily cString], GetNSFontATSUFontId(font));
    return [font _atsFontID];
}

ATSUFontID
gfxQuartzFontCache::GetDefaultATSUFontID (const gfxFontStyle* aStyle)
{
    return [[NSFont userFontOfSize:aStyle->size] _atsFontID];
}
