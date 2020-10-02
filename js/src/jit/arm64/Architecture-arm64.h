/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_arm64_Architecture_arm64_h
#define jit_arm64_Architecture_arm64_h

#include "mozilla/Assertions.h"
#include "mozilla/MathAlgorithms.h"

#include "jit/arm64/vixl/Instructions-vixl.h"
#include "jit/shared/Architecture-shared.h"

#include "js/Utility.h"

#define JS_HAS_HIDDEN_SP
static const uint32_t HiddenSPEncoding = vixl::kSPRegInternalCode;

namespace js {
namespace jit {

// AArch64 has 32 64-bit integer registers, x0 though x31.
//  x31 is special and functions as both the stack pointer and a zero register.
//  The bottom 32 bits of each of the X registers is accessible as w0 through
//  w31. The program counter is no longer accessible as a register.
// SIMD and scalar floating-point registers share a register bank.
//  32 bit float registers are s0 through s31.
//  64 bit double registers are d0 through d31.
//  128 bit SIMD registers are v0 through v31.
// e.g., s0 is the bottom 32 bits of d0, which is the bottom 64 bits of v0.

// AArch64 Calling Convention:
//  x0 - x7: arguments and return value
//  x8: indirect result (struct) location
//  x9 - x15: temporary registers
//  x16 - x17: intra-call-use registers (PLT, linker)
//  x18: platform specific use (TLS)
//  x19 - x28: callee-saved registers
//  x29: frame pointer
//  x30: link register

// AArch64 Calling Convention for Floats:
//  d0 - d7: arguments and return value
//  d8 - d15: callee-saved registers
//   Bits 64:128 are not saved for v8-v15.
//  d16 - d31: temporary registers

// AArch64 does not have soft float.

class Registers {
 public:
  enum RegisterID {
    w0 = 0,
    x0 = 0,
    w1 = 1,
    x1 = 1,
    w2 = 2,
    x2 = 2,
    w3 = 3,
    x3 = 3,
    w4 = 4,
    x4 = 4,
    w5 = 5,
    x5 = 5,
    w6 = 6,
    x6 = 6,
    w7 = 7,
    x7 = 7,
    w8 = 8,
    x8 = 8,
    w9 = 9,
    x9 = 9,
    w10 = 10,
    x10 = 10,
    w11 = 11,
    x11 = 11,
    w12 = 12,
    x12 = 12,
    w13 = 13,
    x13 = 13,
    w14 = 14,
    x14 = 14,
    w15 = 15,
    x15 = 15,
    w16 = 16,
    x16 = 16,
    ip0 = 16,  // MacroAssembler scratch register 1.
    w17 = 17,
    x17 = 17,
    ip1 = 17,  // MacroAssembler scratch register 2.
    w18 = 18,
    x18 = 18,
    tls = 18,  // Platform-specific use (TLS).
    w19 = 19,
    x19 = 19,
    w20 = 20,
    x20 = 20,
    w21 = 21,
    x21 = 21,
    w22 = 22,
    x22 = 22,
    w23 = 23,
    x23 = 23,
    w24 = 24,
    x24 = 24,
    w25 = 25,
    x25 = 25,
    w26 = 26,
    x26 = 26,
    w27 = 27,
    x27 = 27,
    w28 = 28,
    x28 = 28,
    w29 = 29,
    x29 = 29,
    fp = 29,
    w30 = 30,
    x30 = 30,
    lr = 30,
    w31 = 31,
    x31 = 31,
    wzr = 31,
    xzr = 31,
    sp = 31,  // Special: both stack pointer and a zero register.
  };
  typedef uint8_t Code;
  typedef uint32_t Encoding;
  typedef uint32_t SetType;

  // If SP is used as the base register for a memory load or store, then the
  // value of the stack pointer prior to adding any offset must be quadword (16
  // byte) aligned, or else a stack aligment exception will be generated.
  static const Code StackPointer = sp;

  static const Code Invalid = 0xFF;

  union RegisterContent {
    uintptr_t r;
  };

