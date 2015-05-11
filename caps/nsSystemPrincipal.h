/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* The privileged system principal. */

#ifndef nsSystemPrincipal_h__
#define nsSystemPrincipal_h__

#include "nsIPrincipal.h"
#include "nsJSPrincipals.h"

#define NS_SYSTEMPRINCIPAL_CID \
{ 0x4a6212db, 0xaccb, 0x11d3, \
{ 0xb7, 0x65, 0x0, 0x60, 0xb0, 0xb6, 0xce, 0xcb }}
#define NS_SYSTEMPRINCIPAL_CONTRACTID "@mozilla.org/systemprincipal;1"


class nsSystemPrincipal final : public nsJSPrincipals
{
public:
    NS_DECL_NSIPRINCIPAL
    NS_DECL_NSISERIALIZABLE
    NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;

    nsSystemPrincipal() {}

    virtual void GetScriptLocation(nsACString &aStr) override;

protected:
    virtual ~nsSystemPrincipal(void) {}
};

#endif // nsSystemPrincipal_h__
