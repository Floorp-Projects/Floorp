/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/********************************************************************************************************
 
   Interface for representing Address Book Directory
 
*********************************************************************************************************/

#ifndef nsAbMDBDirectory_h__
#define nsAbMDBDirectory_h__

#include "nsAbMDBDirProperty.h"  
#include "nsIAbCard.h"
#include "nsCOMArray.h"
#include "nsISupportsArray.h"
#include "nsCOMPtr.h"
#include "nsDirPrefs.h"
#include "nsIAbDirectorySearch.h"
#include "nsAbDirSearchListener.h"
#include "nsHashtable.h"
#include "nsRDFResource.h"
#include "nsIAddrDBListener.h"

 /* 
  * Address Book Directory
  */ 

class nsAbMDBDirectory:
	public nsRDFResource, 
	public nsAbMDBDirProperty,	// nsIAbDirectory, nsIAbMDBDirectory
	public nsAbDirSearchListenerContext,
  public nsIAddrDBListener, 
	public nsIAbDirectorySearch
{
public: 
	nsAbMDBDirectory(void);
	virtual ~nsAbMDBDirectory(void);

	NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIADDRDBLISTENER

	// nsIRDFResource methods:
	NS_IMETHOD Init(const char* aURI);

	// nsIAbMDBDirectory methods
	NS_IMETHOD ClearDatabase();
	NS_IMETHOD NotifyDirItemAdded(nsISupports *item) { return NotifyItemAdded(item);}
	NS_IMETHOD RemoveElementsFromAddressList();
	NS_IMETHOD RemoveEmailAddressAt(PRUint32 aIndex);
	NS_IMETHOD AddDirectory(const char *uriName, nsIAbDirectory **childDir);
	NS_IMETHOD GetDirUri(char **uri);
    NS_IMETHOD HasCardForEmailAddress(const char * aEmailAddress, PRBool * aCardExists);

	// nsIAbDirectory methods:
	NS_IMETHOD GetChildNodes(nsISimpleEnumerator* *result);
	NS_IMETHOD GetChildCards(nsISimpleEnumerator* *result);
  NS_IMETHOD ModifyDirectory(nsIAbDirectory *directory, nsIAbDirectoryProperties *aProperties);
  	NS_IMETHOD DeleteDirectory(nsIAbDirectory *directory);
 	NS_IMETHOD DeleteCards(nsISupportsArray *cards);
 	NS_IMETHOD HasCard(nsIAbCard *cards, PRBool *hasCard);
	NS_IMETHOD HasDirectory(nsIAbDirectory *dir, PRBool *hasDir);
	NS_IMETHOD CreateNewDirectory(nsIAbDirectoryProperties *aProperties);
	NS_IMETHOD CreateDirectoryByURI(const PRUnichar *dirName, const char *uri, PRBool migrating);
	NS_IMETHOD AddMailList(nsIAbDirectory *list);
  NS_IMETHOD AddMailListWithKey(nsIAbDirectory *list, PRUint32 *key);
	NS_IMETHOD AddCard(nsIAbCard *card, nsIAbCard **addedCard);
	NS_IMETHOD DropCard(nsIAbCard *card, PRBool needToCopyCard);
	NS_IMETHOD EditMailListToDatabase(const char *uri, nsIAbCard *listCard);

	// nsIAbDirectorySearch methods
	NS_DECL_NSIABDIRECTORYSEARCH

	// nsAbDirSearchListenerContext methods
	nsresult OnSearchFinished (PRInt32 result);
	nsresult OnSearchFoundCard (nsIAbCard* card);

	PRBool IsMailingList(){ return (mIsMailingList == 1); }

protected:
	nsresult NotifyPropertyChanged(nsIAbDirectory *list, const char *property, const PRUnichar* oldValue, const PRUnichar* newValue);
	nsresult NotifyItemAdded(nsISupports *item);
	nsresult NotifyItemDeleted(nsISupports *item);
  nsresult NotifyItemChanged(nsISupports *item);
	nsresult RemoveCardFromAddressList(nsIAbCard* card);
  nsresult InternalAddMailList(nsIAbDirectory *list, PRUint32 *key);

	nsresult GetAbDatabase();
	nsCOMPtr<nsIAddrDatabase> mDatabase;  

protected:
	nsCOMArray<nsIAbDirectory> mSubDirectories;
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
