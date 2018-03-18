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

PseudoStack::~PseudoStack()
{
    // The label macros keep a reference to the PseudoStack to avoid a TLS
    // access. If these are somehow not all cleared we will get a
    // use-after-free so better to crash now.
    MOZ_RELEASE_ASSERT(stackPointer == 0);

    delete[] entries;
}

bool
PseudoStack::ensureCapacitySlow()
{
    MOZ_ASSERT(stackPointer >= entryCapacity);
    const uint32_t kInitialCapacity = 128;

    uint32_t sp = stackPointer;
    auto newCapacity = std::max(sp + 1,  entryCapacity ? entryCapacity * 2 : kInitialCapacity);

    auto* newEntries =
        new (mozilla::fallible) js::ProfileEntry[newCapacity];
    if (MOZ_UNLIKELY(!newEntries))
        return false;

    // It's important that `entries` / `entryCapacity` / `stackPointer` remain consistent here at
    // all times.
    for (auto i : mozilla::IntegerRange(entryCapacity))
        newEntries[i] = entries[i];

    js::ProfileEntry* oldEntries = entries;
    entries = newEntries;
    entryCapacity = newCapacity;
    delete[] oldEntries;

    return true;
}
