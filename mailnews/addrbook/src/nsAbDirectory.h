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

#ifndef nsABDirectory_h__
#define nsABDirectory_h__

#include "nsIAbDirectory.h" /* include the interface we are going to support */
#include "nsAbRDFResource.h"
#include "nsIAbCard.h"
#include "nsISupportsArray.h"
#include "nsCOMPtr.h"
#include "nsDirPrefs.h"

 /* 
  * Address Book Directory
  */ 

class nsABDirectory: public nsAbRDFResource, public nsIAbDirectory
{
public: 
	nsABDirectory(void);
	virtual ~nsABDirectory(void);

	NS_DECL_ISUPPORTS_INHERITED

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

	// nsIAbBase methods:
	NS_IMETHOD GetURI(char* *name) { return nsAbRDFResource::GetValue(name); }
	NS_IMETHOD GetName(char **name);
	NS_IMETHOD SetName(char *name);
	NS_IMETHOD GetChildNamed(const char *name, nsISupports* *result);
	NS_IMETHOD GetParent(nsIAbBase* *parent);
	NS_IMETHOD SetParent(nsIAbBase *parent);
	NS_IMETHOD GetChildNodes(nsIEnumerator* *result);

	// nsIAbDirectory methods:
	NS_IMETHOD AddUnique(nsISupports* element);
	NS_IMETHOD ReplaceElement(nsISupports* element, nsISupports* newElement);
	NS_IMETHOD GetChildCards(nsIEnumerator* *result);
	NS_IMETHOD CreateCardFromDirectory(nsIAbCard* *result);
	NS_IMETHOD AddChildCards(const char *uriName, nsIAbCard **childCard);
	NS_IMETHOD GetDirPosition(PRUint32 *pos);
 	NS_IMETHOD DeleteCards(nsISupportsArray *cards);
 	NS_IMETHOD HasCard(nsIAbCard *cards, PRBool *hasCard);

	NS_IMETHOD ContainsChildNamed(const char *name, PRBool* containsChild);
	NS_IMETHOD FindParentOf(nsIAbDirectory * aDirectory, nsIAbDirectory ** aParent);
	NS_IMETHOD IsParentOf(nsIAbDirectory *child, PRBool deep, PRBool *isParent);
	
	// nsIAddrDBListener methods:
	NS_IMETHOD OnCardAttribChange(PRUint32 abCode, nsIAddrDBListener *instigator);
	NS_IMETHOD OnCardEntryChange(PRUint32 abCode, nsIAbCard *card, nsIAddrDBListener *instigator);

protected:
	nsresult NotifyPropertyChanged(char *property, char* oldValue, char* newValue);
	nsresult NotifyItemAdded(nsISupports *item);
	nsresult NotifyItemDeleted(nsISupports *item);
	nsresult AddSubDirectory(nsAutoString name, nsIAbDirectory **childDir);
	nsresult AddChildCards(nsAutoString name, nsIAbCard **childDir);

	nsVoidArray* GetDirList(){ return DIR_GetDirectories(); }

protected:
	nsString mDirName;
	nsCOMPtr<nsISupportsArray> mSubDirectories;
	PRBool mInitialized;
	PRBool mCardInitialized;

	PRInt16 mCsid;
	PRUint8 mDepth;
	PRInt32 mPrefFlags;   
	
	PRUint32 mPos;

};

#endif
