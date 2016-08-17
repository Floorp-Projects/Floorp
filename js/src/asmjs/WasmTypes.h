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
#include "mozilla/RefCounted.h"
#include "mozilla/RefPtr.h"

#include "NamespaceImports.h"

#include "asmjs/WasmBinary.h"
#include "ds/LifoAlloc.h"
#include "jit/IonTypes.h"
#include "js/UniquePtr.h"
#include "js/Utility.h"
#include "js/Vector.h"
#include "vm/MallocProvider.h"

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
using mozilla::RefCounted;
using mozilla::Some;

typedef Vector<uint32_t, 0, SystemAllocPolicy> Uint32Vector;

class Code;
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

// ValType/ExprType utilities

// ExprType::Limit is an out-of-band value and has no wasm-semantic meaning. For
// the purpose of recursive validation, we use this value to represent the type
// of branch/return instructions that don't actually return to the parent
// expression and can thus be used in any context.
const ExprType AnyType = ExprType::Limit;

inline ExprType
Unify(ExprType a, ExprType b)
{
    if (a == AnyType)
        return b;
    if (b == AnyType)
        return a;
    if (a == b)
        return a;
    return ExprType::Void;
}

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
IsSimdType(ExprType et)
{
    return IsVoid(et) ? false : IsSimdType(ValType(et));
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
      case ValType::Limit: break;
    }
    MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("bad type");
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
    union {
        uint32_t i32_;
        uint64_t i64_;
        float f32_;
        double f64_;
        I8x16 i8x16_;
        I16x8 i16x8_;
        I32x4 i32x4_;
        F32x4 f32x4_;
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
    union {
        Val val_;
        struct {
            uint32_t index_;
            ValType type_;
        } global;
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
    union {
        struct {
            union {
                InitExpr initial_;
                struct {
                    ValType type_;
                    uint32_t index_;
                } import;
            } val;
            unsigned offset_;
            bool isMutable_;
        } var;
        Val cst_;
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

// While the frame-pointer chain allows the stack to be unwound without
// metadata, Error.stack still needs to know the line/column of every call in
// the chain. A CallSiteDesc describes a single callsite to which CallSite adds
// the metadata necessary to walk up to the next frame. Lastly CallSiteAndTarget
// adds the function index of the callee.

class CallSiteDesc
{
    uint32_t lineOrBytecode_ : 31;
    uint32_t kind_ : 1;
  public:
    enum Kind {
        Relative,  // pc-relative call
        Register   // call *register
    };
    CallSiteDesc() {}
    explicit CallSiteDesc(Kind kind)
      : lineOrBytecode_(0), kind_(kind)
    {}
    CallSiteDesc(uint32_t lineOrBytecode, Kind kind)
      : lineOrBytecode_(lineOrBytecode), kind_(kind)
    {
        MOZ_ASSERT(lineOrBytecode_ == lineOrBytecode, "must fit in 31 bits");
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
    uint32_t targetIndex_;

  public:
    CallSiteAndTarget(CallSite cs, uint32_t targetIndex)
      : CallSite(cs), targetIndex_(targetIndex)
    { }

    static const uint32_t NOT_INTERNAL = UINT32_MAX;

    bool isInternal() const { return targetIndex_ != NOT_INTERNAL; }
    uint32_t targetIndex() const { MOZ_ASSERT(isInternal()); return targetIndex_; }
};

typedef Vector<CallSiteAndTarget, 0, SystemAllocPolicy> CallSiteAndTargetVector;

// Metadata for a bounds check that may need patching later.

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
    uint32_t cmpOffset_; // absolute offset of the comparison
};

// Summarizes a heap access made by wasm code that needs to be patched later
// and/or looked up by the wasm signal handlers. Different architectures need
// to know different things (x64: offset and length, ARM: where to patch in
// heap length, x86: where to patch in heap length and base).

#if defined(JS_CODEGEN_X86)
class MemoryAccess
{
    uint32_t nextInsOffset_;

  public:
    MemoryAccess() = default;

    explicit MemoryAccess(uint32_t nextInsOffset)
      : nextInsOffset_(nextInsOffset)
    { }

    void* patchMemoryPtrImmAt(uint8_t* code) const { return code + nextInsOffset_; }
    void offsetBy(uint32_t offset) { nextInsOffset_ += offset; }
};
#elif defined(JS_CODEGEN_X64)
class MemoryAccess
{
    uint32_t insnOffset_;
    uint8_t offsetWithinWholeSimdVector_; // if is this e.g. the Z of an XYZ
    bool throwOnOOB_;                     // should we throw on OOB?
    bool wrapOffset_;                     // should we wrap the offset on OOB?

  public:
    enum OutOfBoundsBehavior {
        Throw,
        CarryOn,
    };
    enum WrappingBehavior {
        WrapOffset,
        DontWrapOffset,
    };

    MemoryAccess() = default;

    MemoryAccess(uint32_t insnOffset, OutOfBoundsBehavior onOOB, WrappingBehavior onWrap,
                 uint32_t offsetWithinWholeSimdVector = 0)
      : insnOffset_(insnOffset),
        offsetWithinWholeSimdVector_(offsetWithinWholeSimdVector),
        throwOnOOB_(onOOB == OutOfBoundsBehavior::Throw),
        wrapOffset_(onWrap == WrappingBehavior::WrapOffset)
    {
        MOZ_ASSERT(offsetWithinWholeSimdVector_ == offsetWithinWholeSimdVector, "fits in uint8");
    }

    uint32_t insnOffset() const { return insnOffset_; }
    uint32_t offsetWithinWholeSimdVector() const { return offsetWithinWholeSimdVector_; }
    bool throwOnOOB() const { return throwOnOOB_; }
    bool wrapOffset() const { return wrapOffset_; }

    void offsetBy(uint32_t offset) { insnOffset_ += offset; }
};
#elif defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_ARM64) || \
      defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64) || \
      defined(JS_CODEGEN_NONE)
// Nothing! We just want bounds checks on these platforms.
class MemoryAccess {
  public:
    void offsetBy(uint32_t) { MOZ_CRASH(); }
    uint32_t insnOffset() const { MOZ_CRASH(); }
};
#endif

WASM_DECLARE_POD_VECTOR(MemoryAccess, MemoryAccessVector)
WASM_DECLARE_POD_VECTOR(BoundsCheck, BoundsCheckVector)

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
    HandleTrap,
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
    Limit
};

void*
AddressOf(SymbolicAddress imm, ExclusiveContext* cx);

// A wasm::Trap is a reason for why we reached a trap in executed code. Each
// different trap is mapped to a different error message.

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
    // Unaligned memory access.
    UnalignedAccess,
    // Bad signature for an indirect call.
    BadIndirectCall,

    // (asm.js only) SIMD float to int conversion failed because the input
    // wasn't in bounds.
    ImpreciseSimdConversion,

    Limit
};

