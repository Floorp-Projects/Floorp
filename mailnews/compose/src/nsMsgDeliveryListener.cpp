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
#include "prprf.h"
#include "nsCOMPtr.h"
#include "nsMsgDeliveryListener.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsMsgPrompts.h"

NS_IMPL_ISUPPORTS(nsMsgDeliveryListener, nsCOMTypeInfo<nsIUrlListener>::GetIID())

nsresult 
nsMsgDeliveryListener::OnStartRunningUrl(nsIURI * aUrl)
{
#ifdef NS_DEBUG
//  printf("Starting to run the delivery operation\n");
#endif

  if (mMsgSendObj)
    mMsgSendObj->NotifyListenersOnStartSending(nsnull, nsnull);

  if (mMsgSendLaterObj)
    mMsgSendLaterObj->NotifyListenersOnStartSending(nsnull);
  
	return NS_OK;
}

nsresult 
nsMsgDeliveryListener::OnStopRunningUrl(nsIURI * aUrl, nsresult aExitCode)
{
  nsresult rv = NS_ERROR_UNEXPECTED;
#ifdef NS_DEBUG
//  printf("\nOnStopRunningUrl() called!\n");
#endif

  // First, stop being a listener since we are done.
  if (aUrl)
	{
		// query it for a mailnews interface for now....
		nsCOMPtr<nsIMsgMailNewsUrl> mailUrl = do_QueryInterface(aUrl);
		if (mailUrl)
			mailUrl->UnRegisterListener(this);
	}

  if (mMsgSendObj)
    mMsgSendObj->NotifyListenersOnStopSending(
                                  nsnull,     // const char *aMsgID, 
                                  aExitCode,  // nsresult aStatus, 
                                  nsnull,     // const PRUnichar *aMsg, 
                                  nsnull);    // nsIFileSpec *returnFileSpec);

  if (mMsgSendLaterObj)
    mMsgSendLaterObj->NotifyListenersOnStopSending(aExitCode,
                            nsnull,  // const PRUnichar *aMsg, 
                            nsnull,  // PRUint32 aTotalTried, 
                            nsnull); // PRUint32 aSuccessful);

  // 
  // Now, important, if there was a callback registered, call the 
  // creators exit routine.
  //
  if (mCompletionCallback)
    rv = (*mCompletionCallback) (aUrl, aExitCode, mTagData);
  else
    rv = NS_OK;

	return rv;
}

nsMsgDeliveryListener::nsMsgDeliveryListener(nsMsgDeliveryCompletionCallback callback,
                                             nsMsgDeliveryType delivType, void *tagData)
{
  mTempFileSpec = nsnull;
  mDeliveryType = delivType;
  mTagData = tagData;
  mCompletionCallback = callback;
  mMsgSendObj = nsnull;
  mMsgSendLaterObj = nsnull;

  NS_INIT_REFCNT();
}

nsMsgDeliveryListener::~nsMsgDeliveryListener()
{
  delete mTempFileSpec;
}

nsresult 
nsMsgDeliveryListener::SetMsgComposeAndSendObject(nsMsgComposeAndSend *obj)
{
  mMsgSendObj = obj;
  return NS_OK;
}

nsresult 
nsMsgDeliveryListener::SetMsgSendLaterObject(nsMsgSendLater *obj)
{
  mMsgSendLaterObj = obj;
  return NS_OK;
}
