/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTextEquivUtils.h"

#include "LocalAccessible-inl.h"
#include "AccIterator.h"
#include "nsCoreUtils.h"
#include "mozilla/dom/ChildIterator.h"
#include "mozilla/dom/Text.h"

using namespace mozilla;
using namespace mozilla::a11y;

/**
 * The accessible for which we are computing a text equivalent. It is useful
 * for bailing out during recursive text computation, or for special cases
 * like step f. of the ARIA implementation guide.
 */
static const Accessible* sInitiatorAcc = nullptr;

////////////////////////////////////////////////////////////////////////////////
// nsTextEquivUtils. Public.

nsresult nsTextEquivUtils::GetNameFromSubtree(
    const LocalAccessible* aAccessible, nsAString& aName) {
  aName.Truncate();

  if (sInitiatorAcc) return NS_OK;

  sInitiatorAcc = aAccessible;
  if (GetRoleRule(aAccessible->Role()) == eNameFromSubtreeRule) {
    // XXX: is it necessary to care the accessible is not a document?
    if (aAccessible->IsContent()) {
      nsAutoString name;
      AppendFromAccessibleChildren(aAccessible, &name);
      name.CompressWhitespace();
      if (!nsCoreUtils::IsWhitespaceString(name)) aName = name;
    }
  }

  sInitiatorAcc = nullptr;

  return NS_OK;
}

