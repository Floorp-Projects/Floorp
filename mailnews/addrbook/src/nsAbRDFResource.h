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
  NS_DECL_NSIADDRDBLISTENER

protected:

	nsresult GetAbDatabase();

	nsCOMPtr<nsIAddrDatabase> mDatabase;  
 
};

#endif
