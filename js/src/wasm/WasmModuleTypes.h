/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Copyright 2021 Mozilla Foundation
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

#ifndef wasm_module_types_h
#define wasm_module_types_h

#include "js/AllocPolicy.h"
#include "js/RefCounted.h"
#include "js/Utility.h"
#include "js/Vector.h"

#include "wasm/WasmCompileArgs.h"
#include "wasm/WasmConstants.h"
#include "wasm/WasmExprType.h"
#include "wasm/WasmInitExpr.h"
#include "wasm/WasmMemory.h"
#include "wasm/WasmSerialize.h"
#include "wasm/WasmShareable.h"
#include "wasm/WasmTypeDecls.h"
#include "wasm/WasmValType.h"
#include "wasm/WasmValue.h"

namespace js {
namespace wasm {

using mozilla::Maybe;
using mozilla::Nothing;

class FuncType;
class TypeIdDesc;

// A Module can either be asm.js or wasm.

enum ModuleKind { Wasm, AsmJS };

// CacheableChars is used to cacheably store UniqueChars.

struct CacheableChars : UniqueChars {
  CacheableChars() = default;
  explicit CacheableChars(char* ptr) : UniqueChars(ptr) {}
  MOZ_IMPLICIT CacheableChars(UniqueChars&& rhs)
      : UniqueChars(std::move(rhs)) {}
  WASM_DECLARE_SERIALIZABLE(CacheableChars)
};

using CacheableCharsVector = Vector<CacheableChars, 0, SystemAllocPolicy>;

// Import describes a single wasm import. An ImportVector describes all
// of a single module's imports.
//
// ImportVector is built incrementally by ModuleGenerator and then stored
// immutably by Module.

struct Import {
  CacheableChars module;
  CacheableChars field;
  DefinitionKind kind;

  Import() = default;
  Import(UniqueChars&& module, UniqueChars&& field, DefinitionKind kind)
      : module(std::move(module)), field(std::move(field)), kind(kind) {}

  WASM_DECLARE_SERIALIZABLE(Import)
};

using ImportVector = Vector<Import, 0, SystemAllocPolicy>;

// Export describes the export of a definition in a Module to a field in the
// export object. The Export stores the index of the exported item in the
// appropriate type-specific module data structure (function table, global
// table, table table, and - eventually - memory table).
//
// Note a single definition can be exported by multiple Exports in the
// ExportVector.
//
// ExportVector is built incrementally by ModuleGenerator and then stored
// immutably by Module.

class Export {
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
#ifdef ENABLE_WASM_EXCEPTIONS
  uint32_t tagIndex() const;
#endif
  uint32_t globalIndex() const;
  uint32_t tableIndex() const;

  WASM_DECLARE_SERIALIZABLE(Export)
};

using ExportVector = Vector<Export, 0, SystemAllocPolicy>;

// FuncFlags provides metadata for a function definition.

enum class FuncFlags : uint8_t {
  None = 0x0,
  // The function maybe be accessible by JS and needs thunks generated for it.
  // See `[SMDOC] Exported wasm functions and the jit-entry stubs` in
  // WasmJS.cpp for more information.
  Exported = 0x1,
  // The function should have thunks generated upon instantiation, not upon
  // first call. May only be set if `Exported` is set.
  Eager = 0x2,
  // The function can be the target of a ref.func instruction in the code
  // section. May only be set if `Exported` is set.
  CanRefFunc = 0x4,
};

// A FuncDesc describes a single function definition.

struct FuncDesc {
  FuncType* type;
  TypeIdDesc* typeId;
  // Bit pack to keep this struct small on 32-bit systems
  uint32_t typeIndex : 24;
  FuncFlags flags : 8;

  // Assert that the bit packing scheme is viable
  static_assert(MaxTypes <= (1 << 24) - 1);
  static_assert(sizeof(FuncFlags) == sizeof(uint8_t));

  FuncDesc() = default;
  FuncDesc(FuncType* type, TypeIdDesc* typeId, uint32_t typeIndex)
      : type(type),
        typeId(typeId),
        typeIndex(typeIndex),
        flags(FuncFlags::None) {}

