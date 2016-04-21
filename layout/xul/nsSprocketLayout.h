/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSprocketLayout_h___
#define nsSprocketLayout_h___

#include "mozilla/Attributes.h"
#include "nsBoxLayout.h"
#include "nsCOMPtr.h"
#include "nsIFrame.h"

class nsBoxSize
{
public:

  nsBoxSize();

  nscoord pref;
  nscoord min;
  nscoord max;
  nscoord flex;
  nscoord left;
  nscoord right;
  bool    collapsed;
  bool    bogus;

  nsBoxSize* next;

  void* operator new(size_t sz, nsBoxLayoutState& aState) CPP_THROW_NEW;
  void operator delete(void* aPtr, size_t sz);
};

class nsComputedBoxSize
{
public:
  nsComputedBoxSize();

  nscoord size;
  bool    valid;
  bool    resized;
  nsComputedBoxSize* next;

  void* operator new(size_t sz, nsBoxLayoutState& aState) CPP_THROW_NEW;
  void operator delete(void* aPtr, size_t sz);
};

#define GET_WIDTH(size, isHorizontal) (isHorizontal ? size.width : size.height)
#define GET_HEIGHT(size, isHorizontal) (isHorizontal ? size.height : size.width)
#define GET_X(size, isHorizontal) (isHorizontal ? size.x : size.y)
#define GET_Y(size, isHorizontal) (isHorizontal ? size.y : size.x)
#define GET_COORD(aX, aY, isHorizontal) (isHorizontal ? aX : aY)

#define SET_WIDTH(size, coord, isHorizontal)  if (isHorizontal) { (size).width  = (coord); } else { (size).height = (coord); }
#define SET_HEIGHT(size, coord, isHorizontal) if (isHorizontal) { (size).height = (coord); } else { (size).width  = (coord); }
#define SET_X(size, coord, isHorizontal) if (isHorizontal) { (size).x = (coord); } else { (size).y  = (coord); }
#define SET_Y(size, coord, isHorizontal) if (isHorizontal) { (size).y = (coord); } else { (size).x  = (coord); }

#define SET_COORD(aX, aY, coord, isHorizontal) if (isHorizontal) { aX = (coord); } else { aY  = (coord); }

nsresult NS_NewSprocketLayout(nsCOMPtr<nsBoxLayout>& aNewLayout);

class nsSprocketLayout : public nsBoxLayout {

public:

  friend nsresult NS_NewSprocketLayout(nsCOMPtr<nsBoxLayout>& aNewLayout);
  static void Shutdown();

  NS_IMETHOD XULLayout(nsIFrame* aBox, nsBoxLayoutState& aState) override;

  virtual nsSize GetXULPrefSize(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetXULMinSize(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetXULMaxSize(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState) override;
  virtual nscoord GetAscent(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState) override;

  nsSprocketLayout();

  static bool IsXULHorizontal(nsIFrame* aBox);

  static void SetLargestSize(nsSize& aSize1, const nsSize& aSize2, bool aIsHorizontal);
  static void SetSmallestSize(nsSize& aSize1, const nsSize& aSize2, bool aIsHorizontal);

  static void AddLargestSize(nsSize& aSize, const nsSize& aSizeToAdd, bool aIsHorizontal);
  static void AddSmallestSize(nsSize& aSize, const nsSize& aSizeToAdd, bool aIsHorizontal);
  static void AddCoord(nscoord& aCoord, nscoord aCoordToAdd);

protected:


  void ComputeChildsNextPosition(nsIFrame* aBox,
                                 const nscoord& aCurX, 
                                 const nscoord& aCurY, 
                                 nscoord& aNextX, 
                                 nscoord& aNextY, 
                                 const nsRect& aChildSize);

  void ChildResized(nsIFrame* aBox,
                    nsBoxLayoutState& aState, 
                    nsIFrame* aChild,
                    nsBoxSize* aChildBoxSize, 
                    nsComputedBoxSize* aChildComputedBoxSize, 
                    nsBoxSize* aBoxSizes, 
                    nsComputedBoxSize* aComputedBoxSizes, 
                    const nsRect& aChildLayoutRect, 
                    nsRect& aChildActualRect, 
                    nsRect& aContainingRect, 
                    int32_t aFlexes, 
                    bool& aFinished);

  void AlignChildren(nsIFrame* aBox,
                     nsBoxLayoutState& aState);

  virtual void ComputeChildSizes(nsIFrame* aBox, 
                         nsBoxLayoutState& aState, 
                         nscoord& aGivenSize, 
                         nsBoxSize* aBoxSizes, 
                         nsComputedBoxSize*& aComputedBoxSizes);


  virtual void PopulateBoxSizes(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState,
                                nsBoxSize*& aBoxSizes, nscoord& aMinSize,
                                nscoord& aMaxSize, int32_t& aFlexes);

  virtual void InvalidateComputedSizes(nsComputedBoxSize* aComputedBoxSizes);

  virtual bool GetDefaultFlex(int32_t& aFlex);

  virtual void GetFrameState(nsIFrame* aBox, nsFrameState& aState);

private:


  // because the sprocket layout manager has no instance variables. We 
  // can make a static one and reuse it everywhere.
  static nsBoxLayout* gInstance;

};

#endif

