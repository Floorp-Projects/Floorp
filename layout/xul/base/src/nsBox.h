/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsBox_h___
#define nsBox_h___

#include "nsIBox.h"

class nsITheme;

class nsBox : public nsIBox {

public:

  static void Shutdown();

  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD GetChildBox(nsIBox** aBox);
  NS_IMETHOD GetNextBox(nsIBox** aBox);
  NS_IMETHOD SetNextBox(nsIBox* aBox);
  NS_IMETHOD GetParentBox(nsIBox** aParent);
  NS_IMETHOD SetParentBox(nsIBox* aParent);
  NS_IMETHOD IsDirty(PRBool& aIsDirty);
  NS_IMETHOD HasDirtyChildren(PRBool& aIsDirty);
  NS_IMETHOD MarkDirty(nsBoxLayoutState& aState);
  NS_IMETHOD MarkDirtyChildren(nsBoxLayoutState& aState);
  NS_IMETHOD SetBounds(nsBoxLayoutState& aBoxLayoutState, const nsRect& aRect);
  NS_IMETHOD GetBounds(nsRect& aRect);
  NS_IMETHOD GetBorderAndPadding(nsMargin& aBorderAndPadding);
  NS_IMETHOD GetBorder(nsMargin& aBorderAndPadding);
  NS_IMETHOD GetPadding(nsMargin& aBorderAndPadding);

  NS_IMETHOD GetMargin(nsMargin& aMargin);
  NS_IMETHOD SetLayoutManager(nsIBoxLayout* aLayout);
  NS_IMETHOD GetLayoutManager(nsIBoxLayout** aLayout);
  NS_IMETHOD GetContentRect(nsRect& aContentRect);
  NS_IMETHOD GetClientRect(nsRect& aContentRect);
  NS_IMETHOD GetVAlign(Valignment& aAlign);
  NS_IMETHOD GetHAlign(Halignment& aAlign);
  NS_IMETHOD GetInset(nsMargin& aInset);
  NS_IMETHOD GetOrientation(PRBool& aIsHorizontal);
  NS_IMETHOD GetDirection(PRBool& aIsNormal);

  NS_IMETHOD GetPrefSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetMinSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetMaxSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetFlex(nsBoxLayoutState& aBoxLayoutState, nscoord& aFlex);
  NS_IMETHOD GetOrdinal(nsBoxLayoutState& aBoxLayoutState, PRUint32& aOrdinal);
  NS_IMETHOD GetAscent(nsBoxLayoutState& aBoxLayoutState, nscoord& aAscent);
  NS_IMETHOD IsCollapsed(nsBoxLayoutState& aBoxLayoutState, PRBool& aCollapsed);
  NS_IMETHOD Collapse(nsBoxLayoutState& aBoxLayoutState);
  NS_IMETHOD UnCollapse(nsBoxLayoutState& aBoxLayoutState);

  // do not redefine this. Either create a new layout manager or redefine DoLayout below.
  NS_IMETHOD Layout(nsBoxLayoutState& aBoxLayoutState);

  NS_IMETHOD Redraw(nsBoxLayoutState& aState, const nsRect* aRect = nsnull, PRBool aImmediate = PR_FALSE);
  NS_IMETHOD NeedsRecalc();
  NS_IMETHOD RelayoutDirtyChild(nsBoxLayoutState& aState, nsIBox* aChild);
  NS_IMETHOD RelayoutStyleChange(nsBoxLayoutState& aState, nsIBox* aChild);
  NS_IMETHOD RelayoutChildAtOrdinal(nsBoxLayoutState& aState, nsIBox* aChild);
  NS_IMETHOD GetMouseThrough(PRBool& aMouseThrough);

  NS_IMETHOD MarkChildrenStyleChange();
  NS_IMETHOD MarkStyleChange(nsBoxLayoutState& aState);
#ifdef DEBUG_LAYOUT
  NS_IMETHOD GetDebugBoxAt(const nsPoint& aPoint, nsIBox** aBox);
  NS_IMETHOD GetDebug(PRBool& aDebug);
  NS_IMETHOD SetDebug(nsBoxLayoutState& aState, PRBool aDebug);

  NS_IMETHOD DumpBox(FILE* out);
#endif
  NS_IMETHOD ChildrenMustHaveWidgets(PRBool& aMust);
  NS_IMETHOD GetIndexOf(nsIBox* aChild, PRInt32* aIndex);

  nsBox(nsIPresShell* aShell);
  virtual ~nsBox();

  /**
   * Returns PR_TRUE if this box clips its children, e.g., if this box is an scrollbox.
   */
  virtual PRBool DoesClipChildren() { return PR_FALSE; }

  virtual nsresult SyncLayout(nsBoxLayoutState& aBoxLayoutState);

  virtual PRBool DoesNeedRecalc(const nsSize& aSize);
  virtual PRBool DoesNeedRecalc(nscoord aCoord);
  virtual void SizeNeedsRecalc(nsSize& aSize);
  virtual void CoordNeedsRecalc(nscoord& aCoord);

  virtual void AddBorderAndPadding(nsSize& aSize);
  virtual void AddInset(nsSize& aSize);
  virtual void AddMargin(nsSize& aSize);

  static void AddBorderAndPadding(nsIBox* aBox, nsSize& aSize);
  static void AddInset(nsIBox* aBox, nsSize& aSize);
  static void AddMargin(nsIBox* aChild, nsSize& aSize);
  static void AddMargin(nsSize& aSize, const nsMargin& aMargin);

  static nsresult UnCollapseChild(nsBoxLayoutState& aState, nsIBox* aFrame);
  static nsresult CollapseChild(nsBoxLayoutState& aState, nsIFrame* aFrame, PRBool aHide);

  static void BoundsCheck(nsSize& aMinSize, nsSize& aPrefSize, nsSize& aMaxSize);
  static void BoundsCheck(nscoord& aMinSize, nscoord& aPrefSize, nscoord& aMaxSize);

protected:

#ifdef DEBUG_LAYOUT
  virtual void AppendAttribute(const nsAutoString& aAttribute, const nsAutoString& aValue, nsAutoString& aResult);

  virtual void ListBox(nsAutoString& aResult);
#endif
  
  virtual PRBool HasStyleChange();
  virtual void SetStyleChangeFlag(PRBool aDirty);

  virtual PRBool GetWasCollapsed(nsBoxLayoutState& aState);
  virtual void SetWasCollapsed(nsBoxLayoutState& aState, PRBool aWas);
  virtual PRBool GetDefaultFlex(PRInt32& aFlex);
  virtual void GetLayoutFlags(PRUint32& aFlags);

  NS_IMETHOD BeginLayout(nsBoxLayoutState& aState);
  NS_IMETHOD DoLayout(nsBoxLayoutState& aBoxLayoutState);
  NS_IMETHOD EndLayout(nsBoxLayoutState& aState);

#ifdef DEBUG_LAYOUT
  virtual void GetBoxName(nsAutoString& aName);
#endif

  static PRBool gGotTheme;
  static nsITheme* gTheme;

  enum eMouseThrough {
    unset,
    never,
    always
  };

  eMouseThrough mMouseThrough;
private:

  nsIBox* mNextChild;
  nsIBox* mParentBox;
  //nscoord mX;
  //nscoord mY;
};

#ifdef DEBUG_LAYOUT
#define NS_BOX_ASSERTION(box,expr,str) \
  if (!(expr)) { \
       box->DumpBox(stdout); \
       nsDebug::Assertion(str, #expr, __FILE__, __LINE__); \
  }
#else
#define NS_BOX_ASSERTION(box,expr,str) {}
#endif

#endif

