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

#ifndef wasm_instance_h
#define wasm_instance_h

#include "asmjs/WasmCode.h"
#include "asmjs/WasmTable.h"
#include "gc/Barrier.h"

namespace js {

class WasmActivation;
class WasmInstanceObject;

namespace wasm {

class GeneratedSourceMap;

// Instance represents a wasm instance and provides all the support for runtime
// execution of code in the instance. Instances share various immutable data
// structures with the Module from which they were instantiated and other
// instances instantiated from the same Module. However, an Instance has no
// direct reference to its source Module which allows a Module to be destroyed
// while it still has live Instances.

class Instance
{
    const UniqueCodeSegment              codeSegment_;
    const SharedMetadata                 metadata_;
    const SharedBytes                    maybeBytecode_;
    GCPtrWasmMemoryObject                memory_;
    SharedTableVector                    tables_;

    bool                                 profilingEnabled_;
    CacheableCharsVector                 funcLabels_;

    UniquePtr<GeneratedSourceMap>        maybeSourceMap_;

    // Internal helpers:
    uint8_t** addressOfMemoryBase() const;
    void** addressOfTableBase(size_t tableIndex) const;
    const void** addressOfSigId(const SigIdDesc& sigId) const;
    FuncImportExit& funcImportToExit(const FuncImport& fi);
    MOZ_MUST_USE bool toggleProfiling(JSContext* cx);

    // An instance keeps track of its innermost WasmActivation. A WasmActivation
    // is pushed for the duration of each call of an export.
    friend class js::WasmActivation;
    WasmActivation*& activation();

    // Import call slow paths which are called directly from wasm code.
    friend void* AddressOf(SymbolicAddress, ExclusiveContext*);
    bool callImport(JSContext* cx, uint32_t funcImportIndex, unsigned argc, const uint64_t* argv,
                    MutableHandleValue rval);
    static int32_t callImport_void(int32_t importIndex, int32_t argc, uint64_t* argv);
    static int32_t callImport_i32(int32_t importIndex, int32_t argc, uint64_t* argv);
    static int32_t callImport_i64(int32_t importIndex, int32_t argc, uint64_t* argv);
    static int32_t callImport_f64(int32_t importIndex, int32_t argc, uint64_t* argv);

  public:
    Instance(UniqueCodeSegment codeSegment,
             const Metadata& metadata,
             const ShareableBytes* maybeBytecode,
             HandleWasmMemoryObject memory,
             SharedTableVector&& tables,
             Handle<FunctionVector> funcImports);
    ~Instance();
    bool init(JSContext* cx);
    void trace(JSTracer* trc);

    const CodeSegment& codeSegment() const { return *codeSegment_; }
    const Metadata& metadata() const { return *metadata_; }
    const SharedTableVector& tables() const { return tables_; }
    SharedMem<uint8_t*> memoryBase() const;
    size_t memoryLength() const;

    // Execute the given export given the JS call arguments, storing the return
    // value in args.rval.

    MOZ_MUST_USE bool callExport(JSContext* cx, uint32_t funcIndex, CallArgs args);

    // An instance has a profiling mode that is updated to match the runtime's
    // profiling mode when calling an instance's exports when there are no other
    // activations of the instance live on the stack. Once in profiling mode,
    // ProfilingFrameIterator can be used to asynchronously walk the stack.
    // Otherwise, the ProfilingFrameIterator will skip any activations of this
    // instance.

    bool profilingEnabled() const { return profilingEnabled_; }
    const char* profilingLabel(uint32_t funcIndex) const { return funcLabels_[funcIndex].get(); }

    // If the source binary was saved (by passing the bytecode to the
    // constructor), this method will render the binary as text. Otherwise, a
    // diagnostic string will be returned.

    // Text format support functions:

    JSString* createText(JSContext* cx);
    bool getLineOffsets(size_t lineno, Vector<uint32_t>& offsets);

    // Return the name associated with a given function index, or generate one
    // if none was given by the module.

    bool getFuncName(JSContext* cx, uint32_t funcIndex, TwoByteName* name) const;
    JSAtom* getFuncAtom(JSContext* cx, uint32_t funcIndex) const;

    // Initially, calls to imports in wasm code call out through the generic
    // callImport method. If the imported callee gets JIT compiled and the types
    // match up, callImport will patch the code to instead call through a thunk
    // directly into the JIT code. If the JIT code is released, the Instance must
    // be notified so it can go back to the generic callImport.

    void deoptimizeImportExit(uint32_t funcImportIndex);

    // Stack frame iterator support:

    const CallSite* lookupCallSite(void* returnAddress) const;
    const CodeRange* lookupCodeRange(void* pc) const;
#ifdef ASMJS_MAY_USE_SIGNAL_HANDLERS
    const MemoryAccess* lookupMemoryAccess(void* pc) const;
#endif

    // about:memory reporting:

    void addSizeOfMisc(MallocSizeOf mallocSizeOf,
                       Metadata::SeenSet* seenMetadata,
                       ShareableBytes::SeenSet* seenBytes,
                       Table::SeenSet* seenTables,
                       size_t* code,
                       size_t* data) const;
};

typedef UniquePtr<Instance> UniqueInstance;

} // namespace wasm
} // namespace js

#endif // wasm_instance_h
