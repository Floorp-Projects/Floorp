/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_READBACKPROCESSOR_H
#define GFX_READBACKPROCESSOR_H

#include <stdint.h>       // for uint64_t
#include "nsRect.h"       // for mozilla::gfx::IntRect
#include "nsRegionFwd.h"  // for nsIntRegion
#include "nsTArray.h"     // for nsTArray

namespace mozilla {
namespace layers {

class ContainerLayer;
class PaintedLayer;

class ReadbackProcessor {
 public:
  /**
   * Called by the container before processing any child layers. Call this
   * if any child layer might have changed in any way (other than content-only
   * changes to layers other than ColorLayers and PaintedLayers).
   *
   * This method recomputes the relationship between ReadbackLayers and
   * sibling layers, and dispatches changes to ReadbackLayers. Except that
   * if a PaintedLayer needs its contents sent to some ReadbackLayer, we'll
   * just record that internally and later the PaintedLayer should call
   * GetPaintedLayerUpdates when it paints, to find out which rectangle needs
   * to be sent, and the ReadbackLayer it needs to be sent to.
   */
  void BuildUpdates(ContainerLayer* aContainer);

  struct Update {
    /**
     * The rectangle of content that it should send, in the PaintedLayer's
     * coordinate system. This rectangle is guaranteed to be in the
     * PaintedLayer's visible region. Translate it to mLayer's coordinate system
     * by adding mLayer->GetBackgroundLayerOffset().
     */
    gfx::IntRect mUpdateRect;
    /**
     * The sequence counter value to use when calling DoUpdate
     */
    uint64_t mSequenceCounter;
  };

  ~ReadbackProcessor();

 protected:

  nsTArray<Update> mAllUpdates;
};

}  // namespace layers
}  // namespace mozilla

#endif /* GFX_READBACKPROCESSOR_H */
