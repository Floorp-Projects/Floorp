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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Glazman <glazman@netscape.com>
 *   Mats Palmgren <mats.palmgren@bredband.net>
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

/*
 * temporary (expanded) representation of the property-value pairs
 * within a CSS declaration using during parsing and mutation, and
 * representation of complex values for CSS properties
 */

#include "nscore.h"
#include "nsCSSStruct.h"
#include "nsString.h"
#include "nsIAtom.h"
#include "nsUnicharUtils.h"
#include "nsCRT.h"
#include "nsCSSProps.h"
#include "nsFont.h"

#include "nsStyleConsts.h"

#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsPrintfCString.h"

// --- nsCSSFont -----------------

nsCSSFont::nsCSSFont(void)
{
  MOZ_COUNT_CTOR(nsCSSFont);
}

nsCSSFont::~nsCSSFont(void)
{
  MOZ_COUNT_DTOR(nsCSSFont);
}

// --- nsCSSValueList -----------------

nsCSSValueList::~nsCSSValueList()
{
  MOZ_COUNT_DTOR(nsCSSValueList);
  NS_CSS_DELETE_LIST_MEMBER(nsCSSValueList, this, mNext);
}

nsCSSValueList*
nsCSSValueList::Clone(PRBool aDeep) const
{
  nsCSSValueList* result = new nsCSSValueList(*this);
  if (NS_UNLIKELY(!result))
    return result;
  if (aDeep)
    NS_CSS_CLONE_LIST_MEMBER(nsCSSValueList, this, mNext, result, (PR_FALSE));
  return result;
}

/* static */ PRBool
nsCSSValueList::Equal(nsCSSValueList* aList1, nsCSSValueList* aList2)
{
  if (aList1 == aList2)
    return PR_TRUE;
    
  nsCSSValueList *p1 = aList1, *p2 = aList2;
  for ( ; p1 && p2; p1 = p1->mNext, p2 = p2->mNext) {
    if (p1->mValue != p2->mValue)
      return PR_FALSE;
  }
  return !p1 && !p2; // true if same length, false otherwise
}

// --- nsCSSColor -----------------

nsCSSColor::nsCSSColor(void)
{
  MOZ_COUNT_CTOR(nsCSSColor);
}

nsCSSColor::~nsCSSColor(void)
{
  MOZ_COUNT_DTOR(nsCSSColor);
}

// --- nsCSSText -----------------

nsCSSText::nsCSSText(void)
  : mTextShadow(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSText);
}

nsCSSText::~nsCSSText(void)
{
  MOZ_COUNT_DTOR(nsCSSText);
  delete mTextShadow;
}

// --- nsCSSRect -----------------

nsCSSRect::nsCSSRect(void)
{
  MOZ_COUNT_CTOR(nsCSSRect);
}

nsCSSRect::nsCSSRect(const nsCSSRect& aCopy)
  : mTop(aCopy.mTop),
    mRight(aCopy.mRight),
    mBottom(aCopy.mBottom),
    mLeft(aCopy.mLeft)
{
  MOZ_COUNT_CTOR(nsCSSRect);
}

nsCSSRect::~nsCSSRect()
{
  MOZ_COUNT_DTOR(nsCSSRect);
}

void nsCSSRect::SetAllSidesTo(const nsCSSValue& aValue)
{
  mTop = aValue;
  mRight = aValue;
  mBottom = aValue;
  mLeft = aValue;
}

#if (NS_SIDE_TOP != 0) || (NS_SIDE_RIGHT != 1) || (NS_SIDE_BOTTOM != 2) || (NS_SIDE_LEFT != 3)
#error "Somebody changed the side constants."
#endif

/* static */ const nsCSSRect::side_type nsCSSRect::sides[4] = {
  &nsCSSRect::mTop,
  &nsCSSRect::mRight,
  &nsCSSRect::mBottom,
  &nsCSSRect::mLeft,
};

// --- nsCSSCornerSizes -----------------

nsCSSCornerSizes::nsCSSCornerSizes(void)
{
  MOZ_COUNT_CTOR(nsCSSCornerSizes);
}

