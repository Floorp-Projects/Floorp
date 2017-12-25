/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAccUtils.h"

#include "Accessible-inl.h"
#include "ARIAMap.h"
#include "nsAccessibilityService.h"
#include "nsCoreUtils.h"
#include "DocAccessible.h"
#include "HyperTextAccessible.h"
#include "nsIAccessibleTypes.h"
#include "Role.h"
#include "States.h"
#include "TextLeafAccessible.h"

#include "nsIDOMXULContainerElement.h"
#include "nsIPersistentProperties2.h"
#include "mozilla/dom/Element.h"

using namespace mozilla;
using namespace mozilla::a11y;

void
nsAccUtils::GetAccAttr(nsIPersistentProperties *aAttributes,
                       nsAtom *aAttrName, nsAString& aAttrValue)
{
  aAttrValue.Truncate();

  aAttributes->GetStringProperty(nsAtomCString(aAttrName), aAttrValue);
}

void
nsAccUtils::SetAccAttr(nsIPersistentProperties *aAttributes,
                       nsAtom *aAttrName, const nsAString& aAttrValue)
{
  nsAutoString oldValue;
  aAttributes->SetStringProperty(nsAtomCString(aAttrName), aAttrValue, oldValue);
}

void
nsAccUtils::SetAccAttr(nsIPersistentProperties *aAttributes,
                       nsAtom* aAttrName, nsAtom* aAttrValue)
{
  nsAutoString oldValue;
  aAttributes->SetStringProperty(nsAtomCString(aAttrName),
                                 nsAtomString(aAttrValue), oldValue);
}

void
nsAccUtils::SetAccGroupAttrs(nsIPersistentProperties *aAttributes,
                             int32_t aLevel, int32_t aSetSize,
                             int32_t aPosInSet)
{
  nsAutoString value;

  if (aLevel) {
    value.AppendInt(aLevel);
    SetAccAttr(aAttributes, nsGkAtoms::level, value);
  }

  if (aSetSize && aPosInSet) {
    value.Truncate();
    value.AppendInt(aPosInSet);
    SetAccAttr(aAttributes, nsGkAtoms::posinset, value);

    value.Truncate();
    value.AppendInt(aSetSize);
    SetAccAttr(aAttributes, nsGkAtoms::setsize, value);
  }
}

int32_t
nsAccUtils::GetDefaultLevel(Accessible* aAccessible)
{
  roles::Role role = aAccessible->Role();

  if (role == roles::OUTLINEITEM)
    return 1;

  if (role == roles::ROW) {
    Accessible* parent = aAccessible->Parent();
    // It is a row inside flatten treegrid. Group level is always 1 until it
    // is overriden by aria-level attribute.
    if (parent && parent->Role() == roles::TREE_TABLE)
      return 1;
  }

  return 0;
}

int32_t
nsAccUtils::GetARIAOrDefaultLevel(Accessible* aAccessible)
{
  int32_t level = 0;
  nsCoreUtils::GetUIntAttr(aAccessible->GetContent(),
                           nsGkAtoms::aria_level, &level);

  if (level != 0)
    return level;

  return GetDefaultLevel(aAccessible);
}

int32_t
nsAccUtils::GetLevelForXULContainerItem(nsIContent *aContent)
{
  nsCOMPtr<nsIDOMXULContainerItemElement> item(do_QueryInterface(aContent));
  if (!item)
    return 0;

  nsCOMPtr<nsIDOMXULContainerElement> container;
  item->GetParentContainer(getter_AddRefs(container));
  if (!container)
    return 0;

  // Get level of the item.
  int32_t level = -1;
  while (container) {
    level++;

    nsCOMPtr<nsIDOMXULContainerElement> parentContainer;
    container->GetParentContainer(getter_AddRefs(parentContainer));
    parentContainer.swap(container);
  }

  return level;
}

