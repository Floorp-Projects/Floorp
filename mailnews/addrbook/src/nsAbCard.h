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

#ifndef nsABCard_h__
#define nsABCard_h__

#include "msgCore.h"
#include "nsIAbCard.h" /* include the interface we are going to support */
#include "nsRDFResource.h"
#include "nsISupportsArray.h"
#include "nsCOMPtr.h"
#include "nsIAddrDBListener.h"
#include "nsIAddrDatabase.h"

 /* 
  * Address Book Directory
  */ 

class nsABCard: public nsRDFResource, public nsIAbCard, public nsIAddrDBListener
{
public: 

  NS_DECL_ISUPPORTS_INHERITED

  nsABCard(void);
  virtual ~nsABCard(void);

  // nsICollection methods:
  NS_IMETHOD Count(PRUint32 *result) {
    return mSubDirectories->Count(result);
  }
  NS_IMETHOD GetElementAt(PRUint32 i, nsISupports* *result) {
    return mSubDirectories->GetElementAt(i, result);
  }
  NS_IMETHOD SetElementAt(PRUint32 i, nsISupports* value) {
    return mSubDirectories->SetElementAt(i, value);
  }
  NS_IMETHOD AppendElement(nsISupports *aElement) {
    return mSubDirectories->AppendElement(aElement);
  }
  NS_IMETHOD RemoveElement(nsISupports *aElement) {
    return mSubDirectories->RemoveElement(aElement);
  }
  NS_IMETHOD Enumerate(nsIEnumerator* *result) {
    return mSubDirectories->Enumerate(result);
  }
  NS_IMETHOD Clear(void) {
    return mSubDirectories->Clear();
  }

  // nsIFolder methods:
  NS_IMETHOD GetURI(char* *name) { return nsRDFResource::GetValue(name); }
  NS_IMETHOD GetName(char **name);
  NS_IMETHOD SetName(char *name);
  NS_IMETHOD GetChildNamed(const char *name, nsISupports* *result);
  NS_IMETHOD GetParent(nsIAbBase* *parent);
  NS_IMETHOD SetParent(nsIAbBase *parent);
  NS_IMETHOD GetChildNodes(nsIEnumerator* *result);
  NS_IMETHOD AddAddrBookListener(nsIAbListener * listener);
  NS_IMETHOD RemoveAddrBookListener(nsIAbListener * listener);


  // nsIAddrDBListener methods:
  NS_IMETHOD OnCardAttribChange(PRUint32 abCode, nsIAddrDBListener *instigator);
  NS_IMETHOD OnCardEntryChange(PRUint32 abCode, PRUint32 entryID, nsIAddrDBListener *instigator);
  NS_IMETHOD OnAnnouncerGoingAway(nsIAddrDBAnnouncer *instigator);

  // nsIAbCard methods:
  NS_IMETHOD AddUnique(nsISupports* element);
  NS_IMETHOD ReplaceElement(nsISupports* element, nsISupports* newElement);
  NS_IMETHOD GetPersonName(char * *personName);
  NS_IMETHOD SetPersonName(char * personName);
  NS_IMETHOD GetListName(char * *listName);
  NS_IMETHOD SetListName(char * listName);
  NS_IMETHOD GetEmail(char * *email);
  NS_IMETHOD SetEmail(char * email);
  NS_IMETHOD GetCity(char * *city);
  NS_IMETHOD SetCity(char * city);

