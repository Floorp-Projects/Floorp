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
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef _NS_SCRIPT_SECURITY_MANAGER_H_
#define _NS_SCRIPT_SECURITY_MANAGER_H_

#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "jsapi.h"
#include "nsIXPCSecurityManager.h"
#include "nsHashtable.h"

enum { DOMPROP_MAX=892 };

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
    
    enum PolicyType {
        POLICY_TYPE_NONE = 0,
        POLICY_TYPE_DEFAULT = 1,
        POLICY_TYPE_PERDOMAIN = 2
    };

private:
    NS_IMETHOD
    GetSubjectPrincipal(JSContext *aCx, nsIPrincipal **result);

    NS_IMETHOD
    GetObjectPrincipal(JSContext *aCx, JSObject *aObj, nsIPrincipal **result);

    NS_IMETHOD
    CheckPermissions(JSContext *aCx, JSObject *aObj, const char *aCapability, 
                     PRBool* result);
    PRInt32 
    GetSecurityLevel(JSContext *cx, char *prop_name, PolicyType type, 
                     PRBool isWrite, char **capability);

    char *
    AddSecPolicyPrefix(JSContext *cx, char *pref_str, PolicyType type);

    char *
    GetSitePolicy(const char *org);

    NS_IMETHOD
    CheckXPCPermissions(JSContext *cx);

    NS_IMETHOD
    InitFromPrefs();

    nsIPrincipal *mSystemPrincipal;
    nsSupportsHashtable *mPrincipals;
    PolicyType domPropertyPolicyTypes[DOMPROP_MAX];
};

#endif /*_NS_SCRIPT_SECURITY_MANAGER_H_*/
