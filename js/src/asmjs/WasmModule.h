/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright 2015 Mozilla Foundation
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

#ifndef wasm_module_h
#define wasm_module_h

#include "mozilla/LinkedList.h"

#include "asmjs/WasmCode.h"
#include "gc/Barrier.h"
#include "vm/MallocProvider.h"
#include "vm/NativeObject.h"

namespace js {

class AsmJSModule;
class WasmActivation;
class WasmModuleObject;
namespace jit { struct BaselineScript; }

namespace wasm {

// The StaticLinkData contains all the metadata necessary to perform
// Module::staticallyLink but is not necessary afterwards.
//
// StaticLinkData is built incrementing by ModuleGenerator and then shared
// immutably between modules.

struct StaticLinkData : RefCounted<StaticLinkData>
{
    struct InternalLink {
        enum Kind {
            RawPointer,
            CodeLabel,
            InstructionImmediate
        };
        uint32_t patchAtOffset;
        uint32_t targetOffset;

        InternalLink() = default;
        explicit InternalLink(Kind kind);
        bool isRawPointerPatch();
    };
    typedef Vector<InternalLink, 0, SystemAllocPolicy> InternalLinkVector;

    struct SymbolicLinkArray : EnumeratedArray<SymbolicAddress, SymbolicAddress::Limit, Uint32Vector> {
        WASM_DECLARE_SERIALIZABLE(SymbolicLinkArray)
    };

    struct FuncPtrTable {
        uint32_t globalDataOffset;
        Uint32Vector elemOffsets;
        FuncPtrTable(uint32_t globalDataOffset, Uint32Vector&& elemOffsets)
          : globalDataOffset(globalDataOffset), elemOffsets(Move(elemOffsets))
        {}
        FuncPtrTable(FuncPtrTable&& rhs)
          : globalDataOffset(rhs.globalDataOffset), elemOffsets(Move(rhs.elemOffsets))
        {}
        FuncPtrTable() = default;
        WASM_DECLARE_SERIALIZABLE(FuncPtrTable)
    };
    typedef Vector<FuncPtrTable, 0, SystemAllocPolicy> FuncPtrTableVector;

    struct CacheablePod {
        uint32_t        interruptOffset;
        uint32_t        outOfBoundsOffset;
    } pod;
    InternalLinkVector  internalLinks;
    SymbolicLinkArray   symbolicLinks;
    FuncPtrTableVector  funcPtrTables;

    WASM_DECLARE_SERIALIZABLE(StaticLinkData)
};

typedef RefPtr<StaticLinkData> MutableStaticLinkData;
typedef RefPtr<const StaticLinkData> SharedStaticLinkData;

// The ExportMap describes how Exports are mapped to the fields of the export
// object. This allows a single Export to be used in multiple fields.
// The 'fieldNames' vector provides the list of names of the module's exports.
// For each field in fieldNames, 'fieldsToExports' provides either:
//  - the sentinel value MemoryExport indicating an export of linear memory; or
//  - the index of an export into the ExportVector in Metadata
//
// The ExportMap is built incrementally by ModuleGenerator and then shared
// immutably between modules.

static const uint32_t MemoryExport = UINT32_MAX;

struct ExportMap : RefCounted<ExportMap>
{
    CacheableCharsVector fieldNames;
    Uint32Vector fieldsToExports;

    WASM_DECLARE_SERIALIZABLE(ExportMap)
};

typedef RefPtr<ExportMap> MutableExportMap;
typedef RefPtr<const ExportMap> SharedExportMap;

// Module represents a compiled WebAssembly module which lives until the last
// reference to any exported functions is dropped. Modules must be wrapped by a
// rooted JSObject immediately after creation so that Module::trace() is called
// during GC. Modules are created after compilation completes and start in a
// a fully unlinked state. After creation, a module must be first statically
// linked and then dynamically linked:
//
//  - Static linking patches code or global data that relies on absolute
//    addresses. Static linking should happen after a module is serialized into
//    a cache file so that the cached code is stored unlinked and ready to be
//    statically linked after deserialization.
//
//  - Dynamic linking patches code or global data that relies on the address of
//    the heap and imports of a module. A module may only be dynamically linked
//    once. However, a dynamically-linked module may be cloned so that the clone
//    can be independently dynamically linked.
//
// Once fully dynamically linked, a Module can have its exports invoked via
// callExport().

class Module : public mozilla::LinkedListElement<Module>
{
    struct ImportExit {
        void* code;
        jit::BaselineScript* baselineScript;
        GCPtrFunction fun;
        static_assert(sizeof(GCPtrFunction) == sizeof(void*), "for JIT access");
    };
    struct EntryArg {
        uint64_t lo;
        uint64_t hi;
    };
    typedef int32_t (*EntryFuncPtr)(EntryArg* args, uint8_t* global);
    struct FuncPtrTable {
        uint32_t globalDataOffset;
        uint32_t numElems;
        explicit FuncPtrTable(const StaticLinkData::FuncPtrTable& table)
          : globalDataOffset(table.globalDataOffset),
            numElems(table.elemOffsets.length())
        {}
    };
    typedef Vector<FuncPtrTable, 0, SystemAllocPolicy> FuncPtrTableVector;
    typedef Vector<CacheableChars, 0, SystemAllocPolicy> FuncLabelVector;
    typedef HeapPtr<ArrayBufferObjectMaybeShared*> BufferPtr;
    typedef GCPtr<WasmModuleObject*> ModuleObjectPtr;

