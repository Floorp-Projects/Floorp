/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/********************************************************************************************************
 
   Interface for representing Address Book Directory
 
*********************************************************************************************************/

#ifndef nsAbMDBDirectory_h__
#define nsAbMDBDirectory_h__

#include "nsAbMDBRDFResource.h"
#include "nsAbMDBDirProperty.h"  
#include "nsIAbCard.h"
#include "nsISupportsArray.h"
#include "nsCOMPtr.h"
#include "nsDirPrefs.h"
#include "nsIAbDirectorySearch.h"
#include "nsAbDirSearchListener.h"
#include "nsHashtable.h"

 /* 
  * Address Book Directory
  */ 

class nsAbMDBDirectory:
	public nsAbMDBRDFResource,	// nsIRDFResource
	public nsAbMDBDirProperty,	// nsIAbDirectory, nsIAbMDBDirectory
	public nsIAbDirectorySearch,
	public nsAbDirSearchListenerContext
{
public: 
	nsAbMDBDirectory(void);
	virtual ~nsAbMDBDirectory(void);

	NS_DECL_ISUPPORTS_INHERITED

	// nsIRDFResource methods:
	NS_IMETHOD Init(const char* aURI);

	// nsIAbMDBDirectory methods
	NS_IMETHOD ClearDatabase();
	NS_IMETHOD NotifyDirItemAdded(nsISupports *item) { return NotifyItemAdded(item);}
	NS_IMETHOD RemoveElementsFromAddressList();
	NS_IMETHOD RemoveEmailAddressAt(PRUint32 aIndex);
	NS_IMETHOD AddChildCards(const char *uriName, nsIAbCard **childCard);
	NS_IMETHOD AddDirectory(const char *uriName, nsIAbDirectory **childDir);
	NS_IMETHOD GetDirUri(char **uri);

	// nsIAbDirectory methods:
	NS_IMETHOD GetChildNodes(nsIEnumerator* *result);
	NS_IMETHOD GetChildCards(nsIEnumerator* *result);
  	NS_IMETHOD DeleteDirectory(nsIAbDirectory *directory);
 	NS_IMETHOD DeleteCards(nsISupportsArray *cards);
 	NS_IMETHOD HasCard(nsIAbCard *cards, PRBool *hasCard);
	NS_IMETHOD HasDirectory(nsIAbDirectory *dir, PRBool *hasDir);
	NS_IMETHOD CreateNewDirectory(PRUint32 prefCount, const char **prefName, const PRUnichar **prefValue);
	NS_IMETHOD CreateDirectoryByURI(const PRUnichar *dirName, const char *uri, PRBool migrating);
	NS_IMETHOD AddMailList(nsIAbDirectory *list);
	NS_IMETHOD AddCard(nsIAbCard *card, nsIAbCard **_retval);
	NS_IMETHOD DropCard(nsIAbCard *card, nsIAbCard **_retval);
	NS_IMETHOD EditMailListToDatabase(const char *uri);
	NS_IMETHOD GetTotalCards(PRBool subDirectoryCount, PRUint32 *_retval);

	// nsIAddrDBListener methods:
	NS_IMETHOD OnCardAttribChange(PRUint32 abCode, nsIAddrDBListener *instigator);
	NS_IMETHOD OnCardEntryChange(PRUint32 abCode, nsIAbCard *card, nsIAddrDBListener *instigator);
	NS_IMETHOD OnListEntryChange(PRUint32 abCode, nsIAbDirectory *list, nsIAddrDBListener *instigator);

	// nsIAbDirectorySearch methods
	NS_DECL_NSIABDIRECTORYSEARCH

	// nsAbDirSearchListenerContext methods
	nsresult OnSearchFinished (PRInt32 result);
	nsresult OnSearchFoundCard (nsIAbCard* card);

	PRBool IsMailingList(){ return (mIsMailingList == 1); }

protected:
	nsresult NotifyPropertyChanged(char *property, PRUnichar* oldValue, PRUnichar* newValue);
	nsresult NotifyItemAdded(nsISupports *item);
	nsresult NotifyItemDeleted(nsISupports *item);
	nsresult AddChildCards(nsAutoString name, nsIAbCard **childDir);
	nsresult DeleteDirectoryCards(nsIAbDirectory* directory, DIR_Server *server);
	nsresult RemoveCardFromAddressList(const nsIAbCard* card);

	nsresult AddMailList(const char *uriName);

	nsVoidArray* GetDirList(){ return DIR_GetDirectories(); }

protected:
	nsCOMPtr<nsISupportsArray> mSubDirectories;
	PRBool mInitialized;
	PRInt16 mIsMailingList;

	PRBool mIsValidURI;
	PRBool mIsQueryURI;
	nsCString mPath;
	nsCString mQueryString;
	nsCString mURINoQuery;

	PRInt32 mContext;
	PRBool mPerformingQuery;
	nsSupportsHashtable mSearchCache;
};

#endif
