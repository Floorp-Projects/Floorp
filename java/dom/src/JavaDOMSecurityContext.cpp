/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 */
#include "JavaDOMSecurityContext.h"

static NS_DEFINE_IID(kISecurityContextIID, NS_ISECURITYCONTEXT_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

NS_IMPL_ADDREF(JavaDOMSecurityContext);
NS_IMPL_RELEASE(JavaDOMSecurityContext);

JavaDOMSecurityContext::JavaDOMSecurityContext() {
    NS_INIT_REFCNT();
}

JavaDOMSecurityContext::~JavaDOMSecurityContext() {
}
NS_METHOD
JavaDOMSecurityContext::QueryInterface(const nsIID& aIID, void** aInstancePtr)
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
JavaDOMSecurityContext::Implies(const char* target, const char* action, PRBool *bAllowedAccess)
{
    *bAllowedAccess = PR_TRUE;
    return NS_OK;
}


