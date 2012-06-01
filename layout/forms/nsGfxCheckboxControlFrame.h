/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsGfxCheckboxControlFrame_h___
#define nsGfxCheckboxControlFrame_h___

#include "nsFormControlFrame.h"

#ifdef ACCESSIBILITY
class nsIAccessible;
#endif

class nsGfxCheckboxControlFrame : public nsFormControlFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  nsGfxCheckboxControlFrame(nsStyleContext* aContext);
  virtual ~nsGfxCheckboxControlFrame();

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const {
    return MakeFrameName(NS_LITERAL_STRING("CheckboxControl"), aResult);
  }
#endif

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

#ifdef ACCESSIBILITY
  virtual already_AddRefed<Accessible> CreateAccessible();
#endif

protected:

  bool IsChecked();
  bool IsIndeterminate();
};

#endif

