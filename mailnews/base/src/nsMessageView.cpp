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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsMessageView.h"
#include "nsCOMPtr.h"
#include "nsIMessage.h"
#include "nsIMsgFolder.h"
#include "nsMsgUtils.h"
#include "nsMessageViewDataSource.h"
#include "nsEnumeratorUtils.h"



NS_IMPL_ISUPPORTS1(nsMessageView, nsIMessageView)

nsMessageView::nsMessageView()
{
	NS_INIT_REFCNT();
	mShowThreads = PR_FALSE;
	mViewType = nsIMessageView::eShowAll;

}

nsMessageView::~nsMessageView()
{

}

nsresult nsMessageView::Init()
{
	return NS_OK;
}

NS_IMETHODIMP nsMessageView::GetViewType(PRUint32 *aViewType)
{
	if(!aViewType)
		return NS_ERROR_NULL_POINTER;

	*aViewType = mViewType;
	return NS_OK;
}

NS_IMETHODIMP nsMessageView::SetViewType(PRUint32 aViewType)
{
	mViewType = aViewType;
	return NS_OK;
}

NS_IMETHODIMP nsMessageView::GetShowThreads(PRBool *aShowThreads)
{
	if(!aShowThreads)
		return NS_ERROR_NULL_POINTER;

	*aShowThreads = mShowThreads;
	return NS_OK;
}

NS_IMETHODIMP nsMessageView::SetShowThreads(PRBool aShowThreads)
{
	mShowThreads = aShowThreads;
	return NS_OK;
}

NS_IMETHODIMP nsMessageView::GetMessages(nsIRDFResource *parentResource, nsIMsgWindow* msgWindow, nsISimpleEnumerator **messages)
{
	nsresult rv = NS_OK;
	*messages = nsnull;
	nsCOMPtr<nsIMessage> message(do_QueryInterface(parentResource, &rv));
	if (NS_SUCCEEDED(rv)) {
    
		if(mShowThreads)
		{
			nsCOMPtr<nsIMsgFolder> msgfolder;
			rv = message->GetMsgFolder(getter_AddRefs(msgfolder));
			if(NS_SUCCEEDED(rv))
			{
				nsCOMPtr<nsIMsgThread> thread;
				rv = msgfolder->GetThreadForMessage(message, getter_AddRefs(thread));
				if(NS_SUCCEEDED(rv))
				{
					nsCOMPtr<nsISimpleEnumerator> threadMessages;
					nsMsgKey msgKey;
					message->GetMessageKey(&msgKey);
					thread->EnumerateMessages(msgKey, getter_AddRefs(threadMessages));
					nsCOMPtr<nsMessageFromMsgHdrEnumerator> converter;
					NS_NewMessageFromMsgHdrEnumerator(threadMessages, msgfolder, getter_AddRefs(converter));
					PRUint32 viewType;
					rv = GetViewType(&viewType);
					if(NS_FAILED(rv)) return rv;

					nsMessageViewMessageEnumerator * messageEnumerator = 
						new nsMessageViewMessageEnumerator(converter, viewType);
					if(!messageEnumerator)
						return NS_ERROR_OUT_OF_MEMORY;
					NS_ADDREF(messageEnumerator);
					*messages = messageEnumerator;
					rv = NS_OK;

				}
			}
		}
	}
	nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(parentResource));

	if(folder)
	{
		if(mShowThreads)
		{
			nsCOMPtr<nsISimpleEnumerator> threads;
			rv = folder->GetThreads(msgWindow, getter_AddRefs(threads));
			if (NS_FAILED(rv)) return rv;
			nsMessageViewThreadEnumerator * threadEnumerator = 
				new nsMessageViewThreadEnumerator(threads, folder);
			if(!threadEnumerator)
				return NS_ERROR_OUT_OF_MEMORY;
			NS_ADDREF(threadEnumerator);
			*messages = threadEnumerator;
			rv = NS_OK;
		}
		else
		{
			nsCOMPtr<nsISimpleEnumerator> folderMessages;
			rv = folder->GetMessages(msgWindow, getter_AddRefs(folderMessages));
			if (NS_SUCCEEDED(rv))
			{
				PRUint32 viewType;
				rv = GetViewType(&viewType);
				if(NS_FAILED(rv))
					return rv;

				nsMessageViewMessageEnumerator * messageEnumerator = 
					new nsMessageViewMessageEnumerator(folderMessages, viewType);
				if(!messageEnumerator)
					return NS_ERROR_OUT_OF_MEMORY;
				NS_ADDREF(messageEnumerator);
				*messages = messageEnumerator;
				rv = NS_OK;
			}
		}

	}
	if(!*messages)
	{
		  //return empty array
		  nsCOMPtr<nsISupportsArray> assertions;
		  rv = NS_NewISupportsArray(getter_AddRefs(assertions));
			if(NS_FAILED(rv))
				return rv;

		  rv = NS_NewArrayEnumerator(messages, assertions);
	}
	return rv;
}

