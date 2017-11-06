/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSTEXTRUNTRANSFORMATIONS_H_
#define NSTEXTRUNTRANSFORMATIONS_H_

#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/UniquePtr.h"
#include "gfxTextRun.h"
#include "nsStyleContext.h"

class nsTransformedTextRun;

struct nsTransformedCharStyle final {
  NS_INLINE_DECL_REFCOUNTING(nsTransformedCharStyle)

  explicit nsTransformedCharStyle(nsStyleContext* aContext)
    : mFont(aContext->StyleFont()->mFont)
    , mLanguage(aContext->StyleFont()->mLanguage)
    , mPresContext(aContext->PresContext())
    , mScriptSizeMultiplier(aContext->StyleFont()->mScriptSizeMultiplier)
    , mTextTransform(aContext->StyleText()->mTextTransform)
    , mMathVariant(aContext->StyleFont()->mMathVariant)
    , mExplicitLanguage(aContext->StyleFont()->mExplicitLanguage) {}

  nsFont                  mFont;
  RefPtr<nsAtom>       mLanguage;
  RefPtr<nsPresContext> mPresContext;
  float                   mScriptSizeMultiplier;
  uint8_t                 mTextTransform;
  uint8_t                 mMathVariant;
  bool                    mExplicitLanguage;
  bool                    mForceNonFullWidth = false;

private:
  ~nsTransformedCharStyle() {}
  nsTransformedCharStyle(const nsTransformedCharStyle& aOther) = delete;
  nsTransformedCharStyle& operator=(const nsTransformedCharStyle& aOther) = delete;
};

class nsTransformingTextRunFactory {
public:
  virtual ~nsTransformingTextRunFactory() {}

  // Default 8-bit path just transforms to Unicode and takes that path
  already_AddRefed<nsTransformedTextRun>
  MakeTextRun(const uint8_t* aString, uint32_t aLength,
              const gfxFontGroup::Parameters* aParams,
              gfxFontGroup* aFontGroup,
              mozilla::gfx::ShapedTextFlags aFlags,
              nsTextFrameUtils::Flags aFlags2,
              nsTArray<RefPtr<nsTransformedCharStyle>>&& aStyles,
              bool aOwnsFactory);

  already_AddRefed<nsTransformedTextRun>
  MakeTextRun(const char16_t* aString, uint32_t aLength,
              const gfxFontGroup::Parameters* aParams,
              gfxFontGroup* aFontGroup,
              mozilla::gfx::ShapedTextFlags aFlags,
              nsTextFrameUtils::Flags aFlags2,
              nsTArray<RefPtr<nsTransformedCharStyle>>&& aStyles,
              bool aOwnsFactory);

  virtual void RebuildTextRun(nsTransformedTextRun* aTextRun,
                              mozilla::gfx::DrawTarget* aRefDrawTarget,
                              gfxMissingFontRecorder* aMFR) = 0;
};

/**
 * Builds textruns that transform the text in some way (e.g., capitalize)
 * and then render the text using some other textrun implementation.
 */
class nsCaseTransformTextRunFactory : public nsTransformingTextRunFactory {
public:
  // We could add an optimization here so that when there is no inner
  // factory, no title-case conversion, and no upper-casing of SZLIG, we override
  // MakeTextRun (after making it virtual in the superclass) and have it
  // just convert the string to uppercase or lowercase and create the textrun
  // via the fontgroup.

  // Takes ownership of aInnerTransformTextRunFactory
  explicit nsCaseTransformTextRunFactory(mozilla::UniquePtr<nsTransformingTextRunFactory> aInnerTransformingTextRunFactory,
                                         bool aAllUppercase = false)
    : mInnerTransformingTextRunFactory(Move(aInnerTransformingTextRunFactory)),
      mAllUppercase(aAllUppercase) {}

  virtual void RebuildTextRun(nsTransformedTextRun* aTextRun,
                              mozilla::gfx::DrawTarget* aRefDrawTarget,
                              gfxMissingFontRecorder* aMFR) override;

  // Perform a transformation on the given string, writing the result into
  // aConvertedString. If aAllUppercase is true, the transform is (global)
  // upper-casing, and aLanguage is used to determine any language-specific
  // behavior; otherwise, an nsTransformedTextRun should be passed in
  // as aTextRun and its styles will be used to determine the transform(s)
  // to be applied.
  // If such an input textrun is provided, then its line-breaks and styles
  // will be copied to the output arrays, which must also be provided by
  // the caller. For the global upper-casing usage (no input textrun),
  // these are ignored.
  static bool TransformString(const nsAString& aString,
                              nsString& aConvertedString,
                              bool aAllUppercase,
                              const nsAtom* aLanguage,
                              nsTArray<bool>& aCharsToMergeArray,
                              nsTArray<bool>& aDeletedCharsArray,
                              const nsTransformedTextRun* aTextRun = nullptr,
                              uint32_t aOffsetInTextRun = 0,
                              nsTArray<uint8_t>* aCanBreakBeforeArray = nullptr,
                              nsTArray<RefPtr<nsTransformedCharStyle>>* aStyleArray = nullptr);

protected:
  mozilla::UniquePtr<nsTransformingTextRunFactory> mInnerTransformingTextRunFactory;
  bool mAllUppercase;
};

