/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemSecurity.h"
#include "FileSystemUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/StaticPtr.h"

namespace mozilla::dom {

namespace {

StaticRefPtr<FileSystemSecurity> gFileSystemSecurity;

}  // namespace

/* static */
already_AddRefed<FileSystemSecurity> FileSystemSecurity::Get() {
  MOZ_ASSERT(NS_IsMainThread());
  mozilla::ipc::AssertIsInMainProcess();

  RefPtr<FileSystemSecurity> service = gFileSystemSecurity.get();
  return service.forget();
}

/* static */
already_AddRefed<FileSystemSecurity> FileSystemSecurity::GetOrCreate() {
  MOZ_ASSERT(NS_IsMainThread());
  mozilla::ipc::AssertIsInMainProcess();

  if (!gFileSystemSecurity) {
    gFileSystemSecurity = new FileSystemSecurity();
    ClearOnShutdown(&gFileSystemSecurity);
  }

  RefPtr<FileSystemSecurity> service = gFileSystemSecurity.get();
  return service.forget();
}

FileSystemSecurity::FileSystemSecurity() {
  MOZ_ASSERT(NS_IsMainThread());
  mozilla::ipc::AssertIsInMainProcess();
}

FileSystemSecurity::~FileSystemSecurity() {
  MOZ_ASSERT(NS_IsMainThread());
  mozilla::ipc::AssertIsInMainProcess();
}

void FileSystemSecurity::GrantAccessToContentProcess(
    ContentParentId aId, const nsAString& aDirectoryPath) {
  MOZ_ASSERT(NS_IsMainThread());
  mozilla::ipc::AssertIsInMainProcess();

  mPaths.WithEntryHandle(aId, [&](auto&& entry) {
    if (entry && entry.Data()->Contains(aDirectoryPath)) {
      return;
    }

    entry.OrInsertWith([] { return MakeUnique<nsTArray<nsString>>(); })
        ->AppendElement(aDirectoryPath);
  });
}

void FileSystemSecurity::Forget(ContentParentId aId) {
  MOZ_ASSERT(NS_IsMainThread());
  mozilla::ipc::AssertIsInMainProcess();

  mPaths.Remove(aId);
}

bool FileSystemSecurity::ContentProcessHasAccessTo(ContentParentId aId,
                                                   const nsAString& aPath) {
  MOZ_ASSERT(NS_IsMainThread());
  mozilla::ipc::AssertIsInMainProcess();

#if defined(XP_WIN)
  if (StringBeginsWith(aPath, u"..\\"_ns) ||
      FindInReadable(u"\\..\\"_ns, aPath)) {
    return false;
  }
#elif defined(XP_UNIX)
  if (StringBeginsWith(aPath, u"../"_ns) || FindInReadable(u"/../"_ns, aPath)) {
    return false;
  }
#endif

  nsTArray<nsString>* paths;
  if (!mPaths.Get(aId, &paths)) {
    return false;
  }

  for (uint32_t i = 0, len = paths->Length(); i < len; ++i) {
    if (FileSystemUtils::IsDescendantPath(paths->ElementAt(i), aPath)) {
      return true;
    }
  }

  return false;
}

}  // namespace mozilla::dom
