/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LocalAccessible-inl.h"
#include "mozilla/a11y/DocAccessibleParent.h"
#include "mozilla/StaticPrefs_accessibility.h"
#include "AccAttributes.h"
#include "nsAccUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsIAccessibleRelation.h"
#include "nsIAccessibleRole.h"
#include "nsAccessibleRelation.h"
#include "Relation.h"
#include "Role.h"
#include "RootAccessible.h"
#include "xpcAccessibleDocument.h"

#include "nsIMutableArray.h"
#include "nsPersistentProperties.h"

#ifdef MOZ_WIDGET_COCOA
#  include "xpcAccessibleMacInterface.h"
#endif

using namespace mozilla::a11y;

NS_IMETHODIMP
xpcAccessible::GetParent(nsIAccessible** aParent) {
  NS_ENSURE_ARG_POINTER(aParent);
  *aParent = nullptr;
  if (!IntlGeneric()) return NS_ERROR_FAILURE;

  Accessible* parent = IntlGeneric()->Parent();
  NS_IF_ADDREF(*aParent = ToXPC(parent));
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetNextSibling(nsIAccessible** aNextSibling) {
  NS_ENSURE_ARG_POINTER(aNextSibling);
  *aNextSibling = nullptr;
  if (!IntlGeneric()) return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aNextSibling = ToXPC(IntlGeneric()->NextSibling()));

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetPreviousSibling(nsIAccessible** aPreviousSibling) {
  NS_ENSURE_ARG_POINTER(aPreviousSibling);
  *aPreviousSibling = nullptr;
  if (!IntlGeneric()) return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aPreviousSibling = ToXPC(IntlGeneric()->PrevSibling()));

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetFirstChild(nsIAccessible** aFirstChild) {
  NS_ENSURE_ARG_POINTER(aFirstChild);
  *aFirstChild = nullptr;

  if (!IntlGeneric()) return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aFirstChild = ToXPC(IntlGeneric()->FirstChild()));
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetLastChild(nsIAccessible** aLastChild) {
  NS_ENSURE_ARG_POINTER(aLastChild);
  *aLastChild = nullptr;

  if (!IntlGeneric()) return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aLastChild = ToXPC(IntlGeneric()->LastChild()));
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetChildCount(int32_t* aChildCount) {
  NS_ENSURE_ARG_POINTER(aChildCount);

  if (!IntlGeneric()) return NS_ERROR_FAILURE;

  *aChildCount = IntlGeneric()->ChildCount();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetChildAt(int32_t aChildIndex, nsIAccessible** aChild) {
  NS_ENSURE_ARG_POINTER(aChild);
  *aChild = nullptr;

  if (!IntlGeneric()) return NS_ERROR_FAILURE;

  // If child index is negative, then return last child.
  // XXX: do we really need this?
  if (aChildIndex < 0) aChildIndex = IntlGeneric()->ChildCount() - 1;

  Accessible* child = IntlGeneric()->ChildAt(aChildIndex);
  if (!child) return NS_ERROR_INVALID_ARG;

  NS_ADDREF(*aChild = ToXPC(child));
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetChildren(nsIArray** aChildren) {
  NS_ENSURE_ARG_POINTER(aChildren);
  *aChildren = nullptr;

  if (!IntlGeneric()) return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  nsCOMPtr<nsIMutableArray> children =
      do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t childCount = IntlGeneric()->ChildCount();
  for (uint32_t childIdx = 0; childIdx < childCount; childIdx++) {
    Accessible* child = IntlGeneric()->ChildAt(childIdx);
    children->AppendElement(static_cast<nsIAccessible*>(ToXPC(child)));
  }

  children.forget(aChildren);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetIndexInParent(int32_t* aIndexInParent) {
  NS_ENSURE_ARG_POINTER(aIndexInParent);
  *aIndexInParent = -1;
  if (!IntlGeneric()) return NS_ERROR_FAILURE;

  *aIndexInParent = IntlGeneric()->IndexInParent();

  return *aIndexInParent != -1 ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
xpcAccessible::GetUniqueID(int64_t* aUniqueID) {
  NS_ENSURE_ARG_POINTER(aUniqueID);

  if (!IntlGeneric()) return NS_ERROR_FAILURE;

  if (IntlGeneric()->IsLocal()) {
    *aUniqueID = reinterpret_cast<uintptr_t>(Intl()->UniqueID());
  } else if (IntlGeneric()->IsRemote()) {
    *aUniqueID = IntlGeneric()->AsRemote()->ID();
  }

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetDOMNode(nsINode** aDOMNode) {
  NS_ENSURE_ARG_POINTER(aDOMNode);
  *aDOMNode = nullptr;

  if (!Intl()) return NS_ERROR_FAILURE;

  nsCOMPtr<nsINode> node = Intl()->GetNode();
  node.forget(aDOMNode);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetId(nsAString& aID) {
  if (!IntlGeneric()) {
    return NS_ERROR_FAILURE;
  }

  RemoteAccessible* proxy = IntlGeneric()->AsRemote();
  if (!proxy) {
    return NS_ERROR_FAILURE;
  }

  nsString id;
  proxy->DOMNodeID(id);
  aID.Assign(id);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetDocument(nsIAccessibleDocument** aDocument) {
  NS_ENSURE_ARG_POINTER(aDocument);
  *aDocument = nullptr;

  if (!Intl()) return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aDocument = ToXPCDocument(Intl()->Document()));
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetRootDocument(nsIAccessibleDocument** aRootDocument) {
  NS_ENSURE_ARG_POINTER(aRootDocument);
  *aRootDocument = nullptr;

  if (!Intl()) return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aRootDocument = ToXPCDocument(Intl()->RootAccessible()));
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetRole(uint32_t* aRole) {
  NS_ENSURE_ARG_POINTER(aRole);
  *aRole = nsIAccessibleRole::ROLE_NOTHING;

  if (!IntlGeneric()) return NS_ERROR_FAILURE;

  *aRole = IntlGeneric()->Role();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetState(uint32_t* aState, uint32_t* aExtraState) {
  NS_ENSURE_ARG_POINTER(aState);

  Accessible* acc = IntlGeneric();
  if (acc) {
    nsAccUtils::To32States(acc->State(), aState, aExtraState);
  } else {
    nsAccUtils::To32States(states::DEFUNCT, aState, aExtraState);
  }

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetName(nsAString& aName) {
  aName.Truncate();

  if (!IntlGeneric()) return NS_ERROR_FAILURE;

  nsAutoString name;
  IntlGeneric()->Name(name);

  aName.Assign(name);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetDescription(nsAString& aDescription) {
  if (!IntlGeneric()) return NS_ERROR_FAILURE;

  nsAutoString desc;
  IntlGeneric()->Description(desc);

  aDescription.Assign(desc);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetLanguage(nsAString& aLanguage) {
  if (!IntlGeneric()) return NS_ERROR_FAILURE;

  nsAutoString lang;
  if (RemoteAccessible* proxy = IntlGeneric()->AsRemote()) {
    proxy->Language(lang);
  } else {
    Intl()->Language(lang);
  }

  aLanguage.Assign(lang);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetValue(nsAString& aValue) {
  if (!IntlGeneric()) return NS_ERROR_FAILURE;

  nsAutoString value;
  IntlGeneric()->Value(value);

  aValue.Assign(value);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetHelp(nsAString& aHelp) {
  if (!IntlGeneric()) return NS_ERROR_FAILURE;

  nsAutoString help;
  if (RemoteAccessible* proxy = IntlGeneric()->AsRemote()) {
#if defined(XP_WIN)
    return NS_ERROR_NOT_IMPLEMENTED;
#else
    proxy->Help(help);
#endif
  } else {
    Intl()->Help(help);
  }

  aHelp.Assign(help);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetAccessKey(nsAString& aAccessKey) {
  aAccessKey.Truncate();

  if (!IntlGeneric()) return NS_ERROR_FAILURE;

#if defined(XP_WIN)
  if (IntlGeneric()->IsRemote() &&
      !StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
#endif

  IntlGeneric()->AccessKey().ToString(aAccessKey);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetKeyboardShortcut(nsAString& aKeyBinding) {
  aKeyBinding.Truncate();
  if (!IntlGeneric()) return NS_ERROR_FAILURE;

  if (IntlGeneric()->IsRemote()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  Intl()->KeyboardShortcut().ToString(aKeyBinding);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetAttributes(nsIPersistentProperties** aAttributes) {
  NS_ENSURE_ARG_POINTER(aAttributes);
  *aAttributes = nullptr;

  if (!IntlGeneric()) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<nsPersistentProperties> props = new nsPersistentProperties();

  RefPtr<AccAttributes> attributes = IntlGeneric()->Attributes();

  nsAutoString unused;
  for (auto iter : *attributes) {
    nsAutoString name;
    iter.NameAsString(name);

    nsAutoString value;
    iter.ValueAsString(value);

    props->SetStringProperty(NS_ConvertUTF16toUTF8(name), value, unused);
  }

  props.forget(aAttributes);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetCache(nsIPersistentProperties** aCachedFields) {
  NS_ENSURE_ARG_POINTER(aCachedFields);
  *aCachedFields = nullptr;

  if (!IntlGeneric()) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<nsPersistentProperties> props = new nsPersistentProperties();
  if (IntlGeneric()->IsRemote()) {
    RefPtr<AccAttributes> cachedFields =
        IntlGeneric()->AsRemote()->mCachedFields;

    nsAutoString unused;
    for (auto iter : *cachedFields) {
      nsAutoString name;
      iter.NameAsString(name);

      nsAutoString value;
      iter.ValueAsString(value);

      props->SetStringProperty(NS_ConvertUTF16toUTF8(name), value, unused);
    }
  }

  props.forget(aCachedFields);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetNativeInterface(nsISupports** aNativeInterface) {
#ifdef MOZ_WIDGET_COCOA
  NS_ENSURE_ARG_POINTER(aNativeInterface);

  // We don't cache or store this instance anywhere so each get returns a
  // different instance. So `acc.nativeInterface != acc.nativeInterface`. This
  // just seems simpler and more robust for now.
  nsCOMPtr<nsISupports> macIface = static_cast<nsIAccessibleMacInterface*>(
      new xpcAccessibleMacInterface(IntlGeneric()));
  macIface.swap(*aNativeInterface);

  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
xpcAccessible::GetBounds(int32_t* aX, int32_t* aY, int32_t* aWidth,
                         int32_t* aHeight) {
  NS_ENSURE_ARG_POINTER(aX);
  *aX = 0;
  NS_ENSURE_ARG_POINTER(aY);
  *aY = 0;
  NS_ENSURE_ARG_POINTER(aWidth);
  *aWidth = 0;
  NS_ENSURE_ARG_POINTER(aHeight);
  *aHeight = 0;

  if (!IntlGeneric()) return NS_ERROR_FAILURE;

  LayoutDeviceIntRect rect = IntlGeneric()->Bounds();
  rect.GetRect(aX, aY, aWidth, aHeight);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetBoundsInCSSPixels(int32_t* aX, int32_t* aY, int32_t* aWidth,
                                    int32_t* aHeight) {
  NS_ENSURE_ARG_POINTER(aX);
  *aX = 0;
  NS_ENSURE_ARG_POINTER(aY);
  *aY = 0;
  NS_ENSURE_ARG_POINTER(aWidth);
  *aWidth = 0;
  NS_ENSURE_ARG_POINTER(aHeight);
  *aHeight = 0;

  if (!IntlGeneric()) {
    return NS_ERROR_FAILURE;
  }

  nsIntRect rect = IntlGeneric()->BoundsInCSSPixels();
  rect.GetRect(aX, aY, aWidth, aHeight);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GroupPosition(int32_t* aGroupLevel,
                             int32_t* aSimilarItemsInGroup,
                             int32_t* aPositionInGroup) {
  NS_ENSURE_ARG_POINTER(aGroupLevel);
  NS_ENSURE_ARG_POINTER(aSimilarItemsInGroup);
  NS_ENSURE_ARG_POINTER(aPositionInGroup);

#if defined(XP_WIN)
  if (IntlGeneric()->IsRemote() &&
      !StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    *aGroupLevel = 0;
    *aSimilarItemsInGroup = 0;
    *aPositionInGroup = 0;
    return NS_ERROR_NOT_IMPLEMENTED;
  }
#endif

  GroupPos groupPos = IntlGeneric()->GroupPosition();

  *aGroupLevel = groupPos.level;
  *aSimilarItemsInGroup = groupPos.setSize;
  *aPositionInGroup = groupPos.posInSet;

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetRelationByType(uint32_t aType,
                                 nsIAccessibleRelation** aRelation) {
  NS_ENSURE_ARG_POINTER(aRelation);
  *aRelation = nullptr;

  NS_ENSURE_ARG(aType <= static_cast<uint32_t>(RelationType::LAST));

  if (!IntlGeneric()) return NS_ERROR_FAILURE;

  Relation rel =
      IntlGeneric()->RelationByType(static_cast<RelationType>(aType));
  NS_ADDREF(*aRelation = new nsAccessibleRelation(aType, &rel));
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetRelations(nsIArray** aRelations) {
  NS_ENSURE_ARG_POINTER(aRelations);
  *aRelations = nullptr;

  if (!IntlGeneric()) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIMutableArray> relations = do_CreateInstance(NS_ARRAY_CONTRACTID);
  NS_ENSURE_TRUE(relations, NS_ERROR_OUT_OF_MEMORY);

  static const uint32_t relationTypes[] = {
      nsIAccessibleRelation::RELATION_LABELLED_BY,
      nsIAccessibleRelation::RELATION_LABEL_FOR,
      nsIAccessibleRelation::RELATION_DESCRIBED_BY,
      nsIAccessibleRelation::RELATION_DESCRIPTION_FOR,
      nsIAccessibleRelation::RELATION_NODE_CHILD_OF,
      nsIAccessibleRelation::RELATION_NODE_PARENT_OF,
      nsIAccessibleRelation::RELATION_CONTROLLED_BY,
      nsIAccessibleRelation::RELATION_CONTROLLER_FOR,
      nsIAccessibleRelation::RELATION_FLOWS_TO,
      nsIAccessibleRelation::RELATION_FLOWS_FROM,
      nsIAccessibleRelation::RELATION_MEMBER_OF,
      nsIAccessibleRelation::RELATION_SUBWINDOW_OF,
      nsIAccessibleRelation::RELATION_EMBEDS,
      nsIAccessibleRelation::RELATION_EMBEDDED_BY,
      nsIAccessibleRelation::RELATION_POPUP_FOR,
      nsIAccessibleRelation::RELATION_PARENT_WINDOW_OF,
      nsIAccessibleRelation::RELATION_DEFAULT_BUTTON,
      nsIAccessibleRelation::RELATION_CONTAINING_DOCUMENT,
      nsIAccessibleRelation::RELATION_CONTAINING_TAB_PANE,
      nsIAccessibleRelation::RELATION_CONTAINING_APPLICATION};

  for (uint32_t idx = 0; idx < ArrayLength(relationTypes); idx++) {
    nsCOMPtr<nsIAccessibleRelation> relation;
    nsresult rv =
        GetRelationByType(relationTypes[idx], getter_AddRefs(relation));

    if (NS_SUCCEEDED(rv) && relation) {
      uint32_t targets = 0;
      relation->GetTargetsCount(&targets);
      if (targets) relations->AppendElement(relation);
    }
  }

  NS_ADDREF(*aRelations = relations);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetFocusedChild(nsIAccessible** aChild) {
  NS_ENSURE_ARG_POINTER(aChild);
  *aChild = nullptr;

  if (!IntlGeneric()) return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aChild = ToXPC(IntlGeneric()->FocusedChild()));

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetChildAtPoint(int32_t aX, int32_t aY,
                               nsIAccessible** aAccessible) {
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nullptr;

  if (!IntlGeneric()) return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aAccessible = ToXPC(IntlGeneric()->ChildAtPoint(
                   aX, aY, Accessible::EWhichChildAtPoint::DirectChild)));

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetDeepestChildAtPoint(int32_t aX, int32_t aY,
                                      nsIAccessible** aAccessible) {
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nullptr;

  if (!IntlGeneric()) return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aAccessible = ToXPC(IntlGeneric()->ChildAtPoint(
                   aX, aY, Accessible::EWhichChildAtPoint::DeepestChild)));

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetDeepestChildAtPointInProcess(int32_t aX, int32_t aY,
                                               nsIAccessible** aAccessible) {
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nullptr;

  Accessible* generic = IntlGeneric();
  if (!generic || generic->IsRemote()) {
    return NS_ERROR_FAILURE;
  }

  NS_IF_ADDREF(*aAccessible = ToXPC(Intl()->LocalChildAtPoint(
                   aX, aY, Accessible::EWhichChildAtPoint::DeepestChild)));
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::SetSelected(bool aSelect) {
  if (!IntlGeneric()) return NS_ERROR_FAILURE;

#if defined(XP_WIN)
  if (IntlGeneric()->IsRemote() &&
      !StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
#endif

  IntlGeneric()->SetSelected(aSelect);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::TakeSelection() {
  if (!IntlGeneric()) return NS_ERROR_FAILURE;

#if defined(XP_WIN)
  if (IntlGeneric()->IsRemote() &&
      !StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
#endif

  IntlGeneric()->TakeSelection();

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::TakeFocus() {
  if (!IntlGeneric()) return NS_ERROR_FAILURE;

  IntlGeneric()->TakeFocus();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetActionCount(uint8_t* aActionCount) {
  NS_ENSURE_ARG_POINTER(aActionCount);
  *aActionCount = 0;
  if (!IntlGeneric()) return NS_ERROR_FAILURE;

#if defined(XP_WIN)
  if (IntlGeneric()->IsRemote() &&
      !StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
#endif

  *aActionCount = IntlGeneric()->ActionCount();

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetActionName(uint8_t aIndex, nsAString& aName) {
#if defined(XP_WIN)
  if (IntlGeneric()->IsRemote() &&
      !StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
#endif

  aName.Truncate();

  if (!IntlGeneric()) {
    return NS_ERROR_FAILURE;
  }

  if (aIndex >= IntlGeneric()->ActionCount()) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoString name;
  IntlGeneric()->ActionNameAt(aIndex, name);

  aName.Assign(name);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::GetActionDescription(uint8_t aIndex, nsAString& aDescription) {
#if defined(XP_WIN)
  if (IntlGeneric()->IsRemote() &&
      !StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
#endif

  aDescription.Truncate();

  if (!IntlGeneric()) {
    return NS_ERROR_FAILURE;
  }

  if (aIndex >= IntlGeneric()->ActionCount()) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoString description;
  IntlGeneric()->ActionDescriptionAt(aIndex, description);

  aDescription.Assign(description);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::DoAction(uint8_t aIndex) {
  if (!IntlGeneric()) return NS_ERROR_FAILURE;

#if defined(XP_WIN)
  if (IntlGeneric()->IsRemote() &&
      !StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
#endif

  return IntlGeneric()->DoAction(aIndex) ? NS_OK : NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
xpcAccessible::ScrollTo(uint32_t aHow) {
  if (!IntlGeneric()) return NS_ERROR_FAILURE;

  IntlGeneric()->ScrollTo(aHow);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::ScrollToPoint(uint32_t aCoordinateType, int32_t aX, int32_t aY) {
  if (!IntlGeneric()) return NS_ERROR_FAILURE;

  if (RemoteAccessible* proxy = IntlGeneric()->AsRemote()) {
#if defined(XP_WIN)
    return NS_ERROR_NOT_IMPLEMENTED;
#else
    proxy->ScrollToPoint(aCoordinateType, aX, aY);
#endif
  } else {
    Intl()->ScrollToPoint(aCoordinateType, aX, aY);
  }

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessible::Announce(const nsAString& aAnnouncement, uint16_t aPriority) {
  if (RemoteAccessible* proxy = IntlGeneric()->AsRemote()) {
#if defined(XP_WIN)
    return NS_ERROR_NOT_IMPLEMENTED;
#else
    nsString announcement(aAnnouncement);
    proxy->Announce(announcement, aPriority);
#endif
  } else {
    Intl()->Announce(aAnnouncement, aPriority);
  }

  return NS_OK;
}
