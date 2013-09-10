/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTextEquivUtils.h"

#include "Accessible-inl.h"
#include "AccIterator.h"
#include "nsCoreUtils.h"
#include "nsIDOMXULLabeledControlEl.h"

using namespace mozilla::a11y;

/**
 * The accessible for which we are computing a text equivalent. It is useful
 * for bailing out during recursive text computation, or for special cases
 * like step f. of the ARIA implementation guide.
 */
static Accessible* sInitiatorAcc = nullptr;

////////////////////////////////////////////////////////////////////////////////
// nsTextEquivUtils. Public.

nsresult
nsTextEquivUtils::GetNameFromSubtree(Accessible* aAccessible,
                                     nsAString& aName)
{
  aName.Truncate();

  if (sInitiatorAcc)
    return NS_OK;

  sInitiatorAcc = aAccessible;
  if (GetRoleRule(aAccessible->Role()) == eNameFromSubtreeRule) {
    //XXX: is it necessary to care the accessible is not a document?
    if (aAccessible->IsContent()) {
      nsAutoString name;
      AppendFromAccessibleChildren(aAccessible, &name);
      name.CompressWhitespace();
      if (!nsCoreUtils::IsWhitespaceString(name))
        aName = name;
    }
  }

  sInitiatorAcc = nullptr;

  return NS_OK;
}

