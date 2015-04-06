/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessibleWrap.h"

#include "Accessible2_i.c"
#include "Accessible2_2_i.c"
#include "AccessibleRole.h"
#include "AccessibleStates.h"

#include "Compatibility.h"
#include "ia2AccessibleRelation.h"
#include "IUnknownImpl.h"
#include "nsCoreUtils.h"
#include "nsIAccessibleTypes.h"
#include "mozilla/a11y/PDocAccessible.h"
#include "Relation.h"

#include "nsIPersistentProperties2.h"
#include "nsISimpleEnumerator.h"

using namespace mozilla;
using namespace mozilla::a11y;

template<typename String> static void EscapeAttributeChars(String& aStr);

////////////////////////////////////////////////////////////////////////////////
// ia2Accessible
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP
ia2Accessible::QueryInterface(REFIID iid, void** ppv)
{
  if (!ppv)
    return E_INVALIDARG;

  *ppv = nullptr;

  if (IID_IAccessible2_2 == iid)
    *ppv = static_cast<IAccessible2_2*>(this);
  else if (IID_IAccessible2 == iid && !Compatibility::IsIA2Off())
    *ppv = static_cast<IAccessible2*>(this);

  if (*ppv) {
    (reinterpret_cast<IUnknown*>(*ppv))->AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

////////////////////////////////////////////////////////////////////////////////
// IAccessible2

STDMETHODIMP
ia2Accessible::get_nRelations(long* aNRelations)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aNRelations)
    return E_INVALIDARG;
  *aNRelations = 0;

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  if (acc->IsProxy()) {
    // XXX evaluate performance of collecting all relation targets.
    nsTArray<RelationType> types;
    nsTArray<nsTArray<ProxyAccessible*>> targetSets;
    acc->Proxy()->Relations(&types, &targetSets);
    *aNRelations = types.Length();
    return S_OK;
  }

  for (uint32_t idx = 0; idx < ArrayLength(sRelationTypePairs); idx++) {
    if (sRelationTypePairs[idx].second == IA2_RELATION_NULL)
      continue;

    Relation rel = acc->RelationByType(sRelationTypePairs[idx].first);
    if (rel.Next())
      (*aNRelations)++;
  }
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2Accessible::get_relation(long aRelationIndex,
                            IAccessibleRelation** aRelation)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aRelation || aRelationIndex < 0)
    return E_INVALIDARG;
  *aRelation = nullptr;

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  if (acc->IsProxy()) {
    nsTArray<RelationType> types;
    nsTArray<nsTArray<ProxyAccessible*>> targetSets;
    acc->Proxy()->Relations(&types, &targetSets);

    size_t targetSetCount = targetSets.Length();
    for (size_t i = 0; i < targetSetCount; i++) {
      uint32_t relTypeIdx = static_cast<uint32_t>(types[i]);
      MOZ_ASSERT(sRelationTypePairs[relTypeIdx].first == types[i]);
      if (sRelationTypePairs[relTypeIdx].second == IA2_RELATION_NULL)
        continue;

      if (static_cast<size_t>(aRelationIndex) == i) {
        nsTArray<nsRefPtr<Accessible>> targets;
        size_t targetCount = targetSets[i].Length();
        for (size_t j = 0; j < targetCount; j++)
          targets.AppendElement(WrapperFor(targetSets[i][j]));

        nsRefPtr<ia2AccessibleRelation> rel =
          new ia2AccessibleRelation(types[i], Move(targets));
        rel.forget(aRelation);
        return S_OK;
      }
    }

    return E_INVALIDARG;
  }

  long relIdx = 0;
  for (uint32_t idx = 0; idx < ArrayLength(sRelationTypePairs); idx++) {
    if (sRelationTypePairs[idx].second == IA2_RELATION_NULL)
      continue;

    RelationType relationType = sRelationTypePairs[idx].first;
    Relation rel = acc->RelationByType(relationType);
    nsRefPtr<ia2AccessibleRelation> ia2Relation =
      new ia2AccessibleRelation(relationType, &rel);
    if (ia2Relation->HasTargets()) {
      if (relIdx == aRelationIndex) {
        ia2Relation.forget(aRelation);
        return S_OK;
      }

      relIdx++;
    }
  }

  return E_INVALIDARG;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2Accessible::get_relations(long aMaxRelations,
                             IAccessibleRelation** aRelation,
                             long *aNRelations)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aRelation || !aNRelations || aMaxRelations <= 0)
    return E_INVALIDARG;
  *aNRelations = 0;

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  if (acc->IsProxy()) {
    nsTArray<RelationType> types;
    nsTArray<nsTArray<ProxyAccessible*>> targetSets;
    acc->Proxy()->Relations(&types, &targetSets);

    size_t count = std::min(targetSets.Length(),
                            static_cast<size_t>(aMaxRelations));
    size_t i = 0;
    while (i < count) {
      uint32_t relTypeIdx = static_cast<uint32_t>(types[i]);
      if (sRelationTypePairs[relTypeIdx].second == IA2_RELATION_NULL)
        continue;

      size_t targetCount = targetSets[i].Length();
      nsTArray<nsRefPtr<Accessible>> targets(targetCount);
      for (size_t j = 0; j < targetCount; j++)
        targets.AppendElement(WrapperFor(targetSets[i][j]));

      nsRefPtr<ia2AccessibleRelation> rel =
        new ia2AccessibleRelation(types[i], Move(targets));
      rel.forget(aRelation + i);
      i++;
    }

    *aNRelations = i;
    return S_OK;
  }

  for (uint32_t idx = 0; idx < ArrayLength(sRelationTypePairs) &&
       *aNRelations < aMaxRelations; idx++) {
    if (sRelationTypePairs[idx].second == IA2_RELATION_NULL)
      continue;

    RelationType relationType = sRelationTypePairs[idx].first;
    Relation rel = acc->RelationByType(relationType);
    nsRefPtr<ia2AccessibleRelation> ia2Rel =
      new ia2AccessibleRelation(relationType, &rel);
    if (ia2Rel->HasTargets()) {
      ia2Rel.forget(aRelation + (*aNRelations));
      (*aNRelations)++;
    }
  }
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2Accessible::role(long* aRole)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aRole)
    return E_INVALIDARG;
  *aRole = 0;

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

