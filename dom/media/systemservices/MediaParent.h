/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_MediaParent_h
#define mozilla_MediaParent_h

#include "MediaChild.h"

#include "mozilla/dom/ContentParent.h"
#include "mozilla/media/PMediaParent.h"

namespace mozilla {
namespace media {

// media::Parent implements the chrome-process side of ipc for media::Child APIs

class ParentSingleton;

class Parent : public PMediaParent
{
  NS_INLINE_DECL_REFCOUNTING(Parent)
public:
  virtual bool RecvGetOriginKey(const uint32_t& aRequestId,
                                const nsCString& aOrigin,
                                const bool& aPrivateBrowsing) override;
  virtual bool RecvSanitizeOriginKeys(const uint64_t& aSinceWhen) override;
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  Parent();
private:
  virtual ~Parent();

  nsRefPtr<ParentSingleton> mSingleton;
  bool mDestroyed;
};

PMediaParent* AllocPMediaParent();
bool DeallocPMediaParent(PMediaParent *aActor);

} // namespace media
} // namespace mozilla

#endif  // mozilla_MediaParent_h
