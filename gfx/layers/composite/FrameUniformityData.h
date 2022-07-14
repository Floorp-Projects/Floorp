/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_FrameUniformityData_h_
#define mozilla_layers_FrameUniformityData_h_

#include "ipc/IPCMessageUtils.h"
#include "js/TypeDecls.h"
#include "ipc/IPCMessageUtilsSpecializations.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsTArray.h"

namespace mozilla {
namespace layers {

class FrameUniformityData {
  friend struct IPC::ParamTraits<FrameUniformityData>;

 public:
  bool ToJS(JS::MutableHandle<JS::Value> aOutValue, JSContext* aContext);
  // Contains the calculated frame uniformities
  std::map<uintptr_t, float> mUniformities;
};

}  // namespace layers
}  // namespace mozilla

namespace IPC {
template <>
struct ParamTraits<mozilla::layers::FrameUniformityData> {
  typedef mozilla::layers::FrameUniformityData paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mUniformities);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ParamTraitsStd<std::map<uintptr_t, float>>::Read(
        aReader, &aResult->mUniformities);
  }
};

}  // namespace IPC

#endif  // mozilla_layers_FrameUniformityData_h_
