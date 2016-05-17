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

#if defined(XP_DARWIN) && defined(ASMJS_MAY_USE_SIGNAL_HANDLERS)
# include <mach/mach.h>
# include "jslock.h"
#endif

struct JSRuntime;

namespace js {

// Force any currently-executing asm.js/ion code to call HandleExecutionInterrupt.
extern void
InterruptRunningJitCode(JSRuntime* rt);

namespace wasm {

// Set up any signal/exception handlers needed to execute code in the given
// runtime. Return whether runtime can:
//  - rely on fault handler support for avoiding asm.js heap bounds checks
//  - rely on InterruptRunningJitCode to halt running Ion/asm.js from any thread
MOZ_MUST_USE bool
EnsureSignalHandlersInstalled(JSRuntime* rt);

#if defined(XP_DARWIN) && defined(ASMJS_MAY_USE_SIGNAL_HANDLERS)
// On OSX we are forced to use the lower-level Mach exception mechanism instead
// of Unix signals. Mach exceptions are not handled on the victim's stack but
// rather require an extra thread. For simplicity, we create one such thread
// per JSRuntime (upon the first use of asm.js in the JSRuntime). This thread
// and related resources are owned by AsmJSMachExceptionHandler which is owned
// by JSRuntime.
class MachExceptionHandler
{
    bool installed_;
    PRThread* thread_;
    mach_port_t port_;

    void uninstall();

  public:
    MachExceptionHandler();
    ~MachExceptionHandler() { uninstall(); }
    mach_port_t port() const { return port_; }
    bool installed() const { return installed_; }
    bool install(JSRuntime* rt);
};
#endif

// Test whether the given PC is within the innermost wasm activation. Return
// false if it is not, or it cannot be determined.
bool IsPCInWasmCode(void *pc);

} // namespace wasm
} // namespace js

#endif // wasm_signal_handlers_h