  bool isExported() const {
    return uint8_t(flags) & uint8_t(FuncFlags::Exported);
  }
  bool isEager() const { return uint8_t(flags) & uint8_t(FuncFlags::Eager); }
  bool canRefFunc() const {
    return uint8_t(flags) & uint8_t(FuncFlags::CanRefFunc);
  }
};

using FuncDescVector = Vector<FuncDesc, 0, SystemAllocPolicy>;

// A GlobalDesc describes a single global variable.
//
// wasm can import and export mutable and immutable globals.
//
// asm.js can import mutable and immutable globals, but a mutable global has a
// location that is private to the module, and its initial value is copied into
// that cell from the environment.  asm.js cannot export globals.

enum class GlobalKind { Import, Constant, Variable };

class GlobalDesc {
  GlobalKind kind_;
  // Stores the value type of this global for all kinds, and the initializer
  // expression when `constant` or `variable`.
  InitExpr initial_;
  // Metadata for the global when `variable` or `import`.
  unsigned offset_;
  bool isMutable_;
  bool isWasm_;
  bool isExport_;
  // Metadata for the global when `import`.
  uint32_t importIndex_;

  // Private, as they have unusual semantics.

  bool isExport() const { return !isConstant() && isExport_; }
  bool isWasm() const { return !isConstant() && isWasm_; }

 public:
  GlobalDesc() = default;

  explicit GlobalDesc(InitExpr&& initial, bool isMutable,
                      ModuleKind kind = ModuleKind::Wasm)
      : kind_((isMutable || !initial.isLiteral()) ? GlobalKind::Variable
                                                  : GlobalKind::Constant) {
    initial_ = std::move(initial);
    if (isVariable()) {
      isMutable_ = isMutable;
      isWasm_ = kind == Wasm;
      isExport_ = false;
      offset_ = UINT32_MAX;
    }
  }

  explicit GlobalDesc(ValType type, bool isMutable, uint32_t importIndex,
                      ModuleKind kind = ModuleKind::Wasm)
      : kind_(GlobalKind::Import) {
    initial_ = InitExpr(LitVal(type));
    importIndex_ = importIndex;
    isMutable_ = isMutable;
    isWasm_ = kind == Wasm;
    isExport_ = false;
    offset_ = UINT32_MAX;
  }

  void setOffset(unsigned offset) {
    MOZ_ASSERT(!isConstant());
    MOZ_ASSERT(offset_ == UINT32_MAX);
    offset_ = offset;
  }
  unsigned offset() const {
    MOZ_ASSERT(!isConstant());
    MOZ_ASSERT(offset_ != UINT32_MAX);
    return offset_;
  }

  void setIsExport() {
    if (!isConstant()) {
      isExport_ = true;
    }
  }

  GlobalKind kind() const { return kind_; }
  bool isVariable() const { return kind_ == GlobalKind::Variable; }
  bool isConstant() const { return kind_ == GlobalKind::Constant; }
  bool isImport() const { return kind_ == GlobalKind::Import; }

  bool isMutable() const { return !isConstant() && isMutable_; }
  const InitExpr& initExpr() const {
    MOZ_ASSERT(!isImport());
    return initial_;
  }
  uint32_t importIndex() const {
    MOZ_ASSERT(isImport());
    return importIndex_;
  }

  LitVal constantValue() const { return initial_.literal(); }

  // If isIndirect() is true then storage for the value is not in the
  // instance's global area, but in a WasmGlobalObject::Cell hanging off a
  // WasmGlobalObject; the global area contains a pointer to the Cell.
  //
  // We don't want to indirect unless we must, so only mutable, exposed
  // globals are indirected - in all other cases we copy values into and out
  // of their module.
  //
  // Note that isIndirect() isn't equivalent to getting a WasmGlobalObject:
  // an immutable exported global will still get an object, but will not be
  // indirect.
  bool isIndirect() const {
    return isMutable() && isWasm() && (isImport() || isExport());
  }

  ValType type() const { return initial_.type(); }

  WASM_DECLARE_SERIALIZABLE(GlobalDesc)
};

using GlobalDescVector = Vector<GlobalDesc, 0, SystemAllocPolicy>;

// A TagDesc represents fresh per-instance tags that are used for the
// exception handling proposal and potentially other future proposals.

// The TagOffsetVector represents the offsets in the layout of the
// data stored in a Wasm exception. For non-reference values, it is
// an offset in the ArrayBuffer and for reference values it is the
// offset in the elements of the exception's ArrayObject.
using TagOffsetVector = Vector<int32_t, 0, SystemAllocPolicy>;

// Not guarded by #ifdef like TagDesc as this is required for Wasm JS
// API classes in WasmJS.h.
struct TagType {
  ValTypeVector argTypes;
  TagOffsetVector argOffsets;
  int32_t bufferSize;
  int32_t refCount;

  TagType() : argTypes(), argOffsets(), bufferSize(0), refCount(0) {}
  TagType(ValTypeVector&& argTypes, TagOffsetVector&& argOffsets)
      : argTypes(std::move(argTypes)),
        argOffsets(std::move(argOffsets)),
        bufferSize(0),
        refCount(0) {}

