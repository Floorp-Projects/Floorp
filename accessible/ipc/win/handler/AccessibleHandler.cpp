/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(MOZILLA_INTERNAL_API)
#  error This code is NOT for internal Gecko use!
#endif  // defined(MOZILLA_INTERNAL_API)

#include "AccessibleHandler.h"
#include "AccessibleHandlerControl.h"
#include "HandlerChildEnumerator.h"
#include "HandlerRelation.h"

#include "Factory.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/a11y/HandlerDataCleanup.h"
#include "mozilla/mscom/Registration.h"
#include "mozilla/UniquePtr.h"

#include <objbase.h>
#include <uiautomation.h>
#include <winreg.h>

#include "AccessibleHypertext.h"
#include "AccessibleHypertext2.h"
#include "AccessibleRole.h"
#include "Accessible2_i.c"
#include "Accessible2_2_i.c"
#include "Accessible2_3_i.c"
#include "AccessibleAction_i.c"
#include "AccessibleHyperlink_i.c"
#include "AccessibleHypertext_i.c"
#include "AccessibleHypertext2_i.c"
#include "AccessibleTable_i.c"
#include "AccessibleTable2_i.c"
#include "AccessibleTableCell_i.c"
#include "AccessibleText_i.c"

namespace mozilla {
namespace a11y {

// Must be kept in sync with kClassNameTabContent in
// accessible/windows/msaa/nsWinUtils.h.
const WCHAR kEmulatedWindowClassName[] = L"MozillaContentWindowClass";
const uint32_t kEmulatedWindowClassNameNChars =
    sizeof(kEmulatedWindowClassName) / sizeof(WCHAR);
// Mask to get the content process portion of a Windows accessible unique id.
// This is bits 23 through 30 (LSB 0) of the id. This must be kept in sync
// with kNumContentProcessIDBits in accessible/windows/msaa/MsaaIdGenerator.cpp.
const uint32_t kIdContentProcessMask = 0x7F800000;

static mscom::Factory<AccessibleHandler> sHandlerFactory;

HRESULT
AccessibleHandler::Create(IUnknown* aOuter, REFIID aIid, void** aOutInterface) {
  if (!aOutInterface || !aOuter || aIid != IID_IUnknown) {
    return E_INVALIDARG;
  }

  *aOutInterface = nullptr;

  HRESULT hr;
  RefPtr<AccessibleHandler> handler(new AccessibleHandler(aOuter, &hr));
  if (FAILED(hr)) {
    return hr;
  }

  return handler->InternalQueryInterface(aIid, aOutInterface);
}

AccessibleHandler::AccessibleHandler(IUnknown* aOuter, HRESULT* aResult)
    : mscom::Handler(aOuter, aResult),
      mDispatch(nullptr),
      mIA2PassThru(nullptr),
      mServProvPassThru(nullptr),
      mIAHyperlinkPassThru(nullptr),
      mIATableCellPassThru(nullptr),
      mIAHypertextPassThru(nullptr),
      mCachedData(),
      mCachedDynamicDataMarshaledByCom(false),
      mCacheGen(0),
      mCachedHyperlinks(nullptr),
      mCachedNHyperlinks(-1),
      mCachedTextAttribRuns(nullptr),
      mCachedNTextAttribRuns(-1),
      mCachedRelations(nullptr),
      mCachedNRelations(-1),
      mIsEmulatedWindow(false) {
  RefPtr<AccessibleHandlerControl> ctl(gControlFactory.GetOrCreateSingleton());
  MOZ_ASSERT(ctl);
  if (!ctl) {
    if (aResult) {
      *aResult = E_UNEXPECTED;
    }
    return;
  }

  mCacheGen = ctl->GetCacheGen();
}

AccessibleHandler::~AccessibleHandler() {
  CleanupDynamicIA2Data(mCachedData.mDynamicData,
                        mCachedDynamicDataMarshaledByCom);
  if (mCachedData.mGeckoBackChannel) {
    mCachedData.mGeckoBackChannel->Release();
  }
  ClearTextCache();
  ClearRelationCache();
}

HRESULT
AccessibleHandler::ResolveIA2() {
  if (mIA2PassThru) {
    return S_OK;
  }

  RefPtr<IUnknown> proxy(GetProxy());
  if (!proxy) {
    return E_UNEXPECTED;
  }

  HRESULT hr = proxy->QueryInterface(NEWEST_IA2_IID,
                                     reinterpret_cast<void**>(&mIA2PassThru));
  if (SUCCEEDED(hr)) {
    // mIA2PassThru is a weak reference (see comments in AccesssibleHandler.h)
    mIA2PassThru->Release();
  }

  return hr;
}

HRESULT
AccessibleHandler::ResolveIAHyperlink() {
  if (mIAHyperlinkPassThru) {
    return S_OK;
  }

  RefPtr<IUnknown> proxy(GetProxy());
  if (!proxy) {
    return E_UNEXPECTED;
  }

  HRESULT hr =
      proxy->QueryInterface(IID_IAccessibleHyperlink,
                            reinterpret_cast<void**>(&mIAHyperlinkPassThru));
  if (SUCCEEDED(hr)) {
    // mIAHyperlinkPassThru is a weak reference
    // (see comments in AccesssibleHandler.h)
    mIAHyperlinkPassThru->Release();
  }

  return hr;
}

HRESULT
AccessibleHandler::ResolveIATableCell() {
  if (mIATableCellPassThru) {
    return S_OK;
  }

  RefPtr<IUnknown> proxy(GetProxy());
  if (!proxy) {
    return E_UNEXPECTED;
  }

  HRESULT hr =
      proxy->QueryInterface(IID_IAccessibleTableCell,
                            reinterpret_cast<void**>(&mIATableCellPassThru));
  if (SUCCEEDED(hr)) {
    // mIATableCellPassThru is a weak reference
    // (see comments in AccesssibleHandler.h)
    mIATableCellPassThru->Release();
  }

  return hr;
}

HRESULT
AccessibleHandler::ResolveIAHypertext() {
  if (mIAHypertextPassThru) {
    return S_OK;
  }

  RefPtr<IUnknown> proxy(GetProxy());
  if (!proxy) {
    return E_UNEXPECTED;
  }

  HRESULT hr =
      proxy->QueryInterface(IID_IAccessibleHypertext2,
                            reinterpret_cast<void**>(&mIAHypertextPassThru));
  if (SUCCEEDED(hr)) {
    // mIAHypertextPassThru is a weak reference
    // (see comments in AccessibleHandler.h)
    mIAHypertextPassThru->Release();
  }

  return hr;
}

HRESULT
AccessibleHandler::MaybeUpdateCachedData() {
  RefPtr<AccessibleHandlerControl> ctl(gControlFactory.GetOrCreateSingleton());
  if (!ctl) {
    return E_OUTOFMEMORY;
  }

  uint32_t gen = ctl->GetCacheGen();
  if (gen == mCacheGen) {
    // Cache is already up to date.
    return S_OK;
  }

  if (!mCachedData.mGeckoBackChannel) {
    return E_POINTER;
  }

  // While we're making the outgoing COM call below, an incoming COM call can
  // be handled which calls ReadHandlerPayload or re-enters this function.
  // Therefore, we mustn't update the cached data directly lest it be mutated
  // elsewhere before the outgoing COM call returns and cause corruption or
  // memory leaks. Instead, pass a temporary struct and update the cached data
  // only after this call completes.
  DynamicIA2Data newData;
  HRESULT hr = mCachedData.mGeckoBackChannel->Refresh(&newData);
  if (SUCCEEDED(hr)) {
    // Clean up the old data.
    CleanupDynamicIA2Data(mCachedData.mDynamicData,
                          mCachedDynamicDataMarshaledByCom);
    mCachedData.mDynamicData = newData;
    mCachedDynamicDataMarshaledByCom = true;
    // We just updated the cache, so update this object's cache generation
    // so we only update the cache again after the next change.
    mCacheGen = gen;
  }
  return hr;
}

HRESULT
AccessibleHandler::GetAllTextInfo(BSTR* aText) {
  MOZ_ASSERT(mCachedData.mGeckoBackChannel);

  ClearTextCache();

  return mCachedData.mGeckoBackChannel->get_AllTextInfo(
      aText, &mCachedHyperlinks, &mCachedNHyperlinks, &mCachedTextAttribRuns,
      &mCachedNTextAttribRuns);
}

void AccessibleHandler::ClearTextCache() {
  if (mCachedNHyperlinks >= 0) {
    // We cached hyperlinks, but the caller never retrieved them.
    for (long index = 0; index < mCachedNHyperlinks; ++index) {
      mCachedHyperlinks[index]->Release();
    }
    // mCachedHyperlinks might already be null if there are no hyperlinks.
    if (mCachedHyperlinks) {
      ::CoTaskMemFree(mCachedHyperlinks);
      mCachedHyperlinks = nullptr;
    }
    mCachedNHyperlinks = -1;
  }

  if (mCachedTextAttribRuns) {
    for (long index = 0; index < mCachedNTextAttribRuns; ++index) {
      if (mCachedTextAttribRuns[index].text) {
        // The caller never requested this attribute run.
        ::SysFreeString(mCachedTextAttribRuns[index].text);
      }
    }
    // This array is internal to us, so we must always free it.
    ::CoTaskMemFree(mCachedTextAttribRuns);
    mCachedTextAttribRuns = nullptr;
    mCachedNTextAttribRuns = -1;
  }
}

HRESULT
AccessibleHandler::GetRelationsInfo() {
  MOZ_ASSERT(mCachedData.mGeckoBackChannel);

  ClearRelationCache();

  return mCachedData.mGeckoBackChannel->get_RelationsInfo(&mCachedRelations,
                                                          &mCachedNRelations);
}

void AccessibleHandler::ClearRelationCache() {
  if (mCachedNRelations == -1) {
    // No cache; nothing to do.
    return;
  }

  // We cached relations, but the client never retrieved them.
  if (mCachedRelations) {
    for (long index = 0; index < mCachedNRelations; ++index) {
      IARelationData& relData = mCachedRelations[index];
      if (relData.mType) {
        ::SysFreeString(relData.mType);
      }
    }
    // This array is internal to us, so we must always free it.
    ::CoTaskMemFree(mCachedRelations);
    mCachedRelations = nullptr;
  }
  mCachedNRelations = -1;
}

HRESULT
AccessibleHandler::ResolveIDispatch() {
  if (mDispatch) {
    return S_OK;
  }

  HRESULT hr;

  if (!mDispatchUnk) {
    RefPtr<AccessibleHandlerControl> ctl(
        gControlFactory.GetOrCreateSingleton());
    if (!ctl) {
      return E_OUTOFMEMORY;
    }

    RefPtr<ITypeInfo> typeinfo;
    hr = ctl->GetHandlerTypeInfo(getter_AddRefs(typeinfo));
    if (FAILED(hr)) {
      return hr;
    }

    hr = ::CreateStdDispatch(GetOuter(),
                             static_cast<NEWEST_IA2_INTERFACE*>(this), typeinfo,
                             getter_AddRefs(mDispatchUnk));
    if (FAILED(hr)) {
      return hr;
    }
  }

  hr = mDispatchUnk->QueryInterface(IID_IDispatch,
                                    reinterpret_cast<void**>(&mDispatch));
  if (SUCCEEDED(hr)) {
    // mDispatch is weak (see comments in AccessibleHandler.h)
    mDispatch->Release();
  }

  return hr;
}

HRESULT
AccessibleHandler::QueryHandlerInterface(IUnknown* aProxyUnknown, REFIID aIid,
                                         void** aOutInterface) {
  MOZ_ASSERT(aProxyUnknown);

  static_assert(&NEWEST_IA2_IID == &IID_IAccessible2_3,
                "You have modified NEWEST_IA2_IID. This code needs updating.");
  if (aIid == IID_IDispatch || aIid == IID_IAccessible2_3 ||
      aIid == IID_IAccessible2_2 || aIid == IID_IAccessible2 ||
      aIid == IID_IAccessible) {
    RefPtr<NEWEST_IA2_INTERFACE> ia2(static_cast<NEWEST_IA2_INTERFACE*>(this));
    ia2.forget(aOutInterface);
    return S_OK;
  }

  if (aIid == IID_IServiceProvider) {
    RefPtr<IServiceProvider> svcProv(static_cast<IServiceProvider*>(this));
    svcProv.forget(aOutInterface);
    return S_OK;
  }

  if (HasPayload()) {
    // The proxy manager caches interfaces marshaled in the payload
    // and returns them on QI without a cross-process call.
    // However, it doesn't know about interfaces which don't exist.
    // We can determine this from the payload.
    if (((aIid == IID_IAccessibleText || aIid == IID_IAccessibleHypertext ||
          aIid == IID_IAccessibleHypertext2) &&
         !mCachedData.mStaticData.mIAHypertext) ||
        ((aIid == IID_IAccessibleAction || aIid == IID_IAccessibleHyperlink) &&
         !mCachedData.mStaticData.mIAHyperlink) ||
        (aIid == IID_IAccessibleTable && !mCachedData.mStaticData.mIATable) ||
        (aIid == IID_IAccessibleTable2 && !mCachedData.mStaticData.mIATable2) ||
        (aIid == IID_IAccessibleTableCell &&
         !mCachedData.mStaticData.mIATableCell)) {
      // We already know this interface is not available, so don't query
      // the proxy, thus avoiding a pointless cross-process call.
      // If we return E_NOINTERFACE here, mscom::Handler will try the COM
      // proxy. S_FALSE signals that the proxy should not be tried.
      return S_FALSE;
    }
  }

  if (aIid == IID_IAccessibleAction || aIid == IID_IAccessibleHyperlink) {
    RefPtr<IAccessibleHyperlink> iaLink(
        static_cast<IAccessibleHyperlink*>(this));
    iaLink.forget(aOutInterface);
    return S_OK;
  }

  if (aIid == IID_IAccessibleTableCell) {
    RefPtr<IAccessibleTableCell> iaCell(
        static_cast<IAccessibleTableCell*>(this));
    iaCell.forget(aOutInterface);
    return S_OK;
  }

  if (aIid == IID_IAccessibleText || aIid == IID_IAccessibleHypertext ||
      aIid == IID_IAccessibleHypertext2) {
    RefPtr<IAccessibleHypertext2> iaHt(
        static_cast<IAccessibleHypertext2*>(this));
    iaHt.forget(aOutInterface);
    return S_OK;
  }

  if (aIid == IID_IProvideClassInfo) {
    RefPtr<IProvideClassInfo> clsInfo(this);
    clsInfo.forget(aOutInterface);
    return S_OK;
  }

  if (aIid == IID_IEnumVARIANT && mCachedData.mGeckoBackChannel) {
    if (mCachedData.mDynamicData.mChildCount == 0) {
      return E_NOINTERFACE;
    }
    if (!mCachedData.mStaticData.mIAHypertext &&
        mCachedData.mDynamicData.mChildCount == 1) {
      // This might be an OOP iframe. (We can't check the role because it might
      // be overridden by ARIA.) HandlerChildEnumerator works fine for iframes
      // rendered in the same content process. However, for out-of-process
      // iframes, HandlerProvider::get_AllChildren (called by
      // HandlerChildEnumerator) will fail. This is because we only send down
      // an IDispatch COM proxy for the embedded document, but get_AllChildren
      // will try to QueryInterface this to IAccessible2 to reduce QI calls
      // from the parent process. Because the content process is sandboxed,
      // it can't make the outgoing COM call to QI the proxy from IDispatch to
      // IAccessible2 and so it fails.
      // Since this Accessible only has one child anyway, we don't need the bulk
      // fetch optimization offered by HandlerChildEnumerator or even
      // IEnumVARIANT. Therefore, we explicitly tell the client this interface
      // is not supported, which will cause the oleacc AccessibleChildren
      // function to fall back to accChild. If we return E_NOINTERFACE here,
      // mscom::Handler will try the COM proxy. S_FALSE signals that the proxy
      // should not be tried.
      return S_FALSE;
    }
    RefPtr<IEnumVARIANT> childEnum(
        new HandlerChildEnumerator(this, mCachedData.mGeckoBackChannel));
    childEnum.forget(aOutInterface);
    return S_OK;
  }

  return E_NOINTERFACE;
}

HRESULT
AccessibleHandler::ReadHandlerPayload(IStream* aStream, REFIID aIid) {
  if (!aStream) {
    return E_INVALIDARG;
  }

  mscom::StructFromStream deserializer(aStream);
  if (!deserializer) {
    return E_FAIL;
  }
  if (deserializer.IsEmpty()) {
    return S_FALSE;
  }

  // QueryHandlerInterface might get called while we deserialize the payload,
  // but that checks the interface pointers in the payload to determine what
  // interfaces are available. Therefore, deserialize into a temporary struct
  // and update mCachedData only after deserialization completes.
  // The decoding functions can misbehave if their target memory is not zeroed
  // beforehand, so ensure we do that.
  IA2Payload newData{};
  if (!deserializer.Read(&newData, &IA2Payload_Decode)) {
    return E_FAIL;
  }
  // Clean up the old data.
  CleanupDynamicIA2Data(mCachedData.mDynamicData,
                        mCachedDynamicDataMarshaledByCom);
  mCachedData = newData;
  mCachedDynamicDataMarshaledByCom = false;

  // These interfaces have been aggregated into the proxy manager.
  // The proxy manager will resolve these interfaces now on QI,
  // so we can release these pointers.
  // However, we don't null them out because we use their presence
  // to determine whether the interface is available
  // so as to avoid pointless cross-proc QI calls returning E_NOINTERFACE.
  // Note that if pointers to other objects (in contrast to
  // interfaces of *this* object) are added in future, we should not release
  // those pointers.
  ReleaseStaticIA2DataInterfaces(mCachedData.mStaticData);

  WCHAR className[kEmulatedWindowClassNameNChars];
  if (mCachedData.mDynamicData.mHwnd &&
      ::GetClassName(
          reinterpret_cast<HWND>(uintptr_t(mCachedData.mDynamicData.mHwnd)),
          className, kEmulatedWindowClassNameNChars) > 0 &&
      wcscmp(className, kEmulatedWindowClassName) == 0) {
    mIsEmulatedWindow = true;
  }

  if (!mCachedData.mGeckoBackChannel) {
    return S_OK;
  }

  RefPtr<AccessibleHandlerControl> ctl(gControlFactory.GetOrCreateSingleton());
  if (!ctl) {
    return E_OUTOFMEMORY;
  }

  if (mCachedData.mDynamicData.mIA2Role == ROLE_SYSTEM_COLUMNHEADER ||
      mCachedData.mDynamicData.mIA2Role == ROLE_SYSTEM_ROWHEADER) {
    // Because the same headers can apply to many cells, handler payloads
    // include the ids of header cells, rather than potentially marshaling the
    // same objects many times. We need to cache header cells here so we can
    // get them by id later.
    ctl->CacheAccessible(mCachedData.mDynamicData.mUniqueId, this);
  }

  return ctl->Register(WrapNotNull(mCachedData.mGeckoBackChannel));
}

REFIID
AccessibleHandler::MarshalAs(REFIID aIid) {
  static_assert(&NEWEST_IA2_IID == &IID_IAccessible2_3,
                "You have modified NEWEST_IA2_IID. This code needs updating.");
  if (aIid == IID_IAccessible2_3 || aIid == IID_IAccessible2_2 ||
      aIid == IID_IAccessible2 || aIid == IID_IAccessible ||
      aIid == IID_IDispatch) {
    return NEWEST_IA2_IID;
  }

  return aIid;
}

HRESULT
AccessibleHandler::GetMarshalInterface(REFIID aMarshalAsIid,
                                       NotNull<IUnknown*> aProxy,
                                       NotNull<IID*> aOutIid,
                                       NotNull<IUnknown**> aOutUnk) {
  if (aMarshalAsIid == NEWEST_IA2_IID) {
    *aOutIid = IID_IAccessible;
  } else {
    *aOutIid = aMarshalAsIid;
  }

  return aProxy->QueryInterface(
      aMarshalAsIid,
      reinterpret_cast<void**>(static_cast<IUnknown**>(aOutUnk)));
}

HRESULT
AccessibleHandler::GetHandlerPayloadSize(REFIID aIid, DWORD* aOutPayloadSize) {
  if (!aOutPayloadSize) {
    return E_INVALIDARG;
  }

  // If we're sending the payload to somebody else, we'd better make sure that
  // it is up to date. If the cache update fails then we'll return a 0 payload
  // size so that we don't transfer obsolete data.
  if (FAILED(MaybeUpdateCachedData())) {
    *aOutPayloadSize = mscom::StructToStream::GetEmptySize();
    return S_OK;
  }

  mSerializer =
      MakeUnique<mscom::StructToStream>(mCachedData, &IA2Payload_Encode);
  if (!mSerializer) {
    return E_FAIL;
  }

  *aOutPayloadSize = mSerializer->GetSize();
  return S_OK;
}

HRESULT
AccessibleHandler::WriteHandlerPayload(IStream* aStream, REFIID aIid) {
  if (!aStream) {
    return E_INVALIDARG;
  }

  if (!mSerializer) {
    return E_UNEXPECTED;
  }

  HRESULT hr = mSerializer->Write(aStream);
  mSerializer.reset();
  return hr;
}

HRESULT
AccessibleHandler::QueryInterface(REFIID riid, void** ppv) {
  return Handler::QueryInterface(riid, ppv);
}

ULONG
AccessibleHandler::AddRef() { return Handler::AddRef(); }

ULONG
AccessibleHandler::Release() { return Handler::Release(); }

HRESULT
AccessibleHandler::GetTypeInfoCount(UINT* pctinfo) {
  HRESULT hr = ResolveIDispatch();
  if (FAILED(hr)) {
    return hr;
  }

  return mDispatch->GetTypeInfoCount(pctinfo);
}

HRESULT
AccessibleHandler::GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo) {
  HRESULT hr = ResolveIDispatch();
  if (FAILED(hr)) {
    return hr;
  }

  return mDispatch->GetTypeInfo(iTInfo, lcid, ppTInfo);
}

HRESULT
AccessibleHandler::GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, UINT cNames,
                                 LCID lcid, DISPID* rgDispId) {
  HRESULT hr = ResolveIDispatch();
  if (FAILED(hr)) {
    return hr;
  }

  return mDispatch->GetIDsOfNames(riid, rgszNames, cNames, lcid, rgDispId);
}

