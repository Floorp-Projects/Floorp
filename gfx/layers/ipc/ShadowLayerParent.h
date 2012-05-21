/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_ShadowLayerParent_h
#define mozilla_layers_ShadowLayerParent_h

#include "mozilla/layers/PLayerParent.h"

namespace mozilla {
namespace layers {

class ContainerLayer;
class Layer;
class LayerManager;
class ShadowLayersParent;

class ShadowLayerParent : public PLayerParent
{
public:
  ShadowLayerParent();

  virtual ~ShadowLayerParent();

  void Bind(Layer* layer);
  void Destroy();

  Layer* AsLayer() const { return mLayer; }
  ContainerLayer* AsContainer() const;

private:
  NS_OVERRIDE
  virtual void ActorDestroy(ActorDestroyReason why);

  nsRefPtr<Layer> mLayer;
};

} // namespace layers
} // namespace mozilla

#endif // ifndef mozilla_layers_ShadowLayerParent_h
