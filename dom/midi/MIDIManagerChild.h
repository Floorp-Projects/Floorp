/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MIDIManagerChild_h
#define mozilla_dom_MIDIManagerChild_h

#include "mozilla/dom/PMIDIManagerChild.h"

namespace mozilla::dom {

/**
 * Actor implementation for the Child side of MIDIManager (represented in DOM by
 * MIDIAccess). Manages actor lifetime so that we know to shut down services
 * when all MIDIManagers are gone. Also receives port list update on MIDIAccess
 * object creation.
 *
 */
class MIDIManagerChild final : public PMIDIManagerChild {
 public:
  NS_INLINE_DECL_REFCOUNTING(MIDIManagerChild)

  MIDIManagerChild();
  mozilla::ipc::IPCResult RecvMIDIPortListUpdate(const MIDIPortList& aPortList);
  void Shutdown();

 private:
  ~MIDIManagerChild() = default;
  bool mShutdown;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_MIDIManagerChild_h
