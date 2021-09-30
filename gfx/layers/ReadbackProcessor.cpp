/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ReadbackProcessor.h"
#include <sys/types.h>       // for int32_t
#include "Layers.h"          // for Layer, PaintedLayer, etc
#include "ReadbackLayer.h"   // for ReadbackLayer, ReadbackSink
#include "UnitTransforms.h"  // for ViewAs
#include "Units.h"           // for ParentLayerIntRect
#include "gfxContext.h"      // for gfxContext
#include "gfxUtils.h"
#include "gfxRect.h"  // for gfxRect
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/BasePoint.h"  // for BasePoint
#include "mozilla/gfx/BaseRect.h"   // for BaseRect
#include "mozilla/gfx/Point.h"      // for Intsize
#include "nsDebug.h"                // for NS_ASSERTION
#include "nsISupportsImpl.h"        // for gfxContext::Release, etc
#include "nsPoint.h"                // for nsIntPoint
#include "nsRegion.h"               // for nsIntRegion

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

void ReadbackProcessor::BuildUpdates(ContainerLayer* aContainer) {
  NS_ASSERTION(mAllUpdates.IsEmpty(), "Some updates not processed?");

  if (!aContainer->mMayHaveReadbackChild) return;

  aContainer->mMayHaveReadbackChild = false;
}

ReadbackProcessor::~ReadbackProcessor() {
}

}  // namespace layers
}  // namespace mozilla
