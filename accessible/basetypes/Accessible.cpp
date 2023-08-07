/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Accessible.h"
#include "ARIAMap.h"
#include "nsAccUtils.h"
#include "nsIURI.h"
#include "Relation.h"
#include "States.h"
#include "mozilla/a11y/FocusManager.h"
#include "mozilla/a11y/HyperTextAccessibleBase.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/Components.h"
#include "nsIStringBundle.h"

#ifdef A11Y_LOG
#  include "nsAccessibilityService.h"
#endif

using namespace mozilla;
using namespace mozilla::a11y;

Accessible::Accessible()
    : mType(static_cast<uint32_t>(0)),
      mGenericTypes(static_cast<uint32_t>(0)),
      mRoleMapEntryIndex(aria::NO_ROLE_MAP_ENTRY_INDEX) {}

Accessible::Accessible(AccType aType, AccGenericType aGenericTypes,
                       uint8_t aRoleMapEntryIndex)
    : mType(static_cast<uint32_t>(aType)),
      mGenericTypes(static_cast<uint32_t>(aGenericTypes)),
      mRoleMapEntryIndex(aRoleMapEntryIndex) {}

void Accessible::StaticAsserts() const {
  static_assert(eLastAccType <= (1 << kTypeBits) - 1,
                "Accessible::mType was oversized by eLastAccType!");
  static_assert(
      eLastAccGenericType <= (1 << kGenericTypesBits) - 1,
      "Accessible::mGenericType was oversized by eLastAccGenericType!");
}

bool Accessible::IsBefore(const Accessible* aAcc) const {
  // Build the chain of parents.
  const Accessible* thisP = this;
  const Accessible* otherP = aAcc;
  AutoTArray<const Accessible*, 30> thisParents, otherParents;
  do {
    thisParents.AppendElement(thisP);
    thisP = thisP->Parent();
  } while (thisP);
  do {
    otherParents.AppendElement(otherP);
    otherP = otherP->Parent();
  } while (otherP);

  // Find where the parent chain differs.
  uint32_t thisPos = thisParents.Length(), otherPos = otherParents.Length();
  for (uint32_t len = std::min(thisPos, otherPos); len > 0; --len) {
    const Accessible* thisChild = thisParents.ElementAt(--thisPos);
    const Accessible* otherChild = otherParents.ElementAt(--otherPos);
    if (thisChild != otherChild) {
      return thisChild->IndexInParent() < otherChild->IndexInParent();
    }
  }

  // If the ancestries are the same length (both thisPos and otherPos are 0),
  // we should have returned by now.
  MOZ_ASSERT(thisPos != 0 || otherPos != 0);
  // At this point, one of the ancestries is a superset of the other, so one of
  // thisPos or otherPos should be 0.
  MOZ_ASSERT(thisPos != otherPos);
  // If the other Accessible is deeper than this one (otherPos > 0), this
  // Accessible comes before the other.
  return otherPos > 0;
}

Accessible* Accessible::FocusedChild() {
  Accessible* doc = nsAccUtils::DocumentFor(this);
  Accessible* child = doc->FocusedChild();
  if (child && (child == this || child->Parent() == this)) {
    return child;
  }

  return nullptr;
}

const nsRoleMapEntry* Accessible::ARIARoleMap() const {
  return aria::GetRoleMapFromIndex(mRoleMapEntryIndex);
}

bool Accessible::HasARIARole() const {
  return mRoleMapEntryIndex != aria::NO_ROLE_MAP_ENTRY_INDEX;
}

bool Accessible::IsARIARole(nsAtom* aARIARole) const {
  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  return roleMapEntry && roleMapEntry->Is(aARIARole);
}

bool Accessible::HasStrongARIARole() const {
  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  return roleMapEntry && roleMapEntry->roleRule == kUseMapRole;
}

bool Accessible::HasGenericType(AccGenericType aType) const {
  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  return (mGenericTypes & aType) ||
         (roleMapEntry && roleMapEntry->IsOfType(aType));
}

nsIntRect Accessible::BoundsInCSSPixels() const {
  return BoundsInAppUnits().ToNearestPixels(AppUnitsPerCSSPixel());
}

