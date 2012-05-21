/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIPrivateTextRange_h__
#define nsIPrivateTextRange_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsGUIEvent.h"

#define NS_IPRIVATETEXTRANGE_IID \
{ 0xf795a44d, 0x413a, 0x4c63, \
  { 0xa6, 0xb0, 0x7a, 0xa3, 0x0c, 0xf5, 0xe9, 0xe0 } }

class nsIPrivateTextRange : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IPRIVATETEXTRANGE_IID)

  // Note that the range array may not specify a caret position; in that
  // case there will be no range of type TEXTRANGE_CARETPOSITION in the array.
  enum {
    TEXTRANGE_CARETPOSITION = 1,
    TEXTRANGE_RAWINPUT = 2,
    TEXTRANGE_SELECTEDRAWTEXT = 3,
    TEXTRANGE_CONVERTEDTEXT = 4,
    TEXTRANGE_SELECTEDCONVERTEDTEXT = 5
  };

  NS_IMETHOD    GetRangeStart(PRUint16* aRangeStart)=0;
  NS_IMETHOD    GetRangeEnd(PRUint16* aRangeEnd)=0;
  NS_IMETHOD    GetRangeType(PRUint16* aRangeType)=0;
  NS_IMETHOD    GetRangeStyle(nsTextRangeStyle* aTextRangeStyle)=0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIPrivateTextRange, NS_IPRIVATETEXTRANGE_IID)

#define NS_IPRIVATETEXTRANGELIST_IID \
{0xb5a04b19, 0xed33, 0x4cd0, \
{0x82, 0xa8, 0xb7, 0x00, 0x83, 0xef, 0xc4, 0x91}}

class nsIPrivateTextRangeList : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IPRIVATETEXTRANGELIST_IID)

  NS_IMETHOD_(PRUint16) GetLength()=0;
  NS_IMETHOD_(already_AddRefed<nsIPrivateTextRange>) Item(PRUint16 aIndex)=0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIPrivateTextRangeList,
                              NS_IPRIVATETEXTRANGELIST_IID)

#endif // nsIPrivateTextRange_h__
