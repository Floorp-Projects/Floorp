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
 * Portions created by the Initial Developer are Copyright (C) 2010
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

#ifndef FRAMELAYERBUILDER_H_
#define FRAMELAYERBUILDER_H_

#include "Layers.h"

class nsDisplayListBuilder;
class nsDisplayList;
class nsDisplayItem;
class nsIFrame;
class nsRect;
class nsIntRegion;
class gfxContext;

namespace mozilla {

class FrameLayerBuilder {
public:
  typedef mozilla::layers::Layer Layer;
  typedef mozilla::layers::ThebesLayer ThebesLayer;
  typedef mozilla::layers::LayerManager LayerManager;

  /**
   * Get a container layer for a display item that contains a child
   * list, either reusing an existing one or creating a new one.
   * aContainer may be null, in which case we construct a root layer.
   */
  already_AddRefed<Layer> GetContainerLayerFor(nsDisplayListBuilder* aBuilder,
                                               LayerManager* aManager,
                                               nsDisplayItem* aContainer,
                                               const nsDisplayList& aChildren);

  /**
   * Get a retained layer for a leaf display item. Returns null if no
   * layer is available, in which case the caller will probably need to
   * create one.
   */
  Layer* GetLeafLayerFor(nsDisplayListBuilder* aBuilder,
                         LayerManager* aManager,
                         nsDisplayItem* aItem);

  /**
   * Call this during invalidation if aFrame has
   * the NS_FRAME_HAS_CONTAINER_LAYER state bit. Only the nearest
   * ancestor frame of the damaged frame that has
   * NS_FRAME_HAS_CONTAINER_LAYER needs to be invalidated this way.
   */
  static void InvalidateThebesLayerContents(nsIFrame* aFrame,
                                            const nsRect& aRect);

  /**
   * This callback must be provided to EndTransaction. The callback data
   * must be the nsDisplayListBuilder containing this FrameLayerBuilder.
   */
  static void DrawThebesLayer(ThebesLayer* aLayer,
                              gfxContext* aContext,
                              const nsIntRegion& aRegionToDraw,
                              const nsIntRegion& aRegionToInvalidate,
                              void* aCallbackData);
};

}

#endif /* FRAMELAYERBUILDER_H_ */
