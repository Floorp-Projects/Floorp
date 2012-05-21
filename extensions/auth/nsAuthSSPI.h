/* vim:set ts=4 sw=4 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAuthSSPI_h__
#define nsAuthSSPI_h__

#include "nsAuth.h"
#include "nsIAuthModule.h"
#include "nsString.h"

#include <windows.h>

#define SECURITY_WIN32 1
#include <ntsecapi.h>
#include <security.h>
#include <rpc.h>

// The nsNegotiateAuth class provides responses for the GSS-API Negotiate method
// as specified by Microsoft in draft-brezak-spnego-http-04.txt

// It can also be configured to talk raw NTLM.  This implementation of NTLM has
// the advantage of being able to access the user's logon credentials.  This
// implementation of NTLM should only be used for single-signon.  It should be
// avoided when authenticating over the internet since it may use a lower-grade
// version of password hashing depending on the version of Windows being used.

class nsAuthSSPI : public nsIAuthModule
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIAUTHMODULE

    nsAuthSSPI(pType package = PACKAGE_TYPE_NEGOTIATE);

private:
    ~nsAuthSSPI();

    void Reset();

private:
    CredHandle   mCred;
    CtxtHandle   mCtxt;
    nsCString    mServiceName;
    PRUint32     mServiceFlags;
    PRUint32     mMaxTokenLen;
    pType        mPackage;
    nsString     mDomain;
    nsString     mUsername;
    nsString     mPassword;
    bool         mIsFirst;	
    void*        mCertDERData; 
    PRUint32     mCertDERLength;
};

#endif /* nsAuthSSPI_h__ */