HRESULT
AccessibleHandler::Invoke(DISPID dispIdMember, REFIID riid, LCID lcid,
                          WORD wFlags, DISPPARAMS* pDispParams,
                          VARIANT* pVarResult, EXCEPINFO* pExcepInfo,
                          UINT* puArgErr) {
  HRESULT hr = ResolveIDispatch();
  if (FAILED(hr)) {
    return hr;
  }

  return mDispatch->Invoke(dispIdMember, riid, lcid, wFlags, pDispParams,
                           pVarResult, pExcepInfo, puArgErr);
}

#define BEGIN_CACHE_ACCESS                      \
  {                                             \
    HRESULT hr;                                 \
    if (FAILED(hr = MaybeUpdateCachedData())) { \
      return hr;                                \
    }                                           \
  }

#define GET_FIELD(member, assignTo) \
  { assignTo = mCachedData.mDynamicData.member; }

#define GET_BSTR(member, assignTo) \
  { assignTo = CopyBSTR(mCachedData.mDynamicData.member); }

/*** IAccessible ***/

HRESULT
AccessibleHandler::get_accParent(IDispatch** ppdispParent) {
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_accParent(ppdispParent);
}

HRESULT
AccessibleHandler::get_accChildCount(long* pcountChildren) {
  if (!pcountChildren) {
    return E_INVALIDARG;
  }

  if (!HasPayload()) {
    HRESULT hr = ResolveIA2();
    if (FAILED(hr)) {
      return hr;
    }
    return mIA2PassThru->get_accChildCount(pcountChildren);
  }

  BEGIN_CACHE_ACCESS;
  GET_FIELD(mChildCount, *pcountChildren);
  return S_OK;
}

