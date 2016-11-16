/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_IPDLACTOR_H
#define MOZILLA_LAYERS_IPDLACTOR_H

#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/Unused.h"
#include "gfxPlatform.h"

namespace mozilla {
namespace layers {

/// A base class to facilitate the deallocation of IPDL actors.
///
/// Implements the parent side of the simple deallocation handshake.
/// Override the Destroy method rather than the ActorDestroy method.
template<typename Protocol>
class ParentActor : public Protocol
{
public:
  ParentActor() : mDestroyed(false) {}

  ~ParentActor() { MOZ_ASSERT(mDestroyed); }

  bool CanSend() const { return !mDestroyed; }

  // Override this rather than ActorDestroy
  virtual void Destroy() {}

  virtual mozilla::ipc::IPCResult RecvDestroy() override
  {
    DestroyIfNeeded();
    Unused << Protocol::Send__delete__(this);
    return IPC_OK();
  }

  typedef ipc::IProtocol::ActorDestroyReason Why;

  virtual void ActorDestroy(Why) override {
    DestroyIfNeeded();
  }

protected:
  void DestroyIfNeeded() {
    if (!mDestroyed) {
      Destroy();
      mDestroyed = true;
    }
  }

private:
  bool mDestroyed;
};

} // namespace
} // namespace

#endif
