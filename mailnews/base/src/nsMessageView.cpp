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
#include "nsEnumeratorUtils.h"
#include "MailNewsTypes.h"
#include "MailNewsTypes2.h"

NS_IMPL_ISUPPORTS1(nsMessageView, nsIMessageView)

nsMessageView::nsMessageView()
{
	NS_INIT_REFCNT();
	mShowThreads = PR_FALSE;
	mViewType = nsMsgViewType::eShowAll;
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

NS_IMETHODIMP nsMessageView::HasMessages(nsIRDFResource *parentResource, nsIMsgWindow* msgWindow, PRBool *hasMessages)
{
	nsresult rv = NS_OK;
    *hasMessages = PR_FALSE;

	PRUint32 viewType;
	rv = GetViewType(&viewType);
    NS_ENSURE_SUCCESS(rv,rv);

	nsCOMPtr<nsIMessage> message(do_QueryInterface(parentResource, &rv));
	if (NS_SUCCEEDED(rv)) {
		if(mShowThreads) {
			nsCOMPtr<nsIMsgFolder> msgfolder;
			rv = message->GetMsgFolder(getter_AddRefs(msgfolder));
			if(NS_SUCCEEDED(rv)) {
				nsCOMPtr<nsIMsgThread> thread;
				rv = msgfolder->GetThreadForMessage(message, getter_AddRefs(thread));
				if(NS_SUCCEEDED(rv)) {
                    nsMsgKey msgKey;
                    message->GetMessageKey(&msgKey);

                    rv = thread->HasMessagesOfType(msgKey, viewType, hasMessages);
                    NS_ENSURE_SUCCESS(rv,rv);
				}
			}
		}
	}
    else {
	  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(parentResource));
	  if(folder) {
		if(mShowThreads) {
			rv = folder->HasThreads(msgWindow, hasMessages);
            NS_ENSURE_SUCCESS(rv,rv);
		}
		else {
			rv = folder->HasMessagesOfType(msgWindow, viewType, hasMessages);
            NS_ENSURE_SUCCESS(rv,rv);
		}
      }
	}

	return rv;
}


NS_IMETHODIMP nsMessageView::GetMessages(nsIRDFResource *parentResource, nsIMsgWindow* msgWindow, nsISimpleEnumerator **messages)
{
	nsresult rv = NS_OK;

	PRUint32 viewType;
	rv = GetViewType(&viewType);
    NS_ENSURE_SUCCESS(rv,rv);

	*messages = nsnull;
	nsCOMPtr<nsIMessage> message(do_QueryInterface(parentResource, &rv));
	if (NS_SUCCEEDED(rv)) {
		if(mShowThreads) {
			nsCOMPtr<nsIMsgFolder> msgfolder;
			rv = message->GetMsgFolder(getter_AddRefs(msgfolder));
			if(NS_SUCCEEDED(rv)) {
				nsCOMPtr<nsIMsgThread> thread;
				rv = msgfolder->GetThreadForMessage(message, getter_AddRefs(thread));
				if(NS_SUCCEEDED(rv)) {
					nsCOMPtr<nsISimpleEnumerator> threadMessages;
					nsMsgKey msgKey;
					message->GetMessageKey(&msgKey);
#ifdef HAVE_4X_UNREAD_THREADS
                    switch (viewType) {
                        case nsMsgViewType::eShowAll:
					        thread->EnumerateMessages(msgKey, getter_AddRefs(threadMessages));
                            break;
                        case nsMsgViewType::eShowUnread:
                            thread->EnumerateUnreadMessages(msgKey, getter_AddRefs(threadMessages));
                            break;
                        case nsMsgViewType::eShowRead:
                        case nsMsgViewType::eShowWatched:
                        default:
                            NS_ENSURE_SUCCESS(NS_ERROR_UNEXPECTED,NS_ERROR_UNEXPECTED);
                            break;
                    }
#else
                    // Threads with unread
					thread->EnumerateMessages(msgKey, getter_AddRefs(threadMessages));
#endif /* HAVE_4X_UNREAD_THREADS */
					nsCOMPtr<nsMessageFromMsgHdrEnumerator> converter;
					NS_NewMessageFromMsgHdrEnumerator(threadMessages, msgfolder, getter_AddRefs(converter));

                    // we've done the work (by calling EnumerateMessages()
                    // or EnumerateUnreadMessages(), based on viewType)
                    // so we want all messages that are left.
					nsMessageViewMessageEnumerator * messageEnumerator = 
						new nsMessageViewMessageEnumerator(converter, nsMsgViewType::eShowAll);

					if(!messageEnumerator) return NS_ERROR_OUT_OF_MEMORY;
					NS_ADDREF(messageEnumerator);
					*messages = messageEnumerator;
					rv = NS_OK;
				}
			}
		}
	}
    else {
	  nsCOMPtr<nsIMsgFolder> folder(do_QueryInterface(parentResource));
	  if(folder) {
		if(mShowThreads) {
			nsCOMPtr<nsISimpleEnumerator> threads;
			rv = folder->GetThreadsOfType(msgWindow, viewType, getter_AddRefs(threads));
            NS_ENSURE_SUCCESS(rv,rv);

			nsMessageViewThreadEnumerator * threadEnumerator = 
				new nsMessageViewThreadEnumerator(threads, folder);
			if(!threadEnumerator) return NS_ERROR_OUT_OF_MEMORY;
			NS_ADDREF(threadEnumerator);
			*messages = threadEnumerator;
			rv = NS_OK;
		}
		else {
			nsCOMPtr<nsISimpleEnumerator> folderMessages;
			rv = folder->GetMessages(msgWindow, getter_AddRefs(folderMessages));
			if (NS_SUCCEEDED(rv)) {
				nsMessageViewMessageEnumerator * messageEnumerator = 
					new nsMessageViewMessageEnumerator(folderMessages, viewType);
				if(!messageEnumerator) return NS_ERROR_OUT_OF_MEMORY;
				NS_ADDREF(messageEnumerator);
				*messages = messageEnumerator;
				rv = NS_OK;
			}
		}
      }
	}

	if(!*messages) {
		  //return empty array
		  nsCOMPtr<nsISupportsArray> assertions;
		  rv = NS_NewISupportsArray(getter_AddRefs(assertions));
          NS_ENSURE_SUCCESS(rv,rv);
		  rv = NS_NewArrayEnumerator(messages, assertions);
	}
	return rv;
}

