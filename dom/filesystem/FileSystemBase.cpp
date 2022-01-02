/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FileSystemBase.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BlobImpl.h"
#include "mozilla/dom/FileSystemUtils.h"
#include "nsIFile.h"
#include "OSFileSystem.h"

namespace mozilla::dom {

FileSystemBase::FileSystemBase() : mShutdown(false) {}

FileSystemBase::~FileSystemBase() { AssertIsOnOwningThread(); }

void FileSystemBase::Shutdown() {
  AssertIsOnOwningThread();
  mShutdown = true;
}

nsIGlobalObject* FileSystemBase::GetParentObject() const {
  AssertIsOnOwningThread();
  return nullptr;
}

bool FileSystemBase::GetRealPath(BlobImpl* aFile, nsIFile** aPath) const {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aFile, "aFile Should not be null.");
  MOZ_ASSERT(aPath);

  nsAutoString filePath;
  ErrorResult rv;
  aFile->GetMozFullPathInternal(filePath, rv);
  if (NS_WARN_IF(rv.Failed())) {
    rv.SuppressException();
    return false;
  }

  rv = NS_NewLocalFile(filePath, true, aPath);
  if (NS_WARN_IF(rv.Failed())) {
    rv.SuppressException();
    return false;
  }

  return true;
}

bool FileSystemBase::IsSafeFile(nsIFile* aFile) const {
  AssertIsOnOwningThread();
  return false;
}

bool FileSystemBase::IsSafeDirectory(Directory* aDir) const {
  AssertIsOnOwningThread();
  return false;
}

void FileSystemBase::GetDirectoryName(nsIFile* aFile, nsAString& aRetval,
                                      ErrorResult& aRv) const {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aFile);

  aRv = aFile->GetLeafName(aRetval);
  NS_WARNING_ASSERTION(!aRv.Failed(), "GetLeafName failed");
}

void FileSystemBase::GetDOMPath(nsIFile* aFile, nsAString& aRetval,
                                ErrorResult& aRv) const {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aFile);

  aRetval.Truncate();

  nsCOMPtr<nsIFile> fileSystemPath;
  aRv = NS_NewLocalFile(LocalRootPath(), true, getter_AddRefs(fileSystemPath));
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  nsCOMPtr<nsIFile> path;
  aRv = aFile->Clone(getter_AddRefs(path));
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  nsTArray<nsString> parts;

  while (true) {
    nsAutoString leafName;
    aRv = path->GetLeafName(leafName);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    if (!leafName.IsEmpty()) {
      parts.AppendElement(leafName);
    }

    bool equal = false;
    aRv = fileSystemPath->Equals(path, &equal);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    if (equal) {
      break;
    }

    nsCOMPtr<nsIFile> parentPath;
    aRv = path->GetParent(getter_AddRefs(parentPath));
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    MOZ_ASSERT(parentPath);

    aRv = parentPath->Clone(getter_AddRefs(path));
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }
  }

  if (parts.IsEmpty()) {
    aRetval.AppendLiteral(FILESYSTEM_DOM_PATH_SEPARATOR_LITERAL);
    return;
  }

  for (int32_t i = parts.Length() - 1; i >= 0; --i) {
    aRetval.AppendLiteral(FILESYSTEM_DOM_PATH_SEPARATOR_LITERAL);
    aRetval.Append(parts[i]);
  }
}

void FileSystemBase::AssertIsOnOwningThread() const {
  NS_ASSERT_OWNINGTHREAD(FileSystemBase);
}

}  // namespace mozilla::dom
