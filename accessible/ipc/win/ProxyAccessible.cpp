/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Accessible2.h"
#include "ProxyAccessible.h"
#include "mozilla/a11y/DocAccessibleParent.h"
#include "DocAccessible.h"
#include "mozilla/a11y/DocManager.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/Unused.h"
#include "mozilla/a11y/Platform.h"
#include "RelationType.h"
#include "mozilla/a11y/Role.h"
#include "xpcAccessibleDocument.h"

#include <comutil.h>

namespace mozilla {
namespace a11y {

bool
ProxyAccessible::GetCOMInterface(void** aOutAccessible) const
{
  if (!aOutAccessible) {
    return false;
  }
  RefPtr<IAccessible> addRefed = mCOMProxy;
  addRefed.forget(aOutAccessible);
  return !!mCOMProxy;
}

void
ProxyAccessible::Name(nsString& aName) const
{
  aName.Truncate();
  RefPtr<IAccessible> acc;
  if (!GetCOMInterface((void**)getter_AddRefs(acc))) {
    return;
  }

  VARIANT id;
  id.vt = VT_I4;
  id.lVal = CHILDID_SELF;
  BSTR result;
  HRESULT hr = acc->get_accName(id, &result);
  _bstr_t resultWrap(result, false);
  if (FAILED(hr)) {
    return;
  }
  aName = (wchar_t*)resultWrap;
}

void
ProxyAccessible::Value(nsString& aValue) const
{
  aValue.Truncate();
  RefPtr<IAccessible> acc;
  if (!GetCOMInterface((void**)getter_AddRefs(acc))) {
    return;
  }

  VARIANT id;
  id.vt = VT_I4;
  id.lVal = CHILDID_SELF;
  BSTR result;
  HRESULT hr = acc->get_accValue(id, &result);
  _bstr_t resultWrap(result, false);
  if (FAILED(hr)) {
    return;
  }
  aValue = (wchar_t*)resultWrap;
}

void
ProxyAccessible::Description(nsString& aDesc) const
{
  aDesc.Truncate();
  RefPtr<IAccessible> acc;
  if (!GetCOMInterface((void**)getter_AddRefs(acc))) {
    return;
  }

  VARIANT id;
  id.vt = VT_I4;
  id.lVal = CHILDID_SELF;
  BSTR result;
  HRESULT hr = acc->get_accDescription(id, &result);
  _bstr_t resultWrap(result, false);
  if (FAILED(hr)) {
    return;
  }
  aDesc = (wchar_t*)resultWrap;
}

uint64_t
ProxyAccessible::State() const
{
  uint64_t state = 0;
  RefPtr<IAccessible> acc;
  if (!GetCOMInterface((void**)getter_AddRefs(acc))) {
    return state;
  }

  VARIANT id;
  id.vt = VT_I4;
  id.lVal = CHILDID_SELF;
  VARIANT varState;
  HRESULT hr = acc->get_accState(id, &varState);
  if (FAILED(hr)) {
    return state;
  }
  return uint64_t(varState.lVal);
}

nsIntRect
ProxyAccessible::Bounds()
{
  nsIntRect rect;

  RefPtr<IAccessible> acc;
  if (!GetCOMInterface((void**)getter_AddRefs(acc))) {
    return rect;
  }

  long left;
  long top;
  long width;
  long height;
  VARIANT id;
  id.vt = VT_I4;
  id.lVal = CHILDID_SELF;
  HRESULT hr = acc->accLocation(&left, &top, &width, &height, id);
  if (FAILED(hr)) {
    return rect;
  }
  rect.x = left;
  rect.y = top;
  rect.width = width;
  rect.height = height;
  return rect;
}

void
ProxyAccessible::Language(nsString& aLocale)
{
  aLocale.Truncate();

  RefPtr<IAccessible> acc;
  if (!GetCOMInterface((void**)getter_AddRefs(acc))) {
    return;
  }

  RefPtr<IAccessible2> acc2;
  if (FAILED(acc->QueryInterface(IID_IAccessible2, (void**)getter_AddRefs(acc2)))) {
    return;
  }

  IA2Locale locale;
  HRESULT hr = acc2->get_locale(&locale);

  _bstr_t langWrap(locale.language, false);
  _bstr_t countryWrap(locale.country, false);
  _bstr_t variantWrap(locale.variant, false);

  if (FAILED(hr)) {
    return;
  }

  // The remaining code should essentially be the inverse of the
  // ia2Accessible::get_locale conversion to IA2Locale.

  if (!!variantWrap) {
    aLocale = (wchar_t*)variantWrap;
    return;
  }

  if (!!langWrap) {
    aLocale = (wchar_t*)langWrap;
    if (!!countryWrap) {
      aLocale += L"-";
      aLocale += (wchar_t*)countryWrap;
    }
  }
}

static bool
IsEscapedChar(const wchar_t c)
{
  return c == L'\\' || c == L':' || c == ',' || c == '=' || c == ';';
}

static bool
ConvertBSTRAttributesToArray(const nsAString& aStr,
                             nsTArray<Attribute>* aAttrs)
{
  if (!aAttrs) {
    return false;
  }

  enum
  {
    eName = 0,
    eValue = 1,
    eNumStates
  } state;
  nsAutoString tokens[eNumStates];
  auto itr = aStr.BeginReading(), end = aStr.EndReading();

  state = eName;
  while (itr != end) {
    switch (*itr) {
      case L'\\':
        // Skip the backslash so that we're looking at the escaped char
        ++itr;
        if (itr == end || !IsEscapedChar(*itr)) {
          // Invalid state
          return false;
        }
        break;
      case L':':
        if (state != eName) {
          // Bad, should be looking at name
          return false;
        }
        state = eValue;
        ++itr;
        continue;
      case L';':
        if (state != eValue) {
          // Bad, should be looking at value
          return false;
        }
        state = eName;
        aAttrs->AppendElement(Attribute(NS_ConvertUTF16toUTF8(tokens[eName]),
                                        tokens[eValue]));
        tokens[eName].Truncate();
        tokens[eValue].Truncate();
        ++itr;
        continue;
      default:
        break;
    }
    tokens[state] += *itr;
  }
  return true;
}

void
ProxyAccessible::Attributes(nsTArray<Attribute>* aAttrs) const
{
  aAttrs->Clear();

  RefPtr<IAccessible> acc;
  if (!GetCOMInterface((void**)getter_AddRefs(acc))) {
    return;
  }

  RefPtr<IAccessible2> acc2;
  if (FAILED(acc->QueryInterface(IID_IAccessible2, (void**)getter_AddRefs(acc2)))) {
    return;
  }

  BSTR attrs;
  HRESULT hr = acc2->get_attributes(&attrs);
  _bstr_t attrsWrap(attrs, false);
  if (FAILED(hr)) {
    return;
  }

  ConvertBSTRAttributesToArray(nsDependentString((wchar_t*)attrs,
                                                 attrsWrap.length()),
                               aAttrs);
}

} // namespace a11y
} // namespace mozilla
