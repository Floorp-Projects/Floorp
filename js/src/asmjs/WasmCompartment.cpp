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

#include "asmjs/WasmCompartment.h"

#include "jscompartment.h"

#include "asmjs/WasmInstance.h"

#include "vm/Debugger-inl.h"

using namespace js;
using namespace wasm;

Compartment::Compartment(Zone* zone)
  : instances_(zone, InstanceObjectSet()),
    activationCount_(0),
    profilingEnabled_(false)
{
}

Compartment::~Compartment()
{
    MOZ_ASSERT(activationCount_ == 0);
    MOZ_ASSERT(!instances_.initialized() || instances_.empty());
}

bool
Compartment::registerInstance(JSContext* cx, HandleWasmInstanceObject instanceObj)
{
    if (!instanceObj->instance().ensureProfilingState(cx, profilingEnabled_))
        return false;

    if (!instances_.initialized() && !instances_.init()) {
        ReportOutOfMemory(cx);
        return false;
    }

    if (!instances_.putNew(instanceObj)) {
        ReportOutOfMemory(cx);
        return false;
    }

    Debugger::onNewWasmInstance(cx, instanceObj);
    return true;
}

bool
Compartment::ensureProfilingState(JSContext* cx)
{
    bool newProfilingEnabled = cx->spsProfiler.enabled();
    if (profilingEnabled_ == newProfilingEnabled)
        return true;

    // Since one Instance can call another Instance in the same compartment
    // directly without calling through Instance::callExport(), when profiling
    // is enabled, enable it for the entire compartment at once. It is only safe
    // to enable profiling when the wasm is not on the stack, so delay enabling
    // profiling until there are no live WasmActivations in this compartment.

    if (activationCount_ > 0)
        return true;

    for (InstanceObjectSet::Range r = instances_.all(); !r.empty(); r.popFront()) {
        if (!r.front()->instance().ensureProfilingState(cx, newProfilingEnabled))
            return false;
    }

    profilingEnabled_ = newProfilingEnabled;
    return true;
}

void
Compartment::addSizeOfExcludingThis(MallocSizeOf mallocSizeOf, size_t* compartmentTables)
{
    *compartmentTables += instances_.sizeOfExcludingThis(mallocSizeOf);
}
