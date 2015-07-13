/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_FrameUniformityData_h_
#define mozilla_layers_FrameUniformityData_h_

#include "ipc/IPCMessageUtils.h"
#include "js/TypeDecls.h"
#include "nsRefPtr.h"

namespace mozilla {
namespace layers {
class Layer;

class FrameUniformityData {
  friend struct IPC::ParamTraits<FrameUniformityData>;

public:
  bool ToJS(JS::MutableHandleValue aOutValue, JSContext* aContext);
  // Contains the calculated frame uniformities
  std::map<uintptr_t,float> mUniformities;
};

struct LayerTransforms {
  LayerTransforms() {}

  gfx::Point GetAverage();
  gfx::Point GetStdDev();

  // 60 fps * 5 seconds worth of data
  nsAutoTArray<gfx::Point, 300> mTransforms;
};

class LayerTransformRecorder {
public:
  LayerTransformRecorder() {}
  ~LayerTransformRecorder();

  void RecordTransform(Layer* aLayer, const gfx::Point& aTransform);
  void Reset();
  void EndTest(FrameUniformityData* aOutData);

private:
  float CalculateFrameUniformity(uintptr_t aLayer);
  LayerTransforms* GetLayerTransforms(uintptr_t aLayer);
  std::map<uintptr_t,LayerTransforms*> mFrameTransforms;
};

} // namespace layers
} // namespace mozilla

namespace IPC {
template<>
struct ParamTraits<mozilla::layers::FrameUniformityData>
{
  typedef mozilla::layers::FrameUniformityData paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mUniformities);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ParamTraitsStd<std::map<uintptr_t,float>>::Read(aMsg, aIter, &aResult->mUniformities);
  }
};

} // namespace IPC

#endif // mozilla_layers_FrameUniformityData_h_
