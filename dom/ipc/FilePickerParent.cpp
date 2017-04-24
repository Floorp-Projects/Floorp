/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FilePickerParent.h"
#include "nsComponentManagerUtils.h"
#include "nsNetCID.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"
#include "nsIFile.h"
#include "nsISimpleEnumerator.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/FileBlobImpl.h"
#include "mozilla/dom/FileSystemSecurity.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/dom/IPCBlobUtils.h"

using mozilla::Unused;
using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(FilePickerParent::FilePickerShownCallback,
                  nsIFilePickerShownCallback);

NS_IMETHODIMP
FilePickerParent::FilePickerShownCallback::Done(int16_t aResult)
{
  if (mFilePickerParent) {
    mFilePickerParent->Done(aResult);
  }
  return NS_OK;
}

void
FilePickerParent::FilePickerShownCallback::Destroy()
{
  mFilePickerParent = nullptr;
}

FilePickerParent::~FilePickerParent()
{
}

// We run code in three places:
// 1. The main thread calls Dispatch() to start the runnable.
// 2. The stream transport thread stat()s the file in Run() and then dispatches
// the same runnable on the main thread.
// 3. The main thread sends the results over IPC.
FilePickerParent::IORunnable::IORunnable(FilePickerParent *aFPParent,
                                         nsTArray<nsCOMPtr<nsIFile>>& aFiles,
                                         bool aIsDirectory)
 : mFilePickerParent(aFPParent)
 , mIsDirectory(aIsDirectory)
{
  mFiles.SwapElements(aFiles);
  MOZ_ASSERT_IF(aIsDirectory, mFiles.Length() == 1);
}

bool
FilePickerParent::IORunnable::Dispatch()
{
  MOZ_ASSERT(NS_IsMainThread());

  mEventTarget = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  if (!mEventTarget) {
    return false;
  }

  nsresult rv = mEventTarget->Dispatch(this, NS_DISPATCH_NORMAL);
  return NS_SUCCEEDED(rv);
}

NS_IMETHODIMP
FilePickerParent::IORunnable::Run()
{
  // If we're on the main thread, then that means we're done. Just send the
  // results.
  if (NS_IsMainThread()) {
    if (mFilePickerParent) {
      mFilePickerParent->SendFilesOrDirectories(mResults);
    }
    return NS_OK;
  }

  // We're not on the main thread, so do the IO.

  for (uint32_t i = 0; i < mFiles.Length(); ++i) {
    if (mIsDirectory) {
      nsAutoString path;
      nsresult rv = mFiles[i]->GetPath(path);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        continue;
      }

      BlobImplOrString* data = mResults.AppendElement();
      data->mType = BlobImplOrString::eDirectoryPath;
      data->mDirectoryPath = path;
      continue;
    }

    RefPtr<BlobImpl> blobImpl = new FileBlobImpl(mFiles[i]);

    ErrorResult error;
    blobImpl->GetSize(error);
    if (NS_WARN_IF(error.Failed())) {
      error.SuppressException();
      continue;
    }

    blobImpl->GetLastModified(error);
    if (NS_WARN_IF(error.Failed())) {
      error.SuppressException();
      continue;
    }

    BlobImplOrString* data = mResults.AppendElement();
    data->mType = BlobImplOrString::eBlobImpl;
    data->mBlobImpl = blobImpl;
  }

  // Dispatch ourselves back on the main thread.
  if (NS_FAILED(NS_DispatchToMainThread(this))) {
    // It's hard to see how we can recover gracefully in this case. The child
    // process is waiting for an IPC, but that can only happen on the main
    // thread.
    MOZ_CRASH();
  }

  return NS_OK;
}

void
FilePickerParent::IORunnable::Destroy()
{
  mFilePickerParent = nullptr;
}

void
FilePickerParent::SendFilesOrDirectories(const nsTArray<BlobImplOrString>& aData)
{
  nsIContentParent* parent = TabParent::GetFrom(Manager())->Manager();

  if (mMode == nsIFilePicker::modeGetFolder) {
    MOZ_ASSERT(aData.Length() <= 1);
    if (aData.IsEmpty()) {
      Unused << Send__delete__(this, void_t(), mResult);
      return;
    }

    MOZ_ASSERT(aData[0].mType == BlobImplOrString::eDirectoryPath);

    // Let's inform the security singleton about the given access of this tab on
    // this directory path.
    RefPtr<FileSystemSecurity> fss = FileSystemSecurity::GetOrCreate();
    fss->GrantAccessToContentProcess(parent->ChildID(),
                                     aData[0].mDirectoryPath);

    InputDirectory input;
    input.directoryPath() = aData[0].mDirectoryPath;
    Unused << Send__delete__(this, input, mResult);
    return;
  }

  InfallibleTArray<IPCBlob> ipcBlobs;

  for (unsigned i = 0; i < aData.Length(); i++) {
    IPCBlob ipcBlob;

    MOZ_ASSERT(aData[i].mType == BlobImplOrString::eBlobImpl);
    nsresult rv = IPCBlobUtils::Serialize(aData[i].mBlobImpl, parent, ipcBlob);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      break;
    }

    ipcBlobs.AppendElement(ipcBlob);
  }

  InputBlobs inblobs;
  inblobs.blobs().SwapElements(ipcBlobs);

  Unused << Send__delete__(this, inblobs, mResult);
}

