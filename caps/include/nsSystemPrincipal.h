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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* The privileged system principal. */
#ifndef _NS_SYSTEM_PRINCIPAL_H_
#define _NS_SYSTEM_PRINCIPAL_H_

#include "jsapi.h"
#include "nsIPrincipal.h"
#include "nsIURI.h"
#include "nsJSPrincipals.h"

// TODO: get new cid
#define NS_SYSTEMPRINCIPAL_CID \
{ 0x7ee2a400, 0x0c99, 0xaad3, \
{ 0xba, 0x18, 0xd7, 0x60, 0xb0, 0xf1, 0x99, 0xa2 }}

class nsSystemPrincipal : public nsIPrincipal {
public:
    
    //NS_DEFINE_STATIC_CID_ACCESSOR(NS_PRINCIPAL_CID)
        
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPRINCIPAL
    
    nsSystemPrincipal();
    
    NS_IMETHOD
    Init();

    virtual ~nsSystemPrincipal(void);

private:
    nsJSPrincipals mJSPrincipals;

};

#endif // _NS_SYSTEM_PRINCIPAL_H_