nsCSSCornerSizes::nsCSSCornerSizes(const nsCSSCornerSizes& aCopy)
  : mTopLeft(aCopy.mTopLeft),
    mTopRight(aCopy.mTopRight),
    mBottomRight(aCopy.mBottomRight),
    mBottomLeft(aCopy.mBottomLeft)
{
  MOZ_COUNT_CTOR(nsCSSCornerSizes);
}

nsCSSCornerSizes::~nsCSSCornerSizes()
{
  MOZ_COUNT_DTOR(nsCSSCornerSizes);
}

void
nsCSSCornerSizes::SetAllCornersTo(const nsCSSValue& aValue)
{
  NS_FOR_CSS_FULL_CORNERS(corner) {
    this->GetFullCorner(corner).SetBothValuesTo(aValue);
  }
}

void
nsCSSCornerSizes::Reset()
{
  NS_FOR_CSS_FULL_CORNERS(corner) {
    this->GetFullCorner(corner).Reset();
  }
}

#if NS_CORNER_TOP_LEFT != 0 || NS_CORNER_TOP_RIGHT != 1 || \
    NS_CORNER_BOTTOM_RIGHT != 2 || NS_CORNER_BOTTOM_LEFT != 3
#error "Somebody changed the corner constants."
#endif

/* static */ const nsCSSCornerSizes::corner_type
nsCSSCornerSizes::corners[4] = {
  &nsCSSCornerSizes::mTopLeft,
  &nsCSSCornerSizes::mTopRight,
  &nsCSSCornerSizes::mBottomRight,
  &nsCSSCornerSizes::mBottomLeft,
};

// --- nsCSSValueListRect -----------------

nsCSSValueListRect::nsCSSValueListRect(void)
  : mTop(nsnull),
    mRight(nsnull),
    mBottom(nsnull),
    mLeft(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSValueListRect);
}

nsCSSValueListRect::nsCSSValueListRect(const nsCSSValueListRect& aCopy)
  : mTop(aCopy.mTop),
    mRight(aCopy.mRight),
    mBottom(aCopy.mBottom),
    mLeft(aCopy.mLeft)
{
  MOZ_COUNT_CTOR(nsCSSValueListRect);
}

nsCSSValueListRect::~nsCSSValueListRect()
{
  MOZ_COUNT_DTOR(nsCSSValueListRect);
}

/* static */ const nsCSSValueListRect::side_type
nsCSSValueListRect::sides[4] = {
  &nsCSSValueListRect::mTop,
  &nsCSSValueListRect::mRight,
  &nsCSSValueListRect::mBottom,
  &nsCSSValueListRect::mLeft,
};

// --- nsCSSDisplay -----------------

/* During allocation, null-out the transform list. */
nsCSSDisplay::nsCSSDisplay(void) : mTransform(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSDisplay);
}

nsCSSDisplay::~nsCSSDisplay(void)
{
  MOZ_COUNT_DTOR(nsCSSDisplay);
}

// --- nsCSSMargin -----------------

nsCSSMargin::nsCSSMargin(void)
  : mBoxShadow(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSMargin);
}

nsCSSMargin::~nsCSSMargin(void)
{
  MOZ_COUNT_DTOR(nsCSSMargin);
  delete mBoxShadow;
}

// --- nsCSSPosition -----------------

nsCSSPosition::nsCSSPosition(void)
{
  MOZ_COUNT_CTOR(nsCSSPosition);
}

nsCSSPosition::~nsCSSPosition(void)
{
  MOZ_COUNT_DTOR(nsCSSPosition);
}

// --- nsCSSList -----------------

nsCSSList::nsCSSList(void)
{
  MOZ_COUNT_CTOR(nsCSSList);
}

nsCSSList::~nsCSSList(void)
{
  MOZ_COUNT_DTOR(nsCSSList);
}

// --- nsCSSTable -----------------

nsCSSTable::nsCSSTable(void)
{
  MOZ_COUNT_CTOR(nsCSSTable);
}

