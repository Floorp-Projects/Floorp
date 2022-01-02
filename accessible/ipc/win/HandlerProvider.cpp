/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/a11y/HandlerProvider.h"

#include <memory.h>

#include <utility>

#include "Accessible2_3.h"
#include "AccessibleApplication.h"
#include "AccessibleDocument.h"
#include "AccessibleEditableText.h"
#include "AccessibleImage.h"
#include "AccessibleRelation.h"
#include "AccessibleTable.h"
#include "AccessibleTable2.h"
#include "AccessibleTableCell.h"
#include "HandlerData.h"
#include "HandlerData_i.c"
#include "ISimpleDOM.h"
#include "mozilla/Assertions.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/a11y/AccessibleWrap.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/mscom/AgileReference.h"
#include "mozilla/mscom/FastMarshaler.h"
#include "mozilla/mscom/Interceptor.h"
#include "mozilla/mscom/MainThreadHandoff.h"
#include "mozilla/mscom/MainThreadInvoker.h"
#include "mozilla/mscom/Ptr.h"
#include "mozilla/mscom/StructStream.h"
#include "mozilla/mscom/Utils.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "uiautomation.h"

namespace mozilla {
namespace a11y {

HandlerProvider::HandlerProvider(REFIID aIid,
                                 mscom::InterceptorTargetPtr<IUnknown> aTarget)
    : mRefCnt(0),
      mMutex("mozilla::a11y::HandlerProvider::mMutex"),
      mTargetUnkIid(aIid),
      mTargetUnk(std::move(aTarget)),
      mPayloadMutex("mozilla::a11y::HandlerProvider::mPayloadMutex") {}

HRESULT
HandlerProvider::QueryInterface(REFIID riid, void** ppv) {
  if (!ppv) {
    return E_INVALIDARG;
  }

  if (riid == IID_IUnknown || riid == IID_IGeckoBackChannel) {
    RefPtr<IUnknown> punk(static_cast<IGeckoBackChannel*>(this));
    punk.forget(ppv);
    return S_OK;
  }

  if (riid == IID_IMarshal) {
    if (!mFastMarshalUnk) {
      HRESULT hr =
          mscom::FastMarshaler::Create(static_cast<IGeckoBackChannel*>(this),
                                       getter_AddRefs(mFastMarshalUnk));
      if (FAILED(hr)) {
        return hr;
      }
    }

    return mFastMarshalUnk->QueryInterface(riid, ppv);
  }

  return E_NOINTERFACE;
}

ULONG
HandlerProvider::AddRef() { return ++mRefCnt; }

ULONG
HandlerProvider::Release() {
  ULONG result = --mRefCnt;
  if (!result) {
    delete this;
  }
  return result;
}

HRESULT
HandlerProvider::GetHandler(NotNull<CLSID*> aHandlerClsid) {
  if (!IsTargetInterfaceCacheable()) {
    return E_NOINTERFACE;
  }

  *aHandlerClsid = CLSID_AccessibleHandler;
  return S_OK;
}

void HandlerProvider::GetAndSerializePayload(
    const MutexAutoLock&, NotNull<mscom::IInterceptor*> aInterceptor) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());

  if (mSerializer) {
    return;
  }

  IA2PayloadPtr payload;
  {  // Scope for lock
    MutexAutoLock lock(mPayloadMutex);
    if (mPayload) {
      // The payload was already built by prebuildPayload() called during a
      // bulk fetch operation.
      payload = std::move(mPayload);
    }
  }

  if (!payload) {
    // We don't have a pre-built payload, so build it now.
    payload.reset(new IA2Payload());
    if (!mscom::InvokeOnMainThread(
            "HandlerProvider::BuildInitialIA2Data", this,
            &HandlerProvider::BuildInitialIA2Data,
            std::forward<NotNull<mscom::IInterceptor*>>(aInterceptor),
            std::forward<StaticIA2Data*>(&payload->mStaticData),
            std::forward<DynamicIA2Data*>(&payload->mDynamicData)) ||
        !payload->mDynamicData.mUniqueId) {
      return;
    }
  }

  // But we set mGeckoBackChannel on the current thread which resides in the
  // MTA. This is important to ensure that COM always invokes
  // IGeckoBackChannel methods in an MTA background thread.
  RefPtr<IGeckoBackChannel> payloadRef(this);
  // AddRef/Release pair for this reference is handled by payloadRef
  payload->mGeckoBackChannel = this;

  mSerializer = MakeUnique<mscom::StructToStream>(*payload, &IA2Payload_Encode);
}

