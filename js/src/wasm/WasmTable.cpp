/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Copyright 2016 Mozilla Foundation
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

#include "wasm/WasmTable.h"

#include "mozilla/CheckedInt.h"
#include "mozilla/PodOperations.h"

#include "vm/JSContext.h"
#include "vm/Realm.h"
#include "wasm/WasmInstance.h"
#include "wasm/WasmJS.h"

using namespace js;
using namespace js::wasm;
using mozilla::CheckedInt;
using mozilla::PodZero;

Table::Table(JSContext* cx, const TableDesc& desc,
             HandleWasmTableObject maybeObject, UniqueCodePtrArray codePtrs,
             UniqueTlsPtrArray tlsPtrs)
    : maybeObject_(maybeObject),
      observers_(cx->zone()),
      codePtrs_(std::move(codePtrs)),
      tlsPtrs_(std::move(tlsPtrs)),
      elemType_(desc.elemType),
      isAsmJS_(desc.isAsmJS),
      isPublic_(desc.isImportedOrExported),
      length_(desc.initialLength),
      maximum_(desc.maximumLength) {
  MOZ_ASSERT(repr() == TableRepr::Func);
}

Table::Table(JSContext* cx, const TableDesc& desc,
             HandleWasmTableObject maybeObject, TableAnyRefVector&& objects)
    : maybeObject_(maybeObject),
      observers_(cx->zone()),
      objects_(std::move(objects)),
      elemType_(desc.elemType),
      isAsmJS_(desc.isAsmJS),
      isPublic_(desc.isImportedOrExported),
      length_(desc.initialLength),
      maximum_(desc.maximumLength) {
  MOZ_ASSERT(repr() == TableRepr::Ref);
}

/* static */
SharedTable Table::create(JSContext* cx, const TableDesc& desc,
                          HandleWasmTableObject maybeObject) {
  // We don't support non-nullable references in tables yet.
  MOZ_RELEASE_ASSERT(desc.elemType.isNullable());

  switch (desc.elemType.tableRepr()) {
    case TableRepr::Func: {
      UniqueCodePtrArray codePtrs(cx->pod_calloc<void*>(desc.initialLength));
      if (!codePtrs) {
        return nullptr;
      }
      // In principle we don't need to allocate this for AsmJS, but making this
      // optimization would complicate all other code in this file.  So don't.
      UniqueTlsPtrArray tlsPtrs(cx->pod_calloc<TlsData*>(desc.initialLength));
      if (!tlsPtrs) {
        return nullptr;
      }
      return SharedTable(cx->new_<Table>(
          cx, desc, maybeObject, std::move(codePtrs), std::move(tlsPtrs)));
    }
    case TableRepr::Ref: {
      TableAnyRefVector objects;
      if (!objects.resize(desc.initialLength)) {
        ReportOutOfMemory(cx);
        return nullptr;
      }
      return SharedTable(
          cx->new_<Table>(cx, desc, maybeObject, std::move(objects)));
    }
  }
  MOZ_CRASH("switch is exhaustive");
}

void Table::tracePrivate(JSTracer* trc) {
  // If this table has a WasmTableObject, then this method is only called by
  // WasmTableObject's trace hook so maybeObject_ must already be marked.
  // TraceEdge is called so that the pointer can be updated during a moving
  // GC.
  TraceNullableEdge(trc, &maybeObject_, "wasm table object");

  switch (repr()) {
    case TableRepr::Func: {
      if (isAsmJS_) {
#ifdef DEBUG
        for (uint32_t i = 0; i < length_; i++) {
          MOZ_ASSERT(!tlsPtrs_[i]);
        }
#endif
        break;
      }

      for (uint32_t i = 0; i < length_; i++) {
        if (tlsPtrs_[i]) {
          tlsPtrs_[i]->instance->trace(trc);
        } else {
          MOZ_ASSERT(!codePtrs_[i]);
        }
      }
      break;
    }
    case TableRepr::Ref: {
      objects_.trace(trc);
      break;
    }
  }
}

