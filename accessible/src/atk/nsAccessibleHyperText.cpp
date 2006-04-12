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
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kyle Yuan (kyle.yuan@sun.com)
 *   Ginn Chen (ginn.chen@sun.com)
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

#include "nsAccessibilityAtoms.h"
#include "nsAccessibilityService.h"
#include "nsAccessibleHyperText.h"
#include "nsHTMLLinkAccessibleWrap.h"
#include "nsHTMLTextAccessible.h"
#include "nsPIAccessNode.h"
#include "nsIFrame.h"
#include "nsILink.h"
#include "nsIServiceManager.h"
#include "nsArrayUtils.h"

/*
 * nsAccessibleHyperText supports both nsIAccessibleHyperText and nsIAccessibleText.
 *   It's mainly aimed at the compound content that consists of many text nodes and links.
 *   Typically, it's a paragraph of text, a cell of table, etc.
*/

NS_IMPL_ISUPPORTS2(nsAccessibleHyperText, nsIAccessibleHyperText, nsIAccessibleText)

nsAccessibleHyperText::nsAccessibleHyperText(nsIDOMNode* aDomNode, nsIWeakReference* aShell)
{
  mIndex = -1;
  nsCOMPtr<nsIContent> content(do_QueryInterface(aDomNode));
  if (content) {
    nsCOMPtr<nsIContent> parentContent = content->GetParent();
    if (parentContent)
      mIndex = parentContent->IndexOf(content);
  }

  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(aShell));
  if (shell) {
    mTextChildren = do_CreateInstance(NS_ARRAY_CONTRACTID);
    if (mTextChildren) {
      nsCOMPtr<nsIContent> content(do_QueryInterface(aDomNode));
      nsIFrame *frame = shell->GetPrimaryFrameFor(content);
      nsIFrame *parentFrame = nsAccessible::GetParentBlockFrame(frame);
      NS_ASSERTION(parentFrame, "Error: HyperText can't get parent block frame");
      if (parentFrame) {
        nsIFrame* childFrame = parentFrame->GetFirstChild(nsnull);
        PRBool bSave = PR_FALSE;
        GetAllTextChildren(shell->GetPresContext(), childFrame,
                           aDomNode, bSave);
      }
    }
  }
}

void nsAccessibleHyperText::Shutdown()
{
  mTextChildren = nsnull;
}

PRBool nsAccessibleHyperText::GetAllTextChildren(nsPresContext *aPresContext, nsIFrame *aCurFrame, nsIDOMNode* aNode, PRBool &bSave)
{
  while (aCurFrame) {

    nsIAtom* frameType = aCurFrame->GetType();
    if (frameType == nsAccessibilityAtoms::blockFrame) {
      if (bSave)
        return PR_TRUE;
    }
    else {
      if (frameType == nsAccessibilityAtoms::textFrame) {
        // Skip the empty text frames that usually only consist of "\n"
        if (! aCurFrame->GetRect().IsEmpty()) {
          nsCOMPtr<nsIDOMNode> node(do_QueryInterface(aCurFrame->GetContent()));
          if (bSave || node == aNode) {
            // some long text node may be divided into several frames, 
            // so we must check whether this node is already in the array
            PRUint32 index;
            nsresult rv = mTextChildren->IndexOf(0, node, &index);
            if (NS_FAILED(rv)) {
              mTextChildren->AppendElement(node, PR_FALSE);
            }
            bSave = PR_TRUE;
          }
        }
      }

      nsIFrame* childFrame = aCurFrame->GetFirstChild(nsnull);
      if (GetAllTextChildren(aPresContext, childFrame, aNode, bSave))
        return PR_TRUE;
    }

    nsIFrame* siblingFrame = aCurFrame->GetNextSibling();
    aCurFrame = siblingFrame;
  }
  return PR_FALSE;
}

PRInt32 nsAccessibleHyperText::GetIndex()
// XXX, this index is used for giving a hypertext a meaningful name, such as "Paragraph n",
// but by now, we haven't found a better way to do that, just use the index of our parent's
// children list as the number.
{
  return mIndex;
}

