/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTextRunTransformations.h"

#include <utility>

#include "GreekCasing.h"
#include "IrishCasing.h"
#include "mozilla/ComputedStyleInlines.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/TextEditor.h"
#include "mozilla/gfx/2D.h"
#include "nsGkAtoms.h"
#include "nsSpecialCasingData.h"
#include "nsStyleConsts.h"
#include "nsTextFrameUtils.h"
#include "nsUnicharUtils.h"
#include "nsUnicodeProperties.h"

using namespace mozilla;
using namespace mozilla::gfx;

// Unicode characters needing special casing treatment in tr/az languages
#define LATIN_CAPITAL_LETTER_I_WITH_DOT_ABOVE 0x0130
#define LATIN_SMALL_LETTER_DOTLESS_I 0x0131

// Greek sigma needs custom handling for the lowercase transform; for details
// see comments under "case NS_STYLE_TEXT_TRANSFORM_LOWERCASE" within
// nsCaseTransformTextRunFactory::RebuildTextRun(), and bug 740120.
#define GREEK_CAPITAL_LETTER_SIGMA 0x03A3
#define GREEK_SMALL_LETTER_FINAL_SIGMA 0x03C2
#define GREEK_SMALL_LETTER_SIGMA 0x03C3

already_AddRefed<nsTransformedTextRun> nsTransformedTextRun::Create(
    const gfxTextRunFactory::Parameters* aParams,
    nsTransformingTextRunFactory* aFactory, gfxFontGroup* aFontGroup,
    const char16_t* aString, uint32_t aLength,
    const gfx::ShapedTextFlags aFlags, const nsTextFrameUtils::Flags aFlags2,
    nsTArray<RefPtr<nsTransformedCharStyle>>&& aStyles, bool aOwnsFactory) {
  NS_ASSERTION(!(aFlags & gfx::ShapedTextFlags::TEXT_IS_8BIT),
               "didn't expect text to be marked as 8-bit here");

  void* storage =
      AllocateStorageForTextRun(sizeof(nsTransformedTextRun), aLength);
  if (!storage) {
    return nullptr;
  }

  RefPtr<nsTransformedTextRun> result = new (storage)
      nsTransformedTextRun(aParams, aFactory, aFontGroup, aString, aLength,
                           aFlags, aFlags2, std::move(aStyles), aOwnsFactory);
  return result.forget();
}

void nsTransformedTextRun::SetCapitalization(uint32_t aStart, uint32_t aLength,
                                             bool* aCapitalization) {
  if (mCapitalize.IsEmpty()) {
    // XXX(Bug 1631371) Check if this should use a fallible operation as it
    // pretended earlier.
    mCapitalize.AppendElements(GetLength());
    memset(mCapitalize.Elements(), 0, GetLength() * sizeof(bool));
  }
  memcpy(mCapitalize.Elements() + aStart, aCapitalization,
         aLength * sizeof(bool));
  mNeedsRebuild = true;
}

bool nsTransformedTextRun::SetPotentialLineBreaks(Range aRange,
                                                  const uint8_t* aBreakBefore) {
  bool changed = gfxTextRun::SetPotentialLineBreaks(aRange, aBreakBefore);
  if (changed) {
    mNeedsRebuild = true;
  }
  return changed;
}

size_t nsTransformedTextRun::SizeOfExcludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) {
  size_t total = gfxTextRun::SizeOfExcludingThis(aMallocSizeOf);
  total += mStyles.ShallowSizeOfExcludingThis(aMallocSizeOf);
  total += mCapitalize.ShallowSizeOfExcludingThis(aMallocSizeOf);
  if (mOwnsFactory) {
    total += aMallocSizeOf(mFactory);
  }
  return total;
}

size_t nsTransformedTextRun::SizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

already_AddRefed<nsTransformedTextRun>
nsTransformingTextRunFactory::MakeTextRun(
    const char16_t* aString, uint32_t aLength,
    const gfxTextRunFactory::Parameters* aParams, gfxFontGroup* aFontGroup,
    gfx::ShapedTextFlags aFlags, nsTextFrameUtils::Flags aFlags2,
    nsTArray<RefPtr<nsTransformedCharStyle>>&& aStyles, bool aOwnsFactory) {
  return nsTransformedTextRun::Create(aParams, this, aFontGroup, aString,
                                      aLength, aFlags, aFlags2,
                                      std::move(aStyles), aOwnsFactory);
}

