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
#include "AccessibleTextTearoff.h"

#include "Factory.h"
#include "HandlerData.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/mscom/Registration.h"
#include "mozilla/UniquePtr.h"

#include <objbase.h>
#include <uiautomation.h>
#include <winreg.h>

#include "AccessibleHypertext.h"
#include "Accessible2_i.c"
#include "Accessible2_2_i.c"
#include "Accessible2_3_i.c"

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
  , mCachedData()
  , mCacheGen(0)
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
  if (mCachedData.mGeckoBackChannel) {
    mCachedData.mGeckoBackChannel->Release();
  }
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

  return mCachedData.mGeckoBackChannel->Refresh(&mCachedData.mData);
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

  if (aIid == IID_IAccessibleText || aIid == IID_IAccessibleHypertext) {
    RefPtr<IAccessibleHypertext> textTearoff(new AccessibleTextTearoff(this));
    textTearoff.forget(aOutInterface);
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

  if (!deserializer.Read(&mCachedData, &IA2Payload_Decode)) {
    return E_FAIL;
  }

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

#define BEGIN_CACHE_ACCESS \
  { \
    HRESULT hr; \
    if (FAILED(hr = MaybeUpdateCachedData())) { \
      return hr; \
    } \
  }

static BSTR
CopyBSTR(BSTR aSrc)
{
  return SysAllocStringLen(aSrc, SysStringLen(aSrc));
}

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
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_accChildCount(pcountChildren);
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
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_accName(varChild, pszName);
}

HRESULT
AccessibleHandler::get_accValue(VARIANT varChild, BSTR *pszValue)
{
  if (!pszValue) {
    return E_INVALIDARG;
  }
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_accValue(varChild, pszValue);
}

HRESULT
AccessibleHandler::get_accDescription(VARIANT varChild, BSTR *pszDescription)
{
  if (!pszDescription) {
    return E_INVALIDARG;
  }
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_accDescription(varChild, pszDescription);
}


HRESULT
AccessibleHandler::get_accRole(VARIANT varChild, VARIANT *pvarRole)
{
  if (!pvarRole) {
    return E_INVALIDARG;
  }
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_accRole(varChild, pvarRole);
}


HRESULT
AccessibleHandler::get_accState(VARIANT varChild, VARIANT *pvarState)
{
  if (!pvarState) {
    return E_INVALIDARG;
  }
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_accState(varChild, pvarState);
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
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_accKeyboardShortcut(varChild, pszKeyboardShortcut);
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
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_accDefaultAction(varChild, pszDefaultAction);
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
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->accLocation(pxLeft, pyTop, pcxWidth, pcyHeight,
                                   varChild);
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
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->role(role);
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
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_states(states);
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
  *uniqueID = mCachedData.mData.mUniqueId;
  return S_OK;
}

HRESULT
AccessibleHandler::get_windowHandle(HWND* windowHandle)
{
  if (!windowHandle) {
    return E_INVALIDARG;
  }
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_windowHandle(windowHandle);
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
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_locale(locale);
  return S_OK;
}

HRESULT
AccessibleHandler::get_attributes(BSTR* attributes)
{
  if (!attributes) {
    return E_INVALIDARG;
  }
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_attributes(attributes);
}

HRESULT
AccessibleHandler::get_attribute(BSTR name, VARIANT* attribute)
{
  // We could extract these individually from cached mAttributes.
  // Consider it if traffic warrants it
  HRESULT hr = ResolveIA2();
  if (FAILED(hr)) {
    return hr;
  }
  return mIA2PassThru->get_attribute(name, attribute);
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
  // Unknown, queried by Windows
  {0xb96fdb85, 0x7204, 0x4724, { 0x84, 0x2b, 0xc7, 0x05, 0x9d, 0xed, 0xb9, 0xd0 }}
};

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

HRESULT
AccessibleHandler::GetClassInfo(ITypeInfo** aOutTypeInfo)
{
  RefPtr<AccessibleHandlerControl> ctl(gControlFactory.GetOrCreateSingleton());
  if (!ctl) {
    return E_OUTOFMEMORY;
  }

  return ctl->GetHandlerTypeInfo(aOutTypeInfo);
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