  [[nodiscard]] bool computeLayout();

  [[nodiscard]] bool clone(const TagType& src) {
    MOZ_ASSERT(argTypes.empty());
    MOZ_ASSERT(argOffsets.empty());
    if (!argTypes.appendAll(src.argTypes) ||
        !argOffsets.appendAll(src.argOffsets)) {
      return false;
    }
    bufferSize = src.bufferSize;
    refCount = src.refCount;
    return true;
  }
};

#ifdef ENABLE_WASM_EXCEPTIONS
struct TagDesc {
  TagKind kind;
  TagType type;
  bool isExport;

  TagDesc(TagKind kind, TagType&& type, bool isExport = false)
      : kind(kind), type(std::move(type)), isExport(isExport) {}

  ResultType resultType() const { return ResultType::Vector(type.argTypes); }
};

using TagDescVector = Vector<TagDesc, 0, SystemAllocPolicy>;
#endif

// When a ElemSegment is "passive" it is shared between a wasm::Module and its
// wasm::Instances. To allow each segment to be released as soon as the last
// Instance elem.drops it and the Module is destroyed, each ElemSegment is
// individually atomically ref-counted.

struct ElemSegment : AtomicRefCounted<ElemSegment> {
  enum class Kind {
    Active,
    Passive,
    Declared,
  };

  Kind kind;
  uint32_t tableIndex;
  RefType elemType;
  Maybe<InitExpr> offsetIfActive;
  Uint32Vector elemFuncIndices;  // Element may be NullFuncIndex

  bool active() const { return kind == Kind::Active; }

  const InitExpr& offset() const { return *offsetIfActive; }

  size_t length() const { return elemFuncIndices.length(); }

  WASM_DECLARE_SERIALIZABLE(ElemSegment)
};

// NullFuncIndex represents the case when an element segment (of type funcref)
// contains a null element.
constexpr uint32_t NullFuncIndex = UINT32_MAX;
static_assert(NullFuncIndex > MaxFuncs, "Invariant");

using MutableElemSegment = RefPtr<ElemSegment>;
using SharedElemSegment = SerializableRefPtr<const ElemSegment>;
using ElemSegmentVector = Vector<SharedElemSegment, 0, SystemAllocPolicy>;

// DataSegmentEnv holds the initial results of decoding a data segment from the
// bytecode and is stored in the ModuleEnvironment during compilation. When
// compilation completes, (non-Env) DataSegments are created and stored in
// the wasm::Module which contain copies of the data segment payload. This
// allows non-compilation uses of wasm validation to avoid expensive copies.
//
// When a DataSegment is "passive" it is shared between a wasm::Module and its
// wasm::Instances. To allow each segment to be released as soon as the last
// Instance mem.drops it and the Module is destroyed, each DataSegment is
// individually atomically ref-counted.

struct DataSegmentEnv {
  Maybe<InitExpr> offsetIfActive;
  uint32_t bytecodeOffset;
  uint32_t length;
};

using DataSegmentEnvVector = Vector<DataSegmentEnv, 0, SystemAllocPolicy>;

struct DataSegment : AtomicRefCounted<DataSegment> {
  Maybe<InitExpr> offsetIfActive;
  Bytes bytes;

  DataSegment() = default;

  bool active() const { return !!offsetIfActive; }

  const InitExpr& offset() const { return *offsetIfActive; }

  [[nodiscard]] bool init(const ShareableBytes& bytecode,
                          const DataSegmentEnv& src) {
    if (src.offsetIfActive) {
      offsetIfActive.emplace();
      if (!offsetIfActive->clone(*src.offsetIfActive)) {
        return false;
      }
    }
    return bytes.append(bytecode.begin() + src.bytecodeOffset, src.length);
  }

  WASM_DECLARE_SERIALIZABLE(DataSegment)
};

using MutableDataSegment = RefPtr<DataSegment>;
using SharedDataSegment = SerializableRefPtr<const DataSegment>;
using DataSegmentVector = Vector<SharedDataSegment, 0, SystemAllocPolicy>;

// The CustomSection(Env) structs are like DataSegment(Env): CustomSectionEnv is
// stored in the ModuleEnvironment and CustomSection holds a copy of the payload
// and is stored in the wasm::Module.

struct CustomSectionEnv {
  uint32_t nameOffset;
  uint32_t nameLength;
  uint32_t payloadOffset;
  uint32_t payloadLength;
};

using CustomSectionEnvVector = Vector<CustomSectionEnv, 0, SystemAllocPolicy>;

struct CustomSection {
  Bytes name;
  SharedBytes payload;

