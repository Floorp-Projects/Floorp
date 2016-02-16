/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBFileHandle.h"

#include "IDBEvents.h"
#include "IDBMutableFile.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/IDBFileHandleBinding.h"
#include "mozilla/dom/filehandle/ActorsChild.h"
#include "mozilla/EventDispatcher.h"
#include "nsServiceManagerUtils.h"
#include "nsWidgetsCID.h"

namespace mozilla {
namespace dom {

using namespace mozilla::dom::indexedDB;

IDBFileHandle::IDBFileHandle(FileMode aMode,
                             IDBMutableFile* aMutableFile)
  : FileHandleBase(DEBUGONLY(aMutableFile->OwningThread(),)
                   aMode)
  , mMutableFile(aMutableFile)
{
  AssertIsOnOwningThread();
}

IDBFileHandle::~IDBFileHandle()
{
  AssertIsOnOwningThread();

  mMutableFile->UnregisterFileHandle(this);
}

// static
already_AddRefed<IDBFileHandle>
IDBFileHandle::Create(IDBMutableFile* aMutableFile,
                      FileMode aMode)
{
  MOZ_ASSERT(aMutableFile);
  aMutableFile->AssertIsOnOwningThread();
  MOZ_ASSERT(aMode == FileMode::Readonly || aMode == FileMode::Readwrite);

  RefPtr<IDBFileHandle> fileHandle =
    new IDBFileHandle(aMode, aMutableFile);

  fileHandle->BindToOwner(aMutableFile);

  // XXX Fix!
  MOZ_ASSERT(NS_IsMainThread(), "This won't work on non-main threads!");

  nsCOMPtr<nsIRunnable> runnable = do_QueryObject(fileHandle);
  nsContentUtils::RunInMetastableState(runnable.forget());

  fileHandle->SetCreating();

  aMutableFile->RegisterFileHandle(fileHandle);

  return fileHandle.forget();
}

already_AddRefed<IDBFileRequest>
IDBFileHandle::GetMetadata(const IDBFileMetadataParameters& aParameters,
                           ErrorResult& aRv)
{
  AssertIsOnOwningThread();

  // Common state checking
  if (!CheckState(aRv)) {
    return nullptr;
  }

  // Argument checking for get metadata.
  if (!aParameters.mSize && !aParameters.mLastModified) {
    aRv.ThrowTypeError<MSG_METADATA_NOT_CONFIGURED>();
    return nullptr;
  }

 // Do nothing if the window is closed
  if (!CheckWindow()) {
    return nullptr;
  }

  FileRequestGetMetadataParams params;
  params.size() = aParameters.mSize;
  params.lastModified() = aParameters.mLastModified;

  RefPtr<FileRequestBase> fileRequest = GenerateFileRequest();

  StartRequest(fileRequest, params);

  return fileRequest.forget().downcast<IDBFileRequest>();
}

NS_IMPL_ADDREF_INHERITED(IDBFileHandle, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(IDBFileHandle, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBFileHandle)
  NS_INTERFACE_MAP_ENTRY(nsIRunnable)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBFileHandle)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBFileHandle,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMutableFile)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBFileHandle,
                                                DOMEventTargetHelper)
  // Don't unlink mMutableFile!
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMETHODIMP
IDBFileHandle::Run()
{
  AssertIsOnOwningThread();

  OnReturnToEventLoop();

  return NS_OK;
}

nsresult
IDBFileHandle::PreHandleEvent(EventChainPreVisitor& aVisitor)
{
  AssertIsOnOwningThread();

  aVisitor.mCanHandle = true;
  aVisitor.mParentTarget = mMutableFile;
  return NS_OK;
}

// virtual
JSObject*
IDBFileHandle::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  AssertIsOnOwningThread();

  return IDBFileHandleBinding::Wrap(aCx, this, aGivenProto);
}

mozilla::dom::MutableFileBase*
IDBFileHandle::MutableFile() const
{
  AssertIsOnOwningThread();

  return mMutableFile;
}

void
IDBFileHandle::HandleCompleteOrAbort(bool aAborted)
{
  AssertIsOnOwningThread();

  FileHandleBase::HandleCompleteOrAbort(aAborted);

  nsCOMPtr<nsIDOMEvent> event;
  if (aAborted) {
    event = CreateGenericEvent(this, nsDependentString(kAbortEventType),
                               eDoesBubble, eNotCancelable);
  } else {
    event = CreateGenericEvent(this, nsDependentString(kCompleteEventType),
                               eDoesNotBubble, eNotCancelable);
  }
  if (NS_WARN_IF(!event)) {
    return;
  }

  bool dummy;
  if (NS_FAILED(DispatchEvent(event, &dummy))) {
    NS_WARNING("DispatchEvent failed!");
  }
}

bool
IDBFileHandle::CheckWindow()
{
  AssertIsOnOwningThread();

  return GetOwner();
}

already_AddRefed<mozilla::dom::FileRequestBase>
IDBFileHandle::GenerateFileRequest()
{
  AssertIsOnOwningThread();

  return IDBFileRequest::Create(GetOwner(), this,
                                /* aWrapAsDOMRequest */ false);
}

} // namespace dom
} // namespace mozilla