HRESULT
AccessibleHandler::get_accChild(VARIANT varChild, IDispatch** ppdispChild) {
  if (!ppdispChild) {
    return E_INVALIDARG;
  }
  // Unlikely, but we might as well optimize for it
  if (varChild.vt == VT_I4 && varChild.lVal == CHILDID_SELF) {
    RefPtr<IDispatch> disp(this);
    disp.forget(ppdispChild);
    return S_OK;
  }

  if (mIsEmulatedWindow && varChild.vt == VT_I4 && varChild.lVal < 0 &&
      (varChild.lVal & kIdContentProcessMask) !=
          (mCachedData.mDynamicData.mUniqueId & kIdContentProcessMask)) {
    // Window emulation is enabled and the target id is in a different
    // process to this accessible.
    // When window emulation is enabled, each tab document gets its own HWND.
    // OOP iframes get the same HWND as their tab document and fire events with
    // that HWND. However, the root accessible for the HWND (the tab document)
    // can't return accessibles for OOP iframes. Therefore, we must get the root
    // accessible from the main HWND and call accChild on that instead.
    // We don't need an oleacc proxy, so send WM_GETOBJECT directly instead of
    // calling AccessibleObjectFromEvent.
    HWND rootHwnd = GetParent(
        reinterpret_cast<HWND>(uintptr_t(mCachedData.mDynamicData.mHwnd)));
    MOZ_ASSERT(rootHwnd);
    LRESULT lresult = ::SendMessage(rootHwnd, WM_GETOBJECT, 0, OBJID_CLIENT);
    if (lresult > 0) {
      RefPtr<IAccessible2_3> rootAcc;
      HRESULT hr = ::ObjectFromLresult(lresult, IID_IAccessible2_3, 0,
                                       getter_AddRefs(rootAcc));
      if (hr == S_OK) {
        return rootAcc->get_accChild(varChild, ppdispChild);
      }
    }
  }

  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_accChild(varChild, ppdispChild);
}

