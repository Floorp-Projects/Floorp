/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vtune_vtunewrapper_h
#define vtune_vtunewrapper_h

#ifdef MOZ_VTUNE

#  include "vtune/jitprofiling.h"

#  include "jit/JitCode.h"
#  include "vm/JSScript.h"

namespace js {
namespace vtune {

bool Initialize();
void Shutdown();

// VTune profiling may be attached/detached at any time, but there is no API for
// attaching a callback to execute at attachment time. Methods compiled before
// VTune was most recently attached therefore do not provide any information to
// VTune.
bool IsProfilingActive();

// Wrapper exists in case we need locking in the future.
uint32_t GenerateUniqueMethodID();

void MarkStub(const js::jit::JitCode* code, const char* name);

void MarkRegExp(const js::jit::JitCode* code, bool match_only);

void MarkScript(const js::jit::JitCode* code, JSScript* script,
                const char* module);

void MarkWasm(unsigned methodId, const char* name, void* start, uintptr_t size);

void UnmarkCode(const js::jit::JitCode* code);

void UnmarkBytes(void* bytes, unsigned size);

}  // namespace vtune
}  // namespace js

#endif  // MOZ_VTUNE

#endif  // vtune_vtunewrapper_h
