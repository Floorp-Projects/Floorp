/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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


#ifndef __nsMsgRDFDataSource_h
#define __nsMsgRDFDataSource_h

#include "nsCOMPtr.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsITransactionManager.h"
#include "nsIMsgWindowData.h"

class nsMsgRDFDataSource : public nsIRDFDataSource,
                           public nsIShutdownListener,
  						   public nsIMsgWindowData
{
 public:
  nsMsgRDFDataSource();
  virtual ~nsMsgRDFDataSource();
  virtual nsresult Init();
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMSGWINDOWDATA
  NS_DECL_NSIRDFDATASOURCE
  NS_IMETHOD OnShutdown(const nsCID& aClass, nsISupports* service);


 protected:
	nsIRDFService *getRDFService();
	static PRBool assertEnumFunc(nsISupports *aElement, void *aData);
	static PRBool unassertEnumFunc(nsISupports *aElement, void *aData);
	nsresult  NotifyObservers(nsIRDFResource *subject, nsIRDFResource *property,
								nsIRDFNode *object, PRBool assert);
	nsresult GetTransactionManager(nsISupportsArray *sources, nsITransactionManager **aTransactionManager);

	nsCOMPtr<nsIMsgStatusFeedback> mStatusFeedback;
	nsCOMPtr<nsITransactionManager> mTransactionManager;
	nsCOMPtr<nsIMessageView> mMessageView;

	nsCOMPtr<nsISupportsArray> kEmptyArray;

 private:
  nsIRDFService *mRDFService;
  nsCOMPtr<nsISupportsArray> mObservers;

};

#endif
