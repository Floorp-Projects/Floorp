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

#ifndef nsIBoxLayout_h___
#define nsIBoxLayout_h___

#include "nsISupports.h"
#include "nsIFrame.h"

class nsIPresContext;
class nsIBox;
class nsBoxLayout;
class nsBoxLayoutState;
class nsIRenderingContext;
struct nsRect;

// {162F6B5F-F926-11d3-BA06-001083023C1E}
#define NS_IBOX_LAYOUT_IID { 0x162f6b5f, 0xf926, 0x11d3, { 0xba, 0x6, 0x0, 0x10, 0x83, 0x2, 0x3c, 0x1e } }

class nsIBoxLayout : public nsISupports {

public:

  static const nsIID& GetIID() { static nsIID iid = NS_IBOX_LAYOUT_IID; return iid; }

  NS_IMETHOD Layout(nsIBox* aBox, nsBoxLayoutState& aState)=0;

  NS_IMETHOD GetPrefSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)=0;
  NS_IMETHOD GetMinSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)=0;
  NS_IMETHOD GetMaxSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)=0;
  NS_IMETHOD GetFlex(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nscoord& aFlex)=0;
  NS_IMETHOD GetAscent(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nscoord& aAscent)=0;
  NS_IMETHOD IsCollapsed(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, PRBool& aCollapsed)=0;

  NS_IMETHOD ChildrenInserted(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aPrevBox, nsIBox* aChildList)=0;
  NS_IMETHOD ChildrenAppended(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aChildList)=0;
  NS_IMETHOD ChildrenRemoved(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aChildList)=0;
  NS_IMETHOD ChildBecameDirty(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aChild)=0;
  NS_IMETHOD BecameDirty(nsIBox* aBox, nsBoxLayoutState& aState)=0;
};

#endif

