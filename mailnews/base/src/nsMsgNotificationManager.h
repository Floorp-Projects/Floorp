/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef NSMSGNOTIFICATION_H
#define NSMSGNOTIFICATION_H

#include "nsIFolderListener.h"
#include "nsIRDFDataSource.h"
#include "nsCOMPtr.h"
#include "nsIMsgFolder.h"
#include "nsIServiceManager.h"


class nsMsgNotificationManager : public nsIFolderListener
{
public:

	nsMsgNotificationManager();
	virtual ~nsMsgNotificationManager();

	NS_DECL_ISUPPORTS
    NS_DECL_NSIFOLDERLISTENER
	nsresult Init();

protected:
	nsresult AddNewMailNotification(nsIMsgFolder *folder);
	nsresult RemoveNewMailNotification(nsIMsgFolder *folder);
	nsresult RemoveOldValues(nsIRDFResource *notificationResource);
	nsresult BuildNewMailURI(nsIMsgFolder *folder, nsCAutoString &newMailURI);

protected:
    nsCOMPtr<nsISupports> mInMemoryDataSourceISupports;

	static nsIRDFResource* kNC_FlashRoot;
	static nsIRDFResource* kNC_Type;
	static nsIRDFResource* kNC_Source;
	static nsIRDFResource* kNC_Description;
	static nsIRDFResource* kNC_TimeStamp;
	static nsIRDFResource* kNC_URL;
	static nsIRDFResource* kNC_Child;
	static nsIRDFResource* kNC_NewMessages;

};

NS_BEGIN_EXTERN_C

nsresult
NS_NewMsgNotificationManager(const nsIID& iid, void **result);

NS_END_EXTERN_C

#endif
