/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Accessible2.h"
#include "RemoteAccessible.h"
#include "ia2AccessibleRelation.h"
#include "ia2AccessibleValue.h"
#include "IGeckoCustom.h"
#include "mozilla/a11y/DocAccessibleParent.h"
#include "DocAccessible.h"
#include "mozilla/a11y/DocManager.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/Unused.h"
#include "mozilla/a11y/Platform.h"
#include "Relation.h"
#include "RelationType.h"
#include "mozilla/a11y/Role.h"
#include "mozilla/StaticPrefs_accessibility.h"

#include <comutil.h>

static const VARIANT kChildIdSelf = {{{VT_I4}}};

namespace mozilla {
namespace a11y {

bool RemoteAccessible::GetCOMInterface(void** aOutAccessible) const {
  if (!aOutAccessible) {
    return false;
  }

  // This should never be called if the cache is enabled. We can't get a COM
  // proxy from the content process in that case. Instead, the code below would
  // return an MsaaAccessible from our process which would end up calling
  // methods here in RemoteAccessible, causing infinite recursion.
  MOZ_ASSERT(!StaticPrefs::accessibility_cache_enabled_AtStartup());
  if (!mCOMProxy && mSafeToRecurse) {
    RemoteAccessible* thisPtr = const_cast<RemoteAccessible*>(this);
    // See if we can lazily obtain a COM proxy
    MsaaAccessible* msaa = MsaaAccessible::GetFrom(thisPtr);
    bool isDefunct = false;
    // NB: Don't pass CHILDID_SELF here, use the absolute MSAA ID. Otherwise
    // GetIAccessibleFor will recurse into this function and we will just
    // overflow the stack.
    VARIANT realId = {{{VT_I4}}};
    realId.ulVal = msaa->GetExistingID();
    MOZ_DIAGNOSTIC_ASSERT(realId.ulVal != CHILDID_SELF);
    thisPtr->mCOMProxy = msaa->GetIAccessibleFor(realId, &isDefunct);
  }

  RefPtr<IAccessible> addRefed = mCOMProxy;
  addRefed.forget(aOutAccessible);
  return !!mCOMProxy;
}

/**
 * Specializations of this template map an IAccessible type to its IID
 */
template <typename Interface>
struct InterfaceIID {};

template <>
struct InterfaceIID<IAccessibleValue> {
  static REFIID Value() { return IID_IAccessibleValue; }
};

template <>
struct InterfaceIID<IAccessibleText> {
  static REFIID Value() { return IID_IAccessibleText; }
};

template <>
struct InterfaceIID<IAccessibleHyperlink> {
  static REFIID Value() { return IID_IAccessibleHyperlink; }
};

template <>
struct InterfaceIID<IGeckoCustom> {
  static REFIID Value() { return IID_IGeckoCustom; }
};

template <>
struct InterfaceIID<IAccessible2_2> {
  static REFIID Value() { return IID_IAccessible2_2; }
};

/**
 * Get the COM proxy for this proxy accessible and QueryInterface it with the
 * correct IID
 */
template <typename Interface>
static already_AddRefed<Interface> QueryInterface(
    const RemoteAccessible* aProxy) {
  RefPtr<IAccessible> acc;
  if (!aProxy->GetCOMInterface((void**)getter_AddRefs(acc))) {
    return nullptr;
  }

  RefPtr<Interface> acc2;
  if (FAILED(acc->QueryInterface(InterfaceIID<Interface>::Value(),
                                 (void**)getter_AddRefs(acc2)))) {
    return nullptr;
  }

  return acc2.forget();
}

static Maybe<uint64_t> GetIdFor(DocAccessibleParent* aDoc,
                                IUnknown* aCOMProxy) {
  RefPtr<IGeckoCustom> custom;
  if (FAILED(aCOMProxy->QueryInterface(IID_IGeckoCustom,
                                       (void**)getter_AddRefs(custom)))) {
    return Nothing();
  }

  uint64_t id;
  if (FAILED(custom->get_ID(&id))) {
    return Nothing();
  }

  return Some(id);
}

static RemoteAccessible* GetProxyFor(DocAccessibleParent* aDoc,
                                     IUnknown* aCOMProxy) {
  if (auto id = GetIdFor(aDoc, aCOMProxy)) {
    return aDoc->GetAccessible(*id);
  }
  return nullptr;
}

ENameValueFlag RemoteAccessible::Name(nsString& aName) const {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::Name(aName);
  }