nsresult
nsTextEquivUtils::GetTextEquivFromIDRefs(Accessible* aAccessible,
                                         nsIAtom *aIDRefsAttr,
                                         nsAString& aTextEquiv)
{
  aTextEquiv.Truncate();

  nsIContent* content = aAccessible->GetContent();
  if (!content)
    return NS_OK;

  nsIContent* refContent = nullptr;
  IDRefsIterator iter(aAccessible->Document(), content, aIDRefsAttr);
  while ((refContent = iter.NextElem())) {
    if (!aTextEquiv.IsEmpty())
      aTextEquiv += ' ';

    nsresult rv = AppendTextEquivFromContent(aAccessible, refContent,
                                             &aTextEquiv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
nsTextEquivUtils::AppendTextEquivFromContent(Accessible* aInitiatorAcc,
                                             nsIContent *aContent,
                                             nsAString *aString)
{
  // Prevent recursion which can cause infinite loops.
  if (sInitiatorAcc)
    return NS_OK;

  sInitiatorAcc = aInitiatorAcc;

  // If the given content is not visible or isn't accessible then go down
  // through the DOM subtree otherwise go down through accessible subtree and
  // calculate the flat string.
  nsIFrame *frame = aContent->GetPrimaryFrame();
  bool isVisible = frame && frame->StyleVisibility()->IsVisible();

  nsresult rv = NS_ERROR_FAILURE;
  bool goThroughDOMSubtree = true;

  if (isVisible) {
    Accessible* accessible =
      sInitiatorAcc->Document()->GetAccessible(aContent);
    if (accessible) {
      rv = AppendFromAccessible(accessible, aString);
      goThroughDOMSubtree = false;
    }
  }

  if (goThroughDOMSubtree)
    rv = AppendFromDOMNode(aContent, aString);

  sInitiatorAcc = nullptr;
  return rv;
}

nsresult
nsTextEquivUtils::AppendTextEquivFromTextContent(nsIContent *aContent,
                                                 nsAString *aString)
{
  if (aContent->IsNodeOfType(nsINode::eTEXT)) {
    bool isHTMLBlock = false;

    nsIContent *parentContent = aContent->GetFlattenedTreeParent();
    if (parentContent) {
      nsIFrame *frame = parentContent->GetPrimaryFrame();
      if (frame) {
        // If this text is inside a block level frame (as opposed to span
        // level), we need to add spaces around that block's text, so we don't
        // get words jammed together in final name.
        const nsStyleDisplay* display = frame->StyleDisplay();
        if (display->IsBlockOutsideStyle() ||
            display->mDisplay == NS_STYLE_DISPLAY_TABLE_CELL) {
          isHTMLBlock = true;
          if (!aString->IsEmpty()) {
            aString->Append(PRUnichar(' '));
          }
        }
      }
    }
    
    if (aContent->TextLength() > 0) {
      nsIFrame *frame = aContent->GetPrimaryFrame();
      if (frame) {
        nsresult rv = frame->GetRenderedText(aString);
        NS_ENSURE_SUCCESS(rv, rv);
      } else {
        // If aContent is an object that is display: none, we have no a frame.
        aContent->AppendTextTo(*aString);
      }
      if (isHTMLBlock && !aString->IsEmpty()) {
        aString->Append(PRUnichar(' '));
      }
    }
    
    return NS_OK;
  }
  
  if (aContent->IsHTML() &&
      aContent->NodeInfo()->Equals(nsGkAtoms::br)) {
    aString->AppendLiteral("\r\n");
    return NS_OK;
  }
  
  return NS_OK_NO_NAME_CLAUSE_HANDLED;
}

////////////////////////////////////////////////////////////////////////////////
// nsTextEquivUtils. Private.

nsresult
nsTextEquivUtils::AppendFromAccessibleChildren(Accessible* aAccessible,
                                               nsAString *aString)
{
  nsresult rv = NS_OK_NO_NAME_CLAUSE_HANDLED;

  uint32_t childCount = aAccessible->ChildCount();
  for (uint32_t childIdx = 0; childIdx < childCount; childIdx++) {
    Accessible* child = aAccessible->GetChildAt(childIdx);
    rv = AppendFromAccessible(child, aString);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return rv;
}

nsresult
nsTextEquivUtils::AppendFromAccessible(Accessible* aAccessible,
                                       nsAString *aString)
{
  //XXX: is it necessary to care the accessible is not a document?
  if (aAccessible->IsContent()) {
    nsresult rv = AppendTextEquivFromTextContent(aAccessible->GetContent(),
                                                 aString);
    if (rv != NS_OK_NO_NAME_CLAUSE_HANDLED)
      return rv;
  }

  bool isEmptyTextEquiv = true;

  // If the name is from tooltip then append it to result string in the end
  // (see h. step of name computation guide).
  nsAutoString text;
  if (aAccessible->Name(text) != eNameFromTooltip)
    isEmptyTextEquiv = !AppendString(aString, text);

  // Implementation of f. step.
  nsresult rv = AppendFromValue(aAccessible, aString);
  NS_ENSURE_SUCCESS(rv, rv);

  if (rv != NS_OK_NO_NAME_CLAUSE_HANDLED)
    isEmptyTextEquiv = false;

  // Implementation of g) step of text equivalent computation guide. Go down
  // into subtree if accessible allows "text equivalent from subtree rule" or
  // it's not root and not control.
  if (isEmptyTextEquiv) {
    uint32_t nameRule = GetRoleRule(aAccessible->Role());
    if (nameRule & eNameFromSubtreeIfReqRule) {
      rv = AppendFromAccessibleChildren(aAccessible, aString);
      NS_ENSURE_SUCCESS(rv, rv);

      if (rv != NS_OK_NO_NAME_CLAUSE_HANDLED)
        isEmptyTextEquiv = false;
    }
  }

  // Implementation of h. step
  if (isEmptyTextEquiv && !text.IsEmpty()) {
    AppendString(aString, text);
    return NS_OK;
  }

  return rv;
}

nsresult
nsTextEquivUtils::AppendFromValue(Accessible* aAccessible,
                                  nsAString *aString)
{
  if (GetRoleRule(aAccessible->Role()) != eNameFromValueRule)
    return NS_OK_NO_NAME_CLAUSE_HANDLED;

  // Implementation of step f. of text equivalent computation. If the given
  // accessible is not root accessible (the accessible the text equivalent is
  // computed for in the end) then append accessible value. Otherwise append
  // value if and only if the given accessible is in the middle of its parent.

  nsAutoString text;
  if (aAccessible != sInitiatorAcc) {
    aAccessible->Value(text);

    return AppendString(aString, text) ?
      NS_OK : NS_OK_NO_NAME_CLAUSE_HANDLED;
  }

  //XXX: is it necessary to care the accessible is not a document?
  if (aAccessible->IsDoc())
    return NS_ERROR_UNEXPECTED;

  nsIContent *content = aAccessible->GetContent();

  for (nsIContent* childContent = content->GetPreviousSibling(); childContent;
       childContent = childContent->GetPreviousSibling()) {
    // check for preceding text...
    if (!childContent->TextIsOnlyWhitespace()) {
      for (nsIContent* siblingContent = content->GetNextSibling(); siblingContent;
           siblingContent = siblingContent->GetNextSibling()) {
        // .. and subsequent text
        if (!siblingContent->TextIsOnlyWhitespace()) {
          aAccessible->Value(text);

          return AppendString(aString, text) ?
            NS_OK : NS_OK_NO_NAME_CLAUSE_HANDLED;
          break;
        }
      }
      break;
    }
  }

  return NS_OK_NO_NAME_CLAUSE_HANDLED;
}

nsresult
nsTextEquivUtils::AppendFromDOMChildren(nsIContent *aContent,
                                        nsAString *aString)
{
  for (nsIContent* childContent = aContent->GetFirstChild(); childContent;
       childContent = childContent->GetNextSibling()) {
    nsresult rv = AppendFromDOMNode(childContent, aString);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
nsTextEquivUtils::AppendFromDOMNode(nsIContent *aContent, nsAString *aString)
{
  nsresult rv = AppendTextEquivFromTextContent(aContent, aString);
  NS_ENSURE_SUCCESS(rv, rv);

  if (rv != NS_OK_NO_NAME_CLAUSE_HANDLED)
    return NS_OK;

  if (aContent->IsXUL()) {
    nsAutoString textEquivalent;
    nsCOMPtr<nsIDOMXULLabeledControlElement> labeledEl =
      do_QueryInterface(aContent);

    if (labeledEl) {
      labeledEl->GetLabel(textEquivalent);
    } else {
      if (aContent->NodeInfo()->Equals(nsGkAtoms::label,
                                       kNameSpaceID_XUL))
        aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::value,
                          textEquivalent);

      if (textEquivalent.IsEmpty())
        aContent->GetAttr(kNameSpaceID_None,
                          nsGkAtoms::tooltiptext, textEquivalent);
    }

    AppendString(aString, textEquivalent);
  }

  return AppendFromDOMChildren(aContent, aString);
}

bool
nsTextEquivUtils::AppendString(nsAString *aString,
                               const nsAString& aTextEquivalent)
{
  if (aTextEquivalent.IsEmpty())
    return false;

  // Insert spaces to insure that words from controls aren't jammed together.
  if (!aString->IsEmpty() && !nsCoreUtils::IsWhitespace(aString->Last()))
    aString->Append(PRUnichar(' '));

  aString->Append(aTextEquivalent);

  if (!nsCoreUtils::IsWhitespace(aString->Last()))
    aString->Append(PRUnichar(' '));

  return true;
}

uint32_t 
nsTextEquivUtils::GetRoleRule(role aRole)
{
#define ROLE(geckoRole, stringRole, atkRole, \
             macRole, msaaRole, ia2Role, nameRule) \
  case roles::geckoRole: \
    return nameRule;

  switch (aRole) {
#include "RoleMap.h"
    default:
      MOZ_CRASH("Unknown role.");
  }

#undef ROLE
}

