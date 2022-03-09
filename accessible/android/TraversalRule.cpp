/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TraversalRule.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/a11y/Accessible.h"

#include "Role.h"
#include "HTMLListAccessible.h"
#include "SessionAccessibility.h"
#include "nsAccUtils.h"
#include "nsIAccessiblePivot.h"

using namespace mozilla;
using namespace mozilla::a11y;

TraversalRule::TraversalRule()
    : TraversalRule(java::SessionAccessibility::HTML_GRANULARITY_DEFAULT) {}

TraversalRule::TraversalRule(int32_t aGranularity)
    : mGranularity(aGranularity) {}

uint16_t TraversalRule::Match(Accessible* aAcc) {
  MOZ_ASSERT(aAcc);

  uint16_t result = nsIAccessibleTraversalRule::FILTER_IGNORE;

  if (nsAccUtils::MustPrune(aAcc)) {
    result |= nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }

  uint64_t state = aAcc->State();

  if ((state & states::INVISIBLE) != 0) {
    return result;
  }

  // Bug 1733268: Support OPAQUE1/opacity remotely
  if (aAcc->IsLocal() && (state & states::OPAQUE1) == 0) {
    nsIFrame* frame = aAcc->AsLocal()->GetFrame();
    if (frame && frame->StyleEffects()->mOpacity == 0.0f) {
      return result | nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
    }
  }

  switch (mGranularity) {
    case java::SessionAccessibility::HTML_GRANULARITY_LINK:
      result |= LinkMatch(aAcc);
      break;
    case java::SessionAccessibility::HTML_GRANULARITY_CONTROL:
      result |= ControlMatch(aAcc);
      break;
    case java::SessionAccessibility::HTML_GRANULARITY_SECTION:
      result |= SectionMatch(aAcc);
      break;
    case java::SessionAccessibility::HTML_GRANULARITY_HEADING:
      result |= HeadingMatch(aAcc);
      break;
    case java::SessionAccessibility::HTML_GRANULARITY_LANDMARK:
      result |= LandmarkMatch(aAcc);
      break;
    default:
      result |= DefaultMatch(aAcc);
      break;
  }

  return result;
}

bool TraversalRule::IsSingleLineage(Accessible* aAccessible) {
  Accessible* child = aAccessible;
  while (child) {
    switch (child->ChildCount()) {
      case 0:
        return true;
      case 1:
        child = child->FirstChild();
        break;
      case 2:
        if (IsListItemBullet(child->FirstChild())) {
          child = child->LastChild();
        } else {
          return false;
        }
        break;
      default:
        return false;
    }
  }

  return true;
}

bool TraversalRule::IsListItemBullet(const Accessible* aAccessible) {
  return aAccessible->Role() == roles::LISTITEM_MARKER;
}

bool TraversalRule::IsFlatSubtree(const Accessible* aAccessible) {
  for (auto child = aAccessible->FirstChild(); child;
       child = child->NextSibling()) {
    roles::Role role = child->Role();
    if (role == roles::TEXT_LEAF || role == roles::STATICTEXT) {
      continue;
    }

    if (child->ChildCount() > 0 || child->ActionCount() > 0) {
      return false;
    }
  }

  return true;
}

bool TraversalRule::HasName(const Accessible* aAccessible) {
  nsAutoString name;
  aAccessible->Name(name);
  name.CompressWhitespace();
  return !name.IsEmpty();
}

uint16_t TraversalRule::LinkMatch(Accessible* aAccessible) {
  if (aAccessible->Role() == roles::LINK &&
      (aAccessible->State() & states::LINKED) != 0) {
    return nsIAccessibleTraversalRule::FILTER_MATCH |
           nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }

  return nsIAccessibleTraversalRule::FILTER_IGNORE;
}

uint16_t TraversalRule::HeadingMatch(Accessible* aAccessible) {
  if (aAccessible->Role() == roles::HEADING && aAccessible->ChildCount()) {
    return nsIAccessibleTraversalRule::FILTER_MATCH;
  }

  return nsIAccessibleTraversalRule::FILTER_IGNORE;
}

uint16_t TraversalRule::SectionMatch(Accessible* aAccessible) {
  roles::Role role = aAccessible->Role();
  if (role == roles::HEADING || role == roles::LANDMARK ||
      aAccessible->LandmarkRole()) {
    return nsIAccessibleTraversalRule::FILTER_MATCH;
  }

  return nsIAccessibleTraversalRule::FILTER_IGNORE;
}

uint16_t TraversalRule::LandmarkMatch(Accessible* aAccessible) {
  if (aAccessible->LandmarkRole()) {
    return nsIAccessibleTraversalRule::FILTER_MATCH;
  }

  return nsIAccessibleTraversalRule::FILTER_IGNORE;
}

