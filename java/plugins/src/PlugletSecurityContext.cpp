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
 */
#include "PlugletSecurityContext.h"

static NS_DEFINE_IID(kISecurityContextIID, NS_ISECURITYCONTEXT_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

NS_IMPL_ADDREF(PlugletSecurityContext);
NS_IMPL_RELEASE(PlugletSecurityContext);

PlugletSecurityContext::PlugletSecurityContext() {
    NS_INIT_REFCNT();
}

PlugletSecurityContext::~PlugletSecurityContext() {
}
NS_METHOD
PlugletSecurityContext::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {                                            
        return NS_ERROR_NULL_POINTER;                                        
    }   
    *aInstancePtr = NULL;
    if (aIID.Equals(kISecurityContextIID) ||
        aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (nsISecurityContext*) this;
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE; 
}


NS_METHOD 
PlugletSecurityContext::Implies(const char* target, const char* action, PRBool *bAllowedAccess)
{
    *bAllowedAccess = PR_TRUE;
    return NS_OK;
}