  static uint32_t SetSize(SetType x) {
    static_assert(sizeof(SetType) == 4, "SetType must be 32 bits");
    return mozilla::CountPopulation32(x);
  }
  static uint32_t FirstBit(SetType x) {
    return mozilla::CountTrailingZeroes32(x);
  }
  static uint32_t LastBit(SetType x) {
    return 31 - mozilla::CountLeadingZeroes32(x);
  }

  static const char* GetName(uint32_t code) {
    static const char* const Names[] = {
        "x0",  "x1",  "x2",  "x3",  "x4",  "x5",  "x6",  "x7",
        "x8",  "x9",  "x10", "x11", "x12", "x13", "x14", "x15",
        "x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23",
        "x24", "x25", "x26", "x27", "x28", "x29", "lr",  "sp"};
    static_assert(Total == sizeof(Names) / sizeof(Names[0]),
                  "Table is the correct size");
    if (code >= Total) {
      return "invalid";
    }
    return Names[code];
  }

  static Code FromName(const char* name);

  static const uint32_t Total = 32;
  static const uint32_t TotalPhys = 32;
  static const uint32_t Allocatable =
      27;  // No named special-function registers.

  static const SetType AllMask = 0xFFFFFFFF;
  static const SetType NoneMask = 0x0;

  static const SetType ArgRegMask =
      (1 << Registers::x0) | (1 << Registers::x1) | (1 << Registers::x2) |
      (1 << Registers::x3) | (1 << Registers::x4) | (1 << Registers::x5) |
      (1 << Registers::x6) | (1 << Registers::x7) | (1 << Registers::x8);

  static const SetType VolatileMask =
      (1 << Registers::x0) | (1 << Registers::x1) | (1 << Registers::x2) |
      (1 << Registers::x3) | (1 << Registers::x4) | (1 << Registers::x5) |
      (1 << Registers::x6) | (1 << Registers::x7) | (1 << Registers::x8) |
      (1 << Registers::x9) | (1 << Registers::x10) | (1 << Registers::x11) |
      (1 << Registers::x12) | (1 << Registers::x13) | (1 << Registers::x14) |
      (1 << Registers::x15) | (1 << Registers::x16) | (1 << Registers::x17) |
      (1 << Registers::x18);

  static const SetType NonVolatileMask =
      (1 << Registers::x19) | (1 << Registers::x20) | (1 << Registers::x21) |
      (1 << Registers::x22) | (1 << Registers::x23) | (1 << Registers::x24) |
      (1 << Registers::x25) | (1 << Registers::x26) | (1 << Registers::x27) |
      (1 << Registers::x28) | (1 << Registers::x29) | (1 << Registers::x30);

  static const SetType SingleByteRegs = VolatileMask | NonVolatileMask;

  static const SetType NonAllocatableMask =
      (1 << Registers::x28) |  // PseudoStackPointer.
      (1 << Registers::ip0) |  // First scratch register.
      (1 << Registers::ip1) |  // Second scratch register.
      (1 << Registers::tls) | (1 << Registers::lr) | (1 << Registers::sp);

  static const SetType WrapperMask = VolatileMask;

  // Registers returned from a JS -> JS call.
  static const SetType JSCallMask = (1 << Registers::x2);

  // Registers returned from a JS -> C call.
  static const SetType CallMask = (1 << Registers::x0);

