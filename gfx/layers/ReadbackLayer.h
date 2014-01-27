/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_READBACKLAYER_H
#define GFX_READBACKLAYER_H

#include <stdint.h>                     // for uint64_t
#include "Layers.h"                     // for Layer, etc
#include "gfxColor.h"                   // for gfxRGBA
#include "gfxRect.h"                    // for gfxRect
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsAutoPtr.h"                  // for nsAutoPtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRect.h"                     // for nsIntRect
#include "nsSize.h"                     // for nsIntSize
#include "nscore.h"                     // for nsACString

class gfxContext;

namespace mozilla {
namespace layers {

class ReadbackProcessor;

/**
 * A ReadbackSink receives a stream of updates to a rectangle of pixels.
 * These update callbacks are always called on the main thread, either during
 * EndTransaction or from the event loop.
 */
class ReadbackSink {
public:
  ReadbackSink() {}
  virtual ~ReadbackSink() {}

  /**
   * Sends an update to indicate that the background is currently unknown.
   */
  virtual void SetUnknown(uint64_t aSequenceNumber) = 0;
  /**
   * Called by the layer system to indicate that the contents of part of
   * the readback area are changing.
   * @param aRect is the rectangle of content that is being updated,
   * in the coordinate system of the ReadbackLayer.
   * @param aSequenceNumber updates issued out of order should be ignored.
   * Only use updates whose sequence counter is greater than all other updates
   * seen so far. Return null when a non-fresh sequence value is given.
   * @return a context into which the update should be drawn. This should be
   * set up to clip to aRect. Zero should never be passed as a sequence number.
   * If this returns null, EndUpdate should NOT be called. If it returns
   * non-null, EndUpdate must be called.
   *
   * We don't support partially unknown backgrounds. Therefore, the
   * first BeginUpdate after a SetUnknown will have the complete background.
   */
  virtual already_AddRefed<gfxContext>
      BeginUpdate(const nsIntRect& aRect, uint64_t aSequenceNumber) = 0;
  /**
   * EndUpdate must be called immediately after BeginUpdate, without returning
   * to the event loop.
   * @param aContext the context returned by BeginUpdate
   * Implicitly Restore()s the state of aContext.
   */
  virtual void EndUpdate(gfxContext* aContext, const nsIntRect& aRect) = 0;
};

/**
 * A ReadbackLayer never renders anything. It enables clients to extract
 * the rendered contents of the layer tree below the ReadbackLayer.
 * The rendered contents are delivered asynchronously via calls to a
 * ReadbackSink object supplied by the client.
 *
 * This is a "best effort" API; it is possible for the layer system to tell
 * the ReadbackSink that the contents of the readback area are unknown.
 *
 * This API exists to work around the limitations of transparent windowless
 * plugin rendering APIs. It should not be used for anything else.
 */
class ReadbackLayer : public Layer {
public:
  MOZ_LAYER_DECL_NAME("ReadbackLayer", TYPE_READBACK)

  virtual void ComputeEffectiveTransforms(const gfx::Matrix4x4& aTransformToSurface)
  {
    // Snap our local transform first, and snap the inherited transform as well.
    // This makes our snapping equivalent to what would happen if our content
    // was drawn into a ThebesLayer (gfxContext would snap using the local
    // transform, then we'd snap again when compositing the ThebesLayer).
    mEffectiveTransform =
        SnapTransform(GetLocalTransform(), gfxRect(0, 0, mSize.width, mSize.height),
                      nullptr)*
        SnapTransformTranslation(aTransformToSurface, nullptr);
  }

  /**
   * CONSTRUCTION PHASE ONLY
   * Set the callback object to which readback updates will be delivered.
   * This also resets the "needed rectangle" so that on the next layer tree
   * transaction we will try to deliver the full contents of the readback
   * area to the sink.
   * This layer takes ownership of the sink. It will be deleted when the
   * layer is destroyed or when a new sink is set.
   * Initially the contents of the readback area are completely unknown.
   */
  void SetSink(ReadbackSink* aSink)
  {
    SetUnknown();
    mSink = aSink;
  }
  ReadbackSink* GetSink() { return mSink; }

  /**
   * CONSTRUCTION PHASE ONLY
   * Set the size of content that should be read back. The readback area
   * has its top-left at 0,0 and has size aSize.
   * Can only be called while the sink is null!
   */
  void SetSize(const nsIntSize& aSize)
  {
    NS_ASSERTION(!mSink, "Should have no sink while changing size!");
    mSize = aSize;
  }
  const nsIntSize& GetSize() { return mSize; }
  nsIntRect GetRect() { return nsIntRect(nsIntPoint(0, 0), mSize); }

  bool IsBackgroundKnown()
  {
    return mBackgroundLayer || mBackgroundColor.a == 1.0;
  }

  void NotifyRemoved() {
    SetUnknown();
    mSink = nullptr;
  }

  void NotifyThebesLayerRemoved(ThebesLayer* aLayer)
  {
    if (mBackgroundLayer == aLayer) {
      mBackgroundLayer = nullptr;
    }
  }

  const nsIntPoint& GetBackgroundLayerOffset() { return mBackgroundLayerOffset; }

  uint64_t AllocateSequenceNumber() { return ++mSequenceCounter; }

  void SetUnknown()
  {
    if (IsBackgroundKnown()) {
      if (mSink) {
        mSink->SetUnknown(AllocateSequenceNumber());
      }
      mBackgroundLayer = nullptr;
      mBackgroundColor = gfxRGBA(0,0,0,0);
    }
  }

protected:
  friend class ReadbackProcessor;

  ReadbackLayer(LayerManager* aManager, void* aImplData) :
    Layer(aManager, aImplData),
    mSequenceCounter(0),
    mSize(0,0),
    mBackgroundLayer(nullptr),
    mBackgroundLayerOffset(0, 0),
    mBackgroundColor(gfxRGBA(0,0,0,0))
  {}

  // Print interesting information about this into aTo.  Internally
  // used to implement Dump*() and Log*().
  virtual nsACString& PrintInfo(nsACString& aTo, const char* aPrefix);

  uint64_t mSequenceCounter;
  nsAutoPtr<ReadbackSink> mSink;
  nsIntSize mSize;

  // This can refer to any (earlier) sibling ThebesLayer. That ThebesLayer
  // must have mUsedForReadback set on it. If the ThebesLayer is removed
  // for the container, this will be set to null by NotifyThebesLayerRemoved.
  // This ThebesLayer contains the contents which have previously been reported
  // to mSink. The ThebesLayer had only an integer translation transform,
  // and it covered the entire readback area. This layer also had only an
  // integer translation transform.
  ThebesLayer* mBackgroundLayer;
  // When mBackgroundLayer is non-null, this is the offset to add to
  // convert from the coordinates of mBackgroundLayer to the coordinates
  // of this layer.
  nsIntPoint   mBackgroundLayerOffset;
  // When mBackgroundColor is opaque, this is the color of the ColorLayer
  // that contained the contents we reported to mSink, which covered the
  // entire readback area.
  gfxRGBA      mBackgroundColor;
};

}
}
#endif /* GFX_READBACKLAYER_H */
