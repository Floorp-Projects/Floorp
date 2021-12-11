/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MIDIManagerParent_h
#define mozilla_dom_MIDIManagerParent_h

#include "mozilla/dom/PMIDIManagerParent.h"

namespace mozilla {
namespace dom {

/**
 * Actor implementation for the Parent (PBackground thread) side of MIDIManager
 * (represented in DOM by MIDIAccess). Manages actor lifetime so that we know
 * to shut down services when all MIDIManagers are gone.
 *
 */
class MIDIManagerParent final : public PMIDIManagerParent {
 public:
  NS_INLINE_DECL_REFCOUNTING(MIDIManagerParent);
  MIDIManagerParent() = default;
  mozilla::ipc::IPCResult RecvShutdown();
  void Teardown();
  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  ~MIDIManagerParent() = default;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MIDIManagerParent_h