  /* The return values here exist only to match behvaiour required
   * by the header declaration of this function. On Mac, we'd like
   * to return the associated ENameValueFlag, but we don't have
   * access to that here, so we return a dummy eNameOK value instead.
   */
  aName.Truncate();
  RefPtr<IAccessible> acc;
  if (!GetCOMInterface((void**)getter_AddRefs(acc))) {
    return eNameOK;
  }

  BSTR result;
  HRESULT hr = acc->get_accName(kChildIdSelf, &result);
  _bstr_t resultWrap(result, false);
  if (FAILED(hr)) {
    return eNameOK;
  }
  aName = (wchar_t*)resultWrap;
  return eNameOK;
}

void RemoteAccessible::Value(nsString& aValue) const {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    RemoteAccessibleBase<RemoteAccessible>::Value(aValue);
    return;
  }

  aValue.Truncate();
  RefPtr<IAccessible> acc;
  if (!GetCOMInterface((void**)getter_AddRefs(acc))) {
    return;
  }

  BSTR result;
  HRESULT hr = acc->get_accValue(kChildIdSelf, &result);
  _bstr_t resultWrap(result, false);
  if (FAILED(hr)) {
    return;
  }
  aValue = (wchar_t*)resultWrap;
}

double RemoteAccessible::Step() const {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::Step();
  }

  RefPtr<IGeckoCustom> custom = QueryInterface<IGeckoCustom>(this);
  if (!custom) {
    return 0;
  }

  double increment;
  HRESULT hr = custom->get_minimumIncrement(&increment);
  if (FAILED(hr)) {
    return 0;
  }

  return increment;
}

void RemoteAccessible::Description(nsString& aDesc) const {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::Description(aDesc);
  }

  aDesc.Truncate();
  RefPtr<IAccessible> acc;
  if (!GetCOMInterface((void**)getter_AddRefs(acc))) {
    return;
  }

  BSTR result;
  HRESULT hr = acc->get_accDescription(kChildIdSelf, &result);
  _bstr_t resultWrap(result, false);
  if (FAILED(hr)) {
    return;
  }
  aDesc = (wchar_t*)resultWrap;
}

uint64_t RemoteAccessible::State() {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::State();
  }

  RefPtr<IGeckoCustom> custom = QueryInterface<IGeckoCustom>(this);
  if (!custom) {
    return 0;
  }

  uint64_t state;
  HRESULT hr = custom->get_mozState(&state);
  if (FAILED(hr)) {
    return 0;
  }
  return state;
}

LayoutDeviceIntRect RemoteAccessible::Bounds() const {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::Bounds();
  }

  LayoutDeviceIntRect rect;

  RefPtr<IAccessible> acc;
  if (!GetCOMInterface((void**)getter_AddRefs(acc))) {
    return rect;
  }

  long left;
  long top;
  long width;
  long height;
  HRESULT hr = acc->accLocation(&left, &top, &width, &height, kChildIdSelf);
  if (FAILED(hr)) {
    return rect;
  }
  rect.SetRect(left, top, width, height);
  return rect;
}

nsIntRect RemoteAccessible::BoundsInCSSPixels() const {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::BoundsInCSSPixels();
  }

  RefPtr<IGeckoCustom> custom = QueryInterface<IGeckoCustom>(this);
  if (!custom) {
    return nsIntRect();
  }

  nsIntRect rect;
  Unused << custom->get_boundsInCSSPixels(&rect.x, &rect.y, &rect.width,
                                          &rect.height);
  return rect;
}

void RemoteAccessible::Language(nsString& aLocale) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    // Not yet supported by the cache.
    aLocale.Truncate();
    return;
  }
  aLocale.Truncate();

  RefPtr<IAccessible> acc;
  if (!GetCOMInterface((void**)getter_AddRefs(acc))) {
    return;
  }

  RefPtr<IAccessible2> acc2;
  if (FAILED(acc->QueryInterface(IID_IAccessible2,
                                 (void**)getter_AddRefs(acc2)))) {
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

static bool IsEscapedChar(const wchar_t c) {
  return c == L'\\' || c == L':' || c == ',' || c == '=' || c == ';';
}

// XXX: This creates an all-strings AccAttributes, this isn't ideal
// but an OK temporary stop-gap before IPC goes full IPDL.
static bool ConvertBSTRAttributesToAccAttributes(
    const nsAString& aStr, RefPtr<AccAttributes>& aAttrs) {
  enum { eName = 0, eValue = 1, eNumStates } state;
  nsString tokens[eNumStates];
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
      case L';': {
        if (state != eValue) {
          // Bad, should be looking at value
          return false;
        }
        state = eName;
        RefPtr<nsAtom> nameAtom = NS_Atomize(tokens[eName]);
        aAttrs->SetAttribute(nameAtom, std::move(tokens[eValue]));
        tokens[eName].Truncate();
        ++itr;
        continue;
      }
      default:
        break;
    }
    tokens[state] += *itr;
    ++itr;
  }
  return true;
}