/**
 * So that we can reshape as necessary, we store enough information
 * to fully rebuild the textrun contents.
 */
class nsTransformedTextRun final : public gfxTextRun {
public:

  static already_AddRefed<nsTransformedTextRun>
  Create(const gfxTextRunFactory::Parameters* aParams,
         nsTransformingTextRunFactory* aFactory,
         gfxFontGroup* aFontGroup,
         const char16_t* aString, uint32_t aLength,
         const mozilla::gfx::ShapedTextFlags aFlags,
         const nsTextFrameUtils::Flags aFlags2,
         nsTArray<RefPtr<nsTransformedCharStyle>>&& aStyles,
         bool aOwnsFactory);

  ~nsTransformedTextRun() {
    if (mOwnsFactory) {
      delete mFactory;
    }
  }

  void SetCapitalization(uint32_t aStart, uint32_t aLength,
                         bool* aCapitalization);
  virtual bool SetPotentialLineBreaks(Range aRange,
                                      const uint8_t* aBreakBefore) override;
  /**
   * Called after SetCapitalization and SetPotentialLineBreaks
   * are done and before we request any data from the textrun. Also always
   * called after a Create.
   */
  void FinishSettingProperties(mozilla::gfx::DrawTarget* aRefDrawTarget,
                               gfxMissingFontRecorder* aMFR)
  {
    if (mNeedsRebuild) {
      mNeedsRebuild = false;
      mFactory->RebuildTextRun(this, aRefDrawTarget, aMFR);
    }
  }

  // override the gfxTextRun impls to account for additional members here
  virtual size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) override;
  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) override;

  nsTransformingTextRunFactory       *mFactory;
  nsTArray<RefPtr<nsTransformedCharStyle>> mStyles;
  nsTArray<bool>                      mCapitalize;
  nsString                            mString;
  bool                                mOwnsFactory;
  bool                                mNeedsRebuild;

private:
  nsTransformedTextRun(const gfxTextRunFactory::Parameters* aParams,
                       nsTransformingTextRunFactory* aFactory,
                       gfxFontGroup* aFontGroup,
                       const char16_t* aString, uint32_t aLength,
                       const mozilla::gfx::ShapedTextFlags aFlags,
                       const nsTextFrameUtils::Flags aFlags2,
                       nsTArray<RefPtr<nsTransformedCharStyle>>&& aStyles,
                       bool aOwnsFactory)
    : gfxTextRun(aParams, aLength, aFontGroup, aFlags, aFlags2),
      mFactory(aFactory), mStyles(aStyles), mString(aString, aLength),
      mOwnsFactory(aOwnsFactory), mNeedsRebuild(true)
  {
    mCharacterGlyphs = reinterpret_cast<CompressedGlyph*>(this + 1);
  }
};

/**
 * Copy a given textrun, but merge certain characters into a single logical
 * character. Glyphs for a character are added to the glyph list for the previous
 * character and then the merged character is eliminated. Visually the results
 * are identical.
 *
 * This is used for text-transform:uppercase when we encounter a SZLIG,
 * whose uppercase form is "SS", or other ligature or precomposed form
 * that expands to multiple codepoints during case transformation,
 * and for Greek text when combining diacritics have been deleted.
 *
 * This function is unable to merge characters when they occur in different
 * glyph runs. This only happens in tricky edge cases where a character was
 * decomposed by case-mapping (e.g. there's no precomposed uppercase version
 * of an accented lowercase letter), and then font-matching caused the
 * diacritics to be assigned to a different font than the base character.
 * In this situation, the diacritic(s) get discarded, which is less than
 * ideal, but they probably weren't going to render very well anyway.
 * Bug 543200 will improve this by making font-matching operate on entire
 * clusters instead of individual codepoints.
 *
 * For simplicity, this produces a textrun containing all DetailedGlyphs,
 * no simple glyphs. So don't call it unless you really have merging to do.
 *
 * @param aCharsToMerge when aCharsToMerge[i] is true, this character in aSrc
 * is merged into the previous character
 *
 * @param aDeletedChars when aDeletedChars[i] is true, the character at this
 * position in aDest was deleted (has no corresponding char in aSrc)
 */
void
MergeCharactersInTextRun(gfxTextRun* aDest, gfxTextRun* aSrc,
                         const bool* aCharsToMerge, const bool* aDeletedChars);

gfxTextRunFactory::Parameters
GetParametersForInner(nsTransformedTextRun* aTextRun,
                      mozilla::gfx::ShapedTextFlags* aFlags,
                      mozilla::gfx::DrawTarget* aRefDrawTarget);


#endif /*NSTEXTRUNTRANSFORMATIONS_H_*/
