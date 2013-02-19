/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/PImageBridgeParent.h"

class MessageLoop;

namespace mozilla {
namespace layers {

class CompositorParent;
/**
 * ImageBridgeParent is the manager Protocol of ImageContainerParent.
 * It's purpose is mainly to setup the IPDL connection. Most of the
 * interesting stuff is in ImageContainerParent.
 */
class ImageBridgeParent : public PImageBridgeParent
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ImageBridgeParent)

public:
  ImageBridgeParent(MessageLoop* aLoop, Transport* aTransport);
  ~ImageBridgeParent();

  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  static PImageBridgeParent*
  Create(Transport* aTransport, ProcessId aOtherProcess);

  virtual PGrallocBufferParent*
  AllocPGrallocBuffer(const gfxIntSize&, const uint32_t&, const uint32_t&,
                      MaybeMagicGrallocBufferHandle*) MOZ_OVERRIDE;

  virtual bool
  DeallocPGrallocBuffer(PGrallocBufferParent* actor) MOZ_OVERRIDE;

  // Overriden from PImageBridgeParent.
  PImageContainerParent* AllocPImageContainer(uint64_t* aID) MOZ_OVERRIDE;
  // Overriden from PImageBridgeParent.
  bool DeallocPImageContainer(PImageContainerParent* toDealloc) MOZ_OVERRIDE;
  // Overriden from PImageBridgeParent.
  bool RecvStop() MOZ_OVERRIDE;

  MessageLoop * GetMessageLoop();

private:
  void DeferredDestroy();

  MessageLoop* mMessageLoop;
  Transport* mTransport;
  // This keeps us alive until ActorDestroy(), at which point we do a
  // deferred destruction of ourselves.
  nsRefPtr<ImageBridgeParent> mSelfRef;
};

} // layers
} // mozilla

