/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
  	NS_IMETHOD DeleteDirectory(nsIAbDirectory *directory);
 	NS_IMETHOD DeleteCards(nsISupportsArray *cards);
 	NS_IMETHOD HasCard(nsIAbCard *cards, PRBool *hasCard);
	NS_IMETHOD HasDirectory(nsIAbDirectory *dir, PRBool *hasDir);
	NS_IMETHOD CreateNewDirectory(const PRUnichar *dirName, const char *fileName, PRBool migrating);
	NS_IMETHOD CreateNewMailingList(const char* uri, nsIAbDirectory *list);
	NS_IMETHOD GetDirUri(char **uri);

	// nsIAddrDBListener methods:
	NS_IMETHOD OnCardAttribChange(PRUint32 abCode, nsIAddrDBListener *instigator);
	NS_IMETHOD OnCardEntryChange(PRUint32 abCode, nsIAbCard *card, nsIAddrDBListener *instigator);
	NS_IMETHOD OnListEntryChange(PRUint32 abCode, nsIAbDirectory *list, nsIAddrDBListener *instigator);

	NS_IMETHOD ClearDatabase();
	NS_IMETHOD NotifyDirItemAdded(nsISupports *item) { return NotifyItemAdded(item);}
	NS_IMETHOD RemoveElementsFromAddressList();
	NS_IMETHOD RemoveEmailAddressAt(PRUint32 aIndex);

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
};

#endif
