/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsIPrivateTextRange_h__
#define nsIPrivateTextRange_h__

#include "nsISupports.h"
#include "nsString.h"

#define NS_IPRIVATETEXTRANGE_IID \
{0xb471ab41, 0x2a79, 0x11d3, \
{ 0x9e, 0xa4, 0x0, 0x60, 0x8, 0x9f, 0xe5, 0x9b } } 

class nsIPrivateTextRange : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IPRIVATETEXTRANGE_IID; return iid; }
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
};

#define NS_IPRIVATETEXTRANGELIST_IID \
{ 0x1ee9d531, 0x2a79, 0x11d3, \
{ 0x9e, 0xa4, 0x0, 0x60, 0x8, 0x9f, 0xe5, 0x9b} } 

class nsIPrivateTextRangeList : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IPRIVATETEXTRANGELIST_IID; return iid; }

  NS_IMETHOD    GetLength(PRUint16* aLength)=0;
  NS_IMETHOD    Item(PRUint16 aIndex, nsIPrivateTextRange** aReturn)=0;
};

#endif // nsIPrivateTextRange_h__