  NS_IMETHOD GetFirstName(char * *aFirstName);
  NS_IMETHOD SetFirstName(char * aFirstName);
  NS_IMETHOD GetLastName(char * *aLastName);
  NS_IMETHOD SetLastName(char * aLastName);
  NS_IMETHOD GetDisplayName(char * *aDisplayName);
  NS_IMETHOD SetDisplayName(char * aDisplayName);
  NS_IMETHOD GetPrimaryEmail(char * *aPrimaryEmail);
  NS_IMETHOD SetPrimaryEmail(char * aPrimaryEmail);
  NS_IMETHOD GetSecondEmail(char * *aSecondEmail);
  NS_IMETHOD SetSecondEmail(char * aSecondEmail);
  NS_IMETHOD GetWorkPhone(char * *aWorkPhone);
  NS_IMETHOD SetWorkPhone(char * aWorkPhone);
  NS_IMETHOD GetHomePhone(char * *aHomePhone);
  NS_IMETHOD SetHomePhone(char * aHomePhone);
  NS_IMETHOD GetFaxNumber(char * *aFaxNumber);
  NS_IMETHOD SetFaxNumber(char * aFaxNumber);
  NS_IMETHOD GetPagerNumber(char * *aPagerNumber);
  NS_IMETHOD SetPagerNumber(char * aPagerNumber);
  NS_IMETHOD GetCellularNumber(char * *aCellularNumber);
  NS_IMETHOD SetCellularNumber(char * aCellularNumber);
  NS_IMETHOD GetWorkCity(char * *aWorkCity);
  NS_IMETHOD SetWorkCity(char * aWorkCity);
  NS_IMETHOD GetOrganization(char * *aOrganization);
  NS_IMETHOD SetOrganization(char * aOrganization);
  NS_IMETHOD GetNickName(char * *aNickName);
  NS_IMETHOD SetNickName(char * aNickName);

  NS_IMETHOD GetDbRowID(PRUint32 *aDbRowID);
  NS_IMETHOD SetDbRowID(PRUint32 aDbRowID);

  NS_IMETHOD GetCardValue(const char *attrname, char **value);
  NS_IMETHOD SetCardValue(const char *attrname, const char *value);
  NS_IMETHOD AddCardToDatabase();


//  NS_IMETHOD GetPrettiestName(char ** name);

//  NS_IMETHOD OnCloseDirectory();
//  NS_IMETHOD Delete();

  NS_IMETHODIMP ContainsChildNamed(const char *name, PRBool* containsChild);
  NS_IMETHODIMP FindParentOf(nsIAbCard * aDirectory, nsIAbCard ** aParent);
  NS_IMETHODIMP IsParentOf(nsIAbCard *child, PRBool deep, PRBool *isParent);

//  NS_IMETHOD CreateSubDirectory(const char *dirName);

//  NS_IMETHOD Rename(const char *name);

//  NS_IMETHOD GetDepth(PRUint32 *depth);
//  NS_IMETHOD SetDepth(PRUint32 depth);

#ifdef HAVE_DB
  NS_IMETHOD GetTotalPersonsInDB(PRUint32 *totalPersons) const;					// How many messages in database.
#endif
	
protected:
//	nsresult NotifyPropertyChanged(char *property, char* oldValue, char* newValue);
//	nsresult NotifyItemAdded(nsISupports *item);
//	nsresult NotifyItemDeleted(nsISupports *item);


   nsresult AddSubNode(nsAutoString name, nsIAbCard **childDir);

protected:
	nsString mCardName;
	nsCOMPtr<nsISupportsArray> mSubDirectories;
	nsVoidArray *mListeners;
	PRBool mInitialized;

	PRInt16 mCsid;			// default csid for folder/newsgroup - maintained by fe.
	PRUint8 mDepth;
	PRInt32 mPrefFlags;       // prefs like MSG_PREF_OFFLINE, MSG_PREF_ONE_PANE, etc

	char* m_pFirstName;
	char* m_pLastName;
	char* m_pDisplayName;
	char* m_pPrimaryEmail;
	char* m_pSecondEmail;
	char* m_pWorkPhone;
	char* m_pHomePhone;
	char* m_pFaxNumber;
	char* m_pPagerNumber;
	char* m_pCellularNumber;

	PRUint32 m_dbRowID;

	nsCOMPtr<nsIAddrDatabase> mDatabase;  
};

#endif
