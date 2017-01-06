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

#ifndef wasm_types_h
#define wasm_types_h

#include "mozilla/EnumeratedArray.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/Maybe.h"
#include "mozilla/Move.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Unused.h"

#include "NamespaceImports.h"

#include "ds/LifoAlloc.h"
#include "jit/IonTypes.h"
#include "js/RefCounted.h"
#include "js/UniquePtr.h"
#include "js/Utility.h"
#include "js/Vector.h"
#include "vm/MallocProvider.h"
#include "wasm/WasmBinaryConstants.h"

namespace js {

class PropertyName;
namespace jit { struct BaselineScript; }

// This is a widespread header, so lets keep out the core wasm impl types.

class WasmMemoryObject;
typedef GCPtr<WasmMemoryObject*> GCPtrWasmMemoryObject;
typedef Rooted<WasmMemoryObject*> RootedWasmMemoryObject;
typedef Handle<WasmMemoryObject*> HandleWasmMemoryObject;
typedef MutableHandle<WasmMemoryObject*> MutableHandleWasmMemoryObject;

class WasmModuleObject;
typedef Rooted<WasmModuleObject*> RootedWasmModuleObject;
typedef Handle<WasmModuleObject*> HandleWasmModuleObject;
typedef MutableHandle<WasmModuleObject*> MutableHandleWasmModuleObject;

class WasmInstanceObject;
typedef GCVector<WasmInstanceObject*> WasmInstanceObjectVector;
typedef Rooted<WasmInstanceObject*> RootedWasmInstanceObject;
typedef Handle<WasmInstanceObject*> HandleWasmInstanceObject;
typedef MutableHandle<WasmInstanceObject*> MutableHandleWasmInstanceObject;

class WasmTableObject;
typedef Rooted<WasmTableObject*> RootedWasmTableObject;
typedef Handle<WasmTableObject*> HandleWasmTableObject;
typedef MutableHandle<WasmTableObject*> MutableHandleWasmTableObject;

namespace wasm {

using mozilla::DebugOnly;
using mozilla::EnumeratedArray;
using mozilla::Maybe;
using mozilla::Move;
using mozilla::MallocSizeOf;
using mozilla::Nothing;
using mozilla::PodZero;
using mozilla::PodCopy;
using mozilla::PodEqual;
using mozilla::Some;
using mozilla::Unused;

typedef Vector<uint32_t, 0, SystemAllocPolicy> Uint32Vector;
typedef Vector<uint8_t, 0, SystemAllocPolicy> Bytes;
typedef UniquePtr<Bytes> UniqueBytes;
typedef Vector<char, 0, SystemAllocPolicy> UTF8Bytes;

typedef int8_t I8x16[16];
typedef int16_t I16x8[8];
typedef int32_t I32x4[4];
typedef float F32x4[4];

class Code;
class CodeRange;
class Memory;
class Module;
class Instance;
class Table;

// To call Vector::podResizeToFit, a type must specialize mozilla::IsPod
// which is pretty verbose to do within js::wasm, so factor that process out
// into a macro.

#define WASM_DECLARE_POD_VECTOR(Type, VectorName)                               \
} } namespace mozilla {                                                         \
template <> struct IsPod<js::wasm::Type> : TrueType {};                         \
} namespace js { namespace wasm {                                               \
typedef Vector<Type, 0, SystemAllocPolicy> VectorName;

// A wasm Module and everything it contains must support serialization and
// deserialization. Some data can be simply copied as raw bytes and,
// as a convention, is stored in an inline CacheablePod struct. Everything else
// should implement the below methods which are called recusively by the
// containing Module.

#define WASM_DECLARE_SERIALIZABLE(Type)                                         \
    size_t serializedSize() const;                                              \
    uint8_t* serialize(uint8_t* cursor) const;                                  \
    const uint8_t* deserialize(const uint8_t* cursor);                          \
    size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

#define WASM_DECLARE_SERIALIZABLE_VIRTUAL(Type)                                 \
    virtual size_t serializedSize() const;                                      \
    virtual uint8_t* serialize(uint8_t* cursor) const;                          \
    virtual const uint8_t* deserialize(const uint8_t* cursor);                  \
    virtual size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

#define WASM_DECLARE_SERIALIZABLE_OVERRIDE(Type)                                \
    size_t serializedSize() const override;                                     \
    uint8_t* serialize(uint8_t* cursor) const override;                         \
    const uint8_t* deserialize(const uint8_t* cursor) override;                 \
    size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const override;

// This reusable base class factors out the logic for a resource that is shared
// by multiple instances/modules but should only be counted once when computing
// about:memory stats.

template <class T>
struct ShareableBase : RefCounted<T>
{
    using SeenSet = HashSet<const T*, DefaultHasher<const T*>, SystemAllocPolicy>;

    size_t sizeOfIncludingThisIfNotSeen(MallocSizeOf mallocSizeOf, SeenSet* seen) const {
        const T* self = static_cast<const T*>(this);
        typename SeenSet::AddPtr p = seen->lookupForAdd(self);
        if (p)
            return 0;
        bool ok = seen->add(p, self);
        (void)ok;  // oh well
        return mallocSizeOf(self) + self->sizeOfExcludingThis(mallocSizeOf);
    }
};

// ValType utilities

static inline bool
IsSimdType(ValType vt)
{
    switch (vt) {
      case ValType::I8x16:
      case ValType::I16x8:
      case ValType::I32x4:
      case ValType::F32x4:
      case ValType::B8x16:
      case ValType::B16x8:
      case ValType::B32x4:
        return true;
      default:
        return false;
    }
}

static inline uint32_t
NumSimdElements(ValType vt)
{
    MOZ_ASSERT(IsSimdType(vt));
    switch (vt) {
      case ValType::I8x16:
      case ValType::B8x16:
        return 16;
      case ValType::I16x8:
      case ValType::B16x8:
        return 8;
      case ValType::I32x4:
      case ValType::F32x4:
      case ValType::B32x4:
        return 4;
     default:
        MOZ_CRASH("Unhandled SIMD type");
    }
}

static inline ValType
SimdElementType(ValType vt)
{
    MOZ_ASSERT(IsSimdType(vt));
    switch (vt) {
      case ValType::I8x16:
      case ValType::I16x8:
      case ValType::I32x4:
        return ValType::I32;
      case ValType::F32x4:
        return ValType::F32;
      case ValType::B8x16:
      case ValType::B16x8:
      case ValType::B32x4:
        return ValType::I32;
     default:
        MOZ_CRASH("Unhandled SIMD type");
    }
}

static inline ValType
SimdBoolType(ValType vt)
{
    MOZ_ASSERT(IsSimdType(vt));
    switch (vt) {
      case ValType::I8x16:
      case ValType::B8x16:
        return ValType::B8x16;
      case ValType::I16x8:
      case ValType::B16x8:
        return ValType::B16x8;
      case ValType::I32x4:
      case ValType::F32x4:
      case ValType::B32x4:
        return ValType::B32x4;
     default:
        MOZ_CRASH("Unhandled SIMD type");
    }
}

static inline bool
IsSimdBoolType(ValType vt)
{
    return vt == ValType::B8x16 || vt == ValType::B16x8 || vt == ValType::B32x4;
}

static inline jit::MIRType
ToMIRType(ValType vt)
{
    switch (vt) {
      case ValType::I32: return jit::MIRType::Int32;
      case ValType::I64: return jit::MIRType::Int64;
      case ValType::F32: return jit::MIRType::Float32;
      case ValType::F64: return jit::MIRType::Double;
      case ValType::I8x16: return jit::MIRType::Int8x16;
      case ValType::I16x8: return jit::MIRType::Int16x8;
      case ValType::I32x4: return jit::MIRType::Int32x4;
      case ValType::F32x4: return jit::MIRType::Float32x4;
      case ValType::B8x16: return jit::MIRType::Bool8x16;
      case ValType::B16x8: return jit::MIRType::Bool16x8;
      case ValType::B32x4: return jit::MIRType::Bool32x4;
    }
    MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("bad type");
}

// The ExprType enum represents the type of a WebAssembly expression or return
// value and may either be a value type or void. Soon, expression types will be
// generalized to a list of ValType and this enum will go away, replaced,
// wherever it is used, by a varU32 + list of ValType.

enum class ExprType
{
    Void  = uint8_t(TypeCode::BlockVoid),

    I32   = uint8_t(TypeCode::I32),
    I64   = uint8_t(TypeCode::I64),
    F32   = uint8_t(TypeCode::F32),
    F64   = uint8_t(TypeCode::F64),

    I8x16 = uint8_t(TypeCode::I8x16),
    I16x8 = uint8_t(TypeCode::I16x8),
    I32x4 = uint8_t(TypeCode::I32x4),
    F32x4 = uint8_t(TypeCode::F32x4),
    B8x16 = uint8_t(TypeCode::B8x16),
    B16x8 = uint8_t(TypeCode::B16x8),
    B32x4 = uint8_t(TypeCode::B32x4),

    Limit = uint8_t(TypeCode::Limit)
};

static inline bool
IsVoid(ExprType et)
{
    return et == ExprType::Void;
}

static inline ValType
NonVoidToValType(ExprType et)
{
    MOZ_ASSERT(!IsVoid(et));
    return ValType(et);
}

static inline ExprType
ToExprType(ValType vt)
{
    return ExprType(vt);
}

static inline bool
IsSimdType(ExprType et)
{
    return IsVoid(et) ? false : IsSimdType(ValType(et));
}

static inline jit::MIRType
ToMIRType(ExprType et)
{
    return IsVoid(et) ? jit::MIRType::None : ToMIRType(ValType(et));
}

static inline const char*
ToCString(ExprType type)
{
    switch (type) {
      case ExprType::Void:  return "void";
      case ExprType::I32:   return "i32";
      case ExprType::I64:   return "i64";
      case ExprType::F32:   return "f32";
      case ExprType::F64:   return "f64";
      case ExprType::I8x16: return "i8x16";
      case ExprType::I16x8: return "i16x8";
      case ExprType::I32x4: return "i32x4";
      case ExprType::F32x4: return "f32x4";
      case ExprType::B8x16: return "b8x16";
      case ExprType::B16x8: return "b16x8";
      case ExprType::B32x4: return "b32x4";
      case ExprType::Limit:;
    }
    MOZ_CRASH("bad expression type");
}

static inline const char*
ToCString(ValType type)
{
    return ToCString(ToExprType(type));
}

// The Val class represents a single WebAssembly value of a given value type,
// mostly for the purpose of numeric literals and initializers. A Val does not
// directly map to a JS value since there is not (currently) a precise
// representation of i64 values. A Val may contain non-canonical NaNs since,
// within WebAssembly, floats are not canonicalized. Canonicalization must
// happen at the JS boundary.

class Val
{
    ValType type_;
    union U {
        uint32_t i32_;
        uint64_t i64_;
        float f32_;
        double f64_;
        I8x16 i8x16_;
        I16x8 i16x8_;
        I32x4 i32x4_;
        F32x4 f32x4_;
        U() {}
    } u;

  public:
    Val() = default;

    explicit Val(uint32_t i32) : type_(ValType::I32) { u.i32_ = i32; }
    explicit Val(uint64_t i64) : type_(ValType::I64) { u.i64_ = i64; }

    explicit Val(float f32) : type_(ValType::F32) { u.f32_ = f32; }
    explicit Val(double f64) : type_(ValType::F64) { u.f64_ = f64; }

    explicit Val(const I8x16& i8x16, ValType type = ValType::I8x16) : type_(type) {
        MOZ_ASSERT(type_ == ValType::I8x16 || type_ == ValType::B8x16);
        memcpy(u.i8x16_, i8x16, sizeof(u.i8x16_));
    }
    explicit Val(const I16x8& i16x8, ValType type = ValType::I16x8) : type_(type) {
        MOZ_ASSERT(type_ == ValType::I16x8 || type_ == ValType::B16x8);
        memcpy(u.i16x8_, i16x8, sizeof(u.i16x8_));
    }
    explicit Val(const I32x4& i32x4, ValType type = ValType::I32x4) : type_(type) {
        MOZ_ASSERT(type_ == ValType::I32x4 || type_ == ValType::B32x4);
        memcpy(u.i32x4_, i32x4, sizeof(u.i32x4_));
    }
    explicit Val(const F32x4& f32x4) : type_(ValType::F32x4) {
        memcpy(u.f32x4_, f32x4, sizeof(u.f32x4_));
    }

    ValType type() const { return type_; }
    bool isSimd() const { return IsSimdType(type()); }

    uint32_t i32() const { MOZ_ASSERT(type_ == ValType::I32); return u.i32_; }
    uint64_t i64() const { MOZ_ASSERT(type_ == ValType::I64); return u.i64_; }
    const float& f32() const { MOZ_ASSERT(type_ == ValType::F32); return u.f32_; }
    const double& f64() const { MOZ_ASSERT(type_ == ValType::F64); return u.f64_; }

    const I8x16& i8x16() const {
        MOZ_ASSERT(type_ == ValType::I8x16 || type_ == ValType::B8x16);
        return u.i8x16_;
    }
    const I16x8& i16x8() const {
        MOZ_ASSERT(type_ == ValType::I16x8 || type_ == ValType::B16x8);
        return u.i16x8_;
    }
    const I32x4& i32x4() const {
        MOZ_ASSERT(type_ == ValType::I32x4 || type_ == ValType::B32x4);
        return u.i32x4_;
    }
    const F32x4& f32x4() const {
        MOZ_ASSERT(type_ == ValType::F32x4);
        return u.f32x4_;
    }

    void writePayload(uint8_t* dst) const;
};

typedef Vector<Val, 0, SystemAllocPolicy> ValVector;

// The Sig class represents a WebAssembly function signature which takes a list
// of value types and returns an expression type. The engine uses two in-memory
// representations of the argument Vector's memory (when elements do not fit
// inline): normal malloc allocation (via SystemAllocPolicy) and allocation in
// a LifoAlloc (via LifoAllocPolicy). The former Sig objects can have any
// lifetime since they own the memory. The latter Sig objects must not outlive
// the associated LifoAlloc mark/release interval (which is currently the
// duration of module validation+compilation). Thus, long-lived objects like
// WasmModule must use malloced allocation.

class Sig
{
    ValTypeVector args_;
    ExprType ret_;

  public:
    Sig() : args_(), ret_(ExprType::Void) {}
    Sig(ValTypeVector&& args, ExprType ret) : args_(Move(args)), ret_(ret) {}

    MOZ_MUST_USE bool clone(const Sig& rhs) {
        ret_ = rhs.ret_;
        MOZ_ASSERT(args_.empty());
        return args_.appendAll(rhs.args_);
    }

    ValType arg(unsigned i) const { return args_[i]; }
    const ValTypeVector& args() const { return args_; }
    const ExprType& ret() const { return ret_; }

    HashNumber hash() const {
        return AddContainerToHash(args_, HashNumber(ret_));
    }
    bool operator==(const Sig& rhs) const {
        return ret() == rhs.ret() && EqualContainers(args(), rhs.args());
    }
    bool operator!=(const Sig& rhs) const {
        return !(*this == rhs);
    }

    WASM_DECLARE_SERIALIZABLE(Sig)
};

struct SigHashPolicy
{
    typedef const Sig& Lookup;
    static HashNumber hash(Lookup sig) { return sig.hash(); }
    static bool match(const Sig* lhs, Lookup rhs) { return *lhs == rhs; }
};

// An InitExpr describes a deferred initializer expression, used to initialize
// a global or a table element offset. Such expressions are created during
// decoding and actually executed on module instantiation.

class InitExpr
{
  public:
    enum class Kind {
        Constant,
        GetGlobal
    };

  private:
    Kind kind_;
    union U {
        Val val_;
        struct {
            uint32_t index_;
            ValType type_;
        } global;
        U() {}
    } u;

  public:
    InitExpr() = default;

    explicit InitExpr(Val val) : kind_(Kind::Constant) {
        u.val_ = val;
    }

    explicit InitExpr(uint32_t globalIndex, ValType type) : kind_(Kind::GetGlobal) {
        u.global.index_ = globalIndex;
        u.global.type_ = type;
    }

    Kind kind() const { return kind_; }

    bool isVal() const { return kind() == Kind::Constant; }
    Val val() const { MOZ_ASSERT(isVal()); return u.val_; }

    uint32_t globalIndex() const { MOZ_ASSERT(kind() == Kind::GetGlobal); return u.global.index_; }

    ValType type() const {
        switch (kind()) {
          case Kind::Constant: return u.val_.type();
          case Kind::GetGlobal: return u.global.type_;
        }
        MOZ_CRASH("unexpected initExpr type");
    }
};

// CacheableChars is used to cacheably store UniqueChars.

struct CacheableChars : UniqueChars
{
    CacheableChars() = default;
    explicit CacheableChars(char* ptr) : UniqueChars(ptr) {}
    MOZ_IMPLICIT CacheableChars(UniqueChars&& rhs) : UniqueChars(Move(rhs)) {}
    WASM_DECLARE_SERIALIZABLE(CacheableChars)
};

typedef Vector<CacheableChars, 0, SystemAllocPolicy> CacheableCharsVector;

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
// FuncExportVector in Metadata. For memory and table exports, there is
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

// A GlobalDesc describes a single global variable. Currently, asm.js and wasm
// exposes mutable and immutable private globals, but can't import nor export
// mutable globals.

enum class GlobalKind
{
    Import,
    Constant,
    Variable
};

class GlobalDesc
{
    union V {
        struct {
            union U {
                InitExpr initial_;
                struct {
                    ValType type_;
                    uint32_t index_;
                } import;
                U() {}
            } val;
            unsigned offset_;
            bool isMutable_;
        } var;
        Val cst_;
        V() {}
    } u;
    GlobalKind kind_;

  public:
    GlobalDesc() = default;

    explicit GlobalDesc(InitExpr initial, bool isMutable)
      : kind_((isMutable || !initial.isVal()) ? GlobalKind::Variable : GlobalKind::Constant)
    {
        if (isVariable()) {
            u.var.val.initial_ = initial;
            u.var.isMutable_ = isMutable;
            u.var.offset_ = UINT32_MAX;
        } else {
            u.cst_ = initial.val();
        }
    }

    explicit GlobalDesc(ValType type, bool isMutable, uint32_t importIndex)
      : kind_(GlobalKind::Import)
    {
        u.var.val.import.type_ = type;
        u.var.val.import.index_ = importIndex;
        u.var.isMutable_ = isMutable;
        u.var.offset_ = UINT32_MAX;
    }

    void setOffset(unsigned offset) {
        MOZ_ASSERT(!isConstant());
        MOZ_ASSERT(u.var.offset_ == UINT32_MAX);
        u.var.offset_ = offset;
    }
    unsigned offset() const {
        MOZ_ASSERT(!isConstant());
        MOZ_ASSERT(u.var.offset_ != UINT32_MAX);
        return u.var.offset_;
    }

    GlobalKind kind() const { return kind_; }
    bool isVariable() const { return kind_ == GlobalKind::Variable; }
    bool isConstant() const { return kind_ == GlobalKind::Constant; }
    bool isImport() const { return kind_ == GlobalKind::Import; }

    bool isMutable() const { return !isConstant() && u.var.isMutable_; }
    Val constantValue() const { MOZ_ASSERT(isConstant()); return u.cst_; }
    const InitExpr& initExpr() const { MOZ_ASSERT(isVariable()); return u.var.val.initial_; }
    uint32_t importIndex() const { MOZ_ASSERT(isImport()); return u.var.val.import.index_; }

    ValType type() const {
        switch (kind_) {
          case GlobalKind::Import:   return u.var.val.import.type_;
          case GlobalKind::Variable: return u.var.val.initial_.type();
          case GlobalKind::Constant: return u.cst_.type();
        }
        MOZ_CRASH("unexpected global kind");
    }
};

typedef Vector<GlobalDesc, 0, SystemAllocPolicy> GlobalDescVector;

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

// DataSegment describes the offset of a data segment in the bytecode that is
// to be copied at a given offset into linear memory upon instantiation.

struct DataSegment
{
    InitExpr offset;
    uint32_t bytecodeOffset;
    uint32_t length;
};

typedef Vector<DataSegment, 0, SystemAllocPolicy> DataSegmentVector;

// SigIdDesc describes a signature id that can be used by call_indirect and
// table-entry prologues to structurally compare whether the caller and callee's
// signatures *structurally* match. To handle the general case, a Sig is
// allocated and stored in a process-wide hash table, so that pointer equality
// implies structural equality. As an optimization for the 99% case where the
// Sig has a small number of parameters, the Sig is bit-packed into a uint32
// immediate value so that integer equality implies structural equality. Both
// cases can be handled with a single comparison by always setting the LSB for
// the immediates (the LSB is necessarily 0 for allocated Sig pointers due to
// alignment).

class SigIdDesc
{
  public:
    enum class Kind { None, Immediate, Global };
    static const uintptr_t ImmediateBit = 0x1;

  private:
    Kind kind_;
    size_t bits_;

    SigIdDesc(Kind kind, size_t bits) : kind_(kind), bits_(bits) {}

  public:
    Kind kind() const { return kind_; }
    static bool isGlobal(const Sig& sig);

    SigIdDesc() : kind_(Kind::None), bits_(0) {}
    static SigIdDesc global(const Sig& sig, uint32_t globalDataOffset);
    static SigIdDesc immediate(const Sig& sig);

    bool isGlobal() const { return kind_ == Kind::Global; }

    size_t immediate() const { MOZ_ASSERT(kind_ == Kind::Immediate); return bits_; }
    uint32_t globalDataOffset() const { MOZ_ASSERT(kind_ == Kind::Global); return bits_; }
};

// SigWithId pairs a Sig with SigIdDesc, describing either how to compile code
// that compares this signature's id or, at instantiation what signature ids to
// allocate in the global hash and where to put them.

struct SigWithId : Sig
{
    SigIdDesc id;

    SigWithId() = default;
    explicit SigWithId(Sig&& sig, SigIdDesc id) : Sig(Move(sig)), id(id) {}
    void operator=(Sig&& rhs) { Sig::operator=(Move(rhs)); }

    WASM_DECLARE_SERIALIZABLE(SigWithId)
};

typedef Vector<SigWithId, 0, SystemAllocPolicy> SigWithIdVector;
typedef Vector<const SigWithId*, 0, SystemAllocPolicy> SigWithIdPtrVector;

// The (,Profiling,Func)Offsets classes are used to record the offsets of
// different key points in a CodeRange during compilation.

struct Offsets
{
    explicit Offsets(uint32_t begin = 0, uint32_t end = 0)
      : begin(begin), end(end)
    {}

    // These define a [begin, end) contiguous range of instructions compiled
    // into a CodeRange.
    uint32_t begin;
    uint32_t end;

    void offsetBy(uint32_t offset) {
        begin += offset;
        end += offset;
    }
};

struct ProfilingOffsets : Offsets
{
    MOZ_IMPLICIT ProfilingOffsets(uint32_t profilingReturn = 0)
      : Offsets(), profilingReturn(profilingReturn)
    {}

    // For CodeRanges with ProfilingOffsets, 'begin' is the offset of the
    // profiling entry.
    uint32_t profilingEntry() const { return begin; }

    // The profiling return is the offset of the return instruction, which
    // precedes the 'end' by a variable number of instructions due to
    // out-of-line codegen.
    uint32_t profilingReturn;

    void offsetBy(uint32_t offset) {
        Offsets::offsetBy(offset);
        profilingReturn += offset;
    }
};

struct FuncOffsets : ProfilingOffsets
{
    MOZ_IMPLICIT FuncOffsets()
      : ProfilingOffsets(),
        tableEntry(0),
        tableProfilingJump(0),
        nonProfilingEntry(0),
        profilingJump(0),
        profilingEpilogue(0)
    {}

    // Function CodeRanges have a table entry which takes an extra signature
    // argument which is checked against the callee's signature before falling
    // through to the normal prologue. When profiling is enabled, a nop on the
    // fallthrough is patched to instead jump to the profiling epilogue.
    uint32_t tableEntry;
    uint32_t tableProfilingJump;

    // Function CodeRanges have an additional non-profiling entry that comes
    // after the profiling entry and a non-profiling epilogue that comes before
    // the profiling epilogue.
    uint32_t nonProfilingEntry;

    // When profiling is enabled, the 'nop' at offset 'profilingJump' is
    // overwritten to be a jump to 'profilingEpilogue'.
    uint32_t profilingJump;
    uint32_t profilingEpilogue;

    void offsetBy(uint32_t offset) {
        ProfilingOffsets::offsetBy(offset);
        tableEntry += offset;
        tableProfilingJump += offset;
        nonProfilingEntry += offset;
        profilingJump += offset;
        profilingEpilogue += offset;
    }
};

// A wasm::Trap represents a wasm-defined trap that can occur during execution
// which triggers a WebAssembly.RuntimeError. Generated code may jump to a Trap
// symbolically, passing the bytecode offset to report as the trap offset. The
// generated jump will be bound to a tiny stub which fills the offset and
// then jumps to a per-Trap shared stub at the end of the module.

enum class Trap
{
    // The Unreachable opcode has been executed.
    Unreachable,
    // An integer arithmetic operation led to an overflow.
    IntegerOverflow,
    // Trying to coerce NaN to an integer.
    InvalidConversionToInteger,
    // Integer division by zero.
    IntegerDivideByZero,
    // Out of bounds on wasm memory accesses and asm.js SIMD/atomic accesses.
    OutOfBounds,
    // call_indirect to null.
    IndirectCallToNull,
    // call_indirect signature mismatch.
    IndirectCallBadSig,

    // (asm.js only) SIMD float to int conversion failed because the input
    // wasn't in bounds.
    ImpreciseSimdConversion,

    // The internal stack space was exhausted. For compatibility, this throws
    // the same over-recursed error as JS.
    StackOverflow,

    Limit
};

// A wrapper around the bytecode offset of a wasm instruction within a whole
// module. Trap offsets should refer to the first byte of the instruction that
// triggered the trap and should ultimately derive from OpIter::trapOffset.

struct TrapOffset
{
    uint32_t bytecodeOffset;

    TrapOffset() = default;
    explicit TrapOffset(uint32_t bytecodeOffset) : bytecodeOffset(bytecodeOffset) {}
};

// While the frame-pointer chain allows the stack to be unwound without
// metadata, Error.stack still needs to know the line/column of every call in
// the chain. A CallSiteDesc describes a single callsite to which CallSite adds
// the metadata necessary to walk up to the next frame. Lastly CallSiteAndTarget
// adds the function index of the callee.

class CallSiteDesc
{
    uint32_t lineOrBytecode_ : 30;
    uint32_t kind_ : 2;
  public:
    enum Kind {
        Func,      // pc-relative call to a specific function
        Dynamic,   // dynamic callee called via register
        Symbolic,  // call to a single symbolic callee
        TrapExit   // call to a trap exit
    };
    CallSiteDesc() {}
    explicit CallSiteDesc(Kind kind)
      : lineOrBytecode_(0), kind_(kind)
    {
        MOZ_ASSERT(kind == Kind(kind_));
    }
    CallSiteDesc(uint32_t lineOrBytecode, Kind kind)
      : lineOrBytecode_(lineOrBytecode), kind_(kind)
    {
        MOZ_ASSERT(kind == Kind(kind_));
        MOZ_ASSERT(lineOrBytecode == lineOrBytecode_);
    }
    uint32_t lineOrBytecode() const { return lineOrBytecode_; }
    Kind kind() const { return Kind(kind_); }
};

class CallSite : public CallSiteDesc
{
    uint32_t returnAddressOffset_;
    uint32_t stackDepth_;

  public:
    CallSite() {}

    CallSite(CallSiteDesc desc, uint32_t returnAddressOffset, uint32_t stackDepth)
      : CallSiteDesc(desc),
        returnAddressOffset_(returnAddressOffset),
        stackDepth_(stackDepth)
    { }

    void setReturnAddressOffset(uint32_t r) { returnAddressOffset_ = r; }
    void offsetReturnAddressBy(int32_t o) { returnAddressOffset_ += o; }
    uint32_t returnAddressOffset() const { return returnAddressOffset_; }

    // The stackDepth measures the amount of stack space pushed since the
    // function was called. In particular, this includes the pushed return
    // address on all archs (whether or not the call instruction pushes the
    // return address (x86/x64) or the prologue does (ARM/MIPS)).
    uint32_t stackDepth() const { return stackDepth_; }
};

WASM_DECLARE_POD_VECTOR(CallSite, CallSiteVector)

class CallSiteAndTarget : public CallSite
{
    uint32_t index_;

  public:
    explicit CallSiteAndTarget(CallSite cs)
      : CallSite(cs)
    {
        MOZ_ASSERT(cs.kind() != Func);
    }
    CallSiteAndTarget(CallSite cs, uint32_t funcIndex)
      : CallSite(cs), index_(funcIndex)
    {
        MOZ_ASSERT(cs.kind() == Func);
    }
    CallSiteAndTarget(CallSite cs, Trap trap)
      : CallSite(cs),
        index_(uint32_t(trap))
    {
        MOZ_ASSERT(cs.kind() == TrapExit);
    }

    uint32_t funcIndex() const { MOZ_ASSERT(kind() == Func); return index_; }
    Trap trap() const { MOZ_ASSERT(kind() == TrapExit); return Trap(index_); }
};

typedef Vector<CallSiteAndTarget, 0, SystemAllocPolicy> CallSiteAndTargetVector;

// A wasm::SymbolicAddress represents a pointer to a well-known function or
// object that is embedded in wasm code. Since wasm code is serialized and
// later deserialized into a different address space, symbolic addresses must be
// used for *all* pointers into the address space. The MacroAssembler records a
// list of all SymbolicAddresses and the offsets of their use in the code for
// later patching during static linking.

enum class SymbolicAddress
{
    ToInt32,
#if defined(JS_CODEGEN_ARM)
    aeabi_idivmod,
    aeabi_uidivmod,
    AtomicCmpXchg,
    AtomicXchg,
    AtomicFetchAdd,
    AtomicFetchSub,
    AtomicFetchAnd,
    AtomicFetchOr,
    AtomicFetchXor,
#endif
    ModD,
    SinD,
    CosD,
    TanD,
    ASinD,
    ACosD,
    ATanD,
    CeilD,
    CeilF,
    FloorD,
    FloorF,
    TruncD,
    TruncF,
    NearbyIntD,
    NearbyIntF,
    ExpD,
    LogD,
    PowD,
    ATan2D,
    Context,
    InterruptUint32,
    ReportOverRecursed,
    HandleExecutionInterrupt,
    ReportTrap,
    ReportOutOfBounds,
    ReportUnalignedAccess,
    CallImport_Void,
    CallImport_I32,
    CallImport_I64,
    CallImport_F64,
    CoerceInPlace_ToInt32,
    CoerceInPlace_ToNumber,
    DivI64,
    UDivI64,
    ModI64,
    UModI64,
    TruncateDoubleToInt64,
    TruncateDoubleToUint64,
    Uint64ToFloatingPoint,
    Int64ToFloatingPoint,
    GrowMemory,
    CurrentMemory,
    Limit
};

void*
AddressOf(SymbolicAddress imm, ExclusiveContext* cx);

// Assumptions captures ambient state that must be the same when compiling and
// deserializing a module for the compiled code to be valid. If it's not, then
// the module must be recompiled from scratch.

struct Assumptions
{
    uint32_t              cpuId;
    JS::BuildIdCharVector buildId;

    explicit Assumptions(JS::BuildIdCharVector&& buildId);

    // If Assumptions is constructed without arguments, initBuildIdFromContext()
    // must be called to complete initialization.
    Assumptions();
    bool initBuildIdFromContext(ExclusiveContext* cx);

    bool clone(const Assumptions& other);

    bool operator==(const Assumptions& rhs) const;
    bool operator!=(const Assumptions& rhs) const { return !(*this == rhs); }

    size_t serializedSize() const;
    uint8_t* serialize(uint8_t* cursor) const;
    const uint8_t* deserialize(const uint8_t* cursor, size_t remain);
    size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
};

// A Module can either be asm.js or wasm.

enum ModuleKind
{
    Wasm,
    AsmJS
};

// Represents the resizable limits of memories and tables.

struct Limits
{
    uint32_t initial;
    Maybe<uint32_t> maximum;
};

// TableDesc describes a table as well as the offset of the table's base pointer
// in global memory. Currently, wasm only has "any function" and asm.js only
// "typed function".

enum class TableKind
{
    AnyFunction,
    TypedFunction
};

struct TableDesc
{
    TableKind kind;
    bool external;
    uint32_t globalDataOffset;
    Limits limits;

    TableDesc() = default;
    TableDesc(TableKind kind, Limits limits)
     : kind(kind),
       external(false),
       globalDataOffset(UINT32_MAX),
       limits(limits)
    {}
};

typedef Vector<TableDesc, 0, SystemAllocPolicy> TableDescVector;

// ExportArg holds the unboxed operands to the wasm entry trampoline which can
// be called through an ExportFuncPtr.

struct ExportArg
{
    uint64_t lo;
    uint64_t hi;
};

// TLS data for a single module instance.
//
// Every WebAssembly function expects to be passed a hidden TLS pointer argument
// in WasmTlsReg. The TLS pointer argument points to a TlsData struct.
// Compiled functions expect that the TLS pointer does not change for the
// lifetime of the thread.
//
// There is a TlsData per module instance per thread, so inter-module calls need
// to pass the TLS pointer appropriate for the callee module.
//
// After the TlsData struct follows the module's declared TLS variables.

struct TlsData
{
    // Pointer to the JSContext that contains this TLS data.
    JSContext* cx;

    // Pointer to the Instance that contains this TLS data.
    Instance* instance;

    // Pointer to the global data for this Instance.
    uint8_t* globalData;

    // Pointer to the base of the default memory (or null if there is none).
    uint8_t* memoryBase;

    // Stack limit for the current thread. This limit is checked against the
    // stack pointer in the prologue of functions that allocate stack space. See
    // `CodeGenerator::generateWasm`.
    void* stackLimit;
};

typedef int32_t (*ExportFuncPtr)(ExportArg* args, TlsData* tls);

// FuncImportTls describes the region of wasm global memory allocated in the
// instance's thread-local storage for a function import. This is accessed
// directly from JIT code and mutated by Instance as exits become optimized and
// deoptimized.

struct FuncImportTls
{
    // The code to call at an import site: a wasm callee, a thunk into C++, or a
    // thunk into JIT code.
    void* code;

    // The callee's TlsData pointer, which must be loaded to WasmTlsReg (along
    // with any pinned registers) before calling 'code'.
    TlsData* tls;

    // If 'code' points into a JIT code thunk, the BaselineScript of the callee,
    // for bidirectional registration purposes.
    jit::BaselineScript* baselineScript;

    // A GC pointer which keeps the callee alive. For imported wasm functions,
    // this points to the wasm function's WasmInstanceObject. For all other
    // imported functions, 'obj' points to the JSFunction.
    GCPtrObject obj;
    static_assert(sizeof(GCPtrObject) == sizeof(void*), "for JIT access");
};

// TableTls describes the region of wasm global memory allocated in the
// instance's thread-local storage which is accessed directly from JIT code
// to bounds-check and index the table.

struct TableTls
{
    // Length of the table in number of elements (not bytes).
    uint32_t length;

    // Pointer to the array of elements (of type either ExternalTableElem or
    // void*).
    void* base;
};

// When a table can contain functions from other instances (it is "external"),
// the internal representation is an array of ExternalTableElem instead of just
// an array of code pointers.

struct ExternalTableElem
{
    // The code to call when calling this element. The table ABI is the system
    // ABI with the additional ABI requirements that:
    //  - WasmTlsReg and any pinned registers have been loaded appropriately
    //  - if this is a heterogeneous table that requires a signature check,
    //    WasmTableCallSigReg holds the signature id.
    void* code;

    // The pointer to the callee's instance's TlsData. This must be loaded into
    // WasmTlsReg before calling 'code'.
    TlsData* tls;
};

// CalleeDesc describes how to compile one of the variety of asm.js/wasm calls.
// This is hoisted into WasmTypes.h for sharing between Ion and Baseline.

class CalleeDesc
{
  public:
    enum Which {
        // Calls a function defined in the same module by its index.
        Func,

        // Calls the import identified by the offset of its FuncImportTls in
        // thread-local data.
        Import,

        // Calls a WebAssembly table (heterogeneous, index must be bounds
        // checked, callee instance depends on TableDesc).
        WasmTable,

        // Calls an asm.js table (homogeneous, masked index, same-instance).
        AsmJSTable,

        // Call a C++ function identified by SymbolicAddress.
        Builtin,

        // Like Builtin, but automatically passes Instance* as first argument.
        BuiltinInstanceMethod
    };

  private:
    Which which_;
    union U {
        U() {}
        uint32_t funcIndex_;
        struct {
            uint32_t globalDataOffset_;
        } import;
        struct {
            uint32_t globalDataOffset_;
            bool external_;
            SigIdDesc sigId_;
        } table;
        SymbolicAddress builtin_;
    } u;

  public:
    CalleeDesc() {}
    static CalleeDesc function(uint32_t funcIndex) {
        CalleeDesc c;
        c.which_ = Func;
        c.u.funcIndex_ = funcIndex;
        return c;
    }
    static CalleeDesc import(uint32_t globalDataOffset) {
        CalleeDesc c;
        c.which_ = Import;
        c.u.import.globalDataOffset_ = globalDataOffset;
        return c;
    }
    static CalleeDesc wasmTable(const TableDesc& desc, SigIdDesc sigId) {
        CalleeDesc c;
        c.which_ = WasmTable;
        c.u.table.globalDataOffset_ = desc.globalDataOffset;
        c.u.table.external_ = desc.external;
        c.u.table.sigId_ = sigId;
        return c;
    }
    static CalleeDesc asmJSTable(const TableDesc& desc) {
        CalleeDesc c;
        c.which_ = AsmJSTable;
        c.u.table.globalDataOffset_ = desc.globalDataOffset;
        return c;
    }
    static CalleeDesc builtin(SymbolicAddress callee) {
        CalleeDesc c;
        c.which_ = Builtin;
        c.u.builtin_ = callee;
        return c;
    }
    static CalleeDesc builtinInstanceMethod(SymbolicAddress callee) {
        CalleeDesc c;
        c.which_ = BuiltinInstanceMethod;
        c.u.builtin_ = callee;
        return c;
    }
    Which which() const {
        return which_;
    }
    uint32_t funcIndex() const {
        MOZ_ASSERT(which_ == Func);
        return u.funcIndex_;
    }
    uint32_t importGlobalDataOffset() const {
        MOZ_ASSERT(which_ == Import);
        return u.import.globalDataOffset_;
    }
    bool isTable() const {
        return which_ == WasmTable || which_ == AsmJSTable;
    }
    uint32_t tableLengthGlobalDataOffset() const {
        MOZ_ASSERT(isTable());
        return u.table.globalDataOffset_ + offsetof(TableTls, length);
    }
    uint32_t tableBaseGlobalDataOffset() const {
        MOZ_ASSERT(isTable());
        return u.table.globalDataOffset_ + offsetof(TableTls, base);
    }
    bool wasmTableIsExternal() const {
        MOZ_ASSERT(which_ == WasmTable);
        return u.table.external_;
    }
    SigIdDesc wasmTableSigId() const {
        MOZ_ASSERT(which_ == WasmTable);
        return u.table.sigId_;
    }
    SymbolicAddress builtin() const {
        MOZ_ASSERT(which_ == Builtin || which_ == BuiltinInstanceMethod);
        return u.builtin_;
    }
};

// Because ARM has a fixed-width instruction encoding, ARM can only express a
// limited subset of immediates (in a single instruction).

extern bool
IsValidARMImmediate(uint32_t i);

extern uint32_t
RoundUpToNextValidARMImmediate(uint32_t i);

// The WebAssembly spec hard-codes the virtual page size to be 64KiB and
// requires the size of linear memory to always be a multiple of 64KiB.

static const unsigned PageSize = 64 * 1024;

// Bounds checks always compare the base of the memory access with the bounds
// check limit. If the memory access is unaligned, this means that, even if the
// bounds check succeeds, a few bytes of the access can extend past the end of
// memory. To guard against this, extra space is included in the guard region to
// catch the overflow. MaxMemoryAccessSize is a conservative approximation of
// the maximum guard space needed to catch all unaligned overflows.

static const unsigned MaxMemoryAccessSize = sizeof(Val);

#ifdef JS_CODEGEN_X64

// All other code should use WASM_HUGE_MEMORY instead of JS_CODEGEN_X64 so that
// it is easy to use the huge-mapping optimization for other 64-bit platforms in
// the future.
# define WASM_HUGE_MEMORY

// On WASM_HUGE_MEMORY platforms, every asm.js or WebAssembly memory
// unconditionally allocates a huge region of virtual memory of size
// wasm::HugeMappedSize. This allows all memory resizing to work without
// reallocation and provides enough guard space for all offsets to be folded
// into memory accesses.

static const uint64_t IndexRange = uint64_t(UINT32_MAX) + 1;
static const uint64_t OffsetGuardLimit = uint64_t(INT32_MAX) + 1;
static const uint64_t UnalignedGuardPage = PageSize;
static const uint64_t HugeMappedSize = IndexRange + OffsetGuardLimit + UnalignedGuardPage;

static_assert(MaxMemoryAccessSize <= UnalignedGuardPage, "rounded up to static page size");

#else // !WASM_HUGE_MEMORY

// On !WASM_HUGE_MEMORY platforms:
//  - To avoid OOM in ArrayBuffer::prepareForAsmJS, asm.js continues to use the
//    original ArrayBuffer allocation which has no guard region at all.
//  - For WebAssembly memories, an additional GuardSize is mapped after the
//    accessible region of the memory to catch folded (base+offset) accesses
//    where `offset < OffsetGuardLimit` as well as the overflow from unaligned
//    accesses, as described above for MaxMemoryAccessSize.

static const size_t OffsetGuardLimit = PageSize - MaxMemoryAccessSize;
static const size_t GuardSize = PageSize;

// Return whether the given immediate satisfies the constraints of the platform
// (viz. that, on ARM, IsValidARMImmediate).

extern bool
IsValidBoundsCheckImmediate(uint32_t i);

// For a given WebAssembly/asm.js max size, return the number of bytes to
// map which will necessarily be a multiple of the system page size and greater
// than maxSize. For a returned mappedSize:
//   boundsCheckLimit = mappedSize - GuardSize
//   IsValidBoundsCheckImmediate(boundsCheckLimit)

extern size_t
ComputeMappedSize(uint32_t maxSize);

#endif // WASM_HUGE_MEMORY

// Metadata for bounds check instructions that are patched at runtime with the
// appropriate bounds check limit. On WASM_HUGE_MEMORY platforms for wasm (and
// SIMD/Atomic) bounds checks, no BoundsCheck is created: the signal handler
// catches everything. On !WASM_HUGE_MEMORY, a BoundsCheck is created for each
// memory access (except when statically eliminated by optimizations) so that
// the length can be patched in as an immediate. This requires that the bounds
// check limit IsValidBoundsCheckImmediate.

class BoundsCheck
{
  public:
    BoundsCheck() = default;

    explicit BoundsCheck(uint32_t cmpOffset)
      : cmpOffset_(cmpOffset)
    { }

    uint8_t* patchAt(uint8_t* code) const { return code + cmpOffset_; }
    void offsetBy(uint32_t offset) { cmpOffset_ += offset; }

  private:
    uint32_t cmpOffset_;
};

WASM_DECLARE_POD_VECTOR(BoundsCheck, BoundsCheckVector)

// Metadata for memory accesses. On WASM_HUGE_MEMORY platforms, only
// (non-SIMD/Atomic) asm.js loads and stores create a MemoryAccess so that the
// signal handler can implement the semantically-correct wraparound logic; the
// rest simply redirect to the out-of-bounds stub in the signal handler. On x86,
// the base address of memory is baked into each memory access instruction so
// the MemoryAccess records the location of each for patching. On all other
// platforms, no MemoryAccess is created.

class MemoryAccess
{
    uint32_t insnOffset_;
    uint32_t trapOutOfLineOffset_;

  public:
    MemoryAccess() = default;
    explicit MemoryAccess(uint32_t insnOffset, uint32_t trapOutOfLineOffset = UINT32_MAX)
      : insnOffset_(insnOffset),
        trapOutOfLineOffset_(trapOutOfLineOffset)
    {}

    uint32_t insnOffset() const {
        return insnOffset_;
    }
    bool hasTrapOutOfLineCode() const {
        return trapOutOfLineOffset_ != UINT32_MAX;
    }
    uint8_t* trapOutOfLineCode(uint8_t* code) const {
        MOZ_ASSERT(hasTrapOutOfLineCode());
        return code + trapOutOfLineOffset_;
    }

    void offsetBy(uint32_t delta) {
        insnOffset_ += delta;
        if (hasTrapOutOfLineCode())
            trapOutOfLineOffset_ += delta;
    }
};

WASM_DECLARE_POD_VECTOR(MemoryAccess, MemoryAccessVector)

// Metadata for the offset of an instruction to patch with the base address of
// memory. In practice, this is only used for x86 where the offset points to the
// *end* of the instruction (which is a non-fixed offset from the beginning of
// the instruction). As part of the move away from code patching, this should be
// removed.

struct MemoryPatch
{
    uint32_t offset;

    MemoryPatch() = default;
    explicit MemoryPatch(uint32_t offset) : offset(offset) {}

    void offsetBy(uint32_t delta) {
        offset += delta;
    }
};

WASM_DECLARE_POD_VECTOR(MemoryPatch, MemoryPatchVector)

} // namespace wasm
} // namespace js

#endif // wasm_types_h
