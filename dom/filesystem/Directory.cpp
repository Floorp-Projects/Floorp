/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Directory.h"

#include "CreateDirectoryTask.h"
#include "CreateFileTask.h"
#include "FileSystemPermissionRequest.h"
#include "GetDirectoryListingTask.h"
#include "GetFileOrDirectoryTask.h"
#include "GetFilesTask.h"
#include "RemoveTask.h"
#include "WorkerPrivate.h"

#include "nsCharSeparatedTokenizer.h"
#include "nsString.h"
#include "mozilla/dom/DirectoryBinding.h"
#include "mozilla/dom/FileSystemBase.h"
#include "mozilla/dom/FileSystemUtils.h"
#include "mozilla/dom/OSFileSystem.h"

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

namespace {

bool
TokenizerIgnoreNothing(char16_t /* aChar */)
{
  return false;
}

bool
IsValidRelativeDOMPath(const nsString& aPath, nsTArray<nsString>& aParts)
{
  // We don't allow empty relative path to access the root.
  if (aPath.IsEmpty()) {
    return false;
  }

  // Leading and trailing "/" are not allowed.
  if (aPath.First() == FILESYSTEM_DOM_PATH_SEPARATOR_CHAR ||
      aPath.Last() == FILESYSTEM_DOM_PATH_SEPARATOR_CHAR) {
    return false;
  }

  NS_NAMED_LITERAL_STRING(kCurrentDir, ".");
  NS_NAMED_LITERAL_STRING(kParentDir, "..");

  // Split path and check each path component.
  nsCharSeparatedTokenizerTemplate<TokenizerIgnoreNothing>
    tokenizer(aPath, FILESYSTEM_DOM_PATH_SEPARATOR_CHAR);

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

    aParts.AppendElement(pathComponent);
  }

  return true;
}

} // anonymous namespace

NS_IMPL_CYCLE_COLLECTION_CLASS(Directory)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Directory)
  if (tmp->mFileSystem) {
    tmp->mFileSystem->Unlink();
    tmp->mFileSystem = nullptr;
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Directory)
  if (tmp->mFileSystem) {
    tmp->mFileSystem->Traverse(cb);
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(Directory)

NS_IMPL_CYCLE_COLLECTING_ADDREF(Directory)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Directory)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Directory)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/* static */ bool
Directory::DeviceStorageEnabled(JSContext* aCx, JSObject* aObj)
{
  if (!NS_IsMainThread()) {
    return false;
  }

  return Preferences::GetBool("device.storage.enabled", false);
}

/* static */ bool
Directory::WebkitBlinkDirectoryPickerEnabled(JSContext* aCx, JSObject* aObj)
{
  if (NS_IsMainThread()) {
    return Preferences::GetBool("dom.webkitBlink.dirPicker.enabled", false);
  }

  workers::WorkerPrivate* workerPrivate =
    workers::GetWorkerPrivateFromContext(aCx);
  if (!workerPrivate) {
    return false;
  }

  return workerPrivate->WebkitBlinkDirectoryPickerEnabled();
}

/* static */ already_AddRefed<Promise>
Directory::GetRoot(FileSystemBase* aFileSystem, ErrorResult& aRv)
{
  // Only exposed for DeviceStorage.
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aFileSystem);

  nsCOMPtr<nsIFile> path;
  aRv = NS_NewNativeLocalFile(NS_ConvertUTF16toUTF8(aFileSystem->LocalOrDeviceStorageRootPath()),
                              true, getter_AddRefs(path));
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<GetFileOrDirectoryTaskChild> task =
    GetFileOrDirectoryTaskChild::Create(aFileSystem, path, true, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  FileSystemPermissionRequest::RequestForTask(task);
  return task->GetPromise();
}

/* static */ already_AddRefed<Directory>
Directory::Create(nsISupports* aParent, nsIFile* aFile,
                  FileSystemBase* aFileSystem)
{
  MOZ_ASSERT(aParent);
  MOZ_ASSERT(aFile);

#ifdef DEBUG
  bool isDir;
  nsresult rv = aFile->IsDirectory(&isDir);
  MOZ_ASSERT(NS_SUCCEEDED(rv) && isDir);
#endif

  RefPtr<Directory> directory = new Directory(aParent, aFile, aFileSystem);
  return directory.forget();
}

