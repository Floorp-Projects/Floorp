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
 */

#ifndef _nsMessageView_h
#define _nsMessageView_h

#include "nsIMessageView.h"
#include "nsIEnumerator.h"
#include "nsIMsgFolder.h"
#include "nsIMessage.h"

class nsMessageView : public nsIMessageView {

public:

    NS_DECL_ISUPPORTS

	nsMessageView();
	virtual ~nsMessageView();
	nsresult Init();
	NS_DECL_NSIMESSAGEVIEW

protected:
	PRBool mShowThreads;
	PRUint32 mViewType;

};

class nsMessageViewMessageEnumerator: public nsISimpleEnumerator
{

public:

	NS_DECL_ISUPPORTS

	nsMessageViewMessageEnumerator(nsISimpleEnumerator *srcEnumerator, PRUint32 viewType);
	virtual ~nsMessageViewMessageEnumerator();

  NS_DECL_NSISIMPLEENUMERATOR

protected:
	nsresult SetAtNextItem();
	nsresult MeetsCriteria(nsIMessage *message, PRBool *meetsCriteria);

protected:

	nsCOMPtr<nsISimpleEnumerator> mSrcEnumerator;
	nsCOMPtr<nsISupports> mCurMsg;
	nsCOMPtr<nsISupports> mCurThread;
	PRUint32 mViewType;

};

class nsMessageViewThreadEnumerator: public nsISimpleEnumerator
{

public:

	NS_DECL_ISUPPORTS

	nsMessageViewThreadEnumerator(nsISimpleEnumerator *srcEnumerator, nsIMsgFolder *srcFolder);
	virtual ~nsMessageViewThreadEnumerator();

  NS_DECL_NSISIMPLEENUMERATOR

protected:
	nsresult GetMessagesForCurrentThread();
	nsresult Prefetch();

protected:

	nsCOMPtr<nsISimpleEnumerator> mThreads;
	nsCOMPtr<nsISimpleEnumerator> mMessages;
	nsCOMPtr<nsISupports> mCurThread;
	nsCOMPtr<nsIMsgFolder> mFolder;
	PRBool					mNeedToPrefetch;

};

#endif
