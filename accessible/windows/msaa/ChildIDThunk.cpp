/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChildIDThunk.h"
#include "MainThreadUtils.h"
#include "mozilla/mscom/InterceptorLog.h"

using namespace mozilla::mscom;

namespace mozilla {
namespace a11y {

ChildIDThunk::ChildIDThunk()
  : mRefCnt(0)
{
}

HRESULT
ChildIDThunk::QueryInterface(REFIID riid, void** ppv)
{
  if (!ppv) {
    return E_INVALIDARG;
  }

  if (riid == IID_IUnknown || riid == IID_ICallFrameEvents ||
      riid == IID_IInterceptorSink) {
    *ppv = static_cast<IInterceptorSink*>(this);
    AddRef();
    return S_OK;
  }

  *ppv = nullptr;
  return E_NOINTERFACE;
}

ULONG
ChildIDThunk::AddRef()
{
  return (ULONG) InterlockedIncrement((LONG*)&mRefCnt);
}

ULONG
ChildIDThunk::Release()
{
  ULONG newRefCnt = (ULONG) InterlockedDecrement((LONG*)&mRefCnt);
  if (newRefCnt == 0) {
    MOZ_ASSERT(NS_IsMainThread());
    delete this;
  }
  return newRefCnt;
}

enum IAccessibleVTableIndex
{
  eget_accName = 10,
  eget_accValue = 11,
  eget_accDescription = 12,
  eget_accRole = 13,
  eget_accState = 14,
  eget_accHelp = 15,
  eget_accHelpTopic = 16,
  eget_accKeyboardShortcut = 17,
  eget_accDefaultAction = 20,
  eaccSelect = 21,
  eaccLocation = 22,
  eaccNavigate = 23,
  eaccDoDefaultAction = 25,
  eput_accName = 26,
  eput_accValue = 27
};

Maybe<ULONG>
ChildIDThunk::IsMethodInteresting(const ULONG aMethodIdx)
{
  switch (aMethodIdx) {
    case eget_accName:
    case eget_accValue:
    case eget_accDescription:
    case eget_accRole:
    case eget_accState:
    case eget_accHelp:
    case eget_accKeyboardShortcut:
    case eget_accDefaultAction:
    case eaccDoDefaultAction:
    case eput_accName:
    case eput_accValue:
      return Some(0UL);
    case eget_accHelpTopic:
    case eaccNavigate:
      return Some(1UL);
    case eaccSelect:
      return Some(2UL);
    case eaccLocation:
      return Some(4UL);
    default:
      return Nothing();
  }
}

bool
ChildIDThunk::IsChildIDSelf(ICallFrame* aFrame, const ULONG aChildIDIdx,
                            LONG& aOutChildID)
{
  VARIANT paramValue;
  HRESULT hr = aFrame->GetParam(aChildIDIdx, &paramValue);
  MOZ_ASSERT(SUCCEEDED(hr));
  if (FAILED(hr)) {
    return false;
  }

  const bool isVariant = paramValue.vt & VT_VARIANT;
  MOZ_ASSERT(isVariant);
  if (!isVariant) {
    return false;
  }

  VARIANT& childID = *(paramValue.pvarVal);
  if (childID.vt != VT_I4) {
    return false;
  }

  aOutChildID = childID.lVal;
  return childID.lVal == CHILDID_SELF;
}

/**
 * ICallFrame::SetParam always returns E_NOTIMPL, so we'll just have to work
 * around this by manually poking at the parameter's location on the stack.
 */
/* static */ HRESULT
ChildIDThunk::SetChildIDParam(ICallFrame* aFrame, const ULONG aParamIdx,
                              const LONG aChildID)
{
  void* stackBase = aFrame->GetStackLocation();
  MOZ_ASSERT(stackBase);
  if (!stackBase) {
    return E_UNEXPECTED;
  }

  CALLFRAMEPARAMINFO paramInfo;
  HRESULT hr = aFrame->GetParamInfo(aParamIdx, &paramInfo);
  MOZ_ASSERT(SUCCEEDED(hr));
  if (FAILED(hr)) {
    return hr;
  }

  // We expect this childID to be a VARIANT in-parameter
  MOZ_ASSERT(paramInfo.fIn);
  MOZ_ASSERT(paramInfo.cbParam == sizeof(VARIANT));
  if (!paramInfo.fIn || paramInfo.cbParam != sizeof(VARIANT)) {
    return E_UNEXPECTED;
  }

  // Given the stack base and param offset, calculate the address of the param
  // and replace its value
  VARIANT* pInParam = reinterpret_cast<VARIANT*>(
      reinterpret_cast<PBYTE>(stackBase) + paramInfo.stackOffset);
  MOZ_ASSERT(pInParam->vt == VT_I4);
  if (pInParam->vt != VT_I4) {
    return E_UNEXPECTED;
  }
  pInParam->lVal = aChildID;
  return S_OK;
}

HRESULT
ChildIDThunk::ResolveTargetInterface(REFIID aIid, IUnknown** aOut)
{
  MOZ_ASSERT(aOut);
  *aOut = nullptr;

  RefPtr<IInterceptor> interceptor;
  HRESULT hr = mInterceptor->Resolve(IID_IInterceptor,
                                     (void**)getter_AddRefs(interceptor));
  if (FAILED(hr)) {
    return hr;
  }

  InterceptorTargetPtr target;
  hr = interceptor->GetTargetForIID(aIid, target);
  if (FAILED(hr)) {
    return hr;
  }

  RefPtr<IUnknown> refTarget = target.get();
  refTarget.forget(aOut);
  return S_OK;
}

HRESULT
ChildIDThunk::OnCall(ICallFrame* aFrame)
{
#if defined(MOZILLA_INTERNAL_API)
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsParentProcess());
#endif

