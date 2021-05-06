/* vim:set ts=4 sw=2 et cindent: */
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

class nsAuthSSPI final : public nsIAuthModule {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIAUTHMODULE

  explicit nsAuthSSPI(pType package = PACKAGE_TYPE_NEGOTIATE);

 private:
  ~nsAuthSSPI();

  void Reset();

  typedef ::TimeStamp MS_TimeStamp;

 private:
  nsresult MakeSN(const nsACString& principal, nsCString& result);

  CredHandle mCred;
  CtxtHandle mCtxt;
  nsCString mServiceName;
  uint32_t mServiceFlags;
  uint32_t mMaxTokenLen;
  pType mPackage;
  nsString mDomain;
  nsString mUsername;
  nsString mPassword;
  bool mIsFirst;
  void* mCertDERData;
  uint32_t mCertDERLength;
};

#endif /* nsAuthSSPI_h__ */
