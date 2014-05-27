/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Directory.h"

#include "CreateDirectoryTask.h"
#include "CreateFileTask.h"
#include "FileSystemPermissionRequest.h"
#include "GetFileOrDirectoryTask.h"
#include "RemoveTask.h"

#include "nsCharSeparatedTokenizer.h"
#include "nsString.h"
#include "mozilla/dom/DirectoryBinding.h"
#include "mozilla/dom/FileSystemBase.h"
#include "mozilla/dom/FileSystemUtils.h"
#include "mozilla/dom/UnionTypes.h"

// Resolve the name collision of Microsoft's API name with macros defined in
// Windows header files. Undefine the macro of CreateDirectory to avoid
// Directory#CreateDirectory being replaced by Directory#CreateDirectoryW.
#ifdef CreateDirectory
#undef CreateDirectory
#endif
// Undefine the macro of CreateFile to avoid Directory#CreateFile being replaced
// by Directory#CreateFileW.
#ifdef CreateFile
#undef CreateFile
#endif

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(Directory)
NS_IMPL_CYCLE_COLLECTING_ADDREF(Directory)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Directory)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Directory)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

// static
already_AddRefed<Promise>
Directory::GetRoot(FileSystemBase* aFileSystem)
{
  nsRefPtr<GetFileOrDirectoryTask> task = new GetFileOrDirectoryTask(
    aFileSystem, EmptyString(), true);
  FileSystemPermissionRequest::RequestForTask(task);
  return task->GetPromise();
}

Directory::Directory(FileSystemBase* aFileSystem,
                     const nsAString& aPath)
  : mFileSystem(aFileSystem)
  , mPath(aPath)
{
  MOZ_ASSERT(aFileSystem, "aFileSystem should not be null.");
  // Remove the trailing "/".
  mPath.Trim(FILESYSTEM_DOM_PATH_SEPARATOR, false, true);

  SetIsDOMBinding();
}

Directory::~Directory()
{
}

nsPIDOMWindow*
Directory::GetParentObject() const
{
  return mFileSystem->GetWindow();
}

JSObject*
Directory::WrapObject(JSContext* aCx)
{
  return DirectoryBinding::Wrap(aCx, this);
}

void
Directory::GetName(nsString& aRetval) const
{
  aRetval.Truncate();

  if (mPath.IsEmpty()) {
    aRetval = mFileSystem->GetRootName();
    return;
  }

  aRetval = Substring(mPath,
                      mPath.RFindChar(FileSystemUtils::kSeparatorChar) + 1);
}

already_AddRefed<Promise>
Directory::CreateFile(const nsAString& aPath, const CreateFileOptions& aOptions)
{
  nsresult error = NS_OK;
  nsString realPath;
  nsRefPtr<nsIDOMBlob> blobData;
  InfallibleTArray<uint8_t> arrayData;
  bool replace = (aOptions.mIfExists == CreateIfExistsMode::Replace);

  // Get the file content.
  if (aOptions.mData.WasPassed()) {
    auto& data = aOptions.mData.Value();
    if (data.IsString()) {
      NS_ConvertUTF16toUTF8 str(data.GetAsString());
      arrayData.AppendElements(reinterpret_cast<const uint8_t *>(str.get()),
                               str.Length());
    } else if (data.IsArrayBuffer()) {
      ArrayBuffer& buffer = data.GetAsArrayBuffer();
      buffer.ComputeLengthAndData();
      arrayData.AppendElements(buffer.Data(), buffer.Length());
    } else if (data.IsArrayBufferView()){
      ArrayBufferView& view = data.GetAsArrayBufferView();
      view.ComputeLengthAndData();
      arrayData.AppendElements(view.Data(), view.Length());
    } else {
      blobData = data.GetAsBlob();
    }
  }

  if (!DOMPathToRealPath(aPath, realPath)) {
    error = NS_ERROR_DOM_FILESYSTEM_INVALID_PATH_ERR;
  }

  nsRefPtr<CreateFileTask> task = new CreateFileTask(mFileSystem, realPath,
    blobData, arrayData, replace);
  task->SetError(error);
  FileSystemPermissionRequest::RequestForTask(task);
  return task->GetPromise();
}

already_AddRefed<Promise>
Directory::CreateDirectory(const nsAString& aPath)
{
  nsresult error = NS_OK;
  nsString realPath;
  if (!DOMPathToRealPath(aPath, realPath)) {
    error = NS_ERROR_DOM_FILESYSTEM_INVALID_PATH_ERR;
  }
  nsRefPtr<CreateDirectoryTask> task = new CreateDirectoryTask(
    mFileSystem, realPath);
  task->SetError(error);
  FileSystemPermissionRequest::RequestForTask(task);
  return task->GetPromise();
}