#define ROLE(_geckoRole, stringRole, atkRole, macRole, \
             msaaRole, ia2Role, nameRule) \
  case roles::_geckoRole: \
    *aRole = ia2Role; \
    break;

  a11y::role geckoRole;
  if (acc->IsProxy())
    geckoRole = acc->Proxy()->Role();
  else
    geckoRole = acc->Role();
  switch (geckoRole) {
#include "RoleMap.h"
    default:
      MOZ_CRASH("Unknown role.");
  };

#undef ROLE

  // Special case, if there is a ROLE_ROW inside of a ROLE_TREE_TABLE, then call
  // the IA2 role a ROLE_OUTLINEITEM.
  if (acc->IsProxy()) {
    if (geckoRole == roles::ROW && acc->Proxy()->Parent() &&
        acc->Proxy()->Parent()->Role() == roles::TREE_TABLE)
      *aRole = ROLE_SYSTEM_OUTLINEITEM;
  } else {
    if (geckoRole == roles::ROW) {
      Accessible* xpParent = acc->Parent();
      if (xpParent && xpParent->Role() == roles::TREE_TABLE)
        *aRole = ROLE_SYSTEM_OUTLINEITEM;
    }
  }

  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2Accessible::scrollTo(enum IA2ScrollType aScrollType)
{
  A11Y_TRYBLOCK_BEGIN

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsCoreUtils::ScrollTo(acc->Document()->PresShell(),
                        acc->GetContent(), aScrollType);
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2Accessible::scrollToPoint(enum IA2CoordinateType aCoordType,
                              long aX, long aY)
{
  A11Y_TRYBLOCK_BEGIN

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  uint32_t geckoCoordType = (aCoordType == IA2_COORDTYPE_SCREEN_RELATIVE) ?
    nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE :
    nsIAccessibleCoordinateType::COORDTYPE_PARENT_RELATIVE;

  acc->ScrollToPoint(geckoCoordType, aX, aY);
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2Accessible::get_groupPosition(long* aGroupLevel,
                                 long* aSimilarItemsInGroup,
                                 long* aPositionInGroup)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aGroupLevel || !aSimilarItemsInGroup || !aPositionInGroup)
    return E_INVALIDARG;

  *aGroupLevel = 0;
  *aSimilarItemsInGroup = 0;
  *aPositionInGroup = 0;

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  GroupPos groupPos = acc->GroupPosition();

  // Group information for accessibles having level only (like html headings
  // elements) isn't exposed by this method. AT should look for 'level' object
  // attribute.
  if (!groupPos.setSize && !groupPos.posInSet)
    return S_FALSE;

  *aGroupLevel = groupPos.level;
  *aSimilarItemsInGroup = groupPos.setSize;
  *aPositionInGroup = groupPos.posInSet;

  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2Accessible::get_states(AccessibleStates* aStates)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aStates)
    return E_INVALIDARG;
  *aStates = 0;

  // XXX: bug 344674 should come with better approach that we have here.

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct()) {
    *aStates = IA2_STATE_DEFUNCT;
    return CO_E_OBJNOTCONNECTED;
  }

  uint64_t state;
  if (acc->IsProxy())
    state = acc->Proxy()->State();
  else
    state = acc->State();

  if (state & states::INVALID)
    *aStates |= IA2_STATE_INVALID_ENTRY;
  if (state & states::REQUIRED)
    *aStates |= IA2_STATE_REQUIRED;

  // The following IA2 states are not supported by Gecko
  // IA2_STATE_ARMED
  // IA2_STATE_MANAGES_DESCENDANTS
  // IA2_STATE_ICONIFIED
  // IA2_STATE_INVALID // This is not a state, it is the absence of a state

  if (state & states::ACTIVE)
    *aStates |= IA2_STATE_ACTIVE;
  if (state & states::DEFUNCT)
    *aStates |= IA2_STATE_DEFUNCT;
  if (state & states::EDITABLE)
    *aStates |= IA2_STATE_EDITABLE;
  if (state & states::HORIZONTAL)
    *aStates |= IA2_STATE_HORIZONTAL;
  if (state & states::MODAL)
    *aStates |= IA2_STATE_MODAL;
  if (state & states::MULTI_LINE)
    *aStates |= IA2_STATE_MULTI_LINE;
  if (state & states::OPAQUE1)
    *aStates |= IA2_STATE_OPAQUE;
  if (state & states::SELECTABLE_TEXT)
    *aStates |= IA2_STATE_SELECTABLE_TEXT;
  if (state & states::SINGLE_LINE)
    *aStates |= IA2_STATE_SINGLE_LINE;
  if (state & states::STALE)
    *aStates |= IA2_STATE_STALE;
  if (state & states::SUPPORTS_AUTOCOMPLETION)
    *aStates |= IA2_STATE_SUPPORTS_AUTOCOMPLETION;
  if (state & states::TRANSIENT)
    *aStates |= IA2_STATE_TRANSIENT;
  if (state & states::VERTICAL)
    *aStates |= IA2_STATE_VERTICAL;
  if (state & states::CHECKED)
    *aStates |= IA2_STATE_CHECKABLE;
  if (state & states::PINNED)
    *aStates |= IA2_STATE_PINNED;

  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2Accessible::get_extendedRole(BSTR* aExtendedRole)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aExtendedRole)
    return E_INVALIDARG;

  *aExtendedRole = nullptr;
  return E_NOTIMPL;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2Accessible::get_localizedExtendedRole(BSTR* aLocalizedExtendedRole)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aLocalizedExtendedRole)
    return E_INVALIDARG;

  *aLocalizedExtendedRole = nullptr;
  return E_NOTIMPL;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2Accessible::get_nExtendedStates(long* aNExtendedStates)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aNExtendedStates)
    return E_INVALIDARG;

  *aNExtendedStates = 0;
  return E_NOTIMPL;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2Accessible::get_extendedStates(long aMaxExtendedStates,
                                  BSTR** aExtendedStates,
                                  long* aNExtendedStates)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aExtendedStates || !aNExtendedStates)
    return E_INVALIDARG;

  *aExtendedStates = nullptr;
  *aNExtendedStates = 0;
  return E_NOTIMPL;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2Accessible::get_localizedExtendedStates(long aMaxLocalizedExtendedStates,
                                           BSTR** aLocalizedExtendedStates,
                                           long* aNLocalizedExtendedStates)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aLocalizedExtendedStates || !aNLocalizedExtendedStates)
    return E_INVALIDARG;

  *aLocalizedExtendedStates = nullptr;
  *aNLocalizedExtendedStates = 0;
  return E_NOTIMPL;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2Accessible::get_uniqueID(long* aUniqueID)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aUniqueID)
    return E_INVALIDARG;

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  *aUniqueID = - reinterpret_cast<intptr_t>(acc->UniqueID());
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2Accessible::get_windowHandle(HWND* aWindowHandle)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aWindowHandle)
    return E_INVALIDARG;
  *aWindowHandle = 0;

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  *aWindowHandle = AccessibleWrap::GetHWNDFor(acc);
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2Accessible::get_indexInParent(long* aIndexInParent)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aIndexInParent)
    return E_INVALIDARG;
  *aIndexInParent = -1;

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  if (acc->IsProxy())
    *aIndexInParent = acc->Proxy()->IndexInParent();
  else
    *aIndexInParent = acc->IndexInParent();

  if (*aIndexInParent == -1)
    return S_FALSE;

  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2Accessible::get_locale(IA2Locale* aLocale)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aLocale)
    return E_INVALIDARG;

  // Language codes consist of a primary code and a possibly empty series of
  // subcodes: language-code = primary-code ( "-" subcode )*
  // Two-letter primary codes are reserved for [ISO639] language abbreviations.
  // Any two-letter subcode is understood to be a [ISO3166] country code.

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsAutoString lang;
  acc->Language(lang);

  // If primary code consists from two letters then expose it as language.
  int32_t offset = lang.FindChar('-', 0);
  if (offset == -1) {
    if (lang.Length() == 2) {
      aLocale->language = ::SysAllocString(lang.get());
      return S_OK;
    }
  } else if (offset == 2) {
    aLocale->language = ::SysAllocStringLen(lang.get(), 2);

    // If the first subcode consists from two letters then expose it as
    // country.
    offset = lang.FindChar('-', 3);
    if (offset == -1) {
      if (lang.Length() == 5) {
        aLocale->country = ::SysAllocString(lang.get() + 3);
        return S_OK;
      }
    } else if (offset == 5) {
      aLocale->country = ::SysAllocStringLen(lang.get() + 3, 2);
    }
  }

  // Expose as a string if primary code or subcode cannot point to language or
  // country abbreviations or if there are more than one subcode.
  aLocale->variant = ::SysAllocString(lang.get());
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2Accessible::get_attributes(BSTR* aAttributes)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aAttributes)
    return E_INVALIDARG;
  *aAttributes = nullptr;

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  // The format is name:value;name:value; with \ for escaping these
  // characters ":;=,\".
  if (!acc->IsProxy()) {
    nsCOMPtr<nsIPersistentProperties> attributes = acc->Attributes();
    return ConvertToIA2Attributes(attributes, aAttributes);
  }

  nsTArray<Attribute> attrs;
  acc->Proxy()->Attributes(&attrs);
  return ConvertToIA2Attributes(&attrs, aAttributes);

  A11Y_TRYBLOCK_END
}