// A wasm::JumpTarget represents one of a special set of stubs that can be
// jumped to from any function. Because wasm modules can be larger than the
// range of a plain jump, these potentially out-of-range jumps must be recorded
// and patched specially by the MacroAssembler and ModuleGenerator.

enum class JumpTarget
{
    // Traps
    Unreachable = unsigned(Trap::Unreachable),
    IntegerOverflow = unsigned(Trap::IntegerOverflow),
    InvalidConversionToInteger = unsigned(Trap::InvalidConversionToInteger),
    IntegerDivideByZero = unsigned(Trap::IntegerDivideByZero),
    OutOfBounds = unsigned(Trap::OutOfBounds),
    UnalignedAccess = unsigned(Trap::UnalignedAccess),
    BadIndirectCall = unsigned(Trap::BadIndirectCall),
    ImpreciseSimdConversion = unsigned(Trap::ImpreciseSimdConversion),
    // Non-traps
    StackOverflow,
    Throw,
    Limit
};

typedef EnumeratedArray<JumpTarget, JumpTarget::Limit, Uint32Vector> JumpSiteArray;

// The SignalUsage struct captures global parameters that affect all wasm code
// generation. It also currently is the single source of truth for whether or
// not to use signal handlers for different purposes.

struct SignalUsage
{
    // NB: these fields are serialized as a POD in Assumptions.
    bool forOOB;
    bool forInterrupt;

    SignalUsage();
    bool operator==(SignalUsage rhs) const;
    bool operator!=(SignalUsage rhs) const { return !(*this == rhs); }
};

// Assumptions captures ambient state that must be the same when compiling and
// deserializing a module for the compiled code to be valid. If it's not, then
// the module must be recompiled from scratch.

struct Assumptions
{
    SignalUsage           usesSignal;
    uint32_t              cpuId;
    JS::BuildIdCharVector buildId;
    bool                  newFormat;

    explicit Assumptions(JS::BuildIdCharVector&& buildId);

    // If Assumptions is constructed without arguments, initBuildIdFromContext()
    // must be called to complete initialization.
    Assumptions();
    bool initBuildIdFromContext(ExclusiveContext* cx);

    bool clone(const Assumptions& other);

