/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsIDOMComment_h__
#define nsIDOMComment_h__

#include "nsDOM.h"
#include "nsIDOMNode.h"

#define NS_IDOMCOMMENT_IID \
{ /* 8f6bca7a-ce42-11d1-b724-00600891d8c9 */ \
0x8f6bca7a, 0xce42, 0x11d1, \
  {0xb7, 0x24, 0x00, 0x60, 0x08, 0x91, 0xd8, 0xc9} }

class nsIDOMComment : public nsIDOMNode {
public:
  //attribute UniString        data;
  NS_IMETHOD GetData(nsString &aData) = 0;
  NS_IMETHOD SetData(nsString &aData) = 0;
};

#endif // nsIDOMComment_h__