////////////////////////////////////////////////////////////////////////////////
// IAccessible2_2

STDMETHODIMP
ia2Accessible::get_attribute(BSTR name, VARIANT* aAttribute)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aAttribute)
    return E_INVALIDARG;

  return E_NOTIMPL;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2Accessible::get_accessibleWithCaret(IUnknown** aAccessible,
                                       long* aCaretOffset)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aAccessible || !aCaretOffset)
    return E_INVALIDARG;

  *aAccessible = nullptr;
  *aCaretOffset = -1;
  return E_NOTIMPL;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2Accessible::get_relationTargetsOfType(BSTR aType,
                                         long aMaxTargets,
                                         IUnknown*** aTargets,
                                         long* aNTargets)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aTargets || !aNTargets || aMaxTargets < 0)
    return E_INVALIDARG;
  *aNTargets = 0;

  Maybe<RelationType> relationType;
  for (uint32_t idx = 0; idx < ArrayLength(sRelationTypePairs); idx++) {
    if (wcscmp(aType, sRelationTypePairs[idx].second) == 0) {
      relationType.emplace(sRelationTypePairs[idx].first);
      break;
    }
  }
  if (!relationType)
    return E_INVALIDARG;

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsTArray<Accessible*> targets;
  if (acc->IsProxy()) {
    nsTArray<ProxyAccessible*> targetProxies =
      acc->Proxy()->RelationByType(*relationType);

    size_t targetCount = aMaxTargets;
    if (targetProxies.Length() < targetCount)
      targetCount = targetProxies.Length();
    for (size_t i = 0; i < targetCount; i++)
      targets.AppendElement(WrapperFor(targetProxies[i]));
  } else {
    Relation rel = acc->RelationByType(*relationType);
    Accessible* target = nullptr;
    while ((target = rel.Next()) &&
           static_cast<long>(targets.Length()) <= aMaxTargets)
      targets.AppendElement(target);
  }

  *aNTargets = targets.Length();
  *aTargets = static_cast<IUnknown**>(
    ::CoTaskMemAlloc(sizeof(IUnknown*) * *aNTargets));
  if (!*aTargets)
    return E_OUTOFMEMORY;

  for (int32_t i = 0; i < *aNTargets; i++) {
    AccessibleWrap* target= static_cast<AccessibleWrap*>(targets[i]);
    (*aTargets)[i] = static_cast<IAccessible2*>(target);
    (*aTargets)[i]->AddRef();
  }

  return S_OK;

  A11Y_TRYBLOCK_END
}

