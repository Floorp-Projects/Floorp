/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vtune_vtunewrapper_h
#define vtunewrapper_h

#ifdef MOZ_VTUNE

#include "vtune/jitprofiling.h"

#include "jsscript.h"

#include "jit/IonCode.h"

// VTune profiling may be attached/detached at any time, but there is no API for
// attaching a callback to execute at attachment time. Methods compiled before
// VTune was most recently attached therefore do not provide any information to VTune.
inline bool
IsVTuneProfilingActive()
{
    return iJIT_IsProfilingActive() == iJIT_SAMPLING_ON;
}

// Wrapper exists in case we need locking in the future.
uint32_t VTuneGenerateUniqueMethodID();

void VTuneMarkStub(const js::jit::JitCode* code, const char* name);

void VTuneMarkRegExp(const js::jit::JitCode* code, bool match_only);

void VTuneMarkScript(const js::jit::JitCode* code,
                     const JSScript* script,
                     const char* module);

void VTuneUnloadCode(const js::jit::JitCode* code);

#endif // MOZ_VTUNE

#endif // vtunewrapper_h
