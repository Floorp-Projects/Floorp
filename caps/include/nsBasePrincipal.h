/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
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
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is  Communications Corporation.
 * Portions created by  Communications Corporation are
 * Copyright (C) 1999-2000  Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

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
