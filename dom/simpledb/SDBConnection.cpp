/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SDBConnection.h"

// Local includes
#include "ActorsChild.h"
#include "SDBRequest.h"
#include "SimpleDBCommon.h"

// Global includes
#include <stdint.h>
#include <utility>
#include "MainThreadUtils.h"
#include "js/ArrayBuffer.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/experimental/TypedData.h"
#include "mozilla/Assertions.h"
#include "mozilla/MacroForEach.h"
#include "mozilla/Maybe.h"
#include "mozilla/Preferences.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Variant.h"
#include "mozilla/dom/PBackgroundSDBConnection.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/fallible.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsISDBCallbacks.h"
#include "nsISupportsUtils.h"
#include "nsStringFwd.h"
#include "nscore.h"

namespace mozilla::dom {

using namespace mozilla::ipc;

namespace {

nsresult GetWriteData(JSContext* aCx, JS::Handle<JS::Value> aValue,
                      nsCString& aData) {
  if (aValue.isObject()) {
    JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());

    bool isView = false;
    if (JS::IsArrayBufferObject(obj) ||
        (isView = JS_IsArrayBufferViewObject(obj))) {
      uint8_t* data;
      size_t length;
      bool unused;
      if (isView) {
        JS_GetObjectAsArrayBufferView(obj, &length, &unused, &data);
      } else {
        JS::GetObjectAsArrayBuffer(obj, &length, &data);
      }

      // Throw for large buffers to prevent truncation.
      if (length > INT32_MAX) {
        return NS_ERROR_ILLEGAL_VALUE;
      }

      if (NS_WARN_IF(!aData.Assign(reinterpret_cast<char*>(data), length,
                                   fallible_t()))) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      return NS_OK;
    }
  }

  return NS_ERROR_NOT_IMPLEMENTED;
}

}  // namespace

SDBConnection::SDBConnection()
    : mBackgroundActor(nullptr),
      mPersistenceType(quota::PERSISTENCE_TYPE_INVALID),
      mRunningRequest(false),
      mOpen(false),
      mAllowedToClose(false) {
  AssertIsOnOwningThread();
}

SDBConnection::~SDBConnection() {
  AssertIsOnOwningThread();

  if (mBackgroundActor) {
    mBackgroundActor->SendDeleteMeInternal();
    MOZ_ASSERT(!mBackgroundActor, "SendDeleteMeInternal should have cleared!");
  }
}

// static
nsresult SDBConnection::Create(REFNSIID aIID, void** aResult) {
  MOZ_ASSERT(aResult);

  if (NS_WARN_IF(!Preferences::GetBool(kPrefSimpleDBEnabled, false))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  RefPtr<SDBConnection> connection = new SDBConnection();

  nsresult rv = connection->QueryInterface(aIID, aResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void SDBConnection::ClearBackgroundActor() {
  AssertIsOnOwningThread();

  mBackgroundActor = nullptr;
}

void SDBConnection::OnNewRequest() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mRunningRequest);

  mRunningRequest = true;
}

void SDBConnection::OnRequestFinished() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mRunningRequest);

  mRunningRequest = false;
}

void SDBConnection::OnOpen() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mOpen);

  mOpen = true;
}

void SDBConnection::OnClose(bool aAbnormal) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mOpen);

  mOpen = false;

  if (aAbnormal) {
    MOZ_ASSERT(mAllowedToClose);

    if (mCloseCallback) {
      mCloseCallback->OnClose(this);
    }
  }
}

void SDBConnection::AllowToClose() {
  AssertIsOnOwningThread();

  mAllowedToClose = true;
}

nsresult SDBConnection::CheckState() {
  AssertIsOnOwningThread();

  if (mAllowedToClose) {
    return NS_ERROR_ABORT;
  }

  if (mRunningRequest) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return NS_OK;
}

nsresult SDBConnection::EnsureBackgroundActor() {
  AssertIsOnOwningThread();

  if (mBackgroundActor) {
    return NS_OK;
  }

  PBackgroundChild* backgroundActor =
      BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!backgroundActor)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<SDBConnectionChild> actor = new SDBConnectionChild(this);

  mBackgroundActor = static_cast<SDBConnectionChild*>(
      backgroundActor->SendPBackgroundSDBConnectionConstructor(
          actor, mPersistenceType, *mPrincipalInfo));
  if (NS_WARN_IF(!mBackgroundActor)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult SDBConnection::InitiateRequest(SDBRequest* aRequest,
                                        const SDBRequestParams& aParams) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aRequest);
  MOZ_ASSERT(mBackgroundActor);

  auto actor = new SDBRequestChild(aRequest);

  if (!mBackgroundActor->SendPBackgroundSDBRequestConstructor(actor, aParams)) {
    return NS_ERROR_FAILURE;
  }

  // Balanced in SDBRequestChild::Recv__delete__().
  OnNewRequest();

  return NS_OK;
}