already_AddRefed<nsTransformedTextRun>
nsTransformingTextRunFactory::MakeTextRun(
    const uint8_t* aString, uint32_t aLength,
    const gfxTextRunFactory::Parameters* aParams, gfxFontGroup* aFontGroup,
    gfx::ShapedTextFlags aFlags, nsTextFrameUtils::Flags aFlags2,
    nsTArray<RefPtr<nsTransformedCharStyle>>&& aStyles, bool aOwnsFactory) {
  // We'll only have a Unicode code path to minimize the amount of code needed
  // for these rarely used features
  NS_ConvertASCIItoUTF16 unicodeString(reinterpret_cast<const char*>(aString),
                                       aLength);
  return MakeTextRun(unicodeString.get(), aLength, aParams, aFontGroup,
                     aFlags & ~gfx::ShapedTextFlags::TEXT_IS_8BIT, aFlags2,
                     std::move(aStyles), aOwnsFactory);
}

void MergeCharactersInTextRun(gfxTextRun* aDest, gfxTextRun* aSrc,
                              const bool* aCharsToMerge,
                              const bool* aDeletedChars) {
  MOZ_ASSERT(!aDest->TrailingGlyphRun(), "unexpected glyphRuns in aDest!");
  gfxTextRun::GlyphRunIterator iter(aSrc, gfxTextRun::Range(aSrc));
  uint32_t offset = 0;
  AutoTArray<gfxTextRun::DetailedGlyph, 2> glyphs;
  const gfxTextRun::CompressedGlyph continuationGlyph =
      gfxTextRun::CompressedGlyph::MakeComplex(false, false);
  const gfxTextRun::CompressedGlyph* srcGlyphs = aSrc->GetCharacterGlyphs();
  gfxTextRun::CompressedGlyph* destGlyphs = aDest->GetCharacterGlyphs();
  while (iter.NextRun()) {
    const gfxTextRun::GlyphRun* run = iter.GetGlyphRun();
    aDest->AddGlyphRun(run->mFont, run->mMatchType, offset, false,
                       run->mOrientation, run->mIsCJK);

    bool anyMissing = false;
    uint32_t mergeRunStart = iter.GetStringStart();
    // Initialize to a copy of the first source glyph in the merge run.
    gfxTextRun::CompressedGlyph mergedGlyph = srcGlyphs[mergeRunStart];
    uint32_t stringEnd = iter.GetStringEnd();
    for (uint32_t k = iter.GetStringStart(); k < stringEnd; ++k) {
      const gfxTextRun::CompressedGlyph g = srcGlyphs[k];
      if (g.IsSimpleGlyph()) {
        if (!anyMissing) {
          gfxTextRun::DetailedGlyph details;
          details.mGlyphID = g.GetSimpleGlyph();
          details.mAdvance = g.GetSimpleAdvance();
          glyphs.AppendElement(details);
        }
      } else {
        if (g.IsMissing()) {
          anyMissing = true;
          glyphs.Clear();
        }
        if (g.GetGlyphCount() > 0) {
          glyphs.AppendElements(aSrc->GetDetailedGlyphs(k), g.GetGlyphCount());
        }
      }

      if (k + 1 < iter.GetStringEnd() && aCharsToMerge[k + 1]) {
        // next char is supposed to merge with current, so loop without
        // writing current merged glyph to the destination
        continue;
      }

      // If the start of the merge run is actually a character that should
      // have been merged with the previous character (this can happen
      // if there's a font change in the middle of a case-mapped character,
      // that decomposed into a sequence of base+diacritics, for example),
      // just discard the entire merge run. See comment at start of this
      // function.
      NS_WARNING_ASSERTION(
          !aCharsToMerge[mergeRunStart],
          "unable to merge across a glyph run boundary, glyph(s) discarded");
      if (!aCharsToMerge[mergeRunStart]) {
        // Determine if we can just copy the existing simple glyph record.
        if (mergedGlyph.IsSimpleGlyph() && glyphs.Length() == 1) {
          destGlyphs[offset] = mergedGlyph;
        } else {
          // Otherwise set up complex glyph record and store detailed glyphs.
          mergedGlyph.SetComplex(mergedGlyph.IsClusterStart(),
                                 mergedGlyph.IsLigatureGroupStart());
          destGlyphs[offset] = mergedGlyph;
          aDest->SetDetailedGlyphs(offset, glyphs.Length(), glyphs.Elements());
          if (anyMissing) {
            destGlyphs[offset].SetMissing();
          }
        }
        offset++;

        while (offset < aDest->GetLength() && aDeletedChars[offset]) {
          destGlyphs[offset++] = continuationGlyph;
        }
      }

      glyphs.Clear();
      anyMissing = false;
      mergeRunStart = k + 1;
      if (mergeRunStart < stringEnd) {
        mergedGlyph = srcGlyphs[mergeRunStart];
      }
    }
    NS_ASSERTION(glyphs.Length() == 0,
                 "Leftover glyphs, don't request merging of the last character "
                 "with its next!");
  }
  NS_ASSERTION(offset == aDest->GetLength(), "Bad offset calculations");
}

