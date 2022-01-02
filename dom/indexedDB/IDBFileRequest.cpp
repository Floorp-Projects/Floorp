/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBFileRequest.h"

#include "IDBFileHandle.h"
#include "js/RootingAPI.h"
#include "jsapi.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/IDBFileRequestBinding.h"
#include "mozilla/dom/ProgressEvent.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/EventDispatcher.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsLiteralString.h"

namespace mozilla::dom {

using namespace mozilla::dom::indexedDB;

IDBFileRequest::IDBFileRequest(IDBFileHandle* aFileHandle,
                               bool aWrapAsDOMRequest)
    : DOMRequest(aFileHandle->GetOwnerGlobal()),
      mFileHandle(aFileHandle),
      mWrapAsDOMRequest(aWrapAsDOMRequest),
      mHasEncoding(false) {
  MOZ_ASSERT(aFileHandle);
  aFileHandle->AssertIsOnOwningThread();
}

IDBFileRequest::~IDBFileRequest() { AssertIsOnOwningThread(); }

// static
RefPtr<IDBFileRequest> IDBFileRequest::Create(IDBFileHandle* aFileHandle,
                                              bool aWrapAsDOMRequest) {
  MOZ_ASSERT(aFileHandle);
  aFileHandle->AssertIsOnOwningThread();

  return new IDBFileRequest(aFileHandle, aWrapAsDOMRequest);
}

void IDBFileRequest::FireProgressEvent(uint64_t aLoaded, uint64_t aTotal) {
  AssertIsOnOwningThread();

  if (NS_FAILED(CheckCurrentGlobalCorrectness())) {
    return;
  }

  ProgressEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mLengthComputable = false;
  init.mLoaded = aLoaded;
  init.mTotal = aTotal;

  RefPtr<ProgressEvent> event =
      ProgressEvent::Constructor(this, u"progress"_ns, init);
  DispatchTrustedEvent(event);
}

NS_IMPL_ADDREF_INHERITED(IDBFileRequest, DOMRequest)
NS_IMPL_RELEASE_INHERITED(IDBFileRequest, DOMRequest)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IDBFileRequest)
NS_INTERFACE_MAP_END_INHERITING(DOMRequest)

NS_IMPL_CYCLE_COLLECTION_INHERITED(IDBFileRequest, DOMRequest, mFileHandle)

void IDBFileRequest::GetEventTargetParent(EventChainPreVisitor& aVisitor) {
  AssertIsOnOwningThread();

  aVisitor.mCanHandle = true;
  aVisitor.SetParentTarget(mFileHandle, false);
}

// virtual
JSObject* IDBFileRequest::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  AssertIsOnOwningThread();

  if (mWrapAsDOMRequest) {
    return DOMRequest::WrapObject(aCx, aGivenProto);
  }
  return IDBFileRequest_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
