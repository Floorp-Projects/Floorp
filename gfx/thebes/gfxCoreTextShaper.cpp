/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "gfxCoreTextShaper.h"
#include "gfxMacFont.h"
#include "gfxFontUtils.h"
#include "gfxTextRun.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/UniquePtrExtensions.h"

#include <algorithm>

#include <dlfcn.h>

using namespace mozilla;

// standard font descriptors that we construct the first time they're needed
CTFontDescriptorRef gfxCoreTextShaper::sFeaturesDescriptor[kMaxFontInstances];

// Helper to create a CFDictionary with the right attributes for shaping our
// text, including imposing the given directionality.
CFDictionaryRef
gfxCoreTextShaper::CreateAttrDict(bool aRightToLeft)
{
    // Because we always shape unidirectional runs, and may have applied
    // directional overrides, we want to force a direction rather than
    // allowing CoreText to do its own unicode-based bidi processing.
    SInt16 dirOverride = kCTWritingDirectionOverride |
                         (aRightToLeft ? kCTWritingDirectionRightToLeft
                                       : kCTWritingDirectionLeftToRight);
    CFNumberRef dirNumber =
        ::CFNumberCreate(kCFAllocatorDefault,
                         kCFNumberSInt16Type, &dirOverride);
    CFArrayRef dirArray =
        ::CFArrayCreate(kCFAllocatorDefault,
                        (const void **) &dirNumber, 1,
                        &kCFTypeArrayCallBacks);
    ::CFRelease(dirNumber);
    CFTypeRef attrs[] = { kCTFontAttributeName, kCTWritingDirectionAttributeName };
    CFTypeRef values[] = { mCTFont[0], dirArray };
    CFDictionaryRef attrDict =
        ::CFDictionaryCreate(kCFAllocatorDefault,
                             attrs, values, ArrayLength(attrs),
                             &kCFTypeDictionaryKeyCallBacks,
                             &kCFTypeDictionaryValueCallBacks);
    ::CFRelease(dirArray);
    return attrDict;
}

gfxCoreTextShaper::gfxCoreTextShaper(gfxMacFont *aFont)
    : gfxFontShaper(aFont)
    , mAttributesDictLTR(nullptr)
    , mAttributesDictRTL(nullptr)
{
    for (size_t i = 0; i < kMaxFontInstances; i++) {
        mCTFont[i] = nullptr;
    }
    // Create our default CTFontRef
    mCTFont[0] =
        CreateCTFontWithFeatures(aFont->GetAdjustedSize(),
                                 GetFeaturesDescriptor(kDefaultFeatures));
}

gfxCoreTextShaper::~gfxCoreTextShaper()
{
    if (mAttributesDictLTR) {
        ::CFRelease(mAttributesDictLTR);
    }
    if (mAttributesDictRTL) {
        ::CFRelease(mAttributesDictRTL);
    }
    for (size_t i = 0; i < kMaxFontInstances; i++) {
        if (mCTFont[i]) {
            ::CFRelease(mCTFont[i]);
        }
    }
}

static bool
IsBuggyIndicScript(unicode::Script aScript)
{
    return aScript == unicode::Script::BENGALI ||
           aScript == unicode::Script::KANNADA ||
           aScript == unicode::Script::ORIYA ||
           aScript == unicode::Script::KHMER;
}

