/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FrameUniformityData.h"

#include <map>

#include "Units.h"
#include "gfxPoint.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/dom/APZTestDataBinding.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsTArray.h"

namespace mozilla {
namespace layers {

using namespace gfx;

Point LayerTransforms::GetAverage() {
  MOZ_ASSERT(!mTransforms.IsEmpty());

  Point current = mTransforms[0];
  Point average;
  size_t length = mTransforms.Length();

  for (size_t i = 1; i < length; i++) {
    Point nextTransform = mTransforms[i];
    Point movement = nextTransform - current;
    average += Point(std::fabs(movement.x), std::fabs(movement.y));
    current = nextTransform;
  }

  average = average / (float)length;
  return average;
}

Point LayerTransforms::GetStdDev() {
  Point average = GetAverage();
  Point stdDev;
  Point current = mTransforms[0];

  for (size_t i = 1; i < mTransforms.Length(); i++) {
    Point next = mTransforms[i];
    Point move = next - current;
    move.x = fabs(move.x);
    move.y = fabs(move.y);

    Point diff = move - average;
    diff.x = diff.x * diff.x;
    diff.y = diff.y * diff.y;
    stdDev += diff;

    current = next;
  }

  stdDev = stdDev / mTransforms.Length();
  stdDev.x = sqrt(stdDev.x);
  stdDev.y = sqrt(stdDev.y);
  return stdDev;
}

bool LayerTransforms::Sanitize() {
  // Remove leading and trailing zeros to isolate the composites that actually
  // changed the transform
  for (size_t i = 1; i < mTransforms.Length(); i++) {
    if (mTransforms[i] != mTransforms[i - 1]) {
      mTransforms.RemoveElementsAt(0, i - 1);
      break;
    }
  }
  for (size_t i = mTransforms.Length() - 1; i > 0; i--) {
    if (mTransforms[i - 1] != mTransforms[i]) {
      mTransforms.SetLength(i + 1);
      break;
    }
  }
  return !mTransforms.IsEmpty();
}

LayerTransformRecorder::~LayerTransformRecorder() { Reset(); }

void LayerTransformRecorder::RecordTransform(Layer* aLayer,
                                             const Point& aTransform) {
  LayerTransforms* layerTransforms = GetLayerTransforms((uintptr_t)aLayer);
  layerTransforms->mTransforms.AppendElement(aTransform);
}

void LayerTransformRecorder::EndTest(FrameUniformityData* aOutData) {
  for (const auto& [layer, transforms] : mFrameTransforms) {
    (void)transforms;  // suppress unused variable warning

    float uniformity = CalculateFrameUniformity(layer);

    std::pair<uintptr_t, float> result(layer, uniformity);
    aOutData->mUniformities.insert(result);
  }

  Reset();
}

LayerTransforms* LayerTransformRecorder::GetLayerTransforms(uintptr_t aLayer) {
  auto [iter, inserted] =
      mFrameTransforms.insert(FrameTransformMap::value_type{aLayer, nullptr});
  if (inserted) {
    iter->second = MakeUnique<LayerTransforms>();
  }
  return iter->second.get();
}

void LayerTransformRecorder::Reset() { mFrameTransforms.clear(); }

float LayerTransformRecorder::CalculateFrameUniformity(uintptr_t aLayer) {
  LayerTransforms* layerTransform = GetLayerTransforms(aLayer);
  float yUniformity = -1;
  if (layerTransform->Sanitize()) {
    Point stdDev = layerTransform->GetStdDev();
    yUniformity = stdDev.y;
  }
  return yUniformity;
}

bool FrameUniformityData::ToJS(JS::MutableHandleValue aOutValue,
                               JSContext* aContext) {
  dom::FrameUniformityResults results;
  dom::Sequence<dom::FrameUniformity>& layers =
      results.mLayerUniformities.Construct();

  for (const auto& [layerAddr, uniformity] : mUniformities) {
    // FIXME: Make this infallible after bug 968520 is done.
    MOZ_ALWAYS_TRUE(layers.AppendElement(fallible));
    dom::FrameUniformity& entry = layers.LastElement();

    entry.mLayerAddress.Construct() = layerAddr;
    entry.mFrameUniformity.Construct() = uniformity;
  }

  return dom::ToJSValue(aContext, results, aOutValue);
}

}  // namespace layers
}  // namespace mozilla
