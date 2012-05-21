/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsGridRow.h"
#include "nsIFrame.h"
#include "nsBoxLayoutState.h"

nsGridRow::nsGridRow():mIsBogus(false),
                       mBox(nsnull), 
                       mFlex(-1),
                       mPref(-1),
                       mMin(-1),
                       mMax(-1),
                       mTop(-1),
                       mBottom(-1), 
                       mTopMargin(0),
                       mBottomMargin(0)

{
    MOZ_COUNT_CTOR(nsGridRow);
}

void
nsGridRow::Init(nsIBox* aBox, bool aIsBogus)
{
  mBox = aBox;
  mIsBogus = aIsBogus;
  mFlex = -1;
  mPref = -1;
  mMin = -1;
  mMax = -1;
  mTop = -1;
  mBottom = -1;
  mTopMargin = 0;
  mBottomMargin = 0;
}

nsGridRow::~nsGridRow()
{
   MOZ_COUNT_DTOR(nsGridRow);
}

bool 
nsGridRow::IsCollapsed()
{
  return mBox && mBox->IsCollapsed();
}

