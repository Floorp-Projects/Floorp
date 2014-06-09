/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set sw=4 ts=8 et tw=80 :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FilePickerParent.h"
#include "nsComponentManagerUtils.h"
#include "nsDOMFile.h"
#include "nsNetCID.h"
#include "nsIDocument.h"
#include "nsIDOMFile.h"
#include "nsIDOMWindow.h"
#include "nsIFile.h"
#include "nsISimpleEnumerator.h"
#include "mozilla/unused.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/dom/ipc/Blob.h"

using mozilla::unused;
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

// Before sending a blob to the child, we need to get its size and modification
// date. Otherwise it will be sent as a "mystery blob" by
// GetOrCreateActorForBlob, which will cause problems for the child
// process. This runnable stat()s the file off the main thread.
//
// We run code in three places:
// 1. The main thread calls Dispatch() to start the runnable.
// 2. The stream transport thread stat()s the file in Run() and then dispatches
// the same runnable on the main thread.
// 3. The main thread sends the results over IPC.
FilePickerParent::FileSizeAndDateRunnable::FileSizeAndDateRunnable(FilePickerParent *aFPParent,
                                                                   nsCOMArray<nsIDOMFile>& aDomfiles)
 : mFilePickerParent(aFPParent)
{
  mDomfiles.SwapElements(aDomfiles);
}

bool
FilePickerParent::FileSizeAndDateRunnable::Dispatch()
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
FilePickerParent::FileSizeAndDateRunnable::Run()
{
  // If we're on the main thread, then that means we're done. Just send the
  // results.
  if (NS_IsMainThread()) {
    if (mFilePickerParent) {
      mFilePickerParent->SendFiles(mDomfiles);
    }
    return NS_OK;
  }

  // We're not on the main thread, so do the stat().
  for (unsigned i = 0; i < mDomfiles.Length(); i++) {
    uint64_t size, lastModified;
    mDomfiles[i]->GetSize(&size);
    mDomfiles[i]->GetMozLastModifiedDate(&lastModified);
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
FilePickerParent::FileSizeAndDateRunnable::Destroy()
{
  mFilePickerParent = nullptr;
}

void
FilePickerParent::SendFiles(const nsCOMArray<nsIDOMFile>& aDomfiles)
{
  ContentParent* parent = static_cast<ContentParent*>(Manager()->Manager());
  InfallibleTArray<PBlobParent*> files;

  for (unsigned i = 0; i < aDomfiles.Length(); i++) {
    BlobParent* blob = parent->GetOrCreateActorForBlob(aDomfiles[i]);
    if (blob) {
      files.AppendElement(blob);
    }
  }

  InputFiles infiles;
  infiles.filesParent().SwapElements(files);
  unused << Send__delete__(this, infiles, mResult);
}

void
FilePickerParent::Done(int16_t aResult)
{
  mResult = aResult;

  if (mResult != nsIFilePicker::returnOK) {
    unused << Send__delete__(this, void_t(), mResult);
    return;
  }

  nsCOMArray<nsIDOMFile> domfiles;
  if (mMode == nsIFilePicker::modeOpenMultiple) {
    nsCOMPtr<nsISimpleEnumerator> iter;
    NS_ENSURE_SUCCESS_VOID(mFilePicker->GetFiles(getter_AddRefs(iter)));

    nsCOMPtr<nsISupports> supports;
    bool loop = true;
    while (NS_SUCCEEDED(iter->HasMoreElements(&loop)) && loop) {
      iter->GetNext(getter_AddRefs(supports));
      if (supports) {
        nsCOMPtr<nsIFile> file = do_QueryInterface(supports);
        nsCOMPtr<nsIDOMFile> domfile = new nsDOMFileFile(file);
        domfiles.AppendElement(domfile);
      }
    }
  } else {
    nsCOMPtr<nsIFile> file;
    mFilePicker->GetFile(getter_AddRefs(file));
    if (file) {
      nsCOMPtr<nsIDOMFile> domfile = new nsDOMFileFile(file);
      domfiles.AppendElement(domfile);
    }
  }

  MOZ_ASSERT(!mRunnable);
  mRunnable = new FileSizeAndDateRunnable(this, domfiles);
  if (!mRunnable->Dispatch()) {
    unused << Send__delete__(this, void_t(), nsIFilePicker::returnCancel);
  }
}

bool
FilePickerParent::CreateFilePicker()
{
  mFilePicker = do_CreateInstance("@mozilla.org/filepicker;1");
  if (!mFilePicker) {
    return false;
  }

  Element* element = static_cast<TabParent*>(Manager())->GetOwnerElement();
  if (!element) {
    return false;
  }

  nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(element->OwnerDoc()->GetWindow());
  if (!window) {
    return false;
  }

  return NS_SUCCEEDED(mFilePicker->Init(window, mTitle, mMode));
}

bool
FilePickerParent::RecvOpen(const int16_t& aSelectedType,
                           const bool& aAddToRecentDocs,
                           const nsString& aDefaultFile,
                           const nsString& aDefaultExtension,
                           const InfallibleTArray<nsString>& aFilters,
                           const InfallibleTArray<nsString>& aFilterNames)
{
  if (!CreateFilePicker()) {
    unused << Send__delete__(this, void_t(), nsIFilePicker::returnCancel);
    return true;
  }

  mFilePicker->SetAddToRecentDocs(aAddToRecentDocs);

  for (uint32_t i = 0; i < aFilters.Length(); ++i) {
    mFilePicker->AppendFilter(aFilterNames[i], aFilters[i]);
  }

  mFilePicker->SetDefaultString(aDefaultFile);
  mFilePicker->SetDefaultExtension(aDefaultExtension);
  mFilePicker->SetFilterIndex(aSelectedType);

  mCallback = new FilePickerShownCallback(this);

  mFilePicker->Open(mCallback);
  return true;
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