nsIDOMNode* nsAccessibleHyperText::FindTextNodeByOffset(PRInt32 aOffset, PRInt32& aBeforeLength)
{
  aBeforeLength = 0;

  PRUint32 index, count;
  mTextChildren->GetLength(&count);
  for (index = 0; index < count; index++) {
    nsCOMPtr<nsIDOMNode> domNode(do_QueryElementAt(mTextChildren, index));
    nsAccessibleText accText(domNode);
    PRInt32 charCount;
    if (NS_SUCCEEDED(accText.GetCharacterCount(&charCount))) {
      if (aOffset >= 0 && aOffset <= charCount) {
        return domNode;
      }
      aOffset -= charCount;
      aBeforeLength += charCount;
    }
  }

  return nsnull;
}

nsresult nsAccessibleHyperText::GetTextHelper(EGetTextType aType, nsAccessibleTextBoundary aBoundaryType,
                                              PRInt32 aOffset, PRInt32 *aStartOffset, PRInt32 *aEndOffset, nsAString & aText)
{
  PRInt32 beforeLength;
  nsIDOMNode* domNode = FindTextNodeByOffset(aOffset, beforeLength);
  if (domNode) {
    nsAccessibleText accText(domNode);
    // call nsAccessibleText::GetTextHelper directly so that it can adjust the aStartOffset/aEndOffset
    // according to the mTextChildren
    nsresult rv = accText.GetTextHelper(aType, aBoundaryType, aOffset - beforeLength, aStartOffset, aEndOffset, mTextChildren, aText);
    return rv;
  }

  return NS_ERROR_INVALID_ARG;
}