already_AddRefed<AccAttributes> RemoteAccessible::Attributes() {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::Attributes();
  }
  RefPtr<AccAttributes> attrsObj = new AccAttributes();
  RefPtr<IAccessible> acc;
  if (!GetCOMInterface((void**)getter_AddRefs(acc))) {
    return attrsObj.forget();
  }

  RefPtr<IAccessible2> acc2;
  if (FAILED(acc->QueryInterface(IID_IAccessible2,
                                 (void**)getter_AddRefs(acc2)))) {
    return attrsObj.forget();
  }

  BSTR attrs;
  HRESULT hr = acc2->get_attributes(&attrs);
  _bstr_t attrsWrap(attrs, false);
  if (FAILED(hr)) {
    return attrsObj.forget();
  }

  ConvertBSTRAttributesToAccAttributes(
      nsDependentString((wchar_t*)attrs, attrsWrap.length()), attrsObj);
  return attrsObj.forget();
}

Relation RemoteAccessible::RelationByType(RelationType aType) const {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::RelationByType(aType);
  }

  RefPtr<IAccessible2_2> acc = QueryInterface<IAccessible2_2>(this);
  if (!acc) {
    return Relation();
  }

  _bstr_t relationType;
  for (uint32_t idx = 0; idx < ArrayLength(sRelationTypePairs); idx++) {
    if (aType == sRelationTypePairs[idx].first) {
      relationType = sRelationTypePairs[idx].second;
      break;
    }
  }

  if (!relationType) {
    return Relation();
  }

  IUnknown** targets;
  long nTargets = 0;
  HRESULT hr =
      acc->get_relationTargetsOfType(relationType, 0, &targets, &nTargets);
  if (FAILED(hr)) {
    return Relation();
  }

  nsTArray<uint64_t> ids;
  for (long idx = 0; idx < nTargets; idx++) {
    IUnknown* target = targets[idx];
    if (auto id = GetIdFor(Document(), target)) {
      ids.AppendElement(*id);
    }
    target->Release();
  }
  CoTaskMemFree(targets);

  return Relation(new RemoteAccIterator(std::move(ids), Document()));
}

double RemoteAccessible::CurValue() const {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::CurValue();
  }

  RefPtr<IAccessibleValue> acc = QueryInterface<IAccessibleValue>(this);
  if (!acc) {
    return UnspecifiedNaN<double>();
  }

  VARIANT currentValue;
  HRESULT hr = acc->get_currentValue(&currentValue);
  if (FAILED(hr) || currentValue.vt != VT_R8) {
    return UnspecifiedNaN<double>();
  }

  return currentValue.dblVal;
}

bool RemoteAccessible::SetCurValue(double aValue) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    // Not yet supported by the cache.
    return false;
  }
  RefPtr<IAccessibleValue> acc = QueryInterface<IAccessibleValue>(this);
  if (!acc) {
    return false;
  }

  VARIANT currentValue;
  VariantInit(&currentValue);
  currentValue.vt = VT_R8;
  currentValue.dblVal = aValue;
  HRESULT hr = acc->setCurrentValue(currentValue);
  return SUCCEEDED(hr);
}

double RemoteAccessible::MinValue() const {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::MinValue();
  }

  RefPtr<IAccessibleValue> acc = QueryInterface<IAccessibleValue>(this);
  if (!acc) {
    return UnspecifiedNaN<double>();
  }

  VARIANT minimumValue;
  HRESULT hr = acc->get_minimumValue(&minimumValue);
  if (FAILED(hr) || minimumValue.vt != VT_R8) {
    return UnspecifiedNaN<double>();
  }

  return minimumValue.dblVal;
}

