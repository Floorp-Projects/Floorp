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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsMorkCID.h"
#include "nsIMdbFactoryFactory.h"
#include "mdb.h"

class nsMorkFactoryFactory : public nsIMdbFactoryFactory
{
public:
	nsMorkFactoryFactory();
	// nsISupports methods
	NS_DECL_ISUPPORTS 

	NS_IMETHOD GetMdbFactory(nsIMdbFactory **aFactory);

};

NS_GENERIC_FACTORY_CONSTRUCTOR(nsMorkFactoryFactory)

static nsModuleComponentInfo components[] =
{
    { "Mork Factory", 
      NS_MORK_CID, 
      nsnull,
      nsMorkFactoryFactoryConstructor 
    }
};

NS_IMPL_NSGETMODULE(nsMorkModule, components);




static nsIMdbFactory *gMDBFactory = nsnull;

NS_IMPL_ADDREF(nsMorkFactoryFactory)
NS_IMPL_RELEASE(nsMorkFactoryFactory)

NS_IMETHODIMP
nsMorkFactoryFactory::QueryInterface(REFNSIID iid, void** result)
{
	if (! result)
		return NS_ERROR_NULL_POINTER;

	*result = nsnull;
    if(iid.Equals(NS_GET_IID(nsIMdbFactoryFactory)) ||
		iid.Equals(NS_GET_IID(nsISupports))) {
		*result = NS_STATIC_CAST(nsIMdbFactoryFactory*, this);
		AddRef();
		return NS_OK;
	}
    return NS_NOINTERFACE;
}



nsMorkFactoryFactory::nsMorkFactoryFactory()
{
	NS_INIT_REFCNT();
}

NS_IMETHODIMP nsMorkFactoryFactory::GetMdbFactory(nsIMdbFactory **aFactory)
{
	if (!gMDBFactory)
		gMDBFactory = MakeMdbFactory();
	*aFactory = gMDBFactory;
	return (gMDBFactory) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

