/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MATHMLTEXTRUNFACTORY_H_
#define MATHMLTEXTRUNFACTORY_H_

#include "mozilla/UniquePtr.h"
#include "nsTextRunTransformations.h"

/**
 * Builds textruns that render their text with MathML specific renderings.
 */
class MathMLTextRunFactory : public nsTransformingTextRunFactory {
 public:
  MathMLTextRunFactory(mozilla::UniquePtr<nsTransformingTextRunFactory>
                           aInnerTransformingTextRunFactory,
                       uint32_t aFlags, uint8_t aSSTYScriptLevel,
                       float aFontInflation)
      : mInnerTransformingTextRunFactory(
            std::move(aInnerTransformingTextRunFactory)),
        mFlags(aFlags),
        mFontInflation(aFontInflation),
        mSSTYScriptLevel(aSSTYScriptLevel) {}

  virtual void RebuildTextRun(nsTransformedTextRun* aTextRun,
                              mozilla::gfx::DrawTarget* aRefDrawTarget,
                              gfxMissingFontRecorder* aMFR) override;
  enum {
    // Style effects which may override single character <mi> behaviour
    MATH_FONT_FEATURE_DTLS = 0x4,    // font feature dtls should be set
  };

 protected:
  mozilla::UniquePtr<nsTransformingTextRunFactory>
      mInnerTransformingTextRunFactory;
  uint32_t mFlags;
  float mFontInflation;
  uint8_t mSSTYScriptLevel;
};

#endif /*MATHMLTEXTRUNFACTORY_H_*/