void
nsAccUtils::SetLiveContainerAttributes(nsIPersistentProperties *aAttributes,
                                       nsIContent* aStartContent,
                                       dom::Element* aTopEl)
{
  nsAutoString live, relevant, busy;
  nsIContent* ancestor = aStartContent;
  while (ancestor) {

    // container-relevant attribute
    if (relevant.IsEmpty() &&
        HasDefinedARIAToken(ancestor, nsGkAtoms::aria_relevant) &&
        ancestor->GetAttr(kNameSpaceID_None, nsGkAtoms::aria_relevant, relevant))
      SetAccAttr(aAttributes, nsGkAtoms::containerRelevant, relevant);

    // container-live, and container-live-role attributes
    if (live.IsEmpty()) {
      const nsRoleMapEntry* role = nullptr;
      if (ancestor->IsElement()) {
        role = aria::GetRoleMap(ancestor->AsElement());
      }
      if (HasDefinedARIAToken(ancestor, nsGkAtoms::aria_live)) {
        ancestor->GetAttr(kNameSpaceID_None, nsGkAtoms::aria_live, live);
      } else if (role) {
        GetLiveAttrValue(role->liveAttRule, live);
      }
      if (!live.IsEmpty()) {
        SetAccAttr(aAttributes, nsGkAtoms::containerLive, live);
        if (role) {
          SetAccAttr(aAttributes, nsGkAtoms::containerLiveRole,
                     role->ARIARoleString());
        }
      }
    }

    // container-atomic attribute
    if (ancestor->AttrValueIs(kNameSpaceID_None, nsGkAtoms::aria_atomic,
                              nsGkAtoms::_true, eCaseMatters)) {
      SetAccAttr(aAttributes, nsGkAtoms::containerAtomic,
                 NS_LITERAL_STRING("true"));
    }

    // container-busy attribute
    if (busy.IsEmpty() &&
        HasDefinedARIAToken(ancestor, nsGkAtoms::aria_busy) &&
        ancestor->GetAttr(kNameSpaceID_None, nsGkAtoms::aria_busy, busy))
      SetAccAttr(aAttributes, nsGkAtoms::containerBusy, busy);

    if (ancestor == aTopEl)
      break;

    ancestor = ancestor->GetParent();
    if (!ancestor)
      ancestor = aTopEl; // Use <body>/<frameset>
  }
}

bool
nsAccUtils::HasDefinedARIAToken(nsIContent *aContent, nsAtom *aAtom)
{
  NS_ASSERTION(aContent, "aContent is null in call to HasDefinedARIAToken!");

  if (!aContent->HasAttr(kNameSpaceID_None, aAtom) ||
      aContent->AttrValueIs(kNameSpaceID_None, aAtom,
                            nsGkAtoms::_empty, eCaseMatters) ||
      aContent->AttrValueIs(kNameSpaceID_None, aAtom,
                            nsGkAtoms::_undefined, eCaseMatters)) {
        return false;
  }
  return true;
}

nsAtom*
nsAccUtils::GetARIAToken(dom::Element* aElement, nsAtom* aAttr)
{
  if (!HasDefinedARIAToken(aElement, aAttr))
    return nsGkAtoms::_empty;

  static nsIContent::AttrValuesArray tokens[] =
    { &nsGkAtoms::_false, &nsGkAtoms::_true,
      &nsGkAtoms::mixed, nullptr};

  int32_t idx = aElement->FindAttrValueIn(kNameSpaceID_None,
                                          aAttr, tokens, eCaseMatters);
  if (idx >= 0)
    return *(tokens[idx]);

  return nullptr;
}

Accessible*
nsAccUtils::GetSelectableContainer(Accessible* aAccessible, uint64_t aState)
{
  if (!aAccessible)
    return nullptr;

  if (!(aState & states::SELECTABLE))
    return nullptr;

  Accessible* parent = aAccessible;
  while ((parent = parent->Parent()) && !parent->IsSelect()) {
    if (parent->Role() == roles::PANE)
      return nullptr;
  }
  return parent;
}

bool
nsAccUtils::IsARIASelected(Accessible* aAccessible)
{
  return aAccessible->GetContent()->
    AttrValueIs(kNameSpaceID_None, nsGkAtoms::aria_selected,
                nsGkAtoms::_true, eCaseMatters);
}

Accessible*
nsAccUtils::TableFor(Accessible* aRow)
{
  if (aRow) {
    Accessible* table = aRow->Parent();
    if (table) {
      roles::Role tableRole = table->Role();
      if (tableRole == roles::GROUPING) { // if there's a rowgroup.
        table = table->Parent();
        if (table)
          tableRole = table->Role();
      }

      return (tableRole == roles::TABLE || tableRole == roles::TREE_TABLE ||
              tableRole == roles::MATHML_TABLE) ? table : nullptr;
    }
  }

  return nullptr;
}

HyperTextAccessible*
nsAccUtils::GetTextContainer(nsINode* aNode)
{
  // Get text accessible containing the result node.
  DocAccessible* doc =
    GetAccService()->GetDocAccessible(aNode->OwnerDoc());
  Accessible* accessible =
    doc ? doc->GetAccessibleOrContainer(aNode) : nullptr;
  if (!accessible)
    return nullptr;

  do {
    HyperTextAccessible* textAcc = accessible->AsHyperText();
    if (textAcc)
      return textAcc;

    accessible = accessible->Parent();
  } while (accessible);

  return nullptr;
}

