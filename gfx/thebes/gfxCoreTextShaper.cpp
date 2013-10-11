/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"
#include "gfxCoreTextShaper.h"
#include "gfxMacFont.h"
#include "gfxFontUtils.h"
#include "mozilla/gfx/2D.h"

#include <algorithm>

using namespace mozilla;

// standard font descriptors that we construct the first time they're needed
CTFontDescriptorRef gfxCoreTextShaper::sDefaultFeaturesDescriptor = nullptr;
CTFontDescriptorRef gfxCoreTextShaper::sDisableLigaturesDescriptor = nullptr;

gfxCoreTextShaper::gfxCoreTextShaper(gfxMacFont *aFont)
    : gfxFontShaper(aFont)
{
    // Create our CTFontRef
    mCTFont = ::CTFontCreateWithGraphicsFont(aFont->GetCGFontRef(),
                                             aFont->GetAdjustedSize(),
                                             nullptr,
                                             GetDefaultFeaturesDescriptor());

    // Set up the default attribute dictionary that we will need each time we create a CFAttributedString
    mAttributesDict = ::CFDictionaryCreate(kCFAllocatorDefault,
                                           (const void**) &kCTFontAttributeName,
                                           (const void**) &mCTFont,
                                           1, // count of attributes
                                           &kCFTypeDictionaryKeyCallBacks,
                                           &kCFTypeDictionaryValueCallBacks);
}

gfxCoreTextShaper::~gfxCoreTextShaper()
{
    if (mAttributesDict) {
        ::CFRelease(mAttributesDict);
    }
    if (mCTFont) {
        ::CFRelease(mCTFont);
    }
}