HRESULT
AccessibleHandler::get_accName(VARIANT varChild, BSTR* pszName) {
  if (!pszName) {
    return E_INVALIDARG;
  }

  if (varChild.lVal != CHILDID_SELF || !HasPayload()) {
    HRESULT hr = ResolveIA2();
    if (FAILED(hr)) {
      return hr;
    }
    return mIA2PassThru->get_accName(varChild, pszName);
  }

  BEGIN_CACHE_ACCESS;
  GET_BSTR(mName, *pszName);
  return S_OK;
}

HRESULT
AccessibleHandler::get_accValue(VARIANT varChild, BSTR* pszValue) {
  if (!pszValue) {
    return E_INVALIDARG;
  }

  if (varChild.lVal != CHILDID_SELF || !HasPayload()) {
    HRESULT hr = ResolveIA2();
    if (FAILED(hr)) {
      return hr;
    }
    return mIA2PassThru->get_accValue(varChild, pszValue);
  }

  BEGIN_CACHE_ACCESS;
  GET_BSTR(mValue, *pszValue);
  return S_OK;
}

HRESULT
AccessibleHandler::get_accDescription(VARIANT varChild, BSTR* pszDescription) {
  if (!pszDescription) {
    return E_INVALIDARG;
  }

  if (varChild.lVal != CHILDID_SELF || !HasPayload()) {
    HRESULT hr = ResolveIA2();
    if (FAILED(hr)) {
      return hr;
    }
    return mIA2PassThru->get_accDescription(varChild, pszDescription);
  }

  BEGIN_CACHE_ACCESS;
  GET_BSTR(mDescription, *pszDescription);
  return S_OK;
}

HRESULT
AccessibleHandler::get_accRole(VARIANT varChild, VARIANT* pvarRole) {
  if (!pvarRole) {
    return E_INVALIDARG;
  }

  if (varChild.lVal != CHILDID_SELF || !HasPayload()) {
    HRESULT hr = ResolveIA2();
    if (FAILED(hr)) {
      return hr;
    }
    return mIA2PassThru->get_accRole(varChild, pvarRole);
  }

  BEGIN_CACHE_ACCESS;
  return ::VariantCopy(pvarRole, &mCachedData.mDynamicData.mRole);
}

HRESULT
AccessibleHandler::get_accState(VARIANT varChild, VARIANT* pvarState) {
  if (!pvarState) {
    return E_INVALIDARG;
  }

  if (varChild.lVal != CHILDID_SELF || !HasPayload()) {
    HRESULT hr = ResolveIA2();
    if (FAILED(hr)) {
      return hr;
    }
    return mIA2PassThru->get_accState(varChild, pvarState);
  }

  pvarState->vt = VT_I4;
  BEGIN_CACHE_ACCESS;
  GET_FIELD(mState, pvarState->lVal);
  return S_OK;
}

HRESULT
AccessibleHandler::get_accHelp(VARIANT varChild, BSTR* pszHelp) {
  // This matches what AccessibleWrap does
  if (!pszHelp) {
    return E_INVALIDARG;
  }
  *pszHelp = nullptr;
  return S_FALSE;
}

HRESULT
AccessibleHandler::get_accHelpTopic(BSTR* pszHelpFile, VARIANT varChild,
                                    long* pidTopic) {
  // This matches what AccessibleWrap does
  if (!pszHelpFile || !pidTopic) {
    return E_INVALIDARG;
  }
  *pszHelpFile = nullptr;
  *pidTopic = 0;
  return S_FALSE;
}

HRESULT
AccessibleHandler::get_accKeyboardShortcut(VARIANT varChild,
                                           BSTR* pszKeyboardShortcut) {
  if (!pszKeyboardShortcut) {
    return E_INVALIDARG;
  }

  if (varChild.lVal != CHILDID_SELF || !HasPayload()) {
    HRESULT hr = ResolveIA2();
    if (FAILED(hr)) {
      return hr;
    }
    return mIA2PassThru->get_accKeyboardShortcut(varChild, pszKeyboardShortcut);
  }

  BEGIN_CACHE_ACCESS;
  GET_BSTR(mKeyboardShortcut, *pszKeyboardShortcut);
  return S_OK;
}

HRESULT
AccessibleHandler::get_accFocus(VARIANT* pvarChild) {
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_accFocus(pvarChild);
}

HRESULT
AccessibleHandler::get_accSelection(VARIANT* pvarChildren) {
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_accSelection(pvarChildren);
}

HRESULT
AccessibleHandler::get_accDefaultAction(VARIANT varChild,
                                        BSTR* pszDefaultAction) {
  if (!pszDefaultAction) {
    return E_INVALIDARG;
  }

  if (varChild.lVal != CHILDID_SELF || !HasPayload()) {
    HRESULT hr = ResolveIA2();
    if (FAILED(hr)) {
      return hr;
    }
    return mIA2PassThru->get_accDefaultAction(varChild, pszDefaultAction);
  }

  BEGIN_CACHE_ACCESS;
  GET_BSTR(mDefaultAction, *pszDefaultAction);
  return S_OK;
}

HRESULT
AccessibleHandler::accSelect(long flagsSelect, VARIANT varChild) {
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->accSelect(flagsSelect, varChild);
}

HRESULT
AccessibleHandler::accLocation(long* pxLeft, long* pyTop, long* pcxWidth,
                               long* pcyHeight, VARIANT varChild) {
  if (varChild.lVal != CHILDID_SELF || !HasPayload()) {
    HRESULT hr = ResolveIA2();
    if (FAILED(hr)) {
      return hr;
    }
    return mIA2PassThru->accLocation(pxLeft, pyTop, pcxWidth, pcyHeight,
                                     varChild);
  }

  if (!pxLeft || !pyTop || !pcxWidth || !pcyHeight) {
    return E_INVALIDARG;
  }

  BEGIN_CACHE_ACCESS;
  GET_FIELD(mLeft, *pxLeft);
  GET_FIELD(mTop, *pyTop);
  GET_FIELD(mWidth, *pcxWidth);
  GET_FIELD(mHeight, *pcyHeight);
  return S_OK;
}

