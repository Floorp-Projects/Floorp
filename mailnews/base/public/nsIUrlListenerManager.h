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

#ifndef nsIUrlListenerManager_h___
#define nsIUrlListenerManager_h___

#include "nsIMsgMailNewsUrl.h"
#include "nsIUrlListener.h"

/* 5BCDB610-D00D-11d2-8069-006008128C4E */
#define NS_IURLLISTENERMANAGER_IID   \
{ 0x5bcdb610, 0xd00d, 0x11d2, \
  {0x80, 0x69, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e} }

/* B1AA0820-D04B-11d2-8069-006008128C4E */
#define NS_URLLISTENERMANAGER_CID \
{ 0xb1aa0820, 0xd04b, 0x11d2, \
  {0x80, 0x69, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e} }

/********************************************************************************************
	The url listener manager is a helper class used by all of our subclassed urls to implement
	registration and broadcasting of changes to all of their registered url listeners. I envision
	every url implementation to have a has a relationship with a nsIUrlListenerManager. All
	url listener register/unregister calls are forwarded to the listener manager. In addition,
	the url listener manager handles broadcasting of event changes on the url.
 ********************************************************************************************/

class nsIUrlListenerManager : public nsISupports {
public:
	 static const nsIID& IID() { static nsIID iid = NS_IURLLISTENERMANAGER_IID; return iid; }


	 NS_IMETHOD RegisterListener(nsIUrlListener * aUrlListener) = 0;
	 NS_IMETHOD UnRegisterListener(nsIUrlListener * aUrlListener) = 0;

	 // These functions turn around and broadcast the notifications to all url listeners...
	 NS_IMETHOD OnStartRunningUrl(nsIMsgMailNewsUrl * aUrl) = 0;
	 NS_IMETHOD OnStopRunningUrl(nsIMsgMailNewsUrl * aUrl, nsresult aExitCode) = 0;
};

#endif /* nsIUrlListenerManager_h___ */