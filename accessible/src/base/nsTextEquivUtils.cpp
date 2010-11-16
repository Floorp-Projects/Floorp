/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsTextEquivUtils.h"

#include "nsAccessibilityService.h"
#include "nsAccessible.h"
#include "nsAccUtils.h"

#include "nsIDOMXULLabeledControlEl.h"

#include "nsArrayUtils.h"

#define NS_OK_NO_NAME_CLAUSE_HANDLED \
NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_GENERAL, 0x24)

////////////////////////////////////////////////////////////////////////////////
// nsTextEquivUtils. Public.

nsresult
nsTextEquivUtils::GetNameFromSubtree(nsAccessible *aAccessible,
                                     nsAString& aName)
{
  aName.Truncate();

  if (gInitiatorAcc)
    return NS_OK;

  gInitiatorAcc = aAccessible;

  PRUint32 nameRule = gRoleToNameRulesMap[aAccessible->Role()];
  if (nameRule == eFromSubtree) {
    //XXX: is it necessary to care the accessible is not a document?
    if (aAccessible->IsContent()) {
      nsAutoString name;
      AppendFromAccessibleChildren(aAccessible, &name);
      name.CompressWhitespace();
      if (!IsWhitespaceString(name))
        aName = name;
    }
  }

  gInitiatorAcc = nsnull;

  return NS_OK;
}

