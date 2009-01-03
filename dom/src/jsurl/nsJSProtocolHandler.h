/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
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
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsJSProtocolHandler_h___
#define nsJSProtocolHandler_h___

#include "nsIProtocolHandler.h"
#include "nsITextToSubURI.h"
#include "nsIURI.h"
#include "nsIMutable.h"
#include "nsISerializable.h"
#include "nsIClassInfo.h"

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
    virtual ~nsJSProtocolHandler();

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    nsresult Init();

protected:

    nsresult EnsureUTF8Spec(const nsAFlatCString &aSpec, const char *aCharset, 
                            nsACString &aUTF8Spec);

    nsCOMPtr<nsITextToSubURI>  mTextToSubURI;
};

// Use an extra base object to avoid having to manually retype all the
// nsIURI methods.  I wish we could just inherit from nsSimpleURI instead.
class nsJSURI_base : public nsIURI,
                     public nsIMutable
{
public:
    nsJSURI_base(nsIURI* aSimpleURI) :
        mSimpleURI(aSimpleURI)
    {
        mMutable = do_QueryInterface(mSimpleURI);
        NS_ASSERTION(aSimpleURI && mMutable, "This isn't going to work out");
    }
    virtual ~nsJSURI_base() {}

    // For use only from deserialization
    nsJSURI_base() {}
    
    NS_FORWARD_NSIURI(mSimpleURI->)
    NS_FORWARD_NSIMUTABLE(mMutable->)

protected:
    nsCOMPtr<nsIURI> mSimpleURI;
    nsCOMPtr<nsIMutable> mMutable;
};

class nsJSURI : public nsJSURI_base,
                public nsISerializable,
                public nsIClassInfo
{
public:
    nsJSURI(nsIURI* aBaseURI, nsIURI* aSimpleURI) :
        nsJSURI_base(aSimpleURI), mBaseURI(aBaseURI)
    {}
    virtual ~nsJSURI() {}

    // For use only from deserialization
    nsJSURI() : nsJSURI_base() {}

    NS_DECL_ISUPPORTS
    NS_DECL_NSISERIALIZABLE
    NS_DECL_NSICLASSINFO

    // Override Clone() and Equals()
    NS_IMETHOD Clone(nsIURI** aClone);
    NS_IMETHOD Equals(nsIURI* aOther, PRBool *aResult);

    nsIURI* GetBaseURI() const {
        return mBaseURI;
    }

private:
    nsCOMPtr<nsIURI> mBaseURI;
};
    
#endif /* nsJSProtocolHandler_h___ */
