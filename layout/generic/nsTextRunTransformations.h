/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSTEXTRUNTRANSFORMATIONS_H_
#define NSTEXTRUNTRANSFORMATIONS_H_

#include "mozilla/Attributes.h"
#include "gfxFont.h"

class nsTransformedTextRun;
class nsStyleContext;

class nsTransformingTextRunFactory {
public:
  virtual ~nsTransformingTextRunFactory() {}

  // Default 8-bit path just transforms to Unicode and takes that path
  nsTransformedTextRun* MakeTextRun(const uint8_t* aString, uint32_t aLength,
                                    const gfxFontGroup::Parameters* aParams,
                                    gfxFontGroup* aFontGroup, uint32_t aFlags,
                                    nsStyleContext** aStyles, bool aOwnsFactory = true);
  nsTransformedTextRun* MakeTextRun(const PRUnichar* aString, uint32_t aLength,
                                    const gfxFontGroup::Parameters* aParams,
                                    gfxFontGroup* aFontGroup, uint32_t aFlags,
                                    nsStyleContext** aStyles, bool aOwnsFactory = true);

  virtual void RebuildTextRun(nsTransformedTextRun* aTextRun, gfxContext* aRefContext) = 0;
};

/**
 * Builds textruns that render their text using a font-variant (i.e.,
 * smallcaps).
 */
class nsFontVariantTextRunFactory : public nsTransformingTextRunFactory {
public:
  virtual void RebuildTextRun(nsTransformedTextRun* aTextRun, gfxContext* aRefContext) MOZ_OVERRIDE;
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
  nsCaseTransformTextRunFactory(nsTransformingTextRunFactory* aInnerTransformingTextRunFactory,
                                bool aAllUppercase = false)
    : mInnerTransformingTextRunFactory(aInnerTransformingTextRunFactory),
      mAllUppercase(aAllUppercase) {}

  virtual void RebuildTextRun(nsTransformedTextRun* aTextRun, gfxContext* aRefContext) MOZ_OVERRIDE;

protected:
  nsAutoPtr<nsTransformingTextRunFactory> mInnerTransformingTextRunFactory;
  bool                                    mAllUppercase;
};

/**
 * So that we can reshape as necessary, we store enough information
 * to fully rebuild the textrun contents.
 */
class nsTransformedTextRun : public gfxTextRun {
public:
  static nsTransformedTextRun *Create(const gfxTextRunFactory::Parameters* aParams,
                                      nsTransformingTextRunFactory* aFactory,
                                      gfxFontGroup* aFontGroup,
                                      const PRUnichar* aString, uint32_t aLength,
                                      const uint32_t aFlags, nsStyleContext** aStyles,
                                      bool aOwnsFactory);

  ~nsTransformedTextRun() {
    if (mOwnsFactory) {
      delete mFactory;
    }
  }
  
  void SetCapitalization(uint32_t aStart, uint32_t aLength,
                         bool* aCapitalization,
                         gfxContext* aRefContext);
  virtual bool SetPotentialLineBreaks(uint32_t aStart, uint32_t aLength,
                                        uint8_t* aBreakBefore,
                                        gfxContext* aRefContext);
  /**
   * Called after SetCapitalization and SetPotentialLineBreaks
   * are done and before we request any data from the textrun. Also always
   * called after a Create.
   */
  void FinishSettingProperties(gfxContext* aRefContext)
  {
    if (mNeedsRebuild) {
      mNeedsRebuild = false;
      mFactory->RebuildTextRun(this, aRefContext);
    }
  }

  // override the gfxTextRun impls to account for additional members here
  virtual size_t SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) MOZ_MUST_OVERRIDE;
  virtual size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) MOZ_MUST_OVERRIDE;

  nsTransformingTextRunFactory       *mFactory;
  nsTArray<nsRefPtr<nsStyleContext> > mStyles;
  nsTArray<bool>                      mCapitalize;
  nsString                            mString;
  bool                                mOwnsFactory;
  bool                                mNeedsRebuild;

private:
  nsTransformedTextRun(const gfxTextRunFactory::Parameters* aParams,
                       nsTransformingTextRunFactory* aFactory,
                       gfxFontGroup* aFontGroup,
                       const PRUnichar* aString, uint32_t aLength,
                       const uint32_t aFlags, nsStyleContext** aStyles,
                       bool aOwnsFactory)
    : gfxTextRun(aParams, aLength, aFontGroup, aFlags),
      mFactory(aFactory), mString(aString, aLength),
      mOwnsFactory(aOwnsFactory), mNeedsRebuild(true)
  {
    mCharacterGlyphs = reinterpret_cast<CompressedGlyph*>(this + 1);

    uint32_t i;
    for (i = 0; i < aLength; ++i) {
      mStyles.AppendElement(aStyles[i]);
    }
  }
};

#endif /*NSTEXTRUNTRANSFORMATIONS_H_*/
