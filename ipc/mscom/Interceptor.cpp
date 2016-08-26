/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define INITGUID
#include "mozilla/mscom/Interceptor.h"
#include "mozilla/mscom/InterceptorLog.h"

#include "mozilla/mscom/DispatchForwarder.h"
#include "mozilla/mscom/MainThreadInvoker.h"
#include "mozilla/mscom/Registration.h"
#include "mozilla/mscom/Utils.h"
#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace mscom {

/* static */ HRESULT
Interceptor::Create(STAUniquePtr<IUnknown> aTarget, IInterceptorSink* aSink,
                    REFIID aIid, void** aOutput)
{
  MOZ_ASSERT(aOutput && aTarget && aSink);
  if (!aOutput) {
    return E_INVALIDARG;
  }
  *aOutput = nullptr;
  if (!aTarget || !aSink) {
    return E_INVALIDARG;
  }
  Interceptor* intcpt = new Interceptor(Move(aTarget), aSink);
  HRESULT hr = intcpt->QueryInterface(aIid, aOutput);
  static_cast<WeakReferenceSupport*>(intcpt)->Release();
  return hr;
}

Interceptor::Interceptor(STAUniquePtr<IUnknown> aTarget, IInterceptorSink* aSink)
  : WeakReferenceSupport(WeakReferenceSupport::Flags::eDestroyOnMainThread)
  , mTarget(Move(aTarget))
  , mEventSink(aSink)
  , mMutex("mozilla::mscom::Interceptor::mMutex")
{
  MOZ_ASSERT(aSink);
  MOZ_ASSERT(!IsProxy(mTarget.get()));
  RefPtr<IWeakReference> weakRef;
  if (SUCCEEDED(GetWeakReference(getter_AddRefs(weakRef)))) {
    aSink->SetInterceptor(weakRef);
  }
}

Interceptor::~Interceptor()
{
  // This needs to run on the main thread because it releases target interface
  // reference counts which may not be thread-safe.
  MOZ_ASSERT(NS_IsMainThread());
  for (uint32_t index = 0, len = mInterceptorMap.Length(); index < len; ++index) {
    MapEntry& entry = mInterceptorMap[index];
    entry.mInterceptor->Release();
    entry.mTargetInterface->Release();
  }
}

Interceptor::MapEntry*
Interceptor::Lookup(REFIID aIid)
{
  mMutex.AssertCurrentThreadOwns();
  for (uint32_t index = 0, len = mInterceptorMap.Length(); index < len; ++index) {
    if (mInterceptorMap[index].mIID == aIid) {
      return &mInterceptorMap[index];
    }
  }
  return nullptr;
}

HRESULT
Interceptor::GetTargetForIID(REFIID aIid, InterceptorTargetPtr& aTarget)
{
  MutexAutoLock lock(mMutex);
  MapEntry* entry = Lookup(aIid);
  if (entry) {
    aTarget.reset(entry->mTargetInterface);
    return S_OK;
  }

  return E_NOINTERFACE;
}

// CoGetInterceptor requires information from a typelib to be able to
// generate its emulated vtable. If a typelib is unavailable,
// CoGetInterceptor returns 0x80070002.
static const HRESULT kFileNotFound = 0x80070002;

HRESULT
Interceptor::CreateInterceptor(REFIID aIid, IUnknown* aOuter, IUnknown** aOutput)
{
  // In order to aggregate, we *must* request IID_IUnknown as the initial
  // interface for the interceptor, as that IUnknown is non-delegating.
  // This is a fundamental rule for creating aggregated objects in COM.
  HRESULT hr = ::CoGetInterceptor(aIid, aOuter, IID_IUnknown, (void**)aOutput);
  if (hr != kFileNotFound) {
    return hr;
  }

  // In the case that CoGetInterceptor returns kFileNotFound, we can try to
  // explicitly load typelib data from our runtime registration facility and
  // pass that into CoGetInterceptorFromTypeInfo.

  RefPtr<ITypeInfo> typeInfo;
  bool found = RegisteredProxy::Find(aIid, getter_AddRefs(typeInfo));
  // If this assert fires then we have omitted registering the typelib for a
  // required interface. To fix this, review our calls to mscom::RegisterProxy
  // and mscom::RegisterTypelib, and add the additional typelib as necessary.
  MOZ_ASSERT(found);
  if (!found) {
    return kFileNotFound;
  }

  hr = ::CoGetInterceptorFromTypeInfo(aIid, aOuter, typeInfo, IID_IUnknown,
                                      (void**)aOutput);
  // If this assert fires then the interceptor doesn't like something about
  // the format of the typelib. One thing in particular that it doesn't like
  // is complex types that contain unions.
  MOZ_ASSERT(SUCCEEDED(hr));
  return hr;
}

/**
 * This method contains the core guts of the handling of QueryInterface calls
 * that are delegated to us from the ICallInterceptor.
 *
 * @param aIid ID of the desired interface
 * @param aOutInterceptor The resulting emulated vtable that corresponds to
 * the interface specified by aIid.
 */
