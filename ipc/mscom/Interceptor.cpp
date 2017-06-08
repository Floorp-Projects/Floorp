/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define INITGUID

#include "mozilla/dom/ContentChild.h"
#include "mozilla/Move.h"
#include "mozilla/mscom/DispatchForwarder.h"
#include "mozilla/mscom/Interceptor.h"
#include "mozilla/mscom/InterceptorLog.h"
#include "mozilla/mscom/MainThreadInvoker.h"
#include "mozilla/mscom/Objref.h"
#include "mozilla/mscom/Registration.h"
#include "mozilla/mscom/Utils.h"
#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsRefPtrHashtable.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

namespace mozilla {
namespace mscom {

class LiveSet final
{
public:
  LiveSet()
    : mMutex("mozilla::mscom::LiveSet::mMutex")
  {
  }

  void Lock()
  {
    mMutex.Lock();
  }

  void Unlock()
  {
    mMutex.Unlock();
  }

  void Put(IUnknown* aKey, already_AddRefed<IWeakReference> aValue)
  {
    mMutex.AssertCurrentThreadOwns();
    mLiveSet.Put(aKey, Move(aValue));
  }

  RefPtr<IWeakReference> Get(IUnknown* aKey)
  {
    mMutex.AssertCurrentThreadOwns();
    RefPtr<IWeakReference> result;
    mLiveSet.Get(aKey, getter_AddRefs(result));
    return result;
  }

  void Remove(IUnknown* aKey)
  {
    mMutex.AssertCurrentThreadOwns();
    mLiveSet.Remove(aKey);
  }

