/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
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

#include "wasm/WasmTypeDef.h"

#include "mozilla/MathAlgorithms.h"

#include "jit/JitOptions.h"
#include "js/friend/ErrorMessages.h"  // JSMSG_*
#include "js/HashTable.h"
#include "js/Printf.h"
#include "js/Value.h"
#include "threading/ExclusiveData.h"
#include "vm/StringType.h"
#include "wasm/WasmJS.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

using mozilla::IsPowerOfTwo;

using ImmediateType = uint32_t;  // for 32/64 consistency
static const unsigned sTotalBits = sizeof(ImmediateType) * 8;
static const unsigned sTagBits = 1;
static const unsigned sReturnBit = 1;
static const unsigned sLengthBits = 4;
static const unsigned sTypeBits = 3;
static const unsigned sMaxTypes =
    (sTotalBits - sTagBits - sReturnBit - sLengthBits) / sTypeBits;

static bool IsImmediateValType(ValType vt) {
  switch (vt.kind()) {
    case ValType::I32:
    case ValType::I64:
    case ValType::F32:
    case ValType::F64:
    case ValType::V128:
      return true;
    case ValType::Ref:
      switch (vt.refTypeKind()) {
        case RefType::Func:
        case RefType::Extern:
        case RefType::Eq:
          return true;
        case RefType::TypeRef:
          return false;
      }
      break;
  }
  MOZ_CRASH("bad ValType");
}

static unsigned EncodeImmediateValType(ValType vt) {
  static_assert(7 < (1 << sTypeBits), "fits");
  switch (vt.kind()) {
    case ValType::I32:
      return 0;
    case ValType::I64:
      return 1;
    case ValType::F32:
      return 2;
    case ValType::F64:
      return 3;
    case ValType::V128:
      return 4;
    case ValType::Ref:
      switch (vt.refTypeKind()) {
        case RefType::Func:
          return 5;
        case RefType::Extern:
          return 6;
        case RefType::Eq:
          return 7;
        case RefType::TypeRef:
          break;
      }
      break;
  }
  MOZ_CRASH("bad ValType");
}

static bool IsImmediateFuncType(const FuncType& funcType) {
  const ValTypeVector& results = funcType.results();
  const ValTypeVector& args = funcType.args();
  if (results.length() + args.length() > sMaxTypes) {
    return false;
  }

  if (results.length() > 1) {
    return false;
  }

  for (ValType v : results) {
    if (!IsImmediateValType(v)) {
      return false;
    }
  }

  for (ValType v : args) {
    if (!IsImmediateValType(v)) {
      return false;
    }
  }

  return true;
}

static ImmediateType LengthToBits(uint32_t length) {
  static_assert(sMaxTypes <= ((1 << sLengthBits) - 1), "fits");
  MOZ_ASSERT(length <= sMaxTypes);
  return length;
}

static ImmediateType EncodeImmediateFuncType(const FuncType& funcType) {
  ImmediateType immediate = FuncType::ImmediateBit;
  uint32_t shift = sTagBits;

  if (funcType.results().length() > 0) {
    MOZ_ASSERT(funcType.results().length() == 1);
    immediate |= (1 << shift);
    shift += sReturnBit;

    immediate |= EncodeImmediateValType(funcType.results()[0]) << shift;
    shift += sTypeBits;
  } else {
    shift += sReturnBit;
  }

  immediate |= LengthToBits(funcType.args().length()) << shift;
  shift += sLengthBits;

  for (ValType argType : funcType.args()) {
    immediate |= EncodeImmediateValType(argType) << shift;
    shift += sTypeBits;
  }

  MOZ_ASSERT(shift <= sTotalBits);
  return immediate;
}

void FuncType::initImmediateTypeId() {
  if (!IsImmediateFuncType(*this)) {
    immediateTypeId_ = NO_IMMEDIATE_TYPE_ID;
    return;
  }
  immediateTypeId_ = EncodeImmediateFuncType(*this);
}

bool FuncType::canHaveJitEntry() const {
  return !hasUnexposableArgOrRet() &&
         !temporarilyUnsupportedReftypeForEntry() &&
         !temporarilyUnsupportedResultCountForJitEntry() &&
         JitOptions.enableWasmJitEntry;
}

bool FuncType::canHaveJitExit() const {
  return !hasUnexposableArgOrRet() && !temporarilyUnsupportedReftypeForExit() &&
         !hasInt64Arg() && !temporarilyUnsupportedResultCountForJitExit() &&
         JitOptions.enableWasmJitExit;
}

size_t FuncType::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const {
  return args_.sizeOfExcludingThis(mallocSizeOf);
}

static inline CheckedInt32 RoundUpToAlignment(CheckedInt32 address,
                                              uint32_t align) {
  MOZ_ASSERT(IsPowerOfTwo(align));

  // Note: Be careful to order operators such that we first make the
  // value smaller and then larger, so that we don't get false
  // overflow errors due to (e.g.) adding `align` and then
  // subtracting `1` afterwards when merely adding `align-1` would
  // not have overflowed. Note that due to the nature of two's
  // complement representation, if `address` is already aligned,
  // then adding `align-1` cannot itself cause an overflow.

  return ((address + (align - 1)) / align) * align;
}