void
FilePickerParent::Done(int16_t aResult)
{
  mResult = aResult;

  if (mResult != nsIFilePicker::returnOK) {
    Unused << Send__delete__(this, void_t(), mResult);
    return;
  }

  nsTArray<nsCOMPtr<nsIFile>> files;
  if (mMode == nsIFilePicker::modeOpenMultiple) {
    nsCOMPtr<nsISimpleEnumerator> iter;
    NS_ENSURE_SUCCESS_VOID(mFilePicker->GetFiles(getter_AddRefs(iter)));

    nsCOMPtr<nsISupports> supports;
    bool loop = true;
    while (NS_SUCCEEDED(iter->HasMoreElements(&loop)) && loop) {
      iter->GetNext(getter_AddRefs(supports));
      if (supports) {
        nsCOMPtr<nsIFile> file = do_QueryInterface(supports);
        MOZ_ASSERT(file);
        files.AppendElement(file);
      }
    }
  } else {
    nsCOMPtr<nsIFile> file;
    mFilePicker->GetFile(getter_AddRefs(file));
    if (file) {
      files.AppendElement(file);
    }
  }

  if (files.IsEmpty()) {
    Unused << Send__delete__(this, void_t(), mResult);
    return;
  }

  MOZ_ASSERT(!mRunnable);
  mRunnable = new IORunnable(this, files, mMode == nsIFilePicker::modeGetFolder);

  // Dispatch to background thread to do I/O:
  if (!mRunnable->Dispatch()) {
    Unused << Send__delete__(this, void_t(), nsIFilePicker::returnCancel);
  }
}

bool
FilePickerParent::CreateFilePicker()
{
  mFilePicker = do_CreateInstance("@mozilla.org/filepicker;1");
  if (!mFilePicker) {
    return false;
  }

  Element* element = TabParent::GetFrom(Manager())->GetOwnerElement();
  if (!element) {
    return false;
  }

  nsCOMPtr<mozIDOMWindowProxy> window = element->OwnerDoc()->GetWindow();
  if (!window) {
    return false;
  }

  return NS_SUCCEEDED(mFilePicker->Init(window, mTitle, mMode));
}

mozilla::ipc::IPCResult
FilePickerParent::RecvOpen(const int16_t& aSelectedType,
                           const bool& aAddToRecentDocs,
                           const nsString& aDefaultFile,
                           const nsString& aDefaultExtension,
                           InfallibleTArray<nsString>&& aFilters,
                           InfallibleTArray<nsString>&& aFilterNames,
                           const nsString& aDisplayDirectory,
                           const nsString& aOkButtonLabel)
{
  if (!CreateFilePicker()) {
    Unused << Send__delete__(this, void_t(), nsIFilePicker::returnCancel);
    return IPC_OK();
  }

  mFilePicker->SetAddToRecentDocs(aAddToRecentDocs);

  for (uint32_t i = 0; i < aFilters.Length(); ++i) {
    mFilePicker->AppendFilter(aFilterNames[i], aFilters[i]);
  }

  mFilePicker->SetDefaultString(aDefaultFile);
  mFilePicker->SetDefaultExtension(aDefaultExtension);
  mFilePicker->SetFilterIndex(aSelectedType);
  mFilePicker->SetOkButtonLabel(aOkButtonLabel);

  if (!aDisplayDirectory.IsEmpty()) {
    nsCOMPtr<nsIFile> localFile = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
    if (localFile) {
      localFile->InitWithPath(aDisplayDirectory);
      mFilePicker->SetDisplayDirectory(localFile);
    }
  }

  mCallback = new FilePickerShownCallback(this);

  mFilePicker->Open(mCallback);
  return IPC_OK();
}

void
FilePickerParent::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mCallback) {
    mCallback->Destroy();
    mCallback = nullptr;
  }
  if (mRunnable) {
    mRunnable->Destroy();
    mRunnable = nullptr;
  }
}