  typedef BaseAutoLock<LiveSet> AutoLock;

private:
  Mutex mMutex;
  nsRefPtrHashtable<nsPtrHashKey<IUnknown>, IWeakReference> mLiveSet;
};

static LiveSet&
GetLiveSet()
{
  static LiveSet sLiveSet;
  return sLiveSet;
}

/* static */ HRESULT
Interceptor::Create(STAUniquePtr<IUnknown> aTarget, IInterceptorSink* aSink,
                    REFIID aInitialIid, void** aOutInterface)
{
  MOZ_ASSERT(aOutInterface && aTarget && aSink);
  if (!aOutInterface) {
    return E_INVALIDARG;
  }

  LiveSet::AutoLock lock(GetLiveSet());

  RefPtr<IWeakReference> existingInterceptor(Move(GetLiveSet().Get(aTarget.get())));
  if (existingInterceptor &&
      SUCCEEDED(existingInterceptor->Resolve(aInitialIid, aOutInterface))) {
    return S_OK;
  }

  *aOutInterface = nullptr;

  if (!aTarget || !aSink) {
    return E_INVALIDARG;
  }

  RefPtr<Interceptor> intcpt(new Interceptor(aSink));
  return intcpt->GetInitialInterceptorForIID(aInitialIid, Move(aTarget),
                                             aOutInterface);
}

Interceptor::Interceptor(IInterceptorSink* aSink)
  : WeakReferenceSupport(WeakReferenceSupport::Flags::eDestroyOnMainThread)
  , mEventSink(aSink)
  , mMutex("mozilla::mscom::Interceptor::mMutex")
  , mStdMarshal(nullptr)
{
  MOZ_ASSERT(aSink);
  RefPtr<IWeakReference> weakRef;
  if (SUCCEEDED(GetWeakReference(getter_AddRefs(weakRef)))) {
    aSink->SetInterceptor(weakRef);
  }
}

Interceptor::~Interceptor()
{
  { // Scope for lock
    LiveSet::AutoLock lock(GetLiveSet());
    GetLiveSet().Remove(mTarget.get());
  }

  // This needs to run on the main thread because it releases target interface
  // reference counts which may not be thread-safe.
  MOZ_ASSERT(NS_IsMainThread());
  for (uint32_t index = 0, len = mInterceptorMap.Length(); index < len; ++index) {
    MapEntry& entry = mInterceptorMap[index];
    entry.mInterceptor = nullptr;
    entry.mTargetInterface->Release();
  }
}

HRESULT
Interceptor::GetClassForHandler(DWORD aDestContext, void* aDestContextPtr,
                                CLSID* aHandlerClsid)
{
  if (aDestContextPtr || !aHandlerClsid ||
      aDestContext == MSHCTX_DIFFERENTMACHINE) {
    return E_INVALIDARG;
  }

  MOZ_ASSERT(mEventSink);
  return mEventSink->GetHandler(WrapNotNull(aHandlerClsid));
}

HRESULT
Interceptor::GetUnmarshalClass(REFIID riid, void* pv, DWORD dwDestContext,
                               void* pvDestContext, DWORD mshlflags,
                               CLSID* pCid)
{
  return mStdMarshal->GetUnmarshalClass(riid, pv, dwDestContext, pvDestContext,
                                        mshlflags, pCid);
}

HRESULT
Interceptor::GetMarshalSizeMax(REFIID riid, void* pv, DWORD dwDestContext,
                               void* pvDestContext, DWORD mshlflags,
                               DWORD* pSize)
{
  HRESULT hr = mStdMarshal->GetMarshalSizeMax(riid, pv, dwDestContext,
                                              pvDestContext, mshlflags, pSize);
  if (FAILED(hr)) {
    return hr;
  }

  DWORD payloadSize = 0;
  hr = mEventSink->GetHandlerPayloadSize(WrapNotNull(&payloadSize));
  *pSize += payloadSize;
  return hr;
}

HRESULT
Interceptor::MarshalInterface(IStream* pStm, REFIID riid, void* pv,
                              DWORD dwDestContext, void* pvDestContext,
                              DWORD mshlflags)
{
  HRESULT hr;

#if defined(MOZ_MSCOM_REMARSHAL_NO_HANDLER)
  // Save the current stream position
  LARGE_INTEGER seekTo;
  seekTo.QuadPart = 0;

  ULARGE_INTEGER objrefPos;

  hr = pStm->Seek(seekTo, STREAM_SEEK_CUR, &objrefPos);
  if (FAILED(hr)) {
    return hr;
  }

#endif // defined(MOZ_MSCOM_REMARSHAL_NO_HANDLER)

  hr = mStdMarshal->MarshalInterface(pStm, riid, pv, dwDestContext,
                                     pvDestContext, mshlflags);
  if (FAILED(hr)) {
    return hr;
  }

#if defined(MOZ_MSCOM_REMARSHAL_NO_HANDLER)
  if (XRE_IsContentProcess()) {
    const DWORD chromeMainTid =
      dom::ContentChild::GetSingleton()->GetChromeMainThreadId();

    /*
     * CoGetCallerTID() gives us the caller's thread ID when that thread resides
     * in a single-threaded apartment. Since our chrome main thread does live
     * inside an STA, we will therefore be able to check whether the caller TID
     * equals our chrome main thread TID. This enables us to distinguish
     * between our chrome thread vs other out-of-process callers.
     */
    DWORD callerTid;
    if (::CoGetCallerTID(&callerTid) == S_FALSE && callerTid != chromeMainTid) {
      // The caller isn't our chrome process, so do not provide a handler.
      // First, seek back to the stream position that we prevously saved.
      seekTo.QuadPart = objrefPos.QuadPart;
      hr = pStm->Seek(seekTo, STREAM_SEEK_SET, nullptr);
      if (FAILED(hr)) {
        return hr;
      }

      // Now strip out the handler.
      if (!StripHandlerFromOBJREF(WrapNotNull(pStm))) {
        return E_FAIL;
      }

      return S_OK;
    }
  }
#endif // defined(MOZ_MSCOM_REMARSHAL_NO_HANDLER)

  return mEventSink->WriteHandlerPayload(WrapNotNull(pStm));
}

HRESULT
Interceptor::UnmarshalInterface(IStream* pStm, REFIID riid,
                                void** ppv)
{
  return mStdMarshal->UnmarshalInterface(pStm, riid, ppv);
}

HRESULT
Interceptor::ReleaseMarshalData(IStream* pStm)
{
  return mStdMarshal->ReleaseMarshalData(pStm);
}

HRESULT
Interceptor::DisconnectObject(DWORD dwReserved)
{
  return mStdMarshal->DisconnectObject(dwReserved);
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
Interceptor::GetTargetForIID(REFIID aIid,
                             InterceptorTargetPtr<IUnknown>& aTarget)
{
  MutexAutoLock lock(mMutex);
  MapEntry* entry = Lookup(aIid);
  if (entry) {
    aTarget.reset(entry->mTargetInterface);
    return S_OK;
  }

  return E_NOINTERFACE;
}

// CoGetInterceptor requires type metadata to be able to generate its emulated
// vtable. If no registered metadata is available, CoGetInterceptor returns
// kFileNotFound.
static const HRESULT kFileNotFound = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);

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

HRESULT
Interceptor::GetInitialInterceptorForIID(REFIID aTargetIid,
                                         STAUniquePtr<IUnknown> aTarget,
                                         void** aOutInterceptor)
{
  MOZ_ASSERT(aOutInterceptor);
  MOZ_ASSERT(aTargetIid != IID_IUnknown && aTargetIid != IID_IMarshal);
  MOZ_ASSERT(!IsProxy(aTarget.get()));

  // Raise the refcount for stabilization purposes during aggregation
  RefPtr<IUnknown> kungFuDeathGrip(static_cast<IUnknown*>(
        static_cast<WeakReferenceSupport*>(this)));

  RefPtr<IUnknown> unkInterceptor;
  HRESULT hr = CreateInterceptor(aTargetIid, kungFuDeathGrip,
                                 getter_AddRefs(unkInterceptor));
  if (FAILED(hr)) {
    return hr;
  }

  RefPtr<ICallInterceptor> interceptor;
  hr = unkInterceptor->QueryInterface(IID_ICallInterceptor,
                                      getter_AddRefs(interceptor));
  if (FAILED(hr)) {
    return hr;
  }

  hr = interceptor->RegisterSink(mEventSink);
  if (FAILED(hr)) {
    return hr;
  }

  RefPtr<IWeakReference> weakRef;
  hr = GetWeakReference(getter_AddRefs(weakRef));
  if (FAILED(hr)) {
    return hr;
  }

  // mTarget is a weak reference to aTarget. This is safe because we transfer
  // ownership of aTarget into mInterceptorMap which remains live for the
  // lifetime of this Interceptor.
  mTarget = ToInterceptorTargetPtr(aTarget);
  GetLiveSet().Put(mTarget.get(), weakRef.forget());

  // Now we transfer aTarget's ownership into mInterceptorMap.
  mInterceptorMap.AppendElement(MapEntry(aTargetIid,
                                         unkInterceptor,
                                         aTarget.release()));

  if (mEventSink->MarshalAs(aTargetIid) == aTargetIid) {
    return unkInterceptor->QueryInterface(aTargetIid, aOutInterceptor);
  }

  return GetInterceptorForIID(aTargetIid, aOutInterceptor);
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
    RefPtr<IInterceptor> intcpt(this);
    intcpt.forget(aOutInterceptor);
    return S_OK;
  }

