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

#ifndef nsICalToolkit_h___
#define nsICalToolkit_h___

#include "nsISupports.h"
#include "capi.h"
#include "nscal.h"


class nsIApplicationShell;

//3efc2c70-200d-11d2-9246-00805f8a7ab6
#define NS_ICAL_TOOLKIT_IID   \
{ 0x3efc2c70, 0x200d, 0x11d2,    \
{ 0x92, 0x46, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6 } }

class nsICalToolkit : public nsISupports
{

public:

  NS_IMETHOD Init(nsIApplicationShell * aApplicationShell) = 0;
  NS_IMETHOD_(CAPISession) GetCAPISession() = 0;
  NS_IMETHOD_(CAPIHandle) GetCAPIHandle() = 0;
  NS_IMETHOD_(char *) GetCAPIPassword() = 0 ;

};

#endif /* nsICalToolkit_h___ */


