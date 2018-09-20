/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_PAINTING_TRANSFORMCLIPNODE_H
#define MOZILLA_PAINTING_TRANSFORMCLIPNODE_H

#include "mozilla/gfx/MatrixFwd.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/Maybe.h"
#include "nsISupports.h"
#include "nsRegionFwd.h"

namespace mozilla {

/**
 * TransformClipNode stores a transformation matrix and a post-transform
 * clip rect.
 * They can be used to transform and clip a display item inside a flattened
 * nsDisplayTransform to the coordinate space of that nsDisplayTransform.
 */
class TransformClipNode
{
  NS_INLINE_DECL_REFCOUNTING(TransformClipNode);

public:
  TransformClipNode(const RefPtr<TransformClipNode>& aParent,
                    const gfx::Matrix4x4Flagged& aTransform,
                    const Maybe<gfx::IntRect>& aClip)
    : mParent(aParent)
    , mTransform(aTransform)
    , mClip(aClip)
  {
    MOZ_COUNT_CTOR(TransformClipNode);
  }

  /**
   * Returns the parent node, or nullptr if this is the root node.
   */
  const RefPtr<TransformClipNode>& Parent() const { return mParent; }

  /**
   * Transforms and clips |aRect| up to the root transform node.
   * |aRect| is expected to be in app units.
   */
  nsRect TransformRect(const nsRect& aRect, const int32_t aA2D)
  {
    if (aRect.IsEmpty()) {
      return aRect;
    }

    gfx::Rect result(NSAppUnitsToFloatPixels(aRect.x, aA2D),
                     NSAppUnitsToFloatPixels(aRect.y, aA2D),
                     NSAppUnitsToFloatPixels(aRect.width, aA2D),
                     NSAppUnitsToFloatPixels(aRect.height, aA2D));
    TransformRect(result);
    return nsRect(NSFloatPixelsToAppUnits(result.x, aA2D),
                  NSFloatPixelsToAppUnits(result.y, aA2D),
                  NSFloatPixelsToAppUnits(result.width, aA2D),
                  NSFloatPixelsToAppUnits(result.height, aA2D));
  }

  /**
   * Transforms and clips |aRect| up to the root transform node.
   * |aRect| is expected to be in integer pixels.
   */
  gfx::IntRect TransformRect(const gfx::IntRect& aRect)
  {
    if (aRect.IsEmpty()) {
      return aRect;
    }

    gfx::Rect result(IntRectToRect(aRect));
    TransformRect(result);
    return RoundedToInt(result);
  }

  /**
   * Transforms and clips |aRegion| up to the root transform node.
   * |aRegion| is expected be in integer pixels.
   */
  nsIntRegion TransformRegion(const nsIntRegion& aRegion)
  {
    if (aRegion.IsEmpty()) {
      return aRegion;
    }

    nsIntRegion result = aRegion;

    const TransformClipNode* node = this;
    while (node) {
      const gfx::Matrix4x4Flagged& transform = node->Transform();
      result = result.Transform(transform.GetMatrix());

      if (node->Clip()) {
        const gfx::IntRect clipRect = *node->Clip();
        result.AndWith(clipRect);
      }

      node = node->Parent();
    }

    return result;
  }

protected:
  /**
   * Returns the post-transform clip, if there is one.
   */
  const Maybe<gfx::IntRect>& Clip() const { return mClip; }

  /**
   * Returns the matrix that transforms the item bounds to the coordinate space
   * of the flattened nsDisplayTransform.
   */
  const gfx::Matrix4x4Flagged& Transform() const { return mTransform; }

  void TransformRect(gfx::Rect& aRect)
  {
    const TransformClipNode* node = this;
    while (node) {
      const gfx::Matrix4x4Flagged& transform = node->Transform();
      gfx::Rect maxBounds = gfx::Rect::MaxIntRect();

      if (node->Clip()) {
        maxBounds = IntRectToRect(*node->Clip());
      }

      aRect = transform.TransformAndClipBounds(aRect, maxBounds);
      node = node->Parent();
    }
  }

private:
  ~TransformClipNode() { MOZ_COUNT_DTOR(TransformClipNode); }

  const RefPtr<TransformClipNode> mParent;
  const gfx::Matrix4x4Flagged mTransform;
  const Maybe<gfx::IntRect> mClip;
};

} // namespace mozilla

#endif /* MOZILLA_PAINTING_TRANSFORMCLIPNODE_H */
