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

#ifndef nsAbRDFResource_h__
#define nsAbRDFResource_h__

#include "nsRDFResource.h"
#include "nsIAbCard.h"
#include "nsIAbDirectory.h"
#include "nsISupportsArray.h"
#include "nsCOMPtr.h"
#include "nsDirPrefs.h"
#include "nsIAddrDatabase.h"
#include "nsIAddrDBListener.h"

 /* 
  * Address Book RDF Resources and DB listener
  */ 

class nsAbRDFResource: public nsRDFResource, public nsIAddrDBListener
{
public: 
	nsAbRDFResource(void);
	virtual ~nsAbRDFResource(void);

	NS_DECL_ISUPPORTS_INHERITED

	// nsIAddrDBListener
	NS_IMETHOD OnCardAttribChange(PRUint32 abCode, nsIAddrDBListener *instigator);
	NS_IMETHOD OnCardEntryChange(PRUint32 abCode, nsIAbCard *card, nsIAddrDBListener *instigator);
	NS_IMETHOD OnAnnouncerGoingAway(nsIAddrDBAnnouncer *instigator);

protected:

	nsresult GetAbDatabase();

	nsCOMPtr<nsIAddrDatabase> mDatabase;  
 
};

#endif
