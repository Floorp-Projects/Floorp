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

  virtual nsSize GetXULPrefSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetXULMinSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetXULMaxSize(nsBoxLayoutState& aBoxLayoutState) override;
  virtual nscoord GetXULFlex() override;
  virtual nscoord GetXULBoxAscent(nsBoxLayoutState& aBoxLayoutState) override;

  virtual nsSize GetXULMinSizeForScrollArea(nsBoxLayoutState& aBoxLayoutState) override;

  virtual bool IsXULCollapsed() override;

  virtual void SetXULBounds(nsBoxLayoutState& aBoxLayoutState, const nsRect& aRect,
                            bool aRemoveOverflowAreas = false) override;

  virtual nsresult GetXULBorder(nsMargin& aBorderAndPadding) override;
  virtual nsresult GetXULPadding(nsMargin& aBorderAndPadding) override;
  virtual nsresult GetXULMargin(nsMargin& aMargin) override;

  virtual Valignment GetXULVAlign() const override { return vAlign_Top; }
  virtual Halignment GetXULHAlign() const override { return hAlign_Left; }

  virtual nsresult XULRelayoutChildAtOrdinal(nsIFrame* aChild) override;

#ifdef DEBUG_LAYOUT
  NS_IMETHOD GetDebugBoxAt(const nsPoint& aPoint, nsIFrame** aBox);
  virtual nsresult GetXULDebug(bool& aDebug) override;
  virtual nsresult SetXULDebug(nsBoxLayoutState& aState, bool aDebug) override;

  virtual nsresult XULDumpBox(FILE* out) override;
  void PropagateDebug(nsBoxLayoutState& aState);
#endif

  explicit nsBox(mozilla::FrameType);
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

  static nsIFrame* GetChildXULBox(const nsIFrame* aFrame);
  static nsIFrame* GetNextXULBox(const nsIFrame* aFrame);
  static nsIFrame* GetParentXULBox(const nsIFrame* aFrame);

protected:

#ifdef DEBUG_LAYOUT
  virtual void AppendAttribute(const nsAutoString& aAttribute, const nsAutoString& aValue, nsAutoString& aResult);

  virtual void ListBox(nsAutoString& aResult);
#endif

  nsresult BeginXULLayout(nsBoxLayoutState& aState);
  NS_IMETHOD DoXULLayout(nsBoxLayoutState& aBoxLayoutState);
  nsresult EndXULLayout(nsBoxLayoutState& aState);

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
       box->XULDumpBox(stdout); \
       NS_DebugBreak(NSDebugAssertion, str, #expr, __FILE__, __LINE__); \
  }
#else
#define NS_BOX_ASSERTION(box,expr,str) {}
#endif

#endif