gfxTextRunFactory::Parameters GetParametersForInner(
    nsTransformedTextRun* aTextRun, gfx::ShapedTextFlags* aFlags,
    DrawTarget* aRefDrawTarget) {
  gfxTextRunFactory::Parameters params = {
      aRefDrawTarget, nullptr, nullptr,
      nullptr,        0,       aTextRun->GetAppUnitsPerDevUnit()};
  *aFlags = aTextRun->GetFlags();
  return params;
}

// Some languages have special casing conventions that differ from the
// default Unicode mappings.
// The enum values here are named for well-known exemplar languages that
// exhibit the behavior in question; multiple lang tags may map to the
// same setting here, if the behavior is shared by other languages.
enum LanguageSpecificCasingBehavior {
  eLSCB_None,       // default non-lang-specific behavior
  eLSCB_Dutch,      // treat "ij" digraph as a unit for capitalization
  eLSCB_Greek,      // strip accent when uppercasing Greek vowels
  eLSCB_Irish,      // keep prefix letters as lowercase when uppercasing Irish
  eLSCB_Turkish,    // preserve dotted/dotless-i distinction in uppercase
  eLSCB_Lithuanian  // retain dot on lowercase i/j when an accent is present
};

static LanguageSpecificCasingBehavior GetCasingFor(const nsAtom* aLang) {
  if (!aLang) {
    return eLSCB_None;
  }
  if (aLang == nsGkAtoms::tr || aLang == nsGkAtoms::az ||
      aLang == nsGkAtoms::ba || aLang == nsGkAtoms::crh ||
      aLang == nsGkAtoms::tt) {
    return eLSCB_Turkish;
  }
  if (aLang == nsGkAtoms::nl) {
    return eLSCB_Dutch;
  }
  if (aLang == nsGkAtoms::el) {
    return eLSCB_Greek;
  }
  if (aLang == nsGkAtoms::ga) {
    return eLSCB_Irish;
  }
  if (aLang == nsGkAtoms::lt_) {
    return eLSCB_Lithuanian;
  }

  // Is there a region subtag we should ignore?
  nsAtomString langStr(const_cast<nsAtom*>(aLang));
  int index = langStr.FindChar('-');
  if (index > 0) {
    langStr.Truncate(index);
    RefPtr<nsAtom> truncatedLang = NS_Atomize(langStr);
    return GetCasingFor(truncatedLang);
  }

  return eLSCB_None;
}

