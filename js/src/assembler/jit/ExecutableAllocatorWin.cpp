/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ExecutableAllocator.h"

#if ENABLE_ASSEMBLER && WTF_OS_WINDOWS

#include "jswin.h"

extern uint64_t random_next(uint64_t *, int);

namespace JSC {

uint64_t ExecutableAllocator::rngSeed;

size_t ExecutableAllocator::determinePageSize()
{
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    return system_info.dwPageSize;
}

void *ExecutableAllocator::computeRandomAllocationAddress()
{
    /*
     * Inspiration is V8's OS::Allocate in platform-win32.cc.
     *
     * VirtualAlloc takes 64K chunks out of the virtual address space, so we
     * keep 16b alignment.
     *
     * x86: V8 comments say that keeping addresses in the [64MiB, 1GiB) range
     * tries to avoid system default DLL mapping space. In the end, we get 13
     * bits of randomness in our selection.
     * x64: [2GiB, 4TiB), with 25 bits of randomness.
     */
    static const unsigned chunkBits = 16;
#if WTF_CPU_X86_64
    static const uintptr_t base = 0x0000000080000000;
    static const uintptr_t mask = 0x000003ffffff0000;
#elif WTF_CPU_X86
    static const uintptr_t base = 0x04000000;
    static const uintptr_t mask = 0x3fff0000;
#else
# error "Unsupported architecture"
#endif
    uint64_t rand = random_next(&rngSeed, 32) << chunkBits;
    return (void *) (base | rand & mask);
}

static bool
RandomizeIsBrokenImpl()
{
    OSVERSIONINFO osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

    GetVersionEx(&osvi);

    // Version number mapping is available at:
    // http://msdn.microsoft.com/en-us/library/ms724832%28v=vs.85%29.aspx
    // We disable everything before Vista, for now.
    return osvi.dwMajorVersion <= 5;
}

static bool
RandomizeIsBroken()
{
    // Use the compiler's intrinsic guards for |static type value = expr| to avoid some potential
    // races if runtimes are created from multiple threads.
    static int result = RandomizeIsBrokenImpl();
    return !!result;
}

ExecutablePool::Allocation ExecutableAllocator::systemAlloc(size_t n)
{
    void *allocation = NULL;
    // Randomization disabled to avoid a performance fault on x64 builds.
    // See bug 728623.
#ifndef JS_CPU_X64
    if (allocBehavior == AllocationCanRandomize && !RandomizeIsBroken()) {
        void *randomAddress = computeRandomAllocationAddress();
        allocation = VirtualAlloc(randomAddress, n, MEM_COMMIT | MEM_RESERVE,
                                  PAGE_EXECUTE_READWRITE);
    }
#endif
    if (!allocation)
        allocation = VirtualAlloc(0, n, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    ExecutablePool::Allocation alloc = { reinterpret_cast<char*>(allocation), n };
    return alloc;
}

void ExecutableAllocator::systemRelease(const ExecutablePool::Allocation& alloc)
{
    VirtualFree(alloc.pages, 0, MEM_RELEASE);
}

#if ENABLE_ASSEMBLER_WX_EXCLUSIVE
#error "ASSEMBLER_WX_EXCLUSIVE not yet suported on this platform."
#endif

}

#endif // HAVE(ASSEMBLER)