//////////////////////////   nsMessageViewMessageEnumerator //////////////////


NS_IMPL_ISUPPORTS1(nsMessageViewMessageEnumerator, nsISimpleEnumerator)

nsMessageViewMessageEnumerator::nsMessageViewMessageEnumerator(nsISimpleEnumerator *srcEnumerator,
															   PRUint32 viewType)
{
	NS_INIT_REFCNT();	

	mSrcEnumerator = dont_QueryInterface(srcEnumerator);

	mViewType = viewType;
}
nsMessageViewMessageEnumerator::~nsMessageViewMessageEnumerator()
{
	//member variables are nsCOMPtr's.
}


/** Next will advance the list. will return failed if already at end
*/
NS_IMETHODIMP nsMessageViewMessageEnumerator::GetNext(nsISupports **aItem)
{
	if (!aItem)
		return NS_ERROR_NULL_POINTER;

	if(!mCurMsg)
		return NS_ERROR_FAILURE;

	*aItem = mCurMsg;
	NS_ADDREF(*aItem);

	return NS_OK;
}

/** GetNext will return the next item it will fail if the list is empty
*  @param aItem return value
*/
/** return if the collection is at the end.  that is the beginning following a call to Prev
*  and it is the end of the list following a call to next
*  @param aItem return value
*/
NS_IMETHODIMP nsMessageViewMessageEnumerator::HasMoreElements(PRBool *aResult)
{
	if(!aResult)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = SetAtNextItem();
	*aResult = NS_SUCCEEDED(rv);
	return NS_OK;
}

//This function sets mSrcEnumerator at the next item that fits
//the criteria for this enumerator.  If there are no more items
//returns NS_ERROR_FAILURE
nsresult nsMessageViewMessageEnumerator::SetAtNextItem()
{
	nsresult rv;

	nsCOMPtr<nsISupports> currentItem;
	nsCOMPtr<nsIMessage> message;

	PRBool hasMore = PR_FALSE;
	while(NS_SUCCEEDED(mSrcEnumerator->HasMoreElements(&hasMore)) && hasMore)
	{
		rv = mSrcEnumerator->GetNext(getter_AddRefs(currentItem));
		PRBool successful = PR_FALSE;
		if(NS_FAILED(rv))
			break;

		message = do_QueryInterface(currentItem, &rv);
		if(NS_SUCCEEDED(rv))
		{
			PRBool meetsCriteria;
			rv = MeetsCriteria(message, &meetsCriteria);
			if(NS_SUCCEEDED(rv) & meetsCriteria)
				successful = PR_TRUE;
		}
		if(successful) 
		{
			mCurMsg = do_QueryInterface(currentItem, &rv);
			return rv;
		}

	}
	return NS_ERROR_FAILURE;
}