void Table::trace(JSTracer* trc) {
  // The trace hook of WasmTableObject will call Table::tracePrivate at
  // which point we can mark the rest of the children. If there is no
  // WasmTableObject, call Table::tracePrivate directly. Redirecting through
  // the WasmTableObject avoids marking the entire Table on each incoming
  // edge (once per dependent Instance).
  if (maybeObject_) {
    TraceEdge(trc, &maybeObject_, "wasm table object");
  } else {
    tracePrivate(trc);
  }
}

uint8_t* Table::tlsElements() const {
  if (repr() == TableRepr::Ref) {
    return (uint8_t*)objects_.begin();
  }
  return (uint8_t*)codePtrs_.get();
}

bool Table::getFuncRef(JSContext* cx, uint32_t index,
                       MutableHandleFunction fun) const {
  MOZ_ASSERT(isFunction());

  void* const& codePtr = codePtrs_[index];
  TlsData* const& tlsPtr = tlsPtrs_[index];
  if (!codePtr) {
    fun.set(nullptr);
    return true;
  }

  Instance& instance = *tlsPtr->instance;
  const CodeRange* codeRange = instance.code().lookupIndirectStubRange(codePtr);
  if (!codeRange) {
    codeRange = instance.code().lookupFuncRange(codePtr);
  }
  MOZ_ASSERT(codeRange);

  // If the element is a wasm function imported from another
  // instance then to preserve the === function identity required by
  // the JS embedding spec, we must set the element to the
  // imported function's underlying CodeRange.funcCheckedCallEntry and
  // Instance so that future Table.get()s produce the same
  // function object as was imported.
  const Tier callerTier = instance.code().bestTier();
  JSFunction* callee = nullptr;
  Instance* calleeInstance = instance.getOriginalInstanceAndFunction(
      callerTier, codeRange->funcIndex(), &callee);
  RootedWasmInstanceObject calleeInstanceObj(cx, calleeInstance->object());
  uint32_t calleeFunctionIndex = codeRange->funcIndex();
  if (callee && (calleeInstance != &instance)) {
    const Tier calleeTier = calleeInstance->code().bestTier();
    calleeFunctionIndex =
        calleeInstanceObj->getExportedFunctionCodeRange(callee, calleeTier)
            .funcIndex();
  }
  return WasmInstanceObject::getExportedFunction(cx, calleeInstanceObj,
                                                 calleeFunctionIndex, fun);
}

void Table::setFuncRef(uint32_t index, void* code, const Instance* instance) {
  MOZ_ASSERT(isFunction());

  void*& codePtr = codePtrs_[index];
  TlsData*& tlsPtr = tlsPtrs_[index];
  if (tlsPtr) {
    gc::PreWriteBarrier(tlsPtr->instance->objectUnbarriered());
  }

  if (!isAsmJS_) {
    codePtr = code;
    tlsPtr = instance->tlsData();
    MOZ_ASSERT(tlsPtr->instance->objectUnbarriered()->isTenured(),
               "no postWriteBarrier (Table::set)");
  } else {
    codePtr = code;
    tlsPtr = nullptr;
  }
}

bool Table::fillFuncRef(Maybe<Tier> maybeTier, uint32_t index,
                        uint32_t fillCount, FuncRef ref, JSContext* cx) {
  MOZ_ASSERT(isFunction());

  if (ref.isNull()) {
    for (uint32_t i = index, end = index + fillCount; i != end; i++) {
      setNull(i);
    }
    return true;
  }

  RootedFunction fun(cx, ref.asJSFunction());
  MOZ_RELEASE_ASSERT(IsWasmExportedFunction(fun));

  RootedWasmInstanceObject instanceObj(cx,
                                       ExportedFunctionToInstanceObject(fun));
  uint32_t funcIndex = ExportedFunctionToFuncIndex(fun);

#ifdef DEBUG
  RootedFunction f(cx);
  MOZ_ASSERT(instanceObj->getExportedFunction(cx, instanceObj, funcIndex, &f));
  MOZ_ASSERT(fun == f);
#endif

  Instance& instance = instanceObj->instance();
  Tier tier = maybeTier ? maybeTier.value() : instance.code().bestTier();
  void* code = instance.ensureAndGetIndirectStub(tier, funcIndex);
  if (!code) {
    return false;
  }

  // Note, infallible beyond this point.

  for (uint32_t i = index, end = index + fillCount; i != end; i++) {
    setFuncRef(i, code, &instance);
  }

  return true;
}