uint16_t TraversalRule::ControlMatch(Accessible* aAccessible) {
  switch (aAccessible->Role()) {
    case roles::PUSHBUTTON:
    case roles::SPINBUTTON:
    case roles::TOGGLE_BUTTON:
    case roles::BUTTONDROPDOWN:
    case roles::BUTTONDROPDOWNGRID:
    case roles::COMBOBOX:
    case roles::LISTBOX:
    case roles::ENTRY:
    case roles::PASSWORD_TEXT:
    case roles::PAGETAB:
    case roles::RADIOBUTTON:
    case roles::RADIO_MENU_ITEM:
    case roles::SLIDER:
    case roles::CHECKBUTTON:
    case roles::CHECK_MENU_ITEM:
    case roles::SWITCH:
    case roles::MENUITEM:
      return nsIAccessibleTraversalRule::FILTER_MATCH |
             nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
    case roles::LINK:
      return LinkMatch(aAccessible);
    case roles::EDITCOMBOBOX:
      if (aAccessible->State() & states::EDITABLE) {
        // Only match ARIA 1.0 comboboxes; i.e. where the combobox itself is
        // editable. If it's a 1.1 combobox, the combobox is just a container;
        // we want to stop on the textbox inside it, not the container.
        return nsIAccessibleTraversalRule::FILTER_MATCH |
               nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
      }
      break;
    default:
      break;
  }

  return nsIAccessibleTraversalRule::FILTER_IGNORE;
}

uint16_t TraversalRule::DefaultMatch(Accessible* aAccessible) {
  switch (aAccessible->Role()) {
    case roles::COMBOBOX:
      // We don't want to ignore the subtree because this is often
      // where the list box hangs out.
      return nsIAccessibleTraversalRule::FILTER_MATCH;
    case roles::EDITCOMBOBOX:
      if (aAccessible->State() & states::EDITABLE) {
        // Only match ARIA 1.0 comboboxes; i.e. where the combobox itself is
        // editable. If it's a 1.1 combobox, the combobox is just a container;
        // we want to stop on the textbox inside it.
        return nsIAccessibleTraversalRule::FILTER_MATCH;
      }
      break;
    case roles::TEXT_LEAF:
    case roles::GRAPHIC:
      // Nameless text leaves are boring, skip them.
      if (HasName(aAccessible)) {
        return nsIAccessibleTraversalRule::FILTER_MATCH;
      }
      break;
    case roles::STATICTEXT:
      // Ignore list bullets
      if (!IsListItemBullet(aAccessible)) {
        return nsIAccessibleTraversalRule::FILTER_MATCH;
      }
      break;
    case roles::HEADER:
    case roles::HEADING:
    case roles::COLUMNHEADER:
    case roles::ROWHEADER:
    case roles::STATUSBAR:
      if ((aAccessible->ChildCount() > 0 || HasName(aAccessible)) &&
          (IsSingleLineage(aAccessible) || IsFlatSubtree(aAccessible))) {
        return nsIAccessibleTraversalRule::FILTER_MATCH |
               nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
      }
      break;
    case roles::GRID_CELL:
      if (IsSingleLineage(aAccessible) || IsFlatSubtree(aAccessible)) {
        return nsIAccessibleTraversalRule::FILTER_MATCH |
               nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
      }
      break;
    case roles::LISTITEM:
      if (IsFlatSubtree(aAccessible) || IsSingleLineage(aAccessible)) {
        return nsIAccessibleTraversalRule::FILTER_MATCH |
               nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
      }
      break;
    case roles::LABEL:
      if (IsFlatSubtree(aAccessible)) {
        // Match if this is a label with text but no nested controls.
        return nsIAccessibleTraversalRule::FILTER_MATCH |
               nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
      }
      break;
    case roles::MENUITEM:
    case roles::LINK:
    case roles::PAGETAB:
    case roles::PUSHBUTTON:
    case roles::CHECKBUTTON:
    case roles::RADIOBUTTON:
    case roles::PROGRESSBAR:
    case roles::BUTTONDROPDOWN:
    case roles::BUTTONMENU:
    case roles::CHECK_MENU_ITEM:
    case roles::PASSWORD_TEXT:
    case roles::RADIO_MENU_ITEM:
    case roles::TOGGLE_BUTTON:
    case roles::ENTRY:
    case roles::KEY:
    case roles::SLIDER:
    case roles::SPINBUTTON:
    case roles::OPTION:
    case roles::SWITCH:
    case roles::MATHML_MATH:
      // Ignore the subtree, if there is one. So that we don't land on
      // the same content that was already presented by its parent.
      return nsIAccessibleTraversalRule::FILTER_MATCH |
             nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
    default:
      break;
  }

  return nsIAccessibleTraversalRule::FILTER_IGNORE;
}

uint16_t ExploreByTouchRule::Match(Accessible* aAcc) {
  if (aAcc->IsRemote()) {
    // Explore by touch happens in the local process and should
    // not drill down into remote frames.
    return nsIAccessibleTraversalRule::FILTER_IGNORE |
           nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }

  return TraversalRule::Match(aAcc);
}
