/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_MatrixMessage_h
#define mozilla_layers_MatrixMessage_h

#include "mozilla/Maybe.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/layers/LayersTypes.h"
#include "Units.h"  // for ScreenRect

namespace mozilla {
namespace layers {
class MatrixMessage {
 public:
  // Don't use this one directly
  MatrixMessage() {}

  MatrixMessage(const Maybe<LayerToScreenMatrix4x4>& aMatrix,
                const ScreenRect& aRemoteDocumentRect,
                const LayersId& aLayersId)
      : mMatrix(ToUnknownMatrix(aMatrix)),
        mRemoteDocumentRect(aRemoteDocumentRect),
        mLayersId(aLayersId) {}

  inline Maybe<LayerToScreenMatrix4x4> GetMatrix() const {
    return LayerToScreenMatrix4x4::FromUnknownMatrix(mMatrix);
  }

  inline ScreenRect GetRemoteDocumentRect() const {
    return mRemoteDocumentRect;
  }

  inline const LayersId& GetLayersId() const { return mLayersId; }

  // Fields are public for IPC. Don't access directly
  // elsewhere.
  // Transform matrix to convert this layer to screen coordinate.
  Maybe<gfx::Matrix4x4> mMatrix;  // Untyped for IPC
  // The remote iframe document rectangle corresponding to this layer.
  // The rectangle is the result of clipped out by ancestor async scrolling so
  // that the rectangle will be empty if it's completely scrolled out of view.
  ScreenRect mRemoteDocumentRect;
  LayersId mLayersId;
};
};  // namespace layers
};  // namespace mozilla

#endif  // mozilla_layers_MatrixMessage_h
