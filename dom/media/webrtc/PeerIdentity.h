 /* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
  * vim: sw=2 ts=2 sts=2 expandtab
  * This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PeerIdentity_h
#define PeerIdentity_h

#ifdef MOZILLA_INTERNAL_API
#include "nsString.h"
#else
#include "nsStringAPI.h"
#endif

template <class T> class nsCOMPtr;
class nsIIDNService;

namespace mozilla {

/**
 * This class implements the identifier used in WebRTC identity.  Peers are
 * identified using a string in the form [<user>@]<domain>, for instance,
 * "user@example.com'. The (optional) user portion is a site-controlled string
 * containing any character other than '@'.  The domain portion is a valid IDN
 * domain name and is compared accordingly.
 *
 * See: http://tools.ietf.org/html/draft-ietf-rtcweb-security-arch-09#section-5.6.5.3.3.1
 */
class PeerIdentity final : public RefCounted<PeerIdentity>
{
public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(PeerIdentity)

  explicit PeerIdentity(const nsAString& aPeerIdentity)
    : mPeerIdentity(aPeerIdentity) {}
  ~PeerIdentity() {}

  bool Equals(const PeerIdentity& aOther) const;
  bool Equals(const nsAString& aOtherString) const;
  const nsString& ToString() const { return mPeerIdentity; }

private:
  static void GetUser(const nsAString& aPeerIdentity, nsAString& aUser);
  static void GetHost(const nsAString& aPeerIdentity, nsAString& aHost);

  static void GetNormalizedHost(const nsCOMPtr<nsIIDNService>& aIdnService,
                                const nsAString& aHost,
                                nsACString& aNormalizedHost);

  nsString mPeerIdentity;
};

inline bool
operator==(const PeerIdentity& aOne, const PeerIdentity& aTwo)
{
  return aOne.Equals(aTwo);
}

inline bool
operator==(const PeerIdentity& aOne, const nsAString& aString)
{
  return aOne.Equals(aString);
}

inline bool
operator!=(const PeerIdentity& aOne, const PeerIdentity& aTwo)
{
  return !aOne.Equals(aTwo);
}

inline bool
operator!=(const PeerIdentity& aOne, const nsAString& aString)
{
  return !aOne.Equals(aString);
}


} /* namespace mozilla */

#endif /* PeerIdentity_h */
