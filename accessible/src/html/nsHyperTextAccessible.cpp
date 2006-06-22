/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Initial Developers of the Original Code are
 * Sun Microsystems and IBM Corporation
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ginn Chen (ginn.chen@sun.com)
 *   Aaron Leventhal (aleventh@us.ibm.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsHyperTextAccessible.h"
#include "nsAccessibilityAtoms.h"
#include "nsAccessibilityService.h"
#include "nsPIAccessNode.h"
#include "nsIClipboard.h"
#include "nsContentCID.h"
#include "nsIDOMAbstractView.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMHTMLBRElement.h"
#include "nsIDOMRange.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMXULDocument.h"
#include "nsIFontMetrics.h"
#include "nsIFrame.h"
#include "nsIPlaintextEditor.h"
#include "nsIServiceManager.h"
#include "nsITextContent.h"
#include "nsTextFragment.h"

static NS_DEFINE_IID(kRangeCID, NS_RANGE_CID);

// ------------
// nsHyperTextAccessible
// ------------

NS_IMPL_ADDREF_INHERITED(nsHyperTextAccessible, nsAccessible)
NS_IMPL_RELEASE_INHERITED(nsHyperTextAccessible, nsAccessible)

nsresult nsHyperTextAccessible::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  *aInstancePtr = nsnull;

  nsCOMPtr<nsIDOMXULDocument> xulDoc(do_QueryInterface(mDOMNode));
  if (mDOMNode && !xulDoc) {
    // XXX Remove XUL DOC check if documents end up not inheriting from this class.
    // We need it for now because for now nsDocAccessible must inherit from nsHyperTextAccessible
    // in order for HTML document accessibles to get support for these interfaces
    // However at some point we may push <body> to implement the interfaces and
    // return nsDocAccessible to inherit from nsAccessibleWrap
    if (aIID.Equals(NS_GET_IID(nsIAccessibleText))) {
      // If |this| contains any children
      PRInt32 numChildren;
      GetChildCount(&numChildren);
      if (numChildren > 0) {
        *aInstancePtr = NS_STATIC_CAST(nsIAccessibleText*, this);
        NS_ADDREF_THIS();
        return NS_OK;
      }
      return NS_ERROR_NO_INTERFACE;
    }

    if (aIID.Equals(NS_GET_IID(nsIAccessibleHyperText))) {
      if (IsHyperText()) {
        // If |this| contains text and embedded objects
        *aInstancePtr = NS_STATIC_CAST(nsIAccessibleHyperText*, this);
        NS_ADDREF_THIS();
        return NS_OK;
      }
      return NS_ERROR_NO_INTERFACE;
    }

    if (aIID.Equals(NS_GET_IID(nsIAccessibleEditableText))) {
      // If this contains editable text
      PRUint32 extState;
      GetExtState(&extState);
      if (extState & EXT_STATE_EDITABLE) {
        *aInstancePtr = NS_STATIC_CAST(nsIAccessibleEditableText*, this);
        NS_ADDREF_THIS();
        return NS_OK;
      }
      return NS_ERROR_NO_INTERFACE;
    }
  }

  return nsAccessible::QueryInterface(aIID, aInstancePtr);
}

nsHyperTextAccessible::nsHyperTextAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsAccessibleWrap(aNode, aShell)
{
}

