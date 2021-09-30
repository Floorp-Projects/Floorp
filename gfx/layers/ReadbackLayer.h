/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_READBACKLAYER_H
#define GFX_READBACKLAYER_H

#include <stdint.h>             // for uint64_t
#include "Layers.h"             // for Layer, etc
#include "mozilla/gfx/Rect.h"   // for gfxRect
#include "mozilla/gfx/Point.h"  // for IntPoint
#include "mozilla/mozalloc.h"   // for operator delete
#include "nsCOMPtr.h"           // for already_AddRefed
#include "nsDebug.h"            // for NS_ASSERTION
#include "nsPoint.h"            // for nsIntPoint
#include "nscore.h"             // for nsACString

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
  ReadbackSink() = default;
  virtual ~ReadbackSink() = default;

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
  virtual already_AddRefed<gfx::DrawTarget> BeginUpdate(
      const gfx::IntRect& aRect, uint64_t aSequenceNumber) = 0;
  /**
   * EndUpdate must be called immediately after BeginUpdate, without returning
   * to the event loop.
   * @param aContext the context returned by BeginUpdate
   * Implicitly Restore()s the state of aContext.
   */
  virtual void EndUpdate(const gfx::IntRect& aRect) = 0;
};

}  // namespace layers
}  // namespace mozilla

#endif /* GFX_READBACKLAYER_H */