LayoutDeviceIntSize Accessible::Size() const { return Bounds().Size(); }

LayoutDeviceIntPoint Accessible::Position(uint32_t aCoordType) {
  LayoutDeviceIntPoint point = Bounds().TopLeft();
  nsAccUtils::ConvertScreenCoordsTo(&point.x.value, &point.y.value, aCoordType,
                                    this);
  return point;
}

bool Accessible::IsTextRole() {
  if (!IsHyperText()) {
    return false;
  }

  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  if (roleMapEntry && (roleMapEntry->role == roles::GRAPHIC ||
                       roleMapEntry->role == roles::IMAGE_MAP ||
                       roleMapEntry->role == roles::SLIDER ||
                       roleMapEntry->role == roles::PROGRESSBAR ||
                       roleMapEntry->role == roles::SEPARATOR)) {
    return false;
  }

  return true;
}

uint32_t Accessible::StartOffset() {
  MOZ_ASSERT(IsLink(), "StartOffset is called not on hyper link!");
  Accessible* parent = Parent();
  HyperTextAccessibleBase* hyperText =
      parent ? parent->AsHyperTextBase() : nullptr;
  return hyperText ? hyperText->GetChildOffset(this) : 0;
}

uint32_t Accessible::EndOffset() {
  MOZ_ASSERT(IsLink(), "EndOffset is called on not hyper link!");
  Accessible* parent = Parent();
  HyperTextAccessibleBase* hyperText =
      parent ? parent->AsHyperTextBase() : nullptr;
  return hyperText ? (hyperText->GetChildOffset(this) + 1) : 0;
}

GroupPos Accessible::GroupPosition() {
  GroupPos groupPos;

  // Try aria-row/colcount/index.
  if (IsTableRow()) {
    Accessible* table = nsAccUtils::TableFor(this);
    if (table) {
      if (auto count = table->GetIntARIAAttr(nsGkAtoms::aria_rowcount)) {
        if (*count >= 0) {
          groupPos.setSize = *count;
        }
      }
    }
    if (auto index = GetIntARIAAttr(nsGkAtoms::aria_rowindex)) {
      groupPos.posInSet = *index;
    }
    if (groupPos.setSize && groupPos.posInSet) {
      return groupPos;
    }
  }
  if (IsTableCell()) {
    Accessible* table;
    for (table = Parent(); table; table = table->Parent()) {
      if (table->IsTable()) {
        break;
      }
    }
    if (table) {
      if (auto count = table->GetIntARIAAttr(nsGkAtoms::aria_colcount)) {
        if (*count >= 0) {
          groupPos.setSize = *count;
        }
      }
    }
    if (auto index = GetIntARIAAttr(nsGkAtoms::aria_colindex)) {
      groupPos.posInSet = *index;
    }
    if (groupPos.setSize && groupPos.posInSet) {
      return groupPos;
    }
  }

  // Get group position from ARIA attributes.
  ARIAGroupPosition(&groupPos.level, &groupPos.setSize, &groupPos.posInSet);

  // If ARIA is missed and the accessible is visible then calculate group
  // position from hierarchy.
  if (State() & states::INVISIBLE) return groupPos;

  // Calculate group level if ARIA is missed.
  if (groupPos.level == 0) {
    groupPos.level = GetLevel(false);
  }

  // Calculate position in group and group size if ARIA is missed.
  if (groupPos.posInSet == 0 || groupPos.setSize == 0) {
    int32_t posInSet = 0, setSize = 0;
    GetPositionAndSetSize(&posInSet, &setSize);
    if (posInSet != 0 && setSize != 0) {
      if (groupPos.posInSet == 0) groupPos.posInSet = posInSet;

      if (groupPos.setSize == 0) groupPos.setSize = setSize;
    }
  }

  return groupPos;
}

