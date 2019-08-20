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

namespace mozilla {
namespace layers {
class MatrixMessage {
 public:
  // Don't use this one directly
  MatrixMessage() {}

  MatrixMessage(const Maybe<LayerToScreenMatrix4x4>& aMatrix,
                const LayersId& aLayersId)
      : mMatrix(ToUnknownMatrix(aMatrix)), mLayersId(aLayersId) {}

  inline Maybe<LayerToScreenMatrix4x4> GetMatrix() const {
    return LayerToScreenMatrix4x4::FromUnknownMatrix(mMatrix);
  }

  inline const LayersId& GetLayersId() const { return mLayersId; }

  // Fields are public for IPC. Don't access directly
  // elsewhere.
  Maybe<gfx::Matrix4x4> mMatrix;  // Untyped for IPC
  LayersId mLayersId;
};
};  // namespace layers
};  // namespace mozilla

#endif  // mozilla_layers_MatrixMessage_h
