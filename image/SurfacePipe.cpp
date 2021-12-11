/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SurfacePipe.h"

#include <algorithm>  // for min

#include "Decoder.h"

namespace mozilla {
namespace image {

using namespace gfx;

using std::min;

Maybe<SurfaceInvalidRect> AbstractSurfaceSink::TakeInvalidRect() {
  if (mInvalidRect.IsEmpty()) {
    return Nothing();
  }

  SurfaceInvalidRect invalidRect;
  invalidRect.mInputSpaceRect = invalidRect.mOutputSpaceRect = mInvalidRect;

  // Forget about the invalid rect we're returning.
  mInvalidRect = OrientedIntRect();

  return Some(invalidRect);
}

uint8_t* AbstractSurfaceSink::DoResetToFirstRow() {
  mRow = 0;
  return GetRowPointer();
}

uint8_t* SurfaceSink::DoAdvanceRowFromBuffer(const uint8_t* aInputRow) {
  CopyInputRow(aInputRow);
  return DoAdvanceRow();
}

uint8_t* SurfaceSink::DoAdvanceRow() {
  if (mRow >= uint32_t(InputSize().height)) {
    return nullptr;
  }

  // If we're vertically flipping the output, we need to flip the invalid rect.
  // Since we're dealing with an axis-aligned rect, only the y coordinate needs
  // to change.
  int32_t invalidY = mFlipVertically ? InputSize().height - (mRow + 1) : mRow;
  mInvalidRect.UnionRect(mInvalidRect,
                         OrientedIntRect(0, invalidY, InputSize().width, 1));

  mRow = min(uint32_t(InputSize().height), mRow + 1);

  return mRow < uint32_t(InputSize().height) ? GetRowPointer() : nullptr;
}

nsresult SurfaceSink::Configure(const SurfaceConfig& aConfig) {
  IntSize surfaceSize = aConfig.mOutputSize;

  // Allocate the frame.
  // XXX(seth): Once every Decoder subclass uses SurfacePipe, we probably want
  // to allocate the frame directly here and get rid of Decoder::AllocateFrame
  // altogether.
  nsresult rv = aConfig.mDecoder->AllocateFrame(surfaceSize, aConfig.mFormat,
                                                aConfig.mAnimParams);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mImageData = aConfig.mDecoder->mImageData;
  mImageDataLength = aConfig.mDecoder->mImageDataLength;
  mFlipVertically = aConfig.mFlipVertically;

  MOZ_ASSERT(mImageData);
  MOZ_ASSERT(uint64_t(mImageDataLength) == uint64_t(surfaceSize.width) *
                                               uint64_t(surfaceSize.height) *
                                               sizeof(uint32_t));

  ConfigureFilter(surfaceSize, sizeof(uint32_t));
  return NS_OK;
}

uint8_t* SurfaceSink::GetRowPointer() const {
  // If we're flipping vertically, reverse the order in which we traverse the
  // rows.
  uint32_t row = mFlipVertically ? InputSize().height - (mRow + 1) : mRow;

  uint8_t* rowPtr = mImageData + row * InputSize().width * sizeof(uint32_t);

  MOZ_ASSERT(rowPtr >= mImageData);
  MOZ_ASSERT(rowPtr < mImageData + mImageDataLength);
  MOZ_ASSERT(rowPtr + InputSize().width * sizeof(uint32_t) <=
             mImageData + mImageDataLength);

  return rowPtr;
}

uint8_t* ReorientSurfaceSink::DoAdvanceRowFromBuffer(const uint8_t* aInputRow) {
  if (mRow >= uint32_t(InputSize().height)) {
    return nullptr;
  }

  IntRect dirty = mReorientFn(aInputRow, mRow, mImageData, mSurfaceSize,
                              mSurfaceSize.width * sizeof(uint32_t));
  auto orientedDirty = OrientedIntRect::FromUnknownRect(dirty);
  mInvalidRect.UnionRect(mInvalidRect, orientedDirty);

  mRow = min(uint32_t(InputSize().height), mRow + 1);

  return mRow < uint32_t(InputSize().height) ? GetRowPointer() : nullptr;
}

uint8_t* ReorientSurfaceSink::DoAdvanceRow() {
  return DoAdvanceRowFromBuffer(mBuffer.get());
}

nsresult ReorientSurfaceSink::Configure(const ReorientSurfaceConfig& aConfig) {
  mSurfaceSize = aConfig.mOutputSize.ToUnknownSize();

  // Allocate the frame.
  // XXX(seth): Once every Decoder subclass uses SurfacePipe, we probably want
  // to allocate the frame directly here and get rid of Decoder::AllocateFrame
  // altogether.
  nsresult rv =
      aConfig.mDecoder->AllocateFrame(mSurfaceSize, aConfig.mFormat, Nothing());
  if (NS_FAILED(rv)) {
    return rv;
  }

  // The filters above us need the unoriented size as the input.
  auto inputSize =
      aConfig.mOrientation.ToUnoriented(aConfig.mOutputSize).ToUnknownSize();
  mBuffer.reset(new (fallible) uint8_t[inputSize.width * sizeof(uint32_t)]);
  if (MOZ_UNLIKELY(!mBuffer)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  memset(mBuffer.get(), 0xFF, inputSize.width * sizeof(uint32_t));

  mReorientFn = ReorientRow(aConfig.mOrientation);
  MOZ_ASSERT(mReorientFn);

  mImageData = aConfig.mDecoder->mImageData;
  mImageDataLength = aConfig.mDecoder->mImageDataLength;

  MOZ_ASSERT(mImageData);
  MOZ_ASSERT(uint64_t(mImageDataLength) == uint64_t(mSurfaceSize.width) *
                                               uint64_t(mSurfaceSize.height) *
                                               sizeof(uint32_t));

  ConfigureFilter(inputSize, sizeof(uint32_t));
  return NS_OK;
}

uint8_t* ReorientSurfaceSink::GetRowPointer() const { return mBuffer.get(); }

}  // namespace image
}  // namespace mozilla
