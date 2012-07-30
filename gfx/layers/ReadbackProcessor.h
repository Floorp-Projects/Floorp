/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_READBACKPROCESSOR_H
#define GFX_READBACKPROCESSOR_H

#include "ReadbackLayer.h"
#include "ThebesLayerBuffer.h"
#include "nsTArray.h"

namespace mozilla {
namespace layers {

class ReadbackProcessor {
public:
  /**
   * Called by the container before processing any child layers. Call this
   * if any child layer might have changed in any way (other than content-only
   * changes to layers other than ColorLayers and ThebesLayers).
   *
   * This method recomputes the relationship between ReadbackLayers and
   * sibling layers, and dispatches changes to ReadbackLayers. Except that
   * if a ThebesLayer needs its contents sent to some ReadbackLayer, we'll
   * just record that internally and later the ThebesLayer should call
   * GetThebesLayerUpdates when it paints, to find out which rectangle needs
   * to be sent, and the ReadbackLayer it needs to be sent to.
   */
  void BuildUpdates(ContainerLayer* aContainer);

  struct Update {
    /**
     * The layer a ThebesLayer should send its contents to.
     */
    ReadbackLayer* mLayer;
    /**
     * The rectangle of content that it should send, in the ThebesLayer's
     * coordinate system. This rectangle is guaranteed to be in the ThebesLayer's
     * visible region. Translate it to mLayer's coordinate system
     * by adding mLayer->GetBackgroundLayerOffset().
     */
    nsIntRect      mUpdateRect;
    /**
     * The sequence counter value to use when calling DoUpdate
     */
    PRUint64       mSequenceCounter;
  };
  /**
   * Appends any ReadbackLayers that need to be updated, and the rects that
   * need to be updated, to aUpdates. Only need to call this for ThebesLayers
   * that have been marked UsedForReadback().
   * Each Update's mLayer's mBackgroundLayer will have been set to aLayer.
   * If a ThebesLayer doesn't call GetThebesLayerUpdates, then all the
   * ReadbackLayers that needed data from that ThebesLayer will be marked
   * as having unknown backgrounds.
   * @param aUpdateRegion if non-null, this region is set to the union
   * of the mUpdateRects.
   */
  void GetThebesLayerUpdates(ThebesLayer* aLayer,
                             nsTArray<Update>* aUpdates,
                             nsIntRegion* aUpdateRegion = nullptr);

  ~ReadbackProcessor();

protected:
  void BuildUpdatesForLayer(ReadbackLayer* aLayer);

  nsTArray<Update> mAllUpdates;
};

}
}
#endif /* GFX_READBACKPROCESSOR_H */
