/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
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
  nscoord ascent;
  nscoord flex;
  nscoord left;
  nscoord right;
  PRBool  collapsed;
  PRBool  bogus;

  nsBoxSize* next;

  void Clear();
  void Add(const nsSize& minSize, 
           const nsSize& prefSize,
           const nsSize& maxSize,
           nscoord ascent,
           nscoord flex,
           PRBool aIsHorizontal);

  void Add(const nsMargin& aMargin, PRBool aIsHorizontal);
  void* operator new(size_t sz, nsBoxLayoutState& aState);
  void operator delete(void* aPtr, size_t sz);

};

class nsComputedBoxSize
{
public:
  nsComputedBoxSize();

  nscoord size;
  PRBool  valid;
  PRBool  resized;
  nsComputedBoxSize* next;

  void Clear();
  void* operator new(size_t sz, nsBoxLayoutState& aState);
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

class nsSprocketLayout : public nsBoxLayout {

public:

  friend nsresult NS_NewSprocketLayout(nsIPresShell* aPresShell, nsCOMPtr<nsIBoxLayout>& aNewLayout);
  static void Shutdown();

  NS_IMETHOD Layout(nsIBox* aBox, nsBoxLayoutState& aState);

  NS_IMETHOD GetPrefSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetMinSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetMaxSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetFlex(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nscoord& aFlex);
  NS_IMETHOD GetAscent(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nscoord& aAscent);
  NS_IMETHOD IsCollapsed(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, PRBool& aCollapsed);

  nsSprocketLayout();

  static PRBool IsHorizontal(nsIBox* aBox);

  static void SetLargestSize(nsSize& aSize1, const nsSize& aSize2, PRBool aIsHorizontal);
  static void SetSmallestSize(nsSize& aSize1, const nsSize& aSize2, PRBool aIsHorizontal);

  static void AddLargestSize(nsSize& aSize, const nsSize& aSizeToAdd, PRBool aIsHorizontal);
  static void AddSmallestSize(nsSize& aSize, const nsSize& aSizeToAdd, PRBool aIsHorizontal);
  static void AddCoord(nscoord& aCoord, nscoord aCoordToAdd);

protected:


  virtual void ComputeChildsNextPosition(nsIBox* aBox,
                                         nsIBox* aChild, 
                                         nscoord& aCurX, 
                                         nscoord& aCurY, 
                                         nscoord& aNextX, 
                                         nscoord& aNextY, 
                                         const nsRect& aChildSize, 
                                         const nsRect& aContainingRect,
                                         nscoord childAscent,
                                         nscoord aMaxAscent);

  virtual void ChildResized(nsIBox* aBox,
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
                           PRBool& aFinished);

  virtual void ComputeChildSizes(nsIBox* aBox, 
                         nsBoxLayoutState& aState, 
                         nscoord& aGivenSize, 
                         nsBoxSize* aBoxSizes, 
                         nsComputedBoxSize*& aComputedBoxSizes);


  virtual void PopulateBoxSizes(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsBoxSize*& aBoxSizes, nsComputedBoxSize*& aComputedBoxSizes, nscoord& aMinSize, nscoord& aMaxSize, PRInt32& aFlexes);

  virtual void InvalidateComputedSizes(nsComputedBoxSize* aComputedBoxSizes);

  virtual PRBool GetDefaultFlex(PRInt32& aFlex);

  virtual void GetFrameState(nsIBox* aBox, nsFrameState& aState);
  virtual void SetFrameState(nsIBox* aBox, nsFrameState aState);

private:


  // because the sprocket layout manager has no instance variables. We 
  // can make a static on and reuse it everywhere.
  static nsIBoxLayout* gInstance;

};

#endif

