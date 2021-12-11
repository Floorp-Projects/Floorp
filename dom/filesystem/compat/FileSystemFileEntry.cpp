/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemFileEntry.h"
#include "CallbackRunnables.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FileSystemUtils.h"
#include "mozilla/dom/MultipartBlobImpl.h"
#include "mozilla/dom/FileSystemFileEntryBinding.h"

namespace mozilla::dom {

namespace {

class FileCallbackRunnable final : public Runnable {
 public:
  FileCallbackRunnable(FileCallback* aCallback, File* aFile)
      : Runnable("FileCallbackRunnable"), mCallback(aCallback), mFile(aFile) {
    MOZ_ASSERT(aCallback);
    MOZ_ASSERT(aFile);
  }

  // MOZ_CAN_RUN_SCRIPT_BOUNDARY until Runnable::Run is MOZ_CAN_RUN_SCRIPT.  See
  // bug 1535398.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHOD Run() override {
    // Here we clone the File object.

    RefPtr<File> file = File::Create(mFile->GetParentObject(), mFile->Impl());
    mCallback->Call(*file);
    return NS_OK;
  }

 private:
  const RefPtr<FileCallback> mCallback;
  RefPtr<File> mFile;
};

}  // anonymous namespace

NS_IMPL_CYCLE_COLLECTION_INHERITED(FileSystemFileEntry, FileSystemEntry, mFile)

NS_IMPL_ADDREF_INHERITED(FileSystemFileEntry, FileSystemEntry)
NS_IMPL_RELEASE_INHERITED(FileSystemFileEntry, FileSystemEntry)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FileSystemFileEntry)
NS_INTERFACE_MAP_END_INHERITING(FileSystemEntry)

FileSystemFileEntry::FileSystemFileEntry(nsIGlobalObject* aGlobal, File* aFile,
                                         FileSystemDirectoryEntry* aParentEntry,
                                         FileSystem* aFileSystem)
    : FileSystemEntry(aGlobal, aParentEntry, aFileSystem), mFile(aFile) {
  MOZ_ASSERT(aGlobal);
  MOZ_ASSERT(mFile);
}

FileSystemFileEntry::~FileSystemFileEntry() = default;

JSObject* FileSystemFileEntry::WrapObject(JSContext* aCx,
                                          JS::Handle<JSObject*> aGivenProto) {
  return FileSystemFileEntry_Binding::Wrap(aCx, this, aGivenProto);
}

void FileSystemFileEntry::GetName(nsAString& aName, ErrorResult& aRv) const {
  mFile->GetName(aName);
}

void FileSystemFileEntry::GetFullPath(nsAString& aPath,
                                      ErrorResult& aRv) const {
  mFile->Impl()->GetDOMPath(aPath);
  if (aPath.IsEmpty()) {
    // We're under the root directory. webkitRelativePath
    // (implemented as GetPath) is for cases when file is selected because its
    // ancestor directory is selected. But that is not the case here, so need to
    // manually prepend '/'.
    nsAutoString name;
    mFile->GetName(name);
    aPath.AssignLiteral(FILESYSTEM_DOM_PATH_SEPARATOR_LITERAL);
    aPath.Append(name);
  }
}

void FileSystemFileEntry::GetFile(
    FileCallback& aSuccessCallback,
    const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback) const {
  RefPtr<FileCallbackRunnable> runnable =
      new FileCallbackRunnable(&aSuccessCallback, mFile);

  FileSystemUtils::DispatchRunnable(GetParentObject(), runnable.forget());
}

}  // namespace mozilla::dom
