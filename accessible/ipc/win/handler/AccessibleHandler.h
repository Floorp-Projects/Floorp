/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_AccessibleHandler_h
#define mozilla_a11y_AccessibleHandler_h

#define NEWEST_IA2_BASENAME Accessible2_3

#define __QUOTE(idl) #idl
#define __GENIDL(base) __QUOTE(base##.idl)
#define IDLFOR(base) __GENIDL(base)
#define NEWEST_IA2_IDL IDLFOR(NEWEST_IA2_BASENAME)

#define __GENIFACE(base) I##base
#define INTERFACEFOR(base) __GENIFACE(base)
#define NEWEST_IA2_INTERFACE INTERFACEFOR(NEWEST_IA2_BASENAME)

#define __GENIID(iface) IID_##iface
#define IIDFOR(iface) __GENIID(iface)
#define NEWEST_IA2_IID IIDFOR(NEWEST_IA2_INTERFACE)

#if defined(__midl)

import NEWEST_IA2_IDL;

#else

#include "HandlerData.h"

#include <windows.h>

#if !defined(MOZILLA_INTERNAL_API)

#include "Accessible2_3.h"
#include "Handler.h"
#include "mozilla/mscom/StructStream.h"
#include "mozilla/UniquePtr.h"

#include <ocidl.h>
#include <servprov.h>

namespace mozilla {
namespace a11y {

class AccessibleHandler final : public mscom::Handler
                              , public NEWEST_IA2_INTERFACE
                              , public IServiceProvider
                              , public IProvideClassInfo
{
public:
  static HRESULT Create(IUnknown* aOuter, REFIID aIid, void** aOutInterface);

  // mscom::Handler
  HRESULT QueryHandlerInterface(IUnknown* aProxyUnknown, REFIID aIid,
                                void** aOutInterface) override;
  HRESULT ReadHandlerPayload(IStream* aStream, REFIID aIid) override;

  REFIID MarshalAs(REFIID aRequestedIid) override;
  HRESULT GetHandlerPayloadSize(REFIID aIid, DWORD* aOutPayloadSize) override;
  HRESULT WriteHandlerPayload(IStream* aStream, REFIID aIId) override;

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;

  // IDispatch
  STDMETHODIMP GetTypeInfoCount(UINT *pctinfo) override;
  STDMETHODIMP GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo **ppTInfo) override;
  STDMETHODIMP GetIDsOfNames(REFIID riid, LPOLESTR *rgszNames, UINT cNames,
                             LCID lcid, DISPID *rgDispId) override;
  STDMETHODIMP Invoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags,
                      DISPPARAMS *pDispParams, VARIANT *pVarResult,
                      EXCEPINFO *pExcepInfo, UINT *puArgErr) override;

  // IAccessible
  STDMETHODIMP get_accParent(IDispatch **ppdispParent) override;
  STDMETHODIMP get_accChildCount(long *pcountChildren) override;
  STDMETHODIMP get_accChild(VARIANT varChild, IDispatch **ppdispChild) override;
  STDMETHODIMP get_accName(VARIANT varChild, BSTR *pszName) override;
  STDMETHODIMP get_accValue(VARIANT varChild, BSTR *pszValue) override;
  STDMETHODIMP get_accDescription(VARIANT varChild, BSTR *pszDescription) override;
  STDMETHODIMP get_accRole(VARIANT varChild, VARIANT *pvarRole) override;
  STDMETHODIMP get_accState(VARIANT varChild, VARIANT *pvarState) override;
  STDMETHODIMP get_accHelp(VARIANT varChild, BSTR *pszHelp) override;
  STDMETHODIMP get_accHelpTopic(BSTR *pszHelpFile, VARIANT varChild,
                                long *pidTopic) override;
  STDMETHODIMP get_accKeyboardShortcut(VARIANT varChild,
                                       BSTR *pszKeyboardShortcut) override;
  STDMETHODIMP get_accFocus(VARIANT *pvarChild) override;
  STDMETHODIMP get_accSelection(VARIANT *pvarChildren) override;
  STDMETHODIMP get_accDefaultAction(VARIANT varChild,
                                    BSTR *pszDefaultAction) override;
  STDMETHODIMP accSelect(long flagsSelect, VARIANT varChild) override;
  STDMETHODIMP accLocation(long *pxLeft, long *pyTop, long *pcxWidth,
                           long *pcyHeight, VARIANT varChild) override;
  STDMETHODIMP accNavigate(long navDir, VARIANT varStart,
                           VARIANT *pvarEndUpAt) override;
  STDMETHODIMP accHitTest( long xLeft, long yTop, VARIANT *pvarChild) override;
  STDMETHODIMP accDoDefaultAction(VARIANT varChild) override;
  STDMETHODIMP put_accName(VARIANT varChild, BSTR szName) override;
  STDMETHODIMP put_accValue(VARIANT varChild, BSTR szValue) override;