Directory::Directory(nsISupports* aParent,
                     nsIFile* aFile,
                     FileSystemBase* aFileSystem)
  : mParent(aParent)
  , mFile(aFile)
{
  MOZ_ASSERT(aFile);

  // aFileSystem can be null. In this case we create a OSFileSystem when needed.
  if (aFileSystem) {
    // More likely, this is a OSFileSystem. This object keeps a reference of
    // mParent but it's not cycle collectable and to avoid manual
    // addref/release, it's better to have 1 object per directory. For this
    // reason we clone it here.
    mFileSystem = aFileSystem->Clone();
  }
}

Directory::~Directory()
{
}

nsISupports*
Directory::GetParentObject() const
{
  return mParent;
}

JSObject*
Directory::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return DirectoryBinding::Wrap(aCx, this, aGivenProto);
}

void
Directory::GetName(nsAString& aRetval, ErrorResult& aRv)
{
  aRetval.Truncate();

  RefPtr<FileSystemBase> fs = GetFileSystem(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  fs->GetDirectoryName(mFile, aRetval, aRv);
}

already_AddRefed<Promise>
Directory::CreateFile(const nsAString& aPath, const CreateFileOptions& aOptions,
                      ErrorResult& aRv)
{
  // Only exposed for DeviceStorage.
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<Blob> blobData;
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
      const ArrayBuffer& buffer = data.GetAsArrayBuffer();
      buffer.ComputeLengthAndData();
      arrayData.AppendElements(buffer.Data(), buffer.Length());
    } else if (data.IsArrayBufferView()){
      const ArrayBufferView& view = data.GetAsArrayBufferView();
      view.ComputeLengthAndData();
      arrayData.AppendElements(view.Data(), view.Length());
    } else {
      blobData = data.GetAsBlob();
    }
  }

  nsCOMPtr<nsIFile> realPath;
  nsresult error = DOMPathToRealPath(aPath, getter_AddRefs(realPath));

  RefPtr<FileSystemBase> fs = GetFileSystem(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<CreateFileTaskChild> task =
    CreateFileTaskChild::Create(fs, realPath, blobData, arrayData, replace,
                                aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  task->SetError(error);
  FileSystemPermissionRequest::RequestForTask(task);
  return task->GetPromise();
}

already_AddRefed<Promise>
Directory::CreateDirectory(const nsAString& aPath, ErrorResult& aRv)
{
  // Only exposed for DeviceStorage.
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIFile> realPath;
  nsresult error = DOMPathToRealPath(aPath, getter_AddRefs(realPath));

  RefPtr<FileSystemBase> fs = GetFileSystem(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<CreateDirectoryTaskChild> task =
    CreateDirectoryTaskChild::Create(fs, realPath, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  task->SetError(error);
  FileSystemPermissionRequest::RequestForTask(task);
  return task->GetPromise();
}

already_AddRefed<Promise>
Directory::Get(const nsAString& aPath, ErrorResult& aRv)
{
  // Only exposed for DeviceStorage.
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIFile> realPath;
  nsresult error = DOMPathToRealPath(aPath, getter_AddRefs(realPath));

  RefPtr<FileSystemBase> fs = GetFileSystem(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<GetFileOrDirectoryTaskChild> task =
    GetFileOrDirectoryTaskChild::Create(fs, realPath, false, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  task->SetError(error);
  FileSystemPermissionRequest::RequestForTask(task);
  return task->GetPromise();
}

already_AddRefed<Promise>
Directory::Remove(const StringOrFileOrDirectory& aPath, ErrorResult& aRv)
{
  // Only exposed for DeviceStorage.
  MOZ_ASSERT(NS_IsMainThread());
  return RemoveInternal(aPath, false, aRv);
}

already_AddRefed<Promise>
Directory::RemoveDeep(const StringOrFileOrDirectory& aPath, ErrorResult& aRv)
{
  // Only exposed for DeviceStorage.
  MOZ_ASSERT(NS_IsMainThread());
  return RemoveInternal(aPath, true, aRv);
}

already_AddRefed<Promise>
Directory::RemoveInternal(const StringOrFileOrDirectory& aPath, bool aRecursive,
                          ErrorResult& aRv)
{
  // Only exposed for DeviceStorage.
  MOZ_ASSERT(NS_IsMainThread());

  nsresult error = NS_OK;
  nsCOMPtr<nsIFile> realPath;

  // Check and get the target path.

  RefPtr<FileSystemBase> fs = GetFileSystem(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // If this is a File
  if (aPath.IsFile()) {
    if (!fs->GetRealPath(aPath.GetAsFile().Impl(),
                         getter_AddRefs(realPath))) {
      error = NS_ERROR_DOM_SECURITY_ERR;
    }

  // If this is a string
  } else if (aPath.IsString()) {
    error = DOMPathToRealPath(aPath.GetAsString(), getter_AddRefs(realPath));

  // Directory
  } else {
    MOZ_ASSERT(aPath.IsDirectory());
    if (!fs->IsSafeDirectory(&aPath.GetAsDirectory())) {
      error = NS_ERROR_DOM_SECURITY_ERR;
    } else {
      realPath = aPath.GetAsDirectory().mFile;
    }
  }

  // The target must be a descendant of this directory.
  if (!FileSystemUtils::IsDescendantPath(mFile, realPath)) {
    error = NS_ERROR_DOM_FILESYSTEM_NO_MODIFICATION_ALLOWED_ERR;
  }

  RefPtr<RemoveTaskChild> task =
    RemoveTaskChild::Create(fs, mFile, realPath, aRecursive, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  task->SetError(error);
  FileSystemPermissionRequest::RequestForTask(task);
  return task->GetPromise();
}

void
Directory::GetPath(nsAString& aRetval, ErrorResult& aRv)
{
  // This operation is expensive. Better to cache the result.
  if (mPath.IsEmpty()) {
    RefPtr<FileSystemBase> fs = GetFileSystem(aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    fs->GetDOMPath(mFile, mPath, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }
  }

  aRetval = mPath;
}

nsresult
Directory::GetFullRealPath(nsAString& aPath)
{
  nsresult rv = mFile->GetPath(aPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

already_AddRefed<Promise>
Directory::GetFilesAndDirectories(ErrorResult& aRv)
{
  RefPtr<FileSystemBase> fs = GetFileSystem(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<GetDirectoryListingTaskChild> task =
    GetDirectoryListingTaskChild::Create(fs, this, mFile, mFilters, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  FileSystemPermissionRequest::RequestForTask(task);
  return task->GetPromise();
}

already_AddRefed<Promise>
Directory::GetFiles(bool aRecursiveFlag, ErrorResult& aRv)
{
  ErrorResult rv;
  RefPtr<FileSystemBase> fs = GetFileSystem(rv);
  if (NS_WARN_IF(rv.Failed())) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  RefPtr<GetFilesTaskChild> task =
    GetFilesTaskChild::Create(fs, this, mFile, aRecursiveFlag, rv);
  if (NS_WARN_IF(rv.Failed())) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  FileSystemPermissionRequest::RequestForTask(task);
  return task->GetPromise();
}

void
Directory::SetContentFilters(const nsAString& aFilters)
{
  mFilters = aFilters;
}

FileSystemBase*
Directory::GetFileSystem(ErrorResult& aRv)
{
  if (!mFileSystem) {
    nsAutoString path;
    aRv = mFile->GetPath(path);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    RefPtr<OSFileSystem> fs = new OSFileSystem(path);
    fs->Init(mParent);

    mFileSystem = fs;
  }

  return mFileSystem;
}

nsresult
Directory::DOMPathToRealPath(const nsAString& aPath, nsIFile** aFile) const
{
  nsString relativePath;
  relativePath = aPath;

  // Trim white spaces.
  static const char kWhitespace[] = "\b\t\r\n ";
  relativePath.Trim(kWhitespace);

  nsTArray<nsString> parts;
  if (!IsValidRelativeDOMPath(relativePath, parts)) {
    return NS_ERROR_DOM_FILESYSTEM_INVALID_PATH_ERR;
  }

  nsCOMPtr<nsIFile> file;
  nsresult rv = mFile->Clone(getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  for (uint32_t i = 0; i < parts.Length(); ++i) {
    rv = file->AppendRelativePath(parts[i]);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  file.forget(aFile);
  return NS_OK;
}

bool
Directory::ClonableToDifferentThreadOrProcess() const
{
  // If we don't have a fileSystem we are going to create a OSFileSystem that is
  // clonable everywhere.
  if (!mFileSystem) {
    return true;
  }

  return mFileSystem->ClonableToDifferentThreadOrProcess();
}

} // namespace dom
} // namespace mozilla