nsresult nsMessageViewMessageEnumerator::MeetsCriteria(nsIMessage *message, PRBool *meetsCriteria)
{

	if(!meetsCriteria)
		return NS_ERROR_NULL_POINTER;

	*meetsCriteria = PR_FALSE;

	if(mViewType == nsMsgViewType::eShowAll)
	{
		*meetsCriteria = PR_TRUE;
	}
	else
	{
		PRUint32 flags;
		message->GetFlags(&flags);

		if(mViewType == nsMsgViewType::eShowRead)
			*meetsCriteria = flags & MSG_FLAG_READ;
		else if(mViewType == nsMsgViewType::eShowUnread)
			*meetsCriteria = !(flags & MSG_FLAG_READ);
		else if(mViewType == nsMsgViewType::eShowWatched)
			*meetsCriteria = flags & MSG_FLAG_WATCHED;
	}
	return NS_OK;
}

//////////////////////////   nsMessageViewThreadEnumerator //////////////////


NS_IMPL_ISUPPORTS1(nsMessageViewThreadEnumerator, nsISimpleEnumerator)

nsMessageViewThreadEnumerator::nsMessageViewThreadEnumerator(nsISimpleEnumerator *threads,
															 nsIMsgFolder *srcFolder)
{
	NS_INIT_REFCNT();
	
	mThreads = do_QueryInterface(threads);
	mFolder = do_QueryInterface(srcFolder);
	mNeedToPrefetch = PR_TRUE;
}

nsMessageViewThreadEnumerator::~nsMessageViewThreadEnumerator()
{
	//member variables are nsCOMPtr's
}

/** Next will advance the list. will return failed if already at end
*/
NS_IMETHODIMP nsMessageViewThreadEnumerator::GetNext(nsISupports **aItem)
{
	nsresult rv = NS_OK;

	if(!mMessages)
		return NS_ERROR_FAILURE;

	if (mNeedToPrefetch)
		rv = Prefetch();
	if (NS_SUCCEEDED(rv) && mMessages)
		rv = mMessages->GetNext(aItem);
//	NS_ASSERTION(NS_SUCCEEDED(rv),"getnext shouldn't fail");
	mNeedToPrefetch = PR_TRUE;
	return rv;
}

nsresult nsMessageViewThreadEnumerator::Prefetch()
{
	nsresult rv = NS_OK;

	//then check to see if the there are no more messages in last thread
	//Get the next thread
	rv = mThreads->GetNext(getter_AddRefs(mCurThread));
	if(NS_SUCCEEDED(rv))
	{
		rv = GetMessagesForCurrentThread();
		mNeedToPrefetch = PR_FALSE;
	}
	else
		mMessages = nsnull;
	return rv;
}

/** return if the collection is at the end.  that is the beginning following a call to Prev
*  and it is the end of the list following a call to next
*  @param aItem return value
*/
NS_IMETHODIMP nsMessageViewThreadEnumerator::HasMoreElements(PRBool *aResult)
{
	if (!aResult)
		return NS_ERROR_NULL_POINTER;
	nsresult rv = NS_OK;

	if (mNeedToPrefetch)
		Prefetch();
	//First check to see if there are no more threads
	if (mMessages)
		rv = mMessages->HasMoreElements(aResult);
	else
		*aResult = nsnull;
	return rv;
}

nsresult nsMessageViewThreadEnumerator::GetMessagesForCurrentThread()
{
	nsCOMPtr<nsIMsgThread> thread;
	nsresult rv = NS_OK;
	if(mCurThread)
	{
		thread = do_QueryInterface(mCurThread, &rv);
		if(NS_SUCCEEDED(rv))
		{
			nsCOMPtr<nsISimpleEnumerator> msgHdrs;
			rv = thread->EnumerateMessages(nsMsgKey_None, getter_AddRefs(msgHdrs));
			nsMessageFromMsgHdrEnumerator *messages;
			NS_NewMessageFromMsgHdrEnumerator(msgHdrs, mFolder, &messages);
			mMessages = do_QueryInterface(messages, &rv);
			NS_IF_RELEASE(messages);
		}
	}
	else
		mMessages = nsnull;

	return rv;
}