  static const SetType AllocatableMask = AllMask & ~NonAllocatableMask;
};

// Smallest integer type that can hold a register bitmask.
typedef uint32_t PackedRegisterMask;

template <typename T>
class TypedRegisterSet;

class FloatRegisters {
 public:
  enum FPRegisterID {
    s0 = 0,
    d0 = 0,
    v0 = 0,
    s1 = 1,
    d1 = 1,
    v1 = 1,
    s2 = 2,
    d2 = 2,
    v2 = 2,
    s3 = 3,
    d3 = 3,
    v3 = 3,
    s4 = 4,
    d4 = 4,
    v4 = 4,
    s5 = 5,
    d5 = 5,
    v5 = 5,
    s6 = 6,
    d6 = 6,
    v6 = 6,
    s7 = 7,
    d7 = 7,
    v7 = 7,
    s8 = 8,
    d8 = 8,
    v8 = 8,
    s9 = 9,
    d9 = 9,
    v9 = 9,
    s10 = 10,
    d10 = 10,
    v10 = 10,
    s11 = 11,
    d11 = 11,
    v11 = 11,
    s12 = 12,
    d12 = 12,
    v12 = 12,
    s13 = 13,
    d13 = 13,
    v13 = 13,
    s14 = 14,
    d14 = 14,
    v14 = 14,
    s15 = 15,
    d15 = 15,
    v15 = 15,
    s16 = 16,
    d16 = 16,
    v16 = 16,
    s17 = 17,
    d17 = 17,
    v17 = 17,
    s18 = 18,
    d18 = 18,
    v18 = 18,
    s19 = 19,
    d19 = 19,
    v19 = 19,
    s20 = 20,
    d20 = 20,
    v20 = 20,
    s21 = 21,
    d21 = 21,
    v21 = 21,
    s22 = 22,
    d22 = 22,
    v22 = 22,
    s23 = 23,
    d23 = 23,
    v23 = 23,
    s24 = 24,
    d24 = 24,
    v24 = 24,
    s25 = 25,
    d25 = 25,
    v25 = 25,
    s26 = 26,
    d26 = 26,
    v26 = 26,
    s27 = 27,
    d27 = 27,
    v27 = 27,
    s28 = 28,
    d28 = 28,
    v28 = 28,
    s29 = 29,
    d29 = 29,
    v29 = 29,
    s30 = 30,
    d30 = 30,
    v30 = 30,
    s31 = 31,
    d31 = 31,
    v31 = 31,  // Scratch register.
  };

  // Eight bits: (invalid << 7) | (kind << 5) | encoding
  typedef uint8_t Code;
  typedef FPRegisterID Encoding;
  typedef uint64_t SetType;

  // WARNING!  About SIMD registers on Arm64:
  //
  // There is a Kind 'Simd128' but registers of this kind cannot be stored in
  // register sets, the kind exists only to tag FloatRegisters as vector
  // registers for use outside the sets, see below.  The reason for this
  // weirdness is that the 64-bit SetType is too small to hold information about
  // vector registers, and we have poor options for making SetType larger.
  //
  // (We have two options for increasing the size of SetType: __uint128_t and
  // some simulation of __uint128_t or perhaps __uint96_t.  Using __uint128_t
  // does not work because C++ compilers generate aligned accesses to
  // __uint128_t fields, and structures containing register sets are frequently
  // not properly aligned because they are allocated with TempAllocator.  Using
  // a simulation will result in a lot of code, possibly reduce inlining of set
  // operations, and slow down JS compilation.  We don't want to pay the penalty
  // unless we have to.)
  //
  // Only the baseline compiler and the wasm stubs code need to deal with Arm64
  // vector registers, Ion will never be exposed because JS does not have SIMD
  // and we don't have Arm64 wasm support in Ion.  So a fix that introduces the
  // notion of vector registers but does not allow them to be put into sets
  // works fairly well: The baseline compiler manages vector registers by
  // managing the double registers which alias the vector registers, while the
  // stubs code uses special save and restore paths that always save and restore
  // the vector registers when they may contain meaningful data.  The complexity
  // is local to the stubs code, the lowest level of the register management
  // code in the baseline compiler, and the code in this file.

  enum Kind : uint8_t {
    Double,
    Single,
#ifdef ENABLE_WASM_SIMD
    Simd128,
#endif
    NumTypes
  };

  static constexpr int NumScalarTypes = 2;

  static constexpr Code Invalid = 0x80;