already_AddRefed<Promise>
Directory::Get(const nsAString& aPath)
{
  nsresult error = NS_OK;
  nsString realPath;
  if (!DOMPathToRealPath(aPath, realPath)) {
    error = NS_ERROR_DOM_FILESYSTEM_INVALID_PATH_ERR;
  }
  nsRefPtr<GetFileOrDirectoryTask> task = new GetFileOrDirectoryTask(
    mFileSystem, realPath, false);
  task->SetError(error);
  FileSystemPermissionRequest::RequestForTask(task);
  return task->GetPromise();
}

already_AddRefed<Promise>
Directory::Remove(const StringOrFileOrDirectory& aPath)
{
  return RemoveInternal(aPath, false);
}

already_AddRefed<Promise>
Directory::RemoveDeep(const StringOrFileOrDirectory& aPath)
{
  return RemoveInternal(aPath, true);
}

already_AddRefed<Promise>
Directory::RemoveInternal(const StringOrFileOrDirectory& aPath, bool aRecursive)
{
  nsresult error = NS_OK;
  nsString realPath;
  nsCOMPtr<nsIDOMFile> file;

  // Check and get the target path.

  if (aPath.IsFile()) {
    file = aPath.GetAsFile();
    goto parameters_check_done;
  }

  if (aPath.IsString()) {
    if (!DOMPathToRealPath(aPath.GetAsString(), realPath)) {
      error = NS_ERROR_DOM_FILESYSTEM_INVALID_PATH_ERR;
    }
    goto parameters_check_done;
  }

  if (!mFileSystem->IsSafeDirectory(&aPath.GetAsDirectory())) {
    error = NS_ERROR_DOM_SECURITY_ERR;
    goto parameters_check_done;
  }

  realPath = aPath.GetAsDirectory().mPath;
  // The target must be a descendant of this directory.
  if (!FileSystemUtils::IsDescendantPath(mPath, realPath)) {
    error = NS_ERROR_DOM_FILESYSTEM_NO_MODIFICATION_ALLOWED_ERR;
  }

parameters_check_done:

  nsRefPtr<RemoveTask> task = new RemoveTask(mFileSystem, mPath, file, realPath,
    aRecursive);
  task->SetError(error);
  FileSystemPermissionRequest::RequestForTask(task);
  return task->GetPromise();
}

FileSystemBase*
Directory::GetFileSystem() const
{
  return mFileSystem.get();
}

bool
Directory::DOMPathToRealPath(const nsAString& aPath, nsAString& aRealPath) const
{
  aRealPath.Truncate();

  nsString relativePath;
  relativePath = aPath;

  // Trim white spaces.
  static const char kWhitespace[] = "\b\t\r\n ";
  relativePath.Trim(kWhitespace);

  if (!IsValidRelativePath(relativePath)) {
    return false;
  }

  aRealPath = mPath + NS_LITERAL_STRING(FILESYSTEM_DOM_PATH_SEPARATOR) +
    relativePath;

  return true;
}

// static
bool
Directory::IsValidRelativePath(const nsString& aPath)
{
  // We don't allow empty relative path to access the root.
  if (aPath.IsEmpty()) {
    return false;
  }

  // Leading and trailing "/" are not allowed.
  if (aPath.First() == FileSystemUtils::kSeparatorChar ||
      aPath.Last() == FileSystemUtils::kSeparatorChar) {
    return false;
  }

  NS_NAMED_LITERAL_STRING(kCurrentDir, ".");
  NS_NAMED_LITERAL_STRING(kParentDir, "..");

  // Split path and check each path component.
  nsCharSeparatedTokenizer tokenizer(aPath, FileSystemUtils::kSeparatorChar);
  while (tokenizer.hasMoreTokens()) {
    nsDependentSubstring pathComponent = tokenizer.nextToken();
    // The path containing empty components, such as "foo//bar", is invalid.
    // We don't allow paths, such as "../foo", "foo/./bar" and "foo/../bar",
    // to walk up the directory.
    if (pathComponent.IsEmpty() ||
        pathComponent.Equals(kCurrentDir) ||
        pathComponent.Equals(kParentDir)) {
      return false;
    }
  }

  return true;
}

} // namespace dom
} // namespace mozilla
