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
 * Contributor(s): 
 *   David Hyatt (hyatt@netscape.com)
 */

/**

  Eric D Vaughan
  nsBoxFrame is a frame that can lay its children out either vertically or horizontally.
  It lays them out according to a min max or preferred size.
 
**/

#ifndef nsIBox_h___
#define nsIBox_h___

#include "nsISupports.h"
#include "nsIBoxLayout.h"

class nsBoxLayoutState;
struct nsRect;
struct nsSize;

// {162F6B5A-F926-11d3-BA06-001083023C1E}
#define NS_IBOX_IID { 0x162f6b5a, 0xf926, 0x11d3, { 0xba, 0x6, 0x0, 0x10, 0x83, 0x2, 0x3c, 0x1e } }

#define DEFAULT_ORDINAL_GROUP 1

class nsIBox : public nsISupports {

public:

  static const nsIID& GetIID() { static nsIID iid = NS_IBOX_IID; return iid; }

    enum Halignment {
        hAlign_Left,
        hAlign_Right,
        hAlign_Center
  };

    enum Valignment {
        vAlign_Top,
        vAlign_Middle,
        vAlign_BaseLine,
        vAlign_Bottom
    };

  NS_IMETHOD GetPrefSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)=0;
  NS_IMETHOD GetMinSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)=0;
  NS_IMETHOD GetMaxSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)=0;
  NS_IMETHOD GetFlex(nsBoxLayoutState& aBoxLayoutState, nscoord& aFlex)=0;
  NS_IMETHOD GetOrdinal(nsBoxLayoutState& aBoxLayoutState, PRUint32& aOrdinal)=0;
  NS_IMETHOD GetAscent(nsBoxLayoutState& aBoxLayoutState, nscoord& aAscent)=0;
  NS_IMETHOD IsCollapsed(nsBoxLayoutState& aBoxLayoutState, PRBool& aCollapsed)=0;
  NS_IMETHOD Collapse(nsBoxLayoutState& aBoxLayoutState)=0;
  NS_IMETHOD UnCollapse(nsBoxLayoutState& aBoxLayoutState)=0;
  NS_IMETHOD SetBounds(nsBoxLayoutState& aBoxLayoutState, const nsRect& aRect)=0;
  NS_IMETHOD GetBounds(nsRect& aRect)=0;
  NS_IMETHOD Layout(nsBoxLayoutState& aBoxLayoutState)=0;
  NS_IMETHOD IsDirty(PRBool& aIsDirty)=0;
  NS_IMETHOD HasDirtyChildren(PRBool& aIsDirty)=0;
  NS_IMETHOD MarkDirty(nsBoxLayoutState& aState)=0;
  NS_IMETHOD MarkDirtyChildren(nsBoxLayoutState& aState)=0;
  NS_IMETHOD SetDebug(nsBoxLayoutState& aState, PRBool aDebug)=0;
  NS_IMETHOD GetDebug(PRBool& aDebug)=0;
  NS_IMETHOD GetChildBox(nsIBox** aBox)=0;
  NS_IMETHOD GetNextBox(nsIBox** aBox)=0;
  NS_IMETHOD SetNextBox(nsIBox* aBox)=0;
  NS_IMETHOD GetParentBox(nsIBox** aParent)=0;
  NS_IMETHOD SetParentBox(nsIBox* aParent)=0;
  NS_IMETHOD GetFrame(nsIFrame** aFrame)=0;
  NS_IMETHOD GetBorderAndPadding(nsMargin& aBorderAndPadding)=0;
  NS_IMETHOD GetBorder(nsMargin& aBorderAndPadding)=0;
  NS_IMETHOD GetPadding(nsMargin& aBorderAndPadding)=0;
  NS_IMETHOD GetInset(nsMargin& aInset)=0;
  NS_IMETHOD GetMargin(nsMargin& aMargin)=0;
  NS_IMETHOD SetLayoutManager(nsIBoxLayout* aLayout)=0;
  NS_IMETHOD GetLayoutManager(nsIBoxLayout** aLayout)=0;
  NS_IMETHOD GetContentRect(nsRect& aContentRect) = 0;
  NS_IMETHOD GetClientRect(nsRect& aContentRect) = 0;
  NS_IMETHOD GetVAlign(Valignment& aAlign) = 0;
  NS_IMETHOD GetHAlign(Halignment& aAlign) = 0;
  NS_IMETHOD GetOrientation(PRBool& aIsHorizontal)=0;
  NS_IMETHOD GetDirection(PRBool& aIsNormal)=0;
  NS_IMETHOD Redraw(nsBoxLayoutState& aState, const nsRect* aRect = nsnull, PRBool aImmediate = PR_FALSE)=0;
  NS_IMETHOD NeedsRecalc()=0;
  NS_IMETHOD GetDebugBoxAt(const nsPoint& aPoint, nsIBox** aBox)=0;
  NS_IMETHOD RelayoutDirtyChild(nsBoxLayoutState& aState, nsIBox* aChild)=0;
  NS_IMETHOD RelayoutStyleChange(nsBoxLayoutState& aState, nsIBox* aChild)=0;
  NS_IMETHOD RelayoutChildAtOrdinal(nsBoxLayoutState& aState, nsIBox* aChild)=0;
  NS_IMETHOD GetMouseThrough(PRBool& aMouseThrough)=0;
  NS_IMETHOD MarkChildrenStyleChange()=0;
  NS_IMETHOD MarkStyleChange(nsBoxLayoutState& aState)=0;
  NS_IMETHOD DumpBox(FILE* out)=0;
  NS_IMETHOD ChildrenMustHaveWidgets(PRBool& aMust)=0;

  static PRBool AddCSSPrefSize(nsBoxLayoutState& aState, nsIBox* aBox, nsSize& aSize);
  static PRBool AddCSSMinSize(nsBoxLayoutState& aState, nsIBox* aBox, nsSize& aSize);
  static PRBool AddCSSMaxSize(nsBoxLayoutState& aState, nsIBox* aBox, nsSize& aSize);
  static PRBool AddCSSFlex(nsBoxLayoutState& aState, nsIBox* aBox, nscoord& aFlex);
  static PRBool AddCSSCollapsed(nsBoxLayoutState& aState, nsIBox* aBox, PRBool& aCollapsed);
  static PRBool AddCSSOrdinal(nsBoxLayoutState& aState, nsIBox* aBox, PRUint32& aOrdinal);

};

#endif