HRESULT
HandlerProvider::GetHandlerPayloadSize(
    NotNull<mscom::IInterceptor*> aInterceptor,
    NotNull<DWORD*> aOutPayloadSize) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());

  if (!IsTargetInterfaceCacheable()) {
    // No handler, so no payload for this instance.
    return E_NOTIMPL;
  }

  MutexAutoLock lock(mMutex);

  GetAndSerializePayload(lock, aInterceptor);

  if (!mSerializer || !(*mSerializer)) {
    // Failed payload serialization is non-fatal
    *aOutPayloadSize = mscom::StructToStream::GetEmptySize();
    return S_OK;
  }

  *aOutPayloadSize = mSerializer->GetSize();
  return S_OK;
}

void HandlerProvider::BuildStaticIA2Data(
    NotNull<mscom::IInterceptor*> aInterceptor, StaticIA2Data* aOutData) {
  MOZ_ASSERT(aOutData);
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mTargetUnk);
  MOZ_ASSERT(IsTargetInterfaceCacheable());

  // Include interfaces the client is likely to request.
  // This is cheap here and saves multiple cross-process calls later.
  // These interfaces must be released in ReleaseStaticIA2DataInterfaces!

  // If the target is already an IAccessible2, this pointer is redundant.
  // However, the target might be an IAccessibleHyperlink, etc., in which
  // case the client will almost certainly QI for IAccessible2.
  HRESULT hr = aInterceptor->GetInterceptorForIID(NEWEST_IA2_IID,
                                                  (void**)&aOutData->mIA2);
  if (FAILED(hr)) {
    // IA2 should always be present, so something has
    // gone very wrong if this fails.
    aOutData->mIA2 = nullptr;
    return;
  }

  // Some of these interfaces aren't present on all accessibles,
  // so it's not a failure if these interfaces can't be fetched.
  hr = aInterceptor->GetInterceptorForIID(IID_IAccessibleHypertext2,
                                          (void**)&aOutData->mIAHypertext);
  if (FAILED(hr)) {
    aOutData->mIAHypertext = nullptr;
  }

  hr = aInterceptor->GetInterceptorForIID(IID_IAccessibleHyperlink,
                                          (void**)&aOutData->mIAHyperlink);
  if (FAILED(hr)) {
    aOutData->mIAHyperlink = nullptr;
  }

  hr = aInterceptor->GetInterceptorForIID(IID_IAccessibleTable,
                                          (void**)&aOutData->mIATable);
  if (FAILED(hr)) {
    aOutData->mIATable = nullptr;
  }

  hr = aInterceptor->GetInterceptorForIID(IID_IAccessibleTable2,
                                          (void**)&aOutData->mIATable2);
  if (FAILED(hr)) {
    aOutData->mIATable2 = nullptr;
  }

  hr = aInterceptor->GetInterceptorForIID(IID_IAccessibleTableCell,
                                          (void**)&aOutData->mIATableCell);
  if (FAILED(hr)) {
    aOutData->mIATableCell = nullptr;
  }
}

