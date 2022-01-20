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

#include "wasm/WasmCodegenTypes.h"

#include "wasm/WasmExprType.h"
#include "wasm/WasmStubs.h"
#include "wasm/WasmTypeDef.h"
#include "wasm/WasmValue.h"

using mozilla::MakeEnumeratedRange;
using mozilla::PodZero;

using namespace js;
using namespace js::wasm;

#ifdef ENABLE_WASM_SIMD_WORMHOLE
static const int8_t WormholeTrigger[] = {31, 0, 30, 2,  29, 4,  28, 6,
                                         27, 8, 26, 10, 25, 12, 24};
static_assert(sizeof(WormholeTrigger) == 15);

static const int8_t WormholeSignatureBytes[16] = {0xD, 0xE, 0xA, 0xD, 0xD, 0x0,
                                                  0x0, 0xD, 0xC, 0xA, 0xF, 0xE,
                                                  0xB, 0xA, 0xB, 0xE};
static_assert(sizeof(WormholeSignatureBytes) == 16);

bool wasm::IsWormholeTrigger(const V128& shuffleMask) {
  return memcmp(shuffleMask.bytes, WormholeTrigger, sizeof(WormholeTrigger)) ==
         0;
}

jit::SimdConstant wasm::WormholeSignature() {
  return jit::SimdConstant::CreateX16(WormholeSignatureBytes);
}

#endif

ArgTypeVector::ArgTypeVector(const FuncType& funcType)
    : args_(funcType.args()),
      hasStackResults_(ABIResultIter::HasStackResults(
          ResultType::Vector(funcType.results()))) {}

bool TrapSiteVectorArray::empty() const {
  for (Trap trap : MakeEnumeratedRange(Trap::Limit)) {
    if (!(*this)[trap].empty()) {
      return false;
    }
  }

  return true;
}

void TrapSiteVectorArray::clear() {
  for (Trap trap : MakeEnumeratedRange(Trap::Limit)) {
    (*this)[trap].clear();
  }
}

void TrapSiteVectorArray::swap(TrapSiteVectorArray& rhs) {
  for (Trap trap : MakeEnumeratedRange(Trap::Limit)) {
    (*this)[trap].swap(rhs[trap]);
  }
}

void TrapSiteVectorArray::shrinkStorageToFit() {
  for (Trap trap : MakeEnumeratedRange(Trap::Limit)) {
    (*this)[trap].shrinkStorageToFit();
  }
}

size_t TrapSiteVectorArray::serializedSize() const {
  size_t ret = 0;
  for (Trap trap : MakeEnumeratedRange(Trap::Limit)) {
    ret += SerializedPodVectorSize((*this)[trap]);
  }
  return ret;
}

uint8_t* TrapSiteVectorArray::serialize(uint8_t* cursor) const {
  for (Trap trap : MakeEnumeratedRange(Trap::Limit)) {
    cursor = SerializePodVector(cursor, (*this)[trap]);
  }
  return cursor;
}

const uint8_t* TrapSiteVectorArray::deserialize(const uint8_t* cursor) {
  for (Trap trap : MakeEnumeratedRange(Trap::Limit)) {
    cursor = DeserializePodVector(cursor, &(*this)[trap]);
    if (!cursor) {
      return nullptr;
    }
  }
  return cursor;
}

size_t TrapSiteVectorArray::sizeOfExcludingThis(
    MallocSizeOf mallocSizeOf) const {
  size_t ret = 0;
  for (Trap trap : MakeEnumeratedRange(Trap::Limit)) {
    ret += (*this)[trap].sizeOfExcludingThis(mallocSizeOf);
  }
  return ret;
}

CodeRange::CodeRange(Kind kind, Offsets offsets)
    : begin_(offsets.begin), ret_(0), end_(offsets.end), kind_(kind) {
  MOZ_ASSERT(begin_ <= end_);
  PodZero(&u);
#ifdef DEBUG
  switch (kind_) {
    case FarJumpIsland:
    case TrapExit:
    case Throw:
      break;
    default:
      MOZ_CRASH("should use more specific constructor");
  }
#endif
}

CodeRange::CodeRange(Kind kind, uint32_t funcIndex, Offsets offsets)
    : begin_(offsets.begin), ret_(0), end_(offsets.end), kind_(kind) {
  u.funcIndex_ = funcIndex;
  u.func.lineOrBytecode_ = 0;
  u.func.beginToUncheckedCallEntry_ = 0;
  u.func.beginToTierEntry_ = 0;
  MOZ_ASSERT(isEntry() || isIndirectStub());
  MOZ_ASSERT(begin_ <= end_);
}