nsresult
nsTextEquivUtils::GetTextEquivFromIDRefs(nsAccessible *aAccessible,
                                         nsIAtom *aIDRefsAttr,
                                         nsAString& aTextEquiv)
{
  aTextEquiv.Truncate();

  nsIContent* content = aAccessible->GetContent();
  if (!content)
    return NS_OK;

  nsCOMPtr<nsIArray> refElms;
  nsCoreUtils::GetElementsByIDRefsAttr(content, aIDRefsAttr,
                                       getter_AddRefs(refElms));

  if (!refElms)
    return NS_OK;

  PRUint32 count = 0;
  nsresult rv = refElms->GetLength(&count);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIContent> refContent;
  for (PRUint32 idx = 0; idx < count; idx++) {
    refContent = do_QueryElementAt(refElms, idx, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!aTextEquiv.IsEmpty())
      aTextEquiv += ' ';

    rv = AppendTextEquivFromContent(aAccessible, refContent, &aTextEquiv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
nsTextEquivUtils::AppendTextEquivFromContent(nsAccessible *aInitiatorAcc,
                                             nsIContent *aContent,
                                             nsAString *aString)
{
  // Prevent recursion which can cause infinite loops.
  if (gInitiatorAcc)
    return NS_OK;

  gInitiatorAcc = aInitiatorAcc;

  nsCOMPtr<nsIWeakReference> shell = nsCoreUtils::GetWeakShellFor(aContent);
  if (!shell) {
    NS_ASSERTION(PR_TRUE, "There is no presshell!");
    gInitiatorAcc = nsnull;
    return NS_ERROR_UNEXPECTED;
  }

  // If the given content is not visible or isn't accessible then go down
  // through the DOM subtree otherwise go down through accessible subtree and
  // calculate the flat string.
  nsIFrame *frame = aContent->GetPrimaryFrame();
  PRBool isVisible = frame && frame->GetStyleVisibility()->IsVisible();

  nsresult rv = NS_ERROR_FAILURE;
  PRBool goThroughDOMSubtree = PR_TRUE;

  if (isVisible) {
    nsAccessible *accessible =
      GetAccService()->GetAccessibleInWeakShell(aContent, shell);
    if (accessible) {
      rv = AppendFromAccessible(accessible, aString);
      goThroughDOMSubtree = PR_FALSE;
    }
  }

  if (goThroughDOMSubtree)
    rv = AppendFromDOMNode(aContent, aString);

  gInitiatorAcc = nsnull;
  return rv;
}

nsresult
nsTextEquivUtils::AppendTextEquivFromTextContent(nsIContent *aContent,
                                                 nsAString *aString)
{
  if (aContent->IsNodeOfType(nsINode::eTEXT)) {
    PRBool isHTMLBlock = PR_FALSE;

    nsIContent *parentContent = aContent->GetParent();
    if (parentContent) {
      nsIFrame *frame = parentContent->GetPrimaryFrame();
      if (frame) {
        // If this text is inside a block level frame (as opposed to span
        // level), we need to add spaces around that block's text, so we don't
        // get words jammed together in final name.
        const nsStyleDisplay* display = frame->GetStyleDisplay();
        if (display->IsBlockOutside() ||
            display->mDisplay == NS_STYLE_DISPLAY_TABLE_CELL) {
          isHTMLBlock = PR_TRUE;
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
      aContent->NodeInfo()->Equals(nsAccessibilityAtoms::br)) {
    aString->AppendLiteral("\r\n");
    return NS_OK;
  }
  
  return NS_OK_NO_NAME_CLAUSE_HANDLED;
}

////////////////////////////////////////////////////////////////////////////////
// nsTextEquivUtils. Private.

nsRefPtr<nsAccessible> nsTextEquivUtils::gInitiatorAcc;

nsresult
nsTextEquivUtils::AppendFromAccessibleChildren(nsAccessible *aAccessible,
                                               nsAString *aString)
{
  nsresult rv = NS_OK_NO_NAME_CLAUSE_HANDLED;

  PRInt32 childCount = aAccessible->GetChildCount();
  for (PRInt32 childIdx = 0; childIdx < childCount; childIdx++) {
    nsAccessible *child = aAccessible->GetChildAt(childIdx);
    rv = AppendFromAccessible(child, aString);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return rv;
}

nsresult
nsTextEquivUtils::AppendFromAccessible(nsAccessible *aAccessible,
                                       nsAString *aString)
{
  //XXX: is it necessary to care the accessible is not a document?
  if (aAccessible->IsContent()) {
    nsresult rv = AppendTextEquivFromTextContent(aAccessible->GetContent(),
                                                 aString);
    if (rv != NS_OK_NO_NAME_CLAUSE_HANDLED)
      return rv;
  }

  nsAutoString text;
  nsresult rv = aAccessible->GetName(text);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool isEmptyTextEquiv = PR_TRUE;

  // If the name is from tooltip then append it to result string in the end
  // (see h. step of name computation guide).
  if (rv != NS_OK_NAME_FROM_TOOLTIP)
    isEmptyTextEquiv = !AppendString(aString, text);

  // Implementation of f. step.
  rv = AppendFromValue(aAccessible, aString);
  NS_ENSURE_SUCCESS(rv, rv);

  if (rv != NS_OK_NO_NAME_CLAUSE_HANDLED)
    isEmptyTextEquiv = PR_FALSE;

  // Implementation of g) step of text equivalent computation guide. Go down
  // into subtree if accessible allows "text equivalent from subtree rule" or
  // it's not root and not control.
  if (isEmptyTextEquiv) {
    PRUint32 nameRule = gRoleToNameRulesMap[aAccessible->Role()];
    if (nameRule & eFromSubtreeIfRec) {
      rv = AppendFromAccessibleChildren(aAccessible, aString);
      NS_ENSURE_SUCCESS(rv, rv);

      if (rv != NS_OK_NO_NAME_CLAUSE_HANDLED)
        isEmptyTextEquiv = PR_FALSE;
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
nsTextEquivUtils::AppendFromValue(nsAccessible *aAccessible,
                                  nsAString *aString)
{
  PRUint32 nameRule = gRoleToNameRulesMap[aAccessible->Role()];
  if (nameRule != eFromValue)
    return NS_OK_NO_NAME_CLAUSE_HANDLED;

  // Implementation of step f. of text equivalent computation. If the given
  // accessible is not root accessible (the accessible the text equivalent is
  // computed for in the end) then append accessible value. Otherwise append
  // value if and only if the given accessible is in the middle of its parent.

  nsAutoString text;
  if (aAccessible != gInitiatorAcc) {
    nsresult rv = aAccessible->GetValue(text);
    NS_ENSURE_SUCCESS(rv, rv);

    return AppendString(aString, text) ?
      NS_OK : NS_OK_NO_NAME_CLAUSE_HANDLED;
  }

  //XXX: is it necessary to care the accessible is not a document?
  if (aAccessible->IsDocument())
    return NS_ERROR_UNEXPECTED;

  nsIContent *content = aAccessible->GetContent();

  nsCOMPtr<nsIContent> parent = content->GetParent();
  PRInt32 indexOf = parent->IndexOf(content);

  for (PRInt32 i = indexOf - 1; i >= 0; i--) {
    // check for preceding text...
    if (!parent->GetChildAt(i)->TextIsOnlyWhitespace()) {
      PRUint32 childCount = parent->GetChildCount();
      for (PRUint32 j = indexOf + 1; j < childCount; j++) {
        // .. and subsequent text
        if (!parent->GetChildAt(j)->TextIsOnlyWhitespace()) {
          nsresult rv = aAccessible->GetValue(text);
          NS_ENSURE_SUCCESS(rv, rv);

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
  PRUint32 childCount = aContent->GetChildCount();
  for (PRUint32 childIdx = 0; childIdx < childCount; childIdx++) {
    nsCOMPtr<nsIContent> childContent = aContent->GetChildAt(childIdx);

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
      if (aContent->NodeInfo()->Equals(nsAccessibilityAtoms::label,
                                       kNameSpaceID_XUL))
        aContent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::value,
                          textEquivalent);

      if (textEquivalent.IsEmpty())
        aContent->GetAttr(kNameSpaceID_None,
                          nsAccessibilityAtoms::tooltiptext, textEquivalent);
    }

    AppendString(aString, textEquivalent);
  }

  return AppendFromDOMChildren(aContent, aString);
}

PRBool
nsTextEquivUtils::AppendString(nsAString *aString,
                               const nsAString& aTextEquivalent)
{
  // Insert spaces to insure that words from controls aren't jammed together.
  if (aTextEquivalent.IsEmpty())
    return PR_FALSE;

  if (!aString->IsEmpty())
    aString->Append(PRUnichar(' '));

  aString->Append(aTextEquivalent);
  return PR_TRUE;
}

PRBool
nsTextEquivUtils::IsWhitespaceString(const nsSubstring& aString)
{
  nsSubstring::const_char_iterator iterBegin, iterEnd;

  aString.BeginReading(iterBegin);
  aString.EndReading(iterEnd);

  while (iterBegin != iterEnd && IsWhitespace(*iterBegin))
    ++iterBegin;

  return iterBegin == iterEnd;
}

PRBool
nsTextEquivUtils::IsWhitespace(PRUnichar aChar)
{
  return aChar == ' ' || aChar == '\n' ||
    aChar == '\r' || aChar == '\t' || aChar == 0xa0;
}

////////////////////////////////////////////////////////////////////////////////
// Name rules to role map.

PRUint32 nsTextEquivUtils::gRoleToNameRulesMap[] =
{
  eNoRule,           // ROLE_NOTHING
  eNoRule,           // ROLE_TITLEBAR
  eNoRule,           // ROLE_MENUBAR
  eNoRule,           // ROLE_SCROLLBAR
  eNoRule,           // ROLE_GRIP
  eNoRule,           // ROLE_SOUND
  eNoRule,           // ROLE_CURSOR
  eNoRule,           // ROLE_CARET
  eNoRule,           // ROLE_ALERT
  eNoRule,           // ROLE_WINDOW
  eNoRule,           // ROLE_INTERNAL_FRAME
  eNoRule,           // ROLE_MENUPOPUP
  eFromSubtree,      // ROLE_MENUITEM
  eFromSubtree,      // ROLE_TOOLTIP
  eNoRule,           // ROLE_APPLICATION
  eNoRule,           // ROLE_DOCUMENT
  eNoRule,           // ROLE_PANE
  eNoRule,           // ROLE_CHART
  eNoRule,           // ROLE_DIALOG
  eNoRule,           // ROLE_BORDER
  eNoRule,           // ROLE_GROUPING
  eNoRule,           // ROLE_SEPARATOR
  eNoRule,           // ROLE_TOOLBAR
  eNoRule,           // ROLE_STATUSBAR
  eNoRule,           // ROLE_TABLE
  eFromSubtree,      // ROLE_COLUMNHEADER
  eFromSubtree,      // ROLE_ROWHEADER
  eFromSubtree,      // ROLE_COLUMN
  eFromSubtree,      // ROLE_ROW
  eFromSubtreeIfRec, // ROLE_CELL
  eFromSubtree,      // ROLE_LINK
  eFromSubtree,      // ROLE_HELPBALLOON
  eNoRule,           // ROLE_CHARACTER
  eFromSubtreeIfRec, // ROLE_LIST
  eFromSubtree,      // ROLE_LISTITEM
  eNoRule,           // ROLE_OUTLINE
  eFromSubtree,      // ROLE_OUTLINEITEM
  eFromSubtree,      // ROLE_PAGETAB
  eNoRule,           // ROLE_PROPERTYPAGE
  eNoRule,           // ROLE_INDICATOR
  eNoRule,           // ROLE_GRAPHIC
  eNoRule,           // ROLE_STATICTEXT
  eNoRule,           // ROLE_TEXT_LEAF
  eFromSubtree,      // ROLE_PUSHBUTTON
  eFromSubtree,      // ROLE_CHECKBUTTON
  eFromSubtree,      // ROLE_RADIOBUTTON
  eFromValue,        // ROLE_COMBOBOX
  eNoRule,           // ROLE_DROPLIST
  eFromValue,        // ROLE_PROGRESSBAR
  eNoRule,           // ROLE_DIAL
  eNoRule,           // ROLE_HOTKEYFIELD
  eNoRule,           // ROLE_SLIDER
  eNoRule,           // ROLE_SPINBUTTON
  eNoRule,           // ROLE_DIAGRAM
  eNoRule,           // ROLE_ANIMATION
  eNoRule,           // ROLE_EQUATION
  eFromSubtree,      // ROLE_BUTTONDROPDOWN
  eFromSubtree,      // ROLE_BUTTONMENU
  eFromSubtree,      // ROLE_BUTTONDROPDOWNGRID
  eNoRule,           // ROLE_WHITESPACE
  eNoRule,           // ROLE_PAGETABLIST
  eNoRule,           // ROLE_CLOCK
  eNoRule,           // ROLE_SPLITBUTTON
  eNoRule,           // ROLE_IPADDRESS
  eNoRule,           // ROLE_ACCEL_LABEL
  eNoRule,           // ROLE_ARROW
  eNoRule,           // ROLE_CANVAS
  eFromSubtree,      // ROLE_CHECK_MENU_ITEM
  eNoRule,           // ROLE_COLOR_CHOOSER
  eNoRule,           // ROLE_DATE_EDITOR
  eNoRule,           // ROLE_DESKTOP_ICON
  eNoRule,           // ROLE_DESKTOP_FRAME
  eNoRule,           // ROLE_DIRECTORY_PANE
  eNoRule,           // ROLE_FILE_CHOOSER
  eNoRule,           // ROLE_FONT_CHOOSER
  eNoRule,           // ROLE_CHROME_WINDOW
  eNoRule,           // ROLE_GLASS_PANE
  eFromSubtreeIfRec, // ROLE_HTML_CONTAINER
  eNoRule,           // ROLE_ICON
  eFromSubtree,      // ROLE_LABEL
  eNoRule,           // ROLE_LAYERED_PANE
  eNoRule,           // ROLE_OPTION_PANE
  eNoRule,           // ROLE_PASSWORD_TEXT
  eNoRule,           // ROLE_POPUP_MENU
  eFromSubtree,      // ROLE_RADIO_MENU_ITEM
  eNoRule,           // ROLE_ROOT_PANE
  eNoRule,           // ROLE_SCROLL_PANE
  eNoRule,           // ROLE_SPLIT_PANE
  eFromSubtree,      // ROLE_TABLE_COLUMN_HEADER
  eFromSubtree,      // ROLE_TABLE_ROW_HEADER
  eFromSubtree,      // ROLE_TEAR_OFF_MENU_ITEM
  eNoRule,           // ROLE_TERMINAL
  eFromSubtreeIfRec, // ROLE_TEXT_CONTAINER
  eFromSubtree,      // ROLE_TOGGLE_BUTTON
  eNoRule,           // ROLE_TREE_TABLE
  eNoRule,           // ROLE_VIEWPORT
  eNoRule,           // ROLE_HEADER
  eNoRule,           // ROLE_FOOTER
  eFromSubtreeIfRec, // ROLE_PARAGRAPH
  eNoRule,           // ROLE_RULER
  eNoRule,           // ROLE_AUTOCOMPLETE
  eNoRule,           // ROLE_EDITBAR
  eFromValue,        // ROLE_ENTRY
  eNoRule,           // ROLE_CAPTION
  eNoRule,           // ROLE_DOCUMENT_FRAME
  eFromSubtreeIfRec, // ROLE_HEADING
  eNoRule,           // ROLE_PAGE
  eFromSubtreeIfRec, // ROLE_SECTION
  eNoRule,           // ROLE_REDUNDANT_OBJECT
  eNoRule,           // ROLE_FORM
  eNoRule,           // ROLE_IME
  eNoRule,           // ROLE_APP_ROOT
  eFromSubtree,      // ROLE_PARENT_MENUITEM
  eNoRule,           // ROLE_CALENDAR
  eNoRule,           // ROLE_COMBOBOX_LIST
  eFromSubtree,      // ROLE_COMBOBOX_OPTION
  eNoRule,           // ROLE_IMAGE_MAP
  eFromSubtree,      // ROLE_OPTION
  eFromSubtree,      // ROLE_RICH_OPTION
  eNoRule,           // ROLE_LISTBOX
  eNoRule,           // ROLE_FLAT_EQUATION
  eFromSubtree       // ROLE_GRID_CELL
};