// ------- nsIAccessibleText ---------------
/* attribute long caretOffset; */
NS_IMETHODIMP nsAccessibleHyperText::GetCaretOffset(PRInt32 *aCaretOffset)
{
  *aCaretOffset = 0;

  PRInt32 charCount, caretOffset;
  PRUint32 index, count;
  mTextChildren->GetLength(&count);
  for (index = 0; index < count; index++) {
    nsCOMPtr<nsIDOMNode> domNode(do_QueryElementAt(mTextChildren, index));
    nsAccessibleText accText(domNode);
    if (NS_SUCCEEDED(accText.GetCaretOffset(&caretOffset))) {
      *aCaretOffset += caretOffset;
      return NS_OK;
    } else if (GetLinkNode(domNode) == nsAccessNode::gLastFocusedNode) {
      //Focus is here
      return NS_OK;
    }
    if (NS_SUCCEEDED(accText.GetCharacterCount(&charCount))) {
      *aCaretOffset += charCount;
    }
  }

  // The current focus node is not inside us
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAccessibleHyperText::SetCaretOffset(PRInt32 aCaretOffset)
{
  PRInt32 beforeLength;
  nsIDOMNode* domNode = FindTextNodeByOffset(aCaretOffset, beforeLength);
  if (domNode) {
    nsAccessibleText accText(domNode);
    return accText.SetCaretOffset(aCaretOffset - beforeLength);
  }

  return NS_ERROR_INVALID_ARG;
}

/* readonly attribute long characterCount; */
NS_IMETHODIMP nsAccessibleHyperText::GetCharacterCount(PRInt32 *aCharacterCount)
{
  *aCharacterCount = 0;

  PRInt32 charCount;
  PRUint32 index, count;
  mTextChildren->GetLength(&count);
  for (index = 0; index < count; index++) {
    nsCOMPtr<nsIDOMNode> domNode(do_QueryElementAt(mTextChildren, index));
    nsAccessibleText accText(domNode);
    if (NS_SUCCEEDED(accText.GetCharacterCount(&charCount)))
      *aCharacterCount += charCount;
  }

  return NS_OK;
}

/* readonly attribute long selectionCount; */
NS_IMETHODIMP nsAccessibleHyperText::GetSelectionCount(PRInt32 *aSelectionCount)
{
  *aSelectionCount = 0;

  PRInt32 selCount;
  PRUint32 index, count;
  mTextChildren->GetLength(&count);
  for (index = 0; index < count; index++) {
    nsCOMPtr<nsIDOMNode> domNode(do_QueryElementAt(mTextChildren, index));
    nsAccessibleText accText(domNode);
    if (NS_SUCCEEDED(accText.GetSelectionCount(&selCount)))
      *aSelectionCount += selCount;
  }

  return NS_OK;
}

/* AString getText (in long startOffset, in long endOffset); */
NS_IMETHODIMP nsAccessibleHyperText::GetText(PRInt32 aStartOffset, PRInt32 aEndOffset, nsAString & aText)
{
  if (aEndOffset == -1)
    GetCharacterCount(&aEndOffset);

  PRInt32 charCount, totalCount = 0, currentStart, currentEnd;
  PRUint32 index, count;
  nsAutoString text, nodeText;
  mTextChildren->GetLength(&count);
  for (index = 0; index < count; index++) {
    nsCOMPtr<nsIDOMNode> domNode(do_QueryElementAt(mTextChildren, index));
    nsAccessibleText accText(domNode);
    if (NS_SUCCEEDED(accText.GetCharacterCount(&charCount))) {
      currentStart = aStartOffset - totalCount;
      currentEnd = aEndOffset - totalCount;
      if (currentStart >= 0 && currentStart < charCount) {
        accText.GetText(currentStart, NS_MIN(charCount, currentEnd), nodeText);
        text += nodeText;
        aStartOffset += charCount - currentStart;
        if (aStartOffset >= aEndOffset)
          break;
      }
      totalCount += charCount;
    }
  }

  // Eliminate the new line character
  PRInt32 start = 0, length = text.Length();
  PRInt32 offset = text.FindCharInSet("\n\r");
  while (offset != kNotFound) {
    if (offset > start)
      aText += Substring(text, start, offset - start);

    start = offset + 1;
    offset = text.FindCharInSet("\n\r", start);
  }
  // Consume the last bit of the string if there's any left
  if (start < length) {
    if (start)
      aText += Substring(text, start, length - start);
    else
      aText = text;
  }

  return NS_OK;
}

/* AString getTextBeforeOffset (in long offset, in nsAccessibleTextBoundary boundaryType, out long startOffset, out long endOffset); */
NS_IMETHODIMP nsAccessibleHyperText::GetTextBeforeOffset(PRInt32 aOffset, nsAccessibleTextBoundary aBoundaryType,
                                                         PRInt32 *aStartOffset, PRInt32 *aEndOffset, nsAString & aText)
{
  return GetTextHelper(eGetBefore, aBoundaryType, aOffset, aStartOffset, aEndOffset, aText);
}

/* AString getTextAfterOffset (in long offset, in nsAccessibleTextBoundary boundaryType, out long startOffset, out long endOffset); */
NS_IMETHODIMP nsAccessibleHyperText::GetTextAfterOffset(PRInt32 aOffset, nsAccessibleTextBoundary aBoundaryType,
                                                        PRInt32 *aStartOffset, PRInt32 *aEndOffset, nsAString & aText)
{
  return GetTextHelper(eGetAfter, aBoundaryType, aOffset, aStartOffset, aEndOffset, aText);
}

/* AString getTextAtOffset (in long offset, in nsAccessibleTextBoundary boundaryType, out long startOffset, out long endOffset); */
NS_IMETHODIMP nsAccessibleHyperText::GetTextAtOffset(PRInt32 aOffset, nsAccessibleTextBoundary aBoundaryType,
                                                     PRInt32 *aStartOffset, PRInt32 *aEndOffset, nsAString & aText)
{
  return GetTextHelper(eGetAt, aBoundaryType, aOffset, aStartOffset, aEndOffset, aText);
}

/* wchar getCharacterAtOffset (in long offset); */
NS_IMETHODIMP nsAccessibleHyperText::GetCharacterAtOffset(PRInt32 aOffset, PRUnichar *aCharacter)
{
  PRInt32 beforeLength;
  nsIDOMNode* domNode = FindTextNodeByOffset(aOffset, beforeLength);
  if (domNode) {
    nsAccessibleText accText(domNode);
    return accText.GetCharacterAtOffset(aOffset - beforeLength, aCharacter);
  }

  return NS_ERROR_INVALID_ARG;
}

/* nsISupports getAttributeRange (in long offset, out long rangeStartOffset, out long rangeEndOffset); */
NS_IMETHODIMP nsAccessibleHyperText::GetAttributeRange(PRInt32 aOffset, PRInt32 *aRangeStartOffset, PRInt32 *aRangeEndOffset, nsISupports **aAttributes)
{
  *aRangeStartOffset = aOffset;
  GetCharacterCount(aRangeEndOffset);
  *aAttributes = 0;
  return NS_OK;
}

/* void getCharacterExtents (in long offset, out long x, out long y, out long width, out long height, in nsAccessibleCoordType coordType); */
NS_IMETHODIMP nsAccessibleHyperText::GetCharacterExtents(PRInt32 aOffset, PRInt32 *aX, PRInt32 *aY, PRInt32 *aWidth, PRInt32 *aHeight, nsAccessibleCoordType aCoordType)
{
  PRInt32 beforeLength;
  nsIDOMNode* domNode = FindTextNodeByOffset(aOffset, beforeLength);
  if (domNode) {
    nsAccessibleText accText(domNode);
    return accText.GetCharacterExtents(aOffset - beforeLength, aX, aY, aWidth, aHeight, aCoordType);
  }

  return NS_ERROR_INVALID_ARG;
}

/* long getOffsetAtPoint (in long x, in long y, in nsAccessibleCoordType coordType); */
NS_IMETHODIMP nsAccessibleHyperText::GetOffsetAtPoint(PRInt32 aX, PRInt32 aY, nsAccessibleCoordType aCoordType, PRInt32 *aOffset)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void getSelectionBounds (in long selectionNum, out long startOffset, out long endOffset); */
NS_IMETHODIMP nsAccessibleHyperText::GetSelectionBounds(PRInt32 aSelectionNum, PRInt32 *aStartOffset, PRInt32 *aEndOffset)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void setSelectionBounds (in long selectionNum, in long startOffset, in long endOffset); */
NS_IMETHODIMP nsAccessibleHyperText::SetSelectionBounds(PRInt32 aSelectionNum, PRInt32 aStartOffset, PRInt32 aEndOffset)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void addSelection (in long startOffset, in long endOffset); */
NS_IMETHODIMP nsAccessibleHyperText::AddSelection(PRInt32 aStartOffset, PRInt32 aEndOffset)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void removeSelection (in long selectionNum); */
NS_IMETHODIMP nsAccessibleHyperText::RemoveSelection(PRInt32 aSelectionNum)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// ------- nsIAccessibleHyperText ---------------
/* readonly attribute long links; */NS_IMETHODIMP nsAccessibleHyperText::GetLinks(PRInt32 *aLinks)
{
  *aLinks = 0;

  PRUint32 index, count;
  mTextChildren->GetLength(&count);
  for (index = 0; index < count; index++) {
    nsCOMPtr<nsIDOMNode> domNode(do_QueryElementAt(mTextChildren, index));
    if (GetLinkNode(domNode)) {
      (*aLinks)++;
    }
  }

  return NS_OK;
}

/* nsIAccessibleHyperLink getLink (in long index); */
NS_IMETHODIMP nsAccessibleHyperText::GetLink(PRInt32 aIndex, nsIAccessibleHyperLink **aLink)
{
  PRUint32 index, count, linkCount = 0;
  mTextChildren->GetLength(&count);
  for (index = 0; index < count; index++) {
    nsCOMPtr<nsIDOMNode> domNode(do_QueryElementAt(mTextChildren, index));
    nsIDOMNode* parentNode = GetLinkNode(domNode);
    if (parentNode) {
      if (linkCount++ == NS_STATIC_CAST(PRUint32, aIndex)) {
        nsCOMPtr<nsIWeakReference> weakShell;
        nsAccessibilityService::GetShellFromNode(parentNode, getter_AddRefs(weakShell));
        NS_ENSURE_TRUE(weakShell, NS_ERROR_FAILURE);

        // Check to see if we already have it in the cache.
        nsCOMPtr<nsIAccessibilityService>
          accService(do_GetService("@mozilla.org/accessibilityService;1"));
        NS_ENSURE_TRUE(accService, NS_ERROR_FAILURE);

        nsCOMPtr<nsIAccessible> cachedAcc;
        nsresult rv = accService->GetCachedAccessible(parentNode, weakShell,
                                             getter_AddRefs(cachedAcc));
        NS_ENSURE_SUCCESS(rv, rv);
        *aLink = nsnull;
        if (cachedAcc) {
          // Retrieved from cache
          nsCOMPtr<nsIAccessibleHyperLink> cachedLink(do_QueryInterface(cachedAcc));
          if (cachedLink) {
            *aLink = cachedLink;
            NS_IF_ADDREF(*aLink);
          }
        }
        if (!(*aLink)) {
          *aLink = new nsHTMLLinkAccessibleWrap(parentNode, mTextChildren, weakShell, nsnull);
          NS_ENSURE_TRUE(*aLink, NS_ERROR_OUT_OF_MEMORY);
          NS_ADDREF(*aLink);
          nsCOMPtr<nsPIAccessNode> accessNode(do_QueryInterface(*aLink));
          accessNode->Init();
        }
        break;
      }
    }
  }

  return NS_OK;
}

/* long getLinkIndex (in long charIndex); */
NS_IMETHODIMP nsAccessibleHyperText::GetLinkIndex(PRInt32 aCharIndex, PRInt32 *aLinkIndex)
{
  *aLinkIndex = -1;
  PRInt32 beforeLength_unused;
  PRUint32 nodeIndex;
  nsIDOMNode* domNode = FindTextNodeByOffset(aCharIndex, beforeLength_unused);
  if (GetLinkNode(domNode)
      && NS_SUCCEEDED(mTextChildren->IndexOf(0, domNode, &nodeIndex))) {
    (*aLinkIndex)++;
    for (PRUint32 index = 0; index < nodeIndex; index++) {
      nsCOMPtr<nsIDOMNode> childNode(do_QueryElementAt(mTextChildren, index));
      if (GetLinkNode(childNode)) {
        (*aLinkIndex)++;
      }
    }
  }
  return NS_OK;
}

/* long getSelectedLinkIndex (); */
NS_IMETHODIMP nsAccessibleHyperText::GetSelectedLinkIndex(PRInt32 *aSelectedLinkIndex)
{
  *aSelectedLinkIndex = -1;

  PRUint32 count;
  mTextChildren->GetLength(&count);
  if (count <= 0)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNode> curNode(do_QueryElementAt(mTextChildren, 0));

  PRUint32 index, linkCount = 0;
  for (index = 0; index < count; index++) {
    nsCOMPtr<nsIDOMNode> domNode(do_QueryElementAt(mTextChildren, index));
    nsIDOMNode* linkNode = GetLinkNode(domNode);
    if (linkNode) {
      if (linkNode == nsAccessNode::gLastFocusedNode) {
        *aSelectedLinkIndex = linkCount;
        return NS_OK;
      }
      linkCount++;
    }
  }

  return NS_ERROR_FAILURE;
}

nsresult nsAccessibleHyperText::GetBounds(nsIWeakReference *aWeakShell, PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height)
{
  *x = *y = *width = *height = 0;

  nsRect unionRectTwips;
  PRUint32 index, count;
  mTextChildren->GetLength(&count);
  for (index = 0; index < count; index++) {
    nsCOMPtr<nsIDOMNode> domNode(do_QueryElementAt(mTextChildren, index));
    nsHTMLTextAccessible *accText = new nsHTMLTextAccessible(domNode, aWeakShell, nsnull);
    if (!accText)
      return NS_ERROR_OUT_OF_MEMORY;

    nsRect frameRect;
    accText->GetBounds(&frameRect.x, &frameRect.y, &frameRect.width, &frameRect.height);
    unionRectTwips.UnionRect(unionRectTwips, frameRect);
    delete accText;
  }

  *x      = unionRectTwips.x; 
  *y      = unionRectTwips.y;
  *width  = unionRectTwips.width;
  *height = unionRectTwips.height;

  return NS_OK;
}

nsIDOMNode* nsAccessibleHyperText::GetLinkNode(nsIDOMNode* aNode)
{
  
  nsCOMPtr<nsIDOMNode> parentNode;
  nsCOMPtr<nsILink> link;
  while (aNode && link == nsnull) {
    // text node maybe a child (or grandchild, ...) of a link node
    aNode->GetParentNode(getter_AddRefs(parentNode));
    aNode = parentNode;
    link = do_QueryInterface(parentNode);
  }
  
  return parentNode;
}
