/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MIDIPortChild_h
#define mozilla_dom_MIDIPortChild_h

#include "mozilla/dom/MIDIPortInterface.h"
#include "mozilla/dom/PMIDIPortChild.h"

namespace mozilla::dom {

class MIDIPort;
class MIDIPortInfo;

/**
 * Child actor for a MIDIPort object. Each MIDIPort DOM object in JS has a its
 * own child actor. The lifetime of the actor object is dependent on the
 * lifetime of the JS object.
 *
 */
class MIDIPortChild final : public PMIDIPortChild, public MIDIPortInterface {
 public:
  NS_INLINE_DECL_REFCOUNTING(MIDIPortChild, override);
  mozilla::ipc::IPCResult RecvReceive(nsTArray<MIDIMessage>&& aMsgs);

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvUpdateStatus(const uint32_t& aDeviceState,
                                           const uint32_t& aConnectionState);

  MIDIPortChild(const MIDIPortInfo& aPortInfo, bool aSysexEnabled,
                MIDIPort* aPort);
  nsresult GenerateStableId(const nsACString& aOrigin);
  const nsString& StableId() { return mStableId; };

  void DetachOwner() { mDOMPort = nullptr; }

 private:
  ~MIDIPortChild() = default;
  // Pointer to the DOM object this actor represents. The actor cannot outlive
  // the DOM object.
  MIDIPort* mDOMPort;
  nsString mStableId;
};
}  // namespace mozilla::dom

#endif