////////////////////////////////////////////////////////////////////////////////
// Helpers

template<typename String>
static inline void
EscapeAttributeChars(String& aStr)
{
  int32_t offset = 0;
  static const char kCharsToEscape[] = ":;=,\\";
  while ((offset = aStr.FindCharInSet(kCharsToEscape, offset)) != kNotFound) {
    aStr.Insert('\\', offset);
    offset += 2;
  }
}

HRESULT
ia2Accessible::ConvertToIA2Attributes(nsTArray<Attribute>* aAttributes,
                                      BSTR* aIA2Attributes)
{
  nsString attrStr;
  size_t attrCount = aAttributes->Length();
  for (size_t i = 0; i < attrCount; i++) {
    EscapeAttributeChars(aAttributes->ElementAt(i).Name());
    EscapeAttributeChars(aAttributes->ElementAt(i).Value());
    AppendUTF8toUTF16(aAttributes->ElementAt(i).Name(), attrStr);
    attrStr.Append(':');
    attrStr.Append(aAttributes->ElementAt(i).Value());
    attrStr.Append(';');
  }

  if (attrStr.IsEmpty())
    return S_FALSE;

  *aIA2Attributes = ::SysAllocStringLen(attrStr.get(), attrStr.Length());
  return *aIA2Attributes ? S_OK : E_OUTOFMEMORY;
}

