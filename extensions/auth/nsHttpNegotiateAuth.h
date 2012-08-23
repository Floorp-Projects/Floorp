/* vim:set ts=4 sw=4 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpNegotiateAuth_h__
#define nsHttpNegotiateAuth_h__

#include "nsIHttpAuthenticator.h"
#include "nsIURI.h"
#include "nsSubstring.h"
#include "mozilla/Attributes.h"

// The nsHttpNegotiateAuth class provides responses for the GSS-API Negotiate method
// as specified by Microsoft in draft-brezak-spnego-http-04.txt

class nsHttpNegotiateAuth MOZ_FINAL : public nsIHttpAuthenticator
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIHTTPAUTHENTICATOR

private:
    // returns the value of the given boolean pref
    bool TestBoolPref(const char *pref);

    // tests if the host part of an uri is fully qualified
    bool TestNonFqdn(nsIURI *uri);

    // returns true if URI is accepted by the list of hosts in the pref
    bool TestPref(nsIURI *, const char *pref);

    bool MatchesBaseURI(const nsCSubstring &scheme,
                          const nsCSubstring &host,
                          int32_t             port,
                          const char         *baseStart,
                          const char         *baseEnd);
};
#endif /* nsHttpNegotiateAuth_h__ */
