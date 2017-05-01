/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AutoMemMap.h"
#include "ScriptPreloader-inl.h"

#include "mozilla/Unused.h"
#include "nsIFile.h"

#include <private/pprio.h>

namespace mozilla {
namespace loader {

using namespace mozilla::ipc;

AutoMemMap::~AutoMemMap()
{
    if (fileMap) {
        if (addr) {
            Unused << NS_WARN_IF(PR_MemUnmap(addr, size()) != PR_SUCCESS);
            addr = nullptr;
        }

        Unused << NS_WARN_IF(PR_CloseFileMap(fileMap) != PR_SUCCESS);
        fileMap = nullptr;
    }
}

FileDescriptor
AutoMemMap::cloneFileDescriptor()
{
    if (fd.get()) {
        auto handle = FileDescriptor::PlatformHandleType(PR_FileDesc2NativeHandle(fd.get()));
        return FileDescriptor(handle);
    }
    return FileDescriptor();
}

Result<Ok, nsresult>
AutoMemMap::init(nsIFile* file, int flags, int mode, PRFileMapProtect prot)
{
    MOZ_ASSERT(!fd);

    NS_TRY(file->OpenNSPRFileDesc(flags, mode, &fd.rwget()));

    return initInternal(prot);
}

Result<Ok, nsresult>
AutoMemMap::init(const FileDescriptor& file)
{
    MOZ_ASSERT(!fd);
    if (!file.IsValid()) {
        return Err(NS_ERROR_INVALID_ARG);
    }

    auto handle = file.ClonePlatformHandle();

    fd = PR_ImportFile(PROsfd(handle.get()));
    if (!fd) {
        return Err(NS_ERROR_FAILURE);
    }
    Unused << handle.release();

    return initInternal();
}

Result<Ok, nsresult>
AutoMemMap::initInternal(PRFileMapProtect prot)
{
    MOZ_ASSERT(!fileMap);
    MOZ_ASSERT(!addr);

    PRFileInfo64 fileInfo;
    NS_TRY(PR_GetOpenFileInfo64(fd.get(), &fileInfo));

    if (fileInfo.size > UINT32_MAX)
        return Err(NS_ERROR_INVALID_ARG);

    fileMap = PR_CreateFileMap(fd, 0, prot);
    if (!fileMap)
        return Err(NS_ERROR_FAILURE);

    size_ = fileInfo.size;
    addr = PR_MemMap(fileMap, 0, size_);
    if (!addr)
        return Err(NS_ERROR_FAILURE);

    return Ok();
}

} // namespace loader
} // namespace mozilla
