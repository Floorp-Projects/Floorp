/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_PAINTING_MATRIXSTACK_H
#define MOZILLA_PAINTING_MATRIXSTACK_H

#include "nsISupports.h"
#include "nsTArray.h"
#include "mozilla/gfx/MatrixFwd.h"
#include "mozilla/Maybe.h"
#include "DisplayItemClip.h"

namespace mozilla {

/**
 * MatrixStack stores a stack of matrices and keeps track of the accumulated
 * transform matrix.
 */
template<typename T>
class MatrixStack {
public:
  MatrixStack() = default;

  ~MatrixStack()
  {
    MOZ_ASSERT(mMatrices.IsEmpty());
  }

  /**
   * Returns the current accumulated transform matrix.
   */
  const T& CurrentMatrix() const
  {
    return mCurrentMatrix;
  }

  /**
   * Returns true if any matrices are currently pushed to the stack.
   */
  bool HasTransform() const
  {
    return mMatrices.Length() > 0;
  }

  /**
   * Pushes the given |aMatrix| to the stack.
   */
  void Push(const T& aMatrix)
  {
    mMatrices.AppendElement(mCurrentMatrix);
    mCurrentMatrix = aMatrix * mCurrentMatrix;
  }

  /**
   * Pops the most recently added matrix from the stack.
   */
  void Pop()
  {
    MOZ_ASSERT(mMatrices.Length() > 0);
    mCurrentMatrix = mMatrices.PopLastElement();
  }

private:
  T mCurrentMatrix;
  AutoTArray<T, 2> mMatrices;
};

typedef MatrixStack<gfx::Matrix4x4Flagged> MatrixStack4x4;


/**
 * TransformClipNode stores a transformation matrix and a post-transform
 * clip rect.
 * They can be used to transform and clip a display item inside a flattened
 * nsDisplayTransform to the coordinate space of that nsDisplayTransform.
 */
class TransformClipNode {
  NS_INLINE_DECL_REFCOUNTING(TransformClipNode);
public:
  TransformClipNode(const RefPtr<TransformClipNode>& aParent,
                    const gfx::Matrix4x4Flagged& aTransform,
                    const Maybe<nsRect>& aClip)
  : mParent(aParent)
  , mTransform(aTransform)
  , mClip(aClip)
  {
    MOZ_COUNT_CTOR(TransformClipNode);
  }

  /*
   * Returns the parent node, or nullptr if this is the root node.
   */
  const RefPtr<TransformClipNode>& Parent() const
  {
    return mParent;
  }

  /*
   * Returns the post-transform clip, if there is one.
   */
  const Maybe<nsRect>& Clip() const
  {
    return mClip;
  }

  /*
   * Returns the matrix that transforms the item bounds to the coordinate space
   * of the flattened nsDisplayTransform.
   */
  const gfx::Matrix4x4Flagged& Transform() const
  {
    return mTransform;
  }

private:
  ~TransformClipNode()
  {
    MOZ_COUNT_DTOR(TransformClipNode);
  }

  const RefPtr<TransformClipNode> mParent;
  const gfx::Matrix4x4Flagged mTransform;
  const Maybe<nsRect> mClip;
};

} // namespace mozilla

#endif /* MOZILLA_PAINTING_MATRIXSTACK_H */