  REFIID interceptorIid = mEventSink->MarshalAs(aIid);

  RefPtr<IUnknown> unkInterceptor;
  IUnknown* interfaceForQILog = nullptr;

  // (1) Check to see if we already have an existing interceptor for
  // interceptorIid.

  { // Scope for lock
    MutexAutoLock lock(mMutex);
    MapEntry* entry = Lookup(interceptorIid);
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

    return unkInterceptor->QueryInterface(interceptorIid, aOutInterceptor);
  }

  // (2) Obtain a new target interface.

  // (2a) First, make sure that the target interface is available
  // NB: We *MUST* query the correct interface! ICallEvents::Invoke casts its
  // pvReceiver argument directly to the required interface! DO NOT assume
  // that COM will use QI or upcast/downcast!
  HRESULT hr;

  STAUniquePtr<IUnknown> targetInterface;
  IUnknown* rawTargetInterface = nullptr;
  hr = QueryInterfaceTarget(interceptorIid, (void**)&rawTargetInterface);
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

  hr = CreateInterceptor(interceptorIid, kungFuDeathGrip,
                         getter_AddRefs(unkInterceptor));
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
    MapEntry* entry = Lookup(interceptorIid);
    if (entry && entry->mInterceptor) {
      unkInterceptor = entry->mInterceptor;
    } else {
      // MapEntry has a RefPtr to unkInterceptor, OTOH we must not touch the
      // refcount for the target interface because we are just moving it into
      // the map and its refcounting might not be thread-safe.
      IUnknown* rawTargetInterface = targetInterface.release();
      mInterceptorMap.AppendElement(MapEntry(interceptorIid,
                                             unkInterceptor,
                                             rawTargetInterface));
    }
  }

  return unkInterceptor->QueryInterface(interceptorIid, aOutInterceptor);
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
  if (aIid == IID_INoMarshal) {
    // This entire library is designed around marshaling, so there's no point
    // propagating this QI request all over the place!
    return E_NOINTERFACE;
  }

  if (aIid == IID_IStdMarshalInfo) {
    // Do not indicate that this interface is available unless we actually
    // support it. We'll check that by looking for a successful call to
    // IInterceptorSink::GetHandler()
    CLSID dummy;
    if (FAILED(mEventSink->GetHandler(WrapNotNull(&dummy)))) {
      return E_NOINTERFACE;
    }

    RefPtr<IStdMarshalInfo> std(this);
    std.forget(aOutInterface);
    return S_OK;
  }

  if (aIid == IID_IMarshal) {
    // Do not indicate that this interface is available unless we actually
    // support it. We'll check that by looking for a successful call to
    // IInterceptorSink::GetHandler()
    CLSID dummy;
    if (FAILED(mEventSink->GetHandler(WrapNotNull(&dummy)))) {
      return E_NOINTERFACE;
    }

    if (!mStdMarshalUnk) {
      HRESULT hr = ::CoGetStdMarshalEx(static_cast<IWeakReferenceSource*>(this),
                                       SMEXF_SERVER,
                                       getter_AddRefs(mStdMarshalUnk));
      if (FAILED(hr)) {
        return hr;
      }
    }

    if (!mStdMarshal) {
      HRESULT hr = mStdMarshalUnk->QueryInterface(IID_IMarshal,
                                                  (void**)&mStdMarshal);
      if (FAILED(hr)) {
        return hr;
      }

      // mStdMarshal is weak, so drop its refcount
      mStdMarshal->Release();
    }

    RefPtr<IMarshal> marshal(this);
    marshal.forget(aOutInterface);
    return S_OK;
  }

  if (aIid == IID_IInterceptor) {
    RefPtr<IInterceptor> intcpt(this);
    intcpt.forget(aOutInterface);
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
