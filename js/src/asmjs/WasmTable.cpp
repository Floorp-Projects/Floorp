/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
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

#include "asmjs/WasmTable.h"

#include "jscntxt.h"

#include "asmjs/WasmInstance.h"

using namespace js;
using namespace js::wasm;

/* static */ SharedTable
Table::create(JSContext* cx, const TableDesc& desc)
{
    SharedTable table = cx->new_<Table>();
    if (!table)
        return nullptr;

    // The raw element type of a Table depends on whether it is external: an
    // external table can contain functions from multiple instances and thus
    // must store an additional instance pointer in each element.
    void* array;
    if (desc.external)
        array = cx->pod_calloc<ExternalTableElem>(desc.initial);
    else
        array = cx->pod_calloc<void*>(desc.initial);
    if (!array)
        return nullptr;

    table->array_.reset((uint8_t*)array);
    table->kind_ = desc.kind;
    table->length_ = desc.initial;
    table->initialized_ = false;
    table->external_ = desc.external;
    return table;
}

void
Table::init(Instance& instance)
{
    MOZ_ASSERT(!initialized());
    initialized_ = true;

    void* code = instance.codeSegment().badIndirectCallCode();
    if (external_) {
        ExternalTableElem* array = externalArray();
        TlsData* tls = &instance.tlsData();
        for (uint32_t i = 0; i < length_; i++) {
            array[i].code = code;
            array[i].tls = tls;
        }
    } else {
        void** array = internalArray();
        for (uint32_t i = 0; i < length_; i++)
            array[i] = code;
    }
}

void
Table::trace(JSTracer* trc)
{
    if (!initialized_ || !external_)
        return;

    ExternalTableElem* array = externalArray();
    for (uint32_t i = 0; i < length_; i++)
        array[i].tls->instance->trace(trc);
}

void**
Table::internalArray() const
{
    MOZ_ASSERT(initialized_);
    MOZ_ASSERT(!external_);
    return (void**)array_.get();
}

ExternalTableElem*
Table::externalArray() const
{
    MOZ_ASSERT(initialized_);
    MOZ_ASSERT(external_);
    return (ExternalTableElem*)array_.get();
}

void
Table::set(uint32_t index, void* code, Instance& instance)
{
    if (external_) {
        ExternalTableElem& elem = externalArray()[index];
        elem.code = code;
        elem.tls = &instance.tlsData();
    } else {
        internalArray()[index] = code;
    }
}

void
Table::setNull(uint32_t index)
{
    // Only external tables can set elements to null after initialization.
    ExternalTableElem& elem = externalArray()[index];
    elem.code = elem.tls->instance->codeSegment().badIndirectCallCode();
}

size_t
Table::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return mallocSizeOf(array_.get());
}
