/* vim:set ts=4 sw=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpNegotiateAuth_h__
#define nsHttpNegotiateAuth_h__

#include "nsIHttpAuthenticator.h"
#include "nsIURI.h"
#include "mozilla/Attributes.h"
#include "mozilla/LazyIdleThread.h"
#include "mozilla/StaticPtr.h"

// The nsHttpNegotiateAuth class provides responses for the GSS-API Negotiate
// method as specified by Microsoft in draft-brezak-spnego-http-04.txt

class nsHttpNegotiateAuth final : public nsIHttpAuthenticator {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIHTTPAUTHENTICATOR

  static already_AddRefed<nsIHttpAuthenticator> GetOrCreate();

 private:
  ~nsHttpNegotiateAuth() {}

  // returns the value of the given boolean pref
  bool TestBoolPref(const char* pref);

  // tests if the host part of an uri is fully qualified
  bool TestNonFqdn(nsIURI* uri);

  // Thread for GenerateCredentialsAsync
  RefPtr<mozilla::LazyIdleThread> mNegotiateThread;

  // Singleton pointer
  static mozilla::StaticRefPtr<nsHttpNegotiateAuth> gSingleton;
};
#endif /* nsHttpNegotiateAuth_h__ */