nsCSSTable::~nsCSSTable(void)
{
  MOZ_COUNT_DTOR(nsCSSTable);
}

// --- nsCSSBreaks -----------------

nsCSSBreaks::nsCSSBreaks(void)
{
  MOZ_COUNT_CTOR(nsCSSBreaks);
}

nsCSSBreaks::~nsCSSBreaks(void)
{
  MOZ_COUNT_DTOR(nsCSSBreaks);
}

// --- nsCSSPage -----------------

nsCSSPage::nsCSSPage(void)
{
  MOZ_COUNT_CTOR(nsCSSPage);
}

nsCSSPage::~nsCSSPage(void)
{
  MOZ_COUNT_DTOR(nsCSSPage);
}

// --- nsCSSContent support -----------------

nsCSSValuePairList::~nsCSSValuePairList()
{
  MOZ_COUNT_DTOR(nsCSSValuePairList);
  NS_CSS_DELETE_LIST_MEMBER(nsCSSValuePairList, this, mNext);
}

nsCSSValuePairList*
nsCSSValuePairList::Clone(PRBool aDeep) const
{
  nsCSSValuePairList* result = new nsCSSValuePairList(*this);
  if (NS_UNLIKELY(!result))
    return result;
  if (aDeep)
    NS_CSS_CLONE_LIST_MEMBER(nsCSSValuePairList, this, mNext, result,
                             (PR_FALSE));
  return result;
}

/* static */ PRBool
nsCSSValuePairList::Equal(nsCSSValuePairList* aList1,
                          nsCSSValuePairList* aList2)
{
  if (aList1 == aList2)
    return PR_TRUE;

  nsCSSValuePairList *p1 = aList1, *p2 = aList2;
  for ( ; p1 && p2; p1 = p1->mNext, p2 = p2->mNext) {
    if (p1->mXValue != p2->mXValue ||
        p1->mYValue != p2->mYValue)
      return PR_FALSE;
  }
  return !p1 && !p2; // true if same length, false otherwise
}

// --- nsCSSContent -----------------

nsCSSContent::nsCSSContent(void)
  : mContent(nsnull),
    mCounterIncrement(nsnull),
    mCounterReset(nsnull),
    mQuotes(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSContent);
}

nsCSSContent::~nsCSSContent(void)
{
  MOZ_COUNT_DTOR(nsCSSContent);
  delete mContent;
  delete mCounterIncrement;
  delete mCounterReset;
  delete mQuotes;
}

// --- nsCSSUserInterface -----------------

nsCSSUserInterface::nsCSSUserInterface(void)
  : mCursor(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSUserInterface);
}

nsCSSUserInterface::~nsCSSUserInterface(void)
{
  MOZ_COUNT_DTOR(nsCSSUserInterface);
  delete mCursor;
}

// --- nsCSSAural -----------------

nsCSSAural::nsCSSAural(void)
{
  MOZ_COUNT_CTOR(nsCSSAural);
}

nsCSSAural::~nsCSSAural(void)
{
  MOZ_COUNT_DTOR(nsCSSAural);
}

// --- nsCSSXUL -----------------

nsCSSXUL::nsCSSXUL(void)
{
  MOZ_COUNT_CTOR(nsCSSXUL);
}

nsCSSXUL::~nsCSSXUL(void)
{
  MOZ_COUNT_DTOR(nsCSSXUL);
}

// --- nsCSSColumn -----------------

nsCSSColumn::nsCSSColumn(void)
{
  MOZ_COUNT_CTOR(nsCSSColumn);
}

nsCSSColumn::~nsCSSColumn(void)
{
  MOZ_COUNT_DTOR(nsCSSColumn);
}

#ifdef MOZ_SVG
// --- nsCSSSVG -----------------

nsCSSSVG::nsCSSSVG(void) : mStrokeDasharray(nsnull)
{
  MOZ_COUNT_CTOR(nsCSSSVG);
}

nsCSSSVG::~nsCSSSVG(void)
{
  MOZ_COUNT_DTOR(nsCSSSVG);
  delete mStrokeDasharray;
}

#endif // MOZ_SVG
