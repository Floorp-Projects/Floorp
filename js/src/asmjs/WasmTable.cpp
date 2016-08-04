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

using namespace js;
using namespace js::wasm;

/* static */ SharedTable
Table::create(JSContext* cx, TableKind kind, uint32_t length)
{
    SharedTable table = cx->new_<Table>();
    if (!table)
        return nullptr;

    table->array_.reset(cx->pod_calloc<void*>(length));
    if (!table->array_)
        return nullptr;

    table->kind_ = kind;
    table->length_ = length;
    table->initialized_ = false;
    return table;
}

void
Table::init(const CodeSegment& codeSegment)
{
    MOZ_ASSERT(!initialized());

    for (uint32_t i = 0; i < length_; i++) {
        MOZ_ASSERT(!array_.get()[i]);
        array_.get()[i] = codeSegment.badIndirectCallCode();
    }

    initialized_ = true;
}

size_t
Table::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return mallocSizeOf(array_.get());
}
