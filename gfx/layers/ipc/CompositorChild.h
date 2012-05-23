/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_CompositorChild_h
#define mozilla_layers_CompositorChild_h

#include "mozilla/layers/PCompositorChild.h"

namespace mozilla {
namespace layers {

class LayerManager;
class CompositorParent;

class CompositorChild : public PCompositorChild
{
  NS_INLINE_DECL_REFCOUNTING(CompositorChild)
public:
  CompositorChild(LayerManager *aLayerManager);
  virtual ~CompositorChild();

  void Destroy();

protected:
  virtual PLayersChild* AllocPLayers(const LayersBackend &aBackend, int* aMaxTextureSize);
  virtual bool DeallocPLayers(PLayersChild *aChild);

private:
  nsRefPtr<LayerManager> mLayerManager;

  DISALLOW_EVIL_CONSTRUCTORS(CompositorChild);
};

} // layers
} // mozilla

#endif // mozilla_layers_CompositorChild_h
