/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MATHVARIANTTEXTRUNFACTORY_H_
#define MATHVARIANTTEXTRUNFACTORY_H_

#include "nsTextRunTransformations.h"

/**
 * Builds textruns that render their text using a mathvariant
 */
class nsMathVariantTextRunFactory : public nsTransformingTextRunFactory {
public:
  nsMathVariantTextRunFactory(nsTransformingTextRunFactory* aInnerTransformingTextRunFactory)
    : mInnerTransformingTextRunFactory(aInnerTransformingTextRunFactory) {}

  virtual void RebuildTextRun(nsTransformedTextRun* aTextRun,
                              gfxContext* aRefContext) MOZ_OVERRIDE;
protected:
  nsAutoPtr<nsTransformingTextRunFactory> mInnerTransformingTextRunFactory;
};

#endif /*MATHVARIANTTEXTRUNFACTORY_H_*/
