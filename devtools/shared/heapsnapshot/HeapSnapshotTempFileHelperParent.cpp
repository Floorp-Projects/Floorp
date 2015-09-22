/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/devtools/HeapSnapshot.h"
#include "mozilla/devtools/HeapSnapshotTempFileHelperParent.h"
#include "mozilla/ErrorResult.h"
#include "private/pprio.h"

#include "nsIFile.h"

namespace mozilla {
namespace devtools {

using ipc::FileDescriptor;

static bool
openFileFailure(ErrorResult& rv,
                OpenHeapSnapshotTempFileResponse* outResponse)
{
    *outResponse = rv.StealNSResult();
    return true;
}

bool
HeapSnapshotTempFileHelperParent::RecvOpenHeapSnapshotTempFile(
    OpenHeapSnapshotTempFileResponse* outResponse)
{
    auto start = TimeStamp::Now();
    ErrorResult rv;
    nsAutoString filePath;
    nsCOMPtr<nsIFile> file = HeapSnapshot::CreateUniqueCoreDumpFile(rv,
                                                                    start,
                                                                    filePath);
    if (NS_WARN_IF(rv.Failed()))
        return openFileFailure(rv, outResponse);

    PRFileDesc* prfd;
    rv = file->OpenNSPRFileDesc(PR_WRONLY, 0, &prfd);
    if (NS_WARN_IF(rv.Failed()))
        return openFileFailure(rv, outResponse);

    FileDescriptor::PlatformHandleType handle =
        FileDescriptor::PlatformHandleType(PR_FileDesc2NativeHandle(prfd));
    FileDescriptor fd(handle);
    *outResponse = OpenedFile(filePath, fd);
    return true;
}

} // namespace devtools
} // namespace mozilla
