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
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All 
 * Rights Reserved.
 *
 * Contributor(s):
 * Norris Boyd
 * Mitch Stoltz
 */

/* describes principals for use with signed scripts */

#ifndef _NS_CERTIFICATE_PRINCIPAL_H_
#define _NS_CERTIFICATE_PRINCIPAL_H_
#include "jsapi.h"
#include "nsICertificatePrincipal.h"
#include "nsBasePrincipal.h"

class nsIURI;

#define NS_CERTIFICATEPRINCIPAL_CID \
{ 0x7ee2a4c0, 0x4b91, 0x11d3, \
{ 0xba, 0x18, 0x00, 0x60, 0xb0, 0xf1, 0x99, 0xa2 }}

class nsCertificatePrincipal : public nsICertificatePrincipal, public nsBasePrincipal 
{
public:

    NS_DEFINE_STATIC_CID_ACCESSOR(NS_CERTIFICATEPRINCIPAL_CID)
    NS_DECL_ISUPPORTS
    NS_DECL_NSISERIALIZABLE
    NS_DECL_NSICERTIFICATEPRINCIPAL

    NS_IMETHOD ToString(char **result);

    NS_IMETHOD ToUserVisibleString(char **result);
    
    NS_IMETHOD GetPreferences(char** aPrefName, char** aID, 
                              char** aGrantedList, char** aDeniedList);
    
    NS_IMETHOD Equals(nsIPrincipal *other, PRBool *result);

    NS_IMETHOD HashValue(PRUint32 *result);

    NS_IMETHOD CanEnableCapability(const char *capability, PRInt16 *result);

    NS_IMETHOD Init(const char* aCertificateID);

    nsresult InitFromPersistent(const char* aPrefName, const char* aID, 
                                const char* aGrantedList, const char* aDeniedList);

    nsCertificatePrincipal();

	virtual ~nsCertificatePrincipal(void);

protected:
    char* mCertificateID;
    char* mCommonName;
};

#endif // _NS_CERTIFICATE_PRINCIPAL_H_
