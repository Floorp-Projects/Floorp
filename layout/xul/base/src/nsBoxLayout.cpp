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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsBox.h"
#include "nsCOMPtr.h"
#include "nsContainerFrame.h"
#include "nsBoxLayout.h"

void
nsBoxLayout::AddBorderAndPadding(nsIBox* aBox, nsSize& aSize)
{
  nsBox::AddBorderAndPadding(aBox, aSize);
}

void
nsBoxLayout::AddMargin(nsIBox* aBox, nsSize& aSize)
{
  nsBox::AddMargin(aBox, aSize);
}

void
nsBoxLayout::AddMargin(nsSize& aSize, const nsMargin& aMargin)
{
  nsBox::AddMargin(aSize, aMargin);
}

nsSize
nsBoxLayout::GetPrefSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState)
{
  nsSize pref (0, 0);
  AddBorderAndPadding(aBox, pref);

  return pref;
}

nsSize
nsBoxLayout::GetMinSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState)
{
  nsSize minSize (0,0);
  AddBorderAndPadding(aBox, minSize);
  return minSize;
}

nsSize
nsBoxLayout::GetMaxSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState)
{
  //AddBorderAndPadding () never changes maxSize (NS_INTRINSICSIZE)
  //AddBorderAndPadding(aBox, maxSize);
  return nsSize (NS_INTRINSICSIZE,NS_INTRINSICSIZE);
}


nscoord
nsBoxLayout::GetAscent(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState)
{
  return 0;
}

NS_IMETHODIMP
nsBoxLayout::Layout(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState)
{
  return NS_OK;
}

void
nsBoxLayout::AddLargestSize(nsSize& aSize, const nsSize& aSize2)
{
  if (aSize2.width > aSize.width)
     aSize.width = aSize2.width;

  if (aSize2.height > aSize.height)
     aSize.height = aSize2.height;
}

void
nsBoxLayout::AddSmallestSize(nsSize& aSize, const nsSize& aSize2)
{
  if (aSize2.width < aSize.width)
     aSize.width = aSize2.width;

  if (aSize2.height < aSize.height)
     aSize.height = aSize2.height;
}

NS_IMPL_ISUPPORTS1(nsBoxLayout, nsBoxLayout)
