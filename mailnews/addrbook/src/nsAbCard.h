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

#include "nsIAbBase.h"  
#include "nsAbCardProperty.h"  
#include "nsRDFResource.h"
#include "nsISupportsArray.h"
#include "nsVoidArray.h"
#include "nsCOMPtr.h"
#include "nsIAddrDBListener.h"
#include "nsIAddrDatabase.h"

 /* 
  * Address Book Directory
  */ 

class nsABCard: public nsRDFResource, public nsAbCardProperty
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

  // nsIAbBase methods:
/*  
  NS_IMETHOD GetURI(char* *name) { return nsRDFResource::GetValue(name); }
  NS_IMETHOD GetName(char **name);
  NS_IMETHOD SetName(char *name);
  NS_IMETHOD GetChildNamed(const char *name, nsISupports* *result);
  NS_IMETHOD GetParent(nsIAbBase* *parent);
  NS_IMETHOD SetParent(nsIAbBase *parent);
  NS_IMETHOD GetChildNodes(nsIEnumerator* *result);
  NS_IMETHOD AddAddrBookListener(nsIAbListener * listener);
  NS_IMETHOD RemoveAddrBookListener(nsIAbListener * listener);
  NS_IMETHOD AddUnique(nsISupports* element);
  NS_IMETHOD ReplaceElement(nsISupports* element, nsISupports* newElement);


  // nsIAddrDBListener methods:
  NS_IMETHOD OnCardAttribChange(PRUint32 abCode, nsIAddrDBListener *instigator);
  NS_IMETHOD OnCardEntryChange(PRUint32 abCode, PRUint32 entryID, nsIAddrDBListener *instigator);
  NS_IMETHOD OnAnnouncerGoingAway(nsIAddrDBAnnouncer *instigator);
*/

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
};

#endif
