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

#ifndef _nsMsgDeliveryListener_H_
#define _nsMsgDeliveryListener_H_

#include "nsIUrlListener.h"
#include "nsFileSpec.h"
#include "nsMsgSend.h"
#include "nsMsgSendLater.h"

// For various delivery types
enum nsMsgDeliveryType
{
  nsMailDelivery,
  nsNewsDelivery,
  nsLocalFCCDelivery,
  nsImapFCCDelivery,
  nsFileSaveDelivery,
  nsExternalDelivery
};

// 
// This is the generic callback that will be called when the URL processing operation
// is complete. The tagData is what was passed in by the caller at creation time.
//
typedef nsresult (*nsMsgDeliveryCompletionCallback) (nsIURI *aUrl, nsresult aExitCode, nsMsgDeliveryType deliveryType, nsISupports *tagData);

class nsMsgSendLater;

class nsMsgDeliveryListener: public nsIUrlListener
{
public:
	nsMsgDeliveryListener(nsMsgDeliveryCompletionCallback callback,
                        nsMsgDeliveryType               delivType,  
                        nsISupports                     *tagData);
	virtual     ~nsMsgDeliveryListener();

	NS_DECL_ISUPPORTS

	// nsIUrlListener support
	NS_IMETHOD          OnStartRunningUrl(nsIURI * aUrl);
	NS_IMETHOD          OnStopRunningUrl(nsIURI * aUrl, nsresult aExitCode);
  NS_IMETHOD          SetMsgComposeAndSendObject(nsIMsgSend *obj);
  NS_IMETHOD          SetMsgSendLaterObject(nsMsgSendLater *obj);

private:
  // Private Information
  nsCOMPtr<nsISupports>           mTagData;
  nsFileSpec                      *mTempFileSpec;
  nsMsgDeliveryType               mDeliveryType;
  nsCOMPtr<nsIMsgSend>            mMsgSendObj;
  nsMsgSendLater                  *mMsgSendLaterObj;
  nsMsgDeliveryCompletionCallback mCompletionCallback;
};


#endif /* _nsMsgDeliveryListener_H_ */
