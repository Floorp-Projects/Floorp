/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientCanvasLayer.h"
#include "GeckoProfiler.h"              // for AUTO_PROFILER_LABEL
#include "ClientLayerManager.h"         // for ClientLayerManager, etc
#include "nsCOMPtr.h"                   // for already_AddRefed

namespace mozilla {
namespace layers {

ClientCanvasLayer::~ClientCanvasLayer()
{
  MOZ_COUNT_DTOR(ClientCanvasLayer);
}

void
ClientCanvasLayer::RenderLayer()
{
  AUTO_PROFILER_LABEL("ClientCanvasLayer::RenderLayer", GRAPHICS);

  RenderMaskLayers(this);

  UpdateCompositableClient();
  ClientManager()->Hold(this);
}

already_AddRefed<CanvasLayer>
ClientLayerManager::CreateCanvasLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  RefPtr<ClientCanvasLayer> layer =
    new ClientCanvasLayer(this);
  CREATE_SHADOW(Canvas);
  return layer.forget();
}

} // namespace layers
} // namespace mozilla