    // Initialized when constructed:
    const UniqueCodeSegment      codeSegment_;
    const SharedMetadata         metadata_;

    // Initialized during staticallyLink:
    bool                         staticallyLinked_;
    uint8_t*                     interrupt_;
    uint8_t*                     outOfBounds_;
    FuncPtrTableVector           funcPtrTables_;

    // Initialized during dynamicallyLink:
    bool                         dynamicallyLinked_;
    BufferPtr                    heap_;

    // Mutated after dynamicallyLink:
    bool                         profilingEnabled_;
    FuncLabelVector              funcLabels_;

    // Back pointer to the JS object.
    ModuleObjectPtr              ownerObject_;

    // Possibly stored copy of the bytes from which this module was compiled.
    Bytes                        source_;

    uint8_t* rawHeapPtr() const;
    uint8_t*& rawHeapPtr();
    WasmActivation*& activation();
    void specializeToHeap(ArrayBufferObjectMaybeShared* heap);
    void despecializeFromHeap(ArrayBufferObjectMaybeShared* heap);
    MOZ_MUST_USE bool sendCodeRangesToProfiler(JSContext* cx);
    MOZ_MUST_USE bool setProfilingEnabled(JSContext* cx, bool enabled);
    ImportExit& importToExit(const Import& import);

    bool callImport(JSContext* cx, uint32_t importIndex, unsigned argc, const uint64_t* argv,
                    MutableHandleValue rval);
    static int32_t callImport_void(int32_t importIndex, int32_t argc, uint64_t* argv);
    static int32_t callImport_i32(int32_t importIndex, int32_t argc, uint64_t* argv);
    static int32_t callImport_i64(int32_t importIndex, int32_t argc, uint64_t* argv);
    static int32_t callImport_f64(int32_t importIndex, int32_t argc, uint64_t* argv);

    friend class js::WasmActivation;
    friend void* wasm::AddressOf(SymbolicAddress, ExclusiveContext*);

  protected:
    const CodeSegment& codeSegment() const { return *codeSegment_; }
    const Metadata& metadata() const { return *metadata_; }
    MOZ_MUST_USE bool clone(JSContext* cx, const StaticLinkData& link, Module* clone) const;

  public:
    static const unsigned SizeOfImportExit = sizeof(ImportExit);
    static const unsigned OffsetOfImportExitFun = offsetof(ImportExit, fun);
    static const unsigned SizeOfEntryArg = sizeof(EntryArg);

    explicit Module(UniqueCodeSegment codeSegment, const Metadata& metadata);
    virtual ~Module();
    virtual void trace(JSTracer* trc);
    virtual void readBarrier();
    virtual void addSizeOfMisc(MallocSizeOf mallocSizeOf, size_t* code, size_t* data);

    void setOwner(WasmModuleObject* owner) { MOZ_ASSERT(!ownerObject_); ownerObject_ = owner; }
    inline const GCPtr<WasmModuleObject*>& owner() const;

    void setSource(Bytes&& source) { source_ = Move(source); }

    uint8_t* code() const { return codeSegment_->code(); }
    uint32_t codeLength() const { return codeSegment_->codeLength(); }
    uint8_t* globalData() const { return codeSegment_->globalData(); }
    uint32_t globalDataLength() const { return codeSegment_->globalDataLength(); }
    HeapUsage heapUsage() const { return metadata_->heapUsage; }
    bool usesHeap() const { return UsesHeap(metadata_->heapUsage); }
    bool hasSharedHeap() const { return metadata_->heapUsage == HeapUsage::Shared; }
    CompileArgs compileArgs() const { return metadata_->compileArgs; }
    const ImportVector& imports() const { return metadata_->imports; }
    const ExportVector& exports() const { return metadata_->exports; }
    const CodeRangeVector& codeRanges() const { return metadata_->codeRanges; }
    const char* filename() const { return metadata_->filename.get(); }
    bool staticallyLinked() const { return staticallyLinked_; }
    bool dynamicallyLinked() const { return dynamicallyLinked_; }

    // Some wasm::Module's have the most-derived type AsmJSModule. The
    // AsmJSModule stores the extra metadata necessary to implement asm.js (JS)
    // semantics. The asAsmJS() member may be used as a checked downcast when
    // isAsmJS() is true.

    bool isAsmJS() const { return metadata_->kind == ModuleKind::AsmJS; }
    AsmJSModule& asAsmJS() { MOZ_ASSERT(isAsmJS()); return *(AsmJSModule*)this; }
    const AsmJSModule& asAsmJS() const { MOZ_ASSERT(isAsmJS()); return *(const AsmJSModule*)this; }
    virtual bool mutedErrors() const;
    virtual const char16_t* displayURL() const;
    virtual ScriptSource* maybeScriptSource() const { return nullptr; }