int32_t Accessible::GetLevel(bool aFast) const {
  int32_t level = 0;
  if (!Parent()) return level;

  roles::Role role = Role();
  if (role == roles::OUTLINEITEM) {
    // Always expose 'level' attribute for 'outlineitem' accessible. The number
    // of nested 'grouping' accessibles containing 'outlineitem' accessible is
    // its level.
    level = 1;

    if (!aFast) {
      const Accessible* parent = this;
      while ((parent = parent->Parent()) && !parent->IsDoc()) {
        roles::Role parentRole = parent->Role();

        if (parentRole == roles::OUTLINE) break;
        if (parentRole == roles::GROUPING) ++level;
      }
    }
  } else if (role == roles::LISTITEM && !aFast) {
    // Expose 'level' attribute on nested lists. We support two hierarchies:
    // a) list -> listitem -> list -> listitem (nested list is a last child
    //   of listitem of the parent list);
    // b) list -> listitem -> group -> listitem (nested listitems are contained
    //   by group that is a last child of the parent listitem).

    // Calculate 'level' attribute based on number of parent listitems.
    level = 0;
    const Accessible* parent = this;
    while ((parent = parent->Parent()) && !parent->IsDoc()) {
      roles::Role parentRole = parent->Role();

      if (parentRole == roles::LISTITEM) {
        ++level;
      } else if (parentRole != roles::LIST && parentRole != roles::GROUPING) {
        break;
      }
    }

    if (level == 0) {
      // If this listitem is on top of nested lists then expose 'level'
      // attribute.
      parent = Parent();
      uint32_t siblingCount = parent->ChildCount();
      for (uint32_t siblingIdx = 0; siblingIdx < siblingCount; siblingIdx++) {
        Accessible* sibling = parent->ChildAt(siblingIdx);

        Accessible* siblingChild = sibling->LastChild();
        if (siblingChild) {
          roles::Role lastChildRole = siblingChild->Role();
          if (lastChildRole == roles::LIST ||
              lastChildRole == roles::GROUPING) {
            return 1;
          }
        }
      }
    } else {
      ++level;  // level is 1-index based
    }
  } else if (role == roles::OPTION || role == roles::COMBOBOX_OPTION) {
    if (const Accessible* parent = Parent()) {
      if (parent->IsHTMLOptGroup()) {
        return 2;
      }

      if (parent->IsListControl() && !parent->ARIARoleMap()) {
        // This is for HTML selects only.
        if (aFast) {
          return 1;
        }

        for (uint32_t i = 0, count = parent->ChildCount(); i < count; ++i) {
          if (parent->ChildAt(i)->IsHTMLOptGroup()) {
            return 1;
          }
        }
      }
    }
  } else if (role == roles::HEADING) {
    nsAtom* tagName = TagName();
    if (tagName == nsGkAtoms::h1) {
      return 1;
    }
    if (tagName == nsGkAtoms::h2) {
      return 2;
    }
    if (tagName == nsGkAtoms::h3) {
      return 3;
    }
    if (tagName == nsGkAtoms::h4) {
      return 4;
    }
    if (tagName == nsGkAtoms::h5) {
      return 5;
    }
    if (tagName == nsGkAtoms::h6) {
      return 6;
    }

    const nsRoleMapEntry* ariaRole = this->ARIARoleMap();
    if (ariaRole && ariaRole->Is(nsGkAtoms::heading)) {
      // An aria heading with no aria level has a default level of 2.
      return 2;
    }
  } else if (role == roles::COMMENT) {
    // For comments, count the ancestor elements with the same role to get the
    // level.
    level = 1;

    if (!aFast) {
      const Accessible* parent = this;
      while ((parent = parent->Parent()) && !parent->IsDoc()) {
        roles::Role parentRole = parent->Role();
        if (parentRole == roles::COMMENT) {
          ++level;
        }
      }
    }
  } else if (role == roles::ROW) {
    // It is a row inside flatten treegrid. Group level is always 1 until it
    // is overriden by aria-level attribute.
    const Accessible* parent = Parent();
    if (parent->Role() == roles::TREE_TABLE) {
      return 1;
    }
  }

  return level;
}

void Accessible::GetPositionAndSetSize(int32_t* aPosInSet, int32_t* aSetSize) {
  auto groupInfo = GetOrCreateGroupInfo();
  if (groupInfo) {
    *aPosInSet = groupInfo->PosInSet();
    *aSetSize = groupInfo->SetSize();
  }
}

