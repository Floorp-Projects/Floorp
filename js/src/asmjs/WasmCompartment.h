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

#ifndef wasm_compartment_h
#define wasm_compartment_h

#include "asmjs/WasmJS.h"

namespace js {

class WasmActivation;

namespace wasm {

class Code;
typedef Vector<Instance*, 0, SystemAllocPolicy> InstanceVector;

// wasm::Compartment lives in JSCompartment and contains the wasm-related
// per-compartment state. wasm::Compartment tracks every live instance in the
// compartment and must be notified, via registerInstance(), of any new
// WasmInstanceObject.

class Compartment
{
    InstanceVector instances_;
    volatile bool  mutatingInstances_;
    size_t         activationCount_;
    bool           profilingEnabled_;

    friend class js::WasmActivation;

    struct AutoMutateInstances {
        Compartment &c;
        explicit AutoMutateInstances(Compartment& c) : c(c) {
            MOZ_ASSERT(!c.mutatingInstances_);
            c.mutatingInstances_ = true;
        }
        ~AutoMutateInstances() {
            MOZ_ASSERT(c.mutatingInstances_);
            c.mutatingInstances_ = false;
        }
    };

  public:
    explicit Compartment(Zone* zone);
    ~Compartment();
    void trace(JSTracer* trc);

    // Before a WasmInstanceObject can be considered fully constructed and
    // valid, it must be registered with the Compartment. If this method fails,
    // an error has been reported and the instance object must be abandoned.
    // After a successful registration, an Instance must call
    // unregisterInstance() before being destroyed.

    bool registerInstance(JSContext* cx, HandleWasmInstanceObject instanceObj);
    void unregisterInstance(Instance& instance);

    // Return a vector of all live instances in the compartment. The lifetime of
    // these Instances is determined by their owning WasmInstanceObject.
    // Note that accessing instances()[i]->object() triggers a read barrier
    // since instances() is effectively a weak list.

    const InstanceVector& instances() const { return instances_; }

    // This methods returns the wasm::Code containing the given pc, if any
    // exists in the compartment.

    Code* lookupCode(const void* pc) const;

    // Currently, there is one Code per Instance so it is also possible to
    // lookup a Instance given a pc. However, the goal is to share one Code
    // between multiple Instances at which point in time this method will be
    // removed.

    Instance* lookupInstanceDeprecated(const void* pc) const;

    // To ensure profiling is enabled (so that wasm frames are not lost in
    // profiling callstacks), ensureProfilingState must be called before calling
    // the first wasm function in a compartment.

    bool ensureProfilingState(JSContext* cx);
    bool profilingEnabled() const;

    // about:memory reporting

    void addSizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf, size_t* compartmentTables);
};

} // namespace wasm
} // namespace js

#endif // wasm_compartment_h
