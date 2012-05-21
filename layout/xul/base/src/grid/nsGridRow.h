/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 
  Author:
  Eric D Vaughan

**/

#ifndef nsGridRow_h___
#define nsGridRow_h___

#include "nsIFrame.h"

class nsGridLayout2;
class nsBoxLayoutState;

/**
 * The row (or column) data structure in the grid cellmap.
 */
class nsGridRow
{
public:
   nsGridRow();
   ~nsGridRow();
   
   void Init(nsIBox* aBox, bool aIsBogus);

// accessors
   nsIBox* GetBox()   { return mBox;          }
   bool IsPrefSet() { return (mPref != -1); }
   bool IsMinSet()  { return (mMin  != -1); }
   bool IsMaxSet()  { return (mMax  != -1); } 
   bool IsFlexSet() { return (mFlex != -1); }
   bool IsOffsetSet() { return (mTop != -1 && mBottom != -1); }
   bool IsCollapsed();

public:

   bool    mIsBogus;
   nsIBox* mBox;
   nscoord mFlex;
   nscoord mPref;
   nscoord mMin;
   nscoord mMax;
   nscoord mTop;
   nscoord mBottom;
   nscoord mTopMargin;
   nscoord mBottomMargin;

};


#endif

