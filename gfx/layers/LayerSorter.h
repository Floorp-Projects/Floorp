/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYERSORTER_H
#define GFX_LAYERSORTER_H

#include "nsTArray.h"

namespace mozilla {
namespace layers {

class Layer;

void SortLayersBy3DZOrder(nsTArray<Layer*>& aLayers);

} // namespace layers
} // namespace mozilla

#endif /* GFX_LAYERSORTER_H */
