/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_MediaChild_h
#define mozilla_MediaChild_h

#include "mozilla/dom/ContentChild.h"
#include "mozilla/media/PMediaChild.h"
#include "mozilla/media/PMediaParent.h"
#include "MediaUtils.h"

namespace mozilla {
namespace media {

// media::Child implements proxying to the chrome process for some media-related
// functions, for the moment just:
//
// GetOriginKey() - get a cookie-like persisted unique key for a given origin.
// SanitizeOriginKeys() - reset persisted unique keys.

// GetOriginKey and SanitizeOriginKeys are asynchronous APIs that return pledges
// (promise-like objects) with the future value. Use pledge.Then(func) to access.

already_AddRefed<Pledge<nsCString>>
GetOriginKey(const nsCString& aOrigin, bool aPrivateBrowsing, bool aPersist);

void
SanitizeOriginKeys(const uint64_t& aSinceWhen, bool aOnlyPrivateBrowsing);

class Child : public PMediaChild
{
public:
  static Child* Get();

  Child();

  bool RecvGetOriginKeyResponse(const uint32_t& aRequestId, const nsCString& aKey) override;

  void ActorDestroy(ActorDestroyReason aWhy) override;
  virtual ~Child();
private:

  bool mActorDestroyed;
};

PMediaChild* AllocPMediaChild();
bool DeallocPMediaChild(PMediaChild *aActor);

} // namespace media
} // namespace mozilla

#endif  // mozilla_MediaChild_h
