/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *     John Gaunt (jgaunt@netscape.com)
 *     Aaron Leventhal (aaronl@netscape.com)
 *     Kyle Yuan (kyle.yuan@sun.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// NOTE: alphabetically ordered
#include "nsContentCID.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMRange.h"
#include "nsIFrame.h"
#include "nsITextContent.h"
#include "nsTextAccessible.h"
#include "nsTextFragment.h"

static NS_DEFINE_IID(kRangeCID, NS_RANGE_CID);

NS_IMPL_ISUPPORTS1(nsAccessibleText, nsIAccessibleText)

nsAccessibleText::nsAccessibleText()
{
}

nsAccessibleText::~nsAccessibleText()
{
}

void nsAccessibleText::SetTextNode(nsIDOMNode *aNode)
{
  mTextNode = aNode;
}

/**
  * nsIAccessibleText impl.
  */
nsresult nsAccessibleText::GetSelections(nsISelectionController **aSelCon, nsISelection **aDomSel)
{
  nsCOMPtr<nsIDOMDocument> domDoc;
  mTextNode->GetOwnerDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  if (!doc)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPresShell> shell;
  doc->GetShellAt(0, getter_AddRefs(shell));
  if (!shell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPresContext> context;
  shell->GetPresContext(getter_AddRefs(context));
  nsCOMPtr<nsIContent> content(do_QueryInterface(mTextNode));
  nsIFrame *frame = nsnull;
  if (content && NS_SUCCEEDED(shell->GetPrimaryFrameFor(content, &frame)) && frame) {
    frame->GetSelectionController(context, aSelCon);
    if (*aSelCon)
      (*aSelCon)->GetSelection(nsISelectionController::SELECTION_NORMAL, aDomSel);
  }

  if (*aSelCon && *aDomSel)
    return NS_OK;

  return NS_ERROR_FAILURE;
}

/*
Gets the specified text.

aBoundaryType means:

ATK_TEXT_BOUNDARY_CHAR 
  The character before/at/after the offset is returned.

ATK_TEXT_BOUNDARY_WORD_START 
  The returned string is from the word start before/at/after the offset to the next word start.

ATK_TEXT_BOUNDARY_WORD_END 
  The returned string is from the word end before/at/after the offset to the next work end.

ATK_TEXT_BOUNDARY_SENTENCE_START 
  The returned string is from the sentence start before/at/after the offset to the next sentence start.

ATK_TEXT_BOUNDARY_SENTENCE_END 
  The returned string is from the sentence end before/at/after the offset to the next sentence end.

ATK_TEXT_BOUNDARY_LINE_START 
  The returned string is from the line start before/at/after the offset to the next line start.

ATK_TEXT_BOUNDARY_LINE_END 
  The returned string is from the line end before/at/after the offset to the next line start.
*/
nsresult nsAccessibleText::GetTextHelper(EGetTextType aType, nsAccessibleTextBoundary aBoundaryType, 
                                         PRInt32 aOffset, PRInt32 *aStartOffset, PRInt32 *aEndOffset, nsAString & _retval)
{
  nsCOMPtr<nsISelectionController> selCon;
  nsCOMPtr<nsISelection> domSel;

  if (NS_FAILED(GetSelections(getter_AddRefs(selCon), getter_AddRefs(domSel))))
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMRange> range(do_CreateInstance(kRangeCID));
  if (!range)
    return NS_ERROR_OUT_OF_MEMORY;

  *aStartOffset = *aEndOffset = aOffset;

  // Move caret position to offset
  range->SetStart(mTextNode, aOffset);
  range->SetEnd(mTextNode, aOffset);
  domSel->RemoveAllRanges();
  domSel->AddRange(range);

  // Step1: move caret to an appropriate start position
  // Step2: move caret to end postion and select the text
  PRBool isStep1Forward, isStep2Forward;  // Moving directions for two steps
  switch (aType)
  {
  case eGetBefore:
    isStep1Forward = PR_FALSE;
    isStep2Forward = PR_FALSE;
    break;
  case eGetAt:
    isStep1Forward = PR_FALSE;
    isStep2Forward = PR_TRUE;
    break;
  case eGetAfter:
    isStep1Forward = PR_TRUE;
    isStep2Forward = PR_TRUE;
    break;
  default:
    return NS_ERROR_INVALID_ARG;
  }

  switch (aBoundaryType)
  {
  case BOUNDARY_CHAR:
    if (aType == eGetAfter) { // We need the character next to current position
      selCon->CharacterMove(isStep1Forward, PR_FALSE);
      domSel->GetFocusOffset(aStartOffset);
    }
    selCon->CharacterMove(isStep2Forward, PR_TRUE);
    domSel->GetFocusOffset(aEndOffset);
    break;
  case BOUNDARY_WORD_START:
    selCon->WordMove(isStep1Forward, PR_FALSE); // Move caret to previous/next word start boundary
    domSel->GetFocusOffset(aStartOffset);
    selCon->WordMove(isStep2Forward, PR_TRUE);  // Select previous/next word
    domSel->GetFocusOffset(aEndOffset);
    break;
  case BOUNDARY_LINE_START:
    if (aType != eGetAt) { // We need adjust the caret position to previous/next line
      selCon->LineMove(isStep1Forward, PR_TRUE);
      domSel->GetFocusOffset(aEndOffset);
    }
    selCon->IntraLineMove(PR_FALSE, PR_FALSE);  // Move caret to the line start
    domSel->GetFocusOffset(aStartOffset);
    selCon->IntraLineMove(PR_TRUE, PR_TRUE);    // Move caret to the line end and select the whole line
    domSel->GetFocusOffset(aEndOffset);
    break;
  case BOUNDARY_WORD_END:
  case BOUNDARY_LINE_END:
  case BOUNDARY_SENTENCE_START:
  case BOUNDARY_SENTENCE_END:
  case BOUNDARY_ATTRIBUTE_RANGE:
    return NS_ERROR_NOT_IMPLEMENTED;
  default:
    return NS_ERROR_INVALID_ARG;
  }

  nsXPIDLString text;
  // Get text from selection
  domSel->ToString(getter_Copies(text));
  domSel->RemoveAllRanges();
  _retval = text;

  // Ensure aStartOffset <= aEndOffset
  if (*aStartOffset > *aEndOffset) {
    PRInt32 tmp = *aStartOffset;
    *aStartOffset = *aEndOffset;
    *aEndOffset = tmp;
  }

  return NS_OK;
}

/*
 * Gets the offset position of the caret (cursor).
 */
NS_IMETHODIMP nsAccessibleText::GetCaretOffset(PRInt32 *aCaretOffset)
{
  nsCOMPtr<nsISelectionController> selCon;
  nsCOMPtr<nsISelection> domSel;

  if (NS_FAILED(GetSelections(getter_AddRefs(selCon), getter_AddRefs(domSel))))
    return NS_ERROR_FAILURE;

  return domSel->GetFocusOffset(aCaretOffset);
}

/*
 * Sets the caret (cursor) position to the specified offset.
 */
NS_IMETHODIMP nsAccessibleText::SetCaretOffset(PRInt32 aCaretOffset)
{
  nsCOMPtr<nsISelectionController> selCon;
  nsCOMPtr<nsISelection> domSel;

  if (NS_FAILED(GetSelections(getter_AddRefs(selCon), getter_AddRefs(domSel))))
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMRange> range(do_CreateInstance(kRangeCID));
  if (! range)
    return NS_ERROR_OUT_OF_MEMORY;

  range->SetStart(mTextNode, aCaretOffset);
  range->SetEnd(mTextNode, aCaretOffset);
  return domSel->AddRange(range);
}

/*
 * Gets the character count.
 */
NS_IMETHODIMP nsAccessibleText::GetCharacterCount(PRInt32 *aCharacterCount)
{
  nsCOMPtr<nsITextContent> textContent(do_QueryInterface(mTextNode));
  if (!textContent)
    return NS_ERROR_FAILURE;

  return textContent->GetTextLength(aCharacterCount);
}

/*
 * Gets the number of selected regions.
 */
NS_IMETHODIMP nsAccessibleText::GetSelectionCount(PRInt32 *aSelectionCount)
{
  nsCOMPtr<nsISelectionController> selCon;
  nsCOMPtr<nsISelection> domSel;

  if (NS_FAILED(GetSelections(getter_AddRefs(selCon), getter_AddRefs(domSel))))
    return NS_ERROR_FAILURE;

  return domSel->GetRangeCount(aSelectionCount);
}

/*
 * Gets the specified text.
 */
NS_IMETHODIMP nsAccessibleText::GetText(PRInt32 aStartOffset, PRInt32 aEndOffset, nsAString & _retval)
{
  nsAutoString text;
  mTextNode->GetNodeValue(text);
  _retval = Substring(text, aStartOffset, aEndOffset - aStartOffset);
  return NS_OK;
}

NS_IMETHODIMP nsAccessibleText::GetTextBeforeOffset(PRInt32 aOffset, nsAccessibleTextBoundary aBoundaryType, 
                                                    PRInt32 *aStartOffset, PRInt32 *aEndOffset, nsAString & _retval)
{
  return GetTextHelper(eGetBefore, aBoundaryType, aOffset, aStartOffset, aEndOffset, _retval);
}

NS_IMETHODIMP nsAccessibleText::GetTextAtOffset(PRInt32 aOffset, nsAccessibleTextBoundary aBoundaryType, 
                                                PRInt32 *aStartOffset, PRInt32 *aEndOffset, nsAString & _retval)
{
  return GetTextHelper(eGetAt, aBoundaryType, aOffset, aStartOffset, aEndOffset, _retval);
}

NS_IMETHODIMP nsAccessibleText::GetTextAfterOffset(PRInt32 aOffset, nsAccessibleTextBoundary aBoundaryType, 
                                                   PRInt32 *aStartOffset, PRInt32 *aEndOffset, nsAString & _retval)
{
  return GetTextHelper(eGetAfter, aBoundaryType, aOffset, aStartOffset, aEndOffset, _retval);
}

/*
 * Gets the specified text.
 */
NS_IMETHODIMP nsAccessibleText::GetCharacterAtOffset(PRInt32 aOffset, PRUnichar *_retval)
{
  nsCOMPtr<nsITextContent> textContent(do_QueryInterface(mTextNode));
  if (!textContent)
    return NS_ERROR_FAILURE;

  const nsTextFragment *textFrag;
  textContent->GetText(&textFrag);
  *_retval = textFrag->CharAt(aOffset);
  return NS_OK;
}

NS_IMETHODIMP nsAccessibleText::GetAttributeRange(PRInt32 aOffset, PRInt32 *aRangeStartOffset, PRInt32 *aRangeEndOffset, nsISupports **_retval)
{
  // will do better job later
  return NS_ERROR_NOT_IMPLEMENTED;
}

/*
 * Given an offset, the x, y, width, and height values are filled appropriately.
 */
NS_IMETHODIMP nsAccessibleText::GetCharacterExtents(PRInt32 aOffset, PRInt32 *aX, PRInt32 *aY, PRInt32 *aLength, PRInt32 *aWidth, nsAccessibleCoordType aCoordType)
{
  // will do better job later
  return NS_ERROR_NOT_IMPLEMENTED;
}

/*
 * Gets the offset of the character located at coordinates x and y. x and y are interpreted as being relative to 
 * the screen or this widget's window depending on coords.
 */
NS_IMETHODIMP nsAccessibleText::GetOffsetAtPoint(PRInt32 aX, PRInt32 aY, nsAccessibleCoordType aCoordType, PRInt32 *_retval)
{
  // will do better job later
  return NS_ERROR_NOT_IMPLEMENTED;
}

/*
 * Gets the start and end offset of the specified selection.
 */
NS_IMETHODIMP nsAccessibleText::GetSelectionBounds(PRInt32 aSelectionNum, PRInt32 *aStartOffset, PRInt32 *aEndOffset)
{
  nsCOMPtr<nsISelectionController> selCon;
  nsCOMPtr<nsISelection> domSel;

  if (NS_FAILED(GetSelections(getter_AddRefs(selCon), getter_AddRefs(domSel))))
    return NS_ERROR_FAILURE;

  PRInt32 rangeCount;
  domSel->GetRangeCount(&rangeCount);
  if (aSelectionNum < 0 || aSelectionNum >= rangeCount)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIDOMRange> range;
  domSel->GetRangeAt(aSelectionNum, getter_AddRefs(range));
  range->GetStartOffset(aStartOffset);
  range->GetEndOffset(aEndOffset);
  return NS_OK;
}

/*
 * Changes the start and end offset of the specified selection.
 */
NS_IMETHODIMP nsAccessibleText::SetSelectionBounds(PRInt32 aSelectionNum, PRInt32 aStartOffset, PRInt32 aEndOffset)
{
  nsCOMPtr<nsISelectionController> selCon;
  nsCOMPtr<nsISelection> domSel;

  if (NS_FAILED(GetSelections(getter_AddRefs(selCon), getter_AddRefs(domSel))))
    return NS_ERROR_FAILURE;

  PRInt32 rangeCount;
  domSel->GetRangeCount(&rangeCount);
  if (aSelectionNum < 0 || aSelectionNum >= rangeCount)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIDOMRange> range;
  domSel->GetRangeAt(aSelectionNum, getter_AddRefs(range));

  nsCOMPtr<nsIDOMNode> startParent;
  nsCOMPtr<nsIDOMNode> endParent;
  range->GetStartContainer(getter_AddRefs(startParent));
  range->GetEndContainer(getter_AddRefs(endParent));
  range->SetStart(startParent, aStartOffset);
  range->SetEnd(endParent, aEndOffset);
  return NS_OK;
}

/*
 * Adds a selection bounded by the specified offsets.
 */
NS_IMETHODIMP nsAccessibleText::AddSelection(PRInt32 aStartOffset, PRInt32 aEndOffset)
{
  nsCOMPtr<nsISelectionController> selCon;
  nsCOMPtr<nsISelection> domSel;

  if (NS_FAILED(GetSelections(getter_AddRefs(selCon), getter_AddRefs(domSel))))
    return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIDOMRange> range(do_CreateInstance(kRangeCID));
  if (! range)
    return NS_ERROR_OUT_OF_MEMORY;

  range->SetStart(mTextNode, aStartOffset);
  range->SetEnd(mTextNode, aEndOffset);
  return domSel->AddRange(range);
}

/*
 * Removes the specified selection.
 */
NS_IMETHODIMP nsAccessibleText::RemoveSelection(PRInt32 aSelectionNum)
{
  nsCOMPtr<nsISelectionController> selCon;
  nsCOMPtr<nsISelection> domSel;

  if (NS_FAILED(GetSelections(getter_AddRefs(selCon), getter_AddRefs(domSel))))
    return NS_ERROR_FAILURE;

  PRInt32 rangeCount;
  domSel->GetRangeCount(&rangeCount);
  if (aSelectionNum < 0 || aSelectionNum >= rangeCount)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIDOMRange> range;
  domSel->GetRangeAt(aSelectionNum, getter_AddRefs(range));
  return domSel->RemoveRange(range);
}

// ------------
// Text Accessibles
// ------------

nsTextAccessible::nsTextAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell):
nsLinkableAccessible(aDOMNode, aShell)
{ 
  SetTextNode(aDOMNode);
}

NS_IMPL_ISUPPORTS_INHERITED1(nsTextAccessible, nsLinkableAccessible, nsAccessibleText)

/**
  * We are text
  */
NS_IMETHODIMP nsTextAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_TEXT;
  return NS_OK;
}

/**
  * No Children
  */
NS_IMETHODIMP nsTextAccessible::GetAccFirstChild(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

/**
  * No Children
  */
NS_IMETHODIMP nsTextAccessible::GetAccLastChild(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

/**
  * No Children
  */
NS_IMETHODIMP nsTextAccessible::GetAccChildCount(PRInt32 *_retval)
{
  *_retval = 0;
  return NS_OK;
}
