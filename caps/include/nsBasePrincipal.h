/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1999-2000 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 * Norris Boyd
 */

/* Shared implementation code for principals. */

#ifndef _NS_BASE_PRINCIPAL_H_
#define _NS_BASE_PRINCIPAL_H_

#include "jsapi.h"
#include "nsJSPrincipals.h"
#include "nsVoidArray.h"
#include "nsHashtable.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"

class nsBasePrincipal: public nsIPrincipal {
public:
          
    nsBasePrincipal();
    
    virtual ~nsBasePrincipal(void);

    NS_IMETHOD
    GetJSPrincipals(JSPrincipals **jsprin);

    NS_IMETHOD 
    CanEnableCapability(const char *capability, PRInt16 *result);
    
    NS_IMETHOD 
    SetCanEnableCapability(const char *capability, PRInt16 canEnable);
    
    NS_IMETHOD 
    IsCapabilityEnabled(const char *capability, void *annotation, 
                        PRBool *result);

    NS_IMETHOD 
    EnableCapability(const char *capability, void **annotation);

    NS_IMETHOD 
    RevertCapability(const char *capability, void **annotation);

    NS_IMETHOD 
    DisableCapability(const char *capability, void **annotation);
    
    NS_IMETHOD 
    GetPreferences(char** aPrefName, char** aID, 
                   char** aGrantedList, char** aDeniedList);

    nsresult
    InitFromPersistent(const char* aPrefName,const char* aID, 
                       const char* aGrantedList, const char* aDeniedList);

    NS_IMETHOD
    Read(nsIObjectInputStream* aStream);

    NS_IMETHOD
    Write(nsIObjectOutputStream* aStream);

    static const char Invalid[];

protected:
    enum AnnotationValue { AnnotationEnabled=1, AnnotationDisabled };

    NS_IMETHOD
    SetCapability(const char *capability, void **annotation, 
                  AnnotationValue value);

    nsJSPrincipals mJSPrincipals;
    nsVoidArray mAnnotations;
    nsHashtable *mCapabilities;
    char *mPrefName;
    static int mCapabilitiesOrdinal;
};

// special AddRef/Release to unify reference counts between XPCOM 
//  and JSPrincipals

#define NSBASEPRINCIPALS_ADDREF(className)                                  \
NS_IMETHODIMP_(nsrefcnt)                                                    \
className::AddRef(void)                                                     \
{                                                                           \
    NS_PRECONDITION(PRInt32(mRefCnt) == 0, "illegal mRefCnt");              \
    NS_PRECONDITION(PRInt32(mJSPrincipals.refcount) >= 0, "illegal refcnt");\
    nsrefcnt count = PR_AtomicIncrement((PRInt32 *)&mJSPrincipals.refcount);\
    NS_LOG_ADDREF(this, count, #className, sizeof(*this));                  \
    return count;                                                           \
}

#define NSBASEPRINCIPALS_RELEASE(className)                                 \
NS_IMETHODIMP_(nsrefcnt)                                                    \
className::Release(void)                                                    \
{                                                                           \
    NS_PRECONDITION(PRInt32(mRefCnt) == 0, "illegal mRefCnt");              \
    NS_PRECONDITION(0 != mJSPrincipals.refcount, "dup release");            \
    nsrefcnt count = PR_AtomicDecrement((PRInt32 *)&mJSPrincipals.refcount);\
    NS_LOG_RELEASE(this, count, #className);                                \
    if (count == 0) {                                                       \
        NS_DELETEXPCOM(this);                                               \
        return 0;                                                           \
    }                                                                       \
    return count;                                                           \
}

#endif // _NS_BASE_PRINCIPAL_H_