HRESULT
AccessibleHandler::accNavigate(long navDir, VARIANT varStart,
                               VARIANT* pvarEndUpAt) {
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->accNavigate(navDir, varStart, pvarEndUpAt);
}

HRESULT
AccessibleHandler::accHitTest(long xLeft, long yTop, VARIANT* pvarChild) {
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->accHitTest(xLeft, yTop, pvarChild);
}

HRESULT
AccessibleHandler::accDoDefaultAction(VARIANT varChild) {
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->accDoDefaultAction(varChild);
}

HRESULT
AccessibleHandler::put_accName(VARIANT varChild, BSTR szName) {
  // This matches AccessibleWrap
  return E_NOTIMPL;
}

HRESULT
AccessibleHandler::put_accValue(VARIANT varChild, BSTR szValue) {
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->put_accValue(varChild, szValue);
}

/*** IAccessible2 ***/

HRESULT
AccessibleHandler::get_nRelations(long* nRelations) {
  if (!nRelations) {
    return E_INVALIDARG;
  }

  HRESULT hr;
  if (mCachedData.mGeckoBackChannel) {
    // If the caller wants nRelations, they will almost certainly want the
    // actual relations too.
    hr = GetRelationsInfo();
    if (SUCCEEDED(hr)) {
      *nRelations = mCachedNRelations;
      return S_OK;
    }
    // We fall back to a normal call if this fails.
  }

  hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_nRelations(nRelations);
}

HRESULT
AccessibleHandler::get_relation(long relationIndex,
                                IAccessibleRelation** relation) {
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_relation(relationIndex, relation);
}

HRESULT
AccessibleHandler::get_relations(long maxRelations,
                                 IAccessibleRelation** relations,
                                 long* nRelations) {
  if (maxRelations == 0 || !relations || !nRelations) {
    return E_INVALIDARG;
  }

  // We currently only support retrieval of *all* cached relations at once.
  if (mCachedNRelations != -1 && maxRelations >= mCachedNRelations) {
    for (long index = 0; index < mCachedNRelations; ++index) {
      IARelationData& relData = mCachedRelations[index];
      RefPtr<IAccessibleRelation> hrel(new HandlerRelation(this, relData));
      hrel.forget(&relations[index]);
    }
    *nRelations = mCachedNRelations;
    // Clean up the cache, since we only cache for one call.
    // We don't use ClearRelationCache here because that scans for data to free
    // in the array and we don't we need that. The HandlerRelation instances
    // will handle freeing of the data.
    ::CoTaskMemFree(mCachedRelations);
    mCachedRelations = nullptr;
    mCachedNRelations = -1;
    return S_OK;
  }

  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_relations(maxRelations, relations, nRelations);
}

HRESULT
AccessibleHandler::role(long* role) {
  if (!role) {
    return E_INVALIDARG;
  }

  if (!HasPayload()) {
    HRESULT hr = ResolveIA2();
    if (FAILED(hr)) {
      return hr;
    }
    return mIA2PassThru->role(role);
  }

  BEGIN_CACHE_ACCESS;
  GET_FIELD(mIA2Role, *role);
  return S_OK;
}

HRESULT
AccessibleHandler::scrollTo(IA2ScrollType scrollType) {
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->scrollTo(scrollType);
}

HRESULT
AccessibleHandler::scrollToPoint(IA2CoordinateType coordinateType, long x,
                                 long y) {
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->scrollToPoint(coordinateType, x, y);
}

HRESULT
AccessibleHandler::get_groupPosition(long* groupLevel,
                                     long* similarItemsInGroup,
                                     long* positionInGroup) {
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_groupPosition(groupLevel, similarItemsInGroup,
                                         positionInGroup);
}

HRESULT
AccessibleHandler::get_states(AccessibleStates* states) {
  if (!states) {
    return E_INVALIDARG;
  }

  if (!HasPayload()) {
    HRESULT hr = ResolveIA2();
    if (FAILED(hr)) {
      return hr;
    }
    return mIA2PassThru->get_states(states);
  }

  BEGIN_CACHE_ACCESS;
  GET_FIELD(mIA2States, *states);
  return S_OK;
}

HRESULT
AccessibleHandler::get_extendedRole(BSTR* extendedRole) {
  // This matches ia2Accessible
  if (!extendedRole) {
    return E_INVALIDARG;
  }
  *extendedRole = nullptr;
  return E_NOTIMPL;
}

HRESULT
AccessibleHandler::get_localizedExtendedRole(BSTR* localizedExtendedRole) {
  // This matches ia2Accessible
  if (!localizedExtendedRole) {
    return E_INVALIDARG;
  }
  *localizedExtendedRole = nullptr;
  return E_NOTIMPL;
}

HRESULT
AccessibleHandler::get_nExtendedStates(long* nExtendedStates) {
  // This matches ia2Accessible
  if (!nExtendedStates) {
    return E_INVALIDARG;
  }
  *nExtendedStates = 0;
  return E_NOTIMPL;
}

HRESULT
AccessibleHandler::get_extendedStates(long maxExtendedStates,
                                      BSTR** extendedStates,
                                      long* nExtendedStates) {
  // This matches ia2Accessible
  if (!extendedStates || !nExtendedStates) {
    return E_INVALIDARG;
  }
  *extendedStates = nullptr;
  *nExtendedStates = 0;
  return E_NOTIMPL;
}

HRESULT
AccessibleHandler::get_localizedExtendedStates(long maxLocalizedExtendedStates,
                                               BSTR** localizedExtendedStates,
                                               long* nLocalizedExtendedStates) {
  // This matches ia2Accessible
  if (!localizedExtendedStates || !nLocalizedExtendedStates) {
    return E_INVALIDARG;
  }
  *localizedExtendedStates = nullptr;
  *nLocalizedExtendedStates = 0;
  return E_NOTIMPL;
}

HRESULT
AccessibleHandler::get_uniqueID(long* uniqueID) {
  if (!uniqueID) {
    return E_INVALIDARG;
  }
  if (!HasPayload()) {
    HRESULT hr = ResolveIA2();
    if (FAILED(hr)) {
      return hr;
    }
    return mIA2PassThru->get_uniqueID(uniqueID);
  }
  *uniqueID = mCachedData.mDynamicData.mUniqueId;
  return S_OK;
}

HRESULT
AccessibleHandler::get_windowHandle(HWND* windowHandle) {
  if (!windowHandle) {
    return E_INVALIDARG;
  }

  if (!HasPayload()) {
    HRESULT hr = ResolveIA2();
    if (FAILED(hr)) {
      return hr;
    }
    return mIA2PassThru->get_windowHandle(windowHandle);
  }

  BEGIN_CACHE_ACCESS;
  long hwnd = 0;
  GET_FIELD(mHwnd, hwnd);
  *windowHandle = reinterpret_cast<HWND>(uintptr_t(hwnd));
  return S_OK;
}

HRESULT
AccessibleHandler::get_indexInParent(long* indexInParent) {
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_indexInParent(indexInParent);
}

HRESULT
AccessibleHandler::get_locale(IA2Locale* locale) {
  if (!locale) {
    return E_INVALIDARG;
  }

  if (!HasPayload()) {
    HRESULT hr = ResolveIA2();
    if (FAILED(hr)) {
      return hr;
    }
    return mIA2PassThru->get_locale(locale);
  }

  BEGIN_CACHE_ACCESS;
  GET_BSTR(mIA2Locale.language, locale->language);
  GET_BSTR(mIA2Locale.country, locale->country);
  GET_BSTR(mIA2Locale.variant, locale->variant);
  return S_OK;
}

HRESULT
AccessibleHandler::get_attributes(BSTR* attributes) {
  if (!attributes) {
    return E_INVALIDARG;
  }

  if (!HasPayload()) {
    HRESULT hr = ResolveIA2();
    if (FAILED(hr)) {
      return hr;
    }
    return mIA2PassThru->get_attributes(attributes);
  }

  BEGIN_CACHE_ACCESS;
  GET_BSTR(mAttributes, *attributes);
  return S_OK;
}

