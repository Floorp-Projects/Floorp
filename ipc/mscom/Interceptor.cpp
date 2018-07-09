/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define INITGUID

#include "mozilla/dom/ContentChild.h"
#include "mozilla/Move.h"
#include "mozilla/mscom/DispatchForwarder.h"
#include "mozilla/mscom/FastMarshaler.h"
#include "mozilla/mscom/Interceptor.h"
#include "mozilla/mscom/InterceptorLog.h"
#include "mozilla/mscom/MainThreadInvoker.h"
#include "mozilla/mscom/Objref.h"
#include "mozilla/mscom/Registration.h"
#include "mozilla/mscom/Utils.h"
#include "mozilla/ThreadLocal.h"
#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsExceptionHandler.h"
#include "nsPrintfCString.h"
#include "nsRefPtrHashtable.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

#define ENSURE_HR_SUCCEEDED(hr) \
  MOZ_ASSERT(SUCCEEDED((HRESULT)hr)); \
  if (FAILED((HRESULT)hr)) { \
    return hr; \
  }

namespace mozilla {
namespace mscom {
namespace detail {

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
    mLiveSet.Put(aKey, std::move(aValue));
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

private:
  Mutex mMutex;
  nsRefPtrHashtable<nsPtrHashKey<IUnknown>, IWeakReference> mLiveSet;
};

/**
 * We don't use the normal XPCOM BaseAutoLock because we need the ability
 * to explicitly Unlock.
 */
class MOZ_RAII LiveSetAutoLock final
{
public:
  explicit LiveSetAutoLock(LiveSet& aLiveSet)
    : mLiveSet(&aLiveSet)
  {
    aLiveSet.Lock();
  }

  ~LiveSetAutoLock()
  {
    if (mLiveSet) {
      mLiveSet->Unlock();
    }
  }

  void Unlock()
  {
    MOZ_ASSERT(mLiveSet);
    if (mLiveSet) {
      mLiveSet->Unlock();
      mLiveSet = nullptr;
    }
  }

  LiveSetAutoLock(const LiveSetAutoLock& aOther) = delete;
  LiveSetAutoLock(LiveSetAutoLock&& aOther) = delete;
  LiveSetAutoLock& operator=(const LiveSetAutoLock& aOther) = delete;
  LiveSetAutoLock& operator=(LiveSetAutoLock&& aOther) = delete;

private:
  LiveSet*  mLiveSet;
};

class MOZ_RAII ReentrySentinel final
{
public:
  explicit ReentrySentinel(Interceptor* aCurrent)
    : mCurInterceptor(aCurrent)
  {
    static const bool kHasTls = tlsSentinelStackTop.init();
    MOZ_RELEASE_ASSERT(kHasTls);

    mPrevSentinel = tlsSentinelStackTop.get();
    tlsSentinelStackTop.set(this);
  }

  ~ReentrySentinel()
  {
    tlsSentinelStackTop.set(mPrevSentinel);
  }

  bool IsOutermost() const
  {
    return !(mPrevSentinel && mPrevSentinel->IsMarshaling(mCurInterceptor));
  }

  ReentrySentinel(const ReentrySentinel&) = delete;
  ReentrySentinel(ReentrySentinel&&) = delete;
  ReentrySentinel& operator=(const ReentrySentinel&) = delete;
  ReentrySentinel& operator=(ReentrySentinel&&) = delete;

private:
  bool IsMarshaling(Interceptor* aTopInterceptor) const
  {
    return aTopInterceptor == mCurInterceptor ||
           (mPrevSentinel && mPrevSentinel->IsMarshaling(aTopInterceptor));
  }

private:
  Interceptor*      mCurInterceptor;
  ReentrySentinel*  mPrevSentinel;

  static MOZ_THREAD_LOCAL(ReentrySentinel*) tlsSentinelStackTop;
};

MOZ_THREAD_LOCAL(ReentrySentinel*) ReentrySentinel::tlsSentinelStackTop;

class MOZ_RAII LoggedQIResult final
{
public:
  explicit LoggedQIResult(REFIID aIid)
    : mIid(aIid)
    , mHr(E_UNEXPECTED)
    , mTarget(nullptr)
    , mInterceptor(nullptr)
    , mBegin(TimeStamp::Now())
  {
  }