double RemoteAccessible::MaxValue() const {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::MaxValue();
  }

  RefPtr<IAccessibleValue> acc = QueryInterface<IAccessibleValue>(this);
  if (!acc) {
    return UnspecifiedNaN<double>();
  }

  VARIANT maximumValue;
  HRESULT hr = acc->get_maximumValue(&maximumValue);
  if (FAILED(hr) || maximumValue.vt != VT_R8) {
    return UnspecifiedNaN<double>();
  }

  return maximumValue.dblVal;
}

static IA2TextBoundaryType GetIA2TextBoundary(
    AccessibleTextBoundary aGeckoBoundaryType) {
  switch (aGeckoBoundaryType) {
    case nsIAccessibleText::BOUNDARY_CHAR:
      return IA2_TEXT_BOUNDARY_CHAR;
    case nsIAccessibleText::BOUNDARY_WORD_START:
      return IA2_TEXT_BOUNDARY_WORD;
    case nsIAccessibleText::BOUNDARY_LINE_START:
      return IA2_TEXT_BOUNDARY_LINE;
    case nsIAccessibleText::BOUNDARY_PARAGRAPH:
      return IA2_TEXT_BOUNDARY_PARAGRAPH;
    default:
      MOZ_CRASH();
  }
}

int32_t RemoteAccessible::OffsetAtPoint(int32_t aX, int32_t aY,
                                        uint32_t aCoordinateType) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::OffsetAtPoint(
        aX, aY, aCoordinateType);
  }

  RefPtr<IAccessibleText> acc = QueryInterface<IAccessibleText>(this);
  if (!acc) {
    return -1;
  }

  IA2CoordinateType coordType;
  if (aCoordinateType ==
      nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE) {
    coordType = IA2_COORDTYPE_SCREEN_RELATIVE;
  } else if (aCoordinateType ==
             nsIAccessibleCoordinateType::COORDTYPE_PARENT_RELATIVE) {
    coordType = IA2_COORDTYPE_PARENT_RELATIVE;
  } else {
    MOZ_CRASH("unsupported coord type");
  }

  long offset;
  HRESULT hr = acc->get_offsetAtPoint(
      static_cast<long>(aX), static_cast<long>(aY), coordType, &offset);
  if (FAILED(hr)) {
    return -1;
  }

  return static_cast<int32_t>(offset);
}

void RemoteAccessible::TextSubstring(int32_t aStartOffset, int32_t aEndOffset,
                                     nsAString& aText) const {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::TextSubstring(
        aStartOffset, aEndOffset, aText);
  }

  RefPtr<IAccessibleText> acc = QueryInterface<IAccessibleText>(this);
  if (!acc) {
    return;
  }

  BSTR result;
  HRESULT hr = acc->get_text(static_cast<long>(aStartOffset),
                             static_cast<long>(aEndOffset), &result);
  if (FAILED(hr)) {
    return;
  }

  _bstr_t resultWrap(result, false);
  aText = (wchar_t*)result;
}

void RemoteAccessible::TextBeforeOffset(int32_t aOffset,
                                        AccessibleTextBoundary aBoundaryType,
                                        int32_t* aStartOffset,
                                        int32_t* aEndOffset, nsAString& aText) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::TextBeforeOffset(
        aOffset, aBoundaryType, aStartOffset, aEndOffset, aText);
  }
  RefPtr<IAccessibleText> acc = QueryInterface<IAccessibleText>(this);
  if (!acc) {
    return;
  }

  BSTR result;
  long start, end;
  HRESULT hr = acc->get_textBeforeOffset(
      aOffset, GetIA2TextBoundary(aBoundaryType), &start, &end, &result);
  if (FAILED(hr)) {
    return;
  }

  _bstr_t resultWrap(result, false);
  *aStartOffset = start;
  *aEndOffset = end;
  aText = (wchar_t*)result;
}

void RemoteAccessible::TextAfterOffset(int32_t aOffset,
                                       AccessibleTextBoundary aBoundaryType,
                                       int32_t* aStartOffset,
                                       int32_t* aEndOffset, nsAString& aText) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::TextAfterOffset(
        aOffset, aBoundaryType, aStartOffset, aEndOffset, aText);
  }
  RefPtr<IAccessibleText> acc = QueryInterface<IAccessibleText>(this);
  if (!acc) {
    return;
  }

  BSTR result;
  long start, end;
  HRESULT hr = acc->get_textAfterOffset(
      aOffset, GetIA2TextBoundary(aBoundaryType), &start, &end, &result);
  if (FAILED(hr)) {
    return;
  }

  _bstr_t resultWrap(result, false);
  aText = (wchar_t*)result;
  *aStartOffset = start;
  *aEndOffset = end;
}