bool Accessible::IsLinkValid() {
  MOZ_ASSERT(IsLink(), "IsLinkValid is called on not hyper link!");

  // XXX In order to implement this we would need to follow every link
  // Perhaps we can get information about invalid links from the cache
  // In the mean time authors can use role="link" aria-invalid="true"
  // to force it for links they internally know to be invalid
  return (0 == (State() & mozilla::a11y::states::INVALID));
}

uint32_t Accessible::AnchorCount() {
  if (IsImageMap()) {
    return ChildCount();
  }

  MOZ_ASSERT(IsLink(), "AnchorCount is called on not hyper link!");
  return 1;
}

Accessible* Accessible::AnchorAt(uint32_t aAnchorIndex) const {
  if (IsImageMap()) {
    return ChildAt(aAnchorIndex);
  }

  MOZ_ASSERT(IsLink(), "GetAnchor is called on not hyper link!");
  return aAnchorIndex == 0 ? const_cast<Accessible*>(this) : nullptr;
}

already_AddRefed<nsIURI> Accessible::AnchorURIAt(uint32_t aAnchorIndex) const {
  Accessible* anchor = nullptr;

  if (IsTextLeaf() || IsImage()) {
    for (Accessible* parent = Parent(); parent && !parent->IsOuterDoc();
         parent = parent->Parent()) {
      if (parent->IsLink()) {
        anchor = parent->AnchorAt(aAnchorIndex);
      }
    }
  } else {
    anchor = AnchorAt(aAnchorIndex);
  }

  if (anchor) {
    RefPtr<nsIURI> uri;
    nsAutoString spec;
    anchor->Value(spec);
    nsresult rv = NS_NewURI(getter_AddRefs(uri), spec);
    if (NS_SUCCEEDED(rv)) {
      return uri.forget();
    }
  }

  return nullptr;
}

bool Accessible::IsSearchbox() const {
  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  if (roleMapEntry && roleMapEntry->Is(nsGkAtoms::searchbox)) {
    return true;
  }

  RefPtr<nsAtom> inputType = InputType();
  return inputType == nsGkAtoms::search;
}

#ifdef A11Y_LOG
void Accessible::DebugDescription(nsCString& aDesc) const {
  aDesc.Truncate();
  aDesc.AppendPrintf("%s", IsRemote() ? "Remote" : "Local");
  aDesc.AppendPrintf("[%p] ", this);
  nsAutoString role;
  GetAccService()->GetStringRole(Role(), role);
  aDesc.Append(NS_ConvertUTF16toUTF8(role));

  if (nsAtom* tagAtom = TagName()) {
    nsAutoCString tag;
    tagAtom->ToUTF8String(tag);
    aDesc.AppendPrintf(" %s", tag.get());

    nsAutoString id;
    DOMNodeID(id);
    if (!id.IsEmpty()) {
      aDesc.Append("#");
      aDesc.Append(NS_ConvertUTF16toUTF8(id));
    }
  }
  nsAutoString id;

  nsAutoString name;
  Name(name);
  if (!name.IsEmpty()) {
    aDesc.Append(" '");
    aDesc.Append(NS_ConvertUTF16toUTF8(name));
    aDesc.Append("'");
  }
}

void Accessible::DebugPrint(const char* aPrefix,
                            const Accessible* aAccessible) {
  nsAutoCString desc;
  if (aAccessible) {
    aAccessible->DebugDescription(desc);
  } else {
    desc.AssignLiteral("[null]");
  }
#  if defined(ANDROID)
  printf_stderr("%s %s\n", aPrefix, desc.get());
#  else
  printf("%s %s\n", aPrefix, desc.get());
#  endif
}

#endif

