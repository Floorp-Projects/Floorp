/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsMsgDeliveryListener.h"

#include "nsIMsgMailNewsUrl.h"
#include "nsMsgPrompts.h"

NS_IMPL_ISUPPORTS1(nsMsgDeliveryListener, nsIUrlListener)

nsresult 
nsMsgDeliveryListener::OnStartRunningUrl(nsIURI * aUrl)
{
#ifdef NS_DEBUG
//  printf("Starting to run the delivery operation\n");
#endif

  if (mMsgSendObj)
    mMsgSendObj->NotifyListenerOnStartSending(nsnull, nsnull);

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
    mMsgSendObj->NotifyListenerOnStopSending(
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
    rv = (*mCompletionCallback) (aUrl, aExitCode, mDeliveryType, mTagData);
  else
    rv = NS_OK;

	return rv;
}

nsMsgDeliveryListener::nsMsgDeliveryListener(nsMsgDeliveryCompletionCallback callback,
                                             nsMsgDeliveryType delivType, nsISupports *tagData)
{
#if defined(DEBUG_ducarroz)
  printf("CREATE nsMsgDeliveryListener: %x\n", this);
#endif

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
#if defined(DEBUG_ducarroz)
  printf("DISPOSE nsMsgDeliveryListener: %x\n", this);
#endif

  delete mTempFileSpec;
}

nsresult 
nsMsgDeliveryListener::SetMsgComposeAndSendObject(nsIMsgSend *obj)
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
