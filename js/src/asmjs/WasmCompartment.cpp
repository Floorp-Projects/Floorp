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
  : mutatingInstances_(false),
    activationCount_(0),
    profilingEnabled_(false)
{}

Compartment::~Compartment()
{
    MOZ_ASSERT(activationCount_ == 0);
    MOZ_ASSERT(instances_.empty());
    MOZ_ASSERT(!mutatingInstances_);
}

struct InstanceComparator
{
    const Instance& target;
    explicit InstanceComparator(const Instance& target) : target(target) {}

    int operator()(const Instance* instance) const {
        if (instance == &target)
            return 0;
        MOZ_ASSERT(!target.codeSegment().containsCodePC(instance->codeBase()));
        MOZ_ASSERT(!instance->codeSegment().containsCodePC(target.codeBase()));
        return target.codeBase() < instance->codeBase() ? -1 : 1;
    }
};

void
Compartment::trace(JSTracer* trc)
{
    // A WasmInstanceObject that was initially reachable when called can become
    // unreachable while executing on the stack. Since wasm does not otherwise
    // scan the stack during GC to identify live instances, we mark all instance
    // objects live if there is any running wasm in the compartment.
    if (activationCount_) {
        for (Instance* i : instances_)
            i->trace(trc);
    }
}

bool
Compartment::registerInstance(JSContext* cx, HandleWasmInstanceObject instanceObj)
{
    Instance& instance = instanceObj->instance();
    MOZ_ASSERT(this == &instance.compartment()->wasm);

    if (!instance.ensureProfilingState(cx, profilingEnabled_))
        return false;

    size_t index;
    if (BinarySearchIf(instances_, 0, instances_.length(), InstanceComparator(instance), &index))
        MOZ_CRASH("duplicate registration");

    {
        AutoMutateInstances guard(*this);
        if (!instances_.insert(instances_.begin() + index, &instance)) {
            ReportOutOfMemory(cx);
            return false;
        }
    }

    Debugger::onNewWasmInstance(cx, instanceObj);
    return true;
}

void
Compartment::unregisterInstance(Instance& instance)
{
    size_t index;
    if (!BinarySearchIf(instances_, 0, instances_.length(), InstanceComparator(instance), &index))
        return;

    AutoMutateInstances guard(*this);
    instances_.erase(instances_.begin() + index);
}

struct PCComparator
{
    const void* pc;
    explicit PCComparator(const void* pc) : pc(pc) {}

    int operator()(const Instance* instance) const {
        if (instance->codeSegment().containsCodePC(pc))
            return 0;
        return pc < instance->codeBase() ? -1 : 1;
    }
};

Code*
Compartment::lookupCode(const void* pc) const
{
    Instance* instance = lookupInstanceDeprecated(pc);
    return instance ? &instance->code() : nullptr;
}

Instance*
Compartment::lookupInstanceDeprecated(const void* pc) const
{
    // See profilingEnabled().
    MOZ_ASSERT(!mutatingInstances_);

    size_t index;
    if (!BinarySearchIf(instances_, 0, instances_.length(), PCComparator(pc), &index))
        return nullptr;

    return instances_[index];
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

    for (Instance* instance : instances_) {
        if (!instance->ensureProfilingState(cx, newProfilingEnabled))
            return false;
    }

    profilingEnabled_ = newProfilingEnabled;
    return true;
}

bool
Compartment::profilingEnabled() const
{
    // Profiling can asynchronously interrupt the mutation of the instances_
    // vector which is used by lookupCode() during stack-walking. To handle
    // this rare case, disable profiling during mutation.
    return profilingEnabled_ && !mutatingInstances_;
}

void
Compartment::addSizeOfExcludingThis(MallocSizeOf mallocSizeOf, size_t* compartmentTables)
{
    *compartmentTables += instances_.sizeOfExcludingThis(mallocSizeOf);
}