PRBool nsHyperTextAccessible::IsHyperText()
{
  nsCOMPtr<nsIAccessible> accessible;
  while (NextChild(accessible)) {
    if (Role(accessible) != ROLE_TEXT_LEAF) {
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

NS_IMETHODIMP nsHyperTextAccessible::GetRole(PRUint32 *aRole)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(mDOMNode);
  if (!content) {
    return NS_ERROR_FAILURE;
  }

  nsIAtom *tag = content->Tag();

  if (tag == nsAccessibilityAtoms::caption) {
    *aRole = ROLE_CAPTION;
  }
  // XXX Implement new roles
  else if (tag == nsAccessibilityAtoms::form) {
    *aRole = ROLE_FORM;
  }
  else if (tag == nsAccessibilityAtoms::div ||
           tag == nsAccessibilityAtoms::blockquote) {
    *aRole = ROLE_SECTION;
  }
  else if (tag == nsAccessibilityAtoms::h1 ||
           tag == nsAccessibilityAtoms::h2 ||
           tag == nsAccessibilityAtoms::h3 ||
           tag == nsAccessibilityAtoms::h4 ||
           tag == nsAccessibilityAtoms::h5 ||
           tag == nsAccessibilityAtoms::h6) {
    *aRole = ROLE_HEADING;
  }
  else {
    nsIFrame *frame = GetFrame();
    if (frame && frame->GetType() == nsAccessibilityAtoms::blockFrame) {
      *aRole = ROLE_PARAGRAPH;   // XXX Implement para role
    }
    else {
      *aRole = ROLE_TEXT_CONTAINER; // In ATK this works
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsHyperTextAccessible::GetExtState(PRUint32 *aExtState)
{
  *aExtState = 0;
  if (!mDOMNode) {
    return NS_ERROR_FAILURE; // Node is shut down
  }

  nsresult rv = nsAccessibleWrap::GetExtState(aExtState);
  nsCOMPtr<nsIEditor> editor = GetEditor();
  if (editor) {
    PRUint32 flags;
    editor->GetFlags(&flags);
    if (0 == (flags & nsIPlaintextEditor::eEditorReadonlyMask)) {
      *aExtState |= EXT_STATE_EDITABLE;
    }
  }

  PRInt32 childCount;
  GetChildCount(&childCount);
  if (childCount > 0) {
    *aExtState |= EXT_STATE_SELECTABLE_TEXT;
  }
  return rv;
}

/*
 * Gets the specified text.
 */
nsIFrame* nsHyperTextAccessible::GetStartPosAndText(PRInt32& aStartOffset, PRInt32 aEndOffset, nsAString &aText)
{
  PRInt32 startOffset = aStartOffset;
  PRInt32 endOffset = aEndOffset;

  aText.Truncate();
  if (endOffset == -1) {
    const PRInt32 kMaxTextLength = 32767;
    endOffset = kMaxTextLength; // Max end offset
  }
  else if (startOffset <= endOffset) {
    return nsnull;
  }

  nsIFrame *startFrame = nsnull;
  nsCOMPtr<nsIAccessible> accessible;

  while (endOffset > 0 && NextChild(accessible)) {
    if (Role(accessible) == ROLE_TEXT_LEAF) {
      nsCOMPtr<nsPIAccessNode> accessNode(do_QueryInterface(accessible));
      nsIFrame *frame = accessNode->GetFrame();
      if (frame) {
        // Avoid string copies
        nsITextContent *textContent = NS_STATIC_CAST(nsITextContent*, frame->GetContent());
        NS_ASSERTION(textContent, "No text content for text accessible!");
        PRInt32 textContentLength = textContent->TextLength();
        if (startOffset < textContentLength) {
          // XXX Can we somehow optimize further by getting the nsTextFragment and use
          // CopyTo to a PRUnichar buffer to copy it directly to the string?
          nsAutoString newText;
          textContent->AppendTextTo(newText);
          if (startOffset > 0 || endOffset < textContentLength) {
            // The Cut operation is efficient
            newText.Cut(startOffset,
                        PR_MIN(textContentLength, endOffset - startOffset));
          }
          if (frame && !frame->GetStyleText()->WhiteSpaceIsSignificant()) {
            // Replace \r\n\t in markup with space unless in this is preformatted text
            // where those characters are significant
            newText.ReplaceChar("\r\n\t", ' ');
          }
          if (!startFrame) {
            startFrame = frame;
            aStartOffset = startOffset;
          }
          aText += newText;
          endOffset -= startOffset;
          startOffset = 0;
        }
        else {
          startOffset -= textContentLength;
          endOffset -= textContentLength;
          if (!startFrame) {
            startFrame = frame;
          }
        }
      }
    }
    else {
      // Embedded object, append marker
      // XXX Append \n for <br>'s
      if (startOffset >= 1) {
        -- startOffset;
        -- endOffset;
      }
      else {
        aText += kEmbeddedObjectChar;
        if (!startFrame) {
          nsCOMPtr<nsPIAccessNode> accessNode(do_QueryInterface(accessible));
          startFrame = accessNode->GetFrame();
          aStartOffset = startOffset;
        }
      }
    }
  }

  return startFrame;
}

NS_IMETHODIMP nsHyperTextAccessible::GetText(PRInt32 aStartOffset, PRInt32 aEndOffset, nsAString &aText)
{
  return GetStartPosAndText(aStartOffset, aEndOffset, aText) ? NS_OK : NS_ERROR_FAILURE;
}

/*
 * Gets the character count.
 */
NS_IMETHODIMP nsHyperTextAccessible::GetCharacterCount(PRInt32 *aCharacterCount)
{
  *aCharacterCount = 0;
  if (!mDOMNode) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIAccessible> accessible;

  while (NextChild(accessible)) {
    if (Role(accessible) == ROLE_TEXT_LEAF) {
      nsCOMPtr<nsPIAccessNode> accessNode(do_QueryInterface(accessible));
      nsIFrame *frame = accessNode->GetFrame();
      if (frame) {
        nsITextContent *textContent = NS_STATIC_CAST(nsITextContent*, frame->GetContent());
        *aCharacterCount += textContent->TextLength();
      }
    }
    else {
      ++*aCharacterCount;
    }
  }
  return NS_OK;
}

/*
 * Gets the specified character.
 */
NS_IMETHODIMP nsHyperTextAccessible::GetCharacterAtOffset(PRInt32 aOffset, PRUnichar *aCharacter)
{
  nsAutoString text;
  nsresult rv = GetText(aOffset, aOffset + 1, text);
  NS_ENSURE_SUCCESS(rv, rv);
  *aCharacter = text.First();
  return NS_OK;
}

nsresult nsHyperTextAccessible::GetCurrentOffset(nsISupports *aClosure, nsISelection *aDomSel, PRInt32 *aOffset)
{
  nsCOMPtr<nsIDOMNode> focusNode;
  aDomSel->GetFocusNode(getter_AddRefs(focusNode));
  aDomSel->GetFocusOffset(aOffset);
  return DOMPointToOffset(aClosure, focusNode, *aOffset, aOffset);
}

nsresult nsHyperTextAccessible::DOMPointToOffset(nsISupports *aClosure, nsIDOMNode* aNode, PRInt32 aNodeOffset, PRInt32* aResult)
{
  NS_ENSURE_ARG_POINTER(aNode && aResult);

  *aResult = aNodeOffset;

#if 0
  nsCOMPtr<nsIArray> domNodeArray(do_QueryInterface(aClosure));
  if (domNodeArray) {
    // Static text, calculate the offset from a given set of (text) node
    PRUint32 totalLength = 0;
    PRUint32 index, count;
    domNodeArray->GetLength(&count);
    for (index = 0; index < count; index++) {
      nsCOMPtr<nsIDOMNode> domNode(do_QueryElementAt(domNodeArray, index));
      if (aNode == domNode) {
        *aResult = aNodeOffset + totalLength;
        break;
      }
      nsCOMPtr<nsITextContent> textContent(do_QueryInterface(domNode));
      if (textContent) {
        totalLength += textContent->TextLength();;
      }
    }

    return NS_OK;
  }
  nsCOMPtr<nsIEditor> editor(do_QueryInterface(aClosure));
  if (editor) { // revised according to nsTextControlFrame::DOMPointToOffset
    // Editable text, calculate the offset from the editor
    nsCOMPtr<nsIDOMElement> rootElement;
    editor->GetRootElement(getter_AddRefs(rootElement));
    nsCOMPtr<nsIDOMNode> rootNode(do_QueryInterface(rootElement));

    NS_ENSURE_TRUE(rootNode, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDOMNodeList> nodeList;

    nsresult rv = rootNode->GetChildNodes(getter_AddRefs(nodeList));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(nodeList, NS_ERROR_FAILURE);

    PRUint32 length = 0;
    rv = nodeList->GetLength(&length);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!length || aNodeOffset < 0)
      return NS_OK;

    PRInt32 i, textOffset = 0;
    PRInt32 lastIndex = (PRInt32)length - 1;

    for (i = 0; i < (PRInt32)length; i++) {
      if (rootNode == aNode && i == aNodeOffset) {
        *aResult = textOffset;
        return NS_OK;
      }

      nsCOMPtr<nsIDOMNode> item;
      rv = nodeList->Item(i, getter_AddRefs(item));
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ENSURE_TRUE(item, NS_ERROR_FAILURE);

      if (item == aNode) {
        *aResult = textOffset + aNodeOffset;
        return NS_OK;
      }

      nsCOMPtr<nsITextContent> textContent(do_QueryInterface(item));

      if (textContent) {
        textOffset += textContent->TextLength();;
      }
      else {
        // Must be a BR node. If it's not the last BR node
        // under the root, count it as a newline.
        if (i != lastIndex)
          ++textOffset;
      }
    }

    NS_ASSERTION((aNode == rootNode && aNodeOffset == (PRInt32)length),
                 "Invalid node offset!");

    *aResult = textOffset;
  }
#endif

  return NS_OK;
}

nsresult nsHyperTextAccessible::OffsetToDOMPoint(nsISupports *aClosure, PRInt32 aOffset, nsIDOMNode** aResult, PRInt32* aPosition)
{
  NS_ENSURE_ARG_POINTER(aResult && aPosition);

  *aResult = nsnull;
  *aPosition = 0;

  nsCOMPtr<nsIEditor> editor(do_QueryInterface(aClosure));
  if (editor) { // revised according to nsTextControlFrame::OffsetToDOMPoint
    nsCOMPtr<nsIDOMElement> rootElement;
    editor->GetRootElement(getter_AddRefs(rootElement));
    nsCOMPtr<nsIDOMNode> rootNode(do_QueryInterface(rootElement));

    NS_ENSURE_TRUE(rootNode, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDOMNodeList> nodeList;

    nsresult rv = rootNode->GetChildNodes(getter_AddRefs(nodeList));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(nodeList, NS_ERROR_FAILURE);

    PRUint32 length = 0;

    rv = nodeList->GetLength(&length);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!length || aOffset < 0) {
      *aPosition = 0;
      *aResult = rootNode;
      NS_ADDREF(*aResult);
      return NS_OK;
    }

    PRInt32 textOffset = 0;
    PRUint32 lastIndex = length - 1;

    for (PRUint32 i=0; i<length; i++) {
      nsCOMPtr<nsIDOMNode> item;
      rv = nodeList->Item(i, getter_AddRefs(item));
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ENSURE_TRUE(item, NS_ERROR_FAILURE);

      nsCOMPtr<nsITextContent> textContent(do_QueryInterface(item));

      if (textContent) {
        PRUint32 textLength = textContent->TextLength();;

        // Check if aOffset falls within this range.
        if (aOffset >= textOffset && aOffset <= textOffset+(PRInt32)textLength) {
          *aPosition = aOffset - textOffset;
          *aResult = item;
          NS_ADDREF(*aResult);
          return NS_OK;
        }

        textOffset += textLength;

        // If there aren't any more siblings after this text node,
        // return the point at the end of this text node!

        if (i == lastIndex) {
          *aPosition = textLength;
          *aResult = item;
          NS_ADDREF(*aResult);
          return NS_OK;
        }
      }
      else {
        // Must be a BR node, count it as a newline.

        if (aOffset == textOffset || i == lastIndex) {
          // We've found the correct position, or aOffset takes us
          // beyond the last child under rootNode, just return the point
          // under rootNode that is in front of this br.

          *aPosition = i;
          *aResult = rootNode;
          NS_ADDREF(*aResult);
          return NS_OK;
        }

        ++textOffset;
      }
    }
  }

  return NS_ERROR_FAILURE;
}


/*
Gets the specified text relative to aBoundaryType, which means:
BOUNDARY_CHAR             The character before/at/after the offset is returned.
BOUNDARY_WORD_START       From the word start before/at/after the offset to the next word start.
BOUNDARY_WORD_END         From the word end before/at/after the offset to the next work end.
BOUNDARY_SENTENCE_START   From the sentence start before/at/after the offset to the next sentence start.
BOUNDARY_SENTENCE_END     From the sentence end before/at/after the offset to the next sentence end.
BOUNDARY_LINE_START       From the line start before/at/after the offset to the next line start.
BOUNDARY_LINE_END         From the line end before/at/after the offset to the next line start.
*/
#if 0
nsresult nsHyperTextAccessible::GetTextHelperCore(EGetTextType aType, nsAccessibleTextBoundary aBoundaryType,
                                                  PRInt32 aOffset, PRInt32 *aStartOffset, PRInt32 *aEndOffset,
                                                  nsISelectionController *aSelCon, nsISelection *aDomSel,
                                                  nsISupports *aClosure, nsAString &aText)
{
  PRInt32 rangeCount;
  nsCOMPtr<nsIDOMRange> range, oldRange;
  aDomSel->GetRangeCount(&rangeCount);

  if (rangeCount == 0) { // ever happen?
    SetCaretOffset(aOffset); // a new range will be added here
    rangeCount++;
  }
  aDomSel->GetRangeAt(rangeCount - 1, getter_AddRefs(range));
  NS_ENSURE_TRUE(range, NS_ERROR_FAILURE);

  // backup the original selection range to restore the selection status later
  range->CloneRange(getter_AddRefs(oldRange));

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

  // The start/end focus node may be not our mDOMNode
  nsCOMPtr<nsIDOMNode> startFocusNode, endFocusNode;
  switch (aBoundaryType)
  {
  case BOUNDARY_CHAR:
    if (aType == eGetAfter) { // We need the character next to current position
      aSelCon->CharacterMove(isStep1Forward, PR_FALSE);
      GetCurrentOffset(aClosure, aDomSel, aStartOffset);
    }
    aSelCon->CharacterMove(isStep2Forward, PR_TRUE);
    GetCurrentOffset(aClosure, aDomSel, aEndOffset);
    break;
  case BOUNDARY_WORD_START:
    {
    PRBool dontMove = PR_FALSE;
    // If we are at the word boundary, don't move the caret in the first step
    if (aOffset == 0)
      dontMove = PR_TRUE;
    else {
      PRUnichar prevChar;
      GetCharacterAtOffset(aOffset - 1, &prevChar);
      if (prevChar == ' ' || prevChar == '\t' || prevChar == '\n')
        dontMove = PR_TRUE;
    }
    if (!dontMove) {
      aSelCon->WordMove(isStep1Forward, PR_FALSE); // Move caret to previous/next word start boundary
      GetCurrentOffset(aClosure, aDomSel, aStartOffset);
    }
    aSelCon->WordMove(isStep2Forward, PR_TRUE);  // Select previous/next word
    GetCurrentOffset(aClosure, aDomSel, aEndOffset);
    }
    break;
  case BOUNDARY_LINE_START:
    if (aType != eGetAt) {
      // XXX, don't support yet
      return NS_ERROR_NOT_IMPLEMENTED;
    }
    aSelCon->IntraLineMove(PR_FALSE, PR_FALSE);  // Move caret to the line start
    GetCurrentOffset(aClosure, aDomSel, aStartOffset);
    aSelCon->IntraLineMove(PR_TRUE, PR_TRUE);    // Move caret to the line end and select the whole line
    GetCurrentOffset(aClosure, aDomSel, aEndOffset);
    break;
  case BOUNDARY_WORD_END:
    {
    // please refer to atk implementation (atktext.c)
    // for specification of ATK_TEXT_BOUNDARY_WORD_END when before/at/after offset
    // XXX, need to follow exact definition of ATK_TEXT_BOUNDARY_WORD_END

    if (aType != eGetAt) {
      // XXX, don't support yet
      return NS_ERROR_NOT_IMPLEMENTED;
    }

    // Example of current code: _AB_CD_E_ ("_" is space)
    // offset      return string   startOffset endOffset
    //      0      AB_             1           4
    //      1      AB_             1           4
    //      2      AB_             1           4
    //      3      AB_             1           4
    //      4      CD_             4           7
    //      5      CD_             4           7
    //      6      CD_             4           7
    //      7      E_              7           9
    //      8      E_              7           9

    PRUnichar offsetChar;
    nsresult rv = GetCharacterAtOffset(aOffset, &offsetChar);
    NS_ENSURE_SUCCESS(rv, rv);
    PRBool isOffsetEmpty =  offsetChar == ' ' || offsetChar == '\t' || offsetChar == '\n';

    PRInt32 stepBackwardCount = 0; // Times of move backward to find the word(e.g. "AB_") start boundary
    if (aOffset == 0) {
      if (isOffsetEmpty)
        aSelCon->WordMove(PR_TRUE, PR_FALSE); // Move caret to the first word start boundary
    }
    else {
      PRUnichar prevChar;
      GetCharacterAtOffset(aOffset - 1, &prevChar);
      PRBool isPrevEmpty =  prevChar == ' ' || prevChar == '\t' || prevChar == '\n';
      if (!isPrevEmpty)
        stepBackwardCount = 1;
      else if (isOffsetEmpty)
        stepBackwardCount = 2;
      else
        stepBackwardCount = 0;

      PRInt32 step;
      for (step = 0; step < stepBackwardCount; step++)
        aSelCon->WordMove(PR_FALSE, PR_FALSE); // Move caret to current word start boundary
    }

    GetCurrentOffset(aClosure, aDomSel, aStartOffset);
    // Move twice to select a "word"
    aSelCon->WordMove(PR_TRUE, PR_TRUE);
    aSelCon->WordMove(PR_TRUE, PR_TRUE);
    GetCurrentOffset(aClosure, aDomSel, aEndOffset);
    }
    break;
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
  nsresult rv = aDomSel->ToString(getter_Copies(text));
  aDomSel->RemoveAllRanges();
  // restore the original selection range
  aDomSel->AddRange(oldRange);
  NS_ENSURE_SUCCESS(rv, rv);

  aText = text;

  // Ensure aStartOffset <= aEndOffset
  if (*aStartOffset > *aEndOffset) {
    PRInt32 tmp = *aStartOffset;
    *aStartOffset = *aEndOffset;
    *aEndOffset = tmp;
  }

  return NS_OK;
}
#endif

nsresult nsHyperTextAccessible::GetTextHelper(EGetTextType aType, nsAccessibleTextBoundary aBoundaryType,
                                              PRInt32 aOffset, PRInt32 *aStartOffset, PRInt32 *aEndOffset,
                                              nsISupports *aClosure, nsAString &aText)
{
  return NS_ERROR_NOT_IMPLEMENTED;

  // XXX Remove this hacky method! Get the text without moving the caret or selection!
  // We need to look at how nsSelection::MoveCaret works and take the useful
  // pieces from that:
  // http://lxr.mozilla.org/seamonkey/source/layout/generic/nsSelection.cpp#1309
  // Things like:
  // pos.SetData()
  // frame->PeekOffset()
  // GetFrameForNodeOffset(pos.mResultContent, pos.mContentOffset, tHint, &theFrame, &currentOffset);
  // theFrame->GetOffsets(frameStart, frameEnd);
#if 0
  NS_ENSURE_TRUE((aOffset >= 0), NS_ERROR_INVALID_ARG);
  
  nsCOMPtr<nsISelectionController> selCon;
  nsCOMPtr<nsISelection> domSel;

  nsresult rv = GetSelections(getter_AddRefs(selCon), getter_AddRefs(domSel));
  NS_ENSURE_SUCCESS(rv, rv);

  //backup old settings
  PRInt16 displaySelection;
  selCon->GetDisplaySelection(&displaySelection);
  PRBool caretEnable;
  selCon->GetCaretEnabled(&caretEnable);

  //turn off display and caret
  selCon->SetDisplaySelection(nsISelectionController::SELECTION_HIDDEN);
  selCon->SetCaretEnabled(PR_FALSE);

  //turn off nsCaretAccessible::NotifySelectionChanged
  gSuppressNotifySelectionChanged = PR_TRUE;

  PRInt32 caretOffset = -1;
  if (NS_SUCCEEDED(GetCaretOffset(&caretOffset))) {
    if (caretOffset != aOffset)
      SetCaretOffset(aOffset);
  }

  *aStartOffset = *aEndOffset = aOffset;
  rv = GetTextHelperCore(aType, aBoundaryType, aOffset, aStartOffset, aEndOffset, selCon, domSel, aClosure, aText);

  //restore caret offset
  if (caretOffset >= 0) {
    SetCaretOffset(caretOffset);
  }

  //turn on nsCaretAccessible::NotifySelectionChanged
  gSuppressNotifySelectionChanged = PR_FALSE;

  //restore old settings
  selCon->SetDisplaySelection(displaySelection);
  selCon->SetCaretEnabled(caretEnable);

  return rv;
#endif
}

/**
  * nsIAccessibleText impl.
  */
NS_IMETHODIMP nsHyperTextAccessible::GetTextBeforeOffset(PRInt32 aOffset, nsAccessibleTextBoundary aBoundaryType,
                                                         PRInt32 *aStartOffset, PRInt32 *aEndOffset, nsAString & aText)
{
  return GetTextHelper(eGetBefore, aBoundaryType, aOffset, aStartOffset, aEndOffset, nsnull, aText);
}

NS_IMETHODIMP nsHyperTextAccessible::GetTextAtOffset(PRInt32 aOffset, nsAccessibleTextBoundary aBoundaryType,
                                                     PRInt32 *aStartOffset, PRInt32 *aEndOffset, nsAString & aText)
{
  return GetTextHelper(eGetAt, aBoundaryType, aOffset, aStartOffset, aEndOffset, nsnull, aText);
}

NS_IMETHODIMP nsHyperTextAccessible::GetTextAfterOffset(PRInt32 aOffset, nsAccessibleTextBoundary aBoundaryType,
                                                        PRInt32 *aStartOffset, PRInt32 *aEndOffset, nsAString & aText)
{
  return GetTextHelper(eGetAfter, aBoundaryType, aOffset, aStartOffset, aEndOffset, nsnull, aText);
}

NS_IMETHODIMP nsHyperTextAccessible::GetAttributeRange(PRInt32 aOffset, PRInt32 *aRangeStartOffset, 
                                                       PRInt32 *aRangeEndOffset, nsIAccessible **aAccessibleWithAttrs)
{
  // Return the range of text with common attributes around aOffset
  *aRangeStartOffset = *aRangeEndOffset = 0;
  *aAccessibleWithAttrs = nsnull;

  nsCOMPtr<nsIAccessible> accessible;
  
  while (NextChild(accessible)) {
    PRInt32 length = 1;
    if (Role(accessible) == ROLE_TEXT_LEAF) {
      nsCOMPtr<nsPIAccessNode> accessNode(do_QueryInterface(accessible));
      nsIFrame *frame = accessNode->GetFrame();
      if (frame) {
        nsITextContent *textContent = NS_STATIC_CAST(nsITextContent*, frame->GetContent());
        NS_ASSERTION(textContent, "No text content for a ROLE_TEXT?");
        length = textContent->TextLength();
      }
      else {
        break;
      }
    }
    else {
      length = 1;
    }
    if (*aRangeStartOffset + length > aOffset) {
      *aRangeEndOffset = *aRangeStartOffset + length;
      NS_ADDREF(*aAccessibleWithAttrs = accessible);
      return NS_OK;
    }
    *aRangeStartOffset += length;
  }

  return NS_ERROR_FAILURE;
}

/*
 * Given an offset, the x, y, width, and height values are filled appropriately.
 */
NS_IMETHODIMP nsHyperTextAccessible::GetCharacterExtents(PRInt32 aOffset, PRInt32 *aX, PRInt32 *aY,
                                                         PRInt32 *aWidth, PRInt32 *aHeight,
                                                         nsAccessibleCoordType aCoordType)
{
  *aX = *aY = *aWidth = *aHeight = 0;
  nsCOMPtr<nsIPresShell> shell = GetPresShell();
  NS_ENSURE_TRUE(shell, NS_ERROR_FAILURE);

  nsAutoString firstChar;
  // Will convert aOffset into offset into corresponding descendant frame
  nsIFrame *frame = GetStartPosAndText(aOffset, aOffset + 1, firstChar);
  if (!frame) {
    return NS_ERROR_FAILURE;
  }

  nsIntRect frameScreenRect = frame->GetScreenRectExternal();
  if (firstChar.First() != kEmbeddedObjectChar) {
    nsCOMPtr<nsIRenderingContext> rc;
    shell->CreateRenderingContext(frame, getter_AddRefs(rc));
    NS_ENSURE_TRUE(rc, NS_ERROR_FAILURE);

    const nsStyleFont *font = frame->GetStyleFont();
    const nsStyleVisibility *visibility = frame->GetStyleVisibility();

    nsresult rv = rc->SetFont(font->mFont, visibility->mLangGroup);
    NS_ENSURE_SUCCESS(rv, rv);

    nsIFontMetrics *fm;
    rv = rc->GetFontMetrics(fm);
    NS_ENSURE_SUCCESS(rv, rv);

    nsPresContext *context = shell->GetPresContext();
    float t2p = context->TwipsToPixels();

    //Getting width
    nscoord tmpWidth;
    if (NS_SUCCEEDED(rc->GetWidth(firstChar.First(), tmpWidth))) {
      *aWidth = NSTwipsToIntPixels(tmpWidth, t2p);
    }

    //Getting height
    nscoord tmpHeight;
    if (NS_SUCCEEDED(fm->GetHeight(tmpHeight))) {
      *aHeight = NSTwipsToIntPixels(tmpHeight, t2p);
    }

    if (aOffset > 0) {
      //add the width of the string before current char
      nsITextContent *textContent =
        NS_STATIC_CAST(nsITextContent*, frame->GetContent());
      NS_ASSERTION(textContent, "No embedded char, so this should have from text");
      const nsTextFragment *textFrag = textContent->Text();
      PRInt32 beforeWidth;
      if (textFrag->Is2b()) {
        if (NS_SUCCEEDED(rc->GetWidth(textFrag->Get2b(), aOffset, beforeWidth))) {
          frameScreenRect.x += NSTwipsToIntPixels(beforeWidth, t2p);
        }
      }
      else if (NS_SUCCEEDED(rc->GetWidth(textFrag->Get1b(), aOffset, beforeWidth))) {
        frameScreenRect.x += NSTwipsToIntPixels(beforeWidth, t2p);
      }
    }
  }

  *aX = frameScreenRect.x;
  *aY = frameScreenRect.y;
  if (aCoordType == COORD_TYPE_WINDOW) {
    //co-ord type = window
    nsCOMPtr<nsIDocument> doc = shell->GetDocument();
    nsCOMPtr<nsIDOMDocumentView> docView(do_QueryInterface(doc));
    NS_ENSURE_TRUE(docView, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDOMAbstractView> abstractView;
    docView->GetDefaultView(getter_AddRefs(abstractView));
    NS_ENSURE_TRUE(abstractView, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDOMWindowInternal> windowInter(do_QueryInterface(abstractView));
    NS_ENSURE_TRUE(windowInter, NS_ERROR_FAILURE);

    PRInt32 screenX, screenY;
    if (NS_FAILED(windowInter->GetScreenX(&screenX)) ||
        NS_FAILED(windowInter->GetScreenY(&screenY))) {
      return NS_ERROR_FAILURE;
    }
    *aX -= screenX;
    *aY -= screenY;
  }
  // else default: co-ord type = screen

  return NS_OK;
}

/*
 * Gets the offset of the character located at coordinates x and y. x and y are interpreted as being relative to
 * the screen or this widget's window depending on coords.
 */
NS_IMETHODIMP nsHyperTextAccessible::GetOffsetAtPoint(PRInt32 aX, PRInt32 aY, nsAccessibleCoordType aCoordType, PRInt32 *aOffset)
{
  *aOffset = 0;
  nsCOMPtr<nsIPresShell> shell = GetPresShell();
  if (!shell) {
    return NS_ERROR_FAILURE;
  }
  nsIFrame *frame = GetFrame();
  if (!frame) {
    return NS_ERROR_FAILURE;
  }
  nsIntRect frameScreenRect = frame->GetScreenRectExternal();

  if (aCoordType == COORD_TYPE_WINDOW) {
    nsCOMPtr<nsIDocument> doc = shell->GetDocument();
    nsCOMPtr<nsIDOMDocumentView> docView(do_QueryInterface(doc));
    NS_ENSURE_TRUE(docView, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDOMAbstractView> abstractView;
    docView->GetDefaultView(getter_AddRefs(abstractView));
    NS_ENSURE_TRUE(abstractView, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDOMWindowInternal> windowInter(do_QueryInterface(abstractView));
    NS_ENSURE_TRUE(windowInter, NS_ERROR_FAILURE);

    PRInt32 windowX, windowY;
    if (NS_FAILED(windowInter->GetScreenX(&windowX)) ||
        NS_FAILED(windowInter->GetScreenY(&windowY))) {
      return NS_ERROR_FAILURE;
    }
    aX += windowX;
    aY += windowY;
  }
  // aX, aY are currently screen coordinates, and we need to turn them into
  // frame coordinates relative to the current accessible
  if (!frameScreenRect.Contains(aX, aY)) {
    return NS_ERROR_FAILURE;
  }
  nsPoint pointInFrame(aX - frameScreenRect.x, aY - frameScreenRect.y);
  // Go through the frames to check if each one has the point.
  // When one does, add up the character offsets until we have a match

  // We have an point in an accessible child of this, now we need to add up the
  // offsets before it to what we already have
  nsCOMPtr<nsIAccessible> accessible;

  while (NextChild(accessible)) {
    PRBool finished = frame->GetRect().Contains(pointInFrame);
    nsCOMPtr<nsPIAccessNode> accessNode(do_QueryInterface(accessible));
    nsIFrame *frame = accessNode->GetFrame();
    if (Role(accessible) == ROLE_TEXT_LEAF) {
      if (frame) {
        nsITextContent *textContent = NS_STATIC_CAST(nsITextContent*, frame->GetContent());
        if (finished) {
          nsCOMPtr<nsIRenderingContext> rc;
          shell->CreateRenderingContext(frame, getter_AddRefs(rc));
          NS_ENSURE_TRUE(rc, NS_ERROR_FAILURE);
          const nsTextFragment *textFrag = textContent->Text();
          // For each character in textContent, add to totalWidth
          // until it is wider than the x coordinate we are looking for
          PRInt32 length = textFrag->GetLength();
          PRInt32 totalWidth = 0;
          for (PRInt32 index = 0; index < length; index ++) {
            nscoord charWidth;
            if (NS_FAILED(rc->GetWidth(textFrag->CharAt(index), charWidth))) {
              break;
            }
            totalWidth += charWidth;
            if (pointInFrame.x < totalWidth) {
              *aOffset += index;
              return NS_OK;
            }
          }
          return NS_ERROR_FAILURE;
        }
        *aOffset += textContent->TextLength();
      }
    }
    else if (finished) {
      break;
    }
    else {
      ++*aOffset;
    }
  }
  return NS_OK;
}

// ------- nsIAccessibleHyperText ---------------
NS_IMETHODIMP nsHyperTextAccessible::GetLinks(PRInt32 *aLinks)
{
  *aLinks = 0;
  if (!mDOMNode) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIAccessible> accessible;

  while (NextChild(accessible)) {
    if (Role(accessible) != ROLE_TEXT_LEAF) {
      ++*aLinks;
    }
  }
  return NS_OK;
}


NS_IMETHODIMP nsHyperTextAccessible::GetLink(PRInt32 aIndex, nsIAccessibleHyperLink **aLink)
{
  *aLink = nsnull;
  if (!mDOMNode) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIAccessible> accessible;

  while (NextChild(accessible)) {
    if (Role(accessible) != ROLE_TEXT_LEAF && aIndex-- == 0) {
      CallQueryInterface(accessible, aLink);
      return NS_OK;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsHyperTextAccessible::GetLinkIndex(PRInt32 aCharIndex, PRInt32 *aLinkIndex)
{
  *aLinkIndex = -1;
  PRInt32 characterCount = 0;
  if (!mDOMNode) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIAccessible> accessible;

  while (NextChild(accessible)) {
    if (Role(accessible) == ROLE_TEXT_LEAF) {
      nsCOMPtr<nsPIAccessNode> accessNode(do_QueryInterface(accessible));
      nsIFrame *frame = accessNode->GetFrame();
      if (frame) {
        nsITextContent *textContent = NS_STATIC_CAST(nsITextContent*, frame->GetContent());
        characterCount += textContent->TextLength();
      }
    }
    else {
      if (characterCount == aCharIndex) {
        return NS_OK;
      }
      else if (characterCount > aCharIndex) {
        return NS_ERROR_FAILURE;
      }
      ++ characterCount;
      ++ *aLinkIndex;
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHyperTextAccessible::GetSelectedLinkIndex(PRInt32 *aSelectedLinkIndex)
{
  *aSelectedLinkIndex = 0;
  if (!mDOMNode && !nsAccessNode::gLastFocusedNode) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIAccessible> focusedAccessible;

  nsCOMPtr<nsIAccessibilityService> accService(do_GetService("@mozilla.org/accessibilityService;1"));
  if (NS_FAILED(accService->GetAccessibleInWeakShell(nsAccessNode::gLastFocusedNode, mWeakShell,
                                                     getter_AddRefs(focusedAccessible)))) {
    return NS_ERROR_FAILURE;
  }

  // Make sure focused accessible is a child of ours before doing a lot of work
  nsCOMPtr<nsIAccessible> focusedParent;
  focusedAccessible->GetParent(getter_AddRefs(focusedParent));
  if (focusedParent != this) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIAccessible> accessible;

  while (NextChild(accessible)) {
    if (accessible == focusedAccessible) {
      return NS_OK;
    }
    if (Role(accessible) != ROLE_TEXT_LEAF) {
      ++ *aSelectedLinkIndex;
    }
  }
  NS_NOTREACHED("Should not reach here, focus of parent was this accessible");
  return NS_ERROR_FAILURE;
}

/**
  * nsIAccessibleEditableText impl.
  */
NS_IMETHODIMP nsHyperTextAccessible::SetAttributes(PRInt32 aStartPos, PRInt32 aEndPos,
                                                   nsISupports *aAttributes)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHyperTextAccessible::SetTextContents(const nsAString &aText)
{
  // XXX Need to somehow deal with embedded object chars
#if 0
  // XXX Put in accessible classes for these elements
  nsCOMPtr<nsIDOMHTMLTextAreaElement> textArea(do_QueryInterface(mTextNode));
  if (textArea)
    return textArea->SetValue(aText);

  nsCOMPtr<nsIDOMHTMLInputElement> inputElement(do_QueryInterface(mTextNode));
  if (inputElement)
    return inputElement->SetValue(aText);

  //XXX, editor doesn't support this method yet
#endif

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHyperTextAccessible::InsertText(const nsAString &aText, PRInt32 aPosition)
{
  // XXX Need to somehow deal with embedded object chars
  if (NS_SUCCEEDED(SetSelectionRange(aPosition, aPosition))) {
    nsCOMPtr<nsIEditor> editor = GetEditor();
    nsCOMPtr<nsIPlaintextEditor> peditor(do_QueryInterface(editor));
    return peditor ? peditor->InsertText(aText) : NS_ERROR_FAILURE;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHyperTextAccessible::CopyText(PRInt32 aStartPos, PRInt32 aEndPos)
{
  // XXX Need to somehow deal with embedded object chars
  nsCOMPtr<nsIEditor> editor = GetEditor();
  if (editor && NS_SUCCEEDED(SetSelectionRange(aStartPos, aEndPos)))
    return editor->Copy();

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHyperTextAccessible::CutText(PRInt32 aStartPos, PRInt32 aEndPos)
{
  // XXX Need to somehow deal with embedded object chars
  nsCOMPtr<nsIEditor> editor = GetEditor();
  if (editor && NS_SUCCEEDED(SetSelectionRange(aStartPos, aEndPos)))
    return editor->Cut();

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHyperTextAccessible::DeleteText(PRInt32 aStartPos, PRInt32 aEndPos)
{
  // XXX Need to somehow deal with embedded object chars
  nsCOMPtr<nsIEditor> editor = GetEditor();
  if (editor && NS_SUCCEEDED(SetSelectionRange(aStartPos, aEndPos)))
    return editor->DeleteSelection(nsIEditor::eNone);

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHyperTextAccessible::PasteText(PRInt32 aPosition)
{
  // XXX Need to somehow deal with embedded object chars
  nsCOMPtr<nsIEditor> editor = GetEditor();
  if (editor && NS_SUCCEEDED(SetSelectionRange(aPosition, aPosition)))
    return editor->Paste(nsIClipboard::kGlobalClipboard);

  return NS_ERROR_FAILURE;
}

/**
  * nsIEditActionListener impl.
  */
NS_IMETHODIMP nsHyperTextAccessible::WillCreateNode(const nsAString& aTag,
                                                    nsIDOMNode *aParent, PRInt32 aPosition)
{
  return NS_OK;
}

NS_IMETHODIMP nsHyperTextAccessible::DidCreateNode(const nsAString& aTag, nsIDOMNode *aNode,
                                                   nsIDOMNode *aParent, PRInt32 aPosition, nsresult aResult)
{
  return NS_OK;
}

NS_IMETHODIMP nsHyperTextAccessible::WillInsertNode(nsIDOMNode *aNode, nsIDOMNode *aParent,
                                                    PRInt32 aPosition)
{
  return NS_OK;
}

NS_IMETHODIMP nsHyperTextAccessible::DidInsertNode(nsIDOMNode *aNode, nsIDOMNode *aParent,
                                                   PRInt32 aPosition, nsresult aResult)
{
  AtkTextChange textData;

  nsCOMPtr<nsITextContent> textContent(do_QueryInterface(aNode));
  if (textContent) {
    textData.add = PR_TRUE;
    textData.length = textContent->TextLength();
    nsCOMPtr<nsIEditor> editor = GetEditor();
    DOMPointToOffset(editor, aNode, 0, &textData.start);
    FireTextChangeEvent(&textData);
  }
  return NS_OK;
}

NS_IMETHODIMP nsHyperTextAccessible::WillDeleteNode(nsIDOMNode *aChild)
{
  AtkTextChange textData;

  textData.add = PR_FALSE;
  nsCOMPtr<nsITextContent> textContent(do_QueryInterface(aChild));
  if (textContent) {
    textData.length = textContent->TextLength();
  }
  else {
    //XXX, don't fire event for the last br
    nsCOMPtr<nsIDOMHTMLBRElement> br(do_QueryInterface(aChild));
    if (br)
      textData.length = 1;
    else
      return NS_OK;
  }

  nsCOMPtr<nsIEditor> editor = GetEditor();
  DOMPointToOffset(editor, aChild, 0, &textData.start);
  return FireTextChangeEvent(&textData);
}

NS_IMETHODIMP nsHyperTextAccessible::DidDeleteNode(nsIDOMNode *aChild, nsresult aResult)
{
  return NS_OK;
}

NS_IMETHODIMP nsHyperTextAccessible::WillSplitNode(nsIDOMNode *aExistingRightNode, PRInt32 aOffset)
{
  return NS_OK;
}

NS_IMETHODIMP nsHyperTextAccessible::DidSplitNode(nsIDOMNode *aExistingRightNode, PRInt32 aOffset,
                                                  nsIDOMNode *aNewLeftNode, nsresult aResult)
{
  return NS_OK;
}

NS_IMETHODIMP nsHyperTextAccessible::WillJoinNodes(nsIDOMNode *aLeftNode,
                                                   nsIDOMNode *aRightNode, nsIDOMNode *aParent)
{
  return NS_OK;
}

NS_IMETHODIMP nsHyperTextAccessible::DidJoinNodes(nsIDOMNode *aLeftNode, nsIDOMNode *aRightNode,
                                                  nsIDOMNode *aParent, nsresult aResult)
{
  return NS_OK;
}

NS_IMETHODIMP nsHyperTextAccessible::WillInsertText(nsIDOMCharacterData *aTextNode,
                                                    PRInt32 aOffset, const nsAString& aString)
{
  return NS_OK;
}

NS_IMETHODIMP nsHyperTextAccessible::DidInsertText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset,
                                                   const nsAString& aString, nsresult aResult)
{
  AtkTextChange textData;

  textData.add = PR_TRUE;
  textData.length = aString.Length();
  nsCOMPtr<nsIEditor> editor = GetEditor();
  DOMPointToOffset(editor, aTextNode, aOffset, &textData.start);
  return FireTextChangeEvent(&textData);
}

NS_IMETHODIMP nsHyperTextAccessible::WillDeleteText(nsIDOMCharacterData *aTextNode,
                                                    PRInt32 aOffset, PRInt32 aLength)
{
  AtkTextChange textData;

  textData.add = PR_FALSE;
  textData.length = aLength;
  nsCOMPtr<nsIEditor> editor = GetEditor();
  if (!editor) {
    return NS_ERROR_FAILURE;
  }
  DOMPointToOffset(editor, aTextNode, aOffset, &textData.start);
  return FireTextChangeEvent(&textData);
}

NS_IMETHODIMP nsHyperTextAccessible::DidDeleteText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset,
                                                   PRInt32 aLength, nsresult aResult)
{
  return NS_OK;
}

NS_IMETHODIMP nsHyperTextAccessible::WillDeleteSelection(nsISelection *aSelection)
// <input> & <textarea> fires this event while deleting text
// <editor> fires WillDeleteText/WillDeleteNode instead
{
  PRInt32 selectionStart, selectionEnd;
  nsresult rv = GetSelectionRange(&selectionStart, &selectionEnd);
  NS_ENSURE_SUCCESS(rv, rv);

  AtkTextChange textData;

  textData.add = PR_FALSE;
  textData.start = PR_MIN(selectionStart, selectionEnd);
  textData.length = PR_ABS(selectionEnd - selectionStart);
  return FireTextChangeEvent(&textData);
}

NS_IMETHODIMP nsHyperTextAccessible::DidDeleteSelection(nsISelection *aSelection)
{
  return NS_OK;
}

/**
  * =================== Caret & Selection ======================
  */

nsresult nsHyperTextAccessible::FireTextChangeEvent(AtkTextChange *aTextData)
{
  nsCOMPtr<nsIAccessible> accessible(do_QueryInterface(NS_STATIC_CAST(nsIAccessibleText*, this)));
  nsCOMPtr<nsPIAccessible> privAccessible(do_QueryInterface(accessible));
  if (privAccessible) {
#ifdef DEBUG
    printf("  [start=%d, length=%d, add=%d]\n", aTextData->start, aTextData->length, aTextData->add);
#endif
    privAccessible->FireToolkitEvent(nsIAccessibleEvent::EVENT_ATK_TEXT_CHANGE, accessible, aTextData);
  }
  return NS_OK;
}

nsresult nsHyperTextAccessible::GetSelectionRange(PRInt32 *aStartPos, PRInt32 *aEndPos)
{
  *aStartPos = 0;
  *aEndPos = 0;

#if 0
  nsITextControlFrame *textFrame = GetHyperTextFrame();
  if (textFrame) {
    return textFrame->GetSelectionRange(aStartPos, aEndPos);
  }
  // XXX mEditor case (for textfields and textareas)
  else {
    // editor, revised according to nsTextControlFrame::GetSelectionRange
    NS_ENSURE_TRUE(mEditor, NS_ERROR_FAILURE);
    nsCOMPtr<nsISelection> selection;
    nsresult rv = mEditor->GetSelection(getter_AddRefs(selection));  
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);

    PRInt32 numRanges = 0;
    selection->GetRangeCount(&numRanges);
    NS_ENSURE_TRUE(numRanges >= 1, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDOMRange> firstRange;
    rv = selection->GetRangeAt(0, getter_AddRefs(firstRange));
    NS_ENSURE_TRUE(firstRange, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDOMNode> startNode, endNode;
    PRInt32 startOffset = 0, endOffset = 0;

    // Get the start point of the range.
    rv = firstRange->GetStartContainer(getter_AddRefs(startNode));
    NS_ENSURE_TRUE(startNode, NS_ERROR_FAILURE);

    rv = firstRange->GetStartOffset(&startOffset);
    NS_ENSURE_SUCCESS(rv, rv);

    // Get the end point of the range.
    rv = firstRange->GetEndContainer(getter_AddRefs(endNode));
    NS_ENSURE_TRUE(endNode, NS_ERROR_FAILURE);

    rv = firstRange->GetEndOffset(&endOffset);
    NS_ENSURE_SUCCESS(rv, rv);

    // Convert the start/end point to a selection offset.
    rv = DOMPointToOffset(mEditor, startNode, startOffset, aStartPos);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = DOMPointToOffset(mEditor, endNode, endOffset, aEndPos);
    NS_ENSURE_SUCCESS(rv, rv);
  }
#endif

  return NS_OK;
}

nsresult nsHyperTextAccessible::SetSelectionRange(PRInt32 aStartPos, PRInt32 aEndPos)
{
#if 0
  nsITextControlFrame *frame = GetHyperTextFrame();
  if (textFrame) {
    return textFrame->SetSelectionRange(aStartPos, aEndPos);
  }
  else {
    // editor, revised according to nsTextControlFrame::SetSelectionRange
    NS_ENSURE_TRUE(mEditor, NS_ERROR_FAILURE);
    if (aStartPos > aEndPos)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDOMNode> startNode, endNode;
    PRInt32 startOffset, endOffset;

    nsresult rv = OffsetToDOMPoint(mEditor, aStartPos, getter_AddRefs(startNode), &startOffset);
    NS_ENSURE_SUCCESS(rv, rv);

    if (aStartPos == aEndPos) {
      endNode   = startNode;
      endOffset = startOffset;
    }
    else {
      rv = OffsetToDOMPoint(mEditor, aEndPos, getter_AddRefs(endNode), &endOffset);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // Create a new range to represent the new selection.
    nsCOMPtr<nsIDOMRange> range = do_CreateInstance(kRangeCID);
    NS_ENSURE_TRUE(range, NS_ERROR_FAILURE);

    rv = range->SetStart(startNode, startOffset);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = range->SetEnd(endNode, endOffset);
    NS_ENSURE_SUCCESS(rv, rv);

    // Get the selection, clear it and add the new range to it!
    nsCOMPtr<nsISelection> selection;
    mEditor->GetSelection(getter_AddRefs(selection));
    NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);

    rv = selection->RemoveAllRanges();
    NS_ENSURE_SUCCESS(rv, rv);

    return selection->AddRange(range);
  }
#endif
  return NS_OK;
}

/*
 * Gets the offset position of the caret (cursor).
 */
NS_IMETHODIMP nsHyperTextAccessible::GetCaretOffset(PRInt32 *aCaretOffset)
{
  *aCaretOffset = 0;

  PRInt32 startPos, endPos;
  nsresult rv = GetSelectionRange(&startPos, &endPos);
  NS_ENSURE_SUCCESS(rv, rv);

  if (startPos == endPos) { // selection must be collapsed
    *aCaretOffset = startPos;
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

nsresult nsHyperTextAccessible::GetSelections(nsISelectionController **aSelCon, nsISelection **aDomSel)
{
  *aSelCon = nsnull;
  *aDomSel = nsnull;
#if 0
  nsIContent *hyperTextContent = GetHyperTextContent();
  if (!hyperTextContent) {
    return NS_ERROR_FAILURE;
  }
  nsIDocument *doc = hyperTextContent->GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  nsIPresShell *shell = doc->GetShellAt(0);
  NS_ENSURE_TRUE(shell, NS_ERROR_FAILURE);

  nsIFrame *frame = shell->GetPrimaryFrameFor(hyperTextContent);
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  // Get the selection and selection controller
  nsCOMPtr<nsISelectionController> selCon;
  nsCOMPtr<nsISelection> domSel;
  frame->GetSelectionController(shell->GetPresContext(),
                                getter_AddRefs(selCon));
  if (selCon) {
    selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSel));
  }

  NS_ENSURE_TRUE(selCon && domSel, NS_ERROR_FAILURE);

  PRBool isSelectionCollapsed;
  domSel->GetIsCollapsed(&isSelectionCollapsed);
  // Don't perform any actions when the selection is not collapsed
  if (isSelectionCollapsed) {
    return NS_ERROR_FAILURE;
  }

  NS_ADDREF(*aSelCon = selCon);
  NS_ADDREF(*aDomSel = domSel);
#endif
  return NS_OK;
}

/*
 * Gets the number of selected regions.
 */
NS_IMETHODIMP nsHyperTextAccessible::GetSelectionCount(PRInt32 *aSelectionCount)
{
  nsCOMPtr<nsISelection> domSel;
  nsresult rv = GetSelections(nsnull, getter_AddRefs(domSel));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool isSelectionCollapsed;
  rv = domSel->GetIsCollapsed(&isSelectionCollapsed);
  NS_ENSURE_SUCCESS(rv, rv);

  if (isSelectionCollapsed)
    *aSelectionCount = 0;

  rv = domSel->GetRangeCount(aSelectionCount);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
#if 0
  // XXX impl #2
  *aSelectionCount = 0;

  PRInt32 selCount;
  PRUint32 index, count;
  nsCOMPtr<nsIMutableArray> textChildren(GetTextChildren());
  textChildren->GetLength(&count);
  for (index = 0; index < count; index++) {
    nsCOMPtr<nsIDOMNode> domNode(do_QueryElementAt(textChildren, index));
    nsAccessibleText accText(domNode);
    if (NS_SUCCEEDED(accText.GetSelectionCount(&selCount)))
      *aSelectionCount += selCount;
  }
  return NS_OK;
#endif
}

/*
 * Gets the start and end offset of the specified selection.
 */
NS_IMETHODIMP nsHyperTextAccessible::GetSelectionBounds(PRInt32 aSelectionNum, PRInt32 *aStartOffset, PRInt32 *aEndOffset)
{
  nsCOMPtr<nsISelection> domSel;
  nsresult rv = GetSelections(nsnull, getter_AddRefs(domSel));
  NS_ENSURE_SUCCESS(rv, rv);

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
NS_IMETHODIMP nsHyperTextAccessible::SetSelectionBounds(PRInt32 aSelectionNum, PRInt32 aStartOffset, PRInt32 aEndOffset)
{
  nsCOMPtr<nsISelection> domSel;
  nsresult rv = GetSelections(nsnull, getter_AddRefs(domSel));
  NS_ENSURE_SUCCESS(rv, rv);

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
  PRInt32 oldEndOffset;
  range->GetEndOffset(&oldEndOffset);
  // to avoid set start point after the current end point
  if (aStartOffset < oldEndOffset) {
    range->SetStart(startParent, aStartOffset);
    range->SetEnd(endParent, aEndOffset);
  }
  else {
    range->SetEnd(endParent, aEndOffset);
    range->SetStart(startParent, aStartOffset);
  }
  return NS_OK;
}

/*
 * Adds a selection bounded by the specified offsets.
 */
NS_IMETHODIMP nsHyperTextAccessible::AddSelection(PRInt32 aStartOffset, PRInt32 aEndOffset)
{
  nsCOMPtr<nsISelectionController> selCon;
  nsCOMPtr<nsISelection> domSel;

  if (NS_FAILED(GetSelections(getter_AddRefs(selCon), getter_AddRefs(domSel))))
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMRange> range(do_CreateInstance(kRangeCID));
  if (! range)
    return NS_ERROR_OUT_OF_MEMORY;

  range->SetStart(mDOMNode, aStartOffset);
  range->SetEnd(mDOMNode, aEndOffset);
  return domSel->AddRange(range);
}

/*
 * Removes the specified selection.
 */
NS_IMETHODIMP nsHyperTextAccessible::RemoveSelection(PRInt32 aSelectionNum)
{
  nsCOMPtr<nsISelection> domSel;
  nsresult rv = GetSelections(nsnull, getter_AddRefs(domSel));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 rangeCount;
  domSel->GetRangeCount(&rangeCount);
  if (aSelectionNum < 0 || aSelectionNum >= rangeCount)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIDOMRange> range;
  domSel->GetRangeAt(aSelectionNum, getter_AddRefs(range));
  return domSel->RemoveRange(range);
}

NS_IMETHODIMP nsHyperTextAccessible::SetCaretOffset(PRInt32 aCaretOffset)
{
  return SetSelectionRange(aCaretOffset, aCaretOffset);
#if 0
  // impl #2
  PRInt32 beforeLength;
  nsIDOMNode* domNode = FindTextNodeByOffset(aCaretOffset, beforeLength);
  if (domNode) {
    nsAccessibleText accText(domNode);
    return accText.SetCaretOffset(aCaretOffset - beforeLength);
  }
  return NS_ERROR_INVALID_ARG;
#endif
#if 0
  // impl #3
  nsCOMPtr<nsISelection> domSel;
  nsresult rv = GetSelections(nsnull, getter_AddRefs(domSel));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMRange> range(do_CreateInstance(kRangeCID));
  NS_ENSURE_TRUE(range, NS_ERROR_OUT_OF_MEMORY);

  range->SetStart(mDOMNode, aCaretOffset);
  range->SetEnd(mDOMNode, aCaretOffset);
  domSel->RemoveAllRanges();
  return domSel->AddRange(range);
#endif
}