CodeRange::CodeRange(Kind kind, CallableOffsets offsets)
    : begin_(offsets.begin), ret_(offsets.ret), end_(offsets.end), kind_(kind) {
  MOZ_ASSERT(begin_ < ret_);
  MOZ_ASSERT(ret_ < end_);
  PodZero(&u);
#ifdef DEBUG
  switch (kind_) {
    case DebugTrap:
    case BuiltinThunk:
      break;
    default:
      MOZ_CRASH("should use more specific constructor");
  }
#endif
}

CodeRange::CodeRange(Kind kind, uint32_t funcIndex, CallableOffsets offsets)
    : begin_(offsets.begin), ret_(offsets.ret), end_(offsets.end), kind_(kind) {
  MOZ_ASSERT(isImportExit() && !isImportJitExit());
  MOZ_ASSERT(begin_ < ret_);
  MOZ_ASSERT(ret_ < end_);
  u.funcIndex_ = funcIndex;
  u.func.lineOrBytecode_ = 0;
  u.func.beginToUncheckedCallEntry_ = 0;
  u.func.beginToTierEntry_ = 0;
}

CodeRange::CodeRange(uint32_t funcIndex, JitExitOffsets offsets)
    : begin_(offsets.begin),
      ret_(offsets.ret),
      end_(offsets.end),
      kind_(ImportJitExit) {
  MOZ_ASSERT(isImportJitExit());
  MOZ_ASSERT(begin_ < ret_);
  MOZ_ASSERT(ret_ < end_);
  u.funcIndex_ = funcIndex;
  u.jitExit.beginToUntrustedFPStart_ = offsets.untrustedFPStart - begin_;
  u.jitExit.beginToUntrustedFPEnd_ = offsets.untrustedFPEnd - begin_;
  MOZ_ASSERT(jitExitUntrustedFPStart() == offsets.untrustedFPStart);
  MOZ_ASSERT(jitExitUntrustedFPEnd() == offsets.untrustedFPEnd);
}

CodeRange::CodeRange(uint32_t funcIndex, uint32_t funcLineOrBytecode,
                     FuncOffsets offsets)
    : begin_(offsets.begin),
      ret_(offsets.ret),
      end_(offsets.end),
      kind_(Function) {
  MOZ_ASSERT(begin_ < ret_);
  MOZ_ASSERT(ret_ < end_);
  MOZ_ASSERT(offsets.uncheckedCallEntry - begin_ <= UINT8_MAX);
  MOZ_ASSERT(offsets.tierEntry - begin_ <= UINT8_MAX);
  u.funcIndex_ = funcIndex;
  u.func.lineOrBytecode_ = funcLineOrBytecode;
  u.func.beginToUncheckedCallEntry_ = offsets.uncheckedCallEntry - begin_;
  u.func.beginToTierEntry_ = offsets.tierEntry - begin_;
}

const CodeRange* wasm::LookupInSorted(const CodeRangeVector& codeRanges,
                                      CodeRange::OffsetInCode target) {
  size_t lowerBound = 0;
  size_t upperBound = codeRanges.length();

  size_t match;
  if (!BinarySearch(codeRanges, lowerBound, upperBound, target, &match)) {
    return nullptr;
  }

  return &codeRanges[match];
}

CalleeDesc CalleeDesc::function(uint32_t funcIndex) {
  CalleeDesc c;
  c.which_ = Func;
  c.u.funcIndex_ = funcIndex;
  return c;
}
CalleeDesc CalleeDesc::import(uint32_t globalDataOffset) {
  CalleeDesc c;
  c.which_ = Import;
  c.u.import.globalDataOffset_ = globalDataOffset;
  return c;
}
CalleeDesc CalleeDesc::wasmTable(const TableDesc& desc, TypeIdDesc funcTypeId) {
  CalleeDesc c;
  c.which_ = WasmTable;
  c.u.table.globalDataOffset_ = desc.globalDataOffset;
  c.u.table.minLength_ = desc.initialLength;
  c.u.table.funcTypeId_ = funcTypeId;
  return c;
}
CalleeDesc CalleeDesc::asmJSTable(const TableDesc& desc) {
  CalleeDesc c;
  c.which_ = AsmJSTable;
  c.u.table.globalDataOffset_ = desc.globalDataOffset;
  return c;
}
CalleeDesc CalleeDesc::builtin(SymbolicAddress callee) {
  CalleeDesc c;
  c.which_ = Builtin;
  c.u.builtin_ = callee;
  return c;
}
CalleeDesc CalleeDesc::builtinInstanceMethod(SymbolicAddress callee) {
  CalleeDesc c;
  c.which_ = BuiltinInstanceMethod;
  c.u.builtin_ = callee;
  return c;
}
