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

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsGridCell.h"
#include "nsFrame.h"
#include "nsIBox.h"
#include "nsBoxLayout.h"
#include "nsStackLayout.h"


nsGridCell::nsGridCell():mBoxInRow(nsnull),mBoxInColumn(nsnull)
{
    MOZ_COUNT_CTOR(nsGridCell);
}                                               
        
nsGridCell::~nsGridCell()
{
    MOZ_COUNT_DTOR(nsGridCell);
}

nsresult
nsGridCell::GetPrefSize(nsBoxLayoutState& aState, nsSize& aPref)
{
  aPref.width = 0;
  aPref.height = 0;

  // take ours 2 children and add them up.
  // we are as wide as the widest child plus its left offset
  // we are tall as the tallest child plus its top offset

  nsSize pref(0,0);

  if (mBoxInColumn) {
    mBoxInColumn->GetPrefSize(aState, pref);      
    nsBoxLayout::AddMargin(mBoxInColumn, pref);
    nsStackLayout::AddOffset(aState, mBoxInColumn, pref);

    nsBoxLayout::AddLargestSize(aPref, pref);
  }

  if (mBoxInRow) {
    mBoxInRow->GetPrefSize(aState, pref);

    nsBoxLayout::AddMargin(mBoxInRow, pref);
    nsStackLayout::AddOffset(aState, mBoxInRow, pref);

    nsBoxLayout::AddLargestSize(aPref, pref);
  }

  return NS_OK;
}

nsresult
nsGridCell::GetMinSize(nsBoxLayoutState& aState, nsSize& aMin)
{
  aMin.width = 0;
  aMin.height = 0;

  // take ours 2 children and add them up.
  // we are as wide as the widest child plus its left offset
  // we are tall as the tallest child plus its top offset

  nsSize min(0,0);

  if (mBoxInColumn) {
    mBoxInColumn->GetMinSize(aState, min);      

    nsBoxLayout::AddMargin(mBoxInColumn, min);
    nsStackLayout::AddOffset(aState, mBoxInColumn, min);

    nsBoxLayout::AddLargestSize(aMin, min);
  }

  if (mBoxInRow) {
    mBoxInRow->GetMinSize(aState, min);

    nsBoxLayout::AddMargin(mBoxInRow, min);
    nsStackLayout::AddOffset(aState, mBoxInRow, min);

    nsBoxLayout::AddLargestSize(aMin, min);
  }

  return NS_OK;
}

nsresult
nsGridCell::GetMaxSize(nsBoxLayoutState& aState, nsSize& aMax)
{
  aMax.width = NS_INTRINSICSIZE;
  aMax.height = NS_INTRINSICSIZE;

  // take ours 2 children and add them up.
  // we are as wide as the widest child plus its left offset
  // we are tall as the tallest child plus its top offset

  nsSize max(NS_INTRINSICSIZE,NS_INTRINSICSIZE);

  if (mBoxInColumn) {
    mBoxInColumn->GetMaxSize(aState, max);      

 
    nsBoxLayout::AddMargin(mBoxInColumn, max);
    nsStackLayout::AddOffset(aState, mBoxInColumn, max);

    nsBoxLayout::AddSmallestSize(aMax, max);
  }

  if (mBoxInRow) {
    mBoxInRow->GetMaxSize(aState, max);

    nsBoxLayout::AddMargin(mBoxInRow, max);
    nsStackLayout::AddOffset(aState, mBoxInRow, max);

    nsBoxLayout::AddSmallestSize(aMax, max);
  }

  return NS_OK;
}


nsresult
nsGridCell::IsCollapsed(nsBoxLayoutState& aState, PRBool& aIsCollapsed)
{
  PRBool c1 = PR_FALSE, c2 = PR_FALSE;

  if (mBoxInColumn) 
    mBoxInColumn->IsCollapsed(aState, c1);

  if (mBoxInRow) 
    mBoxInRow->IsCollapsed(aState, c2);

  aIsCollapsed = (c1 || c2);

  return NS_OK;
}


