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

#ifndef nsSprocketLayout_h___
#define nsSprocketLayout_h___

#include "nsBoxLayout.h"
#include "nsCOMPtr.h"

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

nsresult NS_NewSprocketLayout(nsIPresShell* aPresShell, nsCOMPtr<nsBoxLayout>& aNewLayout);

class nsSprocketLayout : public nsBoxLayout {

public:

  friend nsresult NS_NewSprocketLayout(nsIPresShell* aPresShell, nsCOMPtr<nsBoxLayout>& aNewLayout);
  static void Shutdown();

  NS_IMETHOD Layout(nsIBox* aBox, nsBoxLayoutState& aState);

  virtual nsSize GetPrefSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetMinSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetMaxSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState);
  virtual nscoord GetAscent(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState);

  nsSprocketLayout();

  static bool IsHorizontal(nsIBox* aBox);

  static void SetLargestSize(nsSize& aSize1, const nsSize& aSize2, bool aIsHorizontal);
  static void SetSmallestSize(nsSize& aSize1, const nsSize& aSize2, bool aIsHorizontal);

  static void AddLargestSize(nsSize& aSize, const nsSize& aSizeToAdd, bool aIsHorizontal);
  static void AddSmallestSize(nsSize& aSize, const nsSize& aSizeToAdd, bool aIsHorizontal);
  static void AddCoord(nscoord& aCoord, nscoord aCoordToAdd);

protected:


  void ComputeChildsNextPosition(nsIBox* aBox,
                                 const nscoord& aCurX, 
                                 const nscoord& aCurY, 
                                 nscoord& aNextX, 
                                 nscoord& aNextY, 
                                 const nsRect& aChildSize);

  void ChildResized(nsIBox* aBox,
                    nsBoxLayoutState& aState, 
                    nsIBox* aChild,
                    nsBoxSize* aChildBoxSize, 
                    nsComputedBoxSize* aChildComputedBoxSize, 
                    nsBoxSize* aBoxSizes, 
                    nsComputedBoxSize* aComputedBoxSizes, 
                    const nsRect& aChildLayoutRect, 
                    nsRect& aChildActualRect, 
                    nsRect& aContainingRect, 
                    PRInt32 aFlexes, 
                    bool& aFinished);

  void AlignChildren(nsIBox* aBox,
                     nsBoxLayoutState& aState,
                     bool* aNeedsRedraw);

  virtual void ComputeChildSizes(nsIBox* aBox, 
                         nsBoxLayoutState& aState, 
                         nscoord& aGivenSize, 
                         nsBoxSize* aBoxSizes, 
                         nsComputedBoxSize*& aComputedBoxSizes);


  virtual void PopulateBoxSizes(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsBoxSize*& aBoxSizes, nscoord& aMinSize, nscoord& aMaxSize, PRInt32& aFlexes);

  virtual void InvalidateComputedSizes(nsComputedBoxSize* aComputedBoxSizes);

  virtual bool GetDefaultFlex(PRInt32& aFlex);

  virtual void GetFrameState(nsIBox* aBox, nsFrameState& aState);

private:


  // because the sprocket layout manager has no instance variables. We 
  // can make a static one and reuse it everywhere.
  static nsBoxLayout* gInstance;

};

#endif

