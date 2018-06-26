/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_BUFFER_EDGE_PAD_H
#define MOZILLA_LAYERS_BUFFER_EDGE_PAD_H

#include "mozilla/gfx/2D.h"
#include "nsRegion.h"

namespace mozilla {
namespace layers {

void PadDrawTargetOutFromRegion(RefPtr<gfx::DrawTarget> aDrawTarget, nsIntRegion &aRegion);

} // namespace layers
} // namespace mozilla

#endif // MOZILLA_LAYERS_BUFFER_EDGE_PAD_H
