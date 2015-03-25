/* vim:set ts=4 sw=4 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAuthGSSAPI_h__
#define nsAuthGSSAPI_h__

#include "nsAuth.h"
#include "nsIAuthModule.h"
#include "nsString.h"
#include "mozilla/Attributes.h"

#define GSS_USE_FUNCTION_POINTERS 1

#include "gssapi.h"

// The nsAuthGSSAPI class provides responses for the GSS-API Negotiate method
// as specified by Microsoft in draft-brezak-spnego-http-04.txt

/* Some remarks on thread safety ...
 *
 * The thread safety of this class depends largely upon the thread safety of
 * the underlying GSSAPI and Kerberos libraries. This code just loads the 
 * system GSSAPI library, and whilst it avoids loading known bad libraries, 
 * it cannot determine the thread safety of the the code it loads.
 *
 * When used with a non-threadsafe library, it is not safe to simultaneously 
 * use multiple instantiations of this class.
 *
 * When used with a threadsafe Kerberos library, multiple instantiations of 
 * this class may happily co-exist. Methods may be sequentially called from 
 * multiple threads. The nature of the GSSAPI protocol is such that a correct 
 * implementation will never call methods in parallel, as the results of the 
 * last call are required as input to the next.
 */

class nsAuthGSSAPI final : public nsIAuthModule
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIAUTHMODULE

    explicit nsAuthGSSAPI(pType package);

    static void Shutdown();

private:
    ~nsAuthGSSAPI() { Reset(); }

    void    Reset();
    gss_OID GetOID() { return mMechOID; }

private:
    gss_ctx_id_t mCtx;
    gss_OID      mMechOID;
    nsCString    mServiceName;
    uint32_t     mServiceFlags;
    nsString     mUsername;
    bool         mComplete;
};

#endif /* nsAuthGSSAPI_h__ */