void HandlerProvider::BuildDynamicIA2Data(DynamicIA2Data* aOutIA2Data,
                                          bool aMarshaledByCom) {
  MOZ_ASSERT(aOutIA2Data);
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(IsTargetInterfaceCacheable());

  if (!mTargetUnk) {
    return;
  }

  RefPtr<NEWEST_IA2_INTERFACE> target;
  HRESULT hr =
      mTargetUnk.get()->QueryInterface(NEWEST_IA2_IID, getter_AddRefs(target));
  if (FAILED(hr)) {
    return;
  }

  hr = E_UNEXPECTED;

  auto hasFailed = [&hr]() -> bool { return FAILED(hr); };

  auto cleanup = [aOutIA2Data, aMarshaledByCom]() -> void {
    CleanupDynamicIA2Data(*aOutIA2Data, aMarshaledByCom);
  };

  mscom::ExecuteWhen<decltype(hasFailed), decltype(cleanup)> onFail(hasFailed,
                                                                    cleanup);

  // When allocating memory to be returned to the client, you *must* use
  // allocMem, not CoTaskMemAlloc!
  auto allocMem = [aMarshaledByCom](size_t aSize) {
    if (aMarshaledByCom) {
      return ::CoTaskMemAlloc(aSize);
    }
    // We use midl_user_allocate rather than CoTaskMemAlloc because this
    // struct is being marshaled by RPC, not COM.
    return ::midl_user_allocate(aSize);
  };

  const VARIANT kChildIdSelf = {VT_I4};
  VARIANT varVal;

  hr = target->accLocation(&aOutIA2Data->mLeft, &aOutIA2Data->mTop,
                           &aOutIA2Data->mWidth, &aOutIA2Data->mHeight,
                           kChildIdSelf);
  if (FAILED(hr)) {
    return;
  }

  hr = target->get_accRole(kChildIdSelf, &aOutIA2Data->mRole);
  if (FAILED(hr)) {
    return;
  }

  hr = target->get_accState(kChildIdSelf, &varVal);
  if (FAILED(hr)) {
    return;
  }

  aOutIA2Data->mState = varVal.lVal;

  hr = target->get_accKeyboardShortcut(kChildIdSelf,
                                       &aOutIA2Data->mKeyboardShortcut);
  if (FAILED(hr)) {
    return;
  }

  hr = target->get_accName(kChildIdSelf, &aOutIA2Data->mName);
  if (FAILED(hr)) {
    return;
  }

  hr = target->get_accDescription(kChildIdSelf, &aOutIA2Data->mDescription);
  if (FAILED(hr)) {
    return;
  }

  hr = target->get_accDefaultAction(kChildIdSelf, &aOutIA2Data->mDefaultAction);
  if (FAILED(hr)) {
    return;
  }

  hr = target->get_accChildCount(&aOutIA2Data->mChildCount);
  if (FAILED(hr)) {
    return;
  }

  hr = target->get_accValue(kChildIdSelf, &aOutIA2Data->mValue);
  if (FAILED(hr)) {
    return;
  }

  hr = target->get_states(&aOutIA2Data->mIA2States);
  if (FAILED(hr)) {
    return;
  }

  hr = target->get_attributes(&aOutIA2Data->mAttributes);
  if (FAILED(hr)) {
    return;
  }

  HWND hwnd;
  hr = target->get_windowHandle(&hwnd);
  if (FAILED(hr)) {
    return;
  }

  aOutIA2Data->mHwnd = PtrToLong(hwnd);

  hr = target->get_locale(&aOutIA2Data->mIA2Locale);
  if (FAILED(hr)) {
    return;
  }

  hr = target->role(&aOutIA2Data->mIA2Role);
  if (FAILED(hr)) {
    return;
  }

  RefPtr<IAccessibleAction> action;
  // It is not an error if this fails.
  hr = mTargetUnk.get()->QueryInterface(IID_IAccessibleAction,
                                        getter_AddRefs(action));
  if (SUCCEEDED(hr)) {
    hr = action->nActions(&aOutIA2Data->mNActions);
    if (FAILED(hr)) {
      return;
    }
  }

  RefPtr<IAccessibleTableCell> cell;
  // It is not an error if this fails.
  hr = mTargetUnk.get()->QueryInterface(IID_IAccessibleTableCell,
                                        getter_AddRefs(cell));
  if (SUCCEEDED(hr)) {
    hr = cell->get_rowColumnExtents(
        &aOutIA2Data->mRowIndex, &aOutIA2Data->mColumnIndex,
        &aOutIA2Data->mRowExtent, &aOutIA2Data->mColumnExtent,
        &aOutIA2Data->mCellIsSelected);
    if (FAILED(hr)) {
      return;
    }

    // Because the same headers can apply to many cells, include the ids of
    // header cells, rather than the actual objects. Otherwise, we might
    // end up marshaling the same objects (and their payloads) many times.
    IUnknown** headers = nullptr;
    hr = cell->get_rowHeaderCells(&headers, &aOutIA2Data->mNRowHeaderCells);
    if (FAILED(hr)) {
      return;
    }
    if (aOutIA2Data->mNRowHeaderCells > 0) {
      aOutIA2Data->mRowHeaderCellIds = static_cast<long*>(
          allocMem(sizeof(long) * aOutIA2Data->mNRowHeaderCells));
      for (long i = 0; i < aOutIA2Data->mNRowHeaderCells; ++i) {
        RefPtr<IAccessible2> headerAcc;
        hr = headers[i]->QueryInterface(IID_IAccessible2,
                                        getter_AddRefs(headerAcc));
        MOZ_ASSERT(SUCCEEDED(hr));
        headers[i]->Release();
        hr = headerAcc->get_uniqueID(&aOutIA2Data->mRowHeaderCellIds[i]);
        MOZ_ASSERT(SUCCEEDED(hr));
      }
    }
    ::CoTaskMemFree(headers);

    hr = cell->get_columnHeaderCells(&headers,
                                     &aOutIA2Data->mNColumnHeaderCells);
    if (FAILED(hr)) {
      return;
    }
    if (aOutIA2Data->mNColumnHeaderCells > 0) {
      aOutIA2Data->mColumnHeaderCellIds = static_cast<long*>(
          allocMem(sizeof(long) * aOutIA2Data->mNColumnHeaderCells));
      for (long i = 0; i < aOutIA2Data->mNColumnHeaderCells; ++i) {
        RefPtr<IAccessible2> headerAcc;
        hr = headers[i]->QueryInterface(IID_IAccessible2,
                                        getter_AddRefs(headerAcc));
        MOZ_ASSERT(SUCCEEDED(hr));
        headers[i]->Release();
        hr = headerAcc->get_uniqueID(&aOutIA2Data->mColumnHeaderCellIds[i]);
        MOZ_ASSERT(SUCCEEDED(hr));
      }
    }
    ::CoTaskMemFree(headers);
  }

  // NB: get_uniqueID should be the final property retrieved in this method,
  // as its presence is used to determine whether the rest of this data
  // retrieval was successful.
  hr = target->get_uniqueID(&aOutIA2Data->mUniqueId);
}

