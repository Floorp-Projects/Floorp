/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsUrlListenerManager_h___
#define nsUrlListenerManager_h___

#include "nsIUrlListenerManager.h"
#include "nsISupportsArray.h"
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
  // mscott --> for the longest time, I had m_listeners as a nsVoidArray to prevent
  // circular references when the url listener owned the url which owned the manager
  // which owned the url listener. By using a void array, were the manager didn't own
  // the listeners, we got out of this problem. But this made the model very difficult
  // to use for folks when it came to maintenance. Why? Because they would have a url
  // listener which didn't own the url. And they had no way of knowing how to keep
  // their object alive until the listener manager said the url was done. As a result,
  // folks were using strange ref counting techniques on their objects such that they 
  // would addref themselves before adding their listener to the url. Then, when they
  // received a on stop running url, they would throw a random release in there.
  // Needless to say, this is far from ideal so I've decided to change this to 
  // an nsISupportsArray and cleanup the caller's ref counting hacks to get this to work.
  // The danger is of course still the circular reference problem. In order to get around
  // this, when we issue a on stop running url through the manager, I'm going to release
  // all our url listeners. This should break the circle.
	// nsVoidArray * m_listeners;
  nsCOMPtr<nsISupportsArray> m_listeners;

	// helper function used to enumerate ISupportsArray and broadcast the change
	nsresult BroadcastChange(nsIURI * aUrl, nsUrlNotifyType notification, nsresult aErrorCode);
  void ReleaseListeners();
};

#endif /* nsUrlListenerManager_h___ */
