/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(MOZILLA_INTERNAL_API)
#error This code is NOT for internal Gecko use!
#endif // defined(MOZILLA_INTERNAL_API)

#define INITGUID

#include "AccessibleHandler.h"
#include "AccessibleHandlerControl.h"

#include "Factory.h"
#include "HandlerData.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/a11y/HandlerDataCleanup.h"
#include "mozilla/mscom/Registration.h"
#include "mozilla/UniquePtr.h"

#include <objbase.h>
#include <uiautomation.h>
#include <winreg.h>

#include "AccessibleHypertext.h"
#include "AccessibleHypertext2.h"
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

static mscom::Factory<AccessibleHandler> sHandlerFactory;

HRESULT
AccessibleHandler::Create(IUnknown* aOuter, REFIID aIid, void** aOutInterface)
{
  if (!aOutInterface || !aOuter || aIid != IID_IUnknown) {
    return E_INVALIDARG;
  }

  *aOutInterface = nullptr;

  HRESULT hr;
  RefPtr<AccessibleHandler> handler(new AccessibleHandler(aOuter, &hr));
  if (!handler) {
    return E_OUTOFMEMORY;
  }
  if (FAILED(hr)) {
    return hr;
  }

  return handler->InternalQueryInterface(aIid, aOutInterface);
}