void RemoteAccessible::TextAtOffset(int32_t aOffset,
                                    AccessibleTextBoundary aBoundaryType,
                                    int32_t* aStartOffset, int32_t* aEndOffset,
                                    nsAString& aText) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::TextAtOffset(
        aOffset, aBoundaryType, aStartOffset, aEndOffset, aText);
  }
  RefPtr<IAccessibleText> acc = QueryInterface<IAccessibleText>(this);
  if (!acc) {
    return;
  }

  BSTR result;
  long start, end;
  HRESULT hr = acc->get_textAtOffset(aOffset, GetIA2TextBoundary(aBoundaryType),
                                     &start, &end, &result);
  if (FAILED(hr)) {
    return;
  }

  _bstr_t resultWrap(result, false);
  aText = (wchar_t*)result;
  *aStartOffset = start;
  *aEndOffset = end;
}

bool RemoteAccessible::AddToSelection(int32_t aStartOffset,
                                      int32_t aEndOffset) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    // Not yet supported by the cache.
    return false;
  }
  RefPtr<IAccessibleText> acc = QueryInterface<IAccessibleText>(this);
  if (!acc) {
    return false;
  }

  return SUCCEEDED(acc->addSelection(static_cast<long>(aStartOffset),
                                     static_cast<long>(aEndOffset)));
}

bool RemoteAccessible::RemoveFromSelection(int32_t aSelectionNum) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    // Not yet supported by the cache.
    return false;
  }
  RefPtr<IAccessibleText> acc = QueryInterface<IAccessibleText>(this);
  if (!acc) {
    return false;
  }

  return SUCCEEDED(acc->removeSelection(static_cast<long>(aSelectionNum)));
}

int32_t RemoteAccessible::CaretOffset() const {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::CaretOffset();
  }

  RefPtr<IAccessibleText> acc = QueryInterface<IAccessibleText>(this);
  if (!acc) {
    return -1;
  }

  long offset;
  HRESULT hr = acc->get_caretOffset(&offset);
  if (FAILED(hr)) {
    return -1;
  }

  return static_cast<int32_t>(offset);
}

void RemoteAccessible::SetCaretOffset(int32_t aOffset) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::SetCaretOffset(aOffset);
  }

  RefPtr<IAccessibleText> acc = QueryInterface<IAccessibleText>(this);
  if (!acc) {
    return;
  }

  acc->setCaretOffset(static_cast<long>(aOffset));
}

/**
 * aScrollType should be one of the nsIAccessiblescrollType constants.
 */
void RemoteAccessible::ScrollSubstringTo(int32_t aStartOffset,
                                         int32_t aEndOffset,
                                         uint32_t aScrollType) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    // Not yet supported by the cache.
    return;
  }
  RefPtr<IAccessibleText> acc = QueryInterface<IAccessibleText>(this);
  if (!acc) {
    return;
  }

  acc->scrollSubstringTo(static_cast<long>(aStartOffset),
                         static_cast<long>(aEndOffset),
                         static_cast<IA2ScrollType>(aScrollType));
}

/**
 * aCoordinateType is one of the nsIAccessibleCoordinateType constants.
 */
void RemoteAccessible::ScrollSubstringToPoint(int32_t aStartOffset,
                                              int32_t aEndOffset,
                                              uint32_t aCoordinateType,
                                              int32_t aX, int32_t aY) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    // Not yet supported by the cache.
    return;
  }
  RefPtr<IAccessibleText> acc = QueryInterface<IAccessibleText>(this);
  if (!acc) {
    return;
  }

  IA2CoordinateType coordType;
  if (aCoordinateType ==
      nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE) {
    coordType = IA2_COORDTYPE_SCREEN_RELATIVE;
  } else if (aCoordinateType ==
             nsIAccessibleCoordinateType::COORDTYPE_PARENT_RELATIVE) {
    coordType = IA2_COORDTYPE_PARENT_RELATIVE;
  } else {
    MOZ_CRASH("unsupported coord type");
  }

  acc->scrollSubstringToPoint(static_cast<long>(aStartOffset),
                              static_cast<long>(aEndOffset), coordType,
                              static_cast<long>(aX), static_cast<long>(aY));
}