/*** IAccessible2_2 ***/

HRESULT
AccessibleHandler::get_attribute(BSTR name, VARIANT* attribute) {
  // Not yet implemented by ia2Accessible.
  // Once ia2Accessible implements this, we could either pass it through
  // or we could extract these individually from cached mAttributes.
  // The latter should be considered if traffic warrants it.
  return E_NOTIMPL;
}

HRESULT
AccessibleHandler::get_accessibleWithCaret(IUnknown** accessible,
                                           long* caretOffset) {
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_accessibleWithCaret(accessible, caretOffset);
}

HRESULT
AccessibleHandler::get_relationTargetsOfType(BSTR type, long maxTargets,
                                             IUnknown*** targets,
                                             long* nTargets) {
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_relationTargetsOfType(type, maxTargets, targets,
                                                 nTargets);
}

/*** IAccessible2_3 ***/

HRESULT
AccessibleHandler::get_selectionRanges(IA2Range** ranges, long* nRanges) {
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_selectionRanges(ranges, nRanges);
}

/*** IServiceProvider ***/

HRESULT
AccessibleHandler::QueryService(REFGUID aServiceId, REFIID aIid,
                                void** aOutInterface) {
  static_assert(&NEWEST_IA2_IID == &IID_IAccessible2_3,
                "You have modified NEWEST_IA2_IID. This code needs updating.");
  /* We're taking advantage of the fact that we are implementing IA2 as part
     of our own object to implement this just like a QI. */
  if (aIid == IID_IAccessible2_3 || aIid == IID_IAccessible2_2 ||
      aIid == IID_IAccessible2) {
    RefPtr<NEWEST_IA2_INTERFACE> ia2(this);
    ia2.forget(aOutInterface);
    return S_OK;
  }

  // JAWS uses QueryService for these, but QI will work just fine and we can
  // thus avoid a cross-process call. More importantly, if QS is used, the
  // handler won't get used for that object, so our caching won't be used.
  if (aIid == IID_IAccessibleAction || aIid == IID_IAccessibleText) {
    return InternalQueryInterface(aIid, aOutInterface);
  }

  for (uint32_t i = 0; i < ArrayLength(kUnsupportedServices); ++i) {
    if (aServiceId == kUnsupportedServices[i]) {
      return E_NOINTERFACE;
    }
  }

  if (!mServProvPassThru) {
    RefPtr<IUnknown> proxy(GetProxy());
    if (!proxy) {
      return E_UNEXPECTED;
    }

    HRESULT hr = proxy->QueryInterface(
        IID_IServiceProvider, reinterpret_cast<void**>(&mServProvPassThru));
    if (FAILED(hr)) {
      return hr;
    }

    // mServProvPassThru is a weak reference (see comments in
    // AccessibleHandler.h)
    mServProvPassThru->Release();
  }

  return mServProvPassThru->QueryService(aServiceId, aIid, aOutInterface);
}

/*** IProvideClassInfo ***/

HRESULT
AccessibleHandler::GetClassInfo(ITypeInfo** aOutTypeInfo) {
  RefPtr<AccessibleHandlerControl> ctl(gControlFactory.GetOrCreateSingleton());
  if (!ctl) {
    return E_OUTOFMEMORY;
  }

  return ctl->GetHandlerTypeInfo(aOutTypeInfo);
}

/*** IAccessibleAction ***/

HRESULT
AccessibleHandler::nActions(long* nActions) {
  if (!nActions) {
    return E_INVALIDARG;
  }

  if (!HasPayload()) {
    HRESULT hr = ResolveIAHyperlink();
    if (FAILED(hr)) {
      return hr;
    }
    return mIAHyperlinkPassThru->nActions(nActions);
  }

  BEGIN_CACHE_ACCESS;
  GET_FIELD(mNActions, *nActions);
  return S_OK;
}

HRESULT
AccessibleHandler::doAction(long actionIndex) {
  HRESULT hr = ResolveIAHyperlink();
  if (FAILED(hr)) {
    return hr;
  }
  return mIAHyperlinkPassThru->doAction(actionIndex);
}

HRESULT
AccessibleHandler::get_description(long actionIndex, BSTR* description) {
  HRESULT hr = ResolveIAHyperlink();
  if (FAILED(hr)) {
    return hr;
  }
  return mIAHyperlinkPassThru->get_description(actionIndex, description);
}

HRESULT
AccessibleHandler::get_keyBinding(long actionIndex, long nMaxBindings,
                                  BSTR** keyBindings, long* nBindings) {
  HRESULT hr = ResolveIAHyperlink();
  if (FAILED(hr)) {
    return hr;
  }
  return mIAHyperlinkPassThru->get_keyBinding(actionIndex, nMaxBindings,
                                              keyBindings, nBindings);
}

HRESULT
AccessibleHandler::get_name(long actionIndex, BSTR* name) {
  if (!name) {
    return E_INVALIDARG;
  }

  if (HasPayload()) {
    if (actionIndex >= mCachedData.mDynamicData.mNActions) {
      // Action does not exist.
      return E_INVALIDARG;
    }

    if (actionIndex == 0) {
      // same as accDefaultAction.
      GET_BSTR(mDefaultAction, *name);
      return S_OK;
    }
  }

  // At this point, there's either no payload or actionIndex is > 0.
  HRESULT hr = ResolveIAHyperlink();
  if (FAILED(hr)) {
    return hr;
  }
  return mIAHyperlinkPassThru->get_name(actionIndex, name);
}

HRESULT
AccessibleHandler::get_localizedName(long actionIndex, BSTR* localizedName) {
  HRESULT hr = ResolveIAHyperlink();
  if (FAILED(hr)) {
    return hr;
  }
  return mIAHyperlinkPassThru->get_localizedName(actionIndex, localizedName);
}

/*** IAccessibleHyperlink ***/

HRESULT
AccessibleHandler::get_anchor(long index, VARIANT* anchor) {
  HRESULT hr = ResolveIAHyperlink();
  if (FAILED(hr)) {
    return hr;
  }
  return mIAHyperlinkPassThru->get_anchor(index, anchor);
}

HRESULT
AccessibleHandler::get_anchorTarget(long index, VARIANT* anchorTarget) {
  HRESULT hr = ResolveIAHyperlink();
  if (FAILED(hr)) {
    return hr;
  }
  return mIAHyperlinkPassThru->get_anchorTarget(index, anchorTarget);
}

HRESULT
AccessibleHandler::get_startIndex(long* index) {
  HRESULT hr = ResolveIAHyperlink();
  if (FAILED(hr)) {
    return hr;
  }
  return mIAHyperlinkPassThru->get_startIndex(index);
}

HRESULT
AccessibleHandler::get_endIndex(long* index) {
  HRESULT hr = ResolveIAHyperlink();
  if (FAILED(hr)) {
    return hr;
  }
  return mIAHyperlinkPassThru->get_endIndex(index);
}

HRESULT
AccessibleHandler::get_valid(boolean* valid) {
  HRESULT hr = ResolveIAHyperlink();
  if (FAILED(hr)) {
    return hr;
  }
  return mIAHyperlinkPassThru->get_valid(valid);
}

/*** IAccessibleTableCell ***/

HRESULT
AccessibleHandler::get_columnExtent(long* nColumnsSpanned) {
  if (!nColumnsSpanned) {
    return E_INVALIDARG;
  }

  if (!HasPayload()) {
    HRESULT hr = ResolveIATableCell();
    if (FAILED(hr)) {
      return hr;
    }
    return mIATableCellPassThru->get_columnExtent(nColumnsSpanned);
  }

  BEGIN_CACHE_ACCESS;
  GET_FIELD(mColumnExtent, *nColumnsSpanned);
  return S_OK;
}

