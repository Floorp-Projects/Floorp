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

#ifndef nsIPrivateTextRange_h__
#define nsIPrivateTextRange_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsGUIEvent.h"

// {B13C4BD8-CB9F-4763-A020-E99ABC9C2803}
#define NS_IPRIVATETEXTRANGE_IID \
{ 0xb13c4bd8, 0xcb9f, 0x4763, \
{ 0xa0, 0x20, 0xe9, 0x9a, 0xbc, 0x9c, 0x28, 0x3 } }

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
  NS_IMETHOD    SetRangeStart(PRUint16 aRangeStart)=0;

  NS_IMETHOD    GetRangeEnd(PRUint16* aRangeEnd)=0;
  NS_IMETHOD    SetRangeEnd(PRUint16 aRangeEnd)=0;

  NS_IMETHOD    GetRangeType(PRUint16* aRangeType)=0;
  NS_IMETHOD    SetRangeType(PRUint16 aRangeType)=0;

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