CheckedInt32 StructLayout::addField(FieldType type) {
  uint32_t fieldSize = type.size();
  uint32_t fieldAlignment = type.alignmentInStruct();

  // We have to ensure that `offset` is chosen so that no field crosses the
  // inline/outline boundary.  The assertions here ensure that.  See comment
  // on `class StructLayout` for background.
  MOZ_ASSERT(fieldSize >= 1 && fieldSize <= 16);
  MOZ_ASSERT((fieldSize & (fieldSize - 1)) == 0);  // is a power of 2
  MOZ_ASSERT(fieldAlignment == fieldSize);         // is naturally aligned

  // Alignment of the struct is the max of the alignment of its fields.
  structAlignment = std::max(structAlignment, fieldAlignment);

  // Align the pointer.
  CheckedInt32 offset = RoundUpToAlignment(sizeSoFar, fieldAlignment);
  if (!offset.isValid()) {
    return offset;
  }

  // Allocate space.
  sizeSoFar = offset + fieldSize;
  if (!sizeSoFar.isValid()) {
    return sizeSoFar;
  }

  // The following should hold if the three assertions above hold.
  MOZ_ASSERT(offset / 16 == (offset + fieldSize - 1) / 16);
  return offset;
}

CheckedInt32 StructLayout::close() {
  return RoundUpToAlignment(sizeSoFar, structAlignment);
}

bool StructType::init() {
  StructLayout layout;
  for (StructField& field : fields_) {
    CheckedInt32 offset = layout.addField(field.type);
    if (!offset.isValid()) {
      return false;
    }
    field.offset = offset.value();
  }

  CheckedInt32 size = layout.close();
  if (!size.isValid()) {
    return false;
  }
  size_ = size.value();

  return true;
}

size_t StructType::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const {
  return fields_.sizeOfExcludingThis(mallocSizeOf);
}

size_t ArrayType::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const {
  return 0;
}

size_t TypeDef::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const {
  switch (kind_) {
    case TypeDefKind::Struct: {
      return structType_.sizeOfExcludingThis(mallocSizeOf);
    }
    case TypeDefKind::Func: {
      return funcType_.sizeOfExcludingThis(mallocSizeOf);
    }
    case TypeDefKind::Array: {
      return arrayType_.sizeOfExcludingThis(mallocSizeOf);
    }
    case TypeDefKind::None: {
      return 0;
    }
    default:
      break;
  }
  MOZ_ASSERT_UNREACHABLE();
  return 0;
}

struct RecGroupHashPolicy {
  using Lookup = const SharedRecGroup&;

  static HashNumber hash(Lookup lookup) { return lookup->hash(); }

  static bool match(const SharedRecGroup& lhs, Lookup rhs) {
    return RecGroup::matches(*rhs, *lhs);
  }
};

// A global hash set of recursion groups for use in fast type equality checks.
class TypeIdSet {
  using Set = HashSet<SharedRecGroup, RecGroupHashPolicy, SystemAllocPolicy>;
  Set set_;

 public:
  ~TypeIdSet() {
    // We should clean out all dead entries deterministically before shutdown.
    MOZ_ASSERT_IF(!JSRuntime::hasLiveRuntimes(), set_.empty());
  }

  // Attempt to insert a recursion group into the set, returning an existing
  // recursion group if there was one.
  SharedRecGroup insert(SharedRecGroup recGroup) {
    Set::AddPtr p = set_.lookupForAdd(recGroup);
    if (p) {
      // A canonical recursion group already existed, return it.
      return *p;
    }

    // Insert this recursion group into the set, and return it as the canonical
    // recursion group instance.
    if (!set_.add(p, recGroup)) {
      return nullptr;
    }
    return recGroup;
  }

  // Release the provided recursion group reference and remove it from the
  // canonical set if it was the last reference. This is one unified method
  // because we need to perform the lookup before releasing the reference, but
  // need to release the reference in order to see if it was the last reference
  // outside the canonical set.
  void clearRecGroup(SharedRecGroup* recGroupCell) {
    if (Set::Ptr p = set_.lookup(*recGroupCell)) {
      *recGroupCell = nullptr;
      if ((*p)->hasOneRef()) {
        set_.remove(p);
      }
    } else {
      *recGroupCell = nullptr;
    }
  }
};

ExclusiveData<TypeIdSet> typeIdSet(mutexid::WasmTypeIdSet);

SharedRecGroup TypeContext::canonicalizeGroup(SharedRecGroup recGroup) {
  ExclusiveData<TypeIdSet>::Guard locked = typeIdSet.lock();
  return locked->insert(recGroup);
}

TypeContext::~TypeContext() {
  ExclusiveData<TypeIdSet>::Guard locked = typeIdSet.lock();

  // Clear out the recursion groups in this module, freeing them from the
  // canonical type set if needed.
  //
  // We iterate backwards here so that we free every previous recursion group
  // that may be referring to the current recursion group we're freeing. This
  // is possible due to recursion groups being ordered.
  for (int32_t groupIndex = recGroups_.length() - 1; groupIndex >= 0;
       groupIndex--) {
    // Try to remove this entry from the canonical set if we have the last
    // strong reference. The entry may not exist if canonicalization failed
    // and this type context was aborted. This will clear the reference in the
    // vector.
    locked->clearRecGroup(&recGroups_[groupIndex]);
  }
}
