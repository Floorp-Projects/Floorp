/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

  virtual bool IsCollapsed();

  virtual void SetBounds(nsBoxLayoutState& aBoxLayoutState, const nsRect& aRect,
                         bool aRemoveOverflowAreas = false);

  NS_IMETHOD GetBorder(nsMargin& aBorderAndPadding);
  NS_IMETHOD GetPadding(nsMargin& aBorderAndPadding);
  NS_IMETHOD GetMargin(nsMargin& aMargin);

  virtual Valignment GetVAlign() const { return vAlign_Top; }
  virtual Halignment GetHAlign() const { return hAlign_Left; }

  NS_IMETHOD RelayoutChildAtOrdinal(nsBoxLayoutState& aState, nsIFrame* aChild);

#ifdef DEBUG_LAYOUT
  NS_IMETHOD GetDebugBoxAt(const nsPoint& aPoint, nsIFrame** aBox);
  NS_IMETHOD GetDebug(bool& aDebug);
  NS_IMETHOD SetDebug(nsBoxLayoutState& aState, bool aDebug);

  NS_IMETHOD DumpBox(FILE* out);
  NS_HIDDEN_(void) PropagateDebug(nsBoxLayoutState& aState);
#endif

  nsBox();
  virtual ~nsBox();

  /**
   * Returns true if this box clips its children, e.g., if this box is an sc
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

  static void AddBorderAndPadding(nsIFrame* aBox, nsSize& aSize);
  static void AddMargin(nsIFrame* aChild, nsSize& aSize);
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

