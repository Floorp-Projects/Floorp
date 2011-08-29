/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert O'Callahan <robert@ocallahan.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
                             nsIntRegion* aUpdateRegion = nsnull);

  ~ReadbackProcessor();

protected:
  void BuildUpdatesForLayer(ReadbackLayer* aLayer);

  nsTArray<Update> mAllUpdates;
};

}
}
#endif /* GFX_READBACKPROCESSOR_H */
