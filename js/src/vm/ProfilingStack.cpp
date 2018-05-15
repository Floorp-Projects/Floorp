/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/ProfilingStack.h"

#include "mozilla/IntegerRange.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/UniquePtrExtensions.h"

#include <algorithm>

using namespace js;

ProfilingStack::~ProfilingStack()
{
    // The label macros keep a reference to the ProfilingStack to avoid a TLS
    // access. If these are somehow not all cleared we will get a
    // use-after-free so better to crash now.
    MOZ_RELEASE_ASSERT(stackPointer == 0);

    delete[] frames;
}

bool
ProfilingStack::ensureCapacitySlow()
{
    MOZ_ASSERT(stackPointer >= capacity);
    const uint32_t kInitialCapacity = 128;

    uint32_t sp = stackPointer;
    auto newCapacity = std::max(sp + 1,  capacity ? capacity * 2 : kInitialCapacity);

    auto* newFrames =
        new (mozilla::fallible) js::ProfilingStackFrame[newCapacity];
    if (MOZ_UNLIKELY(!newFrames))
        return false;

    // It's important that `frames` / `capacity` / `stackPointer` remain consistent here at
    // all times.
    for (auto i : mozilla::IntegerRange(capacity))
        newFrames[i] = frames[i];

    js::ProfilingStackFrame* oldFrames = frames;
    frames = newFrames;
    capacity = newCapacity;
    delete[] oldFrames;

    return true;
}