bool RemoteAccessible::IsLinkValid() {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    // Not yet supported by the cache.
    return false;
  }
  RefPtr<IAccessibleHyperlink> acc = QueryInterface<IAccessibleHyperlink>(this);
  if (!acc) {
    return false;
  }

  boolean valid;
  if (FAILED(acc->get_valid(&valid))) {
    return false;
  }

  return valid;
}

uint32_t RemoteAccessible::AnchorCount(bool* aOk) {
  *aOk = false;
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    // Not yet supported by the cache.
    return 0;
  }
  RefPtr<IGeckoCustom> custom = QueryInterface<IGeckoCustom>(this);
  if (!custom) {
    return 0;
  }

  long count;
  if (FAILED(custom->get_anchorCount(&count))) {
    return 0;
  }

  *aOk = true;
  return count;
}

RemoteAccessible* RemoteAccessible::AnchorAt(uint32_t aIdx) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    // Not yet supported by the cache.
    return nullptr;
  }
  RefPtr<IAccessibleHyperlink> link =
      QueryInterface<IAccessibleHyperlink>(this);
  if (!link) {
    return nullptr;
  }

  VARIANT anchor;
  if (FAILED(link->get_anchor(aIdx, &anchor))) {
    return nullptr;
  }

  MOZ_ASSERT(anchor.vt == VT_UNKNOWN);
  RemoteAccessible* proxyAnchor = GetProxyFor(Document(), anchor.punkVal);
  anchor.punkVal->Release();
  return proxyAnchor;
}

void RemoteAccessible::DOMNodeID(nsString& aID) const {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::DOMNodeID(aID);
  }

  aID.Truncate();
  RefPtr<IGeckoCustom> custom = QueryInterface<IGeckoCustom>(this);
  if (!custom) {
    return;
  }

  BSTR result;
  HRESULT hr = custom->get_DOMNodeID(&result);
  _bstr_t resultWrap(result, false);
  if (FAILED(hr)) {
    return;
  }
  aID = (wchar_t*)resultWrap;
}

void RemoteAccessible::TakeFocus() const {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::TakeFocus();
  }
  RefPtr<IAccessible> acc;
  if (!GetCOMInterface((void**)getter_AddRefs(acc))) {
    return;
  }
  acc->accSelect(SELFLAG_TAKEFOCUS, kChildIdSelf);
}

Accessible* RemoteAccessible::ChildAtPoint(
    int32_t aX, int32_t aY, Accessible::EWhichChildAtPoint aWhichChild) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::ChildAtPoint(aX, aY,
                                                                aWhichChild);
  }

  RefPtr<IAccessible2_2> target = QueryInterface<IAccessible2_2>(this);
  if (!target) {
    return nullptr;
  }
  DocAccessibleParent* doc = Document();
  RemoteAccessible* proxy = this;
  // accHitTest only does direct children, but we might want the deepest child.
  for (;;) {
    VARIANT childVar;
    if (FAILED(target->accHitTest(aX, aY, &childVar)) ||
        childVar.vt == VT_EMPTY) {
      return nullptr;
    }
    if (childVar.vt == VT_I4 && childVar.lVal == CHILDID_SELF) {
      break;
    }
    MOZ_ASSERT(childVar.vt == VT_DISPATCH && childVar.pdispVal);
    target = nullptr;
    childVar.pdispVal->QueryInterface(IID_IAccessible2_2,
                                      getter_AddRefs(target));
    childVar.pdispVal->Release();
    if (!target) {
      return nullptr;
    }
    // We can't always use GetProxyFor because it can't cross document
    // boundaries.
    if (proxy->ChildCount() == 1) {
      proxy = proxy->RemoteChildAt(0);
      if (proxy->IsDoc()) {
        // We're crossing into a child document.
        doc = proxy->AsDoc();
      }
    } else {
      proxy = GetProxyFor(doc, target);
    }
    if (aWhichChild == Accessible::EWhichChildAtPoint::DirectChild) {
      break;
    }
  }
  return proxy;
}

GroupPos RemoteAccessible::GroupPosition() {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return RemoteAccessibleBase<RemoteAccessible>::GroupPosition();
  }

  // This is only supported when cache is enabled.
  return GroupPos();
}

}  // namespace a11y
}  // namespace mozilla