HRESULT
ia2Accessible::ConvertToIA2Attributes(nsIPersistentProperties* aAttributes,
                                      BSTR* aIA2Attributes)
{
  *aIA2Attributes = nullptr;

  // The format is name:value;name:value; with \ for escaping these
  // characters ":;=,\".

  if (!aAttributes)
    return S_FALSE;

  nsCOMPtr<nsISimpleEnumerator> propEnum;
  aAttributes->Enumerate(getter_AddRefs(propEnum));
  if (!propEnum)
    return E_FAIL;

  nsAutoString strAttrs;

  bool hasMore = false;
  while (NS_SUCCEEDED(propEnum->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> propSupports;
    propEnum->GetNext(getter_AddRefs(propSupports));

    nsCOMPtr<nsIPropertyElement> propElem(do_QueryInterface(propSupports));
    if (!propElem)
      return E_FAIL;

    nsAutoCString name;
    if (NS_FAILED(propElem->GetKey(name)))
      return E_FAIL;

    EscapeAttributeChars(name);

    nsAutoString value;
    if (NS_FAILED(propElem->GetValue(value)))
      return E_FAIL;

    EscapeAttributeChars(value);

    AppendUTF8toUTF16(name, strAttrs);
    strAttrs.Append(':');
    strAttrs.Append(value);
    strAttrs.Append(';');
  }

  if (strAttrs.IsEmpty())
    return S_FALSE;

  *aIA2Attributes = ::SysAllocStringLen(strAttrs.get(), strAttrs.Length());
  return *aIA2Attributes ? S_OK : E_OUTOFMEMORY;
}
