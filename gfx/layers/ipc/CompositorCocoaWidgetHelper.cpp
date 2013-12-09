/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorParent.h"
#include "CompositorCocoaWidgetHelper.h"
#include "nsDebug.h"

namespace mozilla {
namespace layers {
namespace compositor {

LayerManagerComposite*
GetLayerManager(CompositorParent* aParent)
{
  return aParent->GetLayerManager();
}


}
}
}