void HandlerProvider::BuildInitialIA2Data(
    NotNull<mscom::IInterceptor*> aInterceptor, StaticIA2Data* aOutStaticData,
    DynamicIA2Data* aOutDynamicData) {
  BuildStaticIA2Data(aInterceptor, aOutStaticData);
  if (!aOutStaticData->mIA2) {
    return;
  }
  BuildDynamicIA2Data(aOutDynamicData);
}

bool HandlerProvider::IsTargetInterfaceCacheable() {
  return MarshalAs(mTargetUnkIid) == NEWEST_IA2_IID ||
         mTargetUnkIid == IID_IAccessibleHyperlink;
}

HRESULT
HandlerProvider::WriteHandlerPayload(NotNull<mscom::IInterceptor*> aInterceptor,
                                     NotNull<IStream*> aStream) {
  if (!IsTargetInterfaceCacheable()) {
    // No handler, so no payload for this instance.
    return E_NOTIMPL;
  }

  MutexAutoLock lock(mMutex);

  if (!mSerializer || !(*mSerializer)) {
    // Failed payload serialization is non-fatal
    mscom::StructToStream emptyStruct;
    return emptyStruct.Write(aStream);
  }

  HRESULT hr = mSerializer->Write(aStream);

  mSerializer.reset();

  return hr;
}

REFIID
HandlerProvider::MarshalAs(REFIID aIid) {
  static_assert(&NEWEST_IA2_IID == &IID_IAccessible2_3,
                "You have modified NEWEST_IA2_IID. This code needs updating.");
  if (aIid == IID_IDispatch || aIid == IID_IAccessible ||
      aIid == IID_IAccessible2 || aIid == IID_IAccessible2_2 ||
      aIid == IID_IAccessible2_3) {
    // This should always be the newest IA2 interface ID
    return NEWEST_IA2_IID;
  }
  // Otherwise we juse return the identity.
  return aIid;
}

HRESULT
HandlerProvider::DisconnectHandlerRemotes() {
  // If a handlerProvider call is pending on another thread,
  // CoDisconnectObject won't release this HandlerProvider immediately.
  // However, the interceptor and its target (mTargetUnk) might be destroyed.
  mTargetUnk = nullptr;

  IUnknown* unk = static_cast<IGeckoBackChannel*>(this);
  return ::CoDisconnectObject(unk, 0);
}

HRESULT
HandlerProvider::IsInterfaceMaybeSupported(REFIID aIid) {
  static_assert(&NEWEST_IA2_IID == &IID_IAccessible2_3,
                "You have modified NEWEST_IA2_IID. This code needs updating.");
  if (aIid == IID_IUnknown || aIid == IID_IDispatch ||
      aIid == IID_IAccessible || aIid == IID_IServiceProvider ||
      aIid == IID_IEnumVARIANT || aIid == IID_IAccessible2 ||
      aIid == IID_IAccessible2_2 || aIid == IID_IAccessible2_3 ||
      aIid == IID_IAccessibleAction || aIid == IID_IAccessibleApplication ||
      aIid == IID_IAccessibleComponent || aIid == IID_IAccessibleDocument ||
      aIid == IID_IAccessibleEditableText || aIid == IID_IAccessibleHyperlink ||
      aIid == IID_IAccessibleHypertext || aIid == IID_IAccessibleHypertext2 ||
      aIid == IID_IAccessibleImage || aIid == IID_IAccessibleRelation ||
      aIid == IID_IAccessibleTable || aIid == IID_IAccessibleTable2 ||
      aIid == IID_IAccessibleTableCell || aIid == IID_IAccessibleText ||
      aIid == IID_IAccessibleValue || aIid == IID_ISimpleDOMNode ||
      aIid == IID_ISimpleDOMDocument || aIid == IID_ISimpleDOMText ||
      aIid == IID_IAccessibleEx || aIid == IID_IRawElementProviderSimple) {
    return S_OK;
  }
  return E_NOINTERFACE;
}

