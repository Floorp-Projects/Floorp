/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Igor Kushnirskiy <idk@eng.sun.com>
 */

#include "nsIGenericFactory.h"
#include "nsIModule.h"
#include "bcXPCOMWrappers.h"
#include "nsIServiceManager.h"
#include "nsXPIDLServiceManager.h"
#include "bcXPCOMWrappersCID.h"


NS_IMPL_THREADSAFE_ISUPPORTS1(bcXPCOMWrappers,bcIXPCOMWrappers);

bcXPCOMWrappers::bcXPCOMWrappers() {
    NS_INIT_REFCNT();
}

bcXPCOMWrappers::~bcXPCOMWrappers() {
}

NS_IMETHODIMP 
bcXPCOMWrappers::GetWrapper(nsISupports *wrapped,const nsIID & wrappedIID, nsIID * *wrapperIID, nsISupports **_retval) {
    nsresult r = NS_OK;
    if (wrappedIID.Equals(NS_GET_IID(nsIServiceManager))) {
        *_retval = new nsXPIDLServiceManager();
	*wrapperIID = (nsIID*) & NS_GET_IID(nsIXPIDLServiceManager);
	NS_ADDREF(*_retval);
    } else {
        r = NS_ERROR_FAILURE;
    }
    return r;
}


NS_GENERIC_FACTORY_CONSTRUCTOR(bcXPCOMWrappers);
static  nsModuleComponentInfo components[] =
{
    {
        "Black Connect XPCOM wrappers service",
        BC_XPCOMWRAPPERS_CID,
	BC_XPCOMWRAPPERS_CONTRACTID,
        bcXPCOMWrappersConstructor,
    }
};

NS_IMPL_NSGETMODULE("Black Connect XPCOM wrappers service",components);



