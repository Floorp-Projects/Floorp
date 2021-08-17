/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_CompilationMessage_H_
#define GPU_CompilationMessage_H_

#include "nsWrapperCache.h"
#include "ObjectModel.h"
#include "mozilla/dom/WebGPUBinding.h"

namespace mozilla {
namespace dom {
class DOMString;
}  // namespace dom
namespace webgpu {
class CompilationInfo;

class CompilationMessage final : public nsWrapperCache,
                                 public ChildOf<CompilationInfo> {
  dom::GPUCompilationMessageType mType = dom::GPUCompilationMessageType::Error;
  uint64_t mLineNum = 0;
  uint64_t mLinePos = 0;
  uint64_t mOffset = 0;
  uint64_t mLength = 0;

 public:
  GPU_DECL_CYCLE_COLLECTION(CompilationMessage)
  GPU_DECL_JS_WRAP(CompilationMessage)

  void GetMessage(dom::DOMString& aMessage) {}
  dom::GPUCompilationMessageType Type() const { return mType; }
  uint64_t LineNum() const { return mLineNum; }
  uint64_t LinePos() const { return mLinePos; }
  uint64_t Offset() const { return mOffset; }
  uint64_t Length() const { return mLength; }

 private:
  explicit CompilationMessage(CompilationInfo* const aParent);
  ~CompilationMessage() = default;
  void Cleanup() {}
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_CompilationMessage_H_
