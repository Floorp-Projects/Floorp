/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/PosixNSPR.h"

#include "js/Utility.h"

#ifdef JS_POSIX_NSPR

#include <errno.h>
#include <sys/time.h>
#include <time.h>

int32_t
PR_FileDesc2NativeHandle(PRFileDesc* fd)
{
    MOZ_CRASH("PR_FileDesc2NativeHandle");
}

PRStatus
PR_GetOpenFileInfo(PRFileDesc *fd, PRFileInfo *info)
{
    MOZ_CRASH("PR_GetOpenFileInfo");
}

int32_t
PR_Seek(PRFileDesc *fd, int32_t offset, PRSeekWhence whence)
{
    MOZ_CRASH("PR_Seek");
}

PRFileMap*
PR_CreateFileMap(PRFileDesc *fd, int64_t size, PRFileMapProtect prot)
{
    MOZ_CRASH("PR_CreateFileMap");
}

void*
PR_MemMap(PRFileMap *fmap, int64_t offset, uint32_t len)
{
    MOZ_CRASH("PR_MemMap");
}

PRStatus
PR_MemUnmap(void *addr, uint32_t len)
{
    MOZ_CRASH("PR_MemUnmap");
}

PRStatus
PR_CloseFileMap(PRFileMap *fmap)
{
    MOZ_CRASH("PR_CloseFileMap");
}

#endif /* JS_POSIX_NSPR */
