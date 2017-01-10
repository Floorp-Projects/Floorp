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

#ifndef wasmdebugframe_js_h
#define wasmdebugframe_js_h

#include "gc/Barrier.h"
#include "js/RootingAPI.h"
#include "js/TracingAPI.h"
#include "wasm/WasmTypes.h"

namespace js {

class WasmFunctionCallObject;

namespace wasm {

class DebugFrame
{
    union
    {
        int32_t resultI32_;
        int64_t resultI64_;
        float   resultF32_;
        double  resultF64_;
    };

    // The fields below are initialized by the baseline compiler.
    uint32_t    funcIndex_;
    uint32_t    reserved0_;

    union
    {
        struct
        {
            bool    observing_ : 1;
            bool    isDebuggee_ : 1;
            bool    prevUpToDate_ : 1;
            bool    hasCachedSavedFrame_ : 1;
        };
        void*   reserved1_;
    };

    TlsData*    tlsData_;
    Frame       frame_;

    explicit DebugFrame() {}

  public:
    inline uint32_t funcIndex() const { return funcIndex_; }
    inline TlsData* tlsData() const { return tlsData_; }
    inline Frame& frame() { return frame_; }

    Instance* instance() const;
    GlobalObject* global() const;

    JSObject* environmentChain() const;

    void observeFrame(JSContext* cx);
    void leaveFrame(JSContext* cx);

    void trace(JSTracer* trc);

    // These are opaque boolean flags used by the debugger and
    // saved-frame-chains code.
    inline bool isDebuggee() const { return isDebuggee_; }
    inline void setIsDebuggee() { isDebuggee_ = true; }
    inline void unsetIsDebuggee() { isDebuggee_ = false; }

    inline bool prevUpToDate() const { return prevUpToDate_; }
    inline void setPrevUpToDate() { prevUpToDate_ = true; }
    inline void unsetPrevUpToDate() { prevUpToDate_ = false; }

    inline bool hasCachedSavedFrame() const { return hasCachedSavedFrame_; }
    inline void setHasCachedSavedFrame() { hasCachedSavedFrame_ = true; }

    inline void* resultsPtr() { return &resultI32_; }

    static constexpr size_t offsetOfResults() { return offsetof(DebugFrame, resultI32_); }
    static constexpr size_t offsetOfFlagsWord() { return offsetof(DebugFrame, reserved1_); }
    static constexpr size_t offsetOfFuncIndex() { return offsetof(DebugFrame, funcIndex_); }
    static constexpr size_t offsetOfTlsData() { return offsetof(DebugFrame, tlsData_); }
    static constexpr size_t offsetOfFrame() { return offsetof(DebugFrame, frame_); }
};

static_assert(DebugFrame::offsetOfResults() == 0, "results shall be at offset 0");
static_assert(DebugFrame::offsetOfTlsData() + sizeof(TlsData*) == DebugFrame::offsetOfFrame(),
              "TLS pointer must be a field just before the wasm frame");
static_assert(sizeof(DebugFrame) % 8 == 0 && DebugFrame::offsetOfFrame() % 8 == 0,
              "DebugFrame and its portion is 8-bytes aligned for AbstractFramePtr");

} // namespace wasm
} // namespace js

#endif // wasmdebugframe_js_h