  ~LoggedQIResult()
  {
    if (!mTarget) {
      return;
    }

    TimeStamp end(TimeStamp::Now());
    TimeDuration total(end - mBegin);
    TimeDuration overhead(total - mNonOverheadDuration);

    InterceptorLog::QI(mHr, mTarget, mIid, mInterceptor, &overhead,
                       &mNonOverheadDuration);
  }

  void Log(IUnknown* aTarget, IUnknown* aInterceptor)
  {
    mTarget = aTarget;
    mInterceptor = aInterceptor;
  }

  void operator=(HRESULT aHr)
  {
    mHr = aHr;
  }

  operator HRESULT()
  {
    return mHr;
  }

  operator TimeDuration*()
  {
    return &mNonOverheadDuration;
  }

  LoggedQIResult(const LoggedQIResult&) = delete;
  LoggedQIResult(LoggedQIResult&&) = delete;
  LoggedQIResult& operator=(const LoggedQIResult&) = delete;
  LoggedQIResult& operator=(LoggedQIResult&&) = delete;

private:
  REFIID        mIid;
  HRESULT       mHr;
  IUnknown*     mTarget;
  IUnknown*     mInterceptor;
  TimeDuration  mNonOverheadDuration;
  TimeStamp     mBegin;
};

} // namespace detail

static detail::LiveSet&
GetLiveSet()
{
  static detail::LiveSet sLiveSet;
  return sLiveSet;
}

MOZ_THREAD_LOCAL(bool) Interceptor::tlsCreatingStdMarshal;

/* static */ HRESULT
Interceptor::Create(STAUniquePtr<IUnknown> aTarget, IInterceptorSink* aSink,
                    REFIID aInitialIid, void** aOutInterface)
{
  MOZ_ASSERT(aOutInterface && aTarget && aSink);
  if (!aOutInterface) {
    return E_INVALIDARG;
  }

  detail::LiveSetAutoLock lock(GetLiveSet());

  RefPtr<IWeakReference> existingWeak(GetLiveSet().Get(aTarget.get()));
  if (existingWeak) {
    RefPtr<IWeakReferenceSource> existingStrong;
    if (SUCCEEDED(existingWeak->ToStrongRef(getter_AddRefs(existingStrong)))) {
      // QI on existingStrong may touch other threads. Since we now hold a
      // strong ref on the interceptor, we may now release the lock.
      lock.Unlock();
      return existingStrong->QueryInterface(aInitialIid, aOutInterface);
    }
  }

  *aOutInterface = nullptr;

  if (!aTarget || !aSink) {
    return E_INVALIDARG;
  }

  RefPtr<Interceptor> intcpt(new Interceptor(aSink));
  return intcpt->GetInitialInterceptorForIID(lock, aInitialIid, std::move(aTarget),
                                             aOutInterface);
}

Interceptor::Interceptor(IInterceptorSink* aSink)
  : WeakReferenceSupport(WeakReferenceSupport::Flags::eDestroyOnMainThread)
  , mEventSink(aSink)
  , mInterceptorMapMutex("mozilla::mscom::Interceptor::mInterceptorMapMutex")
  , mStdMarshalMutex("mozilla::mscom::Interceptor::mStdMarshalMutex")
  , mStdMarshal(nullptr)
{
  static const bool kHasTls = tlsCreatingStdMarshal.init();
  MOZ_ASSERT(kHasTls);

  MOZ_ASSERT(aSink);
  RefPtr<IWeakReference> weakRef;
  if (SUCCEEDED(GetWeakReference(getter_AddRefs(weakRef)))) {
    aSink->SetInterceptor(weakRef);
  }
}

Interceptor::~Interceptor()
{
  { // Scope for lock
    detail::LiveSetAutoLock lock(GetLiveSet());
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

REFIID
Interceptor::MarshalAs(REFIID aIid) const
{
#if defined(MOZ_MSCOM_REMARSHAL_NO_HANDLER)
  return IsCallerExternalProcess() ? aIid : mEventSink->MarshalAs(aIid);
#else
  return mEventSink->MarshalAs(aIid);
#endif // defined(MOZ_MSCOM_REMARSHAL_NO_HANDLER)
}

HRESULT
Interceptor::GetUnmarshalClass(REFIID riid, void* pv, DWORD dwDestContext,
                               void* pvDestContext, DWORD mshlflags,
                               CLSID* pCid)
{
  return mStdMarshal->GetUnmarshalClass(MarshalAs(riid), pv, dwDestContext,
                                        pvDestContext, mshlflags, pCid);
}

HRESULT
Interceptor::GetMarshalSizeMax(REFIID riid, void* pv, DWORD dwDestContext,
                               void* pvDestContext, DWORD mshlflags,
                               DWORD* pSize)
{
  detail::ReentrySentinel sentinel(this);

  HRESULT hr = mStdMarshal->GetMarshalSizeMax(MarshalAs(riid), pv, dwDestContext,
                                              pvDestContext, mshlflags, pSize);
  if (FAILED(hr) || !sentinel.IsOutermost()) {
    return hr;
  }

  DWORD payloadSize = 0;
  hr = mEventSink->GetHandlerPayloadSize(WrapNotNull(this),
                                         WrapNotNull(&payloadSize));
  if (hr == E_NOTIMPL) {
    return S_OK;
  }

  if (SUCCEEDED(hr)) {
    *pSize += payloadSize;
  }
  return hr;
}

HRESULT
Interceptor::MarshalInterface(IStream* pStm, REFIID riid, void* pv,
                              DWORD dwDestContext, void* pvDestContext,
                              DWORD mshlflags)
{
  detail::ReentrySentinel sentinel(this);

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

  hr = mStdMarshal->MarshalInterface(pStm, MarshalAs(riid), pv, dwDestContext,
                                     pvDestContext, mshlflags);
  if (FAILED(hr) || !sentinel.IsOutermost()) {
    return hr;
  }

#if defined(MOZ_MSCOM_REMARSHAL_NO_HANDLER)
  if (XRE_IsContentProcess() && IsCallerExternalProcess()) {
    // The caller isn't our chrome process, so do not provide a handler.

    // First, save the current position that marks the current end of the
    // OBJREF in the stream.
    ULARGE_INTEGER endPos;
    hr = pStm->Seek(seekTo, STREAM_SEEK_CUR, &endPos);
    if (FAILED(hr)) {
      return hr;
    }

    // Now strip out the handler.
    if (!StripHandlerFromOBJREF(WrapNotNull(pStm), objrefPos.QuadPart,
                                endPos.QuadPart)) {
      return E_FAIL;
    }

    return S_OK;
  }
#endif // defined(MOZ_MSCOM_REMARSHAL_NO_HANDLER)

  hr = mEventSink->WriteHandlerPayload(WrapNotNull(this), WrapNotNull(pStm));
  if (hr == E_NOTIMPL) {
    return S_OK;
  }

  return hr;
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
  mEventSink->DisconnectHandlerRemotes();
  return mStdMarshal->DisconnectObject(dwReserved);
}

Interceptor::MapEntry*
Interceptor::Lookup(REFIID aIid)
{
  mInterceptorMapMutex.AssertCurrentThreadOwns();

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
  MutexAutoLock lock(mInterceptorMapMutex);
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
Interceptor::PublishTarget(detail::LiveSetAutoLock& aLiveSetLock,
                           RefPtr<IUnknown> aInterceptor,
                           REFIID aTargetIid,
                           STAUniquePtr<IUnknown> aTarget)
{
  RefPtr<IWeakReference> weakRef;
  HRESULT hr = GetWeakReference(getter_AddRefs(weakRef));
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
                                         aInterceptor,
                                         aTarget.release()));

  // Release the live set lock because subsequent operations may post work to
  // the main thread, creating potential for deadlocks.
  aLiveSetLock.Unlock();
  return S_OK;
}

HRESULT
Interceptor::GetInitialInterceptorForIID(detail::LiveSetAutoLock& aLiveSetLock,
                                         REFIID aTargetIid,
                                         STAUniquePtr<IUnknown> aTarget,
                                         void** aOutInterceptor)
{
  MOZ_ASSERT(aOutInterceptor);
  MOZ_ASSERT(aTargetIid != IID_IMarshal);
  MOZ_ASSERT(!IsProxy(aTarget.get()));

  HRESULT hr = E_UNEXPECTED;

  auto hasFailed = [&hr]() -> bool {
    return FAILED(hr);
  };

  auto cleanup = [&aLiveSetLock]() -> void {
    aLiveSetLock.Unlock();
  };

  ExecuteWhen<decltype(hasFailed), decltype(cleanup)>
    onFail(hasFailed, cleanup);

  if (aTargetIid == IID_IUnknown) {
    // We must lock mInterceptorMapMutex so that nothing can race with us once
    // we have been published to the live set.
    MutexAutoLock lock(mInterceptorMapMutex);

    hr = PublishTarget(aLiveSetLock, nullptr, aTargetIid, std::move(aTarget));
    ENSURE_HR_SUCCEEDED(hr);

    hr = QueryInterface(aTargetIid, aOutInterceptor);
    ENSURE_HR_SUCCEEDED(hr);
    return hr;
  }

  // Raise the refcount for stabilization purposes during aggregation
  WeakReferenceSupport::StabilizeRefCount stabilizer(*this);

  RefPtr<IUnknown> unkInterceptor;
  hr = CreateInterceptor(aTargetIid,
                                 static_cast<WeakReferenceSupport*>(this),
                                 getter_AddRefs(unkInterceptor));
  ENSURE_HR_SUCCEEDED(hr);

  RefPtr<ICallInterceptor> interceptor;
  hr = unkInterceptor->QueryInterface(IID_ICallInterceptor,
                                      getter_AddRefs(interceptor));
  ENSURE_HR_SUCCEEDED(hr);

  hr = interceptor->RegisterSink(mEventSink);
  ENSURE_HR_SUCCEEDED(hr);

  // We must lock mInterceptorMapMutex so that nothing can race with us once we have
  // been published to the live set.
  MutexAutoLock lock(mInterceptorMapMutex);

  hr = PublishTarget(aLiveSetLock, unkInterceptor, aTargetIid, std::move(aTarget));
  ENSURE_HR_SUCCEEDED(hr);

  if (MarshalAs(aTargetIid) == aTargetIid) {
    hr = unkInterceptor->QueryInterface(aTargetIid, aOutInterceptor);
    ENSURE_HR_SUCCEEDED(hr);
    return hr;
  }

  hr = GetInterceptorForIID(aTargetIid, aOutInterceptor, &lock);
  ENSURE_HR_SUCCEEDED(hr);
  return hr;
}

HRESULT
Interceptor::GetInterceptorForIID(REFIID aIid, void** aOutInterceptor)
{
  return GetInterceptorForIID(aIid, aOutInterceptor, nullptr);
}

/**
 * This method contains the core guts of the handling of QueryInterface calls
 * that are delegated to us from the ICallInterceptor.
 *
 * @param aIid ID of the desired interface
 * @param aOutInterceptor The resulting emulated vtable that corresponds to
 * the interface specified by aIid.
 * @param aAlreadyLocked Proof of an existing lock on |mInterceptorMapMutex|,
 *                       if present.
 */
HRESULT
Interceptor::GetInterceptorForIID(REFIID aIid, void** aOutInterceptor,
                                  MutexAutoLock* aAlreadyLocked)
{
  detail::LoggedQIResult result(aIid);

  if (!aOutInterceptor) {
    return E_INVALIDARG;
  }

  if (aIid == IID_IUnknown) {
    // Special case: When we see IUnknown, we just provide a reference to this
    RefPtr<IInterceptor> intcpt(this);
    intcpt.forget(aOutInterceptor);
    return S_OK;
  }

  REFIID interceptorIid = MarshalAs(aIid);

  RefPtr<IUnknown> unkInterceptor;
  IUnknown* interfaceForQILog = nullptr;

  // (1) Check to see if we already have an existing interceptor for
  // interceptorIid.
  auto doLookup = [&]() -> void {
    MapEntry* entry = Lookup(interceptorIid);
    if (entry) {
      unkInterceptor = entry->mInterceptor;
      interfaceForQILog = entry->mTargetInterface;
    }
  };

  if (aAlreadyLocked) {
    doLookup();
  } else {
    MutexAutoLock lock(mInterceptorMapMutex);
    doLookup();
  }

  // (1a) A COM interceptor already exists for this interface, so all we need
  // to do is run a QI on it.
  if (unkInterceptor) {
    // Technically we didn't actually execute a QI on the target interface, but
    // for logging purposes we would like to record the fact that this interface
    // was requested.
    result.Log(mTarget.get(), interfaceForQILog);
    result = unkInterceptor->QueryInterface(interceptorIid, aOutInterceptor);
    ENSURE_HR_SUCCEEDED(result);
    return result;
  }

  // (2) Obtain a new target interface.

  // (2a) First, make sure that the target interface is available
  // NB: We *MUST* query the correct interface! ICallEvents::Invoke casts its
  // pvReceiver argument directly to the required interface! DO NOT assume
  // that COM will use QI or upcast/downcast!
  HRESULT hr;

  STAUniquePtr<IUnknown> targetInterface;
  IUnknown* rawTargetInterface = nullptr;
  hr = QueryInterfaceTarget(interceptorIid, (void**)&rawTargetInterface, result);
  targetInterface.reset(rawTargetInterface);
  result = hr;
  result.Log(mTarget.get(), targetInterface.get());
  MOZ_ASSERT(SUCCEEDED(hr) || hr == E_NOINTERFACE);
  if (hr == E_NOINTERFACE) {
    return hr;
  }
  ENSURE_HR_SUCCEEDED(hr);

  // We *really* shouldn't be adding interceptors to proxies
  MOZ_ASSERT(aIid != IID_IMarshal);

  // (3) Create a new COM interceptor to that interface that delegates its
  // IUnknown to |this|.

  // Raise the refcount for stabilization purposes during aggregation
  WeakReferenceSupport::StabilizeRefCount stabilizer(*this);

  hr = CreateInterceptor(interceptorIid,
                         static_cast<WeakReferenceSupport*>(this),
                         getter_AddRefs(unkInterceptor));
  ENSURE_HR_SUCCEEDED(hr);

  // (4) Obtain the interceptor's ICallInterceptor interface and register our
  // event sink.
  RefPtr<ICallInterceptor> interceptor;
  hr = unkInterceptor->QueryInterface(IID_ICallInterceptor,
                                      (void**)getter_AddRefs(interceptor));
  ENSURE_HR_SUCCEEDED(hr);

  hr = interceptor->RegisterSink(mEventSink);
  ENSURE_HR_SUCCEEDED(hr);

  // (5) Now that we have this new COM interceptor, insert it into the map.
  auto doInsertion = [&]() -> void {
    // We might have raced with another thread, so first check that we don't
    // already have an entry for this
    MapEntry* entry = Lookup(interceptorIid);
    if (entry && entry->mInterceptor) {
      // Bug 1433046: Because of aggregation, the QI for |interceptor|
      // AddRefed |this|, not |unkInterceptor|. Thus, releasing |unkInterceptor|
      // will destroy the object. Before we do that, we must first release
      // |interceptor|. Otherwise, |interceptor| would be invalidated when
      // |unkInterceptor| is destroyed.
      interceptor = nullptr;
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
  };

  if (aAlreadyLocked) {
    doInsertion();
  } else {
    MutexAutoLock lock(mInterceptorMapMutex);
    doInsertion();
  }

  hr = unkInterceptor->QueryInterface(interceptorIid, aOutInterceptor);
  ENSURE_HR_SUCCEEDED(hr);
  return hr;
}

HRESULT
Interceptor::QueryInterfaceTarget(REFIID aIid, void** aOutput,
                                  TimeDuration* aOutDuration)
{
  // NB: This QI needs to run on the main thread because the target object
  // is probably Gecko code that is not thread-safe. Note that this main
  // thread invocation is *synchronous*.
  if (!NS_IsMainThread() && tlsCreatingStdMarshal.get()) {
    mStdMarshalMutex.AssertCurrentThreadOwns();
    // COM queries for special interfaces such as IFastRundown when creating a
    // marshaler. We don't want these being dispatched to the main thread,
    // since this would cause a deadlock on mStdMarshalMutex if the main
    // thread is also querying for IMarshal. If we do need to respond to these
    // special interfaces, this should be done before this point; e.g. in
    // Interceptor::QueryInterface like we do for INoMarshal.
    return E_NOINTERFACE;
  }

  MainThreadInvoker invoker;
  HRESULT hr;
  auto runOnMainThread = [&]() -> void {
    MOZ_ASSERT(NS_IsMainThread());
    hr = mTarget->QueryInterface(aIid, aOutput);
  };
  if (!invoker.Invoke(NS_NewRunnableFunction("Interceptor::QueryInterface", runOnMainThread))) {
    return E_FAIL;
  }
  if (aOutDuration) {
    *aOutDuration = invoker.GetDuration();
  }
  return hr;
}

HRESULT
Interceptor::QueryInterface(REFIID riid, void** ppv)
{
  if (riid == IID_INoMarshal) {
    // This entire library is designed around marshaling, so there's no point
    // propagating this QI request all over the place!
    return E_NOINTERFACE;
  }

  return WeakReferenceSupport::QueryInterface(riid, ppv);
}

HRESULT
Interceptor::WeakRefQueryInterface(REFIID aIid, IUnknown** aOutInterface)
{
  if (aIid == IID_IStdMarshalInfo) {
    detail::ReentrySentinel sentinel(this);

    if (!sentinel.IsOutermost()) {
      return E_NOINTERFACE;
    }

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
    MutexAutoLock lock(mStdMarshalMutex);

    HRESULT hr;

    if (!mStdMarshalUnk) {
      MOZ_ASSERT(!tlsCreatingStdMarshal.get());
      tlsCreatingStdMarshal.set(true);
      if (XRE_IsContentProcess()) {
        hr = FastMarshaler::Create(static_cast<IWeakReferenceSource*>(this),
                                   getter_AddRefs(mStdMarshalUnk));
      } else {
        hr = ::CoGetStdMarshalEx(static_cast<IWeakReferenceSource*>(this),
                                 SMEXF_SERVER, getter_AddRefs(mStdMarshalUnk));
      }
      tlsCreatingStdMarshal.set(false);

      ENSURE_HR_SUCCEEDED(hr);
    }

    if (!mStdMarshal) {
      hr = mStdMarshalUnk->QueryInterface(IID_IMarshal, (void**)&mStdMarshal);
      ENSURE_HR_SUCCEEDED(hr);

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
    ENSURE_HR_SUCCEEDED(hr);

    disp.reset(rawDisp);
    return DispatchForwarder::Create(this, disp, aOutInterface);
  }

  return GetInterceptorForIID(aIid, (void**)aOutInterface, nullptr);
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

/* static */ HRESULT
Interceptor::DisconnectRemotesForTarget(IUnknown* aTarget)
{
  MOZ_ASSERT(aTarget);

  detail::LiveSetAutoLock lock(GetLiveSet());

  // It is not an error if the interceptor doesn't exist, so we return
  // S_FALSE instead of an error in that case.
  RefPtr<IWeakReference> existingWeak(GetLiveSet().Get(aTarget));
  if (!existingWeak) {
    return S_FALSE;
  }

  RefPtr<IWeakReferenceSource> existingStrong;
  if (FAILED(existingWeak->ToStrongRef(getter_AddRefs(existingStrong)))) {
    return S_FALSE;
  }
  // Since we now hold a strong ref on the interceptor, we may now release the
  // lock.
  lock.Unlock();

  return ::CoDisconnectObject(existingStrong, 0);
}

} // namespace mscom
} // namespace mozilla
