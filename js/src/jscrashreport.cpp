/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "jsapi.h"
#include "jscntxt.h"
#include "jscrashreport.h"
#include "jscrashformat.h"

#include <time.h>

namespace js {
namespace crash {

const static int stack_snapshot_max_size = 32768;

#if defined(XP_WIN)

#include <windows.h>

static bool
GetStack(uint64 *stack, uint64 *stack_len, CrashRegisters *regs, char *buffer, size_t size)
{
    /* Try to figure out how big the stack is. */
    char dummy;
    MEMORY_BASIC_INFORMATION info;
    if (VirtualQuery(reinterpret_cast<LPCVOID>(&dummy), &info, sizeof(info)) == 0)
        return false;
    if (info.State != MEM_COMMIT)
        return false;

    /* 256 is a fudge factor to account for the rest of GetStack's frame. */
    uint64 p = uint64(&dummy) - 256;
    uint64 len = stack_snapshot_max_size;

    if (p + len > uint64(info.BaseAddress) + info.RegionSize)
        len = uint64(info.BaseAddress) + info.RegionSize - p;

    if (len > size)
        len = size;

    *stack = p;
    *stack_len = len;

    /* Get the register state. */
#if defined(_MSC_VER) && JS_BITS_PER_WORD == 32
    /* ASM version for win2k that doesn't support RtlCaptureContext */
    uint32 vip, vsp, vbp;
    __asm {
    Label:
        mov [vbp], ebp;
        mov [vsp], esp;
        mov eax, [Label];
        mov [vip], eax;
    }
    regs->ip = vip;
    regs->sp = vsp;
    regs->bp = vbp;
#else
    CONTEXT context;
    RtlCaptureContext(&context);
#if JS_BITS_PER_WORD == 32
    regs->ip = context.Eip;
    regs->sp = context.Esp;
    regs->bp = context.Ebp;
#else
    regs->ip = context.Rip;
    regs->sp = context.Rsp;
    regs->bp = context.Rbp;
#endif
#endif

    memcpy(buffer, (void *)p, len);

    return true;
}

#elif 0

#include <unistd.h>
#include <ucontext.h>
#include <sys/mman.h>

static bool
GetStack(uint64 *stack, uint64 *stack_len, CrashRegisters *regs, char *buffer, size_t size)
{
    /* 256 is a fudge factor to account for the rest of GetStack's frame. */
    char dummy;
    uint64 p = uint64(&dummy) - 256;
    uint64 pgsz = getpagesize();
    uint64 len = stack_snapshot_max_size;
    p &= ~(pgsz - 1);

    /* Try to figure out how big the stack is. */
    while (len > 0) {
	if (mlock((const void *)p, len) == 0) {
	    munlock((const void *)p, len);
	    break;
	}
	len -= pgsz;
    }

    if (len > size)
        len = size;

    *stack = p;
    *stack_len = len;

    /* Get the register state. */
    ucontext_t context;
    if (getcontext(&context) != 0)
	return false;

#if JS_BITS_PER_WORD == 64
    regs->sp = (uint64)context.uc_mcontext.gregs[REG_RSP];
    regs->bp = (uint64)context.uc_mcontext.gregs[REG_RBP];
    regs->ip = (uint64)context.uc_mcontext.gregs[REG_RIP];
#elif JS_BITS_PER_WORD == 32
    regs->sp = (uint64)context.uc_mcontext.gregs[REG_ESP];
    regs->bp = (uint64)context.uc_mcontext.gregs[REG_EBP];
    regs->ip = (uint64)context.uc_mcontext.gregs[REG_EIP];
#endif

    memcpy(buffer, (void *)p, len);

    return true;
}

#else

static bool
GetStack(uint64 *stack, uint64 *stack_len, CrashRegisters *regs, char *buffer, size_t size)
{
    return false;
}

#endif

class Stack : private CrashStack
{
public:
    Stack(uint64 id);

    bool snapshot();
};

Stack::Stack(uint64 id)
  : CrashStack(id)
{
}

bool
Stack::snapshot()
{
    snaptime = time(NULL);
    return GetStack(&stack_base, &stack_len, &regs, stack, sizeof(stack));
}

class Ring : private CrashRing
{
public:
    Ring(uint64 id);

    void push(uint64 tag, void *data, size_t size);

private:
    size_t bufferSize() { return crash_buffer_size; }
    void copyBytes(void *data, size_t size);
};

Ring::Ring(uint64 id)
  : CrashRing(id)
{
}

void
Ring::push(uint64 tag, void *data, size_t size)
{
    uint64 t = time(NULL);

    copyBytes(&tag, sizeof(uint64));
    copyBytes(&t, sizeof(uint64));
    copyBytes(data, size);
    uint64 mysize = size;
    copyBytes(&mysize, sizeof(uint64));
}

void
Ring::copyBytes(void *data, size_t size)
{
    if (size >= bufferSize())
        size = bufferSize();

    if (offset + size > bufferSize()) {
        size_t first = bufferSize() - offset;
        size_t second = size - first;
        memcpy(&buffer[offset], data, first);
        memcpy(buffer, (char *)data + first, second);
        offset = second;
    } else {
        memcpy(&buffer[offset], data, size);
        offset += size;
    }
}

static bool gInitialized;

static Stack gGCStack(JS_CRASH_STACK_GC);
static Stack gErrorStack(JS_CRASH_STACK_ERROR);
static Ring gRingBuffer(JS_CRASH_RING);

void
SnapshotGCStack()
{
    if (gInitialized)
        gGCStack.snapshot();
}

void
SnapshotErrorStack()
{
    if (gInitialized)
        gErrorStack.snapshot();
}

void
SaveCrashData(uint64 tag, void *ptr, size_t size)
{
    if (gInitialized)
        gRingBuffer.push(tag, ptr, size);
}

} /* namespace crash */
} /* namespace js */

using namespace js;
using namespace js::crash;

JS_PUBLIC_API(void)
JS_EnumerateDiagnosticMemoryRegions(JSEnumerateDiagnosticMemoryCallback callback)
{
#ifdef JS_CRASH_DIAGNOSTICS
    if (!gInitialized) {
        gInitialized = true;
        (*callback)(&gGCStack, sizeof(gGCStack));
        (*callback)(&gErrorStack, sizeof(gErrorStack));
        (*callback)(&gRingBuffer, sizeof(gRingBuffer));
    }
#endif
}