nsresult nsTextEquivUtils::GetTextEquivFromIDRefs(
    const LocalAccessible* aAccessible, nsAtom* aIDRefsAttr,
    nsAString& aTextEquiv) {
  aTextEquiv.Truncate();

  nsIContent* content = aAccessible->GetContent();
  if (!content) return NS_OK;

  nsIContent* refContent = nullptr;
  IDRefsIterator iter(aAccessible->Document(), content, aIDRefsAttr);
  while ((refContent = iter.NextElem())) {
    if (!aTextEquiv.IsEmpty()) aTextEquiv += ' ';

    if (refContent->IsHTMLElement(nsGkAtoms::slot)) printf("jtd idref slot\n");
    nsresult rv =
        AppendTextEquivFromContent(aAccessible, refContent, &aTextEquiv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult nsTextEquivUtils::AppendTextEquivFromContent(
    const LocalAccessible* aInitiatorAcc, nsIContent* aContent,
    nsAString* aString) {
  // Prevent recursion which can cause infinite loops.
  if (sInitiatorAcc) return NS_OK;

  sInitiatorAcc = aInitiatorAcc;

  nsresult rv = NS_ERROR_FAILURE;
  if (LocalAccessible* accessible =
          aInitiatorAcc->Document()->GetAccessible(aContent)) {
    rv = AppendFromAccessible(accessible, aString);
  } else {
    // The given content is invisible or otherwise inaccessible, so use the DOM
    // subtree.
    rv = AppendFromDOMNode(aContent, aString);
  }

  sInitiatorAcc = nullptr;
  return rv;
}

nsresult nsTextEquivUtils::AppendTextEquivFromTextContent(nsIContent* aContent,
                                                          nsAString* aString) {
  if (aContent->IsText()) {
    if (aContent->TextLength() > 0) {
      nsIFrame* frame = aContent->GetPrimaryFrame();
      if (frame) {
        nsIFrame::RenderedText text = frame->GetRenderedText(
            0, UINT32_MAX, nsIFrame::TextOffsetType::OffsetsInContentText,
            nsIFrame::TrailingWhitespace::DontTrim);
        aString->Append(text.mString);
      } else {
        // If aContent is an object that is display: none, we have no a frame.
        aContent->GetAsText()->AppendTextTo(*aString);
      }
    }

    return NS_OK;
  }

  if (aContent->IsHTMLElement() &&
      aContent->NodeInfo()->Equals(nsGkAtoms::br)) {
    aString->AppendLiteral("\r\n");
    return NS_OK;
  }

  return NS_OK_NO_NAME_CLAUSE_HANDLED;
}

nsresult nsTextEquivUtils::AppendFromDOMChildren(nsIContent* aContent,
                                                 nsAString* aString) {
  auto iter =
      dom::AllChildrenIterator(aContent, nsIContent::eAllChildren, true);
  while (nsIContent* childContent = iter.GetNextChild()) {
    nsresult rv = AppendFromDOMNode(childContent, aString);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsTextEquivUtils. Private.

nsresult nsTextEquivUtils::AppendFromAccessibleChildren(
    const Accessible* aAccessible, nsAString* aString) {
  nsresult rv = NS_OK_NO_NAME_CLAUSE_HANDLED;

  uint32_t childCount = aAccessible->ChildCount();
  for (uint32_t childIdx = 0; childIdx < childCount; childIdx++) {
    Accessible* child = aAccessible->ChildAt(childIdx);
    rv = AppendFromAccessible(child, aString);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return rv;
}

nsresult nsTextEquivUtils::AppendFromAccessible(Accessible* aAccessible,
                                                nsAString* aString) {
  // XXX: is it necessary to care the accessible is not a document?
  bool isHTMLBlock = false;
  if (aAccessible->IsLocal() && aAccessible->AsLocal()->IsContent()) {
    nsIContent* content = aAccessible->AsLocal()->GetContent();
    nsresult rv = AppendTextEquivFromTextContent(content, aString);
    if (rv != NS_OK_NO_NAME_CLAUSE_HANDLED) return rv;
    if (!content->IsText()) {
      nsIFrame* frame = content->GetPrimaryFrame();
      if (frame) {
        // If this is a block level frame (as opposed to span level), we need to
        // add spaces around that block's text, so we don't get words jammed
        // together in final name.
        const nsStyleDisplay* display = frame->StyleDisplay();
        if (display->IsBlockOutsideStyle() ||
            display->mDisplay == StyleDisplay::TableCell) {
          isHTMLBlock = true;
          if (!aString->IsEmpty()) {
            aString->Append(char16_t(' '));
          }
        }
      }
    }
  }

  bool isEmptyTextEquiv = true;

  // If the name is from tooltip then append it to result string in the end
  // (see h. step of name computation guide).
  nsAutoString text;
  if (aAccessible->Name(text) != eNameFromTooltip) {
    isEmptyTextEquiv = !AppendString(aString, text);
  }

  // Implementation of f. step.
  nsresult rv = AppendFromValue(aAccessible, aString);
  NS_ENSURE_SUCCESS(rv, rv);

  if (rv != NS_OK_NO_NAME_CLAUSE_HANDLED) isEmptyTextEquiv = false;

  // Implementation of g) step of text equivalent computation guide. Go down
  // into subtree if accessible allows "text equivalent from subtree rule" or
  // it's not root and not control.
  if (isEmptyTextEquiv) {
    if (ShouldIncludeInSubtreeCalculation(aAccessible)) {
      rv = AppendFromAccessibleChildren(aAccessible, aString);
      NS_ENSURE_SUCCESS(rv, rv);

      if (rv != NS_OK_NO_NAME_CLAUSE_HANDLED) isEmptyTextEquiv = false;
    }
  }

  // Implementation of h. step
  if (isEmptyTextEquiv && !text.IsEmpty()) {
    AppendString(aString, text);
    if (isHTMLBlock) {
      aString->Append(char16_t(' '));
    }
    return NS_OK;
  }

  if (!isEmptyTextEquiv && isHTMLBlock) {
    aString->Append(char16_t(' '));
  }
  return rv;
}

nsresult nsTextEquivUtils::AppendFromValue(Accessible* aAccessible,
                                           nsAString* aString) {
  if (GetRoleRule(aAccessible->Role()) != eNameFromValueRule) {
    return NS_OK_NO_NAME_CLAUSE_HANDLED;
  }

  // Implementation of step f. of text equivalent computation. If the given
  // accessible is not root accessible (the accessible the text equivalent is
  // computed for in the end) then append accessible value. Otherwise append
  // value if and only if the given accessible is in the middle of its parent.

  nsAutoString text;
  if (aAccessible != sInitiatorAcc) {
    aAccessible->Value(text);

    return AppendString(aString, text) ? NS_OK : NS_OK_NO_NAME_CLAUSE_HANDLED;
  }

  // XXX: is it necessary to care the accessible is not a document?
  if (aAccessible->IsDoc()) return NS_ERROR_UNEXPECTED;

  for (Accessible* next = aAccessible->NextSibling(); next;
       next = next->NextSibling()) {
    if (!IsWhitespaceLeaf(next)) {
      for (Accessible* prev = aAccessible->PrevSibling(); prev;
           prev = prev->PrevSibling()) {
        if (!IsWhitespaceLeaf(prev)) {
          aAccessible->Value(text);

          return AppendString(aString, text) ? NS_OK
                                             : NS_OK_NO_NAME_CLAUSE_HANDLED;
        }
      }
    }
  }

  return NS_OK_NO_NAME_CLAUSE_HANDLED;
}

nsresult nsTextEquivUtils::AppendFromDOMNode(nsIContent* aContent,
                                             nsAString* aString) {
  nsresult rv = AppendTextEquivFromTextContent(aContent, aString);
  NS_ENSURE_SUCCESS(rv, rv);

  if (rv != NS_OK_NO_NAME_CLAUSE_HANDLED) return NS_OK;

  if (aContent->IsAnyOfHTMLElements(nsGkAtoms::script, nsGkAtoms::style)) {
    // The text within these elements is never meant for users.
    return NS_OK;
  }

  if (aContent->IsXULElement()) {
    nsAutoString textEquivalent;
    if (aContent->NodeInfo()->Equals(nsGkAtoms::label, kNameSpaceID_XUL)) {
      aContent->AsElement()->GetAttr(nsGkAtoms::value, textEquivalent);
    } else {
      aContent->AsElement()->GetAttr(nsGkAtoms::label, textEquivalent);
    }

    if (textEquivalent.IsEmpty()) {
      aContent->AsElement()->GetAttr(nsGkAtoms::tooltiptext, textEquivalent);
    }

    AppendString(aString, textEquivalent);
  }

  return AppendFromDOMChildren(aContent, aString);
}

bool nsTextEquivUtils::AppendString(nsAString* aString,
                                    const nsAString& aTextEquivalent) {
  if (aTextEquivalent.IsEmpty()) return false;

  // Insert spaces to insure that words from controls aren't jammed together.
  if (!aString->IsEmpty() && !nsCoreUtils::IsWhitespace(aString->Last())) {
    aString->Append(char16_t(' '));
  }

  aString->Append(aTextEquivalent);

  if (!nsCoreUtils::IsWhitespace(aString->Last())) {
    aString->Append(char16_t(' '));
  }

  return true;
}

uint32_t nsTextEquivUtils::GetRoleRule(role aRole) {
#define ROLE(geckoRole, stringRole, ariaRole, atkRole, macRole, macSubrole, \
             msaaRole, ia2Role, androidClass, nameRule)                     \
  case roles::geckoRole:                                                    \
    return nameRule;

  switch (aRole) {
#include "RoleMap.h"
    default:
      MOZ_CRASH("Unknown role.");
  }

#undef ROLE
}

bool nsTextEquivUtils::ShouldIncludeInSubtreeCalculation(
    Accessible* aAccessible) {
  uint32_t nameRule = GetRoleRule(aAccessible->Role());
  if (nameRule == eNameFromSubtreeRule) {
    return true;
  }
  if (!(nameRule & eNameFromSubtreeIfReqRule)) {
    return false;
  }

  if (aAccessible == sInitiatorAcc) {
    // We're calculating the text equivalent for this accessible, but this
    // accessible should only be included when calculating the text equivalent
    // for something else.
    return false;
  }

  // sInitiatorAcc can be null when, for example, LocalAccessible::Value calls
  // GetTextEquivFromSubtree.
  role initiatorRole = sInitiatorAcc ? sInitiatorAcc->Role() : roles::NOTHING;
  if (initiatorRole == roles::OUTLINEITEM &&
      aAccessible->Role() == roles::GROUPING) {
    // Child treeitems are contained in a group. We don't want to include those
    // in the parent treeitem's text equivalent.
    return false;
  }

  return true;
}

bool nsTextEquivUtils::IsWhitespaceLeaf(Accessible* aAccessible) {
  if (!aAccessible || !aAccessible->IsTextLeaf()) {
    return false;
  }

  nsAutoString name;
  aAccessible->Name(name);
  return nsCoreUtils::IsWhitespaceString(name);
}