bool nsCaseTransformTextRunFactory::TransformString(
    const nsAString& aString, nsString& aConvertedString, bool aAllUppercase,
    bool aCaseTransformsOnly, const nsAtom* aLanguage,
    nsTArray<bool>& aCharsToMergeArray, nsTArray<bool>& aDeletedCharsArray,
    const nsTransformedTextRun* aTextRun, uint32_t aOffsetInTextRun,
    nsTArray<uint8_t>* aCanBreakBeforeArray,
    nsTArray<RefPtr<nsTransformedCharStyle>>* aStyleArray) {
  bool auxiliaryOutputArrays = aCanBreakBeforeArray && aStyleArray;
  MOZ_ASSERT(!auxiliaryOutputArrays || aTextRun,
             "text run must be provided to use aux output arrays");

  uint32_t length = aString.Length();
  const char16_t* str = aString.BeginReading();
  const char16_t kPasswordMask = TextEditor::PasswordMask();

  bool mergeNeeded = false;

  bool capitalizeDutchIJ = false;
  bool prevIsLetter = false;
  bool ntPrefix = false;  // true immediately after a word-initial 'n' or 't'
                          // when doing Irish lowercasing
  bool seenSoftDotted = false;  // true immediately after an I or J that is
                                // converted to lowercase in Lithuanian mode
  uint32_t sigmaIndex = uint32_t(-1);
  nsUGenCategory cat;

  StyleTextTransform style =
      aAllUppercase ? StyleTextTransform{StyleTextTransformCase::Uppercase,
                                         StyleTextTransformOther()}
                    : StyleTextTransform::None();
  bool forceNonFullWidth = false;
  const nsAtom* lang = aLanguage;

  LanguageSpecificCasingBehavior languageSpecificCasing = GetCasingFor(lang);
  mozilla::GreekCasing::State greekState;
  mozilla::IrishCasing::State irishState;
  uint32_t irishMark = uint32_t(-1);  // location of possible prefix letter(s)
                                      // in the output string
  uint32_t irishMarkSrc = uint32_t(-1);  // corresponding location in source
                                         // string (may differ from output due
                                         // to expansions like eszet -> 'SS')
  uint32_t greekMark = uint32_t(-1);  // location of uppercase ETA that may need
                                      // tonos added (if it is disjunctive eta)
  const char16_t kGreekUpperEta = 0x0397;

  for (uint32_t i = 0; i < length; ++i, ++aOffsetInTextRun) {
    uint32_t ch = str[i];

    RefPtr<nsTransformedCharStyle> charStyle;
    if (aTextRun) {
      charStyle = aTextRun->mStyles[aOffsetInTextRun];
      style = aAllUppercase
                  ? StyleTextTransform{StyleTextTransformCase::Uppercase,
                                       StyleTextTransformOther()}
                  : charStyle->mTextTransform;
      forceNonFullWidth = charStyle->mForceNonFullWidth;

      nsAtom* newLang =
          charStyle->mExplicitLanguage ? charStyle->mLanguage.get() : nullptr;
      if (lang != newLang) {
        lang = newLang;
        languageSpecificCasing = GetCasingFor(lang);
        greekState.Reset();
        irishState.Reset();
        irishMark = uint32_t(-1);
        irishMarkSrc = uint32_t(-1);
        greekMark = uint32_t(-1);
      }
    }

    bool maskPassword = charStyle && charStyle->mMaskPassword;
    int extraChars = 0;
    const mozilla::unicode::MultiCharMapping* mcm;
    bool inhibitBreakBefore = false;  // have we just deleted preceding hyphen?

    if (i < length - 1 && NS_IS_SURROGATE_PAIR(ch, str[i + 1])) {
      ch = SURROGATE_TO_UCS4(ch, str[i + 1]);
    }

    // Skip case transform if we're masking current character.
    if (!maskPassword) {
      switch (style.case_) {
        case StyleTextTransformCase::None:
          break;

        case StyleTextTransformCase::Lowercase:
          if (languageSpecificCasing == eLSCB_Turkish) {
            if (ch == 'I') {
              ch = LATIN_SMALL_LETTER_DOTLESS_I;
              prevIsLetter = true;
              sigmaIndex = uint32_t(-1);
              break;
            }
            if (ch == LATIN_CAPITAL_LETTER_I_WITH_DOT_ABOVE) {
              ch = 'i';
              prevIsLetter = true;
              sigmaIndex = uint32_t(-1);
              break;
            }
          }

          if (languageSpecificCasing == eLSCB_Lithuanian) {
            // clang-format off
            /* From SpecialCasing.txt:
             * # Introduce an explicit dot above when lowercasing capital I's and J's
             * # whenever there are more accents above.
             * # (of the accents used in Lithuanian: grave, acute, tilde above, and ogonek)
             *
             * 0049; 0069 0307; 0049; 0049; lt More_Above; # LATIN CAPITAL LETTER I
             * 004A; 006A 0307; 004A; 004A; lt More_Above; # LATIN CAPITAL LETTER J
             * 012E; 012F 0307; 012E; 012E; lt More_Above; # LATIN CAPITAL LETTER I WITH OGONEK
             * 00CC; 0069 0307 0300; 00CC; 00CC; lt; # LATIN CAPITAL LETTER I WITH GRAVE
             * 00CD; 0069 0307 0301; 00CD; 00CD; lt; # LATIN CAPITAL LETTER I WITH ACUTE
             * 0128; 0069 0307 0303; 0128; 0128; lt; # LATIN CAPITAL LETTER I WITH TILDE
             */
            // clang-format on
            if (ch == 'I' || ch == 'J' || ch == 0x012E) {
              ch = ToLowerCase(ch);
              prevIsLetter = true;
              seenSoftDotted = true;
              sigmaIndex = uint32_t(-1);
              break;
            }
            if (ch == 0x00CC) {
              aConvertedString.Append('i');
              aConvertedString.Append(0x0307);
              extraChars += 2;
              ch = 0x0300;
              prevIsLetter = true;
              seenSoftDotted = false;
              sigmaIndex = uint32_t(-1);
              break;
            }
            if (ch == 0x00CD) {
              aConvertedString.Append('i');
              aConvertedString.Append(0x0307);
              extraChars += 2;
              ch = 0x0301;
              prevIsLetter = true;
              seenSoftDotted = false;
              sigmaIndex = uint32_t(-1);
              break;
            }
            if (ch == 0x0128) {
              aConvertedString.Append('i');
              aConvertedString.Append(0x0307);
              extraChars += 2;
              ch = 0x0303;
              prevIsLetter = true;
              seenSoftDotted = false;
              sigmaIndex = uint32_t(-1);
              break;
            }
          }

          cat = mozilla::unicode::GetGenCategory(ch);

          if (languageSpecificCasing == eLSCB_Irish &&
              cat == nsUGenCategory::kLetter) {
            // See bug 1018805 for Irish lowercasing requirements
            if (!prevIsLetter && (ch == 'n' || ch == 't')) {
              ntPrefix = true;
            } else {
              if (ntPrefix && mozilla::IrishCasing::IsUpperVowel(ch)) {
                aConvertedString.Append('-');
                ++extraChars;
              }
              ntPrefix = false;
            }
          } else {
            ntPrefix = false;
          }

          if (seenSoftDotted && cat == nsUGenCategory::kMark) {
            // The seenSoftDotted flag will only be set in Lithuanian mode.
            if (ch == 0x0300 || ch == 0x0301 || ch == 0x0303) {
              aConvertedString.Append(0x0307);
              ++extraChars;
            }
          }
          seenSoftDotted = false;

          // Special lowercasing behavior for Greek Sigma: note that this is
          // listed as context-sensitive in Unicode's SpecialCasing.txt, but is
          // *not* a language-specific mapping; it applies regardless of the
          // language of the element.
          //
          // The lowercase mapping for CAPITAL SIGMA should be to SMALL SIGMA
          // (i.e. the non-final form) whenever there is a following letter, or
          // when the CAPITAL SIGMA occurs in isolation (neither preceded nor
          // followed by a LETTER); and to FINAL SIGMA when it is preceded by
          // another letter but not followed by one.
          //
          // To implement the context-sensitive nature of this mapping, we keep
          // track of whether the previous character was a letter. If not,
          // CAPITAL SIGMA will map directly to SMALL SIGMA. If the previous
          // character was a letter, CAPITAL SIGMA maps to FINAL SIGMA and we
          // record the position in the converted string; if we then encounter
          // another letter, that FINAL SIGMA is replaced with a standard
          // SMALL SIGMA.

          // If sigmaIndex is not -1, it marks where we have provisionally
          // mapped a CAPITAL SIGMA to FINAL SIGMA; if we now find another
          // letter, we need to change it to SMALL SIGMA.
          if (sigmaIndex != uint32_t(-1)) {
            if (cat == nsUGenCategory::kLetter) {
              aConvertedString.SetCharAt(GREEK_SMALL_LETTER_SIGMA, sigmaIndex);
            }
          }

          if (ch == GREEK_CAPITAL_LETTER_SIGMA) {
            // If preceding char was a letter, map to FINAL instead of SMALL,
            // and note where it occurred by setting sigmaIndex; we'll change
            // it to standard SMALL SIGMA later if another letter follows
            if (prevIsLetter) {
              ch = GREEK_SMALL_LETTER_FINAL_SIGMA;
              sigmaIndex = aConvertedString.Length();
            } else {
              // CAPITAL SIGMA not preceded by a letter is unconditionally
              // mapped to SMALL SIGMA
              ch = GREEK_SMALL_LETTER_SIGMA;
              sigmaIndex = uint32_t(-1);
            }
            prevIsLetter = true;
            break;
          }

          // ignore diacritics for the purpose of contextual sigma mapping;
          // otherwise, reset prevIsLetter appropriately and clear the
          // sigmaIndex marker
          if (cat != nsUGenCategory::kMark) {
            prevIsLetter = (cat == nsUGenCategory::kLetter);
            sigmaIndex = uint32_t(-1);
          }

          mcm = mozilla::unicode::SpecialLower(ch);
          if (mcm) {
            int j = 0;
            while (j < 2 && mcm->mMappedChars[j + 1]) {
              aConvertedString.Append(mcm->mMappedChars[j]);
              ++extraChars;
              ++j;
            }
            ch = mcm->mMappedChars[j];
            break;
          }

          ch = ToLowerCase(ch);
          break;

        case StyleTextTransformCase::Uppercase:
          if (languageSpecificCasing == eLSCB_Turkish && ch == 'i') {
            ch = LATIN_CAPITAL_LETTER_I_WITH_DOT_ABOVE;
            break;
          }

          if (languageSpecificCasing == eLSCB_Greek) {
            bool markEta;
            bool updateEta;
            ch = mozilla::GreekCasing::UpperCase(ch, greekState, markEta,
                                                 updateEta);
            if (markEta) {
              greekMark = aConvertedString.Length();
            } else if (updateEta) {
              // Remove the TONOS from an uppercase ETA-TONOS that turned out
              // not to be disjunctive-eta.
              MOZ_ASSERT(aConvertedString.Length() > 0 &&
                             greekMark < aConvertedString.Length(),
                         "bad greekMark!");
              aConvertedString.SetCharAt(kGreekUpperEta, greekMark);
              greekMark = uint32_t(-1);
            }
            break;
          }

          if (languageSpecificCasing == eLSCB_Lithuanian) {
            /*
             * # Remove DOT ABOVE after "i" with upper or titlecase
             *
             * 0307; 0307; ; ; lt After_Soft_Dotted; # COMBINING DOT ABOVE
             */
            if (ch == 'i' || ch == 'j' || ch == 0x012F) {
              seenSoftDotted = true;
              ch = ToTitleCase(ch);
              break;
            }
            if (seenSoftDotted) {
              seenSoftDotted = false;
              if (ch == 0x0307) {
                ch = uint32_t(-1);
                break;
              }
            }
          }

          if (languageSpecificCasing == eLSCB_Irish) {
            bool mark;
            uint8_t action;
            ch = mozilla::IrishCasing::UpperCase(ch, irishState, mark, action);
            if (mark) {
              irishMark = aConvertedString.Length();
              irishMarkSrc = i;
              break;
            } else if (action) {
              nsString& str = aConvertedString;  // shorthand
              switch (action) {
                case 1:
                  // lowercase a single prefix letter
                  NS_ASSERTION(str.Length() > 0 && irishMark < str.Length(),
                               "bad irishMark!");
                  str.SetCharAt(ToLowerCase(str[irishMark]), irishMark);
                  irishMark = uint32_t(-1);
                  irishMarkSrc = uint32_t(-1);
                  break;
                case 2:
                  // lowercase two prefix letters (immediately before current
                  // pos)
                  NS_ASSERTION(
                      str.Length() >= 2 && irishMark == str.Length() - 2,
                      "bad irishMark!");
                  str.SetCharAt(ToLowerCase(str[irishMark]), irishMark);
                  str.SetCharAt(ToLowerCase(str[irishMark + 1]), irishMark + 1);
                  irishMark = uint32_t(-1);
                  irishMarkSrc = uint32_t(-1);
                  break;
                case 3:
                  // lowercase one prefix letter, and delete following hyphen
                  // (which must be the immediately-preceding char)
                  NS_ASSERTION(
                      str.Length() >= 2 && irishMark == str.Length() - 2,
                      "bad irishMark!");
                  MOZ_ASSERT(
                      irishMark != uint32_t(-1) && irishMarkSrc != uint32_t(-1),
                      "failed to set irishMarks");
                  str.Replace(irishMark, 2, ToLowerCase(str[irishMark]));
                  aDeletedCharsArray[irishMarkSrc + 1] = true;
                  // Remove the trailing entries (corresponding to the deleted
                  // hyphen) from the auxiliary arrays.
                  aCharsToMergeArray.SetLength(aCharsToMergeArray.Length() - 1);
                  if (auxiliaryOutputArrays) {
                    aStyleArray->SetLength(aStyleArray->Length() - 1);
                    aCanBreakBeforeArray->SetLength(
                        aCanBreakBeforeArray->Length() - 1);
                    inhibitBreakBefore = true;
                  }
                  mergeNeeded = true;
                  irishMark = uint32_t(-1);
                  irishMarkSrc = uint32_t(-1);
                  break;
              }
              // ch has been set to the uppercase for current char;
              // No need to check for SpecialUpper here as none of the
              // characters that could trigger an Irish casing action have
              // special mappings.
              break;
            }
            // If we didn't have any special action to perform, fall through
            // to check for special uppercase (ß)
          }

          mcm = mozilla::unicode::SpecialUpper(ch);
          if (mcm) {
            int j = 0;
            while (j < 2 && mcm->mMappedChars[j + 1]) {
              aConvertedString.Append(mcm->mMappedChars[j]);
              ++extraChars;
              ++j;
            }
            ch = mcm->mMappedChars[j];
            break;
          }

          // Bug 1476304: we exclude Georgian letters U+10D0..10FF because of
          // lack of widespread font support for the corresponding Mtavruli
          // characters at this time (July 2018).
          // This condition is to be removed once the major platforms ship with
          // fonts that support U+1C90..1CBF.
          if (ch < 0x10D0 || ch > 0x10FF) {
            ch = ToUpperCase(ch);
          }
          break;

        case StyleTextTransformCase::Capitalize:
          if (aTextRun) {
            if (capitalizeDutchIJ && ch == 'j') {
              ch = 'J';
              capitalizeDutchIJ = false;
              break;
            }
            capitalizeDutchIJ = false;
            if (aOffsetInTextRun < aTextRun->mCapitalize.Length() &&
                aTextRun->mCapitalize[aOffsetInTextRun]) {
              if (languageSpecificCasing == eLSCB_Turkish && ch == 'i') {
                ch = LATIN_CAPITAL_LETTER_I_WITH_DOT_ABOVE;
                break;
              }
              if (languageSpecificCasing == eLSCB_Dutch && ch == 'i') {
                ch = 'I';
                capitalizeDutchIJ = true;
                break;
              }
              if (languageSpecificCasing == eLSCB_Lithuanian) {
                /*
                 * # Remove DOT ABOVE after "i" with upper or titlecase
                 *
                 * 0307; 0307; ; ; lt After_Soft_Dotted; # COMBINING DOT ABOVE
                 */
                if (ch == 'i' || ch == 'j' || ch == 0x012F) {
                  seenSoftDotted = true;
                  ch = ToTitleCase(ch);
                  break;
                }
                if (seenSoftDotted) {
                  seenSoftDotted = false;
                  if (ch == 0x0307) {
                    ch = uint32_t(-1);
                    break;
                  }
                }
              }

              mcm = mozilla::unicode::SpecialTitle(ch);
              if (mcm) {
                int j = 0;
                while (j < 2 && mcm->mMappedChars[j + 1]) {
                  aConvertedString.Append(mcm->mMappedChars[j]);
                  ++extraChars;
                  ++j;
                }
                ch = mcm->mMappedChars[j];
                break;
              }

              ch = ToTitleCase(ch);
            }
          }
          break;

        default:
          MOZ_ASSERT_UNREACHABLE("all cases should be handled");
          break;
      }

      if (!aCaseTransformsOnly) {
        if (!forceNonFullWidth &&
            (style.other_ & StyleTextTransformOther::FULL_WIDTH)) {
          ch = mozilla::unicode::GetFullWidth(ch);
        }

        if (style.other_ & StyleTextTransformOther::FULL_SIZE_KANA) {
          // clang-format off
          static const uint16_t kSmallKanas[] = {
              // ぁ   ぃ      ぅ      ぇ      ぉ      っ      ゃ      ゅ      ょ
              0x3041, 0x3043, 0x3045, 0x3047, 0x3049, 0x3063, 0x3083, 0x3085, 0x3087,
              // ゎ   ゕ      ゖ
              0x308E, 0x3095, 0x3096,
              // ァ   ィ      ゥ      ェ      ォ      ッ      ャ      ュ      ョ
              0x30A1, 0x30A3, 0x30A5, 0x30A7, 0x30A9, 0x30C3, 0x30E3, 0x30E5, 0x30E7,
              // ヮ   ヵ      ヶ      ㇰ      ㇱ      ㇲ      ㇳ      ㇴ      ㇵ
              0x30EE, 0x30F5, 0x30F6, 0x31F0, 0x31F1, 0x31F2, 0x31F3, 0x31F4, 0x31F5,
              // ㇶ   ㇷ      ㇸ      ㇹ      ㇺ      ㇻ      ㇼ      ㇽ      ㇾ
              0x31F6, 0x31F7, 0x31F8, 0x31F9, 0x31FA, 0x31FB, 0x31FC, 0x31FD, 0x31FE,
              // ㇿ
              0x31FF,
              // ｧ    ｨ       ｩ       ｪ       ｫ       ｬ       ｭ       ｮ       ｯ
              0xFF67, 0xFF68, 0xFF69, 0xFF6A, 0xFF6B, 0xFF6C, 0xFF6D, 0xFF6E, 0xFF6F};
          static const uint16_t kFullSizeKanas[] = {
              // あ   い      う      え      お      つ      や      ゆ      よ
              0x3042, 0x3044, 0x3046, 0x3048, 0x304A, 0x3064, 0x3084, 0x3086, 0x3088,
              // わ   か      け
              0x308F, 0x304B, 0x3051,
              // ア   イ      ウ      エ      オ      ツ      ヤ      ユ      ヨ
              0x30A2, 0x30A4, 0x30A6, 0x30A8, 0x30AA, 0x30C4, 0x30E4, 0x30E6, 0x30E8,
              // ワ   カ      ケ      ク      シ      ス      ト      ヌ      ハ
              0x30EF, 0x30AB, 0x30B1, 0x30AF, 0x30B7, 0x30B9, 0x30C8, 0x30CC, 0x30CF,
              // ヒ   フ      ヘ      ホ      ム      ラ      リ      ル      レ
              0x30D2, 0x30D5, 0x30D8, 0x30DB, 0x30E0, 0x30E9, 0x30EA, 0x30EB, 0x30EC,
              // ロ
              0x30ED,
              // ｱ    ｲ       ｳ       ｴ       ｵ       ﾔ       ﾕ       ﾖ        ﾂ
              0xFF71, 0xFF72, 0xFF73, 0xFF74, 0xFF75, 0xFF94, 0xFF95, 0xFF96, 0xFF82};
          // clang-format on

          size_t index;
          const uint16_t len = MOZ_ARRAY_LENGTH(kSmallKanas);
          if (mozilla::BinarySearch(kSmallKanas, 0, len, ch, &index)) {
            ch = kFullSizeKanas[index];
          }
        }
      }

      if (forceNonFullWidth) {
        ch = mozilla::unicode::GetFullWidthInverse(ch);
      }
    }

    if (ch == uint32_t(-1)) {
      aDeletedCharsArray.AppendElement(true);
      mergeNeeded = true;
    } else {
      aDeletedCharsArray.AppendElement(false);
      aCharsToMergeArray.AppendElement(false);
      if (auxiliaryOutputArrays) {
        aStyleArray->AppendElement(charStyle);
        aCanBreakBeforeArray->AppendElement(
            inhibitBreakBefore
                ? gfxShapedText::CompressedGlyph::FLAG_BREAK_TYPE_NONE
                : aTextRun->CanBreakBefore(aOffsetInTextRun));
      }

      if (IS_IN_BMP(ch)) {
        aConvertedString.Append(maskPassword ? kPasswordMask : ch);
      } else {
        if (maskPassword) {
          aConvertedString.Append(kPasswordMask);
          // TODO: We should show a password mask for a surrogate pair later.
          aConvertedString.Append(kPasswordMask);
        } else {
          aConvertedString.Append(H_SURROGATE(ch));
          aConvertedString.Append(L_SURROGATE(ch));
        }
        ++extraChars;
        ++i;
        ++aOffsetInTextRun;
        // Skip the trailing surrogate.
        aDeletedCharsArray.AppendElement(true);
      }

      while (extraChars-- > 0) {
        mergeNeeded = true;
        aCharsToMergeArray.AppendElement(true);
        if (auxiliaryOutputArrays) {
          aStyleArray->AppendElement(charStyle);
          aCanBreakBeforeArray->AppendElement(
              gfxShapedText::CompressedGlyph::FLAG_BREAK_TYPE_NONE);
        }
      }
    }
  }

  return mergeNeeded;
}