bool
gfxCoreTextShaper::ShapeText(DrawTarget      *aDrawTarget,
                             const char16_t *aText,
                             uint32_t         aOffset,
                             uint32_t         aLength,
                             Script           aScript,
                             bool             aVertical,
                             RoundingFlags    aRounding,
                             gfxShapedText   *aShapedText)
{
    // Create a CFAttributedString with text and style info, so we can use CoreText to lay it out.
    bool isRightToLeft = aShapedText->IsRightToLeft();
    const UniChar* text = reinterpret_cast<const UniChar*>(aText);

    CFStringRef stringObj =
        ::CFStringCreateWithCharactersNoCopy(kCFAllocatorDefault,
                                             text, aLength,
                                             kCFAllocatorNull);

    // Figure out whether we should try to set the AAT small-caps feature:
    // examine OpenType tags for the requested style, and see if 'smcp' is
    // among them.
    const gfxFontStyle *style = mFont->GetStyle();
    gfxFontEntry *entry = mFont->GetFontEntry();
    auto handleFeatureTag = [](const uint32_t& aTag, uint32_t& aValue,
                               void *aUserArg) -> void {
        if (aTag == HB_TAG('s','m','c','p') && aValue) {
            *static_cast<bool*>(aUserArg) = true;
        }
    };
    bool addSmallCaps = false;
    MergeFontFeatures(style,
                      entry->mFeatureSettings,
                      false,
                      entry->FamilyName(),
                      false,
                      handleFeatureTag,
                      &addSmallCaps);

    // Get an attributes dictionary suitable for shaping text in the
    // current direction, creating it if necessary.
    CFDictionaryRef attrObj =
        isRightToLeft ? mAttributesDictRTL : mAttributesDictLTR;
    if (!attrObj) {
        attrObj = CreateAttrDict(isRightToLeft);
        (isRightToLeft ? mAttributesDictRTL : mAttributesDictLTR) = attrObj;
    }

    FeatureFlags featureFlags = kDefaultFeatures;
    if (IsBuggyIndicScript(aScript)) {
        // To work around buggy Indic AAT fonts shipped with OS X,
        // we re-enable the Line Initial Smart Swashes feature that is needed
        // for "split vowels" to work in at least Bengali and Kannada fonts.
        // Affected fonts include Bangla MN, Bangla Sangam MN, Kannada MN,
        // Kannada Sangam MN. See bugs 686225, 728557, 953231, 1145515.
        // Also applies to Oriya and Khmer, see bug 1370927 and bug 1403166.
        featureFlags |= kIndicFeatures;
    }
    if (aShapedText->DisableLigatures()) {
        // For letterspacing (or maybe other situations) we need to make
        // a copy of the CTFont with the ligature feature disabled.
        featureFlags |= kDisableLigatures;
    }
    if (addSmallCaps) {
        featureFlags |= kAddSmallCaps;
    }

    // For the disabled-ligature, buggy-indic-font or small-caps case, replace
    // the default CTFont in the attribute dictionary with a tweaked version.
    CFMutableDictionaryRef mutableAttr = nullptr;
    if (featureFlags != 0) {
        if (!mCTFont[featureFlags]) {
            mCTFont[featureFlags] =
                CreateCTFontWithFeatures(mFont->GetAdjustedSize(),
                                         GetFeaturesDescriptor(featureFlags));
        }
        mutableAttr =
            ::CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 2, attrObj);
        ::CFDictionaryReplaceValue(mutableAttr,
                                   kCTFontAttributeName, mCTFont[featureFlags]);
        attrObj = mutableAttr;
    }

    // Now we can create an attributed string
    CFAttributedStringRef attrStringObj =
        ::CFAttributedStringCreate(kCFAllocatorDefault, stringObj, attrObj);
    ::CFRelease(stringObj);

    // Create the CoreText line from our string, then we're done with it
    CTLineRef line = ::CTLineCreateWithAttributedString(attrStringObj);
    ::CFRelease(attrStringObj);

    // and finally retrieve the glyph data and store into the gfxTextRun
    CFArrayRef glyphRuns = ::CTLineGetGlyphRuns(line);
    uint32_t numRuns = ::CFArrayGetCount(glyphRuns);

    // Iterate through the glyph runs.
    bool success = true;
    for (uint32_t runIndex = 0; runIndex < numRuns; runIndex++) {
        CTRunRef aCTRun =
            (CTRunRef)::CFArrayGetValueAtIndex(glyphRuns, runIndex);
        CFRange range = ::CTRunGetStringRange(aCTRun);
        CFDictionaryRef runAttr = ::CTRunGetAttributes(aCTRun);
        if (runAttr != attrObj) {
            // If Core Text manufactured a new dictionary, this may indicate
            // unexpected font substitution. In that case, we fail (and fall
            // back to harfbuzz shaping)...
            const void* font1 =
                ::CFDictionaryGetValue(attrObj, kCTFontAttributeName);
            const void* font2 =
                ::CFDictionaryGetValue(runAttr, kCTFontAttributeName);
            if (font1 != font2) {
                // ...except that if the fallback was only for a variation
                // selector or join control that is otherwise unsupported,
                // we just ignore it.
                if (range.length == 1) {
                    char16_t ch = aText[range.location];
                    if (gfxFontUtils::IsJoinControl(ch) ||
                        gfxFontUtils::IsVarSelector(ch)) {
                        continue;
                    }
                }
                NS_WARNING("unexpected font fallback in Core Text");
                success = false;
                break;
            }
        }
        if (SetGlyphsFromRun(aShapedText, aOffset, aLength, aCTRun) != NS_OK) {
            success = false;
            break;
        }
    }

    if (mutableAttr) {
        ::CFRelease(mutableAttr);
    }
    ::CFRelease(line);

    return success;
}

