/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_GeckoProfiler_inl_h
#define vm_GeckoProfiler_inl_h

#include "vm/GeckoProfiler.h"

#include "vm/Runtime.h"

namespace js {

/*
 * This class is used to suppress profiler sampling during
 * critical sections where stack state is not valid.
 */
class MOZ_RAII AutoSuppressProfilerSampling
{
  public:
    explicit AutoSuppressProfilerSampling(JSContext* cx MOZ_GUARD_OBJECT_NOTIFIER_PARAM);

    ~AutoSuppressProfilerSampling();

  private:
    JSContext* cx_;
    bool previouslyEnabled_;
    JSRuntime::AutoProhibitActiveContextChange prohibitContextChange_;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

} // namespace js

#endif // vm_GeckoProfiler_inl_h