  static const char* GetName(uint32_t code) {
    // Doubles precede singles, see `Kind` enum above.
    static const char* const Names[] = {
        "d0",  "d1",  "d2",  "d3",  "d4",  "d5",  "d6",  "d7",  "d8",  "d9",
        "d10", "d11", "d12", "d13", "d14", "d15", "d16", "d17", "d18", "d19",
        "d20", "d21", "d22", "d23", "d24", "d25", "d26", "d27", "d28", "d29",
        "d30", "d31", "s0",  "s1",  "s2",  "s3",  "s4",  "s5",  "s6",  "s7",
        "s8",  "s9",  "s10", "s11", "s12", "s13", "s14", "s15", "s16", "s17",
        "s18", "s19", "s20", "s21", "s22", "s23", "s24", "s25", "s26", "s27",
        "s28", "s29", "s30", "s31",
#ifdef ENABLE_WASM_SIMD
        "v0",  "v1",  "v2",  "v3",  "v4",  "v5",  "v6",  "v7",  "v8",  "v9",
        "v10", "v11", "v12", "v13", "v14", "v15", "v16", "v17", "v18", "v19",
        "v20", "v21", "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29",
        "v30", "v31",
#endif
    };
    static_assert(TotalWithSimd == sizeof(Names) / sizeof(Names[0]),
                  "Table is the correct size");
    if (code >= TotalWithSimd) {
      return "invalid";
    }
    return Names[code];
  }

  static Code FromName(const char* name);

  static const uint32_t TotalPhys = 32;
  static const uint32_t Total = TotalPhys * NumScalarTypes;
  static const uint32_t TotalWithSimd = TotalPhys * NumTypes;
  static const uint32_t Allocatable = 31;  // Without d31, the scratch register.

  static_assert(sizeof(SetType) * 8 >= Total,
                "SetType should be large enough to enumerate all registers.");

  static const SetType SpreadSingle = SetType(1)
                                      << (uint32_t(Single) * TotalPhys);
  static const SetType SpreadDouble = SetType(1)
                                      << (uint32_t(Double) * TotalPhys);
  static const SetType Spread = SpreadSingle | SpreadDouble;

  static const SetType AllPhysMask = (SetType(1) << TotalPhys) - 1;
  static const SetType AllMask = AllPhysMask * Spread;
  static const SetType AllDoubleMask = AllPhysMask * SpreadDouble;
  static const SetType AllSingleMask = AllPhysMask * SpreadSingle;
  static const SetType NoneMask = SetType(0);

  // d31 is the ScratchFloatReg.
  static const SetType NonVolatileMask =
      SetType((1 << FloatRegisters::d8) | (1 << FloatRegisters::d9) |
              (1 << FloatRegisters::d10) | (1 << FloatRegisters::d11) |
              (1 << FloatRegisters::d12) | (1 << FloatRegisters::d13) |
              (1 << FloatRegisters::d14) | (1 << FloatRegisters::d15) |
              (1 << FloatRegisters::d16) | (1 << FloatRegisters::d17) |
              (1 << FloatRegisters::d18) | (1 << FloatRegisters::d19) |
              (1 << FloatRegisters::d20) | (1 << FloatRegisters::d21) |
              (1 << FloatRegisters::d22) | (1 << FloatRegisters::d23) |
              (1 << FloatRegisters::d24) | (1 << FloatRegisters::d25) |
              (1 << FloatRegisters::d26) | (1 << FloatRegisters::d27) |
              (1 << FloatRegisters::d28) | (1 << FloatRegisters::d29) |
              (1 << FloatRegisters::d30)) *
      Spread;

  static const SetType VolatileMask = AllMask & ~NonVolatileMask;

  static const SetType WrapperMask = VolatileMask;

  // d31 is the ScratchFloatReg.
  static const SetType NonAllocatableMask =
      (SetType(1) << FloatRegisters::d31) * Spread;

  static const SetType AllocatableMask = AllMask & ~NonAllocatableMask;
  union RegisterContent {
    float s;
    double d;
  };

  static constexpr Encoding encoding(Code c) {
    // assert() not available in constexpr function.
    // assert(c < TotalWithSimd);
    return Encoding(c & 31);
  }

  static constexpr Kind kind(Code c) {
    // assert() not available in constexpr function.
    // assert(c < TotalWithSimd && ((c >> 5) & 3) < NumTypes);
    return Kind((c >> 5) & 3);
  }

