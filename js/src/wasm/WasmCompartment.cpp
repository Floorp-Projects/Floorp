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

#include "wasm/WasmCompartment.h"

#include "jscompartment.h"

#include "wasm/WasmInstance.h"

#include "vm/Debugger-inl.h"

using namespace js;
using namespace wasm;

Compartment::Compartment(Zone* zone)
  : mutatingInstances_(false)
{}

Compartment::~Compartment()
{
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

        // Instances can share code, so the segments can be equal (though they
        // can't partially overlap).  If the codeBases are equal, we sort by
        // Instance address.  Thus a Code may map to many instances.

        // Compare by the first tier, always.

        Tier instanceTier = instance->code().stableTier();
        Tier targetTier = target.code().stableTier();

        if (instance->codeBase(instanceTier) == target.codeBase(targetTier))
            return instance < &target ? -1 : 1;

        return target.codeBase(targetTier) < instance->codeBase(instanceTier) ? -1 : 1;
    }
};

struct CodeSegmentPC
{
    const void* pc;

    explicit CodeSegmentPC(const void* pc) : pc(pc) {}
    int operator()(const CodeSegment* cs) const {
        if (cs->containsCodePC(pc))
            return 0;
        if (pc < cs->base())
            return -1;
        return 1;
    }
};

bool
Compartment::registerInstance(JSContext* cx, HandleWasmInstanceObject instanceObj)
{
    Instance& instance = instanceObj->instance();
    MOZ_ASSERT(this == &instance.compartment()->wasm);

    instance.ensureProfilingLabels(cx->runtime()->geckoProfiler().enabled());

    if (instance.debugEnabled() &&
        instance.compartment()->debuggerObservesAllExecution())
    {
        instance.ensureEnterFrameTrapsState(cx, true);
    }

    size_t index;
    if (BinarySearchIf(instances_, 0, instances_.length(), InstanceComparator(instance), &index))
        MOZ_CRASH("duplicate registration");

    {
        AutoMutateInstances guard(*this);
        if (!instances_.insert(instances_.begin() + index, &instance)) {
            ReportOutOfMemory(cx);
            return false;
        }

        const Code& code = instance.code();
        for (auto t : code.tiers()) {
            const CodeSegment& cs = code.segment(t);
            BinarySearchIf(codeSegments_, 0, codeSegments_.length(), CodeSegmentPC(cs.base()),
                           &index);
            if (!codeSegments_.insert(codeSegments_.begin() + index, &cs)) {
                ReportOutOfMemory(cx);
                return false;
            }
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
    const Code& code = instance.code();
    for (auto t : code.tiers()) {
        MOZ_ALWAYS_TRUE(BinarySearchIf(codeSegments_, 0, codeSegments_.length(),
                        CodeSegmentPC(code.segment(t).base()), &index));
        codeSegments_.erase(codeSegments_.begin() + index);
    }
}

const CodeSegment*
Compartment::lookupCodeSegment(const void* pc) const
{
    // lookupCodeSegment() can be called asynchronously from the interrupt signal
    // handler. In that case, the signal handler is just asking whether the pc
    // is in wasm code. If instances_ is being mutated then we can't be
    // executing wasm code so returning nullptr is fine.
    if (mutatingInstances_)
        return nullptr;

    size_t index;
    if (!BinarySearchIf(codeSegments_, 0, codeSegments_.length(), CodeSegmentPC(pc), &index))
        return nullptr;

    return codeSegments_[index];
}

const Code*
Compartment::lookupCode(const void* pc) const
{
    const CodeSegment* found = lookupCodeSegment(pc);
    return found ? found->code() : nullptr;
}

void
Compartment::ensureProfilingLabels(bool profilingEnabled)
{
    for (Instance* instance : instances_)
        instance->ensureProfilingLabels(profilingEnabled);
}

void
Compartment::addSizeOfExcludingThis(MallocSizeOf mallocSizeOf, size_t* compartmentTables)
{
    *compartmentTables += instances_.sizeOfExcludingThis(mallocSizeOf);
}
