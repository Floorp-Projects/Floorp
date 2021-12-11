/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define INITGUID
#include "mozilla/mscom/WeakRef.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/Mutex.h"
#include "nsThreadUtils.h"
#include "nsWindowsHelpers.h"
#include "nsProxyRelease.h"

static void InitializeCS(CRITICAL_SECTION& aCS) {
  DWORD flags = 0;
#if defined(RELEASE_OR_BETA)
  flags |= CRITICAL_SECTION_NO_DEBUG_INFO;
#endif
  InitializeCriticalSectionEx(&aCS, 4000, flags);
}

namespace mozilla {
namespace mscom {

namespace detail {

SharedRef::SharedRef(WeakReferenceSupport* aSupport) : mSupport(aSupport) {
  ::InitializeCS(mCS);
}

SharedRef::~SharedRef() { ::DeleteCriticalSection(&mCS); }

void SharedRef::Lock() { ::EnterCriticalSection(&mCS); }

void SharedRef::Unlock() { ::LeaveCriticalSection(&mCS); }

HRESULT
SharedRef::ToStrongRef(IWeakReferenceSource** aOutStrongReference) {
  RefPtr<IWeakReferenceSource> strongRef;

  {  // Scope for lock
    AutoCriticalSection lock(&mCS);
    if (!mSupport) {
      return E_POINTER;
    }
    strongRef = mSupport;
  }

  strongRef.forget(aOutStrongReference);
  return S_OK;
}

HRESULT
SharedRef::Resolve(REFIID aIid, void** aOutStrongReference) {
  RefPtr<WeakReferenceSupport> strongRef;

  {  // Scope for lock
    AutoCriticalSection lock(&mCS);
    if (!mSupport) {
      return E_POINTER;
    }
    strongRef = mSupport;
  }

  return strongRef->QueryInterface(aIid, aOutStrongReference);
}

void SharedRef::Clear() {
  AutoCriticalSection lock(&mCS);
  mSupport = nullptr;
}

}  // namespace detail

typedef mozilla::detail::BaseAutoLock<detail::SharedRef&> SharedRefAutoLock;
typedef mozilla::detail::BaseAutoUnlock<detail::SharedRef&> SharedRefAutoUnlock;

WeakReferenceSupport::WeakReferenceSupport(Flags aFlags)
    : mRefCnt(0), mFlags(aFlags) {
  mSharedRef = new detail::SharedRef(this);
}

HRESULT
WeakReferenceSupport::QueryInterface(REFIID riid, void** ppv) {
  RefPtr<IUnknown> punk;
  if (!ppv) {
    return E_INVALIDARG;
  }
  *ppv = nullptr;

  // Raise the refcount for stabilization purposes during aggregation
  StabilizeRefCount stabilize(*this);

  if (riid == IID_IUnknown || riid == IID_IWeakReferenceSource) {
    punk = static_cast<IUnknown*>(this);
  } else {
    HRESULT hr = WeakRefQueryInterface(riid, getter_AddRefs(punk));
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (!punk) {
    return E_NOINTERFACE;
  }

  punk.forget(ppv);
  return S_OK;
}

WeakReferenceSupport::StabilizeRefCount::StabilizeRefCount(
    WeakReferenceSupport& aObject)
    : mObject(aObject) {
  SharedRefAutoLock lock(*mObject.mSharedRef);
  ++mObject.mRefCnt;
}

WeakReferenceSupport::StabilizeRefCount::~StabilizeRefCount() {
  // We directly access these fields instead of calling Release() because we
  // want to adjust the ref count without the other side effects (such as
  // deleting this if the count drops back to zero, which may happen during
  // an initial QI during object creation).
  SharedRefAutoLock lock(*mObject.mSharedRef);
  --mObject.mRefCnt;
}

ULONG
WeakReferenceSupport::AddRef() {
  SharedRefAutoLock lock(*mSharedRef);
  ULONG result = ++mRefCnt;
  NS_LOG_ADDREF(this, result, "mscom::WeakReferenceSupport", sizeof(*this));
  return result;
}

ULONG
WeakReferenceSupport::Release() {
  ULONG newRefCnt;
  {  // Scope for lock
    SharedRefAutoLock lock(*mSharedRef);
    newRefCnt = --mRefCnt;
    if (newRefCnt == 0) {
      mSharedRef->Clear();
    }
  }
  NS_LOG_RELEASE(this, newRefCnt, "mscom::WeakReferenceSupport");
  if (newRefCnt == 0) {
    if (mFlags != Flags::eDestroyOnMainThread || NS_IsMainThread()) {
      delete this;
    } else {
      // We need to delete this object on the main thread, but we aren't on the
      // main thread right now, so we send a reference to ourselves to the main
      // thread to be re-released there.
      RefPtr<WeakReferenceSupport> self = this;
      NS_ReleaseOnMainThread("WeakReferenceSupport", self.forget());
    }
  }
  return newRefCnt;
}

HRESULT
WeakReferenceSupport::GetWeakReference(IWeakReference** aOutWeakRef) {
  if (!aOutWeakRef) {
    return E_INVALIDARG;
  }

  RefPtr<WeakRef> weakRef = MakeAndAddRef<WeakRef>(mSharedRef);
  return weakRef->QueryInterface(IID_IWeakReference, (void**)aOutWeakRef);
}

WeakRef::WeakRef(RefPtr<detail::SharedRef>& aSharedRef)
    : mRefCnt(0), mSharedRef(aSharedRef) {
  MOZ_ASSERT(aSharedRef);
}

HRESULT
WeakRef::QueryInterface(REFIID riid, void** ppv) {
  IUnknown* punk = nullptr;
  if (!ppv) {
    return E_INVALIDARG;
  }

  if (riid == IID_IUnknown || riid == IID_IWeakReference) {
    punk = static_cast<IUnknown*>(this);
  }

  *ppv = punk;
  if (!punk) {
    return E_NOINTERFACE;
  }

  punk->AddRef();
  return S_OK;
}

ULONG
WeakRef::AddRef() {
  ULONG result = ++mRefCnt;
  NS_LOG_ADDREF(this, result, "mscom::WeakRef", sizeof(*this));
  return result;
}

ULONG
WeakRef::Release() {
  ULONG newRefCnt = --mRefCnt;
  NS_LOG_RELEASE(this, newRefCnt, "mscom::WeakRef");
  if (newRefCnt == 0) {
    delete this;
  }
  return newRefCnt;
}

HRESULT
WeakRef::ToStrongRef(IWeakReferenceSource** aOutStrongReference) {
  return mSharedRef->ToStrongRef(aOutStrongReference);
}

HRESULT
WeakRef::Resolve(REFIID aIid, void** aOutStrongReference) {
  return mSharedRef->Resolve(aIid, aOutStrongReference);
}

}  // namespace mscom
}  // namespace mozilla
