/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright 2016 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "wasm/WasmDebugFrame.h"

#include "vm/EnvironmentObject.h"
#include "wasm/WasmBaselineCompile.h"
#include "wasm/WasmInstance.h"

#include "jsobjinlines.h"

using namespace js;
using namespace js::wasm;

Instance*
DebugFrame::instance() const
{
    return tlsData_->instance;
}

GlobalObject*
DebugFrame::global() const
{
    return &instance()->object()->global();
}

JSObject*
DebugFrame::environmentChain() const
{
    return &global()->lexicalEnvironment();
}

void
DebugFrame::observeFrame(JSContext* cx)
{
   if (observing_)
       return;

   instance()->code().adjustEnterAndLeaveFrameTrapsState(cx, /* enabled = */ true);
   observing_ = true;
}

void
DebugFrame::leaveFrame(JSContext* cx)
{
   if (!observing_)
       return;

   instance()->code().adjustEnterAndLeaveFrameTrapsState(cx, /* enabled = */ false);
   observing_ = false;
}

void
DebugFrame::clearReturnJSValue()
{
    hasCachedReturnJSValue_ = true;
    cachedReturnJSValue_.setUndefined();
}

void
DebugFrame::updateReturnJSValue()
{
    hasCachedReturnJSValue_ = true;
    ExprType returnType = instance()->code().debugGetResultType(funcIndex());
    switch (returnType) {
      case ExprType::Void:
          cachedReturnJSValue_.setUndefined();
          break;
      case ExprType::I32:
          cachedReturnJSValue_.setInt32(resultI32_);
          break;
      case ExprType::I64:
          // Just display as a Number; it's ok if we lose some precision
          cachedReturnJSValue_.setDouble((double)resultI64_);
          break;
      case ExprType::F32:
          cachedReturnJSValue_.setDouble(JS::CanonicalizeNaN(resultF32_));
          break;
      case ExprType::F64:
          cachedReturnJSValue_.setDouble(JS::CanonicalizeNaN(resultF64_));
          break;
      default:
          MOZ_CRASH("result type");
    }
}

bool
DebugFrame::getLocal(uint32_t localIndex, MutableHandleValue vp)
{
    ValTypeVector locals;
    size_t argsLength;
    if (!instance()->code().debugGetLocalTypes(funcIndex(), &locals, &argsLength))
        return false;

    BaseLocalIter iter(locals, argsLength, /* debugEnabled = */ true);
    while (!iter.done() && iter.index() < localIndex)
        iter++;
    MOZ_ALWAYS_TRUE(!iter.done());

    uint8_t* frame = static_cast<uint8_t*>((void*)this) + offsetOfFrame();
    void* dataPtr = frame - iter.frameOffset();
    switch (iter.mirType()) {
      case jit::MIRType::Int32:
          vp.set(Int32Value(*static_cast<int32_t*>(dataPtr)));
          break;
      case jit::MIRType::Int64:
          // Just display as a Number; it's ok if we lose some precision
          vp.set(NumberValue((double)*static_cast<int64_t*>(dataPtr)));
          break;
      case jit::MIRType::Float32:
          vp.set(NumberValue(JS::CanonicalizeNaN(*static_cast<float*>(dataPtr))));
          break;
      case jit::MIRType::Double:
          vp.set(NumberValue(JS::CanonicalizeNaN(*static_cast<double*>(dataPtr))));
          break;
      default:
          MOZ_CRASH("local type");
    }
    return true;
}

