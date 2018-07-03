/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef nsIRootBox_h___
#define nsIRootBox_h___

#include "nsQueryFrame.h"
class nsPopupSetFrame;
class nsIPresShell;
class nsIContent;

namespace mozilla {
namespace dom {
class Element;
}
}

class nsIRootBox
{
public:
  NS_DECL_QUERYFRAME_TARGET(nsIRootBox)

  virtual nsPopupSetFrame* GetPopupSetFrame() = 0;
  virtual void SetPopupSetFrame(nsPopupSetFrame* aPopupSet) = 0;

  virtual mozilla::dom::Element* GetDefaultTooltip() = 0;
  virtual void SetDefaultTooltip(mozilla::dom::Element* aTooltip) = 0;

  static nsIRootBox* GetRootBox(nsIPresShell* aShell);
};

#endif

