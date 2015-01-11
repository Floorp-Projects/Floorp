/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBFileHandle.h"

#include "IDBEvents.h"
#include "IDBMutableFile.h"
#include "mozilla/dom/FileService.h"
#include "mozilla/dom/IDBFileHandleBinding.h"
#include "mozilla/dom/MetadataHelper.h"
#include "mozilla/EventDispatcher.h"
#include "nsIAppShell.h"
#include "nsServiceManagerUtils.h"
#include "nsWidgetsCID.h"

namespace mozilla {
namespace dom {
namespace indexedDB {

namespace {

NS_DEFINE_CID(kAppShellCID2, NS_APPSHELL_CID);

} // anonymous namespace

IDBFileHandle::IDBFileHandle(FileMode aMode,
                             RequestMode aRequestMode,
                             IDBMutableFile* aMutableFile)
  : FileHandleBase(aMode, aRequestMode)
  , mMutableFile(aMutableFile)
{
}

IDBFileHandle::~IDBFileHandle()
{
}

// static
already_AddRefed<IDBFileHandle>
IDBFileHandle::Create(FileMode aMode,
                      RequestMode aRequestMode,
                      IDBMutableFile* aMutableFile)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<IDBFileHandle> fileHandle =
    new IDBFileHandle(aMode, aRequestMode, aMutableFile);

  fileHandle->BindToOwner(aMutableFile);

  nsCOMPtr<nsIAppShell> appShell = do_GetService(kAppShellCID2);
  if (NS_WARN_IF(!appShell)) {
    return nullptr;
  }

  nsresult rv = appShell->RunBeforeNextEvent(fileHandle);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  fileHandle->SetCreating();

  FileService* service = FileService::GetOrCreate();
  if (NS_WARN_IF(!service)) {
    return nullptr;
  }

  rv = service->Enqueue(fileHandle, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  return fileHandle.forget();
}

mozilla::dom::MutableFileBase*
IDBFileHandle::MutableFile() const
{
  return mMutableFile;
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(IDBFileHandle, DOMEventTargetHelper,
                                   mMutableFile)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBFileHandle)
  NS_INTERFACE_MAP_ENTRY(nsIRunnable)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(IDBFileHandle, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(IDBFileHandle, DOMEventTargetHelper)

nsresult
IDBFileHandle::PreHandleEvent(EventChainPreVisitor& aVisitor)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  aVisitor.mCanHandle = true;
  aVisitor.mParentTarget = mMutableFile;
  return NS_OK;
}

// virtual
JSObject*
IDBFileHandle::WrapObject(JSContext* aCx)
{
  return IDBFileHandleBinding::Wrap(aCx, this);
}

already_AddRefed<IDBFileRequest>
IDBFileHandle::GetMetadata(const IDBFileMetadataParameters& aParameters,
                           ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  // Common state checking
  if (!CheckState(aRv)) {
    return nullptr;
  }

  // Do nothing if the window is closed
  if (!CheckWindow()) {
    return nullptr;
  }

  nsRefPtr<MetadataParameters> params =
    new MetadataParameters(aParameters.mSize, aParameters.mLastModified);
  if (!params->IsConfigured()) {
    aRv.ThrowTypeError(MSG_METADATA_NOT_CONFIGURED);
    return nullptr;
  }

  nsRefPtr<FileRequestBase> fileRequest = GenerateFileRequest();

  nsRefPtr<MetadataHelper> helper =
    new MetadataHelper(this, fileRequest, params);

  if (NS_WARN_IF(NS_FAILED(helper->Enqueue()))) {
    aRv.Throw(NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);
    return nullptr;
  }

  return fileRequest.forget().downcast<IDBFileRequest>();
}

NS_IMETHODIMP
IDBFileHandle::Run()
{
  OnReturnToEventLoop();
  return NS_OK;
}

nsresult
IDBFileHandle::OnCompleteOrAbort(bool aAborted)
{
  nsCOMPtr<nsIDOMEvent> event;
  if (aAborted) {
    event = CreateGenericEvent(this, nsDependentString(kAbortEventType),
                               eDoesBubble, eNotCancelable);
  } else {
    event = CreateGenericEvent(this, nsDependentString(kCompleteEventType),
                               eDoesNotBubble, eNotCancelable);
  }
  if (NS_WARN_IF(!event)) {
    return NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR;
  }

  bool dummy;
  if (NS_FAILED(DispatchEvent(event, &dummy))) {
    NS_WARNING("Dispatch failed!");
  }

  return NS_OK;
}

bool
IDBFileHandle::CheckWindow()
{
  return GetOwner();
}

already_AddRefed<mozilla::dom::FileRequestBase>
IDBFileHandle::GenerateFileRequest()
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  return IDBFileRequest::Create(GetOwner(), this,
                                /* aWrapAsDOMRequest */ false);
}

} // namespace indexedDB
} // namespace dom
} // namespace mozilla
