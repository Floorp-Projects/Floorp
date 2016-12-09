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
// A same-process version may also be created to service non-e10s calls.

class OriginKeyStore;

class NonE10s
{
  typedef mozilla::ipc::IProtocol::ActorDestroyReason
      ActorDestroyReason;
public:
  virtual ~NonE10s() {}
protected:
  virtual mozilla::ipc::IPCResult RecvGetOriginKey(const uint32_t& aRequestId,
                                                   const nsCString& aOrigin,
                                                   const bool& aPersist) = 0;
  virtual mozilla::ipc::IPCResult RecvSanitizeOriginKeys(const uint64_t& aSinceWhen,
                                                         const bool& aOnlyPrivateBrowsing) = 0;
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) = 0;

  bool SendGetOriginKeyResponse(const uint32_t& aRequestId,
                                nsCString aKey);
};

// Super = PMediaParent or NonE10s

template<class Super>
class Parent : public Super
{
  typedef mozilla::ipc::IProtocol::ActorDestroyReason
      ActorDestroyReason;
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Parent<Super>)

  virtual mozilla::ipc::IPCResult RecvGetOriginKey(const uint32_t& aRequestId,
                                                   const nsCString& aOrigin,
                                                   const bool& aPersist) override;
  virtual mozilla::ipc::IPCResult RecvSanitizeOriginKeys(const uint64_t& aSinceWhen,
                                                         const bool& aOnlyPrivateBrowsing) override;
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  Parent();
private:
  virtual ~Parent();

  RefPtr<OriginKeyStore> mOriginKeyStore;
  bool mDestroyed;

  CoatCheck<Pledge<nsCString>> mOutstandingPledges;
};

template<class Parent>
mozilla::ipc::IPCResult IPCResult(Parent* aSelf, bool aSuccess);

template<>
inline mozilla::ipc::IPCResult IPCResult(Parent<PMediaParent>* aSelf, bool aSuccess)
{
  return aSuccess ? IPC_OK() : IPC_FAIL_NO_REASON(aSelf);
}

template<>
inline mozilla::ipc::IPCResult IPCResult(Parent<NonE10s>* aSelf, bool aSuccess)
{
  return IPC_OK();
}

PMediaParent* AllocPMediaParent();
bool DeallocPMediaParent(PMediaParent *aActor);

} // namespace media
} // namespace mozilla

#endif  // mozilla_MediaParent_h
