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
CTFontDescriptorRef gfxCoreTextShaper::sDefaultFeaturesDescriptor = nullptr;
CTFontDescriptorRef gfxCoreTextShaper::sDisableLigaturesDescriptor = nullptr;
CTFontDescriptorRef gfxCoreTextShaper::sIndicFeaturesDescriptor = nullptr;
CTFontDescriptorRef gfxCoreTextShaper::sIndicDisableLigaturesDescriptor = nullptr;

static CFStringRef sCTWritingDirectionAttributeName = nullptr;

// See CTStringAttributes.h
enum {
    kMyCTWritingDirectionEmbedding = (0 << 1),
    kMyCTWritingDirectionOverride = (1 << 1)
};

// Helper to create a CFDictionary with the right attributes for shaping our
// text, including imposing the given directionality.
// This will only be called if we're on 10.8 or later.
CFDictionaryRef
gfxCoreTextShaper::CreateAttrDict(bool aRightToLeft)
{
    // Because we always shape unidirectional runs, and may have applied
    // directional overrides, we want to force a direction rather than
    // allowing CoreText to do its own unicode-based bidi processing.
    SInt16 dirOverride = kMyCTWritingDirectionOverride |
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
    CFTypeRef attrs[] = { kCTFontAttributeName, sCTWritingDirectionAttributeName };
    CFTypeRef values[] = { mCTFont, dirArray };
    CFDictionaryRef attrDict =
        ::CFDictionaryCreate(kCFAllocatorDefault,
                             attrs, values, ArrayLength(attrs),
                             &kCFTypeDictionaryKeyCallBacks,
                             &kCFTypeDictionaryValueCallBacks);
    ::CFRelease(dirArray);
    return attrDict;
}

CFDictionaryRef
gfxCoreTextShaper::CreateAttrDictWithoutDirection()
{
    CFTypeRef attrs[] = { kCTFontAttributeName };
    CFTypeRef values[] = { mCTFont };
    CFDictionaryRef attrDict =
        ::CFDictionaryCreate(kCFAllocatorDefault,
                             attrs, values, ArrayLength(attrs),
                             &kCFTypeDictionaryKeyCallBacks,
                             &kCFTypeDictionaryValueCallBacks);
    return attrDict;
}

gfxCoreTextShaper::gfxCoreTextShaper(gfxMacFont *aFont)
    : gfxFontShaper(aFont)
    , mAttributesDictLTR(nullptr)
    , mAttributesDictRTL(nullptr)
{
    static bool sInitialized = false;
    if (!sInitialized) {
        CFStringRef* pstr = (CFStringRef*)
            dlsym(RTLD_DEFAULT, "kCTWritingDirectionAttributeName");
        if (pstr) {
            sCTWritingDirectionAttributeName = *pstr;
        }
        sInitialized = true;
    }

    // Create our CTFontRef
    mCTFont = CreateCTFontWithFeatures(aFont->GetAdjustedSize(),
                                       GetDefaultFeaturesDescriptor());
}

gfxCoreTextShaper::~gfxCoreTextShaper()
{
    if (mAttributesDictLTR) {
        ::CFRelease(mAttributesDictLTR);
    }
    if (mAttributesDictRTL) {
        ::CFRelease(mAttributesDictRTL);
    }
    if (mCTFont) {
        ::CFRelease(mCTFont);
    }
}

static bool
IsBuggyIndicScript(int32_t aScript)
{
    return aScript == MOZ_SCRIPT_BENGALI || aScript == MOZ_SCRIPT_KANNADA;
}

