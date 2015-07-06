/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_ImageContainerParent_h
#define mozilla_layers_ImageContainerParent_h

#include "mozilla/Attributes.h"         // for override
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/layers/PImageContainerParent.h"
#include "nsAutoPtr.h"                  // for nsRefPtr

namespace mozilla {
namespace layers {

class ImageHost;

class ImageContainerParent : public PImageContainerParent
{
public:
  ImageContainerParent() {}
  ~ImageContainerParent();

  virtual bool RecvAsyncDelete() override;

  nsAutoTArray<ImageHost*,1> mImageHosts;

private:
  virtual void ActorDestroy(ActorDestroyReason why) override {}
};

} // namespace layers
} // namespace mozilla

#endif // ifndef mozilla_layers_ImageContainerParent_h
