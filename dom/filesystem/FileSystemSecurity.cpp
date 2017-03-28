/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemSecurity.h"
#include "FileSystemUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {
namespace dom {

namespace {

StaticRefPtr<FileSystemSecurity> gFileSystemSecurity;

} // anonymous

/* static */ already_AddRefed<FileSystemSecurity>
FileSystemSecurity::Get()
{
  MOZ_ASSERT(NS_IsMainThread());
  AssertIsInMainProcess();

  RefPtr<FileSystemSecurity> service = gFileSystemSecurity.get();
  return service.forget();
}

/* static */ already_AddRefed<FileSystemSecurity>
FileSystemSecurity::GetOrCreate()
{
  MOZ_ASSERT(NS_IsMainThread());
  AssertIsInMainProcess();

  if (!gFileSystemSecurity) {
    gFileSystemSecurity = new FileSystemSecurity();
    ClearOnShutdown(&gFileSystemSecurity);
  }

  RefPtr<FileSystemSecurity> service = gFileSystemSecurity.get();
  return service.forget();
}

FileSystemSecurity::FileSystemSecurity()
{
  MOZ_ASSERT(NS_IsMainThread());
  AssertIsInMainProcess();
}

FileSystemSecurity::~FileSystemSecurity()
{
  MOZ_ASSERT(NS_IsMainThread());
  AssertIsInMainProcess();
}

void
FileSystemSecurity::GrantAccessToContentProcess(ContentParentId aId,
                                                const nsAString& aDirectoryPath)
{
  MOZ_ASSERT(NS_IsMainThread());
  AssertIsInMainProcess();

  nsTArray<nsString>* paths;
  if (!mPaths.Get(aId, &paths)) {
    paths = new nsTArray<nsString>();
    mPaths.Put(aId, paths);
  } else if (paths->Contains(aDirectoryPath)) {
    return;
  }

  paths->AppendElement(aDirectoryPath);
}

void
FileSystemSecurity::Forget(ContentParentId aId)
{
  MOZ_ASSERT(NS_IsMainThread());
  AssertIsInMainProcess();

  mPaths.Remove(aId);
}

bool
FileSystemSecurity::ContentProcessHasAccessTo(ContentParentId aId,
                                              const nsAString& aPath)
{
  MOZ_ASSERT(NS_IsMainThread());
  AssertIsInMainProcess();

  if (FindInReadable(NS_LITERAL_STRING(".."), aPath)) {
    return false;
  }

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

} // dom namespace
} // mozilla namespace