bool
gfxCoreTextShaper::ShapeText(DrawTarget      *aDrawTarget,
                             const char16_t *aText,
                             uint32_t         aOffset,
                             uint32_t         aLength,
                             int32_t          aScript,
                             bool             aVertical,
                             gfxShapedText   *aShapedText)
{
    // Create a CFAttributedString with text and style info, so we can use CoreText to lay it out.
    bool isRightToLeft = aShapedText->IsRightToLeft();
    const UniChar* text = reinterpret_cast<const UniChar*>(aText);
    uint32_t length = aLength;

    uint32_t startOffset;
    CFStringRef stringObj;
    CFDictionaryRef attrObj;

    if (sCTWritingDirectionAttributeName) {
        startOffset = 0;
        stringObj = ::CFStringCreateWithCharactersNoCopy(kCFAllocatorDefault,
                                                         text, length,
                                                         kCFAllocatorNull);

        // Get an attributes dictionary suitable for shaping text in the
        // current direction, creating it if necessary.
        attrObj = isRightToLeft ? mAttributesDictRTL : mAttributesDictLTR;
        if (!attrObj) {
            attrObj = CreateAttrDict(isRightToLeft);
            (isRightToLeft ? mAttributesDictRTL : mAttributesDictLTR) = attrObj;
        }
    } else {
        // OS is too old to support kCTWritingDirectionAttributeName:
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

        // If there's a possibility of any bidi, we wrap the text with
        // direction overrides to ensure neutrals or characters that were
        // bidi-overridden in HTML behave properly.
        static const UniChar beginLTR[]    = { 0x202d, 0x20 };
        static const UniChar beginRTL[]    = { 0x202e, 0x20 };
        static const UniChar endBidiWrap[] = { 0x20, 0x2e, 0x202c };

        if (bidiWrap) {
            startOffset = isRightToLeft ? ArrayLength(beginRTL)
                                        : ArrayLength(beginLTR);
            CFMutableStringRef mutableString =
                ::CFStringCreateMutable(kCFAllocatorDefault,
                                        length + startOffset +
                                            ArrayLength(endBidiWrap));
            ::CFStringAppendCharacters(mutableString,
                                       isRightToLeft ? beginRTL : beginLTR,
                                       startOffset);
            ::CFStringAppendCharacters(mutableString, text, length);
            ::CFStringAppendCharacters(mutableString, endBidiWrap,
                                       ArrayLength(endBidiWrap));
            stringObj = mutableString;
        } else {
            startOffset = 0;
            stringObj =
                ::CFStringCreateWithCharactersNoCopy(kCFAllocatorDefault,
                                                     text, length,
                                                     kCFAllocatorNull);
        }

        // Get an attributes dictionary suitable for shaping text,
        // creating it if necessary. (This dict is not LTR-specific,
        // but we use that field to store it anyway.)
        if (!mAttributesDictLTR) {
            mAttributesDictLTR = CreateAttrDictWithoutDirection();
        }
        attrObj = mAttributesDictLTR;
    }

    CTFontRef tempCTFont = nullptr;
    if (IsBuggyIndicScript(aScript)) {
        // To work around buggy Indic AAT fonts shipped with OS X,
        // we re-enable the Line Initial Smart Swashes feature that is needed
        // for "split vowels" to work in at least Bengali and Kannada fonts.
        // Affected fonts include Bangla MN, Bangla Sangam MN, Kannada MN,
        // Kannada Sangam MN. See bugs 686225, 728557, 953231, 1145515.
        tempCTFont =
            CreateCTFontWithFeatures(::CTFontGetSize(mCTFont),
                                     aShapedText->DisableLigatures()
                                         ? GetIndicDisableLigaturesDescriptor()
                                         : GetIndicFeaturesDescriptor());
    } else if (aShapedText->DisableLigatures()) {
        // For letterspacing (or maybe other situations) we need to make
        // a copy of the CTFont with the ligature feature disabled.
        tempCTFont =
            CreateCTFontWithFeatures(::CTFontGetSize(mCTFont),
                                     GetDisableLigaturesDescriptor());
    }

    // For the disabled-ligature or buggy-indic-font case, we need to replace
    // the standard CTFont in the attribute dictionary with a tweaked version.
    CFMutableDictionaryRef mutableAttr = nullptr;
    if (tempCTFont) {
        mutableAttr = ::CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 2,
                                                      attrObj);
        ::CFDictionaryReplaceValue(mutableAttr,
                                   kCTFontAttributeName, tempCTFont);
        // Having created the dict, we're finished with our temporary
        // Indic and/or ligature-disabled CTFontRef.
        ::CFRelease(tempCTFont);
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
    // Note that this includes the bidi wrapper, so we have to be careful
    // not to include the extra glyphs from there
    bool success = true;
    for (uint32_t runIndex = 0; runIndex < numRuns; runIndex++) {
        CTRunRef aCTRun =
            (CTRunRef)::CFArrayGetValueAtIndex(glyphRuns, runIndex);
        // If the range is purely within bidi-wrapping text, ignore it.
        CFRange range = ::CTRunGetStringRange(aCTRun);
        if (uint32_t(range.location + range.length) <= startOffset ||
            range.location - startOffset >= aLength) {
            continue;
        }
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
                    char16_t ch = aText[range.location - startOffset];
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
        if (SetGlyphsFromRun(aShapedText, aOffset, aLength, aCTRun,
                             startOffset) != NS_OK) {
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
    UniquePtr<CGGlyph[]> glyphsArray;
    UniquePtr<CGPoint[]> positionsArray;
    UniquePtr<CFIndex[]> glyphToCharArray;
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

    nsAutoTArray<gfxShapedText::DetailedGlyph,1> detailedGlyphs;
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
    AutoFallibleTArray<int32_t,SMALL_GLYPH_RUN> charToGlyphArray;
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

            gfxTextRun::CompressedGlyph textRunGlyph;
            textRunGlyph.SetComplex(charGlyphs[baseCharIndex].IsClusterStart(),
                                    true, detailedGlyphs.Length());
            aShapedText->SetGlyphs(aOffset + baseCharIndex, textRunGlyph,
                                   detailedGlyphs.Elements());

            detailedGlyphs.Clear();
        }

        // the rest of the chars in the group are ligature continuations, no associated glyphs
        while (++baseCharIndex != endCharIndex && baseCharIndex < wordLength) {
            gfxShapedText::CompressedGlyph &shapedTextGlyph = charGlyphs[baseCharIndex];
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

#define MAX_FEATURES  3 // max used by any of our Get*Descriptor functions

CTFontDescriptorRef
gfxCoreTextShaper::CreateFontFeaturesDescriptor(
    const std::pair<SInt16,SInt16> aFeatures[],
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
gfxCoreTextShaper::GetDefaultFeaturesDescriptor()
{
    if (sDefaultFeaturesDescriptor == nullptr) {
        const std::pair<SInt16,SInt16> kDefaultFeatures[] = {
            { kSmartSwashType, kLineInitialSwashesOffSelector },
            { kSmartSwashType, kLineFinalSwashesOffSelector }
        };
        sDefaultFeaturesDescriptor =
            CreateFontFeaturesDescriptor(kDefaultFeatures,
                                         ArrayLength(kDefaultFeatures));
    }
    return sDefaultFeaturesDescriptor;
}

CTFontDescriptorRef
gfxCoreTextShaper::GetDisableLigaturesDescriptor()
{
    if (sDisableLigaturesDescriptor == nullptr) {
        const std::pair<SInt16,SInt16> kDisableLigatures[] = {
            { kSmartSwashType, kLineInitialSwashesOffSelector },
            { kSmartSwashType, kLineFinalSwashesOffSelector },
            { kLigaturesType, kCommonLigaturesOffSelector }
        };
        sDisableLigaturesDescriptor =
            CreateFontFeaturesDescriptor(kDisableLigatures,
                                         ArrayLength(kDisableLigatures));
    }
    return sDisableLigaturesDescriptor;
}

CTFontDescriptorRef
gfxCoreTextShaper::GetIndicFeaturesDescriptor()
{
    if (sIndicFeaturesDescriptor == nullptr) {
        const std::pair<SInt16,SInt16> kIndicFeatures[] = {
            { kSmartSwashType, kLineFinalSwashesOffSelector }
        };
        sIndicFeaturesDescriptor =
            CreateFontFeaturesDescriptor(kIndicFeatures,
                                         ArrayLength(kIndicFeatures));
    }
    return sIndicFeaturesDescriptor;
}

CTFontDescriptorRef
gfxCoreTextShaper::GetIndicDisableLigaturesDescriptor()
{
    if (sIndicDisableLigaturesDescriptor == nullptr) {
        const std::pair<SInt16,SInt16> kIndicDisableLigatures[] = {
            { kSmartSwashType, kLineFinalSwashesOffSelector },
            { kLigaturesType, kCommonLigaturesOffSelector }
        };
        sIndicDisableLigaturesDescriptor =
            CreateFontFeaturesDescriptor(kIndicDisableLigatures,
                                         ArrayLength(kIndicDisableLigatures));
    }
    return sIndicDisableLigaturesDescriptor;
}

CTFontRef
gfxCoreTextShaper::CreateCTFontWithFeatures(CGFloat aSize,
                                            CTFontDescriptorRef aDescriptor)
{
    gfxMacFont *f = static_cast<gfxMacFont*>(mFont);
    return ::CTFontCreateWithGraphicsFont(f->GetCGFontRef(), aSize, nullptr,
                                          aDescriptor);
}

void
gfxCoreTextShaper::Shutdown() // [static]
{
    if (sIndicDisableLigaturesDescriptor != nullptr) {
        ::CFRelease(sIndicDisableLigaturesDescriptor);
        sIndicDisableLigaturesDescriptor = nullptr;
    }
    if (sIndicFeaturesDescriptor != nullptr) {
        ::CFRelease(sIndicFeaturesDescriptor);
        sIndicFeaturesDescriptor = nullptr;
    }
    if (sDisableLigaturesDescriptor != nullptr) {
        ::CFRelease(sDisableLigaturesDescriptor);
        sDisableLigaturesDescriptor = nullptr;
    }
    if (sDefaultFeaturesDescriptor != nullptr) {
        ::CFRelease(sDefaultFeaturesDescriptor);
        sDefaultFeaturesDescriptor = nullptr;
    }
}
