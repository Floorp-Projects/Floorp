/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_MediaChild_h
#define mozilla_MediaChild_h

#include "mozilla/media/PMediaChild.h"
#include "mozilla/media/PMediaParent.h"
#include "MediaUtils.h"

namespace mozilla {

namespace ipc {
class PrincipalInfo;
}

namespace media {

typedef MozPromise<nsCString, nsresult, false> PrincipalKeyPromise;

// media::Child implements proxying to the chrome process for some media-related
// functions, for the moment just:
//
// GetPrincipalKey() - get a cookie-like persisted unique key for a given
// principalInfo.
//
// SanitizeOriginKeys() - reset persisted unique keys.

// GetPrincipalKey and SanitizeOriginKeys are asynchronous APIs that return
// pledges (promise-like objects) with the future value. Use pledge.Then(func)
// to access.

RefPtr<PrincipalKeyPromise> GetPrincipalKey(
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo, bool aPersist);

void SanitizeOriginKeys(const uint64_t& aSinceWhen, bool aOnlyPrivateBrowsing);

class Child : public PMediaChild {
 public:
  static Child* Get();

  Child();

  void ActorDestroy(ActorDestroyReason aWhy) override;
  virtual ~Child();

 private:
  bool mActorDestroyed;
};

PMediaChild* AllocPMediaChild();
bool DeallocPMediaChild(PMediaChild* aActor);

}  // namespace media
}  // namespace mozilla

#endif  // mozilla_MediaChild_h