  IID iid;
  ULONG method;
  HRESULT hr = aFrame->GetIIDAndMethod(&iid, &method);
  MOZ_ASSERT(SUCCEEDED(hr));
  if (FAILED(hr)) {
    return hr;
  }

  RefPtr<IUnknown> target;
  hr = ResolveTargetInterface(iid, getter_AddRefs(target));
  if (FAILED(hr)) {
    return hr;
  }

  Maybe<ULONG> childIDIdx;
  LONG childID;
  if (iid != IID_IAccessible ||
      (childIDIdx = IsMethodInteresting(method)).isNothing() ||
      IsChildIDSelf(aFrame, childIDIdx.value(), childID)) {
    // We're only interested in methods which accept a child ID parameter which
    // is not equal to CHILDID_SELF. Otherwise we just invoke against the
    // original target interface.
    hr = aFrame->Invoke(target);
    if (SUCCEEDED(hr)) {
      InterceptorLog::Event(aFrame, target);
    }
    return hr;
  }

  // Otherwise we need to resolve the child node...
  RefPtr<IAccessible> acc;
  hr = target->QueryInterface(iid, (void**)getter_AddRefs(acc));
  if (FAILED(hr)) {
    return hr;
  }

  VARIANT vChildID;
  VariantInit(&vChildID);
  vChildID.vt = VT_I4;
  vChildID.lVal = childID;

  RefPtr<IDispatch> disp;
  hr = acc->get_accChild(vChildID, getter_AddRefs(disp));
  if (FAILED(hr)) {
    aFrame->SetReturnValue(hr);
    return S_OK;
  }

  RefPtr<IAccessible> directAccessible;
  // Yes, given our implementation we could just downcast, but that's not very COMy
  hr = disp->QueryInterface(IID_IAccessible,
                            (void**)getter_AddRefs(directAccessible));
  if (FAILED(hr)) {
    aFrame->SetReturnValue(hr);
    return S_OK;
  }

  // Now that we have the IAccessible for the child ID we need to replace it
  // in the activation record with a self-referant child ID
  hr = SetChildIDParam(aFrame, childIDIdx.value(), CHILDID_SELF);
  MOZ_ASSERT(SUCCEEDED(hr));
  if (FAILED(hr)) {
    return hr;
  }

  hr = aFrame->Invoke(directAccessible);
  if (SUCCEEDED(hr)) {
    InterceptorLog::Event(aFrame, directAccessible);
  }
  return hr;
}

HRESULT
ChildIDThunk::SetInterceptor(IWeakReference* aInterceptor)
{
  mInterceptor = aInterceptor;
  return S_OK;
}

} // namespace a11y
} // namespace mozilla