HRESULT
AccessibleHandler::get_columnHeaderCells(IUnknown*** cellAccessibles,
                                         long* nColumnHeaderCells) {
  if (!cellAccessibles || !nColumnHeaderCells) {
    return E_INVALIDARG;
  }

  HRESULT hr = S_OK;
  if (HasPayload()) {
    RefPtr<AccessibleHandlerControl> ctl(
        gControlFactory.GetOrCreateSingleton());
    if (!ctl) {
      return E_OUTOFMEMORY;
    }
    *nColumnHeaderCells = mCachedData.mDynamicData.mNColumnHeaderCells;
    *cellAccessibles = static_cast<IUnknown**>(
        ::CoTaskMemAlloc(sizeof(IUnknown*) * *nColumnHeaderCells));
    long i;
    for (i = 0; i < *nColumnHeaderCells; ++i) {
      RefPtr<AccessibleHandler> headerAcc;
      hr = ctl->GetCachedAccessible(
          mCachedData.mDynamicData.mColumnHeaderCellIds[i],
          getter_AddRefs(headerAcc));
      if (FAILED(hr)) {
        break;
      }
      hr = headerAcc->QueryInterface(IID_IUnknown,
                                     (void**)&(*cellAccessibles)[i]);
      if (FAILED(hr)) {
        break;
      }
    }
    if (SUCCEEDED(hr)) {
      return S_OK;
    }
    // If we failed to get any of the headers from the cache, don't use the
    // cache at all. We need to clean up anything we did so far.
    long failedHeader = i;
    for (i = 0; i < failedHeader; ++i) {
      (*cellAccessibles)[i]->Release();
    }
    ::CoTaskMemFree(*cellAccessibles);
    *cellAccessibles = nullptr;
  }

  hr = ResolveIATableCell();
  if (FAILED(hr)) {
    return hr;
  }

  return mIATableCellPassThru->get_columnHeaderCells(cellAccessibles,
                                                     nColumnHeaderCells);
}

HRESULT
AccessibleHandler::get_columnIndex(long* columnIndex) {
  if (!columnIndex) {
    return E_INVALIDARG;
  }

  if (!HasPayload()) {
    HRESULT hr = ResolveIATableCell();
    if (FAILED(hr)) {
      return hr;
    }
    return mIATableCellPassThru->get_columnIndex(columnIndex);
  }

  BEGIN_CACHE_ACCESS;
  GET_FIELD(mColumnIndex, *columnIndex);
  return S_OK;
}

HRESULT
AccessibleHandler::get_rowExtent(long* nRowsSpanned) {
  if (!nRowsSpanned) {
    return E_INVALIDARG;
  }

  if (!HasPayload()) {
    HRESULT hr = ResolveIATableCell();
    if (FAILED(hr)) {
      return hr;
    }
    return mIATableCellPassThru->get_rowExtent(nRowsSpanned);
  }

  BEGIN_CACHE_ACCESS;
  GET_FIELD(mRowExtent, *nRowsSpanned);
  return S_OK;
}

HRESULT
AccessibleHandler::get_rowHeaderCells(IUnknown*** cellAccessibles,
                                      long* nRowHeaderCells) {
  if (!cellAccessibles || !nRowHeaderCells) {
    return E_INVALIDARG;
  }

  HRESULT hr = S_OK;
  if (HasPayload()) {
    RefPtr<AccessibleHandlerControl> ctl(
        gControlFactory.GetOrCreateSingleton());
    if (!ctl) {
      return E_OUTOFMEMORY;
    }
    *nRowHeaderCells = mCachedData.mDynamicData.mNRowHeaderCells;
    *cellAccessibles = static_cast<IUnknown**>(
        ::CoTaskMemAlloc(sizeof(IUnknown*) * *nRowHeaderCells));
    long i;
    for (i = 0; i < *nRowHeaderCells; ++i) {
      RefPtr<AccessibleHandler> headerAcc;
      hr = ctl->GetCachedAccessible(
          mCachedData.mDynamicData.mRowHeaderCellIds[i],
          getter_AddRefs(headerAcc));
      if (FAILED(hr)) {
        break;
      }
      hr = headerAcc->QueryInterface(IID_IUnknown,
                                     (void**)&(*cellAccessibles)[i]);
      if (FAILED(hr)) {
        break;
      }
    }
    if (SUCCEEDED(hr)) {
      return S_OK;
    }
    // If we failed to get any of the headers from the cache, don't use the
    // cache at all. We need to clean up anything we did so far.
    long failedHeader = i;
    for (i = 0; i < failedHeader; ++i) {
      (*cellAccessibles)[i]->Release();
    }
    ::CoTaskMemFree(*cellAccessibles);
    *cellAccessibles = nullptr;
  }

  hr = ResolveIATableCell();
  if (FAILED(hr)) {
    return hr;
  }

  return mIATableCellPassThru->get_rowHeaderCells(cellAccessibles,
                                                  nRowHeaderCells);
}

HRESULT
AccessibleHandler::get_rowIndex(long* rowIndex) {
  if (!rowIndex) {
    return E_INVALIDARG;
  }

  if (!HasPayload()) {
    HRESULT hr = ResolveIATableCell();
    if (FAILED(hr)) {
      return hr;
    }
    return mIATableCellPassThru->get_rowIndex(rowIndex);
  }

  BEGIN_CACHE_ACCESS;
  GET_FIELD(mRowIndex, *rowIndex);
  return S_OK;
}

HRESULT
AccessibleHandler::get_isSelected(boolean* isSelected) {
  if (!isSelected) {
    return E_INVALIDARG;
  }

  if (!HasPayload()) {
    HRESULT hr = ResolveIATableCell();
    if (FAILED(hr)) {
      return hr;
    }
    return mIATableCellPassThru->get_isSelected(isSelected);
  }

  BEGIN_CACHE_ACCESS;
  GET_FIELD(mCellIsSelected, *isSelected);
  return S_OK;
}

HRESULT
AccessibleHandler::get_rowColumnExtents(long* row, long* column,
                                        long* rowExtents, long* columnExtents,
                                        boolean* isSelected) {
  if (!row || !column || !rowExtents || !columnExtents || !isSelected) {
    return E_INVALIDARG;
  }

  if (!HasPayload()) {
    HRESULT hr = ResolveIATableCell();
    if (FAILED(hr)) {
      return hr;
    }
    return mIATableCellPassThru->get_rowColumnExtents(
        row, column, rowExtents, columnExtents, isSelected);
  }

  BEGIN_CACHE_ACCESS;
  GET_FIELD(mRowIndex, *row);
  GET_FIELD(mColumnIndex, *column);
  GET_FIELD(mRowExtent, *rowExtents);
  GET_FIELD(mColumnExtent, *columnExtents);
  GET_FIELD(mCellIsSelected, *isSelected);
  return S_OK;
}

HRESULT
AccessibleHandler::get_table(IUnknown** table) {
  HRESULT hr = ResolveIATableCell();
  if (FAILED(hr)) {
    return hr;
  }

  return mIATableCellPassThru->get_table(table);
}

/*** IAccessibleText ***/

HRESULT
AccessibleHandler::addSelection(long startOffset, long endOffset) {
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->addSelection(startOffset, endOffset);
}

HRESULT
AccessibleHandler::get_attributes(long offset, long* startOffset,
                                  long* endOffset, BSTR* textAttributes) {
  if (!startOffset || !endOffset || !textAttributes) {
    return E_INVALIDARG;
  }

  if (mCachedNTextAttribRuns >= 0) {
    // We have cached attributes.
    for (long index = 0; index < mCachedNTextAttribRuns; ++index) {
      auto& attribRun = mCachedTextAttribRuns[index];
      if (attribRun.start <= offset && offset < attribRun.end) {
        *startOffset = attribRun.start;
        *endOffset = attribRun.end;
        *textAttributes = attribRun.text;
        // The caller will clean this up.
        // (We only keep each cached attribute run for one call.)
        attribRun.text = nullptr;
        // The cache for this run is now invalid, so don't visit it again.
        attribRun.end = 0;
        return S_OK;
      }
    }
  }

  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->get_attributes(offset, startOffset, endOffset,
                                              textAttributes);
}

HRESULT
AccessibleHandler::get_caretOffset(long* offset) {
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->get_caretOffset(offset);
}

