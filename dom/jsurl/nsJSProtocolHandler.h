/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsJSProtocolHandler_h___
#define nsJSProtocolHandler_h___

#include "mozilla/Attributes.h"
#include "nsIProtocolHandler.h"
#include "nsITextToSubURI.h"
#include "nsIURI.h"
#include "nsIMutable.h"
#include "nsISerializable.h"
#include "nsIClassInfo.h"
#include "nsSimpleURI.h"

#define NS_JSPROTOCOLHANDLER_CID                     \
{ /* bfc310d2-38a0-11d3-8cd3-0060b0fc14a3 */         \
    0xbfc310d2,                                      \
    0x38a0,                                          \
    0x11d3,                                          \
    {0x8c, 0xd3, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3} \
}

#define NS_JSURI_CID                                 \
{ /* 58f089ee-512a-42d2-a935-d0c874128930 */         \
    0x58f089ee,                                      \
    0x512a,                                          \
    0x42d2,                                          \
    {0xa9, 0x35, 0xd0, 0xc8, 0x74, 0x12, 0x89, 0x30} \
}

#define NS_JSPROTOCOLHANDLER_CONTRACTID \
    NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "javascript"


class nsJSProtocolHandler : public nsIProtocolHandler
{
public:
    NS_DECL_ISUPPORTS

    // nsIProtocolHandler methods:
    NS_DECL_NSIPROTOCOLHANDLER

    // nsJSProtocolHandler methods:
    nsJSProtocolHandler();

    static nsresult
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    nsresult Init();

protected:
    virtual ~nsJSProtocolHandler();

    nsresult EnsureUTF8Spec(const nsAFlatCString &aSpec, const char *aCharset, 
                            nsACString &aUTF8Spec);

    nsCOMPtr<nsITextToSubURI>  mTextToSubURI;
};


class nsJSURI : public nsSimpleURI
{
public:
    using nsSimpleURI::Read;
    using nsSimpleURI::Write;

    nsJSURI() {}

    explicit nsJSURI(nsIURI* aBaseURI) : mBaseURI(aBaseURI) {}

    nsIURI* GetBaseURI() const
    {
        return mBaseURI;
    }

    NS_DECL_ISUPPORTS_INHERITED

    // nsIURI overrides
    virtual nsSimpleURI* StartClone(RefHandlingEnum refHandlingMode) MOZ_OVERRIDE;

    // nsISerializable overrides
    NS_IMETHOD Read(nsIObjectInputStream* aStream) MOZ_OVERRIDE;
    NS_IMETHOD Write(nsIObjectOutputStream* aStream) MOZ_OVERRIDE;

    // Override the nsIClassInfo method GetClassIDNoAlloc to make sure our
    // nsISerializable impl works right.
    NS_IMETHOD GetClassIDNoAlloc(nsCID *aClassIDNoAlloc) MOZ_OVERRIDE;
    //NS_IMETHOD QueryInterface( const nsIID& aIID, void** aInstancePtr );

protected:
    virtual ~nsJSURI() {}

    virtual nsresult EqualsInternal(nsIURI* other,
                                    RefHandlingEnum refHandlingMode,
                                    bool* result) MOZ_OVERRIDE;
private:
    nsCOMPtr<nsIURI> mBaseURI;
};

#endif /* nsJSProtocolHandler_h___ */
