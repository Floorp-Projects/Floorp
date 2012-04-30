/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_BASICIMPLDATA_H
#define GFX_BASICIMPLDATA_H

namespace mozilla {
namespace layers {

/**
 * This is the ImplData for all Basic layers. It also exposes methods
 * private to the Basic implementation that are common to all Basic layer types.
 * In particular, there is an internal Paint() method that we can use
 * to paint the contents of non-Thebes layers.
 *
 * The class hierarchy for Basic layers is like this:
 *                                 BasicImplData
 * Layer                            |   |   |
 *  |                               |   |   |
 *  +-> ContainerLayer              |   |   |
 *  |    |                          |   |   |
 *  |    +-> BasicContainerLayer <--+   |   |
 *  |                                   |   |
 *  +-> ThebesLayer                     |   |
 *  |    |                              |   |
 *  |    +-> BasicThebesLayer <---------+   |
 *  |                                       |
 *  +-> ImageLayer                          |
 *       |                                  |
 *       +-> BasicImageLayer <--------------+
 */
class BasicImplData {
public:
  BasicImplData() : mHidden(false),
    mClipToVisibleRegion(false),
    mDrawAtomically(false),
    mOperator(gfxContext::OPERATOR_OVER)
  {
    MOZ_COUNT_CTOR(BasicImplData);
  }
  virtual ~BasicImplData()
  {
    MOZ_COUNT_DTOR(BasicImplData);
  }

  /**
   * Layers that paint themselves, such as ImageLayers, should paint
   * in response to this method call. aContext will already have been
   * set up to account for all the properties of the layer (transform,
   * opacity, etc).
   */
  virtual void Paint(gfxContext* aContext) {}

  /**
   * Like Paint() but called for ThebesLayers with the additional parameters
   * they need.
   * If mClipToVisibleRegion is set, then the layer must clip to its
   * effective visible region (snapped or unsnapped, it doesn't matter).
   */
  virtual void PaintThebes(gfxContext* aContext,
                           LayerManager::DrawThebesLayerCallback aCallback,
                           void* aCallbackData,
                           ReadbackProcessor* aReadback) {}

  virtual ShadowableLayer* AsShadowableLayer() { return nsnull; }

  /**
   * Implementations return true here if they *must* retain their
   * layer contents.  This is true of shadowable layers with shadows,
   * because there's no target on which to composite directly in the
   * layer-publishing child process.
   */
  virtual bool MustRetainContent() { return false; }

  /**
   * Layers will get this call when their layer manager is destroyed, this
   * indicates they should clear resources they don't really need after their
   * LayerManager ceases to exist.
   */
  virtual void ClearCachedResources() {}

  /**
   * This variable is set by MarkLayersHidden() before painting. It indicates
   * that the layer should not be composited during this transaction.
   */
  void SetHidden(bool aCovered) { mHidden = aCovered; }
  bool IsHidden() const { return false; }
  /**
   * This variable is set by MarkLayersHidden() before painting. This is
   * the operator to be used when compositing the layer in this transaction. It must
   * be OVER or SOURCE.
   */
  void SetOperator(gfxContext::GraphicsOperator aOperator)
  {
    NS_ASSERTION(aOperator == gfxContext::OPERATOR_OVER ||
                 aOperator == gfxContext::OPERATOR_SOURCE,
                 "Bad composition operator");
    mOperator = aOperator;
  }
  gfxContext::GraphicsOperator GetOperator() const { return mOperator; }

  bool GetClipToVisibleRegion() { return mClipToVisibleRegion; }
  void SetClipToVisibleRegion(bool aClip) { mClipToVisibleRegion = aClip; }

  void SetDrawAtomically(bool aDrawAtomically) { mDrawAtomically = aDrawAtomically; }

protected:
  bool mHidden;
  bool mClipToVisibleRegion;
  bool mDrawAtomically;
  gfxContext::GraphicsOperator mOperator;
};

} // layers
} // mozilla

#endif
