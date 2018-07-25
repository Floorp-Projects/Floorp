/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_PAINTING_MATRIXSTACK_H
#define MOZILLA_PAINTING_MATRIXSTACK_H

#include "nsTArray.h"
#include "mozilla/gfx/MatrixFwd.h"

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

} // namespace mozilla

#endif /* MOZILLA_PAINTING_MATRIXSTACK_H */
