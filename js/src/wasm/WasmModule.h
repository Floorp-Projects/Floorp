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

#include "js/TypeDecls.h"

#include "wasm/WasmCode.h"
#include "wasm/WasmTable.h"

namespace js {
namespace wasm {

// LinkData contains all the metadata necessary to patch all the locations
// that depend on the absolute address of a CodeSegment.
//
// LinkData is built incrementing by ModuleGenerator and then stored immutably
// in Module.

struct LinkDataCacheablePod
{
    uint32_t functionCodeLength;
    uint32_t globalDataLength;
    uint32_t interruptOffset;
    uint32_t outOfBoundsOffset;
    uint32_t unalignedAccessOffset;

    LinkDataCacheablePod() { mozilla::PodZero(this); }
};

struct LinkData : LinkDataCacheablePod
{
    LinkDataCacheablePod& pod() { return *this; }
    const LinkDataCacheablePod& pod() const { return *this; }

    struct InternalLink {
        enum Kind {
            RawPointer,
            CodeLabel,
            InstructionImmediate
        };
        MOZ_INIT_OUTSIDE_CTOR uint32_t patchAtOffset;
        MOZ_INIT_OUTSIDE_CTOR uint32_t targetOffset;

        InternalLink() = default;
        explicit InternalLink(Kind kind);
        bool isRawPointerPatch();
    };
    typedef Vector<InternalLink, 0, SystemAllocPolicy> InternalLinkVector;

    struct SymbolicLinkArray : EnumeratedArray<SymbolicAddress, SymbolicAddress::Limit, Uint32Vector> {
        WASM_DECLARE_SERIALIZABLE(SymbolicLinkArray)
    };

    InternalLinkVector  internalLinks;
    SymbolicLinkArray   symbolicLinks;

    WASM_DECLARE_SERIALIZABLE(LinkData)
};

typedef UniquePtr<LinkData> UniqueLinkData;
typedef UniquePtr<const LinkData> UniqueConstLinkData;

// Import describes a single wasm import. An ImportVector describes all
// of a single module's imports.
//
// ImportVector is built incrementally by ModuleGenerator and then stored
// immutably by Module.

struct Import
{
    CacheableChars module;
    CacheableChars field;
    DefinitionKind kind;

    Import() = default;
    Import(UniqueChars&& module, UniqueChars&& field, DefinitionKind kind)
      : module(Move(module)), field(Move(field)), kind(kind)
    {}

    WASM_DECLARE_SERIALIZABLE(Import)
};

typedef Vector<Import, 0, SystemAllocPolicy> ImportVector;

// Export describes the export of a definition in a Module to a field in the
// export object. For functions, Export stores an index into the
// FuncDefExportVector in Metadata. For memory and table exports, there is
// at most one (default) memory/table so no index is needed. Note: a single
// definition can be exported by multiple Exports in the ExportVector.
//
// ExportVector is built incrementally by ModuleGenerator and then stored
// immutably by Module.

class Export
{
    CacheableChars fieldName_;
    struct CacheablePod {
        DefinitionKind kind_;
        uint32_t index_;
    } pod;

  public:
    Export() = default;
    explicit Export(UniqueChars fieldName, uint32_t index, DefinitionKind kind);
    explicit Export(UniqueChars fieldName, DefinitionKind kind);

    const char* fieldName() const { return fieldName_.get(); }

    DefinitionKind kind() const { return pod.kind_; }
    uint32_t funcIndex() const;
    uint32_t globalIndex() const;

    WASM_DECLARE_SERIALIZABLE(Export)
};

typedef Vector<Export, 0, SystemAllocPolicy> ExportVector;

// ElemSegment represents an element segment in the module where each element
// describes both its function index and its code range.

struct ElemSegment
{
    uint32_t tableIndex;
    InitExpr offset;
    Uint32Vector elemFuncIndices;
    Uint32Vector elemCodeRangeIndices;

    ElemSegment() = default;
    ElemSegment(uint32_t tableIndex, InitExpr offset, Uint32Vector&& elemFuncIndices)
      : tableIndex(tableIndex), offset(offset), elemFuncIndices(Move(elemFuncIndices))
    {}