REFIID
HandlerProvider::GetEffectiveOutParamIid(REFIID aCallIid, ULONG aCallMethod) {
  if (aCallIid == IID_IAccessibleTable || aCallIid == IID_IAccessibleTable2 ||
      aCallIid == IID_IAccessibleDocument ||
      aCallIid == IID_IAccessibleTableCell ||
      aCallIid == IID_IAccessibleRelation) {
    return NEWEST_IA2_IID;
  }

  // IAccessible2_2::accessibleWithCaret
  static_assert(&NEWEST_IA2_IID == &IID_IAccessible2_3,
                "You have modified NEWEST_IA2_IID. This code needs updating.");
  if ((aCallIid == IID_IAccessible2_2 || aCallIid == IID_IAccessible2_3) &&
      aCallMethod == 47) {
    return NEWEST_IA2_IID;
  }

  // IAccessible::get_accSelection
  if ((aCallIid == IID_IAccessible || aCallIid == IID_IAccessible2 ||
       aCallIid == IID_IAccessible2_2 || aCallIid == IID_IAccessible2_3) &&
      aCallMethod == 19) {
    return IID_IEnumVARIANT;
  }

  MOZ_ASSERT(false);
  return IID_IUnknown;
}

HRESULT
HandlerProvider::NewInstance(
    REFIID aIid, mscom::InterceptorTargetPtr<IUnknown> aTarget,
    NotNull<mscom::IHandlerProvider**> aOutNewPayload) {
  RefPtr<IHandlerProvider> newPayload(
      new HandlerProvider(aIid, std::move(aTarget)));
  newPayload.forget(aOutNewPayload.get());
  return S_OK;
}

void HandlerProvider::SetHandlerControlOnMainThread(
    DWORD aPid, mscom::ProxyUniquePtr<IHandlerControl> aCtrl) {
  MOZ_ASSERT(NS_IsMainThread());

  auto content = dom::ContentChild::GetSingleton();
  MOZ_ASSERT(content);

  IHandlerControlHolder holder(
      CreateHolderFromHandlerControl(std::move(aCtrl)));
  Unused << content->SendA11yHandlerControl(aPid, holder);
}

HRESULT
HandlerProvider::put_HandlerControl(long aPid, IHandlerControl* aCtrl) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());

  if (!aCtrl) {
    return E_INVALIDARG;
  }

  auto ptrProxy = mscom::ToProxyUniquePtr(aCtrl);

  if (!mscom::InvokeOnMainThread(
          "HandlerProvider::SetHandlerControlOnMainThread", this,
          &HandlerProvider::SetHandlerControlOnMainThread,
          static_cast<DWORD>(aPid), std::move(ptrProxy))) {
    return E_FAIL;
  }

  return S_OK;
}

HRESULT
HandlerProvider::Refresh(DynamicIA2Data* aOutData) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());

  if (!mTargetUnk) {
    return CO_E_OBJNOTCONNECTED;
  }

  if (!mscom::InvokeOnMainThread("HandlerProvider::BuildDynamicIA2Data", this,
                                 &HandlerProvider::BuildDynamicIA2Data,
                                 std::forward<DynamicIA2Data*>(aOutData),
                                 /* aMarshaledByCom */ true)) {
    return E_FAIL;
  }

  if (!aOutData->mUniqueId) {
    // BuildDynamicIA2Data failed.
    if (!mTargetUnk) {
      // Even though we checked this before, the accessible can be shut down
      // before BuildDynamicIA2Data executes on the main thread.
      return CO_E_OBJNOTCONNECTED;
    }
    return E_FAIL;
  }

  return S_OK;
}

void HandlerProvider::PrebuildPayload(
    NotNull<mscom::IInterceptor*> aInterceptor) {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mPayloadMutex);
  mPayload.reset(new IA2Payload());
  BuildInitialIA2Data(aInterceptor, &mPayload->mStaticData,
                      &mPayload->mDynamicData);
  if (!mPayload->mDynamicData.mUniqueId) {
    // Building the payload failed.
    mPayload.reset();
  }
}

