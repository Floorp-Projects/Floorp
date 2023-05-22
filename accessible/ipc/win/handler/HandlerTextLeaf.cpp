/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(MOZILLA_INTERNAL_API)
#  error This code is NOT for internal Gecko use!
#endif  // defined(MOZILLA_INTERNAL_API)

#include "HandlerTextLeaf.h"
#include "mozilla/Assertions.h"

namespace mozilla {
namespace a11y {

HandlerTextLeaf::HandlerTextLeaf(IDispatch* aParent, long aIndexInParent,
                                 HWND aHwnd, AccChildData& aData)
    : mParent(aParent),
      mIndexInParent(aIndexInParent),
      mHwnd(aHwnd),
      mData(aData) {
  MOZ_ASSERT(aParent);
}

HandlerTextLeaf::~HandlerTextLeaf() {
  if (mData.mText) {
    ::SysFreeString(mData.mText);
  }
}

IMPL_IUNKNOWN_QUERY_HEAD(HandlerTextLeaf)
IMPL_IUNKNOWN_QUERY_IFACE(IDispatch)
IMPL_IUNKNOWN_QUERY_IFACE(IAccessible)
IMPL_IUNKNOWN_QUERY_IFACE(IAccessible2)
IMPL_IUNKNOWN_QUERY_IFACE(IServiceProvider)
IMPL_IUNKNOWN_QUERY_TAIL

/*** IDispatch ***/

HRESULT
HandlerTextLeaf::GetTypeInfoCount(UINT* pctinfo) { return E_NOTIMPL; }

HRESULT
HandlerTextLeaf::GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo) {
  return E_NOTIMPL;
}

HRESULT
HandlerTextLeaf::GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, UINT cNames,
                               LCID lcid, DISPID* rgDispId) {
  return E_NOTIMPL;
}

HRESULT
HandlerTextLeaf::Invoke(DISPID dispIdMember, REFIID riid, LCID lcid,
                        WORD wFlags, DISPPARAMS* pDispParams,
                        VARIANT* pVarResult, EXCEPINFO* pExcepInfo,
                        UINT* puArgErr) {
  return E_NOTIMPL;
}

/*** IAccessible ***/

HRESULT
HandlerTextLeaf::get_accParent(IDispatch** ppdispParent) {
  if (!ppdispParent) {
    return E_INVALIDARG;
  }

  RefPtr<IDispatch> parent(mParent);
  parent.forget(ppdispParent);
  return S_OK;
}

HRESULT
HandlerTextLeaf::get_accChildCount(long* pcountChildren) {
  if (!pcountChildren) {
    return E_INVALIDARG;
  }

  *pcountChildren = 0;
  return S_OK;
}

HRESULT
HandlerTextLeaf::get_accChild(VARIANT varChild, IDispatch** ppdispChild) {
  return E_NOTIMPL;
}

HRESULT
HandlerTextLeaf::get_accName(VARIANT varChild, BSTR* pszName) {
  if (varChild.lVal != CHILDID_SELF || !pszName) {
    return E_INVALIDARG;
  }

  *pszName = CopyBSTR(mData.mText);
  return S_OK;
}

HRESULT
HandlerTextLeaf::get_accValue(VARIANT varChild, BSTR* pszValue) {
  return E_NOTIMPL;
}

HRESULT
HandlerTextLeaf::get_accDescription(VARIANT varChild, BSTR* pszDescription) {
  return E_NOTIMPL;
}

HRESULT
HandlerTextLeaf::get_accRole(VARIANT varChild, VARIANT* pvarRole) {
  if (varChild.lVal != CHILDID_SELF || !pvarRole) {
    return E_INVALIDARG;
  }

  pvarRole->vt = VT_I4;
  pvarRole->lVal = mData.mTextRole;
  return S_OK;
}

HRESULT
HandlerTextLeaf::get_accState(VARIANT varChild, VARIANT* pvarState) {
  if (varChild.lVal != CHILDID_SELF || !pvarState) {
    return E_INVALIDARG;
  }

  pvarState->vt = VT_I4;
  pvarState->lVal = mData.mTextState;
  return S_OK;
}

HRESULT
HandlerTextLeaf::get_accHelp(VARIANT varChild, BSTR* pszHelp) {
  return E_NOTIMPL;
}

HRESULT
HandlerTextLeaf::get_accHelpTopic(BSTR* pszHelpFile, VARIANT varChild,
                                  long* pidTopic) {
  return E_NOTIMPL;
}

HRESULT
HandlerTextLeaf::get_accKeyboardShortcut(VARIANT varChild,
                                         BSTR* pszKeyboardShortcut) {
  return E_NOTIMPL;
}

HRESULT
HandlerTextLeaf::get_accFocus(VARIANT* pvarChild) { return E_NOTIMPL; }

HRESULT
HandlerTextLeaf::get_accSelection(VARIANT* pvarChildren) { return E_NOTIMPL; }

HRESULT
HandlerTextLeaf::get_accDefaultAction(VARIANT varChild,
                                      BSTR* pszDefaultAction) {
  return E_NOTIMPL;
}