#define SMALL_GLYPH_RUN 128 // preallocated size of our auto arrays for per-glyph data;
                            // some testing indicates that 90%+ of glyph runs will fit
                            // without requiring a separate allocation

nsresult
gfxCoreTextShaper::SetGlyphsFromRun(gfxShapedText *aShapedText,
                                    uint32_t       aOffset,
                                    uint32_t       aLength,
                                    CTRunRef       aCTRun)
{
    typedef gfxShapedText::CompressedGlyph CompressedGlyph;

    int32_t direction = aShapedText->IsRightToLeft() ? -1 : 1;

    int32_t numGlyphs = ::CTRunGetGlyphCount(aCTRun);
    if (numGlyphs == 0) {
        return NS_OK;
    }

    int32_t wordLength = aLength;

    // character offsets get really confusing here, as we have to keep track of
    // (a) the text in the actual textRun we're constructing
    // (c) the string that was handed to CoreText, which contains the text of the font run
    // (d) the CTRun currently being processed, which may be a sub-run of the CoreText line

    // get the source string range within the CTLine's text
    CFRange stringRange = ::CTRunGetStringRange(aCTRun);
    // skip the run if it is entirely outside the actual range of the font run
    if (stringRange.location + stringRange.length <= 0 ||
        stringRange.location >= wordLength) {
        return NS_OK;
    }

    // retrieve the laid-out glyph data from the CTRun
    UniquePtr<CGGlyph[]> glyphsArray;
    UniquePtr<CGPoint[]> positionsArray;
    UniquePtr<CFIndex[]> glyphToCharArray;
    const CGGlyph* glyphs = nullptr;
    const CGPoint* positions = nullptr;
    const CFIndex* glyphToChar = nullptr;

    // Testing indicates that CTRunGetGlyphsPtr (almost?) always succeeds,
    // and so allocating a new array and copying data with CTRunGetGlyphs
    // will be extremely rare.
    // If this were not the case, we could use an AutoTArray<> to
    // try and avoid the heap allocation for small runs.
    // It's possible that some future change to CoreText will mean that
    // CTRunGetGlyphsPtr fails more often; if this happens, AutoTArray<>
    // may become an attractive option.
    glyphs = ::CTRunGetGlyphsPtr(aCTRun);
    if (!glyphs) {
        glyphsArray = MakeUniqueFallible<CGGlyph[]>(numGlyphs);
        if (!glyphsArray) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
        ::CTRunGetGlyphs(aCTRun, ::CFRangeMake(0, 0), glyphsArray.get());
        glyphs = glyphsArray.get();
    }

    positions = ::CTRunGetPositionsPtr(aCTRun);
    if (!positions) {
        positionsArray = MakeUniqueFallible<CGPoint[]>(numGlyphs);
        if (!positionsArray) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
        ::CTRunGetPositions(aCTRun, ::CFRangeMake(0, 0), positionsArray.get());
        positions = positionsArray.get();
    }

    // Remember that the glyphToChar indices relate to the CoreText line,
    // not to the beginning of the textRun, the font run,
    // or the stringRange of the glyph run
    glyphToChar = ::CTRunGetStringIndicesPtr(aCTRun);
    if (!glyphToChar) {
        glyphToCharArray = MakeUniqueFallible<CFIndex[]>(numGlyphs);
        if (!glyphToCharArray) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
        ::CTRunGetStringIndices(aCTRun, ::CFRangeMake(0, 0), glyphToCharArray.get());
        glyphToChar = glyphToCharArray.get();
    }

    double runWidth = ::CTRunGetTypographicBounds(aCTRun, ::CFRangeMake(0, 0),
                                                  nullptr, nullptr, nullptr);

    AutoTArray<gfxShapedText::DetailedGlyph,1> detailedGlyphs;
    CompressedGlyph* charGlyphs = aShapedText->GetCharacterGlyphs() + aOffset;

    // CoreText gives us the glyphindex-to-charindex mapping, which relates each glyph
    // to a source text character; we also need the charindex-to-glyphindex mapping to
    // find the glyph for a given char. Note that some chars may not map to any glyph
    // (ligature continuations), and some may map to several glyphs (eg Indic split vowels).
    // We set the glyph index to NO_GLYPH for chars that have no associated glyph, and we
    // record the last glyph index for cases where the char maps to several glyphs,
    // so that our clumping will include all the glyph fragments for the character.

    // The charToGlyph array is indexed by char position within the stringRange of the glyph run.

    static const int32_t NO_GLYPH = -1;
    AutoTArray<int32_t,SMALL_GLYPH_RUN> charToGlyphArray;
    if (!charToGlyphArray.SetLength(stringRange.length, fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    int32_t *charToGlyph = charToGlyphArray.Elements();
    for (int32_t offset = 0; offset < stringRange.length; ++offset) {
        charToGlyph[offset] = NO_GLYPH;
    }
    for (int32_t i = 0; i < numGlyphs; ++i) {
        int32_t loc = glyphToChar[i] - stringRange.location;
        if (loc >= 0 && loc < stringRange.length) {
            charToGlyph[loc] = i;
        }
    }

    // Find character and glyph clumps that correspond, allowing for ligatures,
    // indic reordering, split glyphs, etc.
    //
    // The idea is that we'll find a character sequence starting at the first char of stringRange,
    // and extend it until it includes the character associated with the first glyph;
    // we also extend it as long as there are "holes" in the range of glyphs. So we
    // will eventually have a contiguous sequence of characters, starting at the beginning
    // of the range, that map to a contiguous sequence of glyphs, starting at the beginning
    // of the glyph array. That's a clump; then we update the starting positions and repeat.
    //
    // NB: In the case of RTL layouts, we iterate over the stringRange in reverse.
    //

    // This may find characters that fall outside the range 0:wordLength,
    // so we won't necessarily use everything we find here.

    bool isRightToLeft = aShapedText->IsRightToLeft();
    int32_t glyphStart = 0; // looking for a clump that starts at this glyph index
    int32_t charStart = isRightToLeft ?
        stringRange.length - 1 : 0; // and this char index (in the stringRange of the glyph run)

    while (glyphStart < numGlyphs) { // keep finding groups until all glyphs are accounted for
        bool inOrder = true;
        int32_t charEnd = glyphToChar[glyphStart] - stringRange.location;
        NS_WARNING_ASSERTION(
            charEnd >= 0 && charEnd < stringRange.length,
            "glyph-to-char mapping points outside string range");
        // clamp charEnd to the valid range of the string
        charEnd = std::max(charEnd, 0);
        charEnd = std::min(charEnd, int32_t(stringRange.length));

        int32_t glyphEnd = glyphStart;
        int32_t charLimit = isRightToLeft ? -1 : stringRange.length;
        do {
            // This is normally executed once for each iteration of the outer loop,
            // but in unusual cases where the character/glyph association is complex,
            // the initial character range might correspond to a non-contiguous
            // glyph range with "holes" in it. If so, we will repeat this loop to
            // extend the character range until we have a contiguous glyph sequence.
            NS_ASSERTION((direction > 0 && charEnd < charLimit) ||
                         (direction < 0 && charEnd > charLimit),
                         "no characters left in range?");
            charEnd += direction;
            while (charEnd != charLimit && charToGlyph[charEnd] == NO_GLYPH) {
                charEnd += direction;
            }

            // find the maximum glyph index covered by the clump so far
            if (isRightToLeft) {
                for (int32_t i = charStart; i > charEnd; --i) {
                    if (charToGlyph[i] != NO_GLYPH) {
                        // update extent of glyph range
                        glyphEnd = std::max(glyphEnd, charToGlyph[i] + 1);
                    }
                }
            } else {
                for (int32_t i = charStart; i < charEnd; ++i) {
                    if (charToGlyph[i] != NO_GLYPH) {
                        // update extent of glyph range
                        glyphEnd = std::max(glyphEnd, charToGlyph[i] + 1);
                    }
                }
            }

            if (glyphEnd == glyphStart + 1) {
                // for the common case of a single-glyph clump, we can skip the following checks
                break;
            }

            if (glyphEnd == glyphStart) {
                // no glyphs, try to extend the clump
                continue;
            }

            // check whether all glyphs in the range are associated with the characters
            // in our clump; if not, we have a discontinuous range, and should extend it
            // unless we've reached the end of the text
            bool allGlyphsAreWithinCluster = true;
            int32_t prevGlyphCharIndex = charStart;
            for (int32_t i = glyphStart; i < glyphEnd; ++i) {
                int32_t glyphCharIndex = glyphToChar[i] - stringRange.location;
                if (isRightToLeft) {
                    if (glyphCharIndex > charStart || glyphCharIndex <= charEnd) {
                        allGlyphsAreWithinCluster = false;
                        break;
                    }
                    if (glyphCharIndex > prevGlyphCharIndex) {
                        inOrder = false;
                    }
                    prevGlyphCharIndex = glyphCharIndex;
                } else {
                    if (glyphCharIndex < charStart || glyphCharIndex >= charEnd) {
                        allGlyphsAreWithinCluster = false;
                        break;
                    }
                    if (glyphCharIndex < prevGlyphCharIndex) {
                        inOrder = false;
                    }
                    prevGlyphCharIndex = glyphCharIndex;
                }
            }
            if (allGlyphsAreWithinCluster) {
                break;
            }
        } while (charEnd != charLimit);

        NS_WARNING_ASSERTION(glyphStart < glyphEnd,
                             "character/glyph clump contains no glyphs!");
        if (glyphStart == glyphEnd) {
            ++glyphStart; // make progress - avoid potential infinite loop
            charStart = charEnd;
            continue;
        }

        NS_WARNING_ASSERTION(charStart != charEnd,
                             "character/glyph clump contains no characters!");
        if (charStart == charEnd) {
            glyphStart = glyphEnd; // this is bad - we'll discard the glyph(s),
                                   // as there's nowhere to attach them
            continue;
        }

        // Now charStart..charEnd is a ligature clump, corresponding to glyphStart..glyphEnd;
        // Set baseCharIndex to the char we'll actually attach the glyphs to (1st of ligature),
        // and endCharIndex to the limit (position beyond the last char),
        // adjusting for the offset of the stringRange relative to the textRun.
        int32_t baseCharIndex, endCharIndex;
        if (isRightToLeft) {
            while (charEnd >= 0 && charToGlyph[charEnd] == NO_GLYPH) {
                charEnd--;
            }
            baseCharIndex = charEnd + stringRange.location + 1;
            endCharIndex = charStart + stringRange.location + 1;
        } else {
            while (charEnd < stringRange.length && charToGlyph[charEnd] == NO_GLYPH) {
                charEnd++;
            }
            baseCharIndex = charStart + stringRange.location;
            endCharIndex = charEnd + stringRange.location;
        }

        // Then we check if the clump falls outside our actual string range; if so, just go to the next.
        if (endCharIndex <= 0 || baseCharIndex >= wordLength) {
            glyphStart = glyphEnd;
            charStart = charEnd;
            continue;
        }
        // Ensure we won't try to go beyond the valid length of the word's text
        baseCharIndex = std::max(baseCharIndex, 0);
        endCharIndex = std::min(endCharIndex, wordLength);

        // Now we're ready to set the glyph info in the textRun; measure the glyph width
        // of the first (perhaps only) glyph, to see if it is "Simple"
        int32_t appUnitsPerDevUnit = aShapedText->GetAppUnitsPerDevUnit();
        double toNextGlyph;
        if (glyphStart < numGlyphs-1) {
            toNextGlyph = positions[glyphStart+1].x - positions[glyphStart].x;
        } else {
            toNextGlyph = positions[0].x + runWidth - positions[glyphStart].x;
        }
        int32_t advance = int32_t(toNextGlyph * appUnitsPerDevUnit);

        // Check if it's a simple one-to-one mapping
        int32_t glyphsInClump = glyphEnd - glyphStart;
        if (glyphsInClump == 1 &&
            gfxTextRun::CompressedGlyph::IsSimpleGlyphID(glyphs[glyphStart]) &&
            gfxTextRun::CompressedGlyph::IsSimpleAdvance(advance) &&
            charGlyphs[baseCharIndex].IsClusterStart() &&
            positions[glyphStart].y == 0.0)
        {
            charGlyphs[baseCharIndex].SetSimpleGlyph(advance,
                                                     glyphs[glyphStart]);
        } else {
            // collect all glyphs in a list to be assigned to the first char;
            // there must be at least one in the clump, and we already measured its advance,
            // hence the placement of the loop-exit test and the measurement of the next glyph
            while (true) {
                gfxTextRun::DetailedGlyph *details = detailedGlyphs.AppendElement();
                details->mGlyphID = glyphs[glyphStart];
                details->mOffset.y = -positions[glyphStart].y * appUnitsPerDevUnit;
                details->mAdvance = advance;
                if (++glyphStart >= glyphEnd) {
                   break;
                }
                if (glyphStart < numGlyphs-1) {
                    toNextGlyph = positions[glyphStart+1].x - positions[glyphStart].x;
                } else {
                    toNextGlyph = positions[0].x + runWidth - positions[glyphStart].x;
                }
                advance = int32_t(toNextGlyph * appUnitsPerDevUnit);
            }

            bool isClusterStart = charGlyphs[baseCharIndex].IsClusterStart();
            aShapedText->SetGlyphs(aOffset + baseCharIndex,
                                   CompressedGlyph::MakeComplex(isClusterStart, true,
                                                                detailedGlyphs.Length()),
                                   detailedGlyphs.Elements());

            detailedGlyphs.Clear();
        }

        // the rest of the chars in the group are ligature continuations, no associated glyphs
        while (++baseCharIndex != endCharIndex && baseCharIndex < wordLength) {
            CompressedGlyph &shapedTextGlyph = charGlyphs[baseCharIndex];
            NS_ASSERTION(!shapedTextGlyph.IsSimpleGlyph(), "overwriting a simple glyph");
            shapedTextGlyph.SetComplex(inOrder && shapedTextGlyph.IsClusterStart(), false, 0);
        }

        glyphStart = glyphEnd;
        charStart = charEnd;
    }

    return NS_OK;
}

#undef SMALL_GLYPH_RUN

// Construct the font attribute descriptor that we'll apply by default when
// creating a CTFontRef. This will turn off line-edge swashes by default,
// because we don't know the actual line breaks when doing glyph shaping.

// We also cache feature descriptors for shaping with disabled ligatures, and
// for buggy Indic AAT font workarounds, created on an as-needed basis.

#define MAX_FEATURES  5 // max used by any of our Get*Descriptor functions

CTFontDescriptorRef
gfxCoreTextShaper::CreateFontFeaturesDescriptor(
    const std::pair<SInt16,SInt16>* aFeatures,
    size_t aCount)
{
    MOZ_ASSERT(aCount <= MAX_FEATURES);

    CFDictionaryRef featureSettings[MAX_FEATURES];

    for (size_t i = 0; i < aCount; i++) {
        CFNumberRef type = ::CFNumberCreate(kCFAllocatorDefault,
                                            kCFNumberSInt16Type,
                                            &aFeatures[i].first);
        CFNumberRef selector = ::CFNumberCreate(kCFAllocatorDefault,
                                                kCFNumberSInt16Type,
                                                &aFeatures[i].second);

        CFTypeRef keys[]   = { kCTFontFeatureTypeIdentifierKey,
                               kCTFontFeatureSelectorIdentifierKey };
        CFTypeRef values[] = { type, selector };
        featureSettings[i] =
            ::CFDictionaryCreate(kCFAllocatorDefault,
                                 (const void **) keys,
                                 (const void **) values,
                                 ArrayLength(keys),
                                 &kCFTypeDictionaryKeyCallBacks,
                                 &kCFTypeDictionaryValueCallBacks);

        ::CFRelease(selector);
        ::CFRelease(type);
    }

    CFArrayRef featuresArray =
        ::CFArrayCreate(kCFAllocatorDefault,
                        (const void **) featureSettings,
                        aCount, // not ArrayLength(featureSettings), as we
                                // may not have used all the allocated slots
                        &kCFTypeArrayCallBacks);

    for (size_t i = 0; i < aCount; i++) {
        ::CFRelease(featureSettings[i]);
    }

    const CFTypeRef attrKeys[]   = { kCTFontFeatureSettingsAttribute };
    const CFTypeRef attrValues[] = { featuresArray };
    CFDictionaryRef attributesDict =
        ::CFDictionaryCreate(kCFAllocatorDefault,
                             (const void **) attrKeys,
                             (const void **) attrValues,
                             ArrayLength(attrKeys),
                             &kCFTypeDictionaryKeyCallBacks,
                             &kCFTypeDictionaryValueCallBacks);
    ::CFRelease(featuresArray);

    CTFontDescriptorRef descriptor =
        ::CTFontDescriptorCreateWithAttributes(attributesDict);
    ::CFRelease(attributesDict);

    return descriptor;
}

CTFontDescriptorRef
gfxCoreTextShaper::GetFeaturesDescriptor(FeatureFlags aFeatureFlags)
{
    MOZ_ASSERT(aFeatureFlags < kMaxFontInstances);
    if (!sFeaturesDescriptor[aFeatureFlags]) {
        typedef std::pair<SInt16,SInt16> FeatT;
        AutoTArray<FeatT,MAX_FEATURES> features;
        features.AppendElement(FeatT(kSmartSwashType,
                                     kLineFinalSwashesOffSelector));
        if ((aFeatureFlags & kIndicFeatures) == 0) {
            features.AppendElement(FeatT(kSmartSwashType,
                                         kLineInitialSwashesOffSelector));
        }
        if (aFeatureFlags & kAddSmallCaps) {
            features.AppendElement(FeatT(kLetterCaseType,
                                         kSmallCapsSelector));
            features.AppendElement(FeatT(kLowerCaseType,
                                         kLowerCaseSmallCapsSelector));
        }
        if (aFeatureFlags & kDisableLigatures) {
            features.AppendElement(FeatT(kLigaturesType,
                                         kCommonLigaturesOffSelector));
        }
        MOZ_ASSERT(features.Length() <= MAX_FEATURES);
        sFeaturesDescriptor[aFeatureFlags] =
            CreateFontFeaturesDescriptor(features.Elements(),
                                         features.Length());
    }
    return sFeaturesDescriptor[aFeatureFlags];
}

CTFontRef
gfxCoreTextShaper::CreateCTFontWithFeatures(CGFloat aSize,
                                            CTFontDescriptorRef aDescriptor)
{
    const gfxFontEntry* fe = mFont->GetFontEntry();
    bool isInstalledFont = !fe->IsUserFont() || fe->IsLocalUserFont();
    CGFontRef cgFont = static_cast<gfxMacFont*>(mFont)->GetCGFontRef();
    return gfxMacFont::CreateCTFontFromCGFontWithVariations(cgFont, aSize,
                                                            isInstalledFont,
                                                            aDescriptor);
}

void
gfxCoreTextShaper::Shutdown() // [static]
{
    for (size_t i = 0; i < kMaxFontInstances; i++) {
        if (sFeaturesDescriptor[i] != nullptr) {
            ::CFRelease(sFeaturesDescriptor[i]);
            sFeaturesDescriptor[i] = nullptr;
        }
    }
}
