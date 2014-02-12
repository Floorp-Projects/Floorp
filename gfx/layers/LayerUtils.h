/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_LAYERUTILS_H_
#define MOZILLA_LAYERS_LAYERUTILS_H_

#include "mozilla/gfx/2D.h"

namespace mozilla {
namespace layers {

void
PremultiplySurface(gfx::DataSourceSurface* srcSurface,
                   gfx::DataSourceSurface* destSurface = nullptr);

}
}

#endif /* MOZILLA_LAYERS_LAYERUTILS_H_ */
