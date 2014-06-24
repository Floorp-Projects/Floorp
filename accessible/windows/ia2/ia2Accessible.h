/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_ia2Accessible_h_
#define mozilla_a11y_ia2Accessible_h_

#include "nsISupports.h"

#include "Accessible2_2.h"

namespace mozilla {
namespace a11y {

class ia2Accessible : public IAccessible2_2
{
public:

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID, void**);

  // IAccessible2
  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_nRelations(
    /* [retval][out] */ long* nRelations);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_relation(
    /* [in] */ long relationIndex,
    /* [retval][out] */ IAccessibleRelation** relation);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_relations(
    /* [in] */ long maxRelations,
    /* [length_is][size_is][out] */ IAccessibleRelation** relation,
    /* [retval][out] */ long* nRelations);

  virtual HRESULT STDMETHODCALLTYPE role(
    /* [retval][out] */ long* role);

  virtual HRESULT STDMETHODCALLTYPE scrollTo(
    /* [in] */ enum IA2ScrollType scrollType);

  virtual HRESULT STDMETHODCALLTYPE scrollToPoint(
    /* [in] */ enum IA2CoordinateType coordinateType,
    /* [in] */ long x,
    /* [in] */ long y);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_groupPosition(
    /* [out] */ long* groupLevel,
    /* [out] */ long* similarItemsInGroup,
    /* [retval][out] */ long* positionInGroup);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_states(
    /* [retval][out] */ AccessibleStates* states);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_extendedRole(
    /* [retval][out] */ BSTR* extendedRole);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_localizedExtendedRole(
    /* [retval][out] */ BSTR* localizedExtendedRole);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_nExtendedStates(
    /* [retval][out] */ long* nExtendedStates);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_extendedStates(
    /* [in] */ long maxExtendedStates,
    /* [length_is][length_is][size_is][size_is][out] */ BSTR** extendedStates,
    /* [retval][out] */ long* nExtendedStates);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_localizedExtendedStates(
    /* [in] */ long maxLocalizedExtendedStates,
    /* [length_is][length_is][size_is][size_is][out] */ BSTR** localizedExtendedStates,
    /* [retval][out] */ long* nLocalizedExtendedStates);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_uniqueID(
    /* [retval][out] */ long* uniqueID);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_windowHandle(
    /* [retval][out] */ HWND* windowHandle);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_indexInParent(
    /* [retval][out] */ long* indexInParent);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_locale(
    /* [retval][out] */ IA2Locale* locale);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_attributes(
    /* [retval][out] */ BSTR* attributes);

  // IAccessible2_2
  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_attribute(
    /* [in] */ BSTR name,
    /* [out, retval] */ VARIANT* attribute);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_accessibleWithCaret(
    /* [out] */ IUnknown** accessible,
    /* [out, retval] */ long* caretOffset);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_relationTargetsOfType(
    /* [in] */ BSTR type,
    /* [in] */ long maxTargets,
    /* [out, size_is(,*nTargets)] */ IUnknown*** targets,
    /* [out, retval] */ long* nTargets
  );

  // Helper method
  static HRESULT ConvertToIA2Attributes(nsIPersistentProperties* aAttributes,
                                        BSTR* aIA2Attributes);
};

} // namespace a11y
} // namespace mozilla

#endif
