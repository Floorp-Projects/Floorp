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
 * Copyright (C) 1998-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Norris Boyd 
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 03/27/2000   IBM Corp.       Added PR_CALLBACK for Optlink
 *                               use in OS2
 */

#ifndef _NS_SCRIPT_SECURITY_MANAGER_H_
#define _NS_SCRIPT_SECURITY_MANAGER_H_

#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "jsapi.h"
#include "jsdbgapi.h"
#include "nsIXPCSecurityManager.h"
#include "nsHashtable.h"
#include "nsDOMPropEnums.h"
#include "nsCOMPtr.h"
#include "nsIPref.h"
#include "nsISecurityPref.h"

#include "nsIJSContextStack.h"

/////////////////////
// nsIPrincipalKey //
/////////////////////

class nsIPrincipalKey : public nsHashKey {
public:
    nsIPrincipalKey(nsIPrincipal* key) {
        mKey = key;
        NS_IF_ADDREF(mKey);
    }
    
    ~nsIPrincipalKey(void) {
        NS_IF_RELEASE(mKey);
    }
    
    PRUint32 HashCode(void) const {
        PRUint32 hash;
        mKey->HashValue(&hash);
        return hash;
    }
    
    PRBool Equals(const nsHashKey *aKey) const {
        PRBool eq;
        mKey->Equals(((nsIPrincipalKey *) aKey)->mKey, &eq);
        return eq;
    }
    
    nsHashKey *Clone(void) const {
        return new nsIPrincipalKey(mKey);
    }

protected:
    nsIPrincipal* mKey;
};

#define NS_SCRIPTSECURITYMANAGER_CID \
{ 0x7ee2a4c0, 0x4b93, 0x17d3, \
{ 0xba, 0x18, 0x00, 0x60, 0xb0, 0xf1, 0x99, 0xa2 }}

class nsScriptSecurityManager : public nsIScriptSecurityManager,
                                public nsIXPCSecurityManager
{
public:
    nsScriptSecurityManager();
    virtual ~nsScriptSecurityManager();
    
    NS_DEFINE_STATIC_CID_ACCESSOR(NS_SCRIPTSECURITYMANAGER_CID)
        
    NS_DECL_ISUPPORTS
    NS_DECL_NSISCRIPTSECURITYMANAGER
    NS_DECL_NSIXPCSECURITYMANAGER
    
    static nsScriptSecurityManager *
    GetScriptSecurityManager();

    JSContext * GetCurrentContextQuick();
private:

    NS_IMETHOD
    GetSubjectPrincipal(JSContext *aCx, nsIPrincipal **result);

    NS_IMETHOD
    GetObjectPrincipal(JSContext *aCx, JSObject *aObj, nsIPrincipal **result);

    NS_IMETHOD
    CheckPermissions(JSContext *aCx, JSObject *aObj, const char *aCapability);
    
    PRInt32 
    GetSecurityLevel(nsIPrincipal *principal, nsDOMProp domProp, 
                     PRBool isWrite, nsCString &capability);

    NS_IMETHOD
    GetPrefName(nsIPrincipal *principal, nsDOMProp domProp, 
                nsCString &result);

    nsresult 
    CheckXPCCapability(JSContext *aJSContext, const char *aCapability);
 
    NS_IMETHOD
    CheckXPCPermissions(JSContext *cx, nsISupports* aObj);

    NS_IMETHOD
    GetFramePrincipal(JSContext *cx, JSStackFrame *fp, nsIPrincipal **result);
                                                     
    NS_IMETHOD
    GetScriptPrincipal(JSContext *cx, JSScript *script, nsIPrincipal **result);

    NS_IMETHOD
    GetFunctionObjectPrincipal(JSContext *cx, JSObject *obj, 
                               nsIPrincipal **result);

    NS_IMETHOD
    GetPrincipalAndFrame(JSContext *cx, nsIPrincipal **result, 
                         JSStackFrame **frameResult);

    NS_IMETHOD
    SavePrincipal(nsIPrincipal* aToSave);

    NS_IMETHOD
    InitPrefs();

    static nsresult 
    PrincipalPrefNames(const char* pref, char** grantedPref, char** deniedPref);

    static void
    EnumeratePolicyCallback(const char *prefName, void *data);

    static void
    EnumeratePrincipalsCallback(const char *prefName, void *data);

    static int PR_CALLBACK
    JSEnabledPrefChanged(const char *pref, void *data);

    static int PR_CALLBACK
    PrincipalPrefChanged(const char *pref, void *data);

    nsObjectHashtable *mOriginToPolicyMap;
    nsCOMPtr<nsIPref> mPrefs;
    nsCOMPtr<nsISecurityPref> mSecurityPrefs;
    nsIPrincipal *mSystemPrincipal;
    nsCOMPtr<nsIPrincipal> mSystemCertificate;
    nsSupportsHashtable *mPrincipals;
    PRBool mIsJavaScriptEnabled;
    PRBool mIsMailJavaScriptEnabled;
    PRBool mIsWritingPrefs;
    unsigned char hasDomainPolicyVector[(NS_DOM_PROP_MAX >> 3) + 1];
    nsCOMPtr<nsIJSContextStack> mThreadJSContextStack;
};

#endif /*_NS_SCRIPT_SECURITY_MANAGER_H_*/