#ifdef DEBUG
bool Table::isNull(uint32_t index) const {
  if (isFunction()) {
    MOZ_ASSERT_IF(!isAsmJS_, (tlsPtrs_[index] == nullptr) ==
                                 (codePtrs_[index] == nullptr));
    return codePtrs_[index] == nullptr;
  } else {
    return objects_[index] == nullptr;
  }
}
#endif

AnyRef Table::getAnyRef(uint32_t index) const {
  MOZ_ASSERT(!isFunction());
  // TODO/AnyRef-boxing: With boxed immediates and strings, the write barrier
  // is going to have to be more complicated.
  ASSERT_ANYREF_IS_JSOBJECT;
  return AnyRef::fromJSObject(objects_[index]);
}

void Table::fillAnyRef(uint32_t index, uint32_t fillCount, AnyRef ref) {
  MOZ_ASSERT(!isFunction());
  // TODO/AnyRef-boxing: With boxed immediates and strings, the write barrier
  // is going to have to be more complicated.
  ASSERT_ANYREF_IS_JSOBJECT;
  for (uint32_t i = index, end = index + fillCount; i != end; i++) {
    objects_[i] = ref.asJSObject();
  }
}

void Table::setNull(uint32_t index) {
  switch (repr()) {
    case TableRepr::Func: {
      void*& codePtr = codePtrs_[index];
      TlsData*& tlsPtr = tlsPtrs_[index];
      MOZ_RELEASE_ASSERT(!isAsmJS_);
      if (tlsPtr) {
        gc::PreWriteBarrier(tlsPtr->instance->objectUnbarriered());
      }

      codePtr = nullptr;
      tlsPtr = nullptr;
      break;
    }
    case TableRepr::Ref: {
      fillAnyRef(index, 1, AnyRef::null());
      break;
    }
  }
}

bool Table::copy(JSContext* cx, const Table& srcTable, uint32_t dstIndex,
                 uint32_t srcIndex) {
  MOZ_RELEASE_ASSERT(!srcTable.isAsmJS_);
  switch (repr()) {
    case TableRepr::Func: {
      MOZ_RELEASE_ASSERT(elemType().isFunc() && srcTable.elemType().isFunc());
      void*& dstCodePtr = codePtrs_[dstIndex];
      TlsData*& dstTlsPtr = tlsPtrs_[dstIndex];
      if (dstTlsPtr) {
        gc::PreWriteBarrier(dstTlsPtr->instance->objectUnbarriered());
      }

      if (isPublic() && !srcTable.isPublic()) {
        RootedFunction fun(cx);
        if (!srcTable.getFuncRef(cx, srcIndex, &fun)) {
          // OOM, pass it on
          return false;
        }
        if (!fillFuncRef(Nothing(), dstIndex, 1, FuncRef::fromJSFunction(fun),
                         cx)) {
          ReportOutOfMemory(cx);
          return false;
        }
      } else {
        dstCodePtr = srcTable.codePtrs_[srcIndex];
        dstTlsPtr = srcTable.tlsPtrs_[srcIndex];

        if (dstTlsPtr) {
          MOZ_ASSERT(dstCodePtr);
          MOZ_ASSERT(dstTlsPtr->instance->objectUnbarriered()->isTenured(),
                     "no postWriteBarrier (Table::copy)");
        } else {
          MOZ_ASSERT(!dstCodePtr);
        }
      }
      break;
    }
    case TableRepr::Ref: {
      switch (srcTable.repr()) {
        case TableRepr::Ref: {
          fillAnyRef(dstIndex, 1, srcTable.getAnyRef(srcIndex));
          break;
        }
        case TableRepr::Func: {
          MOZ_RELEASE_ASSERT(srcTable.elemType().isFunc());
          // Upcast.
          RootedFunction fun(cx);
          if (!srcTable.getFuncRef(cx, srcIndex, &fun)) {
            // OOM, so just pass it on.
            return false;
          }
          fillAnyRef(dstIndex, 1, AnyRef::fromJSObject(fun));
          break;
        }
      }
      break;
    }
  }
  return true;
}

