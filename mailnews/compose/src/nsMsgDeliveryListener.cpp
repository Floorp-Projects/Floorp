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

NS_IMPL_ISUPPORTS(nsMsgDeliveryListener, nsIUrlListener::GetIID())

nsresult 
nsMsgDeliveryListener::OnStartRunningUrl(nsIURL * aUrl)
{
#ifdef NS_DEBUG
  printf("Starting to run the delivery operation\n");
#endif
  
	return NS_OK;
}

nsresult 
nsMsgDeliveryListener::OnStopRunningUrl(nsIURL * aUrl, nsresult aExitCode)
{
  nsresult rv;
#ifdef NS_DEBUG
  printf("\nOnStopRunningUrl() called!\n");
#endif

  // First, stop being a listener since we are done.
  if (aUrl)
	{
		// query it for a mailnews interface for now....
		nsCOMPtr<nsIMsgMailNewsUrl> mailUrl = do_QueryInterface(aUrl);
		if (mailUrl)
			mailUrl->UnRegisterListener(this);
	}

  // 
  // Now, important, if there was a callback registered, call the 
  // creators exit routine.
  //
  if (mCompletionCallback)
    rv = (*mCompletionCallback) (aUrl, aExitCode, mTagData);

	return rv;
}

nsMsgDeliveryListener::nsMsgDeliveryListener(nsMsgDeliveryCompletionCallback callback,
                                             nsMsgDeliveryType delivType, void *tagData)
{
  mTempFileSpec = nsnull;
  mDeliveryType = delivType;
  mTagData = tagData;
  mCompletionCallback = callback;
}

nsMsgDeliveryListener::~nsMsgDeliveryListener()
{
  delete mTempFileSpec;
}

