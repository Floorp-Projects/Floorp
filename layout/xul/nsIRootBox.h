/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef nsIRootBox_h___
#define nsIRootBox_h___

#include "nsQueryFrame.h"
class nsPopupSetFrame;
class nsIContent;
class nsIPresShell;

class nsIRootBox
{
public:
  NS_DECL_QUERYFRAME_TARGET(nsIRootBox)

  virtual nsPopupSetFrame* GetPopupSetFrame() = 0;
  virtual void SetPopupSetFrame(nsPopupSetFrame* aPopupSet) = 0;

  virtual nsIContent* GetDefaultTooltip() = 0;
  virtual void SetDefaultTooltip(nsIContent* aTooltip) = 0;

  virtual nsresult AddTooltipSupport(nsIContent* aNode) = 0;
  virtual nsresult RemoveTooltipSupport(nsIContent* aNode) = 0;

  static nsIRootBox* GetRootBox(nsIPresShell* aShell);
};

#endif