uint32_t Table::grow(uint32_t delta) {
  // This isn't just an optimization: movingGrowable() assumes that
  // onMovingGrowTable does not fire when length == maximum.
  if (!delta) {
    return length_;
  }

  uint32_t oldLength = length_;

  CheckedInt<uint32_t> newLength = oldLength;
  newLength += delta;
  if (!newLength.isValid() || newLength.value() > MaxTableLength) {
    return -1;
  }

  if (maximum_ && newLength.value() > maximum_.value()) {
    return -1;
  }

  MOZ_ASSERT(movingGrowable());

  switch (repr()) {
    case TableRepr::Func: {
      MOZ_RELEASE_ASSERT(!isAsmJS_);
      // Note that realloc does not release the pointees on failure which is
      // exactly what we need here.
      //
      // Note that the first realloc can succeed but the second can OOM, leaving
      // the first array longer than the second.  This is OK, but be sure to
      // finish work on the first array before trying the second one.

      void** newCodePtrs =
          js_pod_realloc<void*>(codePtrs_.get(), length_, newLength.value());
      if (!newCodePtrs) {
        return -1;
      }
      (void)codePtrs_.release();
      codePtrs_.reset(newCodePtrs);
      PodZero(newCodePtrs + length_, delta);

      TlsData** newTlsPtrs =
          js_pod_realloc<TlsData*>(tlsPtrs_.get(), length_, newLength.value());
      if (!newTlsPtrs) {
        return -1;
      }
      (void)tlsPtrs_.release();
      tlsPtrs_.reset(newTlsPtrs);
      PodZero(newTlsPtrs + length_, delta);

      break;
    }
    case TableRepr::Ref: {
      if (!objects_.resize(newLength.value())) {
        return -1;
      }
      break;
    }
  }

  if (auto* object = maybeObject_.unbarrieredGet()) {
    RemoveCellMemory(object, gcMallocBytes(), MemoryUse::WasmTableTable);
  }

  length_ = newLength.value();

  if (auto* object = maybeObject_.unbarrieredGet()) {
    AddCellMemory(object, gcMallocBytes(), MemoryUse::WasmTableTable);
  }

  for (InstanceSet::Range r = observers_.all(); !r.empty(); r.popFront()) {
    r.front()->instance().onMovingGrowTable(this);
  }

  return oldLength;
}

bool Table::movingGrowable() const {
  return !maximum_ || length_ < maximum_.value();
}

bool Table::addMovingGrowObserver(JSContext* cx, WasmInstanceObject* instance) {
  MOZ_ASSERT(movingGrowable());

  // A table can be imported multiple times into an instance, but we only
  // register the instance as an observer once.

  if (!observers_.put(instance)) {
    ReportOutOfMemory(cx);
    return false;
  }

  return true;
}

size_t Table::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const {
  if (isFunction()) {
    return mallocSizeOf(codePtrs_.get()) + mallocSizeOf(tlsPtrs_.get());
  }
  return objects_.sizeOfExcludingThis(mallocSizeOf);
}

size_t Table::gcMallocBytes() const {
  size_t size = sizeof(*this);
  if (isFunction()) {
    size += length() * (sizeof(void*) + sizeof(TlsData*));
  } else {
    size += length() * sizeof(TableAnyRefVector::ElementType);
  }
  return size;
}
