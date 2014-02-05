/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBox_h___
#define nsBox_h___

#include "mozilla/Attributes.h"
#include "nsIFrame.h"

class nsITheme;

class nsBox : public nsIFrame {

public:

  friend class nsIFrame;

  static void Shutdown();

  virtual nsSize GetPrefSize(nsBoxLayoutState& aBoxLayoutState) MOZ_OVERRIDE;
  virtual nsSize GetMinSize(nsBoxLayoutState& aBoxLayoutState) MOZ_OVERRIDE;
  virtual nsSize GetMaxSize(nsBoxLayoutState& aBoxLayoutState) MOZ_OVERRIDE;
  virtual nscoord GetFlex(nsBoxLayoutState& aBoxLayoutState) MOZ_OVERRIDE;
  virtual nscoord GetBoxAscent(nsBoxLayoutState& aBoxLayoutState) MOZ_OVERRIDE;

  virtual nsSize GetMinSizeForScrollArea(nsBoxLayoutState& aBoxLayoutState) MOZ_OVERRIDE;

  virtual bool IsCollapsed() MOZ_OVERRIDE;

  virtual void SetBounds(nsBoxLayoutState& aBoxLayoutState, const nsRect& aRect,
                         bool aRemoveOverflowAreas = false) MOZ_OVERRIDE;

  NS_IMETHOD GetBorder(nsMargin& aBorderAndPadding) MOZ_OVERRIDE;
  NS_IMETHOD GetPadding(nsMargin& aBorderAndPadding) MOZ_OVERRIDE;
  NS_IMETHOD GetMargin(nsMargin& aMargin) MOZ_OVERRIDE;

  virtual Valignment GetVAlign() const MOZ_OVERRIDE { return vAlign_Top; }
  virtual Halignment GetHAlign() const MOZ_OVERRIDE { return hAlign_Left; }

  NS_IMETHOD RelayoutChildAtOrdinal(nsBoxLayoutState& aState, nsIFrame* aChild) MOZ_OVERRIDE;

#ifdef DEBUG_LAYOUT
  NS_IMETHOD GetDebugBoxAt(const nsPoint& aPoint, nsIFrame** aBox);
  NS_IMETHOD GetDebug(bool& aDebug) MOZ_OVERRIDE;
  NS_IMETHOD SetDebug(nsBoxLayoutState& aState, bool aDebug) MOZ_OVERRIDE;

  NS_IMETHOD DumpBox(FILE* out) MOZ_OVERRIDE;
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
  
  virtual void GetLayoutFlags(uint32_t& aFlags);

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

