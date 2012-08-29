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
public:

  ImageBridgeParent(MessageLoop* aLoop);
  ~ImageBridgeParent();

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
  MessageLoop * mMessageLoop;
};

} // layers
} // mozilla

