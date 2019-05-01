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

#include "vm/JSContext.h"
#include "vm/Realm.h"
#include "wasm/WasmInstance.h"
#include "wasm/WasmJS.h"

using namespace js;
using namespace js::wasm;
using mozilla::CheckedInt;

Table::Table(JSContext* cx, const TableDesc& desc,
             HandleWasmTableObject maybeObject, UniqueFuncRefArray functions)
    : maybeObject_(maybeObject),
      observers_(cx->zone()),
      functions_(std::move(functions)),
      kind_(desc.kind),
      length_(desc.limits.initial),
      maximum_(desc.limits.maximum) {
  MOZ_ASSERT(kind_ != TableKind::AnyRef);
}

Table::Table(JSContext* cx, const TableDesc& desc,
             HandleWasmTableObject maybeObject, TableAnyRefVector&& objects)
    : maybeObject_(maybeObject),
      observers_(cx->zone()),
      objects_(std::move(objects)),
      kind_(desc.kind),
      length_(desc.limits.initial),
      maximum_(desc.limits.maximum) {
  MOZ_ASSERT(kind_ == TableKind::AnyRef);
}

/* static */
SharedTable Table::create(JSContext* cx, const TableDesc& desc,
                          HandleWasmTableObject maybeObject) {
  switch (desc.kind) {
    case TableKind::FuncRef:
    case TableKind::AsmJS: {
      UniqueFuncRefArray functions(
          cx->pod_calloc<FunctionTableElem>(desc.limits.initial));
      if (!functions) {
        return nullptr;
      }
      return SharedTable(
          cx->new_<Table>(cx, desc, maybeObject, std::move(functions)));
    }
    case TableKind::AnyRef: {
      TableAnyRefVector objects;
      if (!objects.resize(desc.limits.initial)) {
        return nullptr;
      }
      return SharedTable(
          cx->new_<Table>(cx, desc, maybeObject, std::move(objects)));
    }
    default:
      MOZ_CRASH();
  }
}

void Table::tracePrivate(JSTracer* trc) {
  // If this table has a WasmTableObject, then this method is only called by
  // WasmTableObject's trace hook so maybeObject_ must already be marked.
  // TraceEdge is called so that the pointer can be updated during a moving
  // GC. TraceWeakEdge may sound better, but it is less efficient given that
  // we know object_ is already marked.
  if (maybeObject_) {
    MOZ_ASSERT(!gc::IsAboutToBeFinalized(&maybeObject_));
    TraceEdge(trc, &maybeObject_, "wasm table object");
  }

  switch (kind_) {
    case TableKind::FuncRef: {
      for (uint32_t i = 0; i < length_; i++) {
        if (functions_[i].tls) {
          functions_[i].tls->instance->trace(trc);
        } else {
          MOZ_ASSERT(!functions_[i].code);
        }
      }
      break;
    }
    case TableKind::AnyRef: {
      objects_.trace(trc);
      break;
    }
    case TableKind::AsmJS: {
#ifdef DEBUG
      for (uint32_t i = 0; i < length_; i++) {
        MOZ_ASSERT(!functions_[i].tls);
      }
#endif
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

uint8_t* Table::functionBase() const {
  if (kind() == TableKind::AnyRef) {
    return nullptr;
  }
  return (uint8_t*)functions_.get();
}

const FunctionTableElem& Table::getFuncRef(uint32_t index) const {
  MOZ_ASSERT(isFunction());
  return functions_[index];
}

AnyRef Table::getAnyRef(uint32_t index) const {
  MOZ_ASSERT(!isFunction());
  // TODO/AnyRef-boxing: With boxed immediates and strings, the write barrier
  // is going to have to be more complicated.
  ASSERT_ANYREF_IS_JSOBJECT;
  return AnyRef::fromJSObject(objects_[index]);
}

void Table::setFuncRef(uint32_t index, void* code, const Instance* instance) {
  MOZ_ASSERT(isFunction());

  FunctionTableElem& elem = functions_[index];
  if (elem.tls) {
    JSObject::writeBarrierPre(elem.tls->instance->objectUnbarriered());
  }

  switch (kind_) {
    case TableKind::FuncRef:
      elem.code = code;
      elem.tls = instance->tlsData();
      MOZ_ASSERT(elem.tls->instance->objectUnbarriered()->isTenured(),
                 "no writeBarrierPost (Table::set)");
      break;
    case TableKind::AsmJS:
      elem.code = code;
      elem.tls = nullptr;
      break;
    case TableKind::AnyRef:
      MOZ_CRASH("Bad table type");
  }
}

void Table::setAnyRef(uint32_t index, AnyRef new_obj) {
  MOZ_ASSERT(!isFunction());
  // TODO/AnyRef-boxing: With boxed immediates and strings, the write barrier
  // is going to have to be more complicated.
  ASSERT_ANYREF_IS_JSOBJECT;
  objects_[index] = new_obj.asJSObject();
}

void Table::setNull(uint32_t index) {
  switch (kind_) {
    case TableKind::FuncRef: {
      FunctionTableElem& elem = functions_[index];
      if (elem.tls) {
        JSObject::writeBarrierPre(elem.tls->instance->objectUnbarriered());
      }

      elem.code = nullptr;
      elem.tls = nullptr;
      break;
    }
    case TableKind::AnyRef: {
      setAnyRef(index, AnyRef::null());
      break;
    }
    case TableKind::AsmJS: {
      MOZ_CRASH("Should not happen");
    }
  }
}

void Table::copy(const Table& srcTable, uint32_t dstIndex, uint32_t srcIndex) {
  switch (kind_) {
    case TableKind::FuncRef: {
      FunctionTableElem& dst = functions_[dstIndex];
      if (dst.tls) {
        JSObject::writeBarrierPre(dst.tls->instance->objectUnbarriered());
      }

      FunctionTableElem& src = srcTable.functions_[srcIndex];
      dst.code = src.code;
      dst.tls = src.tls;

      if (dst.tls) {
        MOZ_ASSERT(dst.code);
        MOZ_ASSERT(dst.tls->instance->objectUnbarriered()->isTenured(),
                   "no writeBarrierPost (Table::copy)");
      } else {
        MOZ_ASSERT(!dst.code);
      }
      break;
    }
    case TableKind::AnyRef: {
      setAnyRef(dstIndex, srcTable.getAnyRef(srcIndex));
      break;
    }
    case TableKind::AsmJS: {
      MOZ_CRASH("Bad table type");
    }
  }
}

uint32_t Table::grow(uint32_t delta, JSContext* cx) {
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

  JSRuntime* rt =
      cx->runtime();  // Use JSRuntime's MallocProvider to avoid throwing.

  switch (kind_) {
    case TableKind::FuncRef: {
      // Note that realloc does not release functions_'s pointee on failure
      // which is exactly what we need here.
      FunctionTableElem* newFunctions = rt->pod_realloc<FunctionTableElem>(
          functions_.get(), length_, newLength.value());
      if (!newFunctions) {
        return -1;
      }
      Unused << functions_.release();
      functions_.reset(newFunctions);

      // Realloc does not zero the delta for us.
      PodZero(newFunctions + length_, delta);
      break;
    }
    case TableKind::AnyRef: {
      if (!objects_.resize(newLength.value())) {
        return -1;
      }
      break;
    }
    case TableKind::AsmJS: {
      MOZ_CRASH("Bad table type");
    }
  }

  length_ = newLength.value();

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
    return mallocSizeOf(functions_.get());
  }
  return objects_.sizeOfExcludingThis(mallocSizeOf);
}