nsIntPoint
nsAccUtils::ConvertToScreenCoords(int32_t aX, int32_t aY,
                                  uint32_t aCoordinateType,
                                  Accessible* aAccessible)
{
  nsIntPoint coords(aX, aY);

  switch (aCoordinateType) {
    case nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE:
      break;

    case nsIAccessibleCoordinateType::COORDTYPE_WINDOW_RELATIVE:
    {
      coords += nsCoreUtils::GetScreenCoordsForWindow(aAccessible->GetNode());
      break;
    }

    case nsIAccessibleCoordinateType::COORDTYPE_PARENT_RELATIVE:
    {
      coords += GetScreenCoordsForParent(aAccessible);
      break;
    }

    default:
      NS_NOTREACHED("invalid coord type!");
  }

  return coords;
}

void
nsAccUtils::ConvertScreenCoordsTo(int32_t *aX, int32_t *aY,
                                  uint32_t aCoordinateType,
                                  Accessible* aAccessible)
{
  switch (aCoordinateType) {
    case nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE:
      break;

    case nsIAccessibleCoordinateType::COORDTYPE_WINDOW_RELATIVE:
    {
      nsIntPoint coords = nsCoreUtils::GetScreenCoordsForWindow(aAccessible->GetNode());
      *aX -= coords.x;
      *aY -= coords.y;
      break;
    }

    case nsIAccessibleCoordinateType::COORDTYPE_PARENT_RELATIVE:
    {
      nsIntPoint coords = GetScreenCoordsForParent(aAccessible);
      *aX -= coords.x;
      *aY -= coords.y;
      break;
    }

    default:
    NS_NOTREACHED("invalid coord type!");
  }
}

nsIntPoint
nsAccUtils::GetScreenCoordsForParent(Accessible* aAccessible)
{
  Accessible* parent = aAccessible->Parent();
  if (!parent)
    return nsIntPoint(0, 0);

  nsIFrame *parentFrame = parent->GetFrame();
  if (!parentFrame)
    return nsIntPoint(0, 0);

  nsRect rect = parentFrame->GetScreenRectInAppUnits();
  return nsPoint(rect.x, rect.y).
    ToNearestPixels(parentFrame->PresContext()->AppUnitsPerDevPixel());
}

bool
nsAccUtils::GetLiveAttrValue(uint32_t aRule, nsAString& aValue)
{
  switch (aRule) {
    case eOffLiveAttr:
      aValue = NS_LITERAL_STRING("off");
      return true;
    case ePoliteLiveAttr:
      aValue = NS_LITERAL_STRING("polite");
      return true;
  }

  return false;
}

#ifdef DEBUG

bool
nsAccUtils::IsTextInterfaceSupportCorrect(Accessible* aAccessible)
{
  // Don't test for accessible docs, it makes us create accessibles too
  // early and fire mutation events before we need to
  if (aAccessible->IsDoc())
    return true;

  bool foundText = false;
  uint32_t childCount = aAccessible->ChildCount();
  for (uint32_t childIdx = 0; childIdx < childCount; childIdx++) {
    Accessible* child = aAccessible->GetChildAt(childIdx);
    if (child->IsText()) {
      foundText = true;
      break;
    }
  }

  return !foundText || aAccessible->IsHyperText();
}
#endif

uint32_t
nsAccUtils::TextLength(Accessible* aAccessible)
{
  if (!aAccessible->IsText()) {
    return 1;
  }

  TextLeafAccessible* textLeaf = aAccessible->AsTextLeaf();
  if (textLeaf)
    return textLeaf->Text().Length();

  // For list bullets (or anything other accessible which would compute its own
  // text. They don't have their own frame.
  // XXX In the future, list bullets may have frame and anon content, so
  // we should be able to remove this at that point
  nsAutoString text;
  aAccessible->AppendTextTo(text); // Get all the text
  return text.Length();
}

bool
nsAccUtils::MustPrune(Accessible* aAccessible)
{
  roles::Role role = aAccessible->Role();

  // Don't prune the tree for certain roles if the tree is more complex than
  // a single text leaf.
  return
    (role == roles::MENUITEM ||
     role == roles::COMBOBOX_OPTION ||
     role == roles::OPTION ||
     role == roles::ENTRY ||
     role == roles::FLAT_EQUATION ||
     role == roles::PASSWORD_TEXT ||
     role == roles::PUSHBUTTON ||
     role == roles::TOGGLE_BUTTON ||
     role == roles::GRAPHIC ||
     role == roles::SLIDER ||
     role == roles::PROGRESSBAR ||
     role == roles::SEPARATOR) &&
    aAccessible->ContentChildCount() == 1 &&
    aAccessible->ContentChildAt(0)->IsTextLeaf();
}
