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

#ifndef nsAbDirectory_h__
#define nsAbDirectory_h__

#include "nsAbRDFResource.h"
#include "nsAbDirProperty.h"  
#include "nsIAbCard.h"
#include "nsISupportsArray.h"
#include "nsCOMPtr.h"
#include "nsDirPrefs.h"

 /* 
  * Address Book Directory
  */ 

class nsAbDirectory: public nsAbRDFResource, public nsAbDirProperty
{
public: 
	nsAbDirectory(void);
	virtual ~nsAbDirectory(void);

	NS_DECL_ISUPPORTS_INHERITED

	// nsIAbDirectory methods:
	NS_IMETHOD GetChildNodes(nsIEnumerator* *result);
	NS_IMETHOD GetChildCards(nsIEnumerator* *result);
	NS_IMETHOD AddChildCards(const char *uriName, nsIAbCard **childCard);
	NS_IMETHOD AddDirectory(const char *uriName, nsIAbDirectory **childDir);
  	NS_IMETHOD DeleteDirectories(nsISupportsArray *directories);
 	NS_IMETHOD DeleteCards(nsISupportsArray *cards);
 	NS_IMETHOD HasCard(nsIAbCard *cards, PRBool *hasCard);
	NS_IMETHOD HasDirectory(nsIAbDirectory *dir, PRBool *hasDir);
	NS_IMETHOD GetMailingList(nsIEnumerator **mailingList);
	NS_IMETHOD CreateNewDirectory(const char *dirName, const char *fileName);

	// nsIAddrDBListener methods:
	NS_IMETHOD OnCardAttribChange(PRUint32 abCode, nsIAddrDBListener *instigator);
	NS_IMETHOD OnCardEntryChange(PRUint32 abCode, nsIAbCard *card, nsIAddrDBListener *instigator);

protected:
	nsresult NotifyPropertyChanged(char *property, char* oldValue, char* newValue);
	nsresult NotifyItemAdded(nsISupports *item);
	nsresult NotifyItemDeleted(nsISupports *item);
	nsresult AddChildCards(nsAutoString name, nsIAbCard **childDir);
	nsresult DeleteDirectoryCards(nsIAbDirectory* directory, DIR_Server *server);

	nsVoidArray* GetDirList(){ return DIR_GetDirectories(); }

protected:
	nsCOMPtr<nsISupportsArray> mSubDirectories;
	PRBool mInitialized;
};

#endif
