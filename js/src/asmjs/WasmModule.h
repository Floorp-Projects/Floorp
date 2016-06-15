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

#include "asmjs/WasmCode.h"
#include "js/TypeDecls.h"

namespace js {

class ArrayBufferObjectMaybeShared;
class WasmInstanceObject;

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

    struct FuncTable {
        uint32_t globalDataOffset;
        Uint32Vector elemOffsets;
        FuncTable(uint32_t globalDataOffset, Uint32Vector&& elemOffsets)
          : globalDataOffset(globalDataOffset), elemOffsets(Move(elemOffsets))
        {}
        FuncTable(FuncTable&& rhs)
          : globalDataOffset(rhs.globalDataOffset), elemOffsets(Move(rhs.elemOffsets))
        {}
        FuncTable() = default;
        WASM_DECLARE_SERIALIZABLE(FuncTable)
    };
    typedef Vector<FuncTable, 0, SystemAllocPolicy> FuncTableVector;

    InternalLinkVector  internalLinks;
    SymbolicLinkArray   symbolicLinks;
    FuncTableVector     funcTables;

    WASM_DECLARE_SERIALIZABLE(LinkData)
};

typedef UniquePtr<LinkData> UniqueLinkData;
typedef UniquePtr<const LinkData> UniqueConstLinkData;

// ImportName describes a single wasm import. An ImportNameVector describes all
// of a single module's imports.
//
// ImportNameVector is built incrementally by ModuleGenerator and then stored
// immutably by Module.

struct ImportName
{
    CacheableChars module;
    CacheableChars func;

    ImportName() = default;
    ImportName(UniqueChars&& module, UniqueChars&& func)
      : module(Move(module)), func(Move(func))
    {}

    WASM_DECLARE_SERIALIZABLE(ImportName)
};

typedef Vector<ImportName, 0, SystemAllocPolicy> ImportNameVector;

// ExportMap describes all of a single module's exports. The ExportMap describes
// how the Exports (stored in Metadata) are mapped to the fields of the export
// object produced by instantiation. The 'fieldNames' vector provides the list
// of names of the module's exports. For each field name, 'fieldsToExports'
// provides either:
//  - the sentinel value MemoryExport indicating an export of linear memory; or
//  - the index of an export into the ExportVector in Metadata
//
// ExportMap is built incrementally by ModuleGenerator and then stored immutably
// by Module.

static const uint32_t MemoryExport = UINT32_MAX;

struct ExportMap
{
    CacheableCharsVector fieldNames;
    Uint32Vector fieldsToExports;

    WASM_DECLARE_SERIALIZABLE(ExportMap)
};

// Module represents a compiled wasm module and primarily provides two
// operations: instantiation and serialization. A Module can be instantiated any
// number of times to produce new Instance objects. A Module can be serialized
// any number of times such that the serialized bytes can be deserialized later
// to produce and new, equivalent Module.
//
// Since fully linked-and-instantiated code (represented by CodeSegment) cannot
// be shared between instances, Module stores an unlinked, uninstantiated copy
// of the code (represented by the Bytes) and creates a new CodeSegment each
// time it is instantiated. In the future, Module will store a shareable,
// immutable CodeSegment that can be shared by all its instances.

class Module
{
    const Bytes            code_;
    const LinkData         linkData_;
    const ImportNameVector importNames_;
    const ExportMap        exportMap_;
    const SharedMetadata   metadata_;
    const SharedBytes      bytecode_;

  public:
    Module(Bytes&& code,
           LinkData&& linkData,
           ImportNameVector&& importNames,
           ExportMap&& exportMap,
           const Metadata& metadata,
           const ShareableBytes& bytecode)
      : code_(Move(code)),
        linkData_(Move(linkData)),
        importNames_(Move(importNames)),
        exportMap_(Move(exportMap)),
        metadata_(&metadata),
        bytecode_(&bytecode)
    {}

    const Metadata& metadata() const { return *metadata_; }
    const ImportNameVector& importNames() const { return importNames_; }

    // Instantiate this module with the given imports:

    bool instantiate(JSContext* cx,
                     Handle<FunctionVector> funcImports,
                     Handle<ArrayBufferObjectMaybeShared*> heap,
                     MutableHandle<WasmInstanceObject*> instanceObj) const;

    // Structured clone support:

    size_t serializedSize() const;
    uint8_t* serialize(uint8_t* cursor) const;
    static const uint8_t* deserialize(ExclusiveContext* cx, const uint8_t* cursor,
                                      UniquePtr<Module>* module,
                                      Metadata* maybeMetadata = nullptr);

    // about:memory reporting:

    void addSizeOfMisc(MallocSizeOf mallocSizeOf,
                       Metadata::SeenSet* seenMetadata,
                       ShareableBytes::SeenSet* seenBytes,
                       size_t* code, size_t* data) const;
};

typedef UniquePtr<Module> UniqueModule;

} // namespace wasm
} // namespace js

#endif // wasm_module_h