template <typename Interface>
HRESULT HandlerProvider::ToWrappedObject(Interface** aObj) {
  MOZ_ASSERT(NS_IsMainThread());
  mscom::STAUniquePtr<Interface> inObj(*aObj);
  RefPtr<HandlerProvider> hprov = new HandlerProvider(
      __uuidof(Interface), mscom::ToInterceptorTargetPtr(inObj));
  HRESULT hr =
      mscom::MainThreadHandoff::WrapInterface(std::move(inObj), hprov, aObj);
  if (FAILED(hr)) {
    *aObj = nullptr;
    return hr;
  }
  // Build the payload for this object now to avoid a cross-thread call when
  // marshaling it later.
  RefPtr<mscom::IInterceptor> interceptor;
  hr = (*aObj)->QueryInterface(mscom::IID_IInterceptor,
                               getter_AddRefs(interceptor));
  MOZ_ASSERT(SUCCEEDED(hr));
  // Even though we created a new HandlerProvider, that won't be used if
  // there's an existing Interceptor. Therefore, we must get the
  // HandlerProvider from the Interceptor.
  RefPtr<mscom::IInterceptorSink> interceptorSink;
  interceptor->GetEventSink(getter_AddRefs(interceptorSink));
  MOZ_ASSERT(interceptorSink);
  RefPtr<mscom::IMainThreadHandoff> handoff;
  hr = interceptorSink->QueryInterface(mscom::IID_IMainThreadHandoff,
                                       getter_AddRefs(handoff));
  // If a11y Interceptors stop using MainThreadHandoff as their event sink, we
  // *really* want to know about it ASAP.
  MOZ_DIAGNOSTIC_ASSERT(SUCCEEDED(hr),
                        "A11y Interceptor isn't using MainThreadHandoff");
  RefPtr<mscom::IHandlerProvider> usedIHprov;
  handoff->GetHandlerProvider(getter_AddRefs(usedIHprov));
  MOZ_ASSERT(usedIHprov);
  auto usedHprov = static_cast<HandlerProvider*>(usedIHprov.get());
  usedHprov->PrebuildPayload(WrapNotNull(interceptor));
  return hr;
}

void HandlerProvider::GetAllTextInfoMainThread(
    BSTR* aText, IAccessibleHyperlink*** aHyperlinks, long* aNHyperlinks,
    IA2TextSegment** aAttribRuns, long* aNAttribRuns, HRESULT* result) {
  MOZ_ASSERT(aText);
  MOZ_ASSERT(aHyperlinks);
  MOZ_ASSERT(aNHyperlinks);
  MOZ_ASSERT(aAttribRuns);
  MOZ_ASSERT(aNAttribRuns);
  MOZ_ASSERT(NS_IsMainThread());

  if (!mTargetUnk) {
    *result = CO_E_OBJNOTCONNECTED;
    return;
  }

  RefPtr<IAccessibleHypertext2> ht;
  HRESULT hr =
      mTargetUnk->QueryInterface(IID_IAccessibleHypertext2, getter_AddRefs(ht));
  if (FAILED(hr)) {
    *result = hr;
    return;
  }

  hr = ht->get_text(0, IA2_TEXT_OFFSET_LENGTH, aText);
  if (FAILED(hr)) {
    *result = hr;
    return;
  }

  if (hr == S_FALSE) {
    // No text.
    *aHyperlinks = nullptr;
    *aNHyperlinks = 0;
    *aAttribRuns = nullptr;
    *aNAttribRuns = 0;
    *result = S_FALSE;
    return;
  }

  hr = ht->get_hyperlinks(aHyperlinks, aNHyperlinks);
  if (FAILED(hr)) {
    *aHyperlinks = nullptr;
    // -1 signals to the handler that it should call hyperlinks itself.
    *aNHyperlinks = -1;
  }
  // We must wrap these hyperlinks in an interceptor.
  for (long index = 0; index < *aNHyperlinks; ++index) {
    ToWrappedObject(&(*aHyperlinks)[index]);
  }

  // Fetch all attribute runs.
  nsTArray<IA2TextSegment> attribRuns;
  long end = 0;
  long length = ::SysStringLen(*aText);
  while (end < length) {
    long offset = end;
    long start;
    BSTR attribs;
    // The (exclusive) end of the last run is the start of the next run.
    hr = ht->get_attributes(offset, &start, &end, &attribs);
    // Bug 1421873: Gecko can return end <= offset in some rare cases, which
    // isn't valid. This is perhaps because the text mutated during the loop
    // for some reason, making this offset invalid.
    if (FAILED(hr) || end <= offset) {
      break;
    }
    attribRuns.AppendElement(IA2TextSegment({attribs, start, end}));
  }

  // Put the attribute runs in a COM array.
  *aNAttribRuns = attribRuns.Length();
  *aAttribRuns = static_cast<IA2TextSegment*>(
      ::CoTaskMemAlloc(sizeof(IA2TextSegment) * *aNAttribRuns));
  for (long index = 0; index < *aNAttribRuns; ++index) {
    (*aAttribRuns)[index] = attribRuns[index];
  }

  *result = S_OK;
}