    // The range [0, functionLength) is a subrange of [0, codeLength) that
    // contains only function body code, not the stub code. This distinction is
    // used by the async interrupt handler to only interrupt when the pc is in
    // function code which, in turn, simplifies reasoning about how stubs
    // enter/exit.

    bool containsFunctionPC(void* pc) const;
    bool containsCodePC(void* pc) const;
    const CallSite* lookupCallSite(void* returnAddress) const;
    const CodeRange* lookupCodeRange(void* pc) const;
    const HeapAccess* lookupHeapAccess(void* pc) const;

    // This function transitions the module from an unlinked state to a
    // statically-linked state. The given StaticLinkData must have come from the
    // compilation of this module.

    MOZ_MUST_USE bool staticallyLink(ExclusiveContext* cx, const StaticLinkData& link);

    // This function transitions the module from a statically-linked state to a
    // dynamically-linked state. If this module usesHeap(), a non-null heap
    // buffer must be given. The given import vector must match the module's
    // ImportVector. The function returns a new export object for this module.

    MOZ_MUST_USE bool dynamicallyLink(JSContext* cx,
                                      Handle<WasmModuleObject*> moduleObj,
                                      Handle<ArrayBufferObjectMaybeShared*> heap,
                                      Handle<FunctionVector> imports,
                                      const ExportMap& exportMap,
                                      MutableHandleObject exportObj);

    // The wasm heap, established by dynamicallyLink.

    SharedMem<uint8_t*> heap() const;
    size_t heapLength() const;

    // The exports of a wasm module are called by preparing an array of
    // arguments (coerced to the corresponding types of the Export signature)
    // and calling the export's entry trampoline.

    MOZ_MUST_USE bool callExport(JSContext* cx, uint32_t exportIndex, CallArgs args);

    // Initially, calls to imports in wasm code call out through the generic
    // callImport method. If the imported callee gets JIT compiled and the types
    // match up, callImport will patch the code to instead call through a thunk
    // directly into the JIT code. If the JIT code is released, the Module must
    // be notified so it can go back to the generic callImport.

    void deoptimizeImportExit(uint32_t importIndex);

    // At runtime, when $pc is in wasm function code (containsFunctionPC($pc)),
    // $pc may be moved abruptly to interrupt() or outOfBounds() by a signal
    // handler or SetContext() from another thread.

    uint8_t* interrupt() const { MOZ_ASSERT(staticallyLinked_); return interrupt_; }
    uint8_t* outOfBounds() const { MOZ_ASSERT(staticallyLinked_); return outOfBounds_; }

    // Every function has an associated display atom which is either the pretty
    // name given by the asm.js function name or wasm symbols or something
    // generated from the function index.

    const char* maybePrettyFuncName(uint32_t funcIndex) const;
    const char* getFuncName(JSContext* cx, uint32_t funcIndex, UniqueChars* owner) const;
    JSAtom* getFuncAtom(JSContext* cx, uint32_t funcIndex) const;

    // If debuggerObservesAsmJS was true when the module was compiled, render
    // the binary to a new source string.

    JSString* createText(JSContext* cx);

    // Each Module has a profilingEnabled state which is updated to match
    // SPSProfiler::enabled() on the next Module::callExport when there are no
    // frames from the Module on the stack. The ProfilingFrameIterator only
    // shows frames for Module activations that have profilingEnabled.

    bool profilingEnabled() const { return profilingEnabled_; }
    const char* profilingLabel(uint32_t funcIndex) const;
};

typedef UniquePtr<Module> UniqueModule;

// Tests to query and access the exported JS functions produced by
// Module::createExportObject.

extern bool
IsExportedFunction(JSFunction* fun);

extern WasmModuleObject*
ExportedFunctionToModuleObject(JSFunction* fun);

extern uint32_t
ExportedFunctionToIndex(JSFunction* fun);

} // namespace wasm

// An WasmModuleObject is an internal object (i.e., not exposed directly to user
// code) which traces and owns a wasm::Module. The WasmModuleObject is
// referenced by the extended slots of exported JSFunctions and serves to keep
// the wasm::Module alive until its last GC reference is dead.

class WasmModuleObject : public NativeObject
{
    static const unsigned MODULE_SLOT = 0;
    static const ClassOps classOps_;

    bool hasModule() const;
    static void finalize(FreeOp* fop, JSObject* obj);
    static void trace(JSTracer* trc, JSObject* obj);
  public:
    static const unsigned RESERVED_SLOTS = 1;
    static WasmModuleObject* create(ExclusiveContext* cx);
    void init(wasm::Module& module);
    wasm::Module& module() const;
    void addSizeOfMisc(mozilla::MallocSizeOf mallocSizeOf, size_t* code, size_t* data);
    static const Class class_;
};

inline const GCPtr<WasmModuleObject*>&
wasm::Module::owner() const {
    MOZ_ASSERT(&ownerObject_->module() == this);
    return ownerObject_;
}

using WasmModuleObjectVector = GCVector<WasmModuleObject*>;

} // namespace js

#endif // wasm_module_h
