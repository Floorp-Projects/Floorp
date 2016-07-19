/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define INITGUID
#include "mozilla/mscom/WeakRef.h"

#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/RefPtr.h"
#include "nsThreadUtils.h"
#include "nsWindowsHelpers.h"

namespace mozilla {
namespace mscom {

WeakReferenceSupport::WeakReferenceSupport(Flags aFlags)
  : mRefCnt(1)
  , mFlags(aFlags)
{
  ::InitializeCriticalSectionAndSpinCount(&mCS, 4000);
}

WeakReferenceSupport::~WeakReferenceSupport()
{
  MOZ_ASSERT(mWeakRefs.IsEmpty());
  ::DeleteCriticalSection(&mCS);
}

HRESULT
WeakReferenceSupport::QueryInterface(REFIID riid, void** ppv)
{
  AutoCriticalSection lock(&mCS);
  RefPtr<IUnknown> punk;
  if (!ppv) {
    return E_INVALIDARG;
  }
  *ppv = nullptr;

  // Raise the refcount for stabilization purposes during aggregation
  RefPtr<IUnknown> kungFuDeathGrip(static_cast<IUnknown*>(this));

  if (riid == IID_IUnknown || riid == IID_IWeakReferenceSource) {
    punk = static_cast<IUnknown*>(this);
  } else {
    HRESULT hr = ThreadSafeQueryInterface(riid, getter_AddRefs(punk));
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

ULONG
WeakReferenceSupport::AddRef()
{
  AutoCriticalSection lock(&mCS);
  return ++mRefCnt;
}

ULONG
WeakReferenceSupport::Release()
{
  ULONG newRefCnt;
  { // Scope for lock
    AutoCriticalSection lock(&mCS);
    newRefCnt = --mRefCnt;
    if (newRefCnt == 0) {
      ClearWeakRefs();
    }
  }
  if (newRefCnt == 0) {
    if (mFlags != Flags::eDestroyOnMainThread || NS_IsMainThread()) {
      delete this;
    } else {
      // It is possible for the last Release() call to happen off-main-thread.
      // If so, we need to dispatch an event to delete ourselves.
      mozilla::DebugOnly<nsresult> rv =
        NS_DispatchToMainThread(NS_NewRunnableFunction([this]() -> void
        {
          delete this;
        }));
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }
  return newRefCnt;
}

void
WeakReferenceSupport::ClearWeakRefs()
{
  for (uint32_t i = 0, len = mWeakRefs.Length(); i < len; ++i) {
    mWeakRefs[i]->Clear();
    mWeakRefs[i]->Release();
  }
  mWeakRefs.Clear();
}

HRESULT
WeakReferenceSupport::GetWeakReference(IWeakReference** aOutWeakRef)
{
  if (!aOutWeakRef) {
    return E_INVALIDARG;
  }
  *aOutWeakRef = nullptr;

  AutoCriticalSection lock(&mCS);
  RefPtr<WeakRef> weakRef = MakeAndAddRef<WeakRef>(this);

  HRESULT hr = weakRef->QueryInterface(IID_IWeakReference, (void**)aOutWeakRef);
  if (FAILED(hr)) {
    return hr;
  }

  mWeakRefs.AppendElement(weakRef.get());
  weakRef->AddRef();
  return S_OK;
}

WeakRef::WeakRef(WeakReferenceSupport* aSupport)
  : mRefCnt(1)
  , mMutex("mozilla::mscom::WeakRef::mMutex")
  , mSupport(aSupport)
{
  MOZ_ASSERT(aSupport);
}

HRESULT
WeakRef::QueryInterface(REFIID riid, void** ppv)
{
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
WeakRef::AddRef()
{
  return (ULONG) InterlockedIncrement((LONG*)&mRefCnt);
}

ULONG
WeakRef::Release()
{
  ULONG newRefCnt = (ULONG) InterlockedDecrement((LONG*)&mRefCnt);
  if (newRefCnt == 0) {
    delete this;
  }
  return newRefCnt;
}

HRESULT
WeakRef::Resolve(REFIID aIid, void** aOutStrongReference)
{
  MutexAutoLock lock(mMutex);
  if (!mSupport) {
    return E_FAIL;
  }
  return mSupport->QueryInterface(aIid, aOutStrongReference);
}

void
WeakRef::Clear()
{
  MutexAutoLock lock(mMutex);
  MOZ_ASSERT(mSupport);
  mSupport = nullptr;
}

} // namespace mscom
} // namespace mozilla

