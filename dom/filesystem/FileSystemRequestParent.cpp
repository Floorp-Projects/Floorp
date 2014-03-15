/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/dom/FileSystemRequestParent.h"

#include "CreateDirectoryTask.h"
#include "GetFileOrDirectoryTask.h"

#include "mozilla/AppProcessChecker.h"
#include "mozilla/dom/FileSystemBase.h"

namespace mozilla {
namespace dom {

FileSystemRequestParent::FileSystemRequestParent()
{
}

FileSystemRequestParent::~FileSystemRequestParent()
{
}

#define FILESYSTEM_REQUEST_PARENT_DISPATCH_ENTRY(name)                         \
    case FileSystemParams::TFileSystem##name##Params: {                        \
      const FileSystem##name##Params& p = aParams;                             \
      mFileSystem = FileSystemBase::FromString(p.filesystem());                \
      task = new name##Task(mFileSystem, p, this);                             \
      break;                                                                   \
    }

bool
FileSystemRequestParent::Dispatch(ContentParent* aParent,
                                  const FileSystemParams& aParams)
{
  MOZ_ASSERT(aParent, "aParent should not be null.");
  nsRefPtr<FileSystemTaskBase> task;
  switch (aParams.type()) {

    FILESYSTEM_REQUEST_PARENT_DISPATCH_ENTRY(CreateDirectory)
    FILESYSTEM_REQUEST_PARENT_DISPATCH_ENTRY(GetFileOrDirectory)

    default: {
      NS_RUNTIMEABORT("not reached");
      break;
    }
  }

  if (NS_WARN_IF(!task || !mFileSystem)) {
    // Should never reach here.
    return false;
  }

  if (!mFileSystem->IsTesting()) {
    // Check the content process permission.

    nsCString access;
    task->GetPermissionAccessType(access);

    nsAutoCString permissionName;
    permissionName = mFileSystem->GetPermission();
    permissionName.AppendLiteral("-");
    permissionName.Append(access);

    if (!AssertAppProcessPermission(aParent, permissionName.get())) {
      return false;
    }
  }

  task->Start();
  return true;
}

void
FileSystemRequestParent::ActorDestroy(ActorDestroyReason why)
{
  if (!mFileSystem) {
    return;
  }
  mFileSystem->Shutdown();
  mFileSystem = nullptr;
}

} // namespace dom
} // namespace mozilla