  // IAccessible2
  STDMETHODIMP get_nRelations(long* nRelations) override;
  STDMETHODIMP get_relation(long relationIndex,
                            IAccessibleRelation** relation) override;
  STDMETHODIMP get_relations(long maxRelations, IAccessibleRelation** relations,
                             long* nRelations) override;
  STDMETHODIMP role(long* role) override;
  STDMETHODIMP scrollTo(IA2ScrollType scrollType) override;
  STDMETHODIMP scrollToPoint(IA2CoordinateType coordinateType, long x,
                             long y) override;
  STDMETHODIMP get_groupPosition(long* groupLevel, long* similarItemsInGroup,
                                 long* positionInGroup) override;
  STDMETHODIMP get_states(AccessibleStates* states) override;
  STDMETHODIMP get_extendedRole(BSTR* extendedRole) override;
  STDMETHODIMP get_localizedExtendedRole(BSTR* localizedExtendedRole) override;
  STDMETHODIMP get_nExtendedStates(long* nExtendedStates) override;
  STDMETHODIMP get_extendedStates(long maxExtendedStates, BSTR** extendedStates,
                                  long* nExtendedStates) override;
  STDMETHODIMP get_localizedExtendedStates(long maxLocalizedExtendedStates,
                                           BSTR** localizedExtendedStates,
                                           long* nLocalizedExtendedStates) override;
  STDMETHODIMP get_uniqueID(long* uniqueID) override;
  STDMETHODIMP get_windowHandle(HWND* windowHandle) override;
  STDMETHODIMP get_indexInParent(long* indexInParent) override;
  STDMETHODIMP get_locale(IA2Locale* locale) override;
  STDMETHODIMP get_attributes(BSTR* attributes) override;

  // IAccessible2_2
  STDMETHODIMP get_attribute(BSTR name, VARIANT* attribute) override;
  STDMETHODIMP get_accessibleWithCaret(IUnknown** accessible,
                                       long* caretOffset) override;
  STDMETHODIMP get_relationTargetsOfType(BSTR type, long maxTargets,
                                         IUnknown*** targets,
                                         long* nTargets) override;

  // IAccessible2_3
  STDMETHODIMP get_selectionRanges(IA2Range** ranges, long* nRanges) override;

  // IServiceProvider
  STDMETHODIMP QueryService(REFGUID aServiceId, REFIID aIid,
                            void** aOutInterface) override;

  // IProvideClassInfo
  STDMETHODIMP GetClassInfo(ITypeInfo** aOutTypeInfo) override;

private:
  AccessibleHandler(IUnknown* aOuter, HRESULT* aResult);
  virtual ~AccessibleHandler();

  HRESULT ResolveIA2();
  HRESULT ResolveIDispatch();
  HRESULT MaybeUpdateCachedData();

  RefPtr<IUnknown>                  mDispatchUnk;
  /**
   * Handlers aggregate their proxies. This means that their proxies delegate
   * their IUnknown implementation to us.
   *
   * mDispatchUnk and the result of Handler::GetProxy() are both strong
   * references to the aggregated objects. OTOH, any interfaces that are QI'd
   * from those aggregated objects have delegated unknowns.
   *
   * AddRef'ing an interface with a delegated unknown ends up incrementing the
   * refcount of the *aggregator*. Since we are the aggregator of mDispatchUnk
   * and of the wrapped proxy, holding a strong reference to any interfaces
   * QI'd off of those objects would create a reference cycle.
   *
   * We may hold onto pointers to those references, but when we query them we
   * must immediately Release() them to prevent these cycles.
   *
   * It is safe for us to use these raw pointers because the aggregated
   * objects's lifetimes are proper subsets of our own lifetime.
   */
  IDispatch*                        mDispatch;         // weak
  NEWEST_IA2_INTERFACE*             mIA2PassThru;      // weak
  IServiceProvider*                 mServProvPassThru; // weak
  IA2Payload                        mCachedData;
  UniquePtr<mscom::StructToStream>  mSerializer;
  uint32_t                          mCacheGen;
};

} // namespace a11y
} // namespace mozilla

#endif // !defined(MOZILLA_INTERNAL_API)

#endif // defined(__midl)

#endif // mozilla_a11y_AccessibleHandler_h
