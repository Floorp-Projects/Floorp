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

#ifndef wasm_debug_h
#define wasm_debug_h

#include "js/HashTable.h"
#include "wasm/WasmCode.h"
#include "wasm/WasmTypes.h"

namespace js {

class Debugger;
class WasmActivation;
class WasmBreakpoint;
class WasmBreakpointSite;
class WasmInstanceObject;

namespace wasm {

struct LinkData;
struct Metadata;
class FrameIterator;

// The generated source location for the AST node/expression. The offset field refers
// an offset in an binary format file.

struct ExprLoc
{
    uint32_t lineno;
    uint32_t column;
    uint32_t offset;
    ExprLoc() : lineno(0), column(0), offset(0) {}
    ExprLoc(uint32_t lineno_, uint32_t column_, uint32_t offset_)
      : lineno(lineno_), column(column_), offset(offset_)
    {}
};

typedef Vector<ExprLoc, 0, SystemAllocPolicy> ExprLocVector;
typedef Vector<uint32_t, 0, SystemAllocPolicy> ExprLocIndexVector;

// The generated source map for WebAssembly binary file. This map is generated during
// building the text buffer (see BinaryToExperimentalText).

class GeneratedSourceMap
{
    ExprLocVector exprlocs_;
    UniquePtr<ExprLocIndexVector> sortedByOffsetExprLocIndices_;
    uint32_t totalLines_;

  public:
    explicit GeneratedSourceMap() : totalLines_(0) {}
    ExprLocVector& exprlocs() { return exprlocs_; }

    uint32_t totalLines() { return totalLines_; }
    void setTotalLines(uint32_t val) { totalLines_ = val; }

    bool searchLineByOffset(JSContext* cx, uint32_t offset, size_t* exprlocIndex);
};

typedef UniquePtr<GeneratedSourceMap> UniqueGeneratedSourceMap;
typedef HashMap<uint32_t, uint32_t, DefaultHasher<uint32_t>, SystemAllocPolicy> StepModeCounters;
typedef HashMap<uint32_t, WasmBreakpointSite*, DefaultHasher<uint32_t>, SystemAllocPolicy> WasmBreakpointSiteMap;

class DebugState
{
    const SharedCode         code_;
    const SharedMetadata     metadata_;
    const SharedBytes        maybeBytecode_;
    UniqueGeneratedSourceMap maybeSourceMap_;

    // State maintained when debugging is enabled.  In this case, the Code is
    // not actually shared, but is referenced uniquely by the instance that is
    // being debugged.

    uint32_t                 enterAndLeaveFrameTrapsCounter_;
    WasmBreakpointSiteMap    breakpointSites_;
    StepModeCounters         stepModeCounters_;

    void toggleDebugTrap(uint32_t offset, bool enabled);
    bool ensureSourceMap(JSContext* cx);

  public:
    DebugState(SharedCode code,
               const Metadata& metadata,
               const ShareableBytes* maybeBytecode);

    // If the source bytecode was saved when this Code was constructed, this
    // method will render the binary as text. Otherwise, a diagnostic string
    // will be returned.

    JSString* createText(JSContext* cx);
    bool getLineOffsets(JSContext* cx, size_t lineno, Vector<uint32_t>* offsets);
    bool getOffsetLocation(JSContext* cx, uint32_t offset, bool* found, size_t* lineno, size_t* column);
    bool totalSourceLines(JSContext* cx, uint32_t* count);

    // The Code can track enter/leave frame events. Any such event triggers
    // debug trap. The enter/leave frame events enabled or disabled across
    // all functions.

    void adjustEnterAndLeaveFrameTrapsState(JSContext* cx, bool enabled);

    // When the Code is debugEnabled, individual breakpoints can be enabled or
    // disabled at instruction offsets.

    bool hasBreakpointTrapAtOffset(uint32_t offset);
    void toggleBreakpointTrap(JSRuntime* rt, uint32_t offset, bool enabled);
    WasmBreakpointSite* getOrCreateBreakpointSite(JSContext* cx, uint32_t offset);
    bool hasBreakpointSite(uint32_t offset);
    void destroyBreakpointSite(FreeOp* fop, uint32_t offset);
    bool clearBreakpointsIn(JSContext* cx, WasmInstanceObject* instance, js::Debugger* dbg, JSObject* handler);

    // When the Code is debug-enabled, single-stepping mode can be toggled on
    // the granularity of individual functions.

    bool stepModeEnabled(uint32_t funcIndex) const;
    bool incrementStepModeCount(JSContext* cx, uint32_t funcIndex);
    bool decrementStepModeCount(JSContext* cx, uint32_t funcIndex);

    // Stack inspection helpers.

    bool debugGetLocalTypes(uint32_t funcIndex, ValTypeVector* locals, size_t* argsLength);
    ExprType debugGetResultType(uint32_t funcIndex);

    // Debug URL helpers.

    JSString* debugDisplayURL(JSContext* cx) const;
};

typedef UniquePtr<DebugState> UniqueDebugState;

} // namespace wasm
} // namespace js

#endif // wasm_debug_h