void nsCaseTransformTextRunFactory::RebuildTextRun(
    nsTransformedTextRun* aTextRun, DrawTarget* aRefDrawTarget,
    gfxMissingFontRecorder* aMFR) {
  nsAutoString convertedString;
  AutoTArray<bool, 50> charsToMergeArray;
  AutoTArray<bool, 50> deletedCharsArray;
  AutoTArray<uint8_t, 50> canBreakBeforeArray;
  AutoTArray<RefPtr<nsTransformedCharStyle>, 50> styleArray;

  bool mergeNeeded = TransformString(
      aTextRun->mString, convertedString, mAllUppercase,
      /* aCaseTransformsOnly = */ false, nullptr, charsToMergeArray,
      deletedCharsArray, aTextRun, 0, &canBreakBeforeArray, &styleArray);

  gfx::ShapedTextFlags flags;
  gfxTextRunFactory::Parameters innerParams =
      GetParametersForInner(aTextRun, &flags, aRefDrawTarget);
  gfxFontGroup* fontGroup = aTextRun->GetFontGroup();

  RefPtr<nsTransformedTextRun> transformedChild;
  RefPtr<gfxTextRun> cachedChild;
  gfxTextRun* child;

  if (mInnerTransformingTextRunFactory) {
    transformedChild = mInnerTransformingTextRunFactory->MakeTextRun(
        convertedString.BeginReading(), convertedString.Length(), &innerParams,
        fontGroup, flags, nsTextFrameUtils::Flags(), std::move(styleArray),
        false);
    child = transformedChild.get();
  } else {
    cachedChild = fontGroup->MakeTextRun(
        convertedString.BeginReading(), convertedString.Length(), &innerParams,
        flags, nsTextFrameUtils::Flags(), aMFR);
    child = cachedChild.get();
  }
  if (!child) {
    return;
  }
  // Copy potential linebreaks into child so they're preserved
  // (and also child will be shaped appropriately)
  NS_ASSERTION(convertedString.Length() == canBreakBeforeArray.Length(),
               "Dropped characters or break-before values somewhere!");
  gfxTextRun::Range range(0, uint32_t(canBreakBeforeArray.Length()));
  child->SetPotentialLineBreaks(range, canBreakBeforeArray.Elements());
  if (transformedChild) {
    transformedChild->FinishSettingProperties(aRefDrawTarget, aMFR);
  }

  aTextRun->ResetGlyphRuns();
  if (mergeNeeded) {
    // Now merge multiple characters into one multi-glyph character as required
    // and deal with skipping deleted accent chars
    NS_ASSERTION(charsToMergeArray.Length() == child->GetLength(),
                 "source length mismatch");
    NS_ASSERTION(deletedCharsArray.Length() == aTextRun->GetLength(),
                 "destination length mismatch");
    MergeCharactersInTextRun(aTextRun, child, charsToMergeArray.Elements(),
                             deletedCharsArray.Elements());
  } else {
    // No merging to do, so just copy; this produces a more optimized textrun.
    // We can't steal the data because the child may be cached and stealing
    // the data would break the cache.
    aTextRun->CopyGlyphDataFrom(child, gfxTextRun::Range(child), 0);
  }
}
