/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsUrlListenerManager_h___
#define nsUrlListenerManager_h___

#include "nsIUrlListenerManager.h"
#include "nsVoidArray.h"
#include "nsCOMPtr.h"

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
	NS_DECL_NSIURLLISTENERMANAGER
    nsUrlListenerManager();
	virtual ~nsUrlListenerManager();
    
protected:
	nsVoidArray * m_listeners;

	// helper function used to enumerate ISupportsArray and broadcast the change
	nsresult BroadcastChange(nsIURI * aUrl, nsUrlNotifyType notification, nsresult aErrorCode);
};

#endif /* nsUrlListenerManager_h___ */