AccessibleHandler::AccessibleHandler(IUnknown* aOuter, HRESULT* aResult)
  : mscom::Handler(aOuter, aResult)
  , mDispatch(nullptr)
  , mIA2PassThru(nullptr)
  , mServProvPassThru(nullptr)
  , mIAHyperlinkPassThru(nullptr)
  , mIATableCellPassThru(nullptr)
  , mIAHypertextPassThru(nullptr)
  , mCachedData()
  , mCacheGen(0)
  , mCachedHyperlinks(nullptr)
  , mCachedNHyperlinks(-1)
  , mCachedTextAttribRuns(nullptr)
  , mCachedNTextAttribRuns(-1)
{
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

AccessibleHandler::~AccessibleHandler()
{
  // No need to zero memory, since we're being destroyed anyway.
  CleanupDynamicIA2Data(mCachedData.mDynamicData, false);
  if (mCachedData.mGeckoBackChannel) {
    mCachedData.mGeckoBackChannel->Release();
  }
  ClearTextCache();
}

HRESULT
AccessibleHandler::ResolveIA2()
{
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
AccessibleHandler::ResolveIAHyperlink()
{
  if (mIAHyperlinkPassThru) {
    return S_OK;
  }

  RefPtr<IUnknown> proxy(GetProxy());
  if (!proxy) {
    return E_UNEXPECTED;
  }

  HRESULT hr = proxy->QueryInterface(IID_IAccessibleHyperlink,
                                     reinterpret_cast<void**>(&mIAHyperlinkPassThru));
  if (SUCCEEDED(hr)) {
    // mIAHyperlinkPassThru is a weak reference
    // (see comments in AccesssibleHandler.h)
    mIAHyperlinkPassThru->Release();
  }

  return hr;
}

HRESULT
AccessibleHandler::ResolveIATableCell()
{
  if (mIATableCellPassThru) {
    return S_OK;
  }

  RefPtr<IUnknown> proxy(GetProxy());
  if (!proxy) {
    return E_UNEXPECTED;
  }

  HRESULT hr = proxy->QueryInterface(IID_IAccessibleTableCell,
    reinterpret_cast<void**>(&mIATableCellPassThru));
  if (SUCCEEDED(hr)) {
    // mIATableCellPassThru is a weak reference
    // (see comments in AccesssibleHandler.h)
    mIATableCellPassThru->Release();
  }

  return hr;
}

HRESULT
AccessibleHandler::ResolveIAHypertext()
{
  if (mIAHypertextPassThru) {
    return S_OK;
  }

  RefPtr<IUnknown> proxy(GetProxy());
  if (!proxy) {
    return E_UNEXPECTED;
  }

  HRESULT hr = proxy->QueryInterface(IID_IAccessibleHypertext2,
    reinterpret_cast<void**>(&mIAHypertextPassThru));
  if (SUCCEEDED(hr)) {
    // mIAHypertextPassThru is a weak reference
    // (see comments in AccessibleHandler.h)
    mIAHypertextPassThru->Release();
  }

  return hr;
}

HRESULT
AccessibleHandler::MaybeUpdateCachedData()
{
  RefPtr<AccessibleHandlerControl> ctl(gControlFactory.GetOrCreateSingleton());
  if (!ctl) {
    return E_OUTOFMEMORY;
  }

  uint32_t gen = ctl->GetCacheGen();
  if (gen == mCacheGen) {
    return S_OK;
  }

  if (!mCachedData.mGeckoBackChannel) {
    return E_POINTER;
  }

  return mCachedData.mGeckoBackChannel->Refresh(&mCachedData.mDynamicData);
}

HRESULT
AccessibleHandler::GetAllTextInfo(BSTR* aText)
{
  MOZ_ASSERT(mCachedData.mGeckoBackChannel);

  ClearTextCache();

  return mCachedData.mGeckoBackChannel->get_AllTextInfo(aText,
    &mCachedHyperlinks, &mCachedNHyperlinks,
    &mCachedTextAttribRuns, &mCachedNTextAttribRuns);
}

void
AccessibleHandler::ClearTextCache()
{
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
AccessibleHandler::ResolveIDispatch()
{
  if (mDispatch) {
    return S_OK;
  }

  HRESULT hr;

  if (!mDispatchUnk) {
    RefPtr<AccessibleHandlerControl> ctl(gControlFactory.GetOrCreateSingleton());
    if (!ctl) {
      return E_OUTOFMEMORY;
    }

    RefPtr<ITypeInfo> typeinfo;
    hr = ctl->GetHandlerTypeInfo(getter_AddRefs(typeinfo));
    if (FAILED(hr)) {
      return hr;
    }

    hr = ::CreateStdDispatch(GetOuter(), static_cast<NEWEST_IA2_INTERFACE*>(this),
                             typeinfo, getter_AddRefs(mDispatchUnk));
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
                                         void** aOutInterface)
{
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
    if ((aIid == IID_IEnumVARIANT &&
         !mCachedData.mStaticData.mIEnumVARIANT) ||
        ((aIid == IID_IAccessibleText || aIid == IID_IAccessibleHypertext ||
          aIid == IID_IAccessibleHypertext2) &&
         !mCachedData.mStaticData.mIAHypertext) ||
        ((aIid == IID_IAccessibleAction || aIid == IID_IAccessibleHyperlink) &&
         !mCachedData.mStaticData.mIAHyperlink) ||
        (aIid == IID_IAccessibleTable &&
         !mCachedData.mStaticData.mIATable) ||
        (aIid == IID_IAccessibleTable2 &&
         !mCachedData.mStaticData.mIATable2) ||
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

  return E_NOINTERFACE;
}

HRESULT
AccessibleHandler::ReadHandlerPayload(IStream* aStream, REFIID aIid)
{
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
  // No need to zero memory, since we're about to completely replace this.
  CleanupDynamicIA2Data(mCachedData.mDynamicData, false);
  mCachedData = newData;

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

  if (!mCachedData.mGeckoBackChannel) {
    return S_OK;
  }

  RefPtr<AccessibleHandlerControl> ctl(gControlFactory.GetOrCreateSingleton());
  if (!ctl) {
    return E_OUTOFMEMORY;
  }

  return ctl->Register(WrapNotNull(mCachedData.mGeckoBackChannel));
}

REFIID
AccessibleHandler::MarshalAs(REFIID aIid)
{
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
                                       NotNull<IUnknown**> aOutUnk)
{
  if (aMarshalAsIid == NEWEST_IA2_IID) {
    *aOutIid = IID_IAccessible;
  } else {
    *aOutIid = aMarshalAsIid;
  }

  return aProxy->QueryInterface(aMarshalAsIid,
      reinterpret_cast<void**>(static_cast<IUnknown**>(aOutUnk)));
}

HRESULT
AccessibleHandler::GetHandlerPayloadSize(REFIID aIid, DWORD* aOutPayloadSize)
{
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

  mSerializer = MakeUnique<mscom::StructToStream>(mCachedData, &IA2Payload_Encode);
  if (!mSerializer) {
    return E_FAIL;
  }

  *aOutPayloadSize = mSerializer->GetSize();
  return S_OK;
}

HRESULT
AccessibleHandler::WriteHandlerPayload(IStream* aStream, REFIID aIid)
{
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
AccessibleHandler::QueryInterface(REFIID riid, void** ppv)
{
  return Handler::QueryInterface(riid, ppv);
}

ULONG
AccessibleHandler::AddRef()
{
  return Handler::AddRef();
}

ULONG
AccessibleHandler::Release()
{
  return Handler::Release();
}

HRESULT
AccessibleHandler::GetTypeInfoCount(UINT *pctinfo)
{
  HRESULT hr = ResolveIDispatch();
  if (FAILED(hr)) {
    return hr;
  }

  return mDispatch->GetTypeInfoCount(pctinfo);
}

HRESULT
AccessibleHandler::GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo **ppTInfo)
{
  HRESULT hr = ResolveIDispatch();
  if (FAILED(hr)) {
    return hr;
  }

  return mDispatch->GetTypeInfo(iTInfo, lcid, ppTInfo);
}


HRESULT
AccessibleHandler::GetIDsOfNames(REFIID riid, LPOLESTR *rgszNames, UINT cNames,
                                 LCID lcid, DISPID *rgDispId)
{
  HRESULT hr = ResolveIDispatch();
  if (FAILED(hr)) {
    return hr;
  }

  return mDispatch->GetIDsOfNames(riid, rgszNames, cNames, lcid, rgDispId);
}

HRESULT
AccessibleHandler::Invoke(DISPID dispIdMember, REFIID riid, LCID lcid,
                          WORD wFlags, DISPPARAMS *pDispParams,
                          VARIANT *pVarResult, EXCEPINFO *pExcepInfo,
                          UINT *puArgErr)
{
  HRESULT hr = ResolveIDispatch();
  if (FAILED(hr)) {
    return hr;
  }

  return mDispatch->Invoke(dispIdMember, riid, lcid, wFlags, pDispParams,
                           pVarResult, pExcepInfo, puArgErr);
}

inline static BSTR
CopyBSTR(BSTR aSrc)
{
  return ::SysAllocStringLen(aSrc, ::SysStringLen(aSrc));
}

#define BEGIN_CACHE_ACCESS \
  { \
    HRESULT hr; \
    if (FAILED(hr = MaybeUpdateCachedData())) { \
      return hr; \
    } \
  }

#define GET_FIELD(member, assignTo) \
  { \
    assignTo = mCachedData.mDynamicData.member; \
  }

#define GET_BSTR(member, assignTo) \
  { \
    assignTo = CopyBSTR(mCachedData.mDynamicData.member); \
  }

/*** IAccessible ***/

HRESULT
AccessibleHandler::get_accParent(IDispatch **ppdispParent)
{
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_accParent(ppdispParent);
}

HRESULT
AccessibleHandler::get_accChildCount(long *pcountChildren)
{
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
AccessibleHandler::get_accChild(VARIANT varChild, IDispatch **ppdispChild)
{
  if (!ppdispChild) {
    return E_INVALIDARG;
  }
  // Unlikely, but we might as well optimize for it
  if (varChild.vt == VT_I4 && varChild.lVal == CHILDID_SELF) {
    RefPtr<IDispatch> disp(this);
    disp.forget(ppdispChild);
    return S_OK;
  }
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_accChild(varChild, ppdispChild);
}

HRESULT
AccessibleHandler::get_accName(VARIANT varChild, BSTR *pszName)
{
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
AccessibleHandler::get_accValue(VARIANT varChild, BSTR *pszValue)
{
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
AccessibleHandler::get_accDescription(VARIANT varChild, BSTR *pszDescription)
{
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
AccessibleHandler::get_accRole(VARIANT varChild, VARIANT *pvarRole)
{
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
AccessibleHandler::get_accState(VARIANT varChild, VARIANT *pvarState)
{
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
AccessibleHandler::get_accHelp(VARIANT varChild, BSTR *pszHelp)
{
  // This matches what AccessibleWrap does
  if (!pszHelp) {
    return E_INVALIDARG;
  }
  *pszHelp = nullptr;
  return S_FALSE;
}

HRESULT
AccessibleHandler::get_accHelpTopic(BSTR *pszHelpFile, VARIANT varChild,
                                    long *pidTopic)
{
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
                                           BSTR *pszKeyboardShortcut)
{
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
AccessibleHandler::get_accFocus(VARIANT *pvarChild)
{
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_accFocus(pvarChild);
}

HRESULT
AccessibleHandler::get_accSelection(VARIANT *pvarChildren)
{
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_accSelection(pvarChildren);
}

HRESULT
AccessibleHandler::get_accDefaultAction(VARIANT varChild,
                                        BSTR *pszDefaultAction)
{
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
AccessibleHandler::accSelect(long flagsSelect, VARIANT varChild)
{
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->accSelect(flagsSelect, varChild);
}

HRESULT
AccessibleHandler::accLocation(long *pxLeft, long *pyTop, long *pcxWidth,
                               long *pcyHeight, VARIANT varChild)
{
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
                               VARIANT *pvarEndUpAt)
{
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->accNavigate(navDir, varStart, pvarEndUpAt);
}

HRESULT
AccessibleHandler::accHitTest(long xLeft, long yTop, VARIANT *pvarChild)
{
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->accHitTest(xLeft, yTop, pvarChild);
}

HRESULT
AccessibleHandler::accDoDefaultAction(VARIANT varChild)
{
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->accDoDefaultAction(varChild);
}

HRESULT
AccessibleHandler::put_accName(VARIANT varChild, BSTR szName)
{
  // This matches AccessibleWrap
  return E_NOTIMPL;
}

HRESULT
AccessibleHandler::put_accValue(VARIANT varChild, BSTR szValue)
{
  // This matches AccessibleWrap
  return E_NOTIMPL;
}

/*** IAccessible2 ***/

HRESULT
AccessibleHandler::get_nRelations(long* nRelations)
{
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_nRelations(nRelations);
}

HRESULT
AccessibleHandler::get_relation(long relationIndex,
                                IAccessibleRelation** relation)
{
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_relation(relationIndex, relation);
}

HRESULT
AccessibleHandler::get_relations(long maxRelations,
                                 IAccessibleRelation** relations,
                                 long* nRelations)
{
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_relations(maxRelations, relations, nRelations);
}

HRESULT
AccessibleHandler::role(long* role)
{
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
AccessibleHandler::scrollTo(IA2ScrollType scrollType)
{
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->scrollTo(scrollType);
}

HRESULT
AccessibleHandler::scrollToPoint(IA2CoordinateType coordinateType, long x,
                                 long y)
{
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->scrollToPoint(coordinateType, x, y);
}

HRESULT
AccessibleHandler::get_groupPosition(long* groupLevel, long* similarItemsInGroup,
                                     long* positionInGroup)
{
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_groupPosition(groupLevel, similarItemsInGroup,
                                         positionInGroup);
}

HRESULT
AccessibleHandler::get_states(AccessibleStates* states)
{
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
AccessibleHandler::get_extendedRole(BSTR* extendedRole)
{
  // This matches ia2Accessible
  if (!extendedRole) {
    return E_INVALIDARG;
  }
  *extendedRole = nullptr;
  return E_NOTIMPL;
}

HRESULT
AccessibleHandler::get_localizedExtendedRole(BSTR* localizedExtendedRole)
{
  // This matches ia2Accessible
  if (!localizedExtendedRole) {
    return E_INVALIDARG;
  }
  *localizedExtendedRole = nullptr;
  return E_NOTIMPL;
}

HRESULT
AccessibleHandler::get_nExtendedStates(long* nExtendedStates)
{
  // This matches ia2Accessible
  if (!nExtendedStates) {
    return E_INVALIDARG;
  }
  *nExtendedStates = 0;
  return E_NOTIMPL;
}

HRESULT
AccessibleHandler::get_extendedStates(long maxExtendedStates, BSTR** extendedStates,
                                      long* nExtendedStates)
{
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
                                               long* nLocalizedExtendedStates)
{
  // This matches ia2Accessible
  if (!localizedExtendedStates || !nLocalizedExtendedStates) {
    return E_INVALIDARG;
  }
  *localizedExtendedStates = nullptr;
  *nLocalizedExtendedStates = 0;
  return E_NOTIMPL;
}

HRESULT
AccessibleHandler::get_uniqueID(long* uniqueID)
{
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
AccessibleHandler::get_windowHandle(HWND* windowHandle)
{
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
AccessibleHandler::get_indexInParent(long* indexInParent)
{
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_indexInParent(indexInParent);
}

HRESULT
AccessibleHandler::get_locale(IA2Locale* locale)
{
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
AccessibleHandler::get_attributes(BSTR* attributes)
{
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
AccessibleHandler::get_attribute(BSTR name, VARIANT* attribute)
{
  // Not yet implemented by ia2Accessible.
  // Once ia2Accessible implements this, we could either pass it through
  // or we could extract these individually from cached mAttributes.
  // The latter should be considered if traffic warrants it.
  return E_NOTIMPL;
}

HRESULT
AccessibleHandler::get_accessibleWithCaret(IUnknown** accessible,
                                           long* caretOffset)
{
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_accessibleWithCaret(accessible, caretOffset);
}

HRESULT
AccessibleHandler::get_relationTargetsOfType(BSTR type, long maxTargets,
                                             IUnknown*** targets,
                                             long* nTargets)
{
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_relationTargetsOfType(type, maxTargets, targets,
                                                 nTargets);
}

/*** IAccessible2_3 ***/

HRESULT
AccessibleHandler::get_selectionRanges(IA2Range** ranges, long* nRanges)
{
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_selectionRanges(ranges, nRanges);
}

static const GUID kUnsupportedServices[] = {
  // Unknown, queried by Windows
  {0x33f139ee, 0xe509, 0x47f7, {0xbf, 0x39, 0x83, 0x76, 0x44, 0xf7, 0x45, 0x76}},
  // Unknown, queried by Windows
  {0xFDA075CF, 0x7C8B, 0x498C, { 0xB5, 0x14, 0xA9, 0xCB, 0x52, 0x1B, 0xBF, 0xB4 }},
  // Unknown, queried by Windows
  {0x8EDAA462, 0x21F4, 0x4C87, { 0xA0, 0x12, 0xB3, 0xCD, 0xA3, 0xAB, 0x01, 0xFC }},
  // Unknown, queried by Windows
  {0xacd46652, 0x829d, 0x41cb, { 0xa5, 0xfc, 0x17, 0xac, 0xf4, 0x36, 0x61, 0xac }},
  // SID_IsUIAutomationObject (undocumented), queried by Windows
  {0xb96fdb85, 0x7204, 0x4724, { 0x84, 0x2b, 0xc7, 0x05, 0x9d, 0xed, 0xb9, 0xd0 }},
  // IIS_IsOleaccProxy (undocumented), queried by Windows
  {0x902697FA, 0x80E4, 0x4560, {0x80, 0x2A, 0xA1, 0x3F, 0x22, 0xA6, 0x47, 0x09}},
  // IID_IHTMLElement, queried by JAWS
  {0x3050F1FF, 0x98B5, 0x11CF, {0xBB, 0x82, 0x00, 0xAA, 0x00, 0xBD, 0xCE, 0x0B}}
};

/*** IServiceProvider ***/

HRESULT
AccessibleHandler::QueryService(REFGUID aServiceId, REFIID aIid,
                                void** aOutInterface)
{
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

    HRESULT hr = proxy->QueryInterface(IID_IServiceProvider,
                                       reinterpret_cast<void**>(&mServProvPassThru));
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
AccessibleHandler::GetClassInfo(ITypeInfo** aOutTypeInfo)
{
  RefPtr<AccessibleHandlerControl> ctl(gControlFactory.GetOrCreateSingleton());
  if (!ctl) {
    return E_OUTOFMEMORY;
  }

  return ctl->GetHandlerTypeInfo(aOutTypeInfo);
}

/*** IAccessibleAction ***/

HRESULT
AccessibleHandler::nActions(long* nActions)
{
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
AccessibleHandler::doAction(long actionIndex)
{
  HRESULT hr = ResolveIAHyperlink();
  if (FAILED(hr)) {
    return hr;
  }
  return mIAHyperlinkPassThru->doAction(actionIndex);
}

HRESULT
AccessibleHandler::get_description(long actionIndex, BSTR* description)
{
  HRESULT hr = ResolveIAHyperlink();
  if (FAILED(hr)) {
    return hr;
  }
  return mIAHyperlinkPassThru->get_description(actionIndex, description);
}

HRESULT
AccessibleHandler::get_keyBinding(long actionIndex,
                                  long nMaxBindings,
                                  BSTR** keyBindings,
                                  long* nBindings)
{
  HRESULT hr = ResolveIAHyperlink();
  if (FAILED(hr)) {
    return hr;
  }
  return mIAHyperlinkPassThru->get_keyBinding(
    actionIndex, nMaxBindings, keyBindings, nBindings);
}

HRESULT
AccessibleHandler::get_name(long actionIndex, BSTR* name)
{
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
AccessibleHandler::get_localizedName(long actionIndex, BSTR* localizedName)
{
  HRESULT hr = ResolveIAHyperlink();
  if (FAILED(hr)) {
    return hr;
  }
  return mIAHyperlinkPassThru->get_localizedName(actionIndex, localizedName);
}

/*** IAccessibleHyperlink ***/

HRESULT
AccessibleHandler::get_anchor(long index, VARIANT* anchor)
{
  HRESULT hr = ResolveIAHyperlink();
  if (FAILED(hr)) {
    return hr;
  }
  return mIAHyperlinkPassThru->get_anchor(index, anchor);
}

HRESULT
AccessibleHandler::get_anchorTarget(long index, VARIANT* anchorTarget)
{
  HRESULT hr = ResolveIAHyperlink();
  if (FAILED(hr)) {
    return hr;
  }
  return mIAHyperlinkPassThru->get_anchorTarget(index, anchorTarget);
}

HRESULT
AccessibleHandler::get_startIndex(long* index)
{
  HRESULT hr = ResolveIAHyperlink();
  if (FAILED(hr)) {
    return hr;
  }
  return mIAHyperlinkPassThru->get_startIndex(index);
}

HRESULT
AccessibleHandler::get_endIndex(long* index)
{
  HRESULT hr = ResolveIAHyperlink();
  if (FAILED(hr)) {
    return hr;
  }
  return mIAHyperlinkPassThru->get_endIndex(index);
}

HRESULT
AccessibleHandler::get_valid(boolean* valid)
{
  HRESULT hr = ResolveIAHyperlink();
  if (FAILED(hr)) {
    return hr;
  }
  return mIAHyperlinkPassThru->get_valid(valid);
}

/*** IAccessibleTableCell ***/

HRESULT
AccessibleHandler::get_columnExtent(long* nColumnsSpanned)
{
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
                                     long* nColumnHeaderCells)
{
  HRESULT hr = ResolveIATableCell();
  if (FAILED(hr)) {
    return hr;
  }

  return mIATableCellPassThru->get_columnHeaderCells(cellAccessibles,
             nColumnHeaderCells);
}

HRESULT
AccessibleHandler::get_columnIndex(long* columnIndex)
{
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
AccessibleHandler::get_rowExtent(long* nRowsSpanned)
{
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
                                  long* nRowHeaderCells)
{
  HRESULT hr = ResolveIATableCell();
  if (FAILED(hr)) {
    return hr;
  }

  return mIATableCellPassThru->get_rowHeaderCells(cellAccessibles,
             nRowHeaderCells);
}

HRESULT
AccessibleHandler::get_rowIndex(long* rowIndex)
{
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
AccessibleHandler::get_isSelected(boolean* isSelected)
{
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
                                     boolean* isSelected)
{
  if (!row || !column || !rowExtents || !columnExtents || !isSelected) {
    return E_INVALIDARG;
  }

  if (!HasPayload()) {
    HRESULT hr = ResolveIATableCell();
    if (FAILED(hr)) {
      return hr;
    }
    return mIATableCellPassThru->get_rowColumnExtents(row, column, rowExtents,
               columnExtents, isSelected);
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
AccessibleHandler::get_table(IUnknown** table)
{
  HRESULT hr = ResolveIATableCell();
  if (FAILED(hr)) {
    return hr;
  }

  return mIATableCellPassThru->get_table(table);
}

/*** IAccessibleText ***/

HRESULT
AccessibleHandler::addSelection(long startOffset, long endOffset)
{
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->addSelection(startOffset, endOffset);
}

HRESULT
AccessibleHandler::get_attributes(long offset, long *startOffset,
                                  long *endOffset, BSTR *textAttributes)
{
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
AccessibleHandler::get_caretOffset(long *offset)
{
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->get_caretOffset(offset);
}

HRESULT
AccessibleHandler::get_characterExtents(long offset,
                                        enum IA2CoordinateType coordType,
                                        long *x, long *y, long *width,
                                        long *height)
{
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->get_characterExtents(offset, coordType, x, y,
                                                    width, height);
}

HRESULT
AccessibleHandler::get_nSelections(long *nSelections)
{
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->get_nSelections(nSelections);
}

HRESULT
AccessibleHandler::get_offsetAtPoint(long x, long y,
                                     enum IA2CoordinateType coordType,
                                     long *offset)
{
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->get_offsetAtPoint(x, y, coordType, offset);
}

HRESULT
AccessibleHandler::get_selection(long selectionIndex, long *startOffset,
                                 long *endOffset)
{
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->get_selection(selectionIndex, startOffset,
                                             endOffset);
}

HRESULT
AccessibleHandler::get_text(long startOffset, long endOffset, BSTR *text)
{
  if (!text) {
    return E_INVALIDARG;
  }

  HRESULT hr;
  if (mCachedData.mGeckoBackChannel &&
      startOffset == 0 && endOffset == IA2_TEXT_OFFSET_LENGTH) {
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
                                        long *startOffset, long *endOffset,
                                        BSTR *text)
{
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->get_textBeforeOffset(offset, boundaryType,
                                                    startOffset, endOffset,
                                                    text);
}

HRESULT
AccessibleHandler::get_textAfterOffset(long offset,
                                       enum IA2TextBoundaryType boundaryType,
                                       long *startOffset, long *endOffset,
                                       BSTR *text)
{
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->get_textAfterOffset(offset, boundaryType,
                                                   startOffset, endOffset,
                                                   text);
}

HRESULT
AccessibleHandler::get_textAtOffset(long offset,
                                    enum IA2TextBoundaryType boundaryType,
                                    long *startOffset, long *endOffset,
                                    BSTR *text)
{
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->get_textAtOffset(offset, boundaryType,
                                                 startOffset, endOffset, text);
}

HRESULT
AccessibleHandler::removeSelection(long selectionIndex)
{
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->removeSelection(selectionIndex);
}

HRESULT
AccessibleHandler::setCaretOffset(long offset)
{
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->setCaretOffset(offset);
}

HRESULT
AccessibleHandler::setSelection(long selectionIndex, long startOffset,
                                long endOffset)
{
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->setSelection(selectionIndex, startOffset,
                                            endOffset);
}

HRESULT
AccessibleHandler::get_nCharacters(long *nCharacters)
{
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->get_nCharacters(nCharacters);
}

HRESULT
AccessibleHandler::scrollSubstringTo(long startIndex, long endIndex,
                                     enum IA2ScrollType scrollType)
{
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
                                          long x, long y)
{
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->scrollSubstringToPoint(startIndex, endIndex,
                                                      coordinateType, x, y);
}

HRESULT
AccessibleHandler::get_newText(IA2TextSegment *newText)
{
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
AccessibleHandler::get_oldText(IA2TextSegment *oldText)
{
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
AccessibleHandler::get_nHyperlinks(long *hyperlinkCount)
{
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->get_nHyperlinks(hyperlinkCount);
}

HRESULT
AccessibleHandler::get_hyperlink(long index,
                                 IAccessibleHyperlink **hyperlink)
{
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->get_hyperlink(index, hyperlink);
}

HRESULT
AccessibleHandler::get_hyperlinkIndex(long charIndex, long *hyperlinkIndex)
{
  HRESULT hr = ResolveIAHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mIAHypertextPassThru->get_hyperlinkIndex(charIndex, hyperlinkIndex);
}

/*** IAccessibleHypertext2 ***/

HRESULT
AccessibleHandler::get_hyperlinks(IAccessibleHyperlink*** hyperlinks,
                                  long* nHyperlinks)
{
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

} // namespace a11y
} // namespace mozilla

extern "C" HRESULT __stdcall
ProxyDllCanUnloadNow();

extern "C" HRESULT __stdcall
DllCanUnloadNow()
{
  return mozilla::mscom::Module::CanUnload() && ProxyDllCanUnloadNow();
}

extern "C" HRESULT __stdcall
ProxyDllGetClassObject(REFCLSID aClsid, REFIID aIid, LPVOID* aOutInterface);

extern "C" HRESULT __stdcall
DllGetClassObject(REFCLSID aClsid, REFIID aIid, LPVOID* aOutInterface)
{
  if (aClsid == CLSID_AccessibleHandler) {
    return mozilla::a11y::sHandlerFactory.QueryInterface(aIid, aOutInterface);
  }
  return ProxyDllGetClassObject(aClsid, aIid, aOutInterface);
}

extern "C" BOOL WINAPI
ProxyDllMain(HINSTANCE aInstDll, DWORD aReason, LPVOID aReserved);

BOOL WINAPI
DllMain(HINSTANCE aInstDll, DWORD aReason, LPVOID aReserved)
{
  if (aReason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls((HMODULE)aInstDll);
  }
  // This is required for ProxyDllRegisterServer to work correctly
  return ProxyDllMain(aInstDll, aReason, aReserved);
}

extern "C" HRESULT __stdcall
ProxyDllRegisterServer();

extern "C" HRESULT __stdcall
DllRegisterServer()
{
  HRESULT hr = mozilla::mscom::Handler::Register(CLSID_AccessibleHandler);
  if (FAILED(hr)) {
    return hr;
  }

  return ProxyDllRegisterServer();
}

extern "C" HRESULT __stdcall
ProxyDllUnregisterServer();

extern "C" HRESULT __stdcall
DllUnregisterServer()
{
  HRESULT hr = mozilla::mscom::Handler::Unregister(CLSID_AccessibleHandler);
  if (FAILED(hr)) {
    return hr;
  }

  return ProxyDllUnregisterServer();
}
