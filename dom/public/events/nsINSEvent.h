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

#ifndef nsINSEvent_h__
#define nsINSEvent_h__

#include "nsISupports.h"
class nsIDOMNode;

/*
 * Base Netscape DOM event class.
 */
#define NS_INSEVENT_IID \
{ /* 64287f80-eb6a-11d1-bd85-00805f8ae3f4 */ \
0x64287f80, 0xeb6a, 0x11d1, \
{0xbd, 0x85, 0x00, 0x80, 0x5f, 0x8a, 0xe3, 0xf4} }

class nsINSEvent : public nsISupports {

public:

  NS_IMETHOD GetLayerX(PRInt32& aX) = 0;
  NS_IMETHOD GetLayerY(PRInt32& aY) = 0;

};
#endif // nsINSEvent_h__
