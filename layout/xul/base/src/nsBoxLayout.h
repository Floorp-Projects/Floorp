/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Author:
 * Eric D Vaughan
 *
 * Contributor(s): 
 */

#ifndef nsBoxLayout_h___
#define nsBoxLayout_h___

#include "nsIBoxLayout.h"

class nsBoxLayout : public nsIBoxLayout {

public:

  nsBoxLayout();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Layout(nsIBox* aBox, nsBoxLayoutState& aState);

  NS_IMETHOD PaintDebug(nsIBox* aBox, 
                        nsIPresContext* aPresContext,
                        nsIRenderingContext& aRenderingContext,
                        const nsRect& aDirtyRect,
                        nsFramePaintLayer aWhichLayer);

  NS_IMETHOD DisplayDebugInfoFor(nsIBox* aBox, 
                                 nsIPresContext* aPresContext,
                                 nsPoint&        aPoint,
                                 PRInt32&        aCursor);


  NS_IMETHOD GetPrefSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetMinSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetMaxSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetFlex(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nscoord& aFlex);
  NS_IMETHOD GetAscent(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nscoord& aAscent);
  NS_IMETHOD IsCollapsed(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, PRBool& aCollapsed);

  virtual void AddBorderAndPadding(nsIBox* aBox, nsSize& aSize);
  virtual void AddInset(nsIBox* aBox, nsSize& aSize);
  virtual void AddMargin(nsIBox* aChild, nsSize& aSize);
  virtual void AddMargin(nsSize& aSize, const nsMargin& aMargin);

  virtual void AddLargestSize(nsSize& aSize, const nsSize& aToAdd);
  virtual void AddSmallestSize(nsSize& aSize, const nsSize& aToAdd);
};

#endif