void Accessible::TranslateString(const nsString& aKey, nsAString& aStringOut) {
  nsCOMPtr<nsIStringBundleService> stringBundleService =
      components::StringBundle::Service();
  if (!stringBundleService) return;

  nsCOMPtr<nsIStringBundle> stringBundle;
  stringBundleService->CreateBundle(
      "chrome://global-platform/locale/accessible.properties",
      getter_AddRefs(stringBundle));
  if (!stringBundle) return;

  nsAutoString xsValue;
  nsresult rv = stringBundle->GetStringFromName(
      NS_ConvertUTF16toUTF8(aKey).get(), xsValue);
  if (NS_SUCCEEDED(rv)) aStringOut.Assign(xsValue);
}

const Accessible* Accessible::ActionAncestor() const {
  // We do want to consider a click handler on the document. However, we don't
  // want to walk outside of this document, so we stop if we see an OuterDoc.
  for (Accessible* parent = Parent(); parent && !parent->IsOuterDoc();
       parent = parent->Parent()) {
    if (parent->HasPrimaryAction()) {
      return parent;
    }
  }

  return nullptr;
}

nsStaticAtom* Accessible::LandmarkRole() const {
  nsAtom* tagName = TagName();
  if (!tagName) {
    // Either no associated content, or no cache.
    return nullptr;
  }

  if (tagName == nsGkAtoms::nav) {
    return nsGkAtoms::navigation;
  }

  if (tagName == nsGkAtoms::aside) {
    return nsGkAtoms::complementary;
  }

  if (tagName == nsGkAtoms::main) {
    return nsGkAtoms::main;
  }

  if (tagName == nsGkAtoms::header) {
    if (Role() == roles::LANDMARK) {
      return nsGkAtoms::banner;
    }
  }

  if (tagName == nsGkAtoms::footer) {
    if (Role() == roles::LANDMARK) {
      return nsGkAtoms::contentinfo;
    }
  }

  if (tagName == nsGkAtoms::section) {
    nsAutoString name;
    Name(name);
    if (!name.IsEmpty()) {
      return nsGkAtoms::region;
    }
  }

  if (tagName == nsGkAtoms::form) {
    nsAutoString name;
    Name(name);
    if (!name.IsEmpty()) {
      return nsGkAtoms::form;
    }
  }

  if (tagName == nsGkAtoms::search) {
    return nsGkAtoms::search;
  }

  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  return roleMapEntry && roleMapEntry->IsOfType(eLandmark)
             ? roleMapEntry->roleAtom
             : nullptr;
}

nsStaticAtom* Accessible::ComputedARIARole() const {
  const nsRoleMapEntry* roleMap = ARIARoleMap();
  if (roleMap && roleMap->roleAtom != nsGkAtoms::_empty &&
      // region has its own Gecko role and it needs to be handled specially.
      roleMap->roleAtom != nsGkAtoms::region &&
      (roleMap->roleRule == kUseNativeRole || roleMap->IsOfType(eLandmark) ||
       roleMap->roleAtom == nsGkAtoms::alertdialog ||
       roleMap->roleAtom == nsGkAtoms::feed ||
       roleMap->roleAtom == nsGkAtoms::rowgroup ||
       roleMap->roleAtom == nsGkAtoms::searchbox)) {
    // Explicit ARIA role (e.g. specified via the role attribute) which does not
    // map to a unique Gecko role.
    return roleMap->roleAtom;
  }
  role geckoRole = Role();
  if (geckoRole == roles::LANDMARK) {
    // Landmark role from native markup; e.g. <main>, <nav>.
    return LandmarkRole();
  }
  if (geckoRole == roles::GROUPING) {
    // Gecko doesn't differentiate between group and rowgroup. It uses
    // roles::GROUPING for both.
    nsAtom* tag = TagName();
    if (tag == nsGkAtoms::tbody || tag == nsGkAtoms::tfoot ||
        tag == nsGkAtoms::thead) {
      return nsGkAtoms::rowgroup;
    }
  }
  // Role from native markup or layout.
#define ROLE(_geckoRole, stringRole, ariaRole, atkRole, macRole, macSubrole, \
             msaaRole, ia2Role, androidClass, nameRule)                      \
  case roles::_geckoRole:                                                    \
    return ariaRole;
  switch (geckoRole) {
#include "RoleMap.h"
  }
#undef ROLE
  MOZ_ASSERT_UNREACHABLE("Unknown role");
  return nullptr;
}

