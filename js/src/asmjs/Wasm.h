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

#ifndef asmjs_wasm_h
#define asmjs_wasm_h

#include "mozilla/HashFunctions.h"

#include "ds/LifoAlloc.h"
#include "jit/IonTypes.h"
#include "js/Utility.h"
#include "js/Vector.h"

namespace js {
namespace wasm {

using mozilla::Move;

// The ValType enum represents the WebAssembly "value type", which are used to
// specify the type of locals and parameters.

enum class ValType : uint8_t
{
    I32,
    I64,
    F32,
    F64,
    I32x4,
    F32x4
};

static inline bool
IsSimdType(ValType vt)
{
    return vt == ValType::I32x4 || vt == ValType::F32x4;
}

static inline jit::MIRType
ToMIRType(ValType vt)
{
    switch (vt) {
      case ValType::I32: return jit::MIRType_Int32;
      case ValType::I64: MOZ_CRASH("NYI");
      case ValType::F32: return jit::MIRType_Float32;
      case ValType::F64: return jit::MIRType_Double;
      case ValType::I32x4: return jit::MIRType_Int32x4;
      case ValType::F32x4: return jit::MIRType_Float32x4;
    }
    MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("bad type");
}

// The Val class represents a single WebAssembly value of a given value type,
// mostly for the purpose of numeric literals and initializers. A Val does not
// directly map to a JS value since there is not (currently) a precise
// representation of i64 values. A Val may contain non-canonical NaNs since,
// within WebAssembly, floats are not canonicalized. Canonicalization must
// happen at the JS boundary.

class Val
{
  public:
    typedef int32_t I32x4[4];
    typedef float F32x4[4];

  private:
    ValType type_;
    union {
        uint32_t i32_;
        uint64_t i64_;
        float f32_;
        double f64_;
        I32x4 i32x4_;
        F32x4 f32x4_;
    } u;

  public:
    Val() = default;

    explicit Val(uint32_t i32) : type_(ValType::I32) { u.i32_ = i32; }
    explicit Val(uint64_t i64) : type_(ValType::I64) { u.i64_ = i64; }
    explicit Val(float f32) : type_(ValType::F32) { u.f32_ = f32; }
    explicit Val(double f64) : type_(ValType::F64) { u.f64_ = f64; }
    explicit Val(const I32x4& i32x4) : type_(ValType::I32x4) { memcpy(u.i32x4_, i32x4, sizeof(u.i32x4_)); }
    explicit Val(const F32x4& f32x4) : type_(ValType::F32x4) { memcpy(u.f32x4_, f32x4, sizeof(u.f32x4_)); }

    ValType type() const { return type_; }
    bool isSimd() const { return IsSimdType(type()); }

    uint32_t i32() const { MOZ_ASSERT(type_ == ValType::I32); return u.i32_; }
    uint64_t i64() const { MOZ_ASSERT(type_ == ValType::I64); return u.i64_; }
    float f32() const { MOZ_ASSERT(type_ == ValType::F32); return u.f32_; }
    double f64() const { MOZ_ASSERT(type_ == ValType::F64); return u.f64_; }
    const I32x4& i32x4() const { MOZ_ASSERT(type_ == ValType::I32x4); return u.i32x4_; }
    const F32x4& f32x4() const { MOZ_ASSERT(type_ == ValType::F32x4); return u.f32x4_; }
};

// The ExprType enum represents the type of a WebAssembly expression or return
// value and may either be a value type or void. A future WebAssembly extension
// may generalize expression types to instead be a list of value types (with
// void represented by the empty list). For now it's easier to have a flat enum
// and be explicit about conversions to/from value types.

enum class ExprType : uint8_t
{
    I32 = uint8_t(ValType::I32),
    I64 = uint8_t(ValType::I64),
    F32 = uint8_t(ValType::F32),
    F64 = uint8_t(ValType::F64),
    I32x4 = uint8_t(ValType::I32x4),
    F32x4 = uint8_t(ValType::F32x4),
    Void
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
    return IsVoid(et) ? jit::MIRType_None : ToMIRType(ValType(et));
}

// The Sig class represents a WebAssembly function signature which takes a list
// of value types and returns an expression type. The engine uses two in-memory
// representations of the argument Vector's memory (when elements do not fit
// inline): normal malloc allocation (via SystemAllocPolicy) and allocation in
// a LifoAlloc (via LifoAllocPolicy). The former Sig objects can have any
// lifetime since they own the memory. The latter Sig objects must not outlive
// the associated LifoAlloc mark/release interval (which is currently the
// duration of module validation+compilation). Thus, long-lived objects like
// AsmJSModule must use malloced allocation.

template <class AllocPolicy>
class Sig
{
  public:
    typedef Vector<ValType, 4, AllocPolicy> ArgVector;

  private:
    ArgVector args_;
    ExprType ret_;

  protected:
    explicit Sig(AllocPolicy alloc = AllocPolicy()) : args_(alloc) {}
    Sig(Sig&& rhs) : args_(Move(rhs.args_)), ret_(rhs.ret_) {}
    Sig(ArgVector&& args, ExprType ret) : args_(Move(args)), ret_(ret) {}

  public:
    void init(ArgVector&& args, ExprType ret) {
        MOZ_ASSERT(args_.empty());
        args_ = Move(args);
        ret_ = ret;
    }

    ValType arg(unsigned i) const { return args_[i]; }
    const ArgVector& args() const { return args_; }
    const ExprType& ret() const { return ret_; }

    HashNumber hash() const {
        HashNumber hn = HashNumber(ret_);
        for (unsigned i = 0; i < args_.length(); i++)
            hn = mozilla::AddToHash(hn, HashNumber(args_[i]));
        return hn;
    }

    template <class AllocPolicy2>
    bool operator==(const Sig<AllocPolicy2>& rhs) const {
        if (ret() != rhs.ret())
            return false;
        if (args().length() != rhs.args().length())
            return false;
        for (unsigned i = 0; i < args().length(); i++) {
            if (arg(i) != rhs.arg(i))
                return false;
        }
        return true;
    }

    template <class AllocPolicy2>
    bool operator!=(const Sig<AllocPolicy2>& rhs) const {
        return !(*this == rhs);
    }
};

class MallocSig : public Sig<SystemAllocPolicy>
{
    typedef Sig<SystemAllocPolicy> BaseSig;

  public:
    MallocSig() = default;
    MallocSig(MallocSig&& rhs) : BaseSig(Move(rhs)) {}
    MallocSig(ArgVector&& args, ExprType ret) : BaseSig(Move(args), ret) {}
};

class LifoSig : public Sig<LifoAllocPolicy<Fallible>>
{
    typedef Sig<LifoAllocPolicy<Fallible>> BaseSig;
    LifoSig(ArgVector&& args, ExprType ret) : BaseSig(Move(args), ret) {}

  public:
    static LifoSig* new_(LifoAlloc& lifo, const MallocSig& src) {
        void* mem = lifo.alloc(sizeof(LifoSig));
        if (!mem)
            return nullptr;
        ArgVector args(lifo);
        if (!args.appendAll(src.args()))
            return nullptr;
        return new (mem) LifoSig(Move(args), src.ret());
    }
};

} // namespace wasm
} // namespace js

#endif // asmjs_wasm_h
