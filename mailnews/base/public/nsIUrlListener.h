/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsIUrlListener_h___
#define nsIUrlListener_h___

#include "nsIURL.h"

/* 47618220-D008-11d2-8069-006008128C4E */
#define NS_IURLLISTENER_IID   \
{ 0x47618220, 0xd008, 0x11d2, \
  {0x80, 0x69, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e} }

/********************************************************************************************
	The nsIUrlListener interface provides a common interface used by applications which want
	to listen to urls. In particular, a url listener is informed by the url when the url (1)
	begins running and (2) when the url is completed. 
 ********************************************************************************************/

class nsIUrlListener : public nsISupports {
public:
	static const nsIID& GetIID() { static nsIID iid = NS_IURLLISTENER_IID; return iid; }
	
	NS_IMETHOD OnStartRunningUrl(nsIURL * aUrl) = 0;
	NS_IMETHOD OnStopRunningUrl(nsIURL * aUrl, nsresult aExitCode) = 0;
};

#endif /* nsIUrlListener_h___ */
