/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(MOZILLA_INTERNAL_API)
#  error This code is NOT for internal Gecko use!
#endif  // defined(MOZILLA_INTERNAL_API)

#ifndef mozilla_a11y_HandlerTextLeaf_h
#  define mozilla_a11y_HandlerTextLeaf_h

#  include "AccessibleHandler.h"
#  include "IUnknownImpl.h"
#  include "mozilla/RefPtr.h"

namespace mozilla {
namespace a11y {

class HandlerTextLeaf final : public IAccessible2, public IServiceProvider {
 public:
  explicit HandlerTextLeaf(IDispatch* aParent, long aIndexInParent, HWND aHwnd,
                           AccChildData& aData);

  DECL_IUNKNOWN

  // IDispatch
  STDMETHODIMP GetTypeInfoCount(UINT* pctinfo) override;
  STDMETHODIMP GetTypeInfo(UINT iTInfo, LCID lcid,
                           ITypeInfo** ppTInfo) override;
  STDMETHODIMP GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, UINT cNames,
                             LCID lcid, DISPID* rgDispId) override;
  STDMETHODIMP Invoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags,
                      DISPPARAMS* pDispParams, VARIANT* pVarResult,
                      EXCEPINFO* pExcepInfo, UINT* puArgErr) override;

  // IAccessible
  STDMETHODIMP get_accParent(IDispatch** ppdispParent) override;
  STDMETHODIMP get_accChildCount(long* pcountChildren) override;
  STDMETHODIMP get_accChild(VARIANT varChild, IDispatch** ppdispChild) override;
  STDMETHODIMP get_accName(VARIANT varChild, BSTR* pszName) override;
  STDMETHODIMP get_accValue(VARIANT varChild, BSTR* pszValue) override;
  STDMETHODIMP get_accDescription(VARIANT varChild,
                                  BSTR* pszDescription) override;
  STDMETHODIMP get_accRole(VARIANT varChild, VARIANT* pvarRole) override;
  STDMETHODIMP get_accState(VARIANT varChild, VARIANT* pvarState) override;
  STDMETHODIMP get_accHelp(VARIANT varChild, BSTR* pszHelp) override;
  STDMETHODIMP get_accHelpTopic(BSTR* pszHelpFile, VARIANT varChild,
                                long* pidTopic) override;
  STDMETHODIMP get_accKeyboardShortcut(VARIANT varChild,
                                       BSTR* pszKeyboardShortcut) override;
  STDMETHODIMP get_accFocus(VARIANT* pvarChild) override;
  STDMETHODIMP get_accSelection(VARIANT* pvarChildren) override;
  STDMETHODIMP get_accDefaultAction(VARIANT varChild,
                                    BSTR* pszDefaultAction) override;
  STDMETHODIMP accSelect(long flagsSelect, VARIANT varChild) override;
  STDMETHODIMP accLocation(long* pxLeft, long* pyTop, long* pcxWidth,
                           long* pcyHeight, VARIANT varChild) override;
  STDMETHODIMP accNavigate(long navDir, VARIANT varStart,
                           VARIANT* pvarEndUpAt) override;
  STDMETHODIMP accHitTest(long xLeft, long yTop, VARIANT* pvarChild) override;
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
  STDMETHODIMP get_localizedExtendedStates(
      long maxLocalizedExtendedStates, BSTR** localizedExtendedStates,
      long* nLocalizedExtendedStates) override;
  STDMETHODIMP get_uniqueID(long* uniqueID) override;
  STDMETHODIMP get_windowHandle(HWND* windowHandle) override;
  STDMETHODIMP get_indexInParent(long* indexInParent) override;
  STDMETHODIMP get_locale(IA2Locale* locale) override;
  STDMETHODIMP get_attributes(BSTR* attributes) override;

  // IServiceProvider
  STDMETHODIMP QueryService(REFGUID aServiceId, REFIID aIid,
                            void** aOutInterface) override;

 private:
  ~HandlerTextLeaf();

  RefPtr<IDispatch> mParent;
  long mIndexInParent;
  HWND mHwnd;
  AccChildData mData;
};

}  // namespace a11y
}  // namespace mozilla

#endif  // mozilla_a11y_HandlerTextLeaf_h