  static constexpr Code fromParts(uint32_t encoding, uint32_t kind,
                                  uint32_t invalid) {
    return Code((invalid << 7) | (kind << 5) | encoding);
  }
};

// In bytes: slots needed for potential memory->memory move spills.
//   +8 for cycles
//   +8 for gpr spills
//   +8 for double spills
static const uint32_t ION_FRAME_SLACK_SIZE = 24;

static const uint32_t ShadowStackSpace = 0;

// When our only strategy for far jumps is to encode the offset directly, and
// not insert any jump islands during assembly for even further jumps, then the
// architecture restricts us to -2^27 .. 2^27-4, to fit into a signed 28-bit
// value.  We further reduce this range to allow the far-jump inserting code to
// have some breathing room.
static const uint32_t JumpImmediateRange = ((1 << 27) - (20 * 1024 * 1024));

static const uint32_t ABIStackAlignment = 16;
static const uint32_t CodeAlignment = 16;
static const bool StackKeptAligned = false;

// Although sp is only usable if 16-byte alignment is kept,
// the Pseudo-StackPointer enables use of 8-byte alignment.
static const uint32_t StackAlignment = 8;
static const uint32_t NativeFrameSize = 8;

struct FloatRegister {
  typedef FloatRegisters Codes;
  typedef Codes::Code Code;
  typedef Codes::Encoding Encoding;
  typedef Codes::SetType SetType;

  static uint32_t SetSize(SetType x) {
    static_assert(sizeof(SetType) == 8, "SetType must be 64 bits");
    x |= x >> FloatRegisters::TotalPhys;
    x &= FloatRegisters::AllPhysMask;
    return mozilla::CountPopulation32(x);
  }

  static uint32_t FirstBit(SetType x) {
    static_assert(sizeof(SetType) == 8, "SetType");
    return mozilla::CountTrailingZeroes64(x);
  }
  static uint32_t LastBit(SetType x) {
    static_assert(sizeof(SetType) == 8, "SetType");
    return 63 - mozilla::CountLeadingZeroes64(x);
  }

  static constexpr size_t SizeOfSimd128 = 16;

 private:
  // These fields only hold valid values: an invalid register is always
  // represented as a valid encoding and kind with the invalid_ bit set.
  uint8_t encoding_;  // 32 encodings
  uint8_t kind_;      // Double, Single, Simd128
  bool invalid_;

  typedef Codes::Kind Kind;

 public:
  constexpr FloatRegister(Encoding encoding, Kind kind)
      : encoding_(encoding), kind_(kind), invalid_(false) {
    // assert(uint32_t(encoding) < Codes::TotalPhys);
  }

  constexpr FloatRegister()
      : encoding_(0), kind_(FloatRegisters::Double), invalid_(true) {}

  static FloatRegister FromCode(uint32_t i) {
    MOZ_ASSERT(i < Codes::TotalWithSimd);
    return FloatRegister(FloatRegisters::encoding(i), FloatRegisters::kind(i));
  }

  bool isSingle() const {
    MOZ_ASSERT(!invalid_);
    return kind_ == FloatRegisters::Single;
  }
  bool isDouble() const {
    MOZ_ASSERT(!invalid_);
    return kind_ == FloatRegisters::Double;
  }
  bool isSimd128() const {
    MOZ_ASSERT(!invalid_);
#ifdef ENABLE_WASM_SIMD
    return kind_ == FloatRegisters::Simd128;
#else
    return false;
#endif
  }
  bool isInvalid() const { return invalid_; }

  FloatRegister asSingle() const {
    MOZ_ASSERT(!invalid_);
    return FloatRegister(Encoding(encoding_), FloatRegisters::Single);
  }
  FloatRegister asDouble() const {
    MOZ_ASSERT(!invalid_);
    return FloatRegister(Encoding(encoding_), FloatRegisters::Double);
  }
  FloatRegister asSimd128() const {
    MOZ_ASSERT(!invalid_);
#ifdef ENABLE_WASM_SIMD
    return FloatRegister(Encoding(encoding_), FloatRegisters::Simd128);
#else
    MOZ_CRASH("No SIMD support");
#endif
  }

  constexpr uint32_t size() const {
    MOZ_ASSERT(!invalid_);
    if (kind_ == FloatRegisters::Double) {
      return sizeof(double);
    }
    if (kind_ == FloatRegisters::Single) {
      return sizeof(float);
    }
#ifdef ENABLE_WASM_SIMD
    MOZ_ASSERT(kind_ == FloatRegisters::Simd128);
    return 16;
#else
    MOZ_CRASH("No SIMD support");
#endif
  }

