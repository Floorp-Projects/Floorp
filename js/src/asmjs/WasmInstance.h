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

#include "mozilla/LinkedList.h"

#include "asmjs/WasmCode.h"
#include "gc/Barrier.h"

namespace js {

class WasmActivation;
class WasmInstanceObject;

namespace wasm {

struct ExportMap;

// TypedFuncTable is a compact version of LinkData::FuncTable that provides the
// necessary range information necessary to patch typed function table elements
// when the profiling mode is toggled.

struct TypedFuncTable
{
    uint32_t globalDataOffset;
    uint32_t numElems;

    TypedFuncTable(uint32_t globalDataOffset, uint32_t numElems)
      : globalDataOffset(globalDataOffset), numElems(numElems)
    {}
};

typedef Vector<TypedFuncTable, 0, SystemAllocPolicy> TypedFuncTableVector;

// Instance represents a wasm instance and provides all the support for runtime
// execution of code in the instance. Instances share various immutable data
// structures with the Module from which they were instantiated and other
// instances instantiated from the same Module. However, an Instance has no
// direct reference to its source Module which allows a Module to be destroyed
// while it still has live Instances.

class Instance : public mozilla::LinkedListElement<Instance>
{
    const UniqueCodeSegment              codeSegment_;
    const SharedMetadata                 metadata_;
    const SharedBytes                    maybeBytecode_;
    const TypedFuncTableVector           typedFuncTables_;

    GCPtr<ArrayBufferObjectMaybeShared*> heap_;
    GCPtr<WasmInstanceObject*>           object_;

    bool                                 profilingEnabled_;
    CacheableCharsVector                 funcLabels_;

    // Internal helpers:
    uint8_t** addressOfHeapPtr() const;
    ImportExit& importToExit(const Import& import);
    MOZ_MUST_USE bool toggleProfiling(JSContext* cx);

    // An instance keeps track of its innermost WasmActivation. A WasmActivation
    // is pushed for the duration of each call of an export.
    friend class js::WasmActivation;
    WasmActivation*& activation();

    // Import call slow paths which are called directly from wasm code.
    friend void* AddressOf(SymbolicAddress, ExclusiveContext*);
    bool callImport(JSContext* cx, uint32_t importIndex, unsigned argc, const uint64_t* argv,
                    MutableHandleValue rval);
    static int32_t callImport_void(int32_t importIndex, int32_t argc, uint64_t* argv);
    static int32_t callImport_i32(int32_t importIndex, int32_t argc, uint64_t* argv);
    static int32_t callImport_i64(int32_t importIndex, int32_t argc, uint64_t* argv);
    static int32_t callImport_f64(int32_t importIndex, int32_t argc, uint64_t* argv);

    template <class T> friend struct js::MallocProvider;
    Instance(UniqueCodeSegment codeSegment,
             const Metadata& metadata,
             const ShareableBytes* maybeBytecode,
             TypedFuncTableVector&& typedFuncTables,
             HandleArrayBufferObjectMaybeShared heap,
             Handle<WasmInstanceObject*> object);

  public:
    static bool create(JSContext* cx,
                       UniqueCodeSegment codeSegment,
                       const Metadata& metadata,
                       const ShareableBytes* maybeBytecode,
                       TypedFuncTableVector&& typedFuncTables,
                       HandleArrayBufferObjectMaybeShared heap,
                       Handle<FunctionVector> funcImports,
                       const ExportMap& exports,
                       MutableHandle<WasmInstanceObject*> instanceObj);
    ~Instance();
    void trace(JSTracer* trc);

    const CodeSegment& codeSegment() const { return *codeSegment_; }
    const Metadata& metadata() const { return *metadata_; }
    SharedMem<uint8_t*> heap() const;
    size_t heapLength() const;

    // Execute the given export given the JS call arguments, storing the return
    // value in args.rval.

    MOZ_MUST_USE bool callExport(JSContext* cx, uint32_t exportIndex, CallArgs args);

    // An instance has a profiling mode that is updated to match the runtime's
    // profiling mode when calling an instance's exports when there are no other
    // activations of the instance live on the stack. Once in profiling mode,
    // ProfilingFrameIterator can be used to asynchronously walk the stack.
    // Otherwise, the ProfilingFrameIterator will skip any activations of this
    // instance.

    bool profilingEnabled() const { return profilingEnabled_; }
    const char* profilingLabel(uint32_t funcIndex) const { return funcLabels_[funcIndex].get(); }

    // If the source binary was saved (by passing the bytecode to 'create'),
    // this method will render the binary as text. Otherwise, a diagnostic
    // string will be returned.

    JSString* createText(JSContext* cx);

    // Initially, calls to imports in wasm code call out through the generic
    // callImport method. If the imported callee gets JIT compiled and the types
    // match up, callImport will patch the code to instead call through a thunk
    // directly into the JIT code. If the JIT code is released, the Instance must
    // be notified so it can go back to the generic callImport.

    void deoptimizeImportExit(uint32_t importIndex);

    // Instances maintain a GC pointer to their wrapping WasmInstanceObject.
    //
    // NB: when this or any GC field of an instance is accessed through a weak
    // references (viz., via JSCompartment::wasmInstanceWeakList), readBarrier()
    // must be called (to ensure GC invariants).

    WasmInstanceObject& object() const { return *object_; }
    void readBarrier();

    // Stack frame iterator support:

    const CallSite* lookupCallSite(void* returnAddress) const;
    const CodeRange* lookupCodeRange(void* pc) const;
    const HeapAccess* lookupHeapAccess(void* pc) const;

    // about:memory reporting:

    void addSizeOfMisc(MallocSizeOf mallocSizeOf,
                       Metadata::SeenSet* seenMetadata,
                       ShareableBytes::SeenSet* seenBytes,
                       size_t* code, size_t* data) const;
};

typedef UniquePtr<Instance> UniqueInstance;

// These accessors are used to implemented the special asm.js semantics of
// exported wasm functions:

extern bool
IsExportedFunction(JSFunction* fun);

extern Instance&
ExportedFunctionToInstance(JSFunction* fun);

extern uint32_t
ExportedFunctionToExportIndex(JSFunction* fun);

} // namespace wasm
} // namespace js

#endif // wasm_instance_h
