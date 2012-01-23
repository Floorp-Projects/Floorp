/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsgc_memory_h___
#define jsgc_memory_h___

#include <stddef.h>
#include "jsgc.h"

namespace js {
namespace gc {

/*
 * Sanity check that our compiled configuration matches the currently running
 * instance and initialize any runtime data needed for allocation.
 */
void InitMemorySubsystem();

void *MapAlignedPages(size_t size, size_t alignment);
void UnmapPages(void *p, size_t size);

bool MarkPagesUnused(void *p, size_t size);
bool MarkPagesInUse(void *p, size_t size);

} /* namespace gc */
} /* namespace js */

#endif /* jsgc_memory_h___ */
