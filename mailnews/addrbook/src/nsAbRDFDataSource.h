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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsAbRDFDataSource_h__
#define nsAbRDFDataSource_h__


#include "nsCOMPtr.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFService.h"
#include "nsISupportsArray.h"
#include "nsString.h"

/**
 * The addressbook data source.
 */
class nsAbRDFDataSource : public nsIRDFDataSource
{
private:
	nsCOMPtr<nsISupportsArray> mObservers;
	PRBool		mInitialized;

	// The cached service managers

	nsIRDFService*	mRDFService;

public:
  
	NS_DECL_ISUPPORTS
  NS_DECL_NSIRDFDATASOURCE
	nsAbRDFDataSource(void);
	virtual ~nsAbRDFDataSource(void);
	virtual nsresult Init();
  
protected:

	nsresult createNode(nsString& str, nsIRDFNode **node);
	nsresult createNode(PRUint32 value, nsIRDFNode **node);

	nsresult NotifyPropertyChanged(nsIRDFResource *resource, nsIRDFResource *propertyResource,
									const PRUnichar *oldValue, const PRUnichar *newValue);

	nsresult NotifyObservers(nsIRDFResource *subject, nsIRDFResource *property,
							 nsIRDFNode *object, PRBool assert, PRBool change);

	static PRBool assertEnumFunc(nsISupports *aElement, void *aData);
	static PRBool unassertEnumFunc(nsISupports *aElement, void *aData);
	static PRBool changeEnumFunc(nsISupports *aElement, void *aData);

};

#endif
