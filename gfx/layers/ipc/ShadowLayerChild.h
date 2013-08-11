/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_ShadowLayerChild_h
#define mozilla_layers_ShadowLayerChild_h

#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/layers/PLayerChild.h"  // for PLayerChild

namespace mozilla {
namespace layers {

class ShadowableLayer;

class ShadowLayerChild : public PLayerChild
{
public:
  ShadowLayerChild(ShadowableLayer* aLayer);
  virtual ~ShadowLayerChild();

  ShadowableLayer* layer() const { return mLayer; }

protected:
  virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

private:
  ShadowableLayer* mLayer;
};

} // namespace layers
} // namespace mozilla

#endif // ifndef mozilla_layers_ShadowLayerChild_h
