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

#ifndef NSMESSAGEVIEWDATASOURCE_H
#define NSMESSAGEVIEWDATASOURCE_H

#include "nsIRDFCompositeDataSource.h"
#include "nsIMessageView.h"
#include "nsIMsgFolder.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsISupportsArray.h"
#include "nsIEnumerator.h"
#include "nsIMessage.h"
#include "nsIMsgThread.h"
#include "nsCOMPtr.h"
#include "nsIMsgWindowData.h"

/**
 * The mail data source.
 */
class nsMessageViewDataSource : public nsIRDFCompositeDataSource, public nsIMessageView,
								public nsIRDFObserver, public nsIMsgWindowData
{
private:
	nsCOMPtr<nsISupportsArray> mObservers;
	PRBool			mInitialized;
	nsIRDFService * mRDFService;

public:
  
	NS_DECL_ISUPPORTS
  NS_DECL_NSIRDFDATASOURCE
  NS_DECL_NSIRDFCOMPOSITEDATASOURCE
  NS_DECL_NSIRDFOBSERVER
  NS_DECL_NSIMESSAGEVIEW
  NS_DECL_NSIMSGWINDOWDATA
	nsMessageViewDataSource(void);
	virtual ~nsMessageViewDataSource (void);
  virtual nsresult Init();


protected:
	nsresult createMessageNode(nsIMessage *message, nsIRDFResource *property, nsIRDFNode **target);

 
	// caching frequently used resources
protected:

	nsCOMPtr<nsIRDFDataSource> mDataSource;
	PRUint32 mViewType;
	PRBool mShowThreads;

	static nsIRDFResource* kNC_MessageChild;
	static nsIRDFResource* kNC_Subject;
	static nsIRDFResource* kNC_Date;
	static nsIRDFResource* kNC_Sender;
	static nsIRDFResource* kNC_Status;

	nsCOMPtr<nsIMsgStatusFeedback> mStatusFeedback;
	nsCOMPtr<nsITransactionManager> mTransactionManager;

};

class nsMessageViewMessageEnumerator: public nsISimpleEnumerator
{

public:

	NS_DECL_ISUPPORTS

	nsMessageViewMessageEnumerator(nsISimpleEnumerator *srcEnumerator, PRUint32 showStatus);
	virtual ~nsMessageViewMessageEnumerator();

  NS_DECL_NSISIMPLEENUMERATOR

protected:
	nsresult SetAtNextItem();
	nsresult MeetsCriteria(nsIMessage *message, PRBool *meetsCriteria);

protected:

	nsCOMPtr<nsISimpleEnumerator> mSrcEnumerator;
	nsCOMPtr<nsISupports> mCurMsg;
	nsCOMPtr<nsISupports> mCurThread;
	PRUint32 mShowStatus;

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


PR_EXTERN(nsresult)
NS_NewMessageViewDataSource(const nsIID& iid, void **result);
