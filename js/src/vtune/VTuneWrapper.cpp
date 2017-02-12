/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vtune/VTuneWrapper.h"

#include "mozilla/Sprintf.h"

#include "jscntxt.h"
#include "jscompartment.h"
#include "jsgc.h"

#include "vm/Shape.h"
#include "wasm/WasmCode.h"

#ifdef MOZ_VTUNE

namespace js {
namespace vtune {

uint32_t
GenerateUniqueMethodID()
{
    return (uint32_t)iJIT_GetNewMethodID();
}


// Stubs and trampolines are created on engine initialization and are never unloaded.
void
MarkStub(const js::jit::JitCode* code, const char* name)
{
    if (!IsProfilingActive())
        return;

    iJIT_Method_Load_V2 method = {0};
    method.method_id = GenerateUniqueMethodID();
    method.method_name = const_cast<char*>(name);
    method.method_load_address = code->raw();
    method.method_size = code->instructionsSize();
    method.module_name = const_cast<char*>("jitstubs");

    iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED_V2, (void*)&method);
}


void
MarkRegExp(const js::jit::JitCode* code, bool match_only)
{
    if (!IsProfilingActive())
        return;

    iJIT_Method_Load_V2 method = {0};
    method.method_id = GenerateUniqueMethodID();
    method.method_load_address = code->raw();
    method.method_size = code->instructionsSize();

    if (match_only)
        method.method_name = const_cast<char*>("regexp (match-only)");
    else
        method.method_name = const_cast<char*>("regexp (normal)");

    method.module_name = const_cast<char*>("irregexp");

    int ok = iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED_V2, (void*)&method);
    if (ok != 1)
        printf("[!] VTune Integration: Failed to load method.\n");
}


void
MarkScript(const js::jit::JitCode* code, const JSScript* script, const char* module)
{
    if (!IsProfilingActive())
        return;

    iJIT_Method_Load_V2 method = {0};
    method.method_id = script->vtuneMethodID();
    method.method_load_address = code->raw();
    method.method_size = code->instructionsSize();
    method.module_name = const_cast<char*>(module);

    // Line numbers begin at 1, but columns begin at 0.
    // Text editors start at 1,1 so fixup is performed to match.
    char namebuf[512];
    SprintfLiteral(namebuf, "%s:%zu:%zu",
                   script->filename(), script->lineno(), script->column() + 1);

    method.method_name = &namebuf[0];

    int ok = iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED_V2, (void*)&method);
    if (ok != 1)
        printf("[!] VTune Integration: Failed to load method.\n");
}


void
MarkWasm(const js::wasm::CodeSegment& cs, const char* name, void* start, uintptr_t size)
{
    if (!IsProfilingActive())
        return;

    iJIT_Method_Load_V2 method = {0};
    method.method_id = cs.vtune_method_id_;
    method.method_name = const_cast<char*>(name);
    method.method_load_address = start;
    method.method_size = (unsigned)size;
    method.module_name = const_cast<char*>("wasm");

    int ok = iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED_V2, (void*)&method);
    if (ok != 1)
        printf("[!] VTune Integration: Failed to load method.\n");
}


void
UnmarkCode(const js::jit::JitCode* code)
{
    UnmarkBytes(code->raw(), (unsigned)code->instructionsSize());
}


void
UnmarkBytes(void* bytes, unsigned size)
{
    if (!IsProfilingActive())
        return;

    // It appears that the method_id is not required for unloading.
    iJIT_Method_Load method = {0};
    method.method_load_address = bytes;
    method.method_size = size;

    // The iJVM_EVENT_TYPE_METHOD_UNLOAD_START event is undocumented.
    // VTune appears to happily accept unload events even for untracked JitCode.
    int ok = iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_UNLOAD_START, (void*)&method);

    // Assertions aren't reported in VTune: instead, they immediately end profiling
    // with no warning that a crash occurred. This can generate misleading profiles.
    // So instead, print out a message to stdout (which VTune does not redirect).
    if (ok != 1)
        printf("[!] VTune Integration: Failed to unload method.\n");
}

} // namespace vtune
} // namespace js

#endif // MOZ_VTUNE
