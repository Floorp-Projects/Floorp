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

  virtual nsSize GetPrefSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetMinSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetMaxSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nscoord GetFlex() override;
  virtual nscoord GetBoxAscent(nsBoxLayoutState& aBoxLayoutState) override;

  virtual nsSize GetMinSizeForScrollArea(nsBoxLayoutState& aBoxLayoutState) override;

  virtual bool IsCollapsed() override;

  virtual void SetBounds(nsBoxLayoutState& aBoxLayoutState, const nsRect& aRect,
                         bool aRemoveOverflowAreas = false) override;

  virtual nsresult GetBorder(nsMargin& aBorderAndPadding) override;
  virtual nsresult GetPadding(nsMargin& aBorderAndPadding) override;
  virtual nsresult GetMargin(nsMargin& aMargin) override;

  virtual Valignment GetVAlign() const override { return vAlign_Top; }
  virtual Halignment GetHAlign() const override { return hAlign_Left; }

  virtual nsresult RelayoutChildAtOrdinal(nsIFrame* aChild) override;

#ifdef DEBUG_LAYOUT
  NS_IMETHOD GetDebugBoxAt(const nsPoint& aPoint, nsIFrame** aBox);
  virtual nsresult GetDebug(bool& aDebug) override;
  virtual nsresult SetDebug(nsBoxLayoutState& aState, bool aDebug) override;

  virtual nsresult DumpBox(FILE* out) override;
  void PropagateDebug(nsBoxLayoutState& aState);
#endif

  nsBox();
  virtual ~nsBox();

  /**
   * Returns true if this box clips its children, e.g., if this box is an sc
rollbox.
  */
  virtual bool DoesClipChildren();
  virtual bool ComputesOwnOverflowArea() = 0;

  nsresult SyncLayout(nsBoxLayoutState& aBoxLayoutState);

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

  static nsIFrame* GetChildBox(const nsIFrame* aFrame);
  static nsIFrame* GetNextBox(const nsIFrame* aFrame);
  static nsIFrame* GetParentBox(const nsIFrame* aFrame);

protected:

#ifdef DEBUG_LAYOUT
  virtual void AppendAttribute(const nsAutoString& aAttribute, const nsAutoString& aValue, nsAutoString& aResult);

  virtual void ListBox(nsAutoString& aResult);
#endif
  
  virtual void GetLayoutFlags(uint32_t& aFlags);

  nsresult BeginLayout(nsBoxLayoutState& aState);
  NS_IMETHOD DoLayout(nsBoxLayoutState& aBoxLayoutState);
  nsresult EndLayout(nsBoxLayoutState& aState);

#ifdef DEBUG_LAYOUT
  virtual void GetBoxName(nsAutoString& aName);
  void PropagateDebug(nsBoxLayoutState& aState);
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

