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
 
   Interface for representing Address Book Person Card Property
 
*********************************************************************************************************/

#ifndef nsAbCardProperty_h__
#define nsAbCardProperty_h__

#include "nsIAbBase.h"  
#include "nsIAbCard.h"  
#include "nsRDFResource.h"
#include "nsISupportsArray.h"
#include "nsVoidArray.h"
#include "nsCOMPtr.h"
#include "nsIAddrDBListener.h"
#include "nsIAddrDatabase.h"

 /* 
  * Address Book Card Property
  */ 

class nsAbCardProperty: public nsIAbCard, public nsIAddrDBListener
{
public: 

	NS_DECL_ISUPPORTS

	nsAbCardProperty(void);
	virtual ~nsAbCardProperty(void);

	// nsIAddrDBListener methods:
	NS_IMETHOD OnCardAttribChange(PRUint32 abCode, nsIAddrDBListener *instigator);
	NS_IMETHOD OnCardEntryChange(PRUint32 abCode, PRUint32 entryID, nsIAddrDBListener *instigator);
	NS_IMETHOD OnAnnouncerGoingAway(nsIAddrDBAnnouncer *instigator);

  // nsICollection methods:
  
  NS_IMETHOD Count(PRUint32 *result) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD GetElementAt(PRUint32 i, nsISupports* *result) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD SetElementAt(PRUint32 i, nsISupports* value) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD AppendElement(nsISupports *aElement) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD RemoveElement(nsISupports *aElement) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD Enumerate(nsIEnumerator* *result) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD Clear(void) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // nsIAbBase methods:
  
  NS_IMETHOD GetURI(char* *name) { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD GetName(char **name) { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD SetName(char *name) { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD GetChildNamed(const char *name, nsISupports* *result) { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD GetParent(nsIAbBase* *parent) { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD SetParent(nsIAbBase *parent) { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD GetChildNodes(nsIEnumerator* *result) { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD AddAddrBookListener(nsIAbListener * listener) { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD RemoveAddrBookListener(nsIAbListener * listener) { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD AddUnique(nsISupports* element) { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD ReplaceElement(nsISupports* element, nsISupports* newElement) { return NS_ERROR_NOT_IMPLEMENTED; }


	// nsIAbCard methods:
	NS_IMETHOD GetListName(char * *listName);
	NS_IMETHOD SetListName(char * listName);

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

	NS_IMETHOD GetDbTableID(PRUint32 *aDbTableID);
	NS_IMETHOD SetDbTableID(PRUint32 aDbTableID);
	NS_IMETHOD GetDbRowID(PRUint32 *aDbRowID);
	NS_IMETHOD SetDbRowID(PRUint32 aDbRowID);

	NS_IMETHOD GetCardValue(const char *attrname, char **value);
	NS_IMETHOD SetCardValue(const char *attrname, const char *value);
	NS_IMETHOD AddCardToDatabase();

protected:

	char* m_pListName;
	char* m_pWorkCity;
	char* m_pOrganization;

	char* m_pFirstName;
	char* m_pLastName;
	char* m_pDisplayName;
	char* m_pNickName;
	char* m_pPrimaryEmail;
	char* m_pSecondEmail;
	char* m_pWorkPhone;
	char* m_pHomePhone;
	char* m_pFaxNumber;
	char* m_pPagerNumber;
	char* m_pCellularNumber;

	PRUint32 m_dbTableID;
	PRUint32 m_dbRowID;

	nsCOMPtr<nsIAddrDatabase> mDatabase;  
};

#endif
