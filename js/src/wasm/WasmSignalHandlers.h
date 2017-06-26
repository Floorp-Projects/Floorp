/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright 2014 Mozilla Foundation
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

#ifndef wasm_signal_handlers_h
#define wasm_signal_handlers_h

#include "mozilla/Attributes.h"

#if defined(XP_DARWIN)
# include <mach/mach.h>
#endif
#include "threading/Thread.h"

struct JSContext;
struct JSRuntime;

namespace js {

// Force any currently-executing asm.js/ion code to call HandleExecutionInterrupt.
extern void
InterruptRunningJitCode(JSContext* cx);

class WasmActivation;

namespace wasm {

// Ensure the given JSRuntime is set up to use signals. Failure to enable signal
// handlers indicates some catastrophic failure and creation of the runtime must
// fail.
MOZ_MUST_USE bool
EnsureSignalHandlers(JSContext* cx);

// Return whether signals can be used in this process for interrupts or
// asm.js/wasm out-of-bounds.
bool
HaveSignalHandlers();

class CodeSegment;

// Returns true if wasm code is on top of the activation stack (and fills out
// the code segment outparam in this case), or false otherwise.
bool
InInterruptibleCode(JSContext* cx, uint8_t* pc, const CodeSegment** cs);

#if defined(XP_DARWIN)
// On OSX we are forced to use the lower-level Mach exception mechanism instead
// of Unix signals. Mach exceptions are not handled on the victim's stack but
// rather require an extra thread. For simplicity, we create one such thread
// per JSContext (upon the first use of wasm in the JSContext). This thread
// and related resources are owned by AsmJSMachExceptionHandler which is owned
// by JSContext.
class MachExceptionHandler
{
    bool installed_;
    js::Thread thread_;
    mach_port_t port_;

    void uninstall();

  public:
    MachExceptionHandler();
    ~MachExceptionHandler() { uninstall(); }
    mach_port_t port() const { return port_; }
    bool installed() const { return installed_; }
    bool install(JSContext* cx);
};
#endif

} // namespace wasm
} // namespace js

#endif // wasm_signal_handlers_h
