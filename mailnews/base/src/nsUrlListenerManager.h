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

#ifndef nsUrlListenerManager_h___
#define nsUrlListenerManager_h___

#include "nsIUrlListenerManager.h"
#include "nsISupportsArray.h"

/********************************************************************************************
	The url listener manager is a helper class used by all of our subclassed urls to implement
	registration and broadcasting of changes to all of their registered url listeners. I envision
	every url implementation to have a has a relationship with a nsIUrlListenerManager. All
	url listener register/unregister calls are forwarded to the listener manager. In addition,
	the url listener manager handles broadcasting of event changes on the url.

    mscott --> hmm now that I think about it this class is probably going to have to be made
	thread safe. It might have to proxy notification calls into another thread....
 ********************************************************************************************/

typedef enum {
	nsUrlNotifyStartRunning = 0,
	nsUrlNotifyStopRunning
} nsUrlNotifyType;

class nsUrlListenerManager : public nsIUrlListenerManager {
public:
	NS_DECL_ISUPPORTS
	nsUrlListenerManager();
	virtual ~nsUrlListenerManager();

	// nsIUrlListenerManager interface support

	 NS_IMETHOD RegisterListener(nsIUrlListener * aUrlListener);
	 NS_IMETHOD UnRegisterListener(nsIUrlListener * aUrlListener);

	 // These functions turn around and broadcast the notifications to all url listeners...
	 NS_IMETHOD OnStartRunningUrl(nsIMsgMailNewsUrl * aUrl);
	 NS_IMETHOD OnStopRunningUrl(nsIMsgMailNewsUrl * aUrl, nsresult aExitCode);

protected:
	nsISupportsArray * m_listeners;

	// helper function used to enumerate ISupportsArray and broadcast the change
	nsresult BroadcastChange(nsIURL * aUrl, nsUrlNotifyType notification, nsresult aErrorCode);
};

#endif /* nsUrlListenerManager_h___ */