HRESULT
HandlerProvider::get_AllTextInfo(BSTR* aText,
                                 IAccessibleHyperlink*** aHyperlinks,
                                 long* aNHyperlinks,
                                 IA2TextSegment** aAttribRuns,
                                 long* aNAttribRuns) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());

  if (!mTargetUnk) {
    return CO_E_OBJNOTCONNECTED;
  }

  HRESULT hr;
  if (!mscom::InvokeOnMainThread(
          "HandlerProvider::GetAllTextInfoMainThread", this,
          &HandlerProvider::GetAllTextInfoMainThread,
          std::forward<BSTR*>(aText),
          std::forward<IAccessibleHyperlink***>(aHyperlinks),
          std::forward<long*>(aNHyperlinks),
          std::forward<IA2TextSegment**>(aAttribRuns),
          std::forward<long*>(aNAttribRuns), std::forward<HRESULT*>(&hr))) {
    return E_FAIL;
  }

  return hr;
}

void HandlerProvider::GetRelationsInfoMainThread(IARelationData** aRelations,
                                                 long* aNRelations,
                                                 HRESULT* hr) {
  MOZ_ASSERT(aRelations);
  MOZ_ASSERT(aNRelations);
  MOZ_ASSERT(NS_IsMainThread());

  if (!mTargetUnk) {
    *hr = CO_E_OBJNOTCONNECTED;
    return;
  }

  RefPtr<NEWEST_IA2_INTERFACE> acc;
  *hr = mTargetUnk.get()->QueryInterface(NEWEST_IA2_IID, getter_AddRefs(acc));
  if (FAILED(*hr)) {
    return;
  }

  *hr = acc->get_nRelations(aNRelations);
  if (FAILED(*hr)) {
    return;
  }

  auto rawRels = MakeUnique<IAccessibleRelation*[]>(*aNRelations);
  *hr = acc->get_relations(*aNRelations, rawRels.get(), aNRelations);
  if (FAILED(*hr)) {
    return;
  }

  *aRelations = static_cast<IARelationData*>(
      ::CoTaskMemAlloc(sizeof(IARelationData) * *aNRelations));
  for (long index = 0; index < *aNRelations; ++index) {
    IAccessibleRelation* rawRel = rawRels[index];
    IARelationData& relData = (*aRelations)[index];
    *hr = rawRel->get_relationType(&relData.mType);
    if (FAILED(*hr)) {
      relData.mType = nullptr;
    }
    *hr = rawRel->get_nTargets(&relData.mNTargets);
    if (FAILED(*hr)) {
      relData.mNTargets = -1;
    }
    rawRel->Release();
  }

  *hr = S_OK;
}

HRESULT
HandlerProvider::get_RelationsInfo(IARelationData** aRelations,
                                   long* aNRelations) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());

  if (!mTargetUnk) {
    return CO_E_OBJNOTCONNECTED;
  }

  HRESULT hr;
  if (!mscom::InvokeOnMainThread(
          "HandlerProvider::GetRelationsInfoMainThread", this,
          &HandlerProvider::GetRelationsInfoMainThread,
          std::forward<IARelationData**>(aRelations),
          std::forward<long*>(aNRelations), std::forward<HRESULT*>(&hr))) {
    return E_FAIL;
  }

  return hr;
}

