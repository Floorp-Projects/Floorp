/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBoxLayout_h___
#define nsBoxLayout_h___

#include "nsISupports.h"
#include "nsCoord.h"
#include "nsFrameList.h"

class nsIFrame;
class nsBoxLayoutState;
struct nsSize;
struct nsMargin;

#define NS_BOX_LAYOUT_IID \
{ 0x09d522a7, 0x304c, 0x4137, \
 { 0xaf, 0xc9, 0xe0, 0x80, 0x2e, 0x89, 0xb7, 0xe8 } }

class nsIGridPart;

class nsBoxLayout : public nsISupports {

protected:
  virtual ~nsBoxLayout() {}

public:

  nsBoxLayout() {}

  NS_DECL_ISUPPORTS

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_BOX_LAYOUT_IID)

  NS_IMETHOD XULLayout(nsIFrame* aBox, nsBoxLayoutState& aState);

  virtual nsSize GetXULPrefSize(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetXULMinSize(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetXULMaxSize(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState);
  virtual nscoord GetAscent(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState);
  virtual void ChildrenInserted(nsIFrame* aBox, nsBoxLayoutState& aState,
                                nsIFrame* aPrevBox,
                                const nsFrameList::Slice& aNewChildren) {}
  virtual void ChildrenAppended(nsIFrame* aBox, nsBoxLayoutState& aState,
                                const nsFrameList::Slice& aNewChildren) {}
  virtual void ChildrenRemoved(nsIFrame* aBox, nsBoxLayoutState& aState, nsIFrame* aChildList) {}
  virtual void ChildrenSet(nsIFrame* aBox, nsBoxLayoutState& aState, nsIFrame* aChildList) {}
  virtual void IntrinsicISizesDirty(nsIFrame* aBox, nsBoxLayoutState& aState) {}

  virtual void AddBorderAndPadding(nsIFrame* aBox, nsSize& aSize);
  virtual void AddMargin(nsIFrame* aChild, nsSize& aSize);
  virtual void AddMargin(nsSize& aSize, const nsMargin& aMargin);

  virtual nsIGridPart* AsGridPart() { return nullptr; }

  static void AddLargestSize(nsSize& aSize, const nsSize& aToAdd);
  static void AddSmallestSize(nsSize& aSize, const nsSize& aToAdd);
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsBoxLayout, NS_BOX_LAYOUT_IID)

#endif

