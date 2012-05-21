/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBoxLayout_h___
#define nsBoxLayout_h___

#include "nsISupports.h"
#include "nsIFrame.h"

#define NS_BOX_LAYOUT_IID \
{ 0x09d522a7, 0x304c, 0x4137, \
 { 0xaf, 0xc9, 0xe0, 0x80, 0x2e, 0x89, 0xb7, 0xe8 } }

class nsIGridPart;

class nsBoxLayout : public nsISupports {

public:

  nsBoxLayout() {}
  virtual ~nsBoxLayout() {}

  NS_DECL_ISUPPORTS

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_BOX_LAYOUT_IID)

  NS_IMETHOD Layout(nsIBox* aBox, nsBoxLayoutState& aState);

  virtual nsSize GetPrefSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetMinSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetMaxSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState);
  virtual nscoord GetAscent(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState);
  virtual void ChildrenInserted(nsIBox* aBox, nsBoxLayoutState& aState,
                                nsIBox* aPrevBox,
                                const nsFrameList::Slice& aNewChildren) {}
  virtual void ChildrenAppended(nsIBox* aBox, nsBoxLayoutState& aState,
                                const nsFrameList::Slice& aNewChildren) {}
  virtual void ChildrenRemoved(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aChildList) {}
  virtual void ChildrenSet(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aChildList) {}
  virtual void IntrinsicWidthsDirty(nsIBox* aBox, nsBoxLayoutState& aState) {}

  virtual void AddBorderAndPadding(nsIBox* aBox, nsSize& aSize);
  virtual void AddMargin(nsIBox* aChild, nsSize& aSize);
  virtual void AddMargin(nsSize& aSize, const nsMargin& aMargin);

  virtual nsIGridPart* AsGridPart() { return nsnull; }

  static void AddLargestSize(nsSize& aSize, const nsSize& aToAdd);
  static void AddSmallestSize(nsSize& aSize, const nsSize& aToAdd);
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsBoxLayout, NS_BOX_LAYOUT_IID)

#endif

