/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ***** END LICENSE BLOCK ***** */

#include "assembler/wtf/Platform.h"

#if ENABLE_ASSEMBLER && WTF_OS_OS2

#define INCL_DOS
#include <os2.h>

#include "assembler/wtf/Assertions.h"

#include "yarr/OSAllocator.h"

namespace WTF {

static inline ULONG protection(bool writable, bool executable)
{
    return (PAG_READ | (writable ? PAG_WRITE : 0) | (executable ? PAG_EXECUTE : 0));
}

void* OSAllocator::reserveUncommitted(size_t bytes, Usage, bool writable, bool executable)
{
    void* result = NULL;
    if (DosAllocMem(&result, bytes, OBJ_ANY | protection(writable, executable)) &&
        DosAllocMem(&result, bytes, protection(writable, executable)))
    {   CRASH();
    }
    return result;
}

void* OSAllocator::reserveAndCommit(size_t bytes, Usage, bool writable, bool executable)
{
    void* result = NULL;
    if (DosAllocMem(&result, bytes, OBJ_ANY | PAG_COMMIT | protection(writable, executable)) &&
        DosAllocMem(&result, bytes, PAG_COMMIT | protection(writable, executable)))
    {   CRASH();
    }
    return result;

}

void OSAllocator::commit(void* address, size_t bytes, bool writable, bool executable)
{
if (DosSetMem(address, bytes, PAG_COMMIT | protection(writable, executable)))
    CRASH();
}

void OSAllocator::decommit(void* address, size_t bytes)
{
if (DosSetMem(address, bytes, PAG_DECOMMIT))
    CRASH();
}

void OSAllocator::releaseDecommitted(void* address, size_t bytes)
{
if (DosFreeMem(address))
    CRASH();
}

} // namespace WTF

#endif
