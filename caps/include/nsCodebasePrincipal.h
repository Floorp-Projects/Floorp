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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/* describes principals by their orginating uris*/
#ifndef _NS_CODEBASE_PRINCIPAL_H_
#define _NS_CODEBASE_PRINCIPAL_H_

#include "jsapi.h"
#include "nsICodebasePrincipal.h"
#include "nsIURI.h"
#include "nsJSPrincipals.h"

#define NS_CODEBASEPRINCIPAL_CID \
{ 0x7ee2a400, 0x0b91, 0xaad3, \
{ 0xba, 0x18, 0xd7, 0x60, 0xb0, 0xf1, 0x99, 0xa2 }}

class nsCodebasePrincipal : public nsICodebasePrincipal {
public:
    
    NS_DEFINE_STATIC_CID_ACCESSOR(NS_CODEBASEPRINCIPAL_CID)
        
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPRINCIPAL
    NS_DECL_NSICODEBASEPRINCIPAL
    
    nsCodebasePrincipal();
    
    NS_IMETHOD
    Init(nsIURI *uri);

    virtual ~nsCodebasePrincipal(void);
    
protected:
    nsIURI *mURI;
    nsJSPrincipals mJSPrincipals;
};

#endif // _NS_CODEBASE_PRINCIPAL_H_
