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
    // We need XUL doc check for now because for now nsDocAccessible must inherit from nsHyperTextAccessible
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
    if (IsEmbeddedObject(accessible)) {
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
      *aRole = ROLE_PARAGRAPH;
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
nsIFrame* nsHyperTextAccessible::GetPosAndText(PRInt32& aStartOffset, PRInt32& aEndOffset, nsAString *aText, nsIFrame **aEndFrame)
{
  PRInt32 startOffset = aStartOffset;
  PRInt32 endOffset = aEndOffset;

  if (aText) {
    aText->Truncate();
  }
  if (endOffset < 0) {
    const PRInt32 kMaxTextLength = 32767;
    endOffset = kMaxTextLength; // Max end offset
  }
  else if (startOffset > endOffset) {
    return nsnull;
  }

  nsIFrame *startFrame = nsnull;
  if (aEndFrame) {
    *aEndFrame = nsnull;
  }
  nsCOMPtr<nsIAccessible> accessible;

  while (NextChild(accessible)) {
    nsCOMPtr<nsPIAccessNode> accessNode(do_QueryInterface(accessible));
    nsIFrame *frame = accessNode->GetFrame();
    if (Role(accessible) == ROLE_TEXT_LEAF) {
      if (frame) {
        // Avoid string copiesn
        nsITextContent *textContent = NS_STATIC_CAST(nsITextContent*, frame->GetContent());
        NS_ASSERTION(textContent, "No text content for text accessible!");
        PRInt32 textContentLength = textContent->TextLength();
        if (startOffset < textContentLength) {
          // XXX Can we somehow optimize further by getting the nsTextFragment and use
          // CopyTo to a PRUnichar buffer to copy it directly to the string?
          nsAutoString newText;
          if (aText) {
            textContent->AppendTextTo(newText);
          }
          if (startOffset > 0 || endOffset < textContentLength) {
            // XXX the Substring operation is efficient, but does the reassignment
            // to the original nsAutoString cause a copy?
            if (aText) {
              newText = Substring(newText, startOffset,
                                  PR_MIN(textContentLength, endOffset - startOffset));
            }
            if (aEndFrame) {
              *aEndFrame = frame;
            }
            aEndOffset = endOffset;
          }
          if (aText) {
            if (frame && !frame->GetStyleText()->WhiteSpaceIsSignificant()) {
              // Replace \r\n\t in markup with space unless in this is preformatted text
              // where those characters are significant
              newText.ReplaceChar("\r\n\t", ' ');
            }
            *aText += newText;
          }
          if (!startFrame) {
            startFrame = frame;
            aStartOffset = startOffset;
          }
          endOffset -= startOffset;
          startOffset = 0;
        }
        else {
          startOffset -= textContentLength;
          endOffset -= textContentLength;
        }
      }
    }
    else {
      // Embedded object, append marker
      // XXX Append \n for <br>'s
      if (startOffset >= 1) {
        -- startOffset;
      }
      else {
        if (aText && endOffset > 0) {
          *aText += (frame->GetType() == nsAccessibilityAtoms::brFrame) ?
                    kForcedNewLineChar : kEmbeddedObjectChar;
        }
        if (!startFrame) {
          startFrame = frame;
          aStartOffset = 0;
        }
      }
      -- endOffset;
    }
    if (!endOffset) {
      break;
    }
  }

  if (aEndFrame && !*aEndFrame) {
    *aEndFrame = startFrame;
  }

  return startFrame;
}

NS_IMETHODIMP nsHyperTextAccessible::GetText(PRInt32 aStartOffset, PRInt32 aEndOffset, nsAString &aText)
{
  if (!mDOMNode) {
    return NS_ERROR_FAILURE;
  }
  return GetPosAndText(aStartOffset, aEndOffset, &aText) ? NS_OK : NS_ERROR_FAILURE;
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
  if (!mDOMNode) {
    return NS_ERROR_FAILURE;
  }
  nsAutoString text;
  nsresult rv = GetText(aOffset, aOffset + 1, text);
  NS_ENSURE_SUCCESS(rv, rv);
  *aCharacter = text.First();
  return NS_OK;
}

nsresult nsHyperTextAccessible::DOMPointToOffset(nsIDOMNode* aNode, PRInt32 aNodeOffset, PRInt32* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = 0;
  NS_ENSURE_ARG_POINTER(aNode);
  NS_ENSURE_TRUE(aNodeOffset >= 0, NS_ERROR_INVALID_ARG);

  unsigned short nodeType;
  aNode->GetNodeType(&nodeType);
  if (nodeType != nsIDOMNode::TEXT_NODE) {
    // If not text, aNodeOffset is a child
    if (aNode != mDOMNode) {
#ifdef DEBUG
      // This should only happen in the empty plaintext case, when there is no text child yet
      nsCOMPtr<nsIEditor> editor = GetEditor();
      nsCOMPtr<nsIPlaintextEditor> plaintextEditor = do_QueryInterface(editor);
      NS_ASSERTION(plaintextEditor, "DOM Point is not in this nsHyperTextAccessible");
#endif
      return NS_OK;  // No text content yet, because textfield is blank, offset must be 0
    }
    nsCOMPtr<nsIAccessible> accessible;
    while (NextChild(accessible)) {
      if (aNodeOffset -- == 0) {
        nsCOMPtr<nsIAccessibleHyperLink> hyperLink = do_QueryInterface(accessible);
        NS_ENSURE_TRUE(hyperLink, NS_ERROR_FAILURE);
        return hyperLink->GetStartIndex(aResult);
      }
    }
    return NS_ERROR_FAILURE; // Child not found
  }

  // This is text, and aNodeOffset is the nth character in the text
  *aResult = aNodeOffset;
  nsCOMPtr<nsIAccessible> accessible;

  while (NextChild(accessible)) {
    nsCOMPtr<nsPIAccessNode> accessNode(do_QueryInterface(accessible));
    nsIFrame *frame = accessNode->GetFrame();
    if (!frame) {
      return NS_ERROR_FAILURE;
    }

    nsIContent *content = frame->GetContent();

    if (frame && SameCOMIdentity(content, aNode)) {
      return NS_OK;
    }

    if (Role(accessible) == ROLE_TEXT_LEAF) {
      if (frame) {
        nsITextContent *textContent = NS_STATIC_CAST(nsITextContent*, content);
        NS_ASSERTION(textContent, "No text content for a ROLE_TEXT?");
        *aResult += textContent->TextLength();
      }
    }
    else {
      ++ *aResult; // Increment by 1 embedded object char
    }
  }

  return NS_ERROR_FAILURE;
}

PRInt32 nsHyperTextAccessible::GetRelativeOffset(nsIPresShell *aPresShell, nsIFrame *aFromFrame, PRInt32 aFromOffset,
                                                 nsSelectionAmount aAmount, nsDirection aDirection, PRBool aNeedsStart)
{
  const PRBool kIsEatingWS = PR_FALSE;            // XXX This is more of a status than something to init, and
                                                  // all callers init it to PR_FALSE. I think it should be removed
                                                  // from SetData() and always init'd to PR_FALSE.
  const PRBool kIsLeftPreferred = PR_FALSE;       // Another out parameter
  const PRBool kIsJumpLinesOk = PR_TRUE;          // okay to jump lines
  const PRBool kIsScrollViewAStop = PR_FALSE;     // do not stop at scroll views
  const PRBool kIsKeyboardSelect = PR_TRUE;       // is keyboard selection
  const PRBool kIsVisualBidi = PR_TRUE;           // use visual order for bidi text

  nsPeekOffsetStruct pos;

  EWordMovementType wordMovementType = aNeedsStart ? eStartWord : eEndWord;
  if (aAmount == eSelectLine) {
    aAmount = (aDirection == eDirNext) ? eSelectEndLine : eSelectBeginLine;
  }

  pos.SetData(aPresShell, 0, aAmount, aDirection, aFromOffset,
              kIsEatingWS, kIsLeftPreferred, kIsJumpLinesOk,
              kIsScrollViewAStop, kIsKeyboardSelect, kIsVisualBidi,
              wordMovementType);
  nsresult rv = aFromFrame->PeekOffset(aPresShell->GetPresContext(), &pos);

  PRInt32 resultOffset = pos.mContentOffset;
  nsIContent *resultContent = nsnull;
  if (NS_SUCCEEDED(rv)) {
    resultContent = pos.mResultContent;
  }
  nsCOMPtr<nsIDOMNode> resultNode = do_QueryInterface(resultContent);
  PRInt32 hyperTextOffset;
  if (resultContent && NS_SUCCEEDED(DOMPointToOffset(resultNode, resultOffset, &hyperTextOffset))) {
    if (aAmount == eSelectLine && !aNeedsStart && hyperTextOffset > 0) {
      -- hyperTextOffset; // We want the end of the previous line in this case
    }
  }
  else if (aDirection == eDirNext) {
    GetCharacterCount(&hyperTextOffset);
  }
  else {
    hyperTextOffset = 0;
  }

  return hyperTextOffset;
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

nsresult nsHyperTextAccessible::GetTextHelper(EGetTextType aType, nsAccessibleTextBoundary aBoundaryType,
                                              PRInt32 aOffset, PRInt32 *aStartOffset, PRInt32 *aEndOffset,
                                              nsAString &aText)
{
  aText.Truncate();
  *aStartOffset = *aEndOffset = 0;

  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }

  PRInt32 startOffset = aOffset;
  PRInt32 endOffset = aOffset;
  nsIFrame *startFrame = GetPosAndText(startOffset, endOffset); // Convert offsets to frame-relative
  if (!startFrame) {
    return NS_ERROR_FAILURE;
  }

  nsSelectionAmount amount;
  PRBool needsStart = PR_FALSE;
  switch (aBoundaryType)
  {
  case BOUNDARY_CHAR:
    amount = eSelectCharacter;
    if (aType == eGetAt) {
      aType = eGetAfter; // Avoid returning 2 characters
    }
    break;
  case BOUNDARY_WORD_START:
    needsStart = PR_TRUE;
    amount = eSelectWord;
    break;
  case BOUNDARY_WORD_END:
    amount = eSelectWord;
    break;
  case BOUNDARY_LINE_START:
    needsStart = PR_TRUE;
    amount = eSelectLine;
    break;
  case BOUNDARY_LINE_END:
    amount = eSelectLine;
    break;
  case BOUNDARY_SENTENCE_START:
    // XXX Need to implement
    // isStartPreferred = PR_TRUE;
    return NS_ERROR_NOT_IMPLEMENTED;
  case BOUNDARY_SENTENCE_END:
    // XXX Need to implement
    return NS_ERROR_NOT_IMPLEMENTED;
  case BOUNDARY_ATTRIBUTE_RANGE:
    {
      // XXX We should merge identically formatted frames
      nsITextContent *textContent = NS_STATIC_CAST(nsITextContent*, startFrame->GetContent());
      // If not text, then it's represented by an embedded object char (length of 1)
      PRInt32 textLength = textContent ? textContent->TextLength() : 1;
      *aStartOffset = aOffset - startOffset;
      *aEndOffset = *aStartOffset + textLength;
      startOffset = *aStartOffset;
      endOffset = *aEndOffset;
      return GetText(startOffset, endOffset, aText);
    }
  default:
    return NS_ERROR_INVALID_ARG;
  }

  // If aType == eGetAt we'll change both the start and end offset from the original offset
  startOffset = (aType == eGetAfter)  ? aOffset : GetRelativeOffset(presShell, startFrame, startOffset,
                                                                    amount, eDirPrevious, needsStart);
  if (aBoundaryType == BOUNDARY_LINE_START && startOffset > 0) {
    -- startOffset; // XXX Do we need to deal with endOffset the same way, or increase it for BOUNDARY_LINE_END?
  }
  *aStartOffset = startOffset;

  if (aType == eGetBefore) {
    endOffset = aOffset;
  }
  else {
    // Start moving forward from the start so that we don't get 2 words/lines/sentences if 
    // the offset occured on whitespace boundary
    PRInt32 tempOffset = endOffset = startOffset;  // Use temp so that startOffset is not modified (it is passed by reference)
    nsIFrame *endFrame = GetPosAndText(tempOffset, endOffset);
    endOffset = GetRelativeOffset(presShell, endFrame, endOffset, amount, eDirNext, needsStart);
  }

  *aEndOffset = endOffset;

  return GetPosAndText(startOffset, endOffset, &aText) ? NS_OK : NS_ERROR_FAILURE;
}

/**
  * nsIAccessibleText impl.
  */
NS_IMETHODIMP nsHyperTextAccessible::GetTextBeforeOffset(PRInt32 aOffset, nsAccessibleTextBoundary aBoundaryType,
                                                         PRInt32 *aStartOffset, PRInt32 *aEndOffset, nsAString & aText)
{
  return GetTextHelper(eGetBefore, aBoundaryType, aOffset, aStartOffset, aEndOffset, aText);
}

NS_IMETHODIMP nsHyperTextAccessible::GetTextAtOffset(PRInt32 aOffset, nsAccessibleTextBoundary aBoundaryType,
                                                     PRInt32 *aStartOffset, PRInt32 *aEndOffset, nsAString & aText)
{
  return GetTextHelper(eGetAt, aBoundaryType, aOffset, aStartOffset, aEndOffset, aText);
}

NS_IMETHODIMP nsHyperTextAccessible::GetTextAfterOffset(PRInt32 aOffset, nsAccessibleTextBoundary aBoundaryType,
                                                        PRInt32 *aStartOffset, PRInt32 *aEndOffset, nsAString & aText)
{
  return GetTextHelper(eGetAfter, aBoundaryType, aOffset, aStartOffset, aEndOffset, aText);
}

NS_IMETHODIMP nsHyperTextAccessible::GetAttributeRange(PRInt32 aOffset, PRInt32 *aRangeStartOffset, 
                                                       PRInt32 *aRangeEndOffset, nsIAccessible **aAccessibleWithAttrs)
{
  // Return the range of text with common attributes around aOffset
  *aRangeStartOffset = *aRangeEndOffset = 0;
  *aAccessibleWithAttrs = nsnull;

  if (!mDOMNode) {
    return NS_ERROR_FAILURE;
  }

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
  PRInt32 endOffset = aOffset + 1;
  nsIFrame *frame = GetPosAndText(aOffset, endOffset, &firstChar);
  if (!frame) {
    return NS_ERROR_FAILURE;
  }

  nsIntRect frameScreenRect = frame->GetScreenRectExternal();
  if (firstChar.First() != kEmbeddedObjectChar) {
    nsresult rv = frame->GetChildFrameContainingOffset(aOffset, PR_FALSE,
                                                       &aOffset, &frame);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIRenderingContext> rc;
    shell->CreateRenderingContext(frame, getter_AddRefs(rc));
    NS_ENSURE_TRUE(rc, NS_ERROR_FAILURE);

    const nsStyleFont *font = frame->GetStyleFont();
    const nsStyleVisibility *visibility = frame->GetStyleVisibility();

    rv = rc->SetFont(font->mFont, visibility->mLangGroup);
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

    //add the width of the string before current char
    nsPoint pt;
    rv = frame->GetPointFromOffset(context, rc, aOffset, &pt);
    NS_ENSURE_TRUE(rv, rv);
    frameScreenRect.x += NSTwipsToIntPixels(pt.x, t2p);
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
    if (!IsEmbeddedObject(accessible)) {
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
    if (!IsEmbeddedObject(accessible) && aIndex-- == 0) {
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
    PRUint32 role = Role(accessible);
    if (role == ROLE_TEXT_LEAF) {
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
      if (role != ROLE_WHITESPACE) {
        ++ *aLinkIndex;
      }
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
    if (!IsEmbeddedObject(accessible)) {
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
  PRInt32 numChars;
  GetCharacterCount(&numChars);
  if (numChars == 0 || NS_SUCCEEDED(DeleteText(0, numChars))) {
    return InsertText(aText, 0);
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHyperTextAccessible::InsertText(const nsAString &aText, PRInt32 aPosition)
{
  if (NS_SUCCEEDED(SetCaretOffset(aPosition))) {
    nsCOMPtr<nsIEditor> editor = GetEditor();
    nsCOMPtr<nsIPlaintextEditor> peditor(do_QueryInterface(editor));
    return peditor ? peditor->InsertText(aText) : NS_ERROR_FAILURE;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHyperTextAccessible::CopyText(PRInt32 aStartPos, PRInt32 aEndPos)
{
  nsCOMPtr<nsIEditor> editor = GetEditor();
  if (editor && NS_SUCCEEDED(SetSelectionRange(aStartPos, aEndPos)))
    return editor->Copy();

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHyperTextAccessible::CutText(PRInt32 aStartPos, PRInt32 aEndPos)
{
  nsCOMPtr<nsIEditor> editor = GetEditor();
  if (editor && NS_SUCCEEDED(SetSelectionRange(aStartPos, aEndPos)))
    return editor->Cut();

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHyperTextAccessible::DeleteText(PRInt32 aStartPos, PRInt32 aEndPos)
{
  nsCOMPtr<nsIEditor> editor = GetEditor();
  if (editor && NS_SUCCEEDED(SetSelectionRange(aStartPos, aEndPos)))
    return editor->DeleteSelection(nsIEditor::eNone);

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHyperTextAccessible::PasteText(PRInt32 aPosition)
{
  nsCOMPtr<nsIEditor> editor = GetEditor();
  if (editor && NS_SUCCEEDED(SetCaretOffset(aPosition)))
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
    DOMPointToOffset(aNode, 0, &textData.start);
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
    // XXX, don't fire event for the last br
    // Should we use editor->DocumentIsEmpty() instead?
    nsCOMPtr<nsIDOMHTMLBRElement> br(do_QueryInterface(aChild));
    if (br)
      textData.length = 1;
    else
      return NS_OK;
  }

  DOMPointToOffset(aChild, 0, &textData.start);
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
  DOMPointToOffset(aTextNode, aOffset, &textData.start);
  return FireTextChangeEvent(&textData);
}

NS_IMETHODIMP nsHyperTextAccessible::WillDeleteText(nsIDOMCharacterData *aTextNode,
                                                    PRInt32 aOffset, PRInt32 aLength)
{
  AtkTextChange textData;

  textData.add = PR_FALSE;
  textData.length = aLength;
  DOMPointToOffset(aTextNode, aOffset, &textData.start);
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
// XXX Deal with > 1 selections
{
  PRInt32 selectionStart, selectionEnd;
  nsresult rv = GetSelectionBounds(0, &selectionStart, &selectionEnd);
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

nsresult nsHyperTextAccessible::SetSelectionRange(PRInt32 aStartPos, PRInt32 aEndPos)
{
  // This will clear all the selections out and set the selection
  nsCOMPtr<nsISelection> domSel;
  GetSelections(nsnull, getter_AddRefs(domSel));
  if (!domSel) {
    return NS_ERROR_FAILURE;
  }
  if (domSel) {
    domSel->RemoveAllRanges();
    AddSelection(aStartPos, aEndPos);
  }
  return NS_OK;
}

NS_IMETHODIMP nsHyperTextAccessible::SetCaretOffset(PRInt32 aCaretOffset)
{
  return SetSelectionRange(aCaretOffset, aCaretOffset);
}

/*
 * Gets the offset position of the caret (cursor).
 */
NS_IMETHODIMP nsHyperTextAccessible::GetCaretOffset(PRInt32 *aCaretOffset)
{
  *aCaretOffset = 0;

  nsCOMPtr<nsISelection> domSel;
  nsresult rv = GetSelections(nsnull, getter_AddRefs(domSel));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> caretNode;
  rv = domSel->GetFocusNode(getter_AddRefs(caretNode));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 caretOffset;
  domSel->GetFocusOffset(&caretOffset);

  return DOMPointToOffset(caretNode, caretOffset, aCaretOffset);
}

nsresult nsHyperTextAccessible::GetSelections(nsISelectionController **aSelCon, nsISelection **aDomSel)
{
  if (aSelCon) {
    *aSelCon = nsnull;
  }
  if (aDomSel) {
    *aDomSel = nsnull;
  }
  nsIFrame *frame = GetFrame();
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  // Get the selection and selection controller
  nsCOMPtr<nsISelectionController> selCon;
  frame->GetSelectionController(GetPresContext(),
                                getter_AddRefs(selCon));
  NS_ENSURE_TRUE(selCon, NS_ERROR_FAILURE);
  if (aSelCon) {
    NS_ADDREF(*aSelCon = selCon);
  }

  if (aDomSel) {
    nsCOMPtr<nsISelection> domSel;
    selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSel));
    NS_ENSURE_TRUE(domSel, NS_ERROR_FAILURE);
    NS_ADDREF(*aDomSel = domSel);
  }

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
}

/*
 * Gets the start and end offset of the specified selection.
 */
NS_IMETHODIMP nsHyperTextAccessible::GetSelectionBounds(PRInt32 aSelectionNum, PRInt32 *aStartOffset, PRInt32 *aEndOffset)
{
  *aStartOffset = *aEndOffset = 0;

  nsCOMPtr<nsISelection> domSel;
  nsresult rv = GetSelections(nsnull, getter_AddRefs(domSel));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 rangeCount;
  domSel->GetRangeCount(&rangeCount);
  if (aSelectionNum < 0 || aSelectionNum >= rangeCount)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIDOMRange> range;
  rv = domSel->GetRangeAt(aSelectionNum, getter_AddRefs(range));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> startNode;
  range->GetStartContainer(getter_AddRefs(startNode));
  PRInt32 startOffset;
  range->GetStartOffset(&startOffset);
  rv = DOMPointToOffset(startNode, startOffset, aStartOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> endNode;
  range->GetEndContainer(getter_AddRefs(endNode));
  PRInt32 endOffset;
  range->GetEndOffset(&endOffset);
  if (startNode == endNode && startOffset == endOffset) {
    // Shortcut for collapsed selection case (caret)
    *aEndOffset = *aStartOffset;
    return NS_OK;
  }
  return DOMPointToOffset(endNode, endOffset, aEndOffset);
}

/*
 * Changes the start and end offset of the specified selection.
 */
NS_IMETHODIMP nsHyperTextAccessible::SetSelectionBounds(PRInt32 aSelectionNum, PRInt32 aStartOffset, PRInt32 aEndOffset)
{
  nsCOMPtr<nsISelection> domSel;
  nsresult rv = GetSelections(nsnull, getter_AddRefs(domSel));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 isOnlyCaret = (aStartOffset == aEndOffset); // Caret is a collapsed selection

  PRInt32 rangeCount;
  domSel->GetRangeCount(&rangeCount);
  if (aSelectionNum < 0 || aSelectionNum >= rangeCount)
    return NS_ERROR_INVALID_ARG;

  nsIFrame *endFrame;
  nsIFrame *startFrame = GetPosAndText(aStartOffset, aEndOffset, nsnull, &endFrame);
  nsCOMPtr<nsIDOMRange> range;
  domSel->GetRangeAt(aSelectionNum, getter_AddRefs(range));

  if (!startFrame) {
    // If blank editor, make sure to position selection anyway
    nsCOMPtr<nsIEditor> editor = GetEditor();
    if (editor) {
      nsCOMPtr<nsIDOMElement> rootElement;
      editor->GetRootElement(getter_AddRefs(rootElement));
      nsCOMPtr<nsIDOMNode> rootEditorNode(do_QueryInterface(rootElement));
      return range->SelectNodeContents(rootEditorNode);
    }
    return NS_ERROR_FAILURE;
  }


  nsIContent *startParentContent = startFrame->GetContent();
  if (startFrame->GetType() != nsAccessibilityAtoms::textFrame) {
    nsIContent *newParent = startParentContent->GetParent();
    aStartOffset = newParent->IndexOf(startParentContent);
    startParentContent = newParent;
  }
  nsCOMPtr<nsIDOMNode> startParentNode(do_QueryInterface(startParentContent));
  NS_ENSURE_TRUE(startParentNode, NS_ERROR_FAILURE);
  range->SelectNodeContents(mDOMNode);
  range->SetStart(startParentNode, aStartOffset);
  if (isOnlyCaret) { 
    range->Collapse(PR_TRUE);
  }
  else {
    nsIContent *endParentContent = endFrame->GetContent();
    if (endFrame->GetType() != nsAccessibilityAtoms::textFrame) {
      nsIContent *newParent = endParentContent->GetParent();
      aEndOffset = newParent->IndexOf(endParentContent);
      endParentContent = newParent;
    }
    nsCOMPtr<nsIDOMNode> endParentNode(do_QueryInterface(endParentContent));
    NS_ENSURE_TRUE(endParentNode, NS_ERROR_FAILURE);
    range->SetEnd(endParentNode, aEndOffset);
  }

  return NS_OK;
}

/*
 * Adds a selection bounded by the specified offsets.
 */
NS_IMETHODIMP nsHyperTextAccessible::AddSelection(PRInt32 aStartOffset, PRInt32 aEndOffset)
{
  nsCOMPtr<nsISelection> domSel;
  nsresult rv = GetSelections(nsnull, getter_AddRefs(domSel));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMRange> range(do_CreateInstance(kRangeCID));
  if (! range)
    return NS_ERROR_OUT_OF_MEMORY;

  range->SelectNodeContents(mDOMNode);  // Must be a valid range or we can't add it
  rv = domSel->AddRange(range);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 rangeCount;
  domSel->GetRangeCount(&rangeCount);
  return SetSelectionBounds(rangeCount - 1, aStartOffset, aEndOffset);
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

