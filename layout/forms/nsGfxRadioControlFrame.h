/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGfxRadioControlFrame_h___
#define nsGfxRadioControlFrame_h___

#include "mozilla/Attributes.h"
#include "nsFormControlFrame.h"


// nsGfxRadioControlFrame

class nsGfxRadioControlFrame : public nsFormControlFrame
{
public:
  explicit nsGfxRadioControlFrame(nsStyleContext* aContext);
  ~nsGfxRadioControlFrame();

  NS_DECL_FRAMEARENA_HELPERS

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() MOZ_OVERRIDE;
#endif

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;
};

#endif
