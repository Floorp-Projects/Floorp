/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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



#endif