HRESULT
Interceptor::GetInterceptorForIID(REFIID aIid, void** aOutInterceptor)
{
  if (!aOutInterceptor) {
    return E_INVALIDARG;
  }

  if (aIid == IID_IUnknown) {
    // Special case: When we see IUnknown, we just provide a reference to this
    *aOutInterceptor = static_cast<IInterceptor*>(this);
    AddRef();
    return S_OK;
  }

  RefPtr<IUnknown> unkInterceptor;
  IUnknown* interfaceForQILog = nullptr;

  // (1) Check to see if we already have an existing interceptor for aIid.

  { // Scope for lock
    MutexAutoLock lock(mMutex);
    MapEntry* entry = Lookup(aIid);
    if (entry) {
      unkInterceptor = entry->mInterceptor;
      interfaceForQILog = entry->mTargetInterface;
    }
  }

  // (1a) A COM interceptor already exists for this interface, so all we need
  // to do is run a QI on it.
  if (unkInterceptor) {
    // Technically we didn't actually execute a QI on the target interface, but
    // for logging purposes we would like to record the fact that this interface
    // was requested.
    InterceptorLog::QI(S_OK, mTarget.get(), aIid, interfaceForQILog);

    return unkInterceptor->QueryInterface(aIid, aOutInterceptor);
  }

  // (2) Obtain a new target interface.

  // (2a) First, make sure that the target interface is available
  // NB: We *MUST* query the correct interface! ICallEvents::Invoke casts its
  // pvReceiver argument directly to the required interface! DO NOT assume
  // that COM will use QI or upcast/downcast!
  HRESULT hr;

  STAUniquePtr<IUnknown> targetInterface;
  IUnknown* rawTargetInterface = nullptr;
  hr = QueryInterfaceTarget(aIid, (void**)&rawTargetInterface);
  targetInterface.reset(rawTargetInterface);
  InterceptorLog::QI(hr, mTarget.get(), aIid, targetInterface.get());
  MOZ_ASSERT(SUCCEEDED(hr) || hr == E_NOINTERFACE);
  if (FAILED(hr)) {
    return hr;
  }

  // We *really* shouldn't be adding interceptors to proxies
  MOZ_ASSERT(aIid != IID_IMarshal);

  // (3) Create a new COM interceptor to that interface that delegates its
  // IUnknown to |this|.

  // Raise the refcount for stabilization purposes during aggregation
  RefPtr<IUnknown> kungFuDeathGrip(static_cast<IUnknown*>(
        static_cast<WeakReferenceSupport*>(this)));

  hr = CreateInterceptor(aIid, kungFuDeathGrip, getter_AddRefs(unkInterceptor));
  if (FAILED(hr)) {
    return hr;
  }

  // (4) Obtain the interceptor's ICallInterceptor interface and register our
  // event sink.
  RefPtr<ICallInterceptor> interceptor;
  hr = unkInterceptor->QueryInterface(IID_ICallInterceptor,
                                      (void**)getter_AddRefs(interceptor));
  if (FAILED(hr)) {
    return hr;
  }

  hr = interceptor->RegisterSink(mEventSink);
  if (FAILED(hr)) {
    return hr;
  }

  // (5) Now that we have this new COM interceptor, insert it into the map.

  { // Scope for lock
    MutexAutoLock lock(mMutex);
    // We might have raced with another thread, so first check that we don't
    // already have an entry for this
    MapEntry* entry = Lookup(aIid);
    if (entry && entry->mInterceptor) {
      unkInterceptor = entry->mInterceptor;
    } else {
      // We're inserting unkInterceptor into the map but we still want to hang
      // onto it locally so that we can QI it below.
      unkInterceptor->AddRef();
      // OTOH we must not touch the refcount for the target interface
      // because we are just moving it into the map and its refcounting might
      // not be thread-safe.
      IUnknown* rawTargetInterface = targetInterface.release();
      mInterceptorMap.AppendElement(MapEntry(aIid,
                                             unkInterceptor,
                                             rawTargetInterface));
    }
  }

  return unkInterceptor->QueryInterface(aIid, aOutInterceptor);
}

HRESULT
Interceptor::QueryInterfaceTarget(REFIID aIid, void** aOutput)
{
  // NB: This QI needs to run on the main thread because the target object
  // is probably Gecko code that is not thread-safe. Note that this main
  // thread invocation is *synchronous*.
  MainThreadInvoker invoker;
  HRESULT hr;
  auto runOnMainThread = [&]() -> void {
    MOZ_ASSERT(NS_IsMainThread());
    hr = mTarget->QueryInterface(aIid, aOutput);
  };
  if (!invoker.Invoke(NS_NewRunnableFunction(runOnMainThread))) {
    return E_FAIL;
  }
  return hr;
}

HRESULT
Interceptor::QueryInterface(REFIID riid, void** ppv)
{
  return WeakReferenceSupport::QueryInterface(riid, ppv);
}

HRESULT
Interceptor::ThreadSafeQueryInterface(REFIID aIid, IUnknown** aOutInterface)
{
  if (aIid == IID_IInterceptor) {
    *aOutInterface = static_cast<IInterceptor*>(this);
    (*aOutInterface)->AddRef();
    return S_OK;
  }

  if (aIid == IID_IDispatch) {
    STAUniquePtr<IDispatch> disp;
    IDispatch* rawDisp = nullptr;
    HRESULT hr = QueryInterfaceTarget(aIid, (void**)&rawDisp);
    if (FAILED(hr)) {
      return hr;
    }
    disp.reset(rawDisp);
    return DispatchForwarder::Create(this, disp, aOutInterface);
  }

  return GetInterceptorForIID(aIid, (void**)aOutInterface);
}

ULONG
Interceptor::AddRef()
{
  return WeakReferenceSupport::AddRef();
}

ULONG
Interceptor::Release()
{
  return WeakReferenceSupport::Release();
}

} // namespace mscom
} // namespace mozilla