    WASM_DECLARE_SERIALIZABLE(ElemSegment)
};

typedef Vector<ElemSegment, 0, SystemAllocPolicy> ElemSegmentVector;

// Module represents a compiled wasm module and primarily provides two
// operations: instantiation and serialization. A Module can be instantiated any
// number of times to produce new Instance objects. A Module can be serialized
// any number of times such that the serialized bytes can be deserialized later
// to produce a new, equivalent Module.
//
// Since fully linked-and-instantiated code (represented by CodeSegment) cannot
// be shared between instances, Module stores an unlinked, uninstantiated copy
// of the code (represented by the Bytes) and creates a new CodeSegment each
// time it is instantiated. In the future, Module will store a shareable,
// immutable CodeSegment that can be shared by all its instances.

class Module : public JS::WasmModule
{
    const Assumptions       assumptions_;
    const Bytes             code_;
    const LinkData          linkData_;
    const ImportVector      imports_;
    const ExportVector      exports_;
    const DataSegmentVector dataSegments_;
    const ElemSegmentVector elemSegments_;
    const SharedMetadata    metadata_;
    const SharedBytes       bytecode_;

    bool instantiateFunctions(JSContext* cx, Handle<FunctionVector> funcImports) const;
    bool instantiateMemory(JSContext* cx, MutableHandleWasmMemoryObject memory) const;
    bool instantiateTable(JSContext* cx,
                          MutableHandleWasmTableObject table,
                          SharedTableVector* tables) const;
    bool initSegments(JSContext* cx,
                      HandleWasmInstanceObject instance,
                      Handle<FunctionVector> funcImports,
                      HandleWasmMemoryObject memory,
                      const ValVector& globalImports) const;

  public:
    Module(Assumptions&& assumptions,
           Bytes&& code,
           LinkData&& linkData,
           ImportVector&& imports,
           ExportVector&& exports,
           DataSegmentVector&& dataSegments,
           ElemSegmentVector&& elemSegments,
           const Metadata& metadata,
           const ShareableBytes& bytecode)
      : assumptions_(Move(assumptions)),
        code_(Move(code)),
        linkData_(Move(linkData)),
        imports_(Move(imports)),
        exports_(Move(exports)),
        dataSegments_(Move(dataSegments)),
        elemSegments_(Move(elemSegments)),
        metadata_(&metadata),
        bytecode_(&bytecode)
    {}
    ~Module() override { /* Note: can be called on any thread */ }

    const Metadata& metadata() const { return *metadata_; }
    const ImportVector& imports() const { return imports_; }

    // Instantiate this module with the given imports:

    bool instantiate(JSContext* cx,
                     Handle<FunctionVector> funcImports,
                     HandleWasmTableObject tableImport,
                     HandleWasmMemoryObject memoryImport,
                     const ValVector& globalImports,
                     HandleObject instanceProto,
                     MutableHandleWasmInstanceObject instanceObj) const;

    // Structured clone support:

    void serializedSize(size_t* bytecodeSize, size_t* compiledSize) const override;
    void serialize(uint8_t* bytecodeBegin, size_t bytecodeSize,
                   uint8_t* compiledBegin, size_t compiledSize) const override;
    static bool assumptionsMatch(const Assumptions& current, const uint8_t* compiledBegin);
    static RefPtr<Module> deserialize(const uint8_t* bytecodeBegin, size_t bytecodeSize,
                                      const uint8_t* compiledBegin, size_t compiledSize,
                                      Metadata* maybeMetadata = nullptr);
    JSObject* createObject(JSContext* cx) override;

    // about:memory reporting:

    void addSizeOfMisc(MallocSizeOf mallocSizeOf,
                       Metadata::SeenSet* seenMetadata,
                       ShareableBytes::SeenSet* seenBytes,
                       size_t* code, size_t* data) const;

    // Generated code analysis support:

    bool extractCode(JSContext* cx, MutableHandleValue vp);
};

typedef RefPtr<Module> SharedModule;

// JS API implementations:

bool
CompiledModuleAssumptionsMatch(PRFileDesc* compiled, JS::BuildIdCharVector&& buildId);

SharedModule
DeserializeModule(PRFileDesc* bytecode, PRFileDesc* maybeCompiled, JS::BuildIdCharVector&& buildId,
                  UniqueChars filename, unsigned line, unsigned column);

} // namespace wasm
} // namespace js

#endif // wasm_module_h