    bool operator==(const Assumptions& rhs) const;
    bool operator!=(const Assumptions& rhs) const { return !(*this == rhs); }

    WASM_DECLARE_SERIALIZABLE(Assumptions)
};

// A Module can either be asm.js or wasm.

enum ModuleKind
{
    Wasm,
    AsmJS
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
    uint32_t initial;
    uint32_t maximum;

    TableDesc() { PodZero(this); }
};

WASM_DECLARE_POD_VECTOR(TableDesc, TableDescVector)

// CalleeDesc describes how to compile one of the variety of asm.js/wasm calls.
// This is hoisted into WasmTypes.h for sharing between Ion and Baseline.

class CalleeDesc
{
  public:
    enum Which { Internal, Import, WasmTable, AsmJSTable, Builtin };

  private:
    Which which_;
    union U {
        U() {}
        uint32_t internalFuncIndex_;
        struct {
            uint32_t globalDataOffset_;
        } import;
        struct {
            TableDesc desc_;
            SigIdDesc sigId_;
        } table;
        SymbolicAddress builtin_;
    } u;

  public:
    CalleeDesc() {}
    static CalleeDesc internal(uint32_t callee) {
        CalleeDesc c;
        c.which_ = Internal;
        c.u.internalFuncIndex_ = callee;
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
        c.u.table.desc_ = desc;
        c.u.table.sigId_ = sigId;
        return c;
    }
    static CalleeDesc asmJSTable(const TableDesc& desc) {
        CalleeDesc c;
        c.which_ = AsmJSTable;
        c.u.table.desc_ = desc;
        return c;
    }
    static CalleeDesc builtin(SymbolicAddress callee) {
        CalleeDesc c;
        c.which_ = Builtin;
        c.u.builtin_ = callee;
        return c;
    }
    Which which() const {
        return which_;
    }
    uint32_t internalFuncIndex() const {
        MOZ_ASSERT(which_ == Internal);
        return u.internalFuncIndex_;
    }
    uint32_t importGlobalDataOffset() const {
        MOZ_ASSERT(which_ == Import);
        return u.import.globalDataOffset_;
    }
    bool isTable() const {
        return which_ == WasmTable || which_ == AsmJSTable;
    }
    uint32_t tableGlobalDataOffset() const {
        MOZ_ASSERT(isTable());
        return u.table.desc_.globalDataOffset;
    }
    uint32_t wasmTableLength() const {
        MOZ_ASSERT(which_ == WasmTable);
        return u.table.desc_.initial;
    }
    bool wasmTableIsExternal() const {
        MOZ_ASSERT(which_ == WasmTable);
        return u.table.desc_.external;
    }
    SigIdDesc wasmTableSigId() const {
        MOZ_ASSERT(which_ == WasmTable);
        return u.table.sigId_;
    }
    SymbolicAddress builtin() const {
        MOZ_ASSERT(which_ == Builtin);
        return u.builtin_;
    }
};

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

// When a table can be shared between instances (it is "external"), the internal
// representation is an array of ExternalTableElem instead of just an array of
// code pointers.

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

// Constants:

// The WebAssembly spec hard-codes the virtual page size to be 64KiB and
// requires linear memory to always be a multiple of 64KiB.
static const unsigned PageSize = 64 * 1024;

#ifdef ASMJS_MAY_USE_SIGNAL_HANDLERS_FOR_OOB
static const uint64_t Uint32Range = uint64_t(UINT32_MAX) + 1;
static const uint64_t MappedSize = 2 * Uint32Range + PageSize;
#endif

static const unsigned NaN64GlobalDataOffset       = 0;
static const unsigned NaN32GlobalDataOffset       = NaN64GlobalDataOffset + sizeof(double);
static const unsigned InitialGlobalDataBytes      = NaN32GlobalDataOffset + sizeof(float);

static const unsigned MaxSigs                     =        4 * 1024;
static const unsigned MaxFuncs                    =      512 * 1024;
static const unsigned MaxGlobals                  =        4 * 1024;
static const unsigned MaxLocals                   =       64 * 1024;
static const unsigned MaxImports                  =       64 * 1024;
static const unsigned MaxExports                  =       64 * 1024;
static const unsigned MaxTables                   =        4 * 1024;
static const unsigned MaxTableElems               =     1024 * 1024;
static const unsigned MaxDataSegments             =       64 * 1024;
static const unsigned MaxElemSegments             =       64 * 1024;
static const unsigned MaxArgsPerFunc              =        4 * 1024;
static const unsigned MaxBrTableElems             = 4 * 1024 * 1024;

} // namespace wasm
} // namespace js

#endif // wasm_types_h
