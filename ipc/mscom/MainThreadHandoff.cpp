/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mscom/MainThreadHandoff.h"

#include "mozilla/Attributes.h"
#include "mozilla/Move.h"
#include "mozilla/mscom/AgileReference.h"
#include "mozilla/mscom/InterceptorLog.h"
#include "mozilla/mscom/Registration.h"
#include "mozilla/mscom/Utils.h"
#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "nsThreadUtils.h"
#include "nsProxyRelease.h"

using mozilla::DebugOnly;
using mozilla::mscom::AgileReference;

namespace {

class MOZ_NON_TEMPORARY_CLASS InParamWalker : private ICallFrameWalker
{
public:
  InParamWalker()
    : mPreHandoff(true)
  {
  }

  void SetHandoffDone()
  {
    mPreHandoff = false;
    mAgileRefsItr = mAgileRefs.begin();
  }

  HRESULT Walk(ICallFrame* aFrame)
  {
    MOZ_ASSERT(aFrame);
    if (!aFrame) {
      return E_INVALIDARG;
    }

    return aFrame->WalkFrame(CALLFRAME_WALK_IN, this);
  }

private:
  // IUnknown
  STDMETHODIMP QueryInterface(REFIID aIid, void** aOutInterface) override
  {
    if (!aOutInterface) {
      return E_INVALIDARG;
    }
    *aOutInterface = nullptr;

    if (aIid == IID_IUnknown || aIid == IID_ICallFrameWalker) {
      *aOutInterface = static_cast<ICallFrameWalker*>(this);
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  STDMETHODIMP_(ULONG) AddRef() override
  {
    return 2;
  }

  STDMETHODIMP_(ULONG) Release() override
  {
    return 1;
  }

  // ICallFrameWalker
  STDMETHODIMP OnWalkInterface(REFIID aIid, PVOID* aInterface, BOOL aIn,
                               BOOL aOut) override
  {
    MOZ_ASSERT(aIn);
    if (!aIn) {
      return E_UNEXPECTED;
    }

    IUnknown* origInterface = static_cast<IUnknown*>(*aInterface);
    if (!origInterface) {
      // Nothing to do
      return S_OK;
    }

    if (mPreHandoff) {
      mAgileRefs.AppendElement(AgileReference(aIid, origInterface));
      return S_OK;
    }

    MOZ_ASSERT(mAgileRefsItr != mAgileRefs.end());
    if (mAgileRefsItr == mAgileRefs.end()) {
      return E_UNEXPECTED;
    }

    HRESULT hr = mAgileRefsItr->Resolve(aIid, aInterface);
    MOZ_ASSERT(SUCCEEDED(hr));
    if (SUCCEEDED(hr)) {
      ++mAgileRefsItr;
    }

    return hr;
  }

  InParamWalker(const InParamWalker&) = delete;
  InParamWalker(InParamWalker&&) = delete;
  InParamWalker& operator=(const InParamWalker&) = delete;
  InParamWalker& operator=(InParamWalker&&) = delete;

private:
  bool                                mPreHandoff;
  AutoTArray<AgileReference, 1>       mAgileRefs;
  nsTArray<AgileReference>::iterator  mAgileRefsItr;
};

class HandoffRunnable : public mozilla::Runnable
{
public:
  explicit HandoffRunnable(ICallFrame* aCallFrame, IUnknown* aTargetInterface)
    : Runnable("HandoffRunnable")
    , mCallFrame(aCallFrame)
    , mTargetInterface(aTargetInterface)
    , mResult(E_UNEXPECTED)
  {
    DebugOnly<HRESULT> hr = mInParamWalker.Walk(aCallFrame);
    MOZ_ASSERT(SUCCEEDED(hr));
  }

  NS_IMETHOD Run() override
  {
    mInParamWalker.SetHandoffDone();
    // We declare hr a DebugOnly because if mInParamWalker.Walk() fails, then
    // mCallFrame->Invoke will fail anyway.
    DebugOnly<HRESULT> hr = mInParamWalker.Walk(mCallFrame);
    MOZ_ASSERT(SUCCEEDED(hr));
    mResult = mCallFrame->Invoke(mTargetInterface);
    return NS_OK;
  }

  HRESULT GetResult() const
  {
    return mResult;
  }

private:
  ICallFrame*   mCallFrame;
  InParamWalker mInParamWalker;
  IUnknown*     mTargetInterface;
  HRESULT       mResult;
};

} // anonymous namespace

namespace mozilla {
namespace mscom {

/* static */ HRESULT
MainThreadHandoff::Create(IHandlerProvider* aHandlerProvider,
                          IInterceptorSink** aOutput)
{
  RefPtr<MainThreadHandoff> handoff(new MainThreadHandoff(aHandlerProvider));
  return handoff->QueryInterface(IID_IInterceptorSink, (void**) aOutput);
}

MainThreadHandoff::MainThreadHandoff(IHandlerProvider* aHandlerProvider)
  : mRefCnt(0)
  , mHandlerProvider(aHandlerProvider)
{
}

MainThreadHandoff::~MainThreadHandoff()
{
  MOZ_ASSERT(NS_IsMainThread());
}

HRESULT
MainThreadHandoff::QueryInterface(REFIID riid, void** ppv)
{
  IUnknown* punk = nullptr;
  if (!ppv) {
    return E_INVALIDARG;
  }

  if (riid == IID_IUnknown || riid == IID_ICallFrameEvents ||
      riid == IID_IInterceptorSink) {
    punk = static_cast<IInterceptorSink*>(this);
  } else if (riid == IID_ICallFrameWalker) {
    punk = static_cast<ICallFrameWalker*>(this);
  }

  *ppv = punk;
  if (!punk) {
    return E_NOINTERFACE;
  }

  punk->AddRef();
  return S_OK;
}

ULONG
MainThreadHandoff::AddRef()
{
  return (ULONG) InterlockedIncrement((LONG*)&mRefCnt);
}

ULONG
MainThreadHandoff::Release()
{
  ULONG newRefCnt = (ULONG) InterlockedDecrement((LONG*)&mRefCnt);
  if (newRefCnt == 0) {
    // It is possible for the last Release() call to happen off-main-thread.
    // If so, we need to dispatch an event to delete ourselves.
    if (NS_IsMainThread()) {
      delete this;
    } else {
      // We need to delete this object on the main thread, but we aren't on the
      // main thread right now, so we send a reference to ourselves to the main
      // thread to be re-released there.
      RefPtr<MainThreadHandoff> self = this;
      NS_ReleaseOnMainThreadSystemGroup(
        "MainThreadHandoff", self.forget());
    }
  }
  return newRefCnt;
}

HRESULT
MainThreadHandoff::FixIServiceProvider(ICallFrame* aFrame)
{
  MOZ_ASSERT(aFrame);

  CALLFRAMEPARAMINFO iidOutParamInfo;
  HRESULT hr = aFrame->GetParamInfo(1, &iidOutParamInfo);
  if (FAILED(hr)) {
    return hr;
  }

  VARIANT varIfaceOut;
  hr = aFrame->GetParam(2, &varIfaceOut);
  if (FAILED(hr)) {
    return hr;
  }

  MOZ_ASSERT(varIfaceOut.vt == (VT_UNKNOWN | VT_BYREF));
  if (varIfaceOut.vt != (VT_UNKNOWN | VT_BYREF)) {
    return DISP_E_BADVARTYPE;
  }

  IID** iidOutParam = reinterpret_cast<IID**>(
                        static_cast<BYTE*>(aFrame->GetStackLocation()) +
                        iidOutParamInfo.stackOffset);

  return OnWalkInterface(**iidOutParam,
                         reinterpret_cast<void**>(varIfaceOut.ppunkVal), FALSE,
                         TRUE);
}

HRESULT
MainThreadHandoff::OnCall(ICallFrame* aFrame)
{
  // (1) Get info about the method call
  HRESULT hr;
  IID iid;
  ULONG method;
  hr = aFrame->GetIIDAndMethod(&iid, &method);
  if (FAILED(hr)) {
    return hr;
  }

  RefPtr<IInterceptor> interceptor;
  hr = mInterceptor->Resolve(IID_IInterceptor,
                             (void**)getter_AddRefs(interceptor));
  if (FAILED(hr)) {
    return hr;
  }

  InterceptorTargetPtr<IUnknown> targetInterface;
  hr = interceptor->GetTargetForIID(iid, targetInterface);
  if (FAILED(hr)) {
    return hr;
  }

  // (2) Execute the method call synchronously on the main thread
  RefPtr<HandoffRunnable> handoffInfo(new HandoffRunnable(aFrame,
                                                          targetInterface.get()));
  MainThreadInvoker invoker;
  if (!invoker.Invoke(do_AddRef(handoffInfo))) {
    MOZ_ASSERT(false);
    return E_UNEXPECTED;
  }
  hr = handoffInfo->GetResult();
  MOZ_ASSERT(SUCCEEDED(hr));
  if (FAILED(hr)) {
    return hr;
  }

  // (3) Log *before* wrapping outputs so that the log will contain pointers to
  // the true target interface, not the wrapped ones.
  InterceptorLog::Event(aFrame, targetInterface.get());

  // (4) Scan the function call for outparams that contain interface pointers.
  // Those will need to be wrapped with MainThreadHandoff so that they too will
  // be exeuted on the main thread.

  hr = aFrame->GetReturnValue();
  if (FAILED(hr)) {
    // If the call resulted in an error then there's not going to be anything
    // that needs to be wrapped.
    return S_OK;
  }

  if (iid == IID_IServiceProvider) {
    // The only possible method index for IID_IServiceProvider is for
    // QueryService at index 3; its other methods are inherited from IUnknown
    // and are not processed here.
    MOZ_ASSERT(method == 3);
    // (5) If our interface is IServiceProvider, we need to manually ensure
    // that the correct IID is provided for the interface outparam in
    // IServiceProvider::QueryService.
    hr = FixIServiceProvider(aFrame);
    if (FAILED(hr)) {
      return hr;
    }
  } else if (const ArrayData* arrayData = FindArrayData(iid, method)) {
    // (6) Unfortunately ICallFrame::WalkFrame does not correctly handle array
    // outparams. Instead, we find out whether anybody has called
    // mscom::RegisterArrayData to supply array parameter information and use it
    // if available. This is a terrible hack, but it works for the short term. In
    // the longer term we want to be able to use COM proxy/stub metadata to
    // resolve array information for us.
    hr = FixArrayElements(aFrame, *arrayData);
    if (FAILED(hr)) {
      return hr;
    }
  } else {
    // (7) Scan the outputs looking for any outparam interfaces that need wrapping.
    // NB: WalkFrame does not correctly handle array outparams. It processes the
    // first element of an array but not the remaining elements (if any).
    hr = aFrame->WalkFrame(CALLFRAME_WALK_OUT, this);
    if (FAILED(hr)) {
      return hr;
    }
  }

  return S_OK;
}

static PVOID
ResolveArrayPtr(VARIANT& aVariant)
{
  if (!(aVariant.vt & VT_BYREF)) {
    return nullptr;
  }
  return aVariant.byref;
}

static PVOID*
ResolveInterfacePtr(PVOID aArrayPtr, VARTYPE aVartype, LONG aIndex)
{
  if (aVartype != (VT_VARIANT | VT_BYREF)) {
    IUnknown** ifaceArray = reinterpret_cast<IUnknown**>(aArrayPtr);
    return reinterpret_cast<PVOID*>(&ifaceArray[aIndex]);
  }
  VARIANT* variantArray = reinterpret_cast<VARIANT*>(aArrayPtr);
  VARIANT& element = variantArray[aIndex];
  return &element.byref;
}

HRESULT
MainThreadHandoff::FixArrayElements(ICallFrame* aFrame,
                                    const ArrayData& aArrayData)
{
  // Extract the array length
  VARIANT paramVal;
  VariantInit(&paramVal);
  HRESULT hr = aFrame->GetParam(aArrayData.mLengthParamIndex, &paramVal);
  MOZ_ASSERT(SUCCEEDED(hr) &&
             (paramVal.vt == (VT_I4 | VT_BYREF) ||
             paramVal.vt == (VT_UI4 | VT_BYREF)));
  if (FAILED(hr) || (paramVal.vt != (VT_I4 | VT_BYREF) &&
                     paramVal.vt != (VT_UI4 | VT_BYREF))) {
    return hr;
  }

  const LONG arrayLength = *(paramVal.plVal);
  if (!arrayLength) {
    // Nothing to do
    return S_OK;
  }

  // Extract the array parameter
  VariantInit(&paramVal);
  PVOID arrayPtr = nullptr;
  hr = aFrame->GetParam(aArrayData.mArrayParamIndex, &paramVal);
  if (hr == DISP_E_BADVARTYPE) {
    // ICallFrame::GetParam is not able to coerce the param into a VARIANT.
    // That's ok, we can try to do it ourselves.
    CALLFRAMEPARAMINFO paramInfo;
    hr = aFrame->GetParamInfo(aArrayData.mArrayParamIndex, &paramInfo);
    if (FAILED(hr)) {
      return hr;
    }
    PVOID stackBase = aFrame->GetStackLocation();
    if (aArrayData.mFlag == ArrayData::Flag::eAllocatedByServer) {
      // In order for the server to allocate the array's buffer and store it in
      // an outparam, the parameter must be typed as Type***. Since the base
      // of the array is Type*, we must dereference twice.
      arrayPtr = **reinterpret_cast<PVOID**>(reinterpret_cast<PBYTE>(stackBase) +
                                             paramInfo.stackOffset);
    } else {
      // We dereference because we need to obtain the value of a parameter
      // from a stack offset. This pointer is the base of the array.
      arrayPtr = *reinterpret_cast<PVOID*>(reinterpret_cast<PBYTE>(stackBase) +
                                           paramInfo.stackOffset);
    }
  } else if (FAILED(hr)) {
    return hr;
  } else {
    arrayPtr = ResolveArrayPtr(paramVal);
  }

  MOZ_ASSERT(arrayPtr);
  if (!arrayPtr) {
    return DISP_E_BADVARTYPE;
  }

  // We walk the elements of the array and invoke OnWalkInterface to wrap each
  // one, just as ICallFrame::WalkFrame would do.
  for (LONG index = 0; index < arrayLength; ++index) {
    hr = OnWalkInterface(aArrayData.mArrayParamIid,
                         ResolveInterfacePtr(arrayPtr, paramVal.vt, index),
                         FALSE, TRUE);
    if (FAILED(hr)) {
      return hr;
    }
  }
  return S_OK;
}

HRESULT
MainThreadHandoff::SetInterceptor(IWeakReference* aInterceptor)
{
  mInterceptor = aInterceptor;
  return S_OK;
}

HRESULT
MainThreadHandoff::GetHandler(NotNull<CLSID*> aHandlerClsid)
{
  if (!mHandlerProvider) {
    return E_NOTIMPL;
  }

  return mHandlerProvider->GetHandler(aHandlerClsid);
}

HRESULT
MainThreadHandoff::GetHandlerPayloadSize(NotNull<DWORD*> aOutPayloadSize)
{
  if (!mHandlerProvider) {
    return E_NOTIMPL;
  }
  return mHandlerProvider->GetHandlerPayloadSize(aOutPayloadSize);
}

HRESULT
MainThreadHandoff::WriteHandlerPayload(NotNull<IStream*> aStream)
{
  if (!mHandlerProvider) {
    return E_NOTIMPL;
  }
  return mHandlerProvider->WriteHandlerPayload(aStream);
}

REFIID
MainThreadHandoff::MarshalAs(REFIID aIid)
{
  if (!mHandlerProvider) {
    return aIid;
  }
  return mHandlerProvider->MarshalAs(aIid);
}

HRESULT
MainThreadHandoff::OnWalkInterface(REFIID aIid, PVOID* aInterface,
                                   BOOL aIsInParam, BOOL aIsOutParam)
{
  MOZ_ASSERT(aInterface && aIsOutParam);
  if (!aInterface || !aIsOutParam) {
    return E_UNEXPECTED;
  }

  // Adopt aInterface for the time being. We can't touch its refcount off
  // the main thread, so we'll use STAUniquePtr so that we can safely
  // Release() it if necessary.
  STAUniquePtr<IUnknown> origInterface(static_cast<IUnknown*>(*aInterface));
  *aInterface = nullptr;

  if (!origInterface) {
    // Nothing to wrap.
    return S_OK;
  }

  // First make sure that aInterface isn't a proxy - we don't want to wrap
  // those.
  if (IsProxy(origInterface.get())) {
    *aInterface = origInterface.release();
    return S_OK;
  }

  RefPtr<IInterceptor> interceptor;
  HRESULT hr = mInterceptor->Resolve(IID_IInterceptor,
                                     (void**) getter_AddRefs(interceptor));
  MOZ_ASSERT(SUCCEEDED(hr));
  if (FAILED(hr)) {
    return hr;
  }

  // Now make sure that origInterface isn't referring to the same IUnknown
  // as an interface that we are already managing. We can determine this by
  // querying (NOT casting!) both objects for IUnknown and then comparing the
  // resulting pointers.
  InterceptorTargetPtr<IUnknown> existingTarget;
  hr = interceptor->GetTargetForIID(aIid, existingTarget);
  if (SUCCEEDED(hr)) {
    // We'll start by checking the raw pointers. If they are equal, then the
    // objects are equal. OTOH, if they differ, we must compare their
    // IUnknown pointers to know for sure.
    bool areTargetsEqual = existingTarget.get() == origInterface.get();

    if (!areTargetsEqual) {
      // This check must be done on the main thread
      auto checkFn = [&existingTarget, &origInterface, &areTargetsEqual]() -> void {
        RefPtr<IUnknown> unkExisting;
        HRESULT hrExisting =
          existingTarget->QueryInterface(IID_IUnknown,
                                         (void**)getter_AddRefs(unkExisting));
        RefPtr<IUnknown> unkNew;
        HRESULT hrNew =
          origInterface->QueryInterface(IID_IUnknown,
                                        (void**)getter_AddRefs(unkNew));
        areTargetsEqual = SUCCEEDED(hrExisting) && SUCCEEDED(hrNew) &&
                          unkExisting == unkNew;
      };

      MainThreadInvoker invoker;
      invoker.Invoke(NS_NewRunnableFunction("MainThreadHandoff::OnWalkInterface", checkFn));
    }

    if (areTargetsEqual) {
      // The existing interface and the new interface both belong to the same
      // target object. Let's just use the existing one.
      void* intercepted = nullptr;
      hr = interceptor->GetInterceptorForIID(aIid, &intercepted);
      MOZ_ASSERT(SUCCEEDED(hr));
      if (FAILED(hr)) {
        return hr;
      }
      *aInterface = intercepted;
      return S_OK;
    }
  }

  RefPtr<IHandlerProvider> payload;
  if (mHandlerProvider) {
    hr = mHandlerProvider->NewInstance(aIid,
                                       ToInterceptorTargetPtr(origInterface),
                                       WrapNotNull((IHandlerProvider**)getter_AddRefs(payload)));
    MOZ_ASSERT(SUCCEEDED(hr));
    if (FAILED(hr)) {
      return hr;
    }
  }

  // Now create a new MainThreadHandoff wrapper...
  RefPtr<IInterceptorSink> handoff;
  hr = MainThreadHandoff::Create(payload, getter_AddRefs(handoff));
  MOZ_ASSERT(SUCCEEDED(hr));
  if (FAILED(hr)) {
    return hr;
  }

  REFIID interceptorIid = payload ? payload->MarshalAs(aIid) : aIid;

  RefPtr<IUnknown> wrapped;
  hr = Interceptor::Create(Move(origInterface), handoff, interceptorIid,
                           getter_AddRefs(wrapped));
  MOZ_ASSERT(SUCCEEDED(hr));
  if (FAILED(hr)) {
    return hr;
  }

  // And replace the original interface pointer with the wrapped one.
  wrapped.forget(reinterpret_cast<IUnknown**>(aInterface));

  return S_OK;
}

} // namespace mscom
} // namespace mozilla
