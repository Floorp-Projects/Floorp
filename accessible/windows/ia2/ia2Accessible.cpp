/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessibleWrap.h"

#include "Accessible2_i.c"
#include "Accessible2_2_i.c"
#include "Accessible2_3_i.c"
#include "AccessibleRole.h"
#include "AccessibleStates.h"

#include "AccAttributes.h"
#include "Compatibility.h"
#include "ia2AccessibleRelation.h"
#include "IUnknownImpl.h"
#include "nsCoreUtils.h"
#include "nsIAccessibleTypes.h"
#include "mozilla/a11y/PDocAccessible.h"
#include "Relation.h"
#include "TextRange-inl.h"
#include "nsAccessibilityService.h"

#include "mozilla/PresShell.h"
#include "nsISimpleEnumerator.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// ia2Accessible
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP
ia2Accessible::QueryInterface(REFIID iid, void** ppv) {
  if (!ppv) return E_INVALIDARG;

  *ppv = nullptr;

  // NOTE: If any new versions of IAccessible2 are added here, they should
  // also be added to the IA2 Handler in
  // /accessible/ipc/win/handler/AccessibleHandler.cpp

  if (IID_IAccessible2_3 == iid)
    *ppv = static_cast<IAccessible2_3*>(this);
  else if (IID_IAccessible2_2 == iid)
    *ppv = static_cast<IAccessible2_2*>(this);
  else if (IID_IAccessible2 == iid)
    *ppv = static_cast<IAccessible2*>(this);

  if (*ppv) {
    (reinterpret_cast<IUnknown*>(*ppv))->AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

AccessibleWrap* ia2Accessible::LocalAcc() {
  return static_cast<MsaaAccessible*>(this)->LocalAcc();
}

Accessible* ia2Accessible::Acc() {
  return static_cast<MsaaAccessible*>(this)->Acc();
}

////////////////////////////////////////////////////////////////////////////////
// IAccessible2

STDMETHODIMP
ia2Accessible::get_nRelations(long* aNRelations) {
  if (!aNRelations) return E_INVALIDARG;
  *aNRelations = 0;

  Accessible* acc = Acc();
  if (!acc) {
    return CO_E_OBJNOTCONNECTED;
  }

  for (uint32_t idx = 0; idx < ArrayLength(sRelationTypePairs); idx++) {
    if (sRelationTypePairs[idx].second == IA2_RELATION_NULL) continue;

    Relation rel = acc->RelationByType(sRelationTypePairs[idx].first);
    if (rel.Next()) (*aNRelations)++;
  }
  return S_OK;
}

STDMETHODIMP
ia2Accessible::get_relation(long aRelationIndex,
                            IAccessibleRelation** aRelation) {
  if (!aRelation || aRelationIndex < 0) return E_INVALIDARG;
  *aRelation = nullptr;

  Accessible* acc = Acc();
  if (!acc) {
    return CO_E_OBJNOTCONNECTED;
  }

  long relIdx = 0;
  for (uint32_t idx = 0; idx < ArrayLength(sRelationTypePairs); idx++) {
    if (sRelationTypePairs[idx].second == IA2_RELATION_NULL) continue;

    RelationType relationType = sRelationTypePairs[idx].first;
    Relation rel = acc->RelationByType(relationType);
    RefPtr<ia2AccessibleRelation> ia2Relation =
        new ia2AccessibleRelation(relationType, &rel);
    if (ia2Relation->HasTargets()) {
      if (relIdx == aRelationIndex) {
        MsaaAccessible* msaa = static_cast<MsaaAccessible*>(this);
        msaa->AssociateCOMObjectForDisconnection(ia2Relation);
        ia2Relation.forget(aRelation);
        return S_OK;
      }

      relIdx++;
    }
  }

  return E_INVALIDARG;
}

STDMETHODIMP
ia2Accessible::get_relations(long aMaxRelations,
                             IAccessibleRelation** aRelation,
                             long* aNRelations) {
  if (!aRelation || !aNRelations || aMaxRelations <= 0) return E_INVALIDARG;
  *aNRelations = 0;

  Accessible* acc = Acc();
  if (!acc) {
    return CO_E_OBJNOTCONNECTED;
  }

  for (uint32_t idx = 0;
       idx < ArrayLength(sRelationTypePairs) && *aNRelations < aMaxRelations;
       idx++) {
    if (sRelationTypePairs[idx].second == IA2_RELATION_NULL) continue;

    RelationType relationType = sRelationTypePairs[idx].first;
    Relation rel = acc->RelationByType(relationType);
    RefPtr<ia2AccessibleRelation> ia2Rel =
        new ia2AccessibleRelation(relationType, &rel);
    if (ia2Rel->HasTargets()) {
      MsaaAccessible* msaa = static_cast<MsaaAccessible*>(this);
      msaa->AssociateCOMObjectForDisconnection(ia2Rel);
      ia2Rel.forget(aRelation + (*aNRelations));
      (*aNRelations)++;
    }
  }
  return S_OK;
}

STDMETHODIMP
ia2Accessible::role(long* aRole) {
  if (!aRole) return E_INVALIDARG;
  *aRole = 0;

  Accessible* acc = Acc();
  if (!acc) return CO_E_OBJNOTCONNECTED;

#define ROLE(_geckoRole, stringRole, atkRole, macRole, macSubrole, msaaRole, \
             ia2Role, androidClass, nameRule)                                \
  case roles::_geckoRole:                                                    \
    *aRole = ia2Role;                                                        \
    break;

  a11y::role geckoRole;
  geckoRole = acc->Role();
  switch (geckoRole) {
#include "RoleMap.h"
    default:
      MOZ_CRASH("Unknown role.");
  }

#undef ROLE

  // Special case, if there is a ROLE_ROW inside of a ROLE_TREE_TABLE, then call
  // the IA2 role a ROLE_OUTLINEITEM.
  if (geckoRole == roles::ROW) {
    Accessible* xpParent = acc->Parent();
    if (xpParent && xpParent->Role() == roles::TREE_TABLE)
      *aRole = ROLE_SYSTEM_OUTLINEITEM;
  }

  return S_OK;
}

// XXX Use MOZ_CAN_RUN_SCRIPT_BOUNDARY for now due to bug 1543294.
MOZ_CAN_RUN_SCRIPT_BOUNDARY STDMETHODIMP
ia2Accessible::scrollTo(enum IA2ScrollType aScrollType) {
  Accessible* acc = Acc();
  if (!acc) {
    return CO_E_OBJNOTCONNECTED;
  }

  acc->ScrollTo(aScrollType);
  return S_OK;
}

STDMETHODIMP
ia2Accessible::scrollToPoint(enum IA2CoordinateType aCoordType, long aX,
                             long aY) {
  if (!Acc()) {
    return CO_E_OBJNOTCONNECTED;
  }
  AccessibleWrap* acc = LocalAcc();
  if (!acc) {
    return E_NOTIMPL;  // XXX Not supported for RemoteAccessible yet.
  }

  uint32_t geckoCoordType =
      (aCoordType == IA2_COORDTYPE_SCREEN_RELATIVE)
          ? nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE
          : nsIAccessibleCoordinateType::COORDTYPE_PARENT_RELATIVE;

  acc->ScrollToPoint(geckoCoordType, aX, aY);

  return S_OK;
}

STDMETHODIMP
ia2Accessible::get_groupPosition(long* aGroupLevel, long* aSimilarItemsInGroup,
                                 long* aPositionInGroup) {
  if (!aGroupLevel || !aSimilarItemsInGroup || !aPositionInGroup)
    return E_INVALIDARG;

  *aGroupLevel = 0;
  *aSimilarItemsInGroup = 0;
  *aPositionInGroup = 0;

  Accessible* acc = Acc();
  if (!acc) {
    return CO_E_OBJNOTCONNECTED;
  }

  GroupPos groupPos = acc->GroupPosition();

  // Group information for accessibles having level only (like html headings
  // elements) isn't exposed by this method. AT should look for 'level' object
  // attribute.
  if (!groupPos.setSize && !groupPos.posInSet) return S_FALSE;

  *aGroupLevel = groupPos.level;
  *aSimilarItemsInGroup = groupPos.setSize;
  *aPositionInGroup = groupPos.posInSet;

  return S_OK;
}

STDMETHODIMP
ia2Accessible::get_states(AccessibleStates* aStates) {
  if (!aStates) return E_INVALIDARG;
  *aStates = 0;

  // XXX: bug 344674 should come with better approach that we have here.

  Accessible* acc = Acc();
  if (!acc) {
    *aStates = IA2_STATE_DEFUNCT;
    return S_OK;
  }

  uint64_t state;
  state = acc->State();

  if (state & states::INVALID) *aStates |= IA2_STATE_INVALID_ENTRY;
  if (state & states::REQUIRED) *aStates |= IA2_STATE_REQUIRED;

  // The following IA2 states are not supported by Gecko
  // IA2_STATE_ARMED
  // IA2_STATE_MANAGES_DESCENDANTS
  // IA2_STATE_ICONIFIED
  // IA2_STATE_INVALID // This is not a state, it is the absence of a state

  if (state & states::ACTIVE) *aStates |= IA2_STATE_ACTIVE;
  if (state & states::DEFUNCT) *aStates |= IA2_STATE_DEFUNCT;
  if (state & states::EDITABLE) *aStates |= IA2_STATE_EDITABLE;
  if (state & states::HORIZONTAL) *aStates |= IA2_STATE_HORIZONTAL;
  if (state & states::MODAL) *aStates |= IA2_STATE_MODAL;
  if (state & states::MULTI_LINE) *aStates |= IA2_STATE_MULTI_LINE;
  if (state & states::OPAQUE1) *aStates |= IA2_STATE_OPAQUE;
  if (state & states::SELECTABLE_TEXT) *aStates |= IA2_STATE_SELECTABLE_TEXT;
  if (state & states::SINGLE_LINE) *aStates |= IA2_STATE_SINGLE_LINE;
  if (state & states::STALE) *aStates |= IA2_STATE_STALE;
  if (state & states::SUPPORTS_AUTOCOMPLETION)
    *aStates |= IA2_STATE_SUPPORTS_AUTOCOMPLETION;
  if (state & states::TRANSIENT) *aStates |= IA2_STATE_TRANSIENT;
  if (state & states::VERTICAL) *aStates |= IA2_STATE_VERTICAL;
  if (state & states::CHECKED) *aStates |= IA2_STATE_CHECKABLE;
  if (state & states::PINNED) *aStates |= IA2_STATE_PINNED;

  return S_OK;
}

STDMETHODIMP
ia2Accessible::get_extendedRole(BSTR* aExtendedRole) {
  if (!aExtendedRole) return E_INVALIDARG;

  *aExtendedRole = nullptr;
  return E_NOTIMPL;
}

STDMETHODIMP
ia2Accessible::get_localizedExtendedRole(BSTR* aLocalizedExtendedRole) {
  if (!aLocalizedExtendedRole) return E_INVALIDARG;

  *aLocalizedExtendedRole = nullptr;
  return E_NOTIMPL;
}

STDMETHODIMP
ia2Accessible::get_nExtendedStates(long* aNExtendedStates) {
  if (!aNExtendedStates) return E_INVALIDARG;

  *aNExtendedStates = 0;
  return E_NOTIMPL;
}

STDMETHODIMP
ia2Accessible::get_extendedStates(long aMaxExtendedStates,
                                  BSTR** aExtendedStates,
                                  long* aNExtendedStates) {
  if (!aExtendedStates || !aNExtendedStates) return E_INVALIDARG;

  *aExtendedStates = nullptr;
  *aNExtendedStates = 0;
  return E_NOTIMPL;
}

STDMETHODIMP
ia2Accessible::get_localizedExtendedStates(long aMaxLocalizedExtendedStates,
                                           BSTR** aLocalizedExtendedStates,
                                           long* aNLocalizedExtendedStates) {
  if (!aLocalizedExtendedStates || !aNLocalizedExtendedStates)
    return E_INVALIDARG;

  *aLocalizedExtendedStates = nullptr;
  *aNLocalizedExtendedStates = 0;
  return E_NOTIMPL;
}

STDMETHODIMP
ia2Accessible::get_uniqueID(long* aUniqueID) {
  if (!aUniqueID) return E_INVALIDARG;

  Accessible* acc = Acc();
  *aUniqueID = MsaaAccessible::GetChildIDFor(acc);
  return S_OK;
}

STDMETHODIMP
ia2Accessible::get_windowHandle(HWND* aWindowHandle) {
  if (!aWindowHandle) return E_INVALIDARG;
  *aWindowHandle = 0;

  Accessible* acc = Acc();
  if (!acc) return CO_E_OBJNOTCONNECTED;

  *aWindowHandle = MsaaAccessible::GetHWNDFor(acc);
  return S_OK;
}

STDMETHODIMP
ia2Accessible::get_indexInParent(long* aIndexInParent) {
  if (!aIndexInParent) return E_INVALIDARG;
  *aIndexInParent = -1;

  Accessible* acc = Acc();
  if (!acc) return CO_E_OBJNOTCONNECTED;

  *aIndexInParent = acc->IndexInParent();

  if (*aIndexInParent == -1) return S_FALSE;

  return S_OK;
}

STDMETHODIMP
ia2Accessible::get_locale(IA2Locale* aLocale) {
  if (!aLocale) return E_INVALIDARG;

  // Language codes consist of a primary code and a possibly empty series of
  // subcodes: language-code = primary-code ( "-" subcode )*
  // Two-letter primary codes are reserved for [ISO639] language abbreviations.
  // Any two-letter subcode is understood to be a [ISO3166] country code.

  if (!Acc()) {
    return CO_E_OBJNOTCONNECTED;
  }
  AccessibleWrap* acc = LocalAcc();
  if (!acc) {
    return E_NOTIMPL;  // XXX Not supported for RemoteAccessible yet.
  }

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
}

STDMETHODIMP
ia2Accessible::get_attributes(BSTR* aAttributes) {
  if (!aAttributes) return E_INVALIDARG;
  *aAttributes = nullptr;

  Accessible* acc = Acc();
  if (!acc) {
    return CO_E_OBJNOTCONNECTED;
  }

  // The format is name:value;name:value; with \ for escaping these
  // characters ":;=,\".
  RefPtr<AccAttributes> attributes = acc->Attributes();
  return ConvertToIA2Attributes(attributes, aAttributes);
}

////////////////////////////////////////////////////////////////////////////////
// IAccessible2_2

STDMETHODIMP
ia2Accessible::get_attribute(BSTR name, VARIANT* aAttribute) {
  if (!aAttribute) return E_INVALIDARG;

  return E_NOTIMPL;
}

STDMETHODIMP
ia2Accessible::get_accessibleWithCaret(IUnknown** aAccessible,
                                       long* aCaretOffset) {
  if (!aAccessible || !aCaretOffset) return E_INVALIDARG;

  *aAccessible = nullptr;
  *aCaretOffset = -1;

  if (!Acc()) {
    return CO_E_OBJNOTCONNECTED;
  }
  AccessibleWrap* acc = LocalAcc();
  if (!acc) {
    return E_NOTIMPL;  // XXX Not supported for RemoteAccessible yet.
  }

  int32_t caretOffset = -1;
  LocalAccessible* accWithCaret =
      SelectionMgr()->AccessibleWithCaret(&caretOffset);
  if (!accWithCaret || acc->Document() != accWithCaret->Document())
    return S_FALSE;

  LocalAccessible* child = accWithCaret;
  while (!child->IsDoc() && child != acc) child = child->LocalParent();

  if (child != acc) return S_FALSE;

  RefPtr<IAccessible2> ia2WithCaret;
  accWithCaret->GetNativeInterface(getter_AddRefs(ia2WithCaret));
  ia2WithCaret.forget(aAccessible);
  *aCaretOffset = caretOffset;
  return S_OK;
}

STDMETHODIMP
ia2Accessible::get_relationTargetsOfType(BSTR aType, long aMaxTargets,
                                         IUnknown*** aTargets,
                                         long* aNTargets) {
  if (!aTargets || !aNTargets || aMaxTargets < 0) return E_INVALIDARG;
  *aNTargets = 0;

  Maybe<RelationType> relationType;
  for (uint32_t idx = 0; idx < ArrayLength(sRelationTypePairs); idx++) {
    if (wcscmp(aType, sRelationTypePairs[idx].second) == 0) {
      relationType.emplace(sRelationTypePairs[idx].first);
      break;
    }
  }
  if (!relationType) return E_INVALIDARG;

  Accessible* acc = Acc();
  if (!acc) {
    return CO_E_OBJNOTCONNECTED;
  }

  nsTArray<Accessible*> targets;
  Relation rel = acc->RelationByType(*relationType);
  Accessible* target = nullptr;
  while (
      (target = rel.Next()) &&
      (aMaxTargets == 0 || static_cast<long>(targets.Length()) < aMaxTargets)) {
    targets.AppendElement(target);
  }

  *aNTargets = targets.Length();
  *aTargets =
      static_cast<IUnknown**>(::CoTaskMemAlloc(sizeof(IUnknown*) * *aNTargets));
  if (!*aTargets) return E_OUTOFMEMORY;

  for (int32_t i = 0; i < *aNTargets; i++) {
    (*aTargets)[i] = MsaaAccessible::NativeAccessible(targets[i]);
  }

  return S_OK;
}

STDMETHODIMP
ia2Accessible::get_selectionRanges(IA2Range** aRanges, long* aNRanges) {
  if (!aRanges || !aNRanges) return E_INVALIDARG;

  *aNRanges = 0;

  if (!Acc()) {
    return CO_E_OBJNOTCONNECTED;
  }
  AccessibleWrap* acc = LocalAcc();
  if (!acc) {
    return E_NOTIMPL;  // XXX Not supported for RemoteAccessible yet.
  }

  AutoTArray<TextRange, 1> ranges;
  acc->Document()->SelectionRanges(&ranges);
  ranges.RemoveElementsBy([acc](auto& range) { return !range.Crop(acc); });

  *aNRanges = ranges.Length();
  *aRanges =
      static_cast<IA2Range*>(::CoTaskMemAlloc(sizeof(IA2Range) * *aNRanges));
  if (!*aRanges) return E_OUTOFMEMORY;

  for (uint32_t idx = 0; idx < static_cast<uint32_t>(*aNRanges); idx++) {
    RefPtr<IAccessible2> anchor =
        MsaaAccessible::GetFrom(ranges[idx].StartContainer());
    anchor.forget(&(*aRanges)[idx].anchor);

    (*aRanges)[idx].anchorOffset = ranges[idx].StartOffset();

    RefPtr<IAccessible2> active =
        MsaaAccessible::GetFrom(ranges[idx].EndContainer());
    active.forget(&(*aRanges)[idx].active);

    (*aRanges)[idx].activeOffset = ranges[idx].EndOffset();
  }

  return S_OK;
}

////////////////////////////////////////////////////////////////////////////////
// Helpers

static inline void EscapeAttributeChars(nsString& aStr) {
  int32_t offset = 0;
  static const char16_t kCharsToEscape[] = u":;=,\\";
  while ((offset = aStr.FindCharInSet(kCharsToEscape, offset)) != kNotFound) {
    aStr.Insert('\\', offset);
    offset += 2;
  }
}

HRESULT
ia2Accessible::ConvertToIA2Attributes(AccAttributes* aAttributes,
                                      BSTR* aIA2Attributes) {
  *aIA2Attributes = nullptr;

  // The format is name:value;name:value; with \ for escaping these
  // characters ":;=,\".

  if (!aAttributes) return S_FALSE;

  nsAutoString strAttrs;

  for (auto iter : *aAttributes) {
    nsAutoString name;
    iter.NameAsString(name);
    EscapeAttributeChars(name);

    nsAutoString value;
    iter.ValueAsString(value);
    EscapeAttributeChars(value);

    strAttrs.Append(name);
    strAttrs.Append(':');
    strAttrs.Append(value);
    strAttrs.Append(';');
  }

  if (strAttrs.IsEmpty()) return S_FALSE;

  *aIA2Attributes = ::SysAllocStringLen(strAttrs.get(), strAttrs.Length());
  return *aIA2Attributes ? S_OK : E_OUTOFMEMORY;
}
