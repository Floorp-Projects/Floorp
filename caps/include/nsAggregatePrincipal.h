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

/* describes aggregate principals which combine the
   behavior of one or more other principals */

#ifndef _NS_AGGREGATE_PRINCIPAL_H_
#define _NS_AGGREGATE_PRINCIPAL_H_
#include "nsIAggregatePrincipal.h"
#include "nsICertificatePrincipal.h"
#include "nsICodebasePrincipal.h"
#include "nsBasePrincipal.h"
#include "nsCOMPtr.h"

#define NS_AGGREGATEPRINCIPAL_CID \
{ 0x867cf414, 0x1dd2, 0x11b2, \
{ 0x82, 0x66, 0xca, 0x64, 0x3b, 0xbc, 0x35, 0x64 }}
/* 867cf414-1dd2-11b2-8266-ca643bbc3564 */
class nsAggregatePrincipal : public nsIAggregatePrincipal,
                             public nsICertificatePrincipal,
                             public nsICodebasePrincipal,
                             public nsBasePrincipal 
{
public:

    NS_DEFINE_STATIC_CID_ACCESSOR(NS_AGGREGATEPRINCIPAL_CID)
    NS_DECL_ISUPPORTS
    NS_DECL_NSIAGGREGATEPRINCIPAL
    NS_FORWARD_NSICERTIFICATEPRINCIPAL(mCertificate->)
    NS_FORWARD_NSICODEBASEPRINCIPAL(mCodebase->)

    NS_IMETHOD 
    ToString(char **result);

    NS_IMETHOD 
    ToUserVisibleString(char **result);

    NS_IMETHOD 
    Equals(nsIPrincipal *other, PRBool *result);

    NS_IMETHOD 
    HashValue(PRUint32 *result);

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
    Save(nsSupportsHashtable* aPrincipals, nsIPref *prefs);
    
    nsAggregatePrincipal();

	virtual ~nsAggregatePrincipal(void);

protected:
    /* mCertificate and mCodebase have the same lifetime
       as the nsCOMPtrs mCertificatePrincipal and 
       mCodebasePrincipal, so they are not refcounted. 
    */
    nsCOMPtr<nsIPrincipal> mCertificatePrincipal;
    nsICertificatePrincipal*  mCertificate;
    nsCOMPtr<nsIPrincipal> mCodebasePrincipal;
    nsICodebasePrincipal*  mCodebase;

    /* This is the member principal (normally the certificate)
       which will handle the privilege checking and persistence
       behavior of the aggregate.
    */
    nsCOMPtr<nsIPrincipal> mMasterPrincipal;

};

#endif // _NS_AGGREGATE_PRINCIPAL_H_
