/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1999-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Norris Boyd
 */

/* The privileged system principal. */

#ifndef _NS_SYSTEM_PRINCIPAL_H_
#define _NS_SYSTEM_PRINCIPAL_H_

#include "nsBasePrincipal.h"

#define NS_SYSTEMPRINCIPAL_CLASSNAME "systemprincipal"
#define NS_SYSTEMPRINCIPAL_CID \
{ 0x4a6212db, 0xaccb, 0x11d3, \
{ 0xb7, 0x65, 0x0, 0x60, 0xb0, 0xb6, 0xce, 0xcb }}
#define NS_SYSTEMPRINCIPAL_CONTRACTID "@mozilla.org/systemprincipal;1"


class nsSystemPrincipal : public nsBasePrincipal {
public:
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSISERIALIZABLE
    
    NS_IMETHOD ToString(char **result);

    NS_IMETHOD ToUserVisibleString(char **result);

    NS_IMETHOD Equals(nsIPrincipal *other, PRBool *result);

    NS_IMETHOD HashValue(PRUint32 *result);

    NS_IMETHOD CanEnableCapability(const char *capability, PRInt16 *result);

    NS_IMETHOD SetCanEnableCapability(const char *capability, 
                                      PRInt16 canEnable);

    NS_IMETHOD IsCapabilityEnabled(const char *capability, void * annotation, 
                                   PRBool *result);

    NS_IMETHOD EnableCapability(const char *capability, void * *annotation);

    NS_IMETHOD RevertCapability(const char *capability, void * *annotation);

    NS_IMETHOD DisableCapability(const char *capability, void * *annotation);

    NS_IMETHOD GetPreferences(char** aPrefName, char** aID, 
                              char** aGrantedList, char** aDeniedList);
    
    NS_IMETHOD Init();

    nsSystemPrincipal();

    virtual ~nsSystemPrincipal(void);
};

#endif // _NS_SYSTEM_PRINCIPAL_H_