NS_IMPL_ISUPPORTS(SDBConnection, nsISDBConnection)

NS_IMETHODIMP
SDBConnection::Init(nsIPrincipal* aPrincipal,
                    const nsACString& aPersistenceType) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  UniquePtr<PrincipalInfo> principalInfo(new PrincipalInfo());
  nsresult rv = PrincipalToPrincipalInfo(aPrincipal, principalInfo.get());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (principalInfo->type() != PrincipalInfo::TContentPrincipalInfo &&
      principalInfo->type() != PrincipalInfo::TSystemPrincipalInfo) {
    NS_WARNING("Simpledb not allowed for this principal!");
    return NS_ERROR_INVALID_ARG;
  }

  if (NS_WARN_IF(!quota::QuotaManager::IsPrincipalInfoValid(*principalInfo))) {
    return NS_ERROR_INVALID_ARG;
  }

  PersistenceType persistenceType;
  if (aPersistenceType.IsVoid()) {
    persistenceType = quota::PERSISTENCE_TYPE_DEFAULT;
  } else {
    const auto maybePersistenceType =
        quota::PersistenceTypeFromString(aPersistenceType, fallible);
    if (NS_WARN_IF(maybePersistenceType.isNothing())) {
      return NS_ERROR_INVALID_ARG;
    }

    persistenceType = maybePersistenceType.value();
  }

  mPrincipalInfo = std::move(principalInfo);
  mPersistenceType = persistenceType;

  return NS_OK;
}

NS_IMETHODIMP
SDBConnection::Open(const nsAString& aName, nsISDBRequest** _retval) {
  AssertIsOnOwningThread();

  nsresult rv = CheckState();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (mOpen) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  SDBRequestOpenParams params;
  params.name() = aName;

  RefPtr<SDBRequest> request = new SDBRequest(this);

  rv = EnsureBackgroundActor();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = InitiateRequest(request, params);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
SDBConnection::Seek(uint64_t aOffset, nsISDBRequest** _retval) {
  AssertIsOnOwningThread();

  nsresult rv = CheckState();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!mOpen) {
    return NS_BASE_STREAM_CLOSED;
  }

  SDBRequestSeekParams params;
  params.offset() = aOffset;

  RefPtr<SDBRequest> request = new SDBRequest(this);

  rv = InitiateRequest(request, params);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
SDBConnection::Read(uint64_t aSize, nsISDBRequest** _retval) {
  AssertIsOnOwningThread();

  nsresult rv = CheckState();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!mOpen) {
    return NS_BASE_STREAM_CLOSED;
  }

  SDBRequestReadParams params;
  params.size() = aSize;

  RefPtr<SDBRequest> request = new SDBRequest(this);

  rv = InitiateRequest(request, params);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
SDBConnection::Write(JS::Handle<JS::Value> aValue, JSContext* aCx,
                     nsISDBRequest** _retval) {
  AssertIsOnOwningThread();

  nsresult rv = CheckState();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!mOpen) {
    return NS_BASE_STREAM_CLOSED;
  }

  JS::Rooted<JS::Value> value(aCx, aValue);

  nsCString data;
  rv = GetWriteData(aCx, value, data);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  SDBRequestWriteParams params;
  params.data() = data;

  RefPtr<SDBRequest> request = new SDBRequest(this);

  rv = InitiateRequest(request, params);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
SDBConnection::Close(nsISDBRequest** _retval) {
  AssertIsOnOwningThread();

  nsresult rv = CheckState();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!mOpen) {
    return NS_BASE_STREAM_CLOSED;
  }

  SDBRequestCloseParams params;

  RefPtr<SDBRequest> request = new SDBRequest(this);

  rv = InitiateRequest(request, params);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
SDBConnection::GetCloseCallback(nsISDBCloseCallback** aCloseCallback) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aCloseCallback);

  NS_IF_ADDREF(*aCloseCallback = mCloseCallback);
  return NS_OK;
}

NS_IMETHODIMP
SDBConnection::SetCloseCallback(nsISDBCloseCallback* aCloseCallback) {
  AssertIsOnOwningThread();

  mCloseCallback = aCloseCallback;
  return NS_OK;
}

}  // namespace mozilla::dom