// Helper function for GetAllChildrenMainThread.
static bool SetChildDataForTextLeaf(NEWEST_IA2_INTERFACE* acc,
                                    AccChildData& data) {
  const VARIANT kChildIdSelf = {VT_I4};
  VARIANT varVal;

  // 1. Check whether this is a text leaf.

  // 1.1. A text leaf always has ROLE_SYSTEM_TEXT or ROLE_SYSTEM_WHITESPACE.
  HRESULT hr = acc->get_accRole(kChildIdSelf, &varVal);
  if (FAILED(hr)) {
    return false;
  }
  if (varVal.vt != VT_I4) {
    return false;
  }
  long role = varVal.lVal;
  if (role != ROLE_SYSTEM_TEXT && role != ROLE_SYSTEM_WHITESPACE) {
    return false;
  }

  // 1.2. A text leaf doesn't support IAccessibleText.
  RefPtr<IAccessibleText> iaText;
  hr = acc->QueryInterface(IID_IAccessibleText, getter_AddRefs(iaText));
  if (SUCCEEDED(hr)) {
    return false;
  }

  // 1.3. A text leaf doesn't have children.
  long count;
  hr = acc->get_accChildCount(&count);
  if (FAILED(hr) || count != 0) {
    return false;
  }

  // 2. Update |data| with the data for this text leaf.
  // Because marshaling objects is more expensive than marshaling other data,
  // we just marshal the data we need for text leaf children, rather than
  // marshaling the full accessible object.

  // |data| has already been zeroed, so we don't need to do anything if these
  // calls fail.
  acc->get_accName(kChildIdSelf, &data.mText);
  data.mTextRole = role;
  acc->get_uniqueID(&data.mTextId);
  acc->get_accState(kChildIdSelf, &varVal);
  data.mTextState = varVal.lVal;
  acc->accLocation(&data.mTextLeft, &data.mTextTop, &data.mTextWidth,
                   &data.mTextHeight, kChildIdSelf);

  return true;
}

void HandlerProvider::GetAllChildrenMainThread(AccChildData** aChildren,
                                               ULONG* aNChildren, HRESULT* hr) {
  MOZ_ASSERT(aChildren);
  MOZ_ASSERT(aNChildren);
  MOZ_ASSERT(NS_IsMainThread());

  if (!mTargetUnk) {
    *hr = CO_E_OBJNOTCONNECTED;
    return;
  }

  RefPtr<NEWEST_IA2_INTERFACE> acc;
  *hr = mTargetUnk.get()->QueryInterface(NEWEST_IA2_IID, getter_AddRefs(acc));
  if (FAILED(*hr)) {
    return;
  }

  long count;
  *hr = acc->get_accChildCount(&count);
  if (FAILED(*hr)) {
    return;
  }
  MOZ_ASSERT(count >= 0);

  if (count == 0) {
    *aChildren = nullptr;
    *aNChildren = 0;
    return;
  }

  RefPtr<IEnumVARIANT> enumVar;
  *hr = mTargetUnk.get()->QueryInterface(IID_IEnumVARIANT,
                                         getter_AddRefs(enumVar));
  if (FAILED(*hr)) {
    return;
  }

  auto rawChildren = MakeUnique<VARIANT[]>(count);
  *hr = enumVar->Next((ULONG)count, rawChildren.get(), aNChildren);
  if (FAILED(*hr)) {
    *aChildren = nullptr;
    *aNChildren = 0;
    return;
  }

  *aChildren = static_cast<AccChildData*>(
      ::CoTaskMemAlloc(sizeof(AccChildData) * *aNChildren));
  for (ULONG index = 0; index < *aNChildren; ++index) {
    (*aChildren)[index] = {};
    AccChildData& child = (*aChildren)[index];

    MOZ_ASSERT(rawChildren[index].vt == VT_DISPATCH);
    MOZ_ASSERT(rawChildren[index].pdispVal);
    RefPtr<NEWEST_IA2_INTERFACE> childAcc;
    *hr = rawChildren[index].pdispVal->QueryInterface(NEWEST_IA2_IID,
                                                      getter_AddRefs(childAcc));
    rawChildren[index].pdispVal->Release();
    MOZ_ASSERT(SUCCEEDED(*hr));
    if (FAILED(*hr)) {
      continue;
    }

    if (!SetChildDataForTextLeaf(childAcc, child)) {
      // This isn't a text leaf. Marshal the accessible.
      childAcc.forget(&child.mAccessible);
      // We must wrap this accessible in an Interceptor.
      ToWrappedObject(&child.mAccessible);
    }
  }

  *hr = S_OK;
}

HRESULT
HandlerProvider::get_AllChildren(AccChildData** aChildren, ULONG* aNChildren) {
  MOZ_ASSERT(mscom::IsCurrentThreadMTA());

  HRESULT hr;
  if (!mscom::InvokeOnMainThread(
          "HandlerProvider::GetAllChildrenMainThread", this,
          &HandlerProvider::GetAllChildrenMainThread,
          std::forward<AccChildData**>(aChildren),
          std::forward<ULONG*>(aNChildren), std::forward<HRESULT*>(&hr))) {
    return E_FAIL;
  }

  return hr;
}

}  // namespace a11y
}  // namespace mozilla
