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

/********************************************************************************************************
 
   Interface for representing Address Book Directory
 
*********************************************************************************************************/

#ifndef nsAbDirProperty_h__
#define nsAbDirProperty_h__

#include "nsIAbDirectory.h" /* include the interface we are going to support */
#include "nsAbRDFResource.h"
#include "nsIAbCard.h"
#include "nsISupportsArray.h"
#include "nsCOMPtr.h"
#include "nsDirPrefs.h"

 /* 
  * Address Book Directory
  */ 

class nsAbDirProperty: public nsIAbDirectory
{
public: 
	nsAbDirProperty(void);
	virtual ~nsAbDirProperty(void);

	NS_DECL_ISUPPORTS

	// nsIAbDirectory methods:
	NS_IMETHOD GetDirName(char **name);
	NS_IMETHOD SetDirName(char * name);
	NS_IMETHOD GetLastModifiedDate(char * *aLastModifiedDate);
	NS_IMETHOD SetLastModifiedDate(char * aLastModifiedDate);
	NS_IMETHOD GetServer(DIR_Server * *aServer);
	NS_IMETHOD SetServer(DIR_Server * aServer);
	NS_IMETHOD GetDirFilePath(char **dbPath);

	NS_IMETHOD GetChildNodes(nsIEnumerator **childList) { return NS_OK; }
	NS_IMETHOD GetChildCards(nsIEnumerator **childCards) { return NS_OK; }
	NS_IMETHOD AddChildCards(const char *uriName, nsIAbCard **childCard) { return NS_OK; }
	NS_IMETHOD AddDirectory(const char *uriName, nsIAbDirectory **childDir) { return NS_OK; }
	NS_IMETHOD DeleteDirectories(nsISupportsArray *dierctories) { return NS_OK; }
	NS_IMETHOD DeleteCards(nsISupportsArray *cards) { return NS_OK; }
	NS_IMETHOD HasCard(nsIAbCard *cards, PRBool *hasCard) { return NS_OK; }
	NS_IMETHOD HasDirectory(nsIAbDirectory *dir, PRBool *hasDir) { return NS_OK; }
	NS_IMETHOD GetMailingList(nsIEnumerator **mailingList) { return NS_OK; }
	NS_IMETHOD CreateNewDirectory(const char *dirName, const char *fileName) { return NS_OK; }

protected:

	char* m_DirName;
	char* m_LastModifiedDate;
	nsFileSpec* m_DbPath;
	DIR_Server* m_Server;
};

#endif