HRESULT
AccessibleHandler::get_characterExtents(long offset,
                                        enum IA2CoordinateType coordType,
                                        long* x, long* y, long* width,
                                        long* height) {
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->get_characterExtents(offset, coordType, x, y,
                                                    width, height);
}

HRESULT
AccessibleHandler::get_nSelections(long* nSelections) {
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->get_nSelections(nSelections);
}

HRESULT
AccessibleHandler::get_offsetAtPoint(long x, long y,
                                     enum IA2CoordinateType coordType,
                                     long* offset) {
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->get_offsetAtPoint(x, y, coordType, offset);
}

HRESULT
AccessibleHandler::get_selection(long selectionIndex, long* startOffset,
                                 long* endOffset) {
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->get_selection(selectionIndex, startOffset,
                                             endOffset);
}

HRESULT
AccessibleHandler::get_text(long startOffset, long endOffset, BSTR* text) {
  if (!text) {
    return E_INVALIDARG;
  }

  HRESULT hr;
  if (mCachedData.mGeckoBackChannel && startOffset == 0 &&
      endOffset == IA2_TEXT_OFFSET_LENGTH) {
    // If the caller is retrieving all text, they will probably want all
    // hyperlinks and attributes as well.
    hr = GetAllTextInfo(text);
    if (SUCCEEDED(hr)) {
      return hr;
    }
    // We fall back to a normal call if this fails.
  }

  hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->get_text(startOffset, endOffset, text);
}

HRESULT
AccessibleHandler::get_textBeforeOffset(long offset,
                                        enum IA2TextBoundaryType boundaryType,
                                        long* startOffset, long* endOffset,
                                        BSTR* text) {
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->get_textBeforeOffset(
      offset, boundaryType, startOffset, endOffset, text);
}

HRESULT
AccessibleHandler::get_textAfterOffset(long offset,
                                       enum IA2TextBoundaryType boundaryType,
                                       long* startOffset, long* endOffset,
                                       BSTR* text) {
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->get_textAfterOffset(
      offset, boundaryType, startOffset, endOffset, text);
}

HRESULT
AccessibleHandler::get_textAtOffset(long offset,
                                    enum IA2TextBoundaryType boundaryType,
                                    long* startOffset, long* endOffset,
                                    BSTR* text) {
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->get_textAtOffset(offset, boundaryType,
                                                startOffset, endOffset, text);
}

HRESULT
AccessibleHandler::removeSelection(long selectionIndex) {
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->removeSelection(selectionIndex);
}

HRESULT
AccessibleHandler::setCaretOffset(long offset) {
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->setCaretOffset(offset);
}

HRESULT
AccessibleHandler::setSelection(long selectionIndex, long startOffset,
                                long endOffset) {
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->setSelection(selectionIndex, startOffset,
                                            endOffset);
}

HRESULT
AccessibleHandler::get_nCharacters(long* nCharacters) {
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->get_nCharacters(nCharacters);
}

HRESULT
AccessibleHandler::scrollSubstringTo(long startIndex, long endIndex,
                                     enum IA2ScrollType scrollType) {
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->scrollSubstringTo(startIndex, endIndex,
                                                 scrollType);
}

HRESULT
AccessibleHandler::scrollSubstringToPoint(long startIndex, long endIndex,
                                          enum IA2CoordinateType coordinateType,
                                          long x, long y) {
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->scrollSubstringToPoint(startIndex, endIndex,
                                                      coordinateType, x, y);
}

HRESULT
AccessibleHandler::get_newText(IA2TextSegment* newText) {
  if (!newText) {
    return E_INVALIDARG;
  }

  RefPtr<AccessibleHandlerControl> ctl(gControlFactory.GetSingleton());
  MOZ_ASSERT(ctl);
  if (!ctl) {
    return S_OK;
  }

  long id;
  HRESULT hr = this->get_uniqueID(&id);
  if (FAILED(hr)) {
    return hr;
  }

  return ctl->GetNewText(id, WrapNotNull(newText));
}

HRESULT
AccessibleHandler::get_oldText(IA2TextSegment* oldText) {
  if (!oldText) {
    return E_INVALIDARG;
  }

  RefPtr<AccessibleHandlerControl> ctl(gControlFactory.GetSingleton());
  MOZ_ASSERT(ctl);
  if (!ctl) {
    return S_OK;
  }

  long id;
  HRESULT hr = this->get_uniqueID(&id);
  if (FAILED(hr)) {
    return hr;
  }

  return ctl->GetOldText(id, WrapNotNull(oldText));
}

/*** IAccessibleHypertext ***/

HRESULT
AccessibleHandler::get_nHyperlinks(long* hyperlinkCount) {
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->get_nHyperlinks(hyperlinkCount);
}

HRESULT
AccessibleHandler::get_hyperlink(long index, IAccessibleHyperlink** hyperlink) {
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->get_hyperlink(index, hyperlink);
}

HRESULT
AccessibleHandler::get_hyperlinkIndex(long charIndex, long* hyperlinkIndex) {
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->get_hyperlinkIndex(charIndex, hyperlinkIndex);
}

/*** IAccessibleHypertext2 ***/

HRESULT
AccessibleHandler::get_hyperlinks(IAccessibleHyperlink*** hyperlinks,
                                  long* nHyperlinks) {
  if (!hyperlinks || !nHyperlinks) {
    return E_INVALIDARG;
  }

  if (mCachedNHyperlinks >= 0) {
    // We have cached hyperlinks.
    *hyperlinks = mCachedHyperlinks;
    *nHyperlinks = mCachedNHyperlinks;
    // The client will clean these up. (We only keep the cache for one call.)
    mCachedHyperlinks = nullptr;
    mCachedNHyperlinks = -1;
    return mCachedNHyperlinks == 0 ? S_FALSE : S_OK;
  }

  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->get_hyperlinks(hyperlinks, nHyperlinks);
}

}  // namespace a11y
}  // namespace mozilla

extern "C" HRESULT __stdcall ProxyDllCanUnloadNow();

extern "C" HRESULT __stdcall DllCanUnloadNow() {
  return mozilla::mscom::Module::CanUnload() && ProxyDllCanUnloadNow();
}

extern "C" HRESULT __stdcall ProxyDllGetClassObject(REFCLSID aClsid,
                                                    REFIID aIid,
                                                    LPVOID* aOutInterface);

extern "C" HRESULT __stdcall DllGetClassObject(REFCLSID aClsid, REFIID aIid,
                                               LPVOID* aOutInterface) {
  if (aClsid == CLSID_AccessibleHandler) {
    return mozilla::a11y::sHandlerFactory.QueryInterface(aIid, aOutInterface);
  }
  return ProxyDllGetClassObject(aClsid, aIid, aOutInterface);
}

extern "C" BOOL WINAPI ProxyDllMain(HINSTANCE aInstDll, DWORD aReason,
                                    LPVOID aReserved);

BOOL WINAPI DllMain(HINSTANCE aInstDll, DWORD aReason, LPVOID aReserved) {
  if (aReason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls((HMODULE)aInstDll);
  }
  // This is required for ProxyDllRegisterServer to work correctly
  return ProxyDllMain(aInstDll, aReason, aReserved);
}

extern "C" HRESULT __stdcall ProxyDllRegisterServer();

extern "C" HRESULT __stdcall DllRegisterServer() {
  HRESULT hr = mozilla::mscom::Handler::Register(CLSID_AccessibleHandler);
  if (FAILED(hr)) {
    return hr;
  }

  return ProxyDllRegisterServer();
}

extern "C" HRESULT __stdcall ProxyDllUnregisterServer();

extern "C" HRESULT __stdcall DllUnregisterServer() {
  HRESULT hr = mozilla::mscom::Handler::Unregister(CLSID_AccessibleHandler);
  if (FAILED(hr)) {
    return hr;
  }

  return ProxyDllUnregisterServer();
}

extern "C" HRESULT __stdcall RegisterMsix() {
  return mozilla::mscom::Handler::Register(CLSID_AccessibleHandler,
                                           /* aMsixContainer */ true);
}
