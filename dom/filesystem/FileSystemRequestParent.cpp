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

bool
FileSystemRequestParent::Dispatch(ContentParent* aParent,
                                  const FileSystemParams& aParams)
{
  MOZ_ASSERT(aParent, "aParent should not be null.");
  nsRefPtr<FileSystemTaskBase> task;
  switch (aParams.type()) {

    case FileSystemParams::TFileSystemCreateDirectoryParams: {
      const FileSystemCreateDirectoryParams& p = aParams;
      mFileSystem = FileSystemBase::FromString(p.filesystem());
      task = new CreateDirectoryTask(mFileSystem, p, this);
      break;
    }

    case FileSystemParams::TFileSystemGetFileOrDirectoryParams: {
      const FileSystemGetFileOrDirectoryParams& p = aParams;
      mFileSystem = FileSystemBase::FromString(p.filesystem());
      task  = new GetFileOrDirectoryTask(mFileSystem, p, this);
      break;
    }

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

} // namespace dom
} // namespace mozilla