  WASM_DECLARE_SERIALIZABLE(CustomSection)
};

using CustomSectionVector = Vector<CustomSection, 0, SystemAllocPolicy>;

// A Name represents a string of utf8 chars embedded within the name custom
// section. The offset of a name is expressed relative to the beginning of the
// name section's payload so that Names can stored in wasm::Code, which only
// holds the name section's bytes, not the whole bytecode.

struct Name {
  // All fields are treated as cacheable POD:
  uint32_t offsetInNamePayload;
  uint32_t length;

  Name() : offsetInNamePayload(UINT32_MAX), length(0) {}
};

using NameVector = Vector<Name, 0, SystemAllocPolicy>;

// The kind of limits to decode or convert from JS.

enum class LimitsKind {
  Memory,
  Table,
};

// Represents the resizable limits of memories and tables.

struct Limits {
  // `indexType` will always be I32 for tables, but may be I64 for memories
  // when memory64 is enabled.
  IndexType indexType;

  // The initial and maximum limit. The unit is pages for memories and elements
  // for tables.
  uint64_t initial;
  Maybe<uint64_t> maximum;

  // `shared` is Shareable::False for tables but may be Shareable::True for
  // memories.
  Shareable shared;

  Limits() = default;
  explicit Limits(uint64_t initial, const Maybe<uint64_t>& maximum = Nothing(),
                  Shareable shared = Shareable::False)
      : indexType(IndexType::I32),
        initial(initial),
        maximum(maximum),
        shared(shared) {}
};

// MemoryDesc describes a memory.

struct MemoryDesc {
  Limits limits;

  bool isShared() const { return limits.shared == Shareable::True; }

  // Whether a backing store for this memory may move when grown.
  bool canMovingGrow() const { return limits.maximum.isNothing(); }

  // Whether the bounds check limit (see the doc comment in
  // ArrayBufferObject.cpp regarding linear memory structure) can ever be
  // larger than 32-bits.
  bool boundsCheckLimitIs32Bits() const {
    return limits.maximum.isSome() &&
           limits.maximum.value() < (0x100000000 / PageSize);
  }

  IndexType indexType() const { return limits.indexType; }

  // The initial length of this memory in pages.
  Pages initialPages() const { return Pages(limits.initial); }

  // The maximum length of this memory in pages.
  Maybe<Pages> maximumPages() const {
    return limits.maximum.map([](uint64_t x) { return Pages(x); });
  }

  // The initial length of this memory in bytes. Only valid for memory32.
  uint64_t initialLength32() const {
    MOZ_ASSERT(indexType() == IndexType::I32);
    // See static_assert after MemoryDesc for why this is safe.
    return limits.initial * PageSize;
  }

  uint64_t initialLength64() const {
    MOZ_ASSERT(indexType() == IndexType::I64);
    return limits.initial * PageSize;
  }

  MemoryDesc() = default;
  explicit MemoryDesc(Limits limits) : limits(limits) {}
};

// We don't need to worry about overflow with a Memory32 field when
// using a uint64_t.
static_assert(MaxMemory32LimitField <= UINT64_MAX / PageSize);

// TableDesc describes a table as well as the offset of the table's base pointer
// in global memory.
//
// A TableDesc contains the element type and whether the table is for asm.js,
// which determines the table representation.
//  - ExternRef: a wasm anyref word (wasm::AnyRef)
//  - FuncRef: a two-word FunctionTableElem (wasm indirect call ABI)
//  - FuncRef (if `isAsmJS`): a two-word FunctionTableElem (asm.js ABI)
// Eventually there should be a single unified AnyRef representation.

struct TableDesc {
  RefType elemType;
  bool importedOrExported;
  bool isAsmJS;
  uint32_t globalDataOffset;
  uint32_t initialLength;
  Maybe<uint32_t> maximumLength;

  TableDesc() = default;
  TableDesc(RefType elemType, uint32_t initialLength,
            Maybe<uint32_t> maximumLength, bool isAsmJS,
            bool importedOrExported = false)
      : elemType(elemType),
        importedOrExported(importedOrExported),
        isAsmJS(isAsmJS),
        globalDataOffset(UINT32_MAX),
        initialLength(initialLength),
        maximumLength(maximumLength) {}
};

using TableDescVector = Vector<TableDesc, 0, SystemAllocPolicy>;

}  // namespace wasm
}  // namespace js

#endif  // wasm_module_types_h
