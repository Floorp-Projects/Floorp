/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/OSFileSystem.h"

#include "mozilla/dom/Directory.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FileSystemUtils.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsIFile.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {

OSFileSystem::OSFileSystem(const nsAString& aRootDir)
{
  mLocalRootPath = aRootDir;
  FileSystemUtils::LocalPathToNormalizedPath(mLocalRootPath,
                                             mNormalizedLocalRootPath);

  // Non-mobile devices don't have the concept of separate permissions to
  // access different parts of devices storage like Pictures, or Videos, etc.
  mRequiresPermissionChecks = false;

  mString = mLocalRootPath;

#ifdef DEBUG
  mPermission.AssignLiteral("never-used");
#endif
}

void
OSFileSystem::Init(nsPIDOMWindow* aWindow)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(!mWindow, "No duple Init() calls");
  MOZ_ASSERT(aWindow);
  mWindow = aWindow;
}

nsPIDOMWindow*
OSFileSystem::GetWindow() const
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  return mWindow;
}

void
OSFileSystem::GetRootName(nsAString& aRetval) const
{
  return aRetval.AssignLiteral("/");
}

bool
OSFileSystem::IsSafeFile(nsIFile* aFile) const
{
  // The concept of "safe files" is specific to the Device Storage API where
  // files are only "safe" if they're of a type that is appropriate for the
  // area of device storage that is being used.
  MOZ_CRASH("Don't use OSFileSystem with the Device Storage API");
  return true;
}

bool
OSFileSystem::IsSafeDirectory(Directory* aDir) const
{
  // The concept of "safe directory" is specific to the Device Storage API
  // where a directory is only "safe" if it belongs to the area of device
  // storage that it is being used with.
  MOZ_CRASH("Don't use OSFileSystem with the Device Storage API");
  return true;
}

} // namespace dom
} // namespace mozilla
