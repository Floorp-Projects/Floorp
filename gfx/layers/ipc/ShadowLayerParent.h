/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_ShadowLayerParent_h
#define mozilla_layers_ShadowLayerParent_h

#include "mozilla/Attributes.h"         // for override
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/layers/PLayerParent.h"  // for PLayerParent

namespace mozilla {
namespace layers {

class ContainerLayer;
class Layer;

class CanvasLayer;
class ColorLayer;
class TextLayer;
class ContainerLayer;
class ImageLayer;
class RefLayer;
class PaintedLayer;

class ShadowLayerParent : public PLayerParent
{
public:
  ShadowLayerParent();

  virtual ~ShadowLayerParent();

  void Bind(Layer* layer);
  void Destroy();

  Layer* AsLayer() const { return mLayer; }

  ContainerLayer* AsContainerLayer() const;
  CanvasLayer* AsCanvasLayer() const;
  ColorLayer* AsColorLayer() const;
  TextLayer* AsTextLayer() const;
  ImageLayer* AsImageLayer() const;
  BorderLayer* AsBorderLayer() const;
  RefLayer* AsRefLayer() const;
  PaintedLayer* AsPaintedLayer() const;

private:
  virtual void ActorDestroy(ActorDestroyReason why) override;

  void Disconnect();

  RefPtr<Layer> mLayer;
};

} // namespace layers
} // namespace mozilla

#endif // ifndef mozilla_layers_ShadowLayerParent_h
