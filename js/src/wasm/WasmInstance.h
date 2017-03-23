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

#include "gc/Barrier.h"
#include "wasm/WasmCode.h"
#include "wasm/WasmTable.h"

namespace js {
namespace wasm {

// A wasm GlobalSegment owns the allocated global data for a wasm module.  A
// module may be compiled multiple times (at multiple tiers) but the compiled
// representations share the same GlobalSegment.

class GlobalSegment
{
    uint32_t globalDataLength_;
    TlsData* tlsData_;

    GlobalSegment(const GlobalSegment&) = delete;
    GlobalSegment(GlobalSegment&&) = delete;
    void operator=(const GlobalSegment&) = delete;
    void operator=(GlobalSegment&&) = delete;

  public:
    static UniquePtr<GlobalSegment> create(uint32_t globalDataLength);

    GlobalSegment() { PodZero(this); }
    ~GlobalSegment();

    TlsData* tlsData() const { return tlsData_; }
    uint8_t* globalData() const { return (uint8_t*)&tlsData_->globalArea; }
    uint32_t globalDataLength() const { return globalDataLength_; }

    size_t sizeOfMisc(MallocSizeOf mallocSizeOf) const;
};

typedef UniquePtr<GlobalSegment> UniqueGlobalSegment;

// Instance represents a wasm instance and provides all the support for runtime
// execution of code in the instance. Instances share various immutable data
// structures with the Module from which they were instantiated and other
// instances instantiated from the same Module. However, an Instance has no
// direct reference to its source Module which allows a Module to be destroyed
// while it still has live Instances.

class Instance
{
    JSCompartment* const            compartment_;
    ReadBarrieredWasmInstanceObject object_;
    const UniqueCode                code_;
    const UniqueGlobalSegment       globals_;
    GCPtrWasmMemoryObject           memory_;
    SharedTableVector               tables_;
    bool                            enterFrameTrapsEnabled_;

    // Internal helpers:
    const void** addressOfSigId(const SigIdDesc& sigId) const;
    FuncImportTls& funcImportTls(const FuncImport& fi);
    TableTls& tableTls(const TableDesc& td) const;

    // Import call slow paths which are called directly from wasm code.
    friend void* AddressOf(SymbolicAddress);
    static int32_t callImport_void(Instance*, int32_t, int32_t, uint64_t*);
    static int32_t callImport_i32(Instance*, int32_t, int32_t, uint64_t*);
    static int32_t callImport_i64(Instance*, int32_t, int32_t, uint64_t*);
    static int32_t callImport_f64(Instance*, int32_t, int32_t, uint64_t*);
    static uint32_t growMemory_i32(Instance* instance, uint32_t delta);
    static uint32_t currentMemory_i32(Instance* instance);
    bool callImport(JSContext* cx, uint32_t funcImportIndex, unsigned argc, const uint64_t* argv,
                    MutableHandleValue rval);

    // Only WasmInstanceObject can call the private trace function.
    friend class js::WasmInstanceObject;
    void tracePrivate(JSTracer* trc);

  public:
    Instance(JSContext* cx,
             HandleWasmInstanceObject object,
             UniqueCode code,
             UniqueGlobalSegment globals,
             HandleWasmMemoryObject memory,
             SharedTableVector&& tables,
             Handle<FunctionVector> funcImports,
             const ValVector& globalImports);
    ~Instance();
    bool init(JSContext* cx);
    void trace(JSTracer* trc);

    JSContext* cx() const { return tlsData()->cx; }
    JSCompartment* compartment() const { return compartment_; }
    Code& code() { return *code_; }
    const Code& code() const { return *code_; }
    const CodeSegment& codeSegment() const { return code_->segment(); }
    const GlobalSegment& globalSegment() const { return *globals_; }
    uint8_t* codeBase() const { return code_->segment().base(); }
    const Metadata& metadata() const { return code_->metadata(); }
    bool isAsmJS() const { return metadata().isAsmJS(); }
    const SharedTableVector& tables() const { return tables_; }
    SharedMem<uint8_t*> memoryBase() const;
    size_t memoryLength() const;
    size_t memoryMappedSize() const;
    bool memoryAccessInGuardRegion(uint8_t* addr, unsigned numBytes) const;
    TlsData* tlsData() const { return globals_->tlsData(); }

    // This method returns a pointer to the GC object that owns this Instance.
    // Instances may be reached via weak edges (e.g., Compartment::instances_)
    // so this perform a read-barrier on the returned object unless the barrier
    // is explicitly waived.

    WasmInstanceObject* object() const;
    WasmInstanceObject* objectUnbarriered() const;

    // Execute the given export given the JS call arguments, storing the return
    // value in args.rval.

    MOZ_MUST_USE bool callExport(JSContext* cx, uint32_t funcIndex, CallArgs args);

    // Initially, calls to imports in wasm code call out through the generic
    // callImport method. If the imported callee gets JIT compiled and the types
    // match up, callImport will patch the code to instead call through a thunk
    // directly into the JIT code. If the JIT code is released, the Instance must
    // be notified so it can go back to the generic callImport.

    void deoptimizeImportExit(uint32_t funcImportIndex);

    // Called by simulators to check whether accessing 'numBytes' starting at
    // 'addr' would trigger a fault and be safely handled by signal handlers.

    bool memoryAccessWouldFault(uint8_t* addr, unsigned numBytes);

    // Called by Wasm(Memory|Table)Object when a moving resize occurs:

    void onMovingGrowMemory(uint8_t* prevMemoryBase);
    void onMovingGrowTable();

    // Debug support:
    bool debugEnabled() const { return code_->metadata().debugEnabled; }
    bool enterFrameTrapsEnabled() const { return enterFrameTrapsEnabled_; }
    void ensureEnterFrameTrapsState(JSContext* cx, bool enabled);

    // about:memory reporting:

    void addSizeOfMisc(MallocSizeOf mallocSizeOf,
                       Metadata::SeenSet* seenMetadata,
                       ShareableBytes::SeenSet* seenBytes,
                       Table::SeenSet* seenTables,
                       size_t* code,
                       size_t* data) const;
};

typedef UniquePtr<Instance> UniqueInstance;

bool InitInstanceStaticData();
void ShutDownInstanceStaticData();

} // namespace wasm
} // namespace js

#endif // wasm_instance_h