bool
gfxCoreTextShaper::ShapeText(gfxContext      *aContext,
                             const PRUnichar *aText,
                             uint32_t         aOffset,
                             uint32_t         aLength,
                             int32_t          aScript,
                             gfxShapedText   *aShapedText)
{
    // Create a CFAttributedString with text and style info, so we can use CoreText to lay it out.

    bool isRightToLeft = aShapedText->IsRightToLeft();
    uint32_t length = aLength;

    // we need to bidi-wrap the text if the run is RTL,
    // or if it is an LTR run but may contain (overridden) RTL chars
    bool bidiWrap = isRightToLeft;
    if (!bidiWrap && !aShapedText->TextIs8Bit()) {
        uint32_t i;
        for (i = 0; i < length; ++i) {
            if (gfxFontUtils::PotentialRTLChar(aText[i])) {
                bidiWrap = true;
                break;
            }
        }
    }

    // If there's a possibility of any bidi, we wrap the text with direction overrides
    // to ensure neutrals or characters that were bidi-overridden in HTML behave properly.
    const UniChar beginLTR[]    = { 0x202d, 0x20 };
    const UniChar beginRTL[]    = { 0x202e, 0x20 };
    const UniChar endBidiWrap[] = { 0x20, 0x2e, 0x202c };

    uint32_t startOffset;
    CFStringRef stringObj;
    if (bidiWrap) {
        startOffset = isRightToLeft ?
            mozilla::ArrayLength(beginRTL) : mozilla::ArrayLength(beginLTR);
        CFMutableStringRef mutableString =
            ::CFStringCreateMutable(kCFAllocatorDefault,
                                    length + startOffset + mozilla::ArrayLength(endBidiWrap));
        ::CFStringAppendCharacters(mutableString,
                                   isRightToLeft ? beginRTL : beginLTR,
                                   startOffset);
        ::CFStringAppendCharacters(mutableString, reinterpret_cast<const UniChar*>(aText), length);
        ::CFStringAppendCharacters(mutableString,
                                   endBidiWrap, mozilla::ArrayLength(endBidiWrap));
        stringObj = mutableString;
    } else {
        startOffset = 0;
        stringObj = ::CFStringCreateWithCharactersNoCopy(kCFAllocatorDefault,
                                                         reinterpret_cast<const UniChar*>(aText),
                                                         length, kCFAllocatorNull);
    }

    CFDictionaryRef attrObj;
    if (aShapedText->DisableLigatures()) {
        // For letterspacing (or maybe other situations) we need to make a copy of the CTFont
        // with the ligature feature disabled
        CTFontRef ctFont =
            CreateCTFontWithDisabledLigatures(::CTFontGetSize(mCTFont));

        attrObj =
            ::CFDictionaryCreate(kCFAllocatorDefault,
                                 (const void**) &kCTFontAttributeName,
                                 (const void**) &ctFont,
                                 1, // count of attributes
                                 &kCFTypeDictionaryKeyCallBacks,
                                 &kCFTypeDictionaryValueCallBacks);
        // Having created the dict, we're finished with our ligature-disabled CTFontRef
        ::CFRelease(ctFont);
    } else {
        attrObj = mAttributesDict;
        ::CFRetain(attrObj);
    }

    // Now we can create an attributed string
    CFAttributedStringRef attrStringObj =
        ::CFAttributedStringCreate(kCFAllocatorDefault, stringObj, attrObj);
    ::CFRelease(stringObj);
    ::CFRelease(attrObj);

    // Create the CoreText line from our string, then we're done with it
    CTLineRef line = ::CTLineCreateWithAttributedString(attrStringObj);
    ::CFRelease(attrStringObj);

    // and finally retrieve the glyph data and store into the gfxTextRun
    CFArrayRef glyphRuns = ::CTLineGetGlyphRuns(line);
    uint32_t numRuns = ::CFArrayGetCount(glyphRuns);

    // Iterate through the glyph runs.
    // Note that this includes the bidi wrapper, so we have to be careful
    // not to include the extra glyphs from there
    bool success = true;
    for (uint32_t runIndex = 0; runIndex < numRuns; runIndex++) {
        CTRunRef aCTRun =
            (CTRunRef)::CFArrayGetValueAtIndex(glyphRuns, runIndex);
        if (SetGlyphsFromRun(aShapedText, aOffset, aLength, aCTRun, startOffset) != NS_OK) {
            success = false;
            break;
        }
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
                                    CTRunRef       aCTRun,
                                    int32_t        aStringOffset)
{
    // The word has been bidi-wrapped; aStringOffset is the number
    // of chars at the beginning of the CTLine that we should skip.
    // aCTRun is a glyph run from the CoreText layout process.

    int32_t direction = aShapedText->IsRightToLeft() ? -1 : 1;

    int32_t numGlyphs = ::CTRunGetGlyphCount(aCTRun);
    if (numGlyphs == 0) {
        return NS_OK;
    }

    int32_t wordLength = aLength;

    // character offsets get really confusing here, as we have to keep track of
    // (a) the text in the actual textRun we're constructing
    // (c) the string that was handed to CoreText, which contains the text of the font run
    //     plus directional-override padding
    // (d) the CTRun currently being processed, which may be a sub-run of the CoreText line
    //     (but may extend beyond the actual font run into the bidi wrapping text).
    //     aStringOffset tells us how many initial characters of the line to ignore.

    // get the source string range within the CTLine's text
    CFRange stringRange = ::CTRunGetStringRange(aCTRun);
    // skip the run if it is entirely outside the actual range of the font run
    if (stringRange.location - aStringOffset + stringRange.length <= 0 ||
        stringRange.location - aStringOffset >= wordLength) {
        return NS_OK;
    }

    // retrieve the laid-out glyph data from the CTRun
    nsAutoArrayPtr<CGGlyph> glyphsArray;
    nsAutoArrayPtr<CGPoint> positionsArray;
    nsAutoArrayPtr<CFIndex> glyphToCharArray;
    const CGGlyph* glyphs = nullptr;
    const CGPoint* positions = nullptr;
    const CFIndex* glyphToChar = nullptr;

    // Testing indicates that CTRunGetGlyphsPtr (almost?) always succeeds,
    // and so allocating a new array and copying data with CTRunGetGlyphs
    // will be extremely rare.
    // If this were not the case, we could use an nsAutoTArray<> to
    // try and avoid the heap allocation for small runs.
    // It's possible that some future change to CoreText will mean that
    // CTRunGetGlyphsPtr fails more often; if this happens, nsAutoTArray<>
    // may become an attractive option.
    glyphs = ::CTRunGetGlyphsPtr(aCTRun);
    if (!glyphs) {
        glyphsArray = new (std::nothrow) CGGlyph[numGlyphs];
        if (!glyphsArray) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
        ::CTRunGetGlyphs(aCTRun, ::CFRangeMake(0, 0), glyphsArray.get());
        glyphs = glyphsArray.get();
    }

    positions = ::CTRunGetPositionsPtr(aCTRun);
    if (!positions) {
        positionsArray = new (std::nothrow) CGPoint[numGlyphs];
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
        glyphToCharArray = new (std::nothrow) CFIndex[numGlyphs];
        if (!glyphToCharArray) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
        ::CTRunGetStringIndices(aCTRun, ::CFRangeMake(0, 0), glyphToCharArray.get());
        glyphToChar = glyphToCharArray.get();
    }

    double runWidth = ::CTRunGetTypographicBounds(aCTRun, ::CFRangeMake(0, 0),
                                                  nullptr, nullptr, nullptr);

    nsAutoTArray<gfxShapedText::DetailedGlyph,1> detailedGlyphs;
    gfxShapedText::CompressedGlyph g;
    gfxShapedText::CompressedGlyph *charGlyphs =
        aShapedText->GetCharacterGlyphs() + aOffset;

    // CoreText gives us the glyphindex-to-charindex mapping, which relates each glyph
    // to a source text character; we also need the charindex-to-glyphindex mapping to
    // find the glyph for a given char. Note that some chars may not map to any glyph
    // (ligature continuations), and some may map to several glyphs (eg Indic split vowels).
    // We set the glyph index to NO_GLYPH for chars that have no associated glyph, and we
    // record the last glyph index for cases where the char maps to several glyphs,
    // so that our clumping will include all the glyph fragments for the character.

    // The charToGlyph array is indexed by char position within the stringRange of the glyph run.

    static const int32_t NO_GLYPH = -1;
    nsAutoTArray<int32_t,SMALL_GLYPH_RUN> charToGlyphArray;
    if (!charToGlyphArray.SetLength(stringRange.length)) {
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
        NS_WARN_IF_FALSE(charEnd >= 0 && charEnd < stringRange.length,
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

        NS_WARN_IF_FALSE(glyphStart < glyphEnd,
                         "character/glyph clump contains no glyphs!");
        if (glyphStart == glyphEnd) {
            ++glyphStart; // make progress - avoid potential infinite loop
            charStart = charEnd;
            continue;
        }

        NS_WARN_IF_FALSE(charStart != charEnd,
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
            baseCharIndex = charEnd + stringRange.location - aStringOffset + 1;
            endCharIndex = charStart + stringRange.location - aStringOffset + 1;
        } else {
            while (charEnd < stringRange.length && charToGlyph[charEnd] == NO_GLYPH) {
                charEnd++;
            }
            baseCharIndex = charStart + stringRange.location - aStringOffset;
            endCharIndex = charEnd + stringRange.location - aStringOffset;
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
            while (1) {
                gfxTextRun::DetailedGlyph *details = detailedGlyphs.AppendElement();
                details->mGlyphID = glyphs[glyphStart];
                details->mXOffset = 0;
                details->mYOffset = -positions[glyphStart].y * appUnitsPerDevUnit;
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

            gfxTextRun::CompressedGlyph g;
            g.SetComplex(charGlyphs[baseCharIndex].IsClusterStart(),
                         true, detailedGlyphs.Length());
            aShapedText->SetGlyphs(aOffset + baseCharIndex, g, detailedGlyphs.Elements());

            detailedGlyphs.Clear();
        }

        // the rest of the chars in the group are ligature continuations, no associated glyphs
        while (++baseCharIndex != endCharIndex && baseCharIndex < wordLength) {
            gfxShapedText::CompressedGlyph &g = charGlyphs[baseCharIndex];
            NS_ASSERTION(!g.IsSimpleGlyph(), "overwriting a simple glyph");
            g.SetComplex(inOrder && g.IsClusterStart(), false, 0);
        }

        glyphStart = glyphEnd;
        charStart = charEnd;
    }

    return NS_OK;
}

// Construct the font attribute descriptor that we'll apply by default when creating a CTFontRef.
// This will turn off line-edge swashes by default, because we don't know the actual line breaks
// when doing glyph shaping.
void
gfxCoreTextShaper::CreateDefaultFeaturesDescriptor()
{
    if (sDefaultFeaturesDescriptor != nullptr) {
        return;
    }

    SInt16 val = kSmartSwashType;
    CFNumberRef swashesType =
        ::CFNumberCreate(kCFAllocatorDefault,
                         kCFNumberSInt16Type,
                         &val);
    val = kLineInitialSwashesOffSelector;
    CFNumberRef lineInitialsOffSelector =
        ::CFNumberCreate(kCFAllocatorDefault,
                         kCFNumberSInt16Type,
                         &val);

    CFTypeRef keys[]   = { kCTFontFeatureTypeIdentifierKey,
                           kCTFontFeatureSelectorIdentifierKey };
    CFTypeRef values[] = { swashesType,
                           lineInitialsOffSelector };
    CFDictionaryRef featureSettings[2];
    featureSettings[0] =
        ::CFDictionaryCreate(kCFAllocatorDefault,
                             (const void **) keys,
                             (const void **) values,
                             ArrayLength(keys),
                             &kCFTypeDictionaryKeyCallBacks,
                             &kCFTypeDictionaryValueCallBacks);
    ::CFRelease(lineInitialsOffSelector);

    val = kLineFinalSwashesOffSelector;
    CFNumberRef lineFinalsOffSelector =
        ::CFNumberCreate(kCFAllocatorDefault,
                         kCFNumberSInt16Type,
                         &val);
    values[1] = lineFinalsOffSelector;
    featureSettings[1] =
        ::CFDictionaryCreate(kCFAllocatorDefault,
                             (const void **) keys,
                             (const void **) values,
                             ArrayLength(keys),
                             &kCFTypeDictionaryKeyCallBacks,
                             &kCFTypeDictionaryValueCallBacks);
    ::CFRelease(lineFinalsOffSelector);
    ::CFRelease(swashesType);

    CFArrayRef featuresArray =
        ::CFArrayCreate(kCFAllocatorDefault,
                        (const void **) featureSettings,
                        ArrayLength(featureSettings),
                        &kCFTypeArrayCallBacks);
    ::CFRelease(featureSettings[0]);
    ::CFRelease(featureSettings[1]);

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

    sDefaultFeaturesDescriptor =
        ::CTFontDescriptorCreateWithAttributes(attributesDict);
    ::CFRelease(attributesDict);
}

// Create a CTFontRef, with the Common Ligatures feature disabled
CTFontRef
gfxCoreTextShaper::CreateCTFontWithDisabledLigatures(CGFloat aSize)
{
    if (sDisableLigaturesDescriptor == nullptr) {
        // initialize cached descriptor to turn off the Common Ligatures feature
        SInt16 val = kLigaturesType;
        CFNumberRef ligaturesType =
            ::CFNumberCreate(kCFAllocatorDefault,
                             kCFNumberSInt16Type,
                             &val);
        val = kCommonLigaturesOffSelector;
        CFNumberRef commonLigaturesOffSelector =
            ::CFNumberCreate(kCFAllocatorDefault,
                             kCFNumberSInt16Type,
                             &val);

        const CFTypeRef keys[]   = { kCTFontFeatureTypeIdentifierKey,
                                     kCTFontFeatureSelectorIdentifierKey };
        const CFTypeRef values[] = { ligaturesType,
                                     commonLigaturesOffSelector };
        CFDictionaryRef featureSettingDict =
            ::CFDictionaryCreate(kCFAllocatorDefault,
                                 (const void **) keys,
                                 (const void **) values,
                                 ArrayLength(keys),
                                 &kCFTypeDictionaryKeyCallBacks,
                                 &kCFTypeDictionaryValueCallBacks);
        ::CFRelease(ligaturesType);
        ::CFRelease(commonLigaturesOffSelector);

        CFArrayRef featuresArray =
            ::CFArrayCreate(kCFAllocatorDefault,
                            (const void **) &featureSettingDict,
                            1,
                            &kCFTypeArrayCallBacks);
        ::CFRelease(featureSettingDict);

        CFDictionaryRef attributesDict =
            ::CFDictionaryCreate(kCFAllocatorDefault,
                                 (const void **) &kCTFontFeatureSettingsAttribute,
                                 (const void **) &featuresArray,
                                 1, // count of keys & values
                                 &kCFTypeDictionaryKeyCallBacks,
                                 &kCFTypeDictionaryValueCallBacks);
        ::CFRelease(featuresArray);

        sDisableLigaturesDescriptor =
            ::CTFontDescriptorCreateCopyWithAttributes(GetDefaultFeaturesDescriptor(),
                                                       attributesDict);
        ::CFRelease(attributesDict);
    }

    gfxMacFont *f = static_cast<gfxMacFont*>(mFont);
    return ::CTFontCreateWithGraphicsFont(f->GetCGFontRef(), aSize, nullptr,
                                          sDisableLigaturesDescriptor);
}

void
gfxCoreTextShaper::Shutdown() // [static]
{
    if (sDisableLigaturesDescriptor != nullptr) {
        ::CFRelease(sDisableLigaturesDescriptor);
        sDisableLigaturesDescriptor = nullptr;
    }        
    if (sDefaultFeaturesDescriptor != nullptr) {
        ::CFRelease(sDefaultFeaturesDescriptor);
        sDefaultFeaturesDescriptor = nullptr;
    }
}