HRESULT
HandlerTextLeaf::accSelect(long flagsSelect, VARIANT varChild) {
  return E_NOTIMPL;
}

HRESULT
HandlerTextLeaf::accLocation(long* pxLeft, long* pyTop, long* pcxWidth,
                             long* pcyHeight, VARIANT varChild) {
  if (varChild.lVal != CHILDID_SELF || !pxLeft || !pyTop || !pcxWidth ||
      !pcyHeight) {
    return E_INVALIDARG;
  }

  *pxLeft = mData.mTextLeft;
  *pyTop = mData.mTextTop;
  *pcxWidth = mData.mTextWidth;
  *pcyHeight = mData.mTextHeight;
  return S_OK;
}

HRESULT
HandlerTextLeaf::accNavigate(long navDir, VARIANT varStart,
                             VARIANT* pvarEndUpAt) {
  return E_NOTIMPL;
}

HRESULT
HandlerTextLeaf::accHitTest(long xLeft, long yTop, VARIANT* pvarChild) {
  return E_NOTIMPL;
}

HRESULT
HandlerTextLeaf::accDoDefaultAction(VARIANT varChild) { return E_NOTIMPL; }

HRESULT
HandlerTextLeaf::put_accName(VARIANT varChild, BSTR szName) {
  return E_NOTIMPL;
}

HRESULT
HandlerTextLeaf::put_accValue(VARIANT varChild, BSTR szValue) {
  return E_NOTIMPL;
}

/*** IAccessible2 ***/

HRESULT
HandlerTextLeaf::get_nRelations(long* nRelations) { return E_NOTIMPL; }

HRESULT
HandlerTextLeaf::get_relation(long relationIndex,
                              IAccessibleRelation** relation) {
  return E_NOTIMPL;
}

HRESULT
HandlerTextLeaf::get_relations(long maxRelations,
                               IAccessibleRelation** relations,
                               long* nRelations) {
  return E_NOTIMPL;
}

HRESULT
HandlerTextLeaf::role(long* role) {
  if (!role) {
    return E_INVALIDARG;
  }

  *role = mData.mTextRole;
  return S_OK;
}

HRESULT
HandlerTextLeaf::scrollTo(IA2ScrollType scrollType) { return E_NOTIMPL; }

HRESULT
HandlerTextLeaf::scrollToPoint(IA2CoordinateType coordinateType, long x,
                               long y) {
  return E_NOTIMPL;
}

HRESULT
HandlerTextLeaf::get_groupPosition(long* groupLevel, long* similarItemsInGroup,
                                   long* positionInGroup) {
  return E_NOTIMPL;
}

HRESULT
HandlerTextLeaf::get_states(AccessibleStates* states) {
  if (!states) {
    return E_INVALIDARG;
  }

  *states = 0;
  return S_OK;
}

HRESULT
HandlerTextLeaf::get_extendedRole(BSTR* extendedRole) { return E_NOTIMPL; }

HRESULT
HandlerTextLeaf::get_localizedExtendedRole(BSTR* localizedExtendedRole) {
  return E_NOTIMPL;
}

HRESULT
HandlerTextLeaf::get_nExtendedStates(long* nExtendedStates) {
  return E_NOTIMPL;
}

HRESULT
HandlerTextLeaf::get_extendedStates(long maxExtendedStates,
                                    BSTR** extendedStates,
                                    long* nExtendedStates) {
  return E_NOTIMPL;
}

HRESULT
HandlerTextLeaf::get_localizedExtendedStates(long maxLocalizedExtendedStates,
                                             BSTR** localizedExtendedStates,
                                             long* nLocalizedExtendedStates) {
  return E_NOTIMPL;
}

HRESULT
HandlerTextLeaf::get_uniqueID(long* uniqueID) {
  if (!uniqueID) {
    return E_INVALIDARG;
  }

  *uniqueID = mData.mTextId;
  return S_OK;
}

HRESULT
HandlerTextLeaf::get_windowHandle(HWND* windowHandle) {
  if (!windowHandle) {
    return E_INVALIDARG;
  }

  *windowHandle = mHwnd;
  return S_OK;
}

HRESULT
HandlerTextLeaf::get_indexInParent(long* indexInParent) {
  if (!indexInParent) {
    return E_INVALIDARG;
  }

  *indexInParent = mIndexInParent;
  return S_OK;
}

HRESULT
HandlerTextLeaf::get_locale(IA2Locale* locale) { return E_NOTIMPL; }

HRESULT
HandlerTextLeaf::get_attributes(BSTR* attributes) { return E_NOTIMPL; }

/*** IServiceProvider ***/

HRESULT
HandlerTextLeaf::QueryService(REFGUID aServiceId, REFIID aIid,
                              void** aOutInterface) {
  if (aIid == IID_IAccessible2) {
    RefPtr<IAccessible2> ia2(this);
    ia2.forget(aOutInterface);
    return S_OK;
  }

  return E_INVALIDARG;
}

}  // namespace a11y
}  // namespace mozilla
