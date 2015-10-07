/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientContainerLayer.h"
#include "ClientLayerManager.h"         // for ClientLayerManager, etc
#include "mozilla/mozalloc.h"           // for operator new
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsISupportsImpl.h"            // for Layer::AddRef, etc

namespace mozilla {
namespace layers {

already_AddRefed<ContainerLayer>
ClientLayerManager::CreateContainerLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<ClientContainerLayer> layer =
    new ClientContainerLayer(this);
  CREATE_SHADOW(Container);
  return layer.forget();
}

already_AddRefed<RefLayer>
ClientLayerManager::CreateRefLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<ClientRefLayer> layer =
    new ClientRefLayer(this);
  CREATE_SHADOW(Ref);
  return layer.forget();
}

} // namespace layers
} // namespace mozilla