  constexpr Code code() const {
    // assert(!invalid_);
    return Codes::fromParts(encoding_, kind_, invalid_);
  }

  constexpr Encoding encoding() const {
    MOZ_ASSERT(!invalid_);
    return Encoding(encoding_);
  }

  const char* name() const { return FloatRegisters::GetName(code()); }
  bool volatile_() const {
    MOZ_ASSERT(!invalid_);
    return !!((SetType(1) << code()) & FloatRegisters::VolatileMask);
  }
  constexpr bool operator!=(FloatRegister other) const {
    return code() != other.code();
  }
  constexpr bool operator==(FloatRegister other) const {
    return code() == other.code();
  }

  bool aliases(FloatRegister other) const {
    return other.encoding_ == encoding_;
  }
  // This function mostly exists for the ARM backend.  It is to ensure that two
  // floating point registers' types are equivalent.  e.g. S0 is not equivalent
  // to D16, since S0 holds a float32, and D16 holds a Double.
  // Since all floating point registers on x86 and x64 are equivalent, it is
  // reasonable for this function to do the same.
  bool equiv(FloatRegister other) const {
    MOZ_ASSERT(!invalid_);
    return kind_ == other.kind_;
  }

  // numAliased is used only by Ion's register allocator, ergo we ignore SIMD
  // registers here as Ion will not be exposed to SIMD on this platform.  See
  // comments above in FloatRegisters.
  uint32_t numAliased() const { return Codes::NumScalarTypes; }
  uint32_t numAlignedAliased() { return numAliased(); }

  FloatRegister aliased(uint32_t aliasIdx) {
    MOZ_ASSERT(!invalid_);
    MOZ_ASSERT(aliasIdx < numAliased());
    return FloatRegister(Encoding(encoding_),
                         Kind((aliasIdx + kind_) % numAliased()));
  }
  FloatRegister alignedAliased(uint32_t aliasIdx) {
    MOZ_ASSERT(aliasIdx < numAliased());
    return aliased(aliasIdx);
  }
  SetType alignedOrDominatedAliasedSet() const {
    return Codes::Spread << encoding_;
  }

  static constexpr RegTypeName DefaultType = RegTypeName::Float64;

  template <RegTypeName Name = DefaultType>
  static SetType LiveAsIndexableSet(SetType s) {
    return SetType(0);
  }

  template <RegTypeName Name = DefaultType>
  static SetType AllocatableAsIndexableSet(SetType s) {
    static_assert(Name != RegTypeName::Any, "Allocatable set are not iterable");
    return LiveAsIndexableSet<Name>(s);
  }

  static TypedRegisterSet<FloatRegister> ReduceSetForPush(
      const TypedRegisterSet<FloatRegister>& s);
  static uint32_t GetPushSizeInBytes(const TypedRegisterSet<FloatRegister>& s);
#ifdef ENABLE_WASM_SIMD
  static uint32_t GetPushSizeInBytesForWasmStubs(
      const TypedRegisterSet<FloatRegister>& s);
#endif
  uint32_t getRegisterDumpOffsetInBytes();
};

template <>
inline FloatRegister::SetType
FloatRegister::LiveAsIndexableSet<RegTypeName::Float32>(SetType set) {
  return set & FloatRegisters::AllSingleMask;
}

template <>
inline FloatRegister::SetType
FloatRegister::LiveAsIndexableSet<RegTypeName::Float64>(SetType set) {
  return set & FloatRegisters::AllDoubleMask;
}

template <>
inline FloatRegister::SetType
FloatRegister::LiveAsIndexableSet<RegTypeName::Any>(SetType set) {
  return set;
}

// ARM/D32 has double registers that cannot be treated as float32.
// Luckily, ARMv8 doesn't have the same misfortune.
inline bool hasUnaliasedDouble() { return false; }

// ARM prior to ARMv8 also has doubles that alias multiple floats.
// Again, ARMv8 is in the clear.
inline bool hasMultiAlias() { return false; }

uint32_t GetARM64Flags();

bool CanFlushICacheFromBackgroundThreads();

}  // namespace jit
}  // namespace js

#endif  // jit_arm64_Architecture_arm64_h
