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
 * The Original Code mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Dave Camp <dcamp@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsDOMOfflineLoadStatusList_h___
#define nsDOMOfflineLoadStatusList_h___

#include "nscore.h"
#include "nsIDOMLoadStatus.h"
#include "nsIDOMLoadStatusEvent.h"
#include "nsIDOMLoadStatusList.h"
#include "nsIOfflineCacheUpdate.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsIURI.h"
#include "nsDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMEventListener.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsIScriptContext.h"

class nsDOMOfflineLoadStatus;

class nsDOMOfflineLoadStatusList : public nsIDOMLoadStatusList,
                                   public nsIDOMEventTarget,
                                   public nsIObserver,
                                   public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMLOADSTATUSLIST
  NS_DECL_NSIDOMEVENTTARGET
  NS_DECL_NSIOBSERVER

  nsDOMOfflineLoadStatusList(nsIURI *aURI);
  virtual ~nsDOMOfflineLoadStatusList();

  nsresult Init();

private :
  nsresult          UpdateAdded         (nsIOfflineCacheUpdate *aUpdate);
  nsresult          UpdateCompleted     (nsIOfflineCacheUpdate *aUpdate);
  nsIDOMLoadStatus *FindWrapper         (nsIDOMLoadStatus *aStatus,
                                         PRUint32 *aIndex);
  void              NotifyEventListeners(const nsCOMArray<nsIDOMEventListener>& aListeners,
                                         nsIDOMEvent* aEvent);

  nsresult          SendLoadEvent       (const nsAString& aEventName,
                                         const nsCOMArray<nsIDOMEventListener>& aListeners,
                                         nsIDOMLoadStatus *aStatus);

  PRBool mInitialized;

  nsCOMPtr<nsIURI> mURI;
  nsCOMArray<nsIDOMLoadStatus> mItems;
  nsCString mHostPort;

  nsCOMPtr<nsIScriptContext> mScriptContext;

  nsCOMArray<nsIDOMEventListener> mLoadRequestedEventListeners;
  nsCOMArray<nsIDOMEventListener> mLoadCompletedEventListeners;
  nsCOMArray<nsIDOMEventListener> mUpdateCompletedEventListeners;
};

class nsDOMLoadStatusEvent : public nsDOMEvent,
                             public nsIDOMLoadStatusEvent
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMLOADSTATUSEVENT
  NS_FORWARD_NSIDOMEVENT(nsDOMEvent::)

  nsDOMLoadStatusEvent(const nsAString& aEventName, nsIDOMLoadStatus *aStatus)
    : nsDOMEvent(nsnull, nsnull), mEventName(aEventName), mStatus(aStatus)
  {
  }

  virtual ~nsDOMLoadStatusEvent() { }

  nsresult Init();

private:
  nsAutoString mEventName;
  nsCOMPtr<nsIDOMLoadStatus> mStatus;
};

#endif