void Accessible::ApplyImplicitState(uint64_t& aState) const {
  // nsAccessibilityService (and thus FocusManager) can be shut down before
  // RemoteAccessibles.
  if (const auto* focusMgr = FocusMgr()) {
    if (focusMgr->IsFocused(this)) {
      aState |= states::FOCUSED;
    }
  }

  // If this is an ARIA item of the selectable widget and if it's focused and
  // not marked unselected explicitly (i.e. aria-selected="false") then expose
  // it as selected to make ARIA widget authors life easier.
  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  if (roleMapEntry && !(aState & states::SELECTED) &&
      ARIASelected().valueOr(true)) {
    // Special case for tabs: focused tab or focus inside related tab panel
    // implies selected state.
    if (roleMapEntry->role == roles::PAGETAB) {
      if (aState & states::FOCUSED) {
        aState |= states::SELECTED;
      } else {
        // If focus is in a child of the tab panel surely the tab is selected!
        Relation rel = RelationByType(RelationType::LABEL_FOR);
        Accessible* relTarget = nullptr;
        while ((relTarget = rel.Next())) {
          if (relTarget->Role() == roles::PROPERTYPAGE &&
              FocusMgr()->IsFocusWithin(relTarget)) {
            aState |= states::SELECTED;
          }
        }
      }
    } else if (aState & states::FOCUSED) {
      Accessible* container = nsAccUtils::GetSelectableContainer(this, aState);
      if (container && !(container->State() & states::MULTISELECTABLE)) {
        aState |= states::SELECTED;
      }
    }
  }

  if (Opacity() == 1.0f && !(aState & states::INVISIBLE)) {
    aState |= states::OPAQUE1;
  }
}

////////////////////////////////////////////////////////////////////////////////
// KeyBinding class

// static
uint32_t KeyBinding::AccelModifier() {
  switch (WidgetInputEvent::AccelModifier()) {
    case MODIFIER_ALT:
      return kAlt;
    case MODIFIER_CONTROL:
      return kControl;
    case MODIFIER_META:
      return kMeta;
    default:
      MOZ_CRASH("Handle the new result of WidgetInputEvent::AccelModifier()");
      return 0;
  }
}

void KeyBinding::ToPlatformFormat(nsAString& aValue) const {
  nsCOMPtr<nsIStringBundle> keyStringBundle;
  nsCOMPtr<nsIStringBundleService> stringBundleService =
      mozilla::components::StringBundle::Service();
  if (stringBundleService) {
    stringBundleService->CreateBundle(
        "chrome://global-platform/locale/platformKeys.properties",
        getter_AddRefs(keyStringBundle));
  }

  if (!keyStringBundle) return;

  nsAutoString separator;
  keyStringBundle->GetStringFromName("MODIFIER_SEPARATOR", separator);

  nsAutoString modifierName;
  if (mModifierMask & kControl) {
    keyStringBundle->GetStringFromName("VK_CONTROL", modifierName);

    aValue.Append(modifierName);
    aValue.Append(separator);
  }

  if (mModifierMask & kAlt) {
    keyStringBundle->GetStringFromName("VK_ALT", modifierName);

    aValue.Append(modifierName);
    aValue.Append(separator);
  }

  if (mModifierMask & kShift) {
    keyStringBundle->GetStringFromName("VK_SHIFT", modifierName);

    aValue.Append(modifierName);
    aValue.Append(separator);
  }

  if (mModifierMask & kMeta) {
    keyStringBundle->GetStringFromName("VK_META", modifierName);

    aValue.Append(modifierName);
    aValue.Append(separator);
  }

  aValue.Append(mKey);
}

void KeyBinding::ToAtkFormat(nsAString& aValue) const {
  nsAutoString modifierName;
  if (mModifierMask & kControl) aValue.AppendLiteral("<Control>");

  if (mModifierMask & kAlt) aValue.AppendLiteral("<Alt>");

  if (mModifierMask & kShift) aValue.AppendLiteral("<Shift>");

  if (mModifierMask & kMeta) aValue.AppendLiteral("<Meta>");

  aValue.Append(mKey);
}
