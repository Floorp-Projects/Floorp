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

#include "nsIFrame.h"

class nsITheme;

#define NS_STATE_IS_ROOT        NS_FRAME_STATE_BIT(24)
#define NS_STATE_SET_TO_DEBUG   NS_FRAME_STATE_BIT(26)
#define NS_STATE_DEBUG_WAS_SET  NS_FRAME_STATE_BIT(27)

class nsBox : public nsIFrame {

public:

  friend class nsIFrame;

  static void Shutdown();

  virtual nsSize GetPrefSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetMinSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetMaxSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nscoord GetFlex(nsBoxLayoutState& aBoxLayoutState);
  virtual nscoord GetBoxAscent(nsBoxLayoutState& aBoxLayoutState);

  virtual nsSize GetMinSizeForScrollArea(nsBoxLayoutState& aBoxLayoutState);

  virtual bool IsCollapsed(nsBoxLayoutState& aBoxLayoutState);

  virtual void SetBounds(nsBoxLayoutState& aBoxLayoutState, const nsRect& aRect,
                         bool aRemoveOverflowAreas = false);

  NS_IMETHOD GetBorder(nsMargin& aBorderAndPadding);
  NS_IMETHOD GetPadding(nsMargin& aBorderAndPadding);
  NS_IMETHOD GetMargin(nsMargin& aMargin);

  virtual Valignment GetVAlign() const { return vAlign_Top; }
  virtual Halignment GetHAlign() const { return hAlign_Left; }

  NS_IMETHOD RelayoutChildAtOrdinal(nsBoxLayoutState& aState, nsIBox* aChild);

#ifdef DEBUG_LAYOUT
  NS_IMETHOD GetDebugBoxAt(const nsPoint& aPoint, nsIBox** aBox);
  NS_IMETHOD GetDebug(bool& aDebug);
  NS_IMETHOD SetDebug(nsBoxLayoutState& aState, bool aDebug);

  NS_IMETHOD DumpBox(FILE* out);
  NS_HIDDEN_(void) PropagateDebug(nsBoxLayoutState& aState);
#endif

  nsBox();
  virtual ~nsBox();

  /**
   * Returns PR_TRUE if this box clips its children, e.g., if this box is an sc
rollbox.
  */
  virtual bool DoesClipChildren();
  virtual bool ComputesOwnOverflowArea() = 0;

  NS_HIDDEN_(nsresult) SyncLayout(nsBoxLayoutState& aBoxLayoutState);

  bool DoesNeedRecalc(const nsSize& aSize);
  bool DoesNeedRecalc(nscoord aCoord);
  void SizeNeedsRecalc(nsSize& aSize);
  void CoordNeedsRecalc(nscoord& aCoord);

  void AddBorderAndPadding(nsSize& aSize);

  static void AddBorderAndPadding(nsIBox* aBox, nsSize& aSize);
  static void AddMargin(nsIBox* aChild, nsSize& aSize);
  static void AddMargin(nsSize& aSize, const nsMargin& aMargin);

  static nsSize BoundsCheckMinMax(const nsSize& aMinSize, const nsSize& aMaxSize);
  static nsSize BoundsCheck(const nsSize& aMinSize, const nsSize& aPrefSize, const nsSize& aMaxSize);
  static nscoord BoundsCheck(nscoord aMinSize, nscoord aPrefSize, nscoord aMaxSize);

protected:

#ifdef DEBUG_LAYOUT
  virtual void AppendAttribute(const nsAutoString& aAttribute, const nsAutoString& aValue, nsAutoString& aResult);

  virtual void ListBox(nsAutoString& aResult);
#endif
  
  virtual void GetLayoutFlags(PRUint32& aFlags);

  NS_HIDDEN_(nsresult) BeginLayout(nsBoxLayoutState& aState);
  NS_IMETHOD DoLayout(nsBoxLayoutState& aBoxLayoutState);
  NS_HIDDEN_(nsresult) EndLayout(nsBoxLayoutState& aState);

#ifdef DEBUG_LAYOUT
  virtual void GetBoxName(nsAutoString& aName);
  NS_HIDDEN_(void) PropagateDebug(nsBoxLayoutState& aState);
#endif

  static bool gGotTheme;
  static nsITheme* gTheme;

  enum eMouseThrough {
    unset,
    never,
    always
  };

private:

  //nscoord mX;
  //nscoord mY;
};

#ifdef DEBUG_LAYOUT
#define NS_BOX_ASSERTION(box,expr,str) \
  if (!(expr)) { \
       box->DumpBox(stdout); \
       NS_DebugBreak(NSDebugAssertion, str, #expr, __FILE__, __LINE__); \
  }
#else
#define NS_BOX_ASSERTION(box,expr,str) {}
#endif

#endif

