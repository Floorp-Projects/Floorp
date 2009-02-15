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
#include "nsAccessibleTreeWalker.h"
#include "nsTextAttrs.h"

#include "nsIClipboard.h"
#include "nsContentCID.h"
#include "nsIDOMAbstractView.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMDocument.h"
#include "nsPIDOMWindow.h"        
#include "nsIDOMDocumentView.h"
#include "nsIDOMRange.h"
#include "nsIDOMNSRange.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMXULDocument.h"
#include "nsIEditingSession.h"
#include "nsIEditor.h"
#include "nsIFontMetrics.h"
#include "nsIFrame.h"
#include "nsFrameSelection.h"
#include "nsILineIterator.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPlaintextEditor.h"
#include "nsIScrollableFrame.h"
#include "nsISelection2.h"
#include "nsISelectionPrivate.h"
#include "nsIServiceManager.h"
#include "nsTextFragment.h"
#include "gfxSkipChars.h"

static NS_DEFINE_IID(kRangeCID, NS_RANGE_CID);

// ------------
// nsHyperTextAccessible
// ------------

NS_IMPL_ADDREF_INHERITED(nsHyperTextAccessible, nsAccessibleWrap)
NS_IMPL_RELEASE_INHERITED(nsHyperTextAccessible, nsAccessibleWrap)

nsresult nsHyperTextAccessible::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  *aInstancePtr = nsnull;

  nsCOMPtr<nsIDOMXULDocument> xulDoc(do_QueryInterface(mDOMNode));
  if (mDOMNode && !xulDoc) {
    // We need XUL doc check for now because for now nsDocAccessible must
    // inherit from nsHyperTextAccessible in order for HTML document accessibles
    // to get support for these interfaces.
    // However at some point we may push <body> to implement the interfaces and
    // return nsDocAccessible to inherit from nsAccessibleWrap.

    if (aIID.Equals(NS_GET_IID(nsHyperTextAccessible))) {
      *aInstancePtr = static_cast<nsHyperTextAccessible*>(this);
      NS_ADDREF_THIS();
      return NS_OK;
    }

    if (mRoleMapEntry &&
        (mRoleMapEntry->role == nsIAccessibleRole::ROLE_GRAPHIC ||
         mRoleMapEntry->role == nsIAccessibleRole::ROLE_IMAGE_MAP ||
         mRoleMapEntry->role == nsIAccessibleRole::ROLE_SLIDER ||
         mRoleMapEntry->role == nsIAccessibleRole::ROLE_PROGRESSBAR ||
         mRoleMapEntry->role == nsIAccessibleRole::ROLE_SEPARATOR)) {
      // ARIA roles that these interfaces are not appropriate for
      return nsAccessible::QueryInterface(aIID, aInstancePtr);
    }

    if (aIID.Equals(NS_GET_IID(nsIAccessibleText))) {
      *aInstancePtr = static_cast<nsIAccessibleText*>(this);
      NS_ADDREF_THIS();
      return NS_OK;
    }

    if (aIID.Equals(NS_GET_IID(nsIAccessibleHyperText))) {
      *aInstancePtr = static_cast<nsIAccessibleHyperText*>(this);
      NS_ADDREF_THIS();
      return NS_OK;
    }

    if (aIID.Equals(NS_GET_IID(nsIAccessibleEditableText))) {
      *aInstancePtr = static_cast<nsIAccessibleEditableText*>(this);
      NS_ADDREF_THIS();
      return NS_OK;
    }
  }

  return nsAccessible::QueryInterface(aIID, aInstancePtr);
}

nsHyperTextAccessible::nsHyperTextAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsAccessibleWrap(aNode, aShell)
{
}

NS_IMETHODIMP nsHyperTextAccessible::GetRole(PRUint32 *aRole)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(mDOMNode);
  if (!content) {
    return NS_ERROR_FAILURE;
  }

  nsIAtom *tag = content->Tag();

  if (tag == nsAccessibilityAtoms::form) {
    *aRole = nsIAccessibleRole::ROLE_FORM;
  }
  else if (tag == nsAccessibilityAtoms::div ||
           tag == nsAccessibilityAtoms::blockquote) {
    *aRole = nsIAccessibleRole::ROLE_SECTION;
  }
  else if (tag == nsAccessibilityAtoms::h1 ||
           tag == nsAccessibilityAtoms::h2 ||
           tag == nsAccessibilityAtoms::h3 ||
           tag == nsAccessibilityAtoms::h4 ||
           tag == nsAccessibilityAtoms::h5 ||
           tag == nsAccessibilityAtoms::h6) {
    *aRole = nsIAccessibleRole::ROLE_HEADING;
  }
  else {
    nsIFrame *frame = GetFrame();
    if (frame && frame->GetType() == nsAccessibilityAtoms::blockFrame &&
        frame->GetContent()->Tag() != nsAccessibilityAtoms::input) {
      // An html:input @type="file" is the only input that is exposed as a
      // blockframe. It must be exposed as ROLE_TEXT_CONTAINER for JAWS.
      *aRole = nsIAccessibleRole::ROLE_PARAGRAPH;
    }
    else {
      *aRole = nsIAccessibleRole::ROLE_TEXT_CONTAINER; // In ATK this works
    }
  }
  return NS_OK;
}

nsresult
nsHyperTextAccessible::GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState)
{
  nsresult rv = nsAccessibleWrap::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  if (!aExtraState)
    return NS_OK;

  nsCOMPtr<nsIEditor> editor;
  GetAssociatedEditor(getter_AddRefs(editor));
  if (editor) {
    PRUint32 flags;
    editor->GetFlags(&flags);
    if (0 == (flags & nsIPlaintextEditor::eEditorReadonlyMask)) {
      *aExtraState |= nsIAccessibleStates::EXT_STATE_EDITABLE;
    }
  }

  PRInt32 childCount;
  GetChildCount(&childCount);
  if (childCount > 0) {
    *aExtraState |= nsIAccessibleStates::EXT_STATE_SELECTABLE_TEXT;
  }

  return NS_OK;
}

void nsHyperTextAccessible::CacheChildren()
{
  if (!mWeakShell) {
    // This node has been shut down
    mAccChildCount = eChildCountUninitialized;
    return;
  }

  // Special case for text entry fields, go directly to editor's root for children
  if (mAccChildCount == eChildCountUninitialized) {
    PRUint32 role;
    GetRole(&role);
    if (role != nsIAccessibleRole::ROLE_ENTRY && role != nsIAccessibleRole::ROLE_PASSWORD_TEXT) {
      nsAccessible::CacheChildren();
      return;
    }
    nsCOMPtr<nsIEditor> editor;
    GetAssociatedEditor(getter_AddRefs(editor));
    if (!editor) {
      nsAccessible::CacheChildren();
      return;
    }
    mAccChildCount = 0;  // Avoid reentry
    nsCOMPtr<nsIDOMElement> editorRoot;
    editor->GetRootElement(getter_AddRefs(editorRoot));
    nsCOMPtr<nsIDOMNode> editorRootDOMNode = do_QueryInterface(editorRoot);
    if (!editorRootDOMNode) {
      return;
    }
    nsAccessibleTreeWalker walker(mWeakShell, editorRootDOMNode, PR_TRUE);
    nsCOMPtr<nsPIAccessible> privatePrevAccessible;
    PRInt32 childCount = 0;
    walker.GetFirstChild();
    SetFirstChild(walker.mState.accessible);

    while (walker.mState.accessible) {
      ++ childCount;
      privatePrevAccessible = do_QueryInterface(walker.mState.accessible);
      privatePrevAccessible->SetParent(this);
      walker.GetNextSibling();
      privatePrevAccessible->SetNextSibling(walker.mState.accessible);
    }
    mAccChildCount = childCount;
  }
}

// Substring must be entirely within the same text node
nsIntRect nsHyperTextAccessible::GetBoundsForString(nsIFrame *aFrame, PRUint32 aStartRenderedOffset,
                                                    PRUint32 aEndRenderedOffset)
{
  nsIntRect screenRect;
  NS_ENSURE_TRUE(aFrame, screenRect);
  if (aFrame->GetType() != nsAccessibilityAtoms::textFrame) {
    // XXX fallback for non-text frames, happens for bullets right now
    // but in the future bullets will have proper text frames
    return aFrame->GetScreenRectExternal();
  }

  PRInt32 startContentOffset, endContentOffset;
  nsresult rv = RenderedToContentOffset(aFrame, aStartRenderedOffset, &startContentOffset);
  NS_ENSURE_SUCCESS(rv, screenRect);
  rv = RenderedToContentOffset(aFrame, aEndRenderedOffset, &endContentOffset);
  NS_ENSURE_SUCCESS(rv, screenRect);

  nsIFrame *frame;
  PRInt32 startContentOffsetInFrame;
  // Get the right frame continuation -- not really a child, but a sibling of
  // the primary frame passed in
  rv = aFrame->GetChildFrameContainingOffset(startContentOffset, PR_FALSE,
                                             &startContentOffsetInFrame, &frame);
  NS_ENSURE_SUCCESS(rv, screenRect);

  nsCOMPtr<nsIPresShell> shell = GetPresShell();
  NS_ENSURE_TRUE(shell, screenRect);

  nsPresContext *context = shell->GetPresContext();

  while (frame && startContentOffset < endContentOffset) {
    // Start with this frame's screen rect, which we will 
    // shrink based on the substring we care about within it.
    // We will then add that frame to the total screenRect we
    // are returning.
    nsIntRect frameScreenRect = frame->GetScreenRectExternal();

    // Get the length of the substring in this frame that we want the bounds for
    PRInt32 startFrameTextOffset, endFrameTextOffset;
    frame->GetOffsets(startFrameTextOffset, endFrameTextOffset);
    PRInt32 frameTotalTextLength = endFrameTextOffset - startFrameTextOffset;
    PRInt32 seekLength = endContentOffset - startContentOffset;
    PRInt32 frameSubStringLength = PR_MIN(frameTotalTextLength - startContentOffsetInFrame, seekLength);

    // Add the point where the string starts to the frameScreenRect
    nsPoint frameTextStartPoint;
    rv = frame->GetPointFromOffset(startContentOffset, &frameTextStartPoint);
    NS_ENSURE_SUCCESS(rv, nsIntRect());
    frameScreenRect.x += context->AppUnitsToDevPixels(frameTextStartPoint.x);

    // Use the point for the end offset to calculate the width
    nsPoint frameTextEndPoint;
    rv = frame->GetPointFromOffset(startContentOffset + frameSubStringLength, &frameTextEndPoint);
    NS_ENSURE_SUCCESS(rv, nsIntRect());
    frameScreenRect.width = context->AppUnitsToDevPixels(frameTextEndPoint.x - frameTextStartPoint.x);

    screenRect.UnionRect(frameScreenRect, screenRect);

    // Get ready to loop back for next frame continuation
    startContentOffset += frameSubStringLength;
    startContentOffsetInFrame = 0;
    frame = frame->GetNextContinuation();
  }

  return screenRect;
}

/*
 * Gets the specified text.
 */
nsIFrame*
nsHyperTextAccessible::GetPosAndText(PRInt32& aStartOffset, PRInt32& aEndOffset,
                                     nsAString *aText, nsIFrame **aEndFrame,
                                     nsIntRect *aBoundsRect,
                                     nsIAccessible **aStartAcc,
                                     nsIAccessible **aEndAcc)
{
  if (aStartOffset == nsIAccessibleText::TEXT_OFFSET_END_OF_TEXT) {
    GetCharacterCount(&aStartOffset);
  }
  if (aStartOffset == nsIAccessibleText::TEXT_OFFSET_CARET) {
    GetCaretOffset(&aStartOffset);
  }
  if (aEndOffset == nsIAccessibleText::TEXT_OFFSET_END_OF_TEXT) {
    GetCharacterCount(&aEndOffset);
  }
  if (aEndOffset == nsIAccessibleText::TEXT_OFFSET_CARET) {
    GetCaretOffset(&aEndOffset);
  }

  PRInt32 startOffset = aStartOffset;
  PRInt32 endOffset = aEndOffset;
  // XXX this prevents text interface usage on <input type="password">
  PRBool isPassword =
    (nsAccUtils::Role(this) == nsIAccessibleRole::ROLE_PASSWORD_TEXT);

  // Clear out parameters and set up loop
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
  if (aBoundsRect) {
    aBoundsRect->Empty();
  }
  if (aStartAcc)
    *aStartAcc = nsnull;
  if (aEndAcc)
    *aEndAcc = nsnull;

  nsIntRect unionRect;
  nsCOMPtr<nsIAccessible> accessible, lastAccessible;

  gfxSkipChars skipChars;
  gfxSkipCharsIterator iter;

  // Loop through children and collect valid offsets, text and bounds
  // depending on what we need for out parameters
  while (NextChild(accessible)) {
    lastAccessible = accessible;
    nsRefPtr<nsAccessNode> accessNode = nsAccUtils::QueryAccessNode(accessible);

    nsIFrame *frame = accessNode->GetFrame();
    if (!frame) {
      continue;
    }
    nsIFrame *primaryFrame = frame;
    if (nsAccUtils::IsText(accessible)) {
      // We only need info up to rendered offset -- that is what we're
      // converting to content offset
      PRInt32 substringEndOffset = -1;
      PRUint32 ourRenderedStart = 0;
      PRInt32 ourContentStart = 0;
      if (frame->GetType() == nsAccessibilityAtoms::textFrame) {
        nsresult rv = frame->GetRenderedText(nsnull, &skipChars, &iter);
        if (NS_SUCCEEDED(rv)) {
          ourRenderedStart = iter.GetSkippedOffset();
          ourContentStart = iter.GetOriginalOffset();
          substringEndOffset =
            iter.ConvertOriginalToSkipped(skipChars.GetOriginalCharCount() +
                                          ourContentStart) - ourRenderedStart;
        }
      }
      if (substringEndOffset < 0) {
        // XXX for non-textframe text like list bullets,
        // should go away after list bullet rewrite
        substringEndOffset = nsAccUtils::TextLength(accessible);
      }
      if (startOffset < substringEndOffset) {
        // Our start is within this substring
        if (startOffset > 0 || endOffset < substringEndOffset) {
          // We don't want the whole string for this accessible
          // Get out the continuing text frame with this offset
          PRInt32 outStartLineUnused;
          PRInt32 contentOffset;
          if (frame->GetType() == nsAccessibilityAtoms::textFrame) {
            contentOffset = iter.ConvertSkippedToOriginal(startOffset) +
                            ourRenderedStart - ourContentStart;
          }
          else {
            contentOffset = startOffset;
          }
          frame->GetChildFrameContainingOffset(contentOffset, PR_TRUE,
                                               &outStartLineUnused, &frame);
          if (aEndFrame) {
            *aEndFrame = frame; // We ended in the current frame
            if (aEndAcc)
              NS_ADDREF(*aEndAcc = accessible);
          }
          if (substringEndOffset > endOffset) {
            // Need to stop before the end of the available text
            substringEndOffset = endOffset;
          }
          aEndOffset = endOffset;
        }
        if (aText) {
          if (isPassword) {
            for (PRInt32 count = startOffset; count < substringEndOffset; count ++)
              *aText += '*'; // Show *'s only for password text
          }
          else {
            nsCOMPtr<nsPIAccessible> pAcc(do_QueryInterface(accessible));
            pAcc->AppendTextTo(*aText, startOffset,
                               substringEndOffset - startOffset);
          }
        }
        if (aBoundsRect) {    // Caller wants the bounds of the text
          aBoundsRect->UnionRect(*aBoundsRect,
                                 GetBoundsForString(primaryFrame, startOffset,
                                                    substringEndOffset));
        }
        if (!startFrame) {
          startFrame = frame;
          aStartOffset = startOffset;
          if (aStartAcc)
            NS_ADDREF(*aStartAcc = accessible);
        }
        // We already started copying in this accessible's string,
        // for the next accessible we'll start at offset 0
        startOffset = 0;
      }
      else {
        // We have not found the start position yet, get the new startOffset
        // that is relative to next accessible
        startOffset -= substringEndOffset;
      }
      // The endOffset needs to be relative to the new startOffset
      endOffset -= substringEndOffset;
    }
    else {
      // Embedded object, append marker
      // XXX Append \n for <br>'s
      if (startOffset >= 1) {
        -- startOffset;
      }
      else {
        if (endOffset > 0) {
          if (aText) {
            if (frame->GetType() == nsAccessibilityAtoms::brFrame) {
              *aText += kForcedNewLineChar;
            } else if (nsAccUtils::MustPrune(this)) {
              *aText += kImaginaryEmbeddedObjectChar;
              // Expose imaginary embedded object character if the accessible
              // hans't children.
            } else {
              *aText += kEmbeddedObjectChar;
            }
          }
          if (aBoundsRect) {
            aBoundsRect->UnionRect(*aBoundsRect,
                                   frame->GetScreenRectExternal());
          }
        }
        if (!startFrame) {
          startFrame = frame;
          aStartOffset = 0;
          if (aStartAcc)
            NS_ADDREF(*aStartAcc = accessible);
        }
      }
      -- endOffset;
    }
    if (endOffset <= 0 && startFrame) {
      break; // If we don't have startFrame yet, get that in next loop iteration
    }
  }

  if (aStartAcc && !*aStartAcc) {
    NS_IF_ADDREF(*aStartAcc = lastAccessible);
  }
  if (aEndFrame && !*aEndFrame) {
    *aEndFrame = startFrame;
    if (aStartAcc && aEndAcc)
      NS_IF_ADDREF(*aEndAcc = *aStartAcc);
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
    PRInt32 textLength = nsAccUtils::TextLength(accessible);
    NS_ENSURE_TRUE(textLength >= 0, nsnull);
    *aCharacterCount += textLength;
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
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (text.IsEmpty()) {
    return NS_ERROR_FAILURE;
  }
  *aCharacter = text.First();
  return NS_OK;
}

nsresult nsHyperTextAccessible::DOMPointToHypertextOffset(nsIDOMNode* aNode, PRInt32 aNodeOffset,
                                                          PRInt32* aHyperTextOffset,
                                                          nsIAccessible **aFinalAccessible,
                                                          PRBool aIsEndOffset)
{
  // Turn a DOM Node and offset into an offset into this hypertext.
  // On failure, return null. On success, return the DOM node which contains the offset.
  NS_ENSURE_ARG_POINTER(aHyperTextOffset);
  *aHyperTextOffset = 0;

  if (!aNode) {
    return NS_ERROR_FAILURE;
  }
  if (aFinalAccessible) {
    *aFinalAccessible = nsnull;
  }

  PRUint32 addTextOffset = 0;
  nsCOMPtr<nsIDOMNode> findNode;

  unsigned short nodeType;
  aNode->GetNodeType(&nodeType);
  if (aNodeOffset == -1) {
    findNode = aNode;
  }
  else if (nodeType == nsIDOMNode::TEXT_NODE) {
    // For text nodes, aNodeOffset comes in as a character offset
    // Text offset will be added at the end, if we find the offset in this hypertext
    // We want the "skipped" offset into the text (rendered text without the extra whitespace)
    nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
    NS_ASSERTION(content, "No nsIContent for dom node");
    nsCOMPtr<nsIPresShell> presShell = GetPresShell();
    NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);
    nsIFrame *frame = presShell->GetPrimaryFrameFor(content);
    NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);
    nsresult rv = ContentToRenderedOffset(frame, aNodeOffset, &addTextOffset);
    NS_ENSURE_SUCCESS(rv, rv);
    // Get the child node and 
    findNode = aNode;
  }
  else {
    // For non-text nodes, aNodeOffset comes in as a child node index
    nsCOMPtr<nsIContent> parentContent(do_QueryInterface(aNode));
    // Should not happen, but better to protect against crash if doc node is somehow passed in
    NS_ENSURE_TRUE(parentContent, NS_ERROR_FAILURE);
    // findNode could be null if aNodeOffset == # of child nodes, which means one of two things:
    // 1) we're at the end of the children, keep findNode = null, so that we get the last possible offset
    // 2) there are no children and the passed-in node is mDOMNode, which means we're an aempty nsIAccessibleText
    // 3) there are no children, and the passed-in node is not mDOMNode -- use parentContent for the node to find
     
    findNode = do_QueryInterface(parentContent->GetChildAt(aNodeOffset));
    if (!findNode && !aNodeOffset) {
      if (SameCOMIdentity(parentContent, mDOMNode)) {
        // There are no children, which means this is an empty nsIAccessibleText, in which
        // case we can only be at hypertext offset 0
        *aHyperTextOffset = 0;
        return NS_OK;
      }
      findNode = do_QueryInterface(parentContent); // Case #2: there are no children
    }
  }

  // Get accessible for this findNode, or if that node isn't accessible, use the
  // accessible for the next DOM node which has one (based on forward depth first search)
  nsCOMPtr<nsIAccessible> descendantAccessible;
  if (findNode) {
    nsCOMPtr<nsIContent> findContent = do_QueryInterface(findNode);
    if (findContent->IsNodeOfType(nsINode::eHTML) && 
        findContent->NodeInfo()->Equals(nsAccessibilityAtoms::br)) {
      nsIContent *parent = findContent->GetParent();
      if (parent &&
          parent->IsRootOfNativeAnonymousSubtree() &&
          parent->GetChildCount() == 1) {
        // This <br> is the only node in a text control, therefore it is the hacky
        // "bogus node" used when there is no text in a control
        *aHyperTextOffset = 0;
        return NS_OK;
      }
    }
    descendantAccessible = GetFirstAvailableAccessible(findNode);
  }
  // From the descendant, go up and get the immediate child of this hypertext
  nsCOMPtr<nsIAccessible> childAccessible;
  while (descendantAccessible) {
    nsCOMPtr<nsIAccessible> parentAccessible;
    descendantAccessible->GetParent(getter_AddRefs(parentAccessible));
    if (this == parentAccessible) {
      childAccessible = descendantAccessible;
      break;
    }
    // This offset no longer applies because the passed-in text object is not a child
    // of the hypertext. This happens when there are nested hypertexts, e.g.
    // <div>abc<h1>def</h1>ghi</div>
    // If the passed-in DOM point was not on a direct child of the hypertext, we will
    // return the offset for that entire hypertext
    if (aIsEndOffset) {
      // Not inclusive, the indicated char comes at index before this offset
      // If the end offset is after the first character of the passed in object, use 1 for
    // addTextOffset, to put us after the embedded object char. We'll only treat the offset as
    // before the embedded object char if we end at the very beginning of the child.
    addTextOffset = addTextOffset > 0;
    }
    else {
      // Start offset, inclusive
      // Make sure the offset lands on the embedded object character in order to indicate
      // the true inner offset is inside the subtree for that link
      addTextOffset =
        (nsAccUtils::TextLength(descendantAccessible) == static_cast<PRInt32>(addTextOffset)) ? 1 : 0;
    }
    descendantAccessible = parentAccessible;
  }  

  // Loop through, adding offsets until we reach childAccessible
  // If childAccessible is null we will end up adding up the entire length of
  // the hypertext, which is good -- it just means our offset node
  // came after the last accessible child's node
  nsCOMPtr<nsIAccessible> accessible;
  while (NextChild(accessible) && accessible != childAccessible) {
    PRInt32 textLength = nsAccUtils::TextLength(accessible);
    NS_ENSURE_TRUE(textLength >= 0, nsnull);
    *aHyperTextOffset += textLength;
  }
  if (accessible) {
    *aHyperTextOffset += addTextOffset;
    NS_ASSERTION(accessible == childAccessible, "These should be equal whenever we exit loop and accessible != nsnull");
    if (aFinalAccessible &&
        (NextChild(accessible) ||
         static_cast<PRInt32>(addTextOffset) < nsAccUtils::TextLength(childAccessible))) {  
      // If not at end of last text node, we will return the accessible we were in
      NS_ADDREF(*aFinalAccessible = childAccessible);
    }
  }

  return NS_OK;
}

nsresult
nsHyperTextAccessible::HypertextOffsetToDOMPoint(PRInt32 aHTOffset,
                                                 nsIDOMNode **aNode,
                                                 PRInt32 *aOffset)
{
  nsCOMPtr<nsIDOMNode> endNode;
  PRInt32 endOffset;

  return HypertextOffsetsToDOMRange(aHTOffset, aHTOffset, aNode, aOffset,
                                    getter_AddRefs(endNode), &endOffset);
}

nsresult
nsHyperTextAccessible::HypertextOffsetsToDOMRange(PRInt32 aStartHTOffset,
                                                  PRInt32 aEndHTOffset,
                                                  nsIDOMNode **aStartNode,
                                                  PRInt32 *aStartOffset,
                                                  nsIDOMNode **aEndNode,
                                                  PRInt32 *aEndOffset)
{
  NS_ENSURE_ARG_POINTER(aStartNode);
  *aStartNode = nsnull;

  NS_ENSURE_ARG_POINTER(aStartOffset);
  *aStartOffset = -1;

  NS_ENSURE_ARG_POINTER(aEndNode);
  *aEndNode = nsnull;

  NS_ENSURE_ARG_POINTER(aEndOffset);
  *aEndOffset = -1;

  // If the given offsets are 0 and associated editor is empty then return
  // collapsed range with editor root element as range container.
  if (aStartHTOffset == 0 && aEndHTOffset == 0) {
    nsCOMPtr<nsIEditor> editor;
    GetAssociatedEditor(getter_AddRefs(editor));
    if (editor) {
      PRBool isEmpty = PR_FALSE;
      editor->GetDocumentIsEmpty(&isEmpty);
      if (isEmpty) {
        nsCOMPtr<nsIDOMElement> editorRootElm;
        editor->GetRootElement(getter_AddRefs(editorRootElm));

        nsCOMPtr<nsIDOMNode> editorRoot(do_QueryInterface(editorRootElm));
        if (editorRoot) {
          *aStartOffset = *aEndOffset = 0;
          NS_ADDREF(*aStartNode = editorRoot);
          NS_ADDREF(*aEndNode = editorRoot);

          return NS_OK;
        }
      }
    }
  }

  nsCOMPtr<nsIAccessible> startAcc, endAcc;
  PRInt32 startOffset = aStartHTOffset, endOffset = aEndHTOffset;
  nsIFrame *startFrame = nsnull, *endFrame = nsnull;

  startFrame = GetPosAndText(startOffset, endOffset, nsnull, &endFrame, nsnull,
                             getter_AddRefs(startAcc), getter_AddRefs(endAcc));
  if (!startAcc || !endAcc)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNode> startNode, endNode;
  nsresult rv = GetDOMPointByFrameOffset(startFrame, startOffset, startAcc,
                                         getter_AddRefs(startNode),
                                         &startOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aStartHTOffset != aEndHTOffset) {
    rv = GetDOMPointByFrameOffset(endFrame, endOffset, endAcc,
                                  getter_AddRefs(endNode), &endOffset);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    endNode = startNode;
    endOffset = startOffset;
  }

  NS_ADDREF(*aStartNode = startNode);
  *aStartOffset = startOffset;

  NS_ADDREF(*aEndNode = endNode);
  *aEndOffset = endOffset;

  return NS_OK;
}

PRInt32
nsHyperTextAccessible::GetRelativeOffset(nsIPresShell *aPresShell,
                                         nsIFrame *aFromFrame,
                                         PRInt32 aFromOffset,
                                         nsIAccessible *aFromAccessible,
                                         nsSelectionAmount aAmount,
                                         nsDirection aDirection,
                                         PRBool aNeedsStart)
{
  const PRBool kIsJumpLinesOk = PR_TRUE;          // okay to jump lines
  const PRBool kIsScrollViewAStop = PR_FALSE;     // do not stop at scroll views
  const PRBool kIsKeyboardSelect = PR_TRUE;       // is keyboard selection
  const PRBool kIsVisualBidi = PR_FALSE;          // use visual order for bidi text

  EWordMovementType wordMovementType = aNeedsStart ? eStartWord : eEndWord;
  if (aAmount == eSelectLine) {
    aAmount = (aDirection == eDirNext) ? eSelectEndLine : eSelectBeginLine;
  }

  // Ask layout for the new node and offset, after moving the appropriate amount
  nsPeekOffsetStruct pos;

  nsresult rv;
  PRInt32 contentOffset = aFromOffset;
  if (nsAccUtils::IsText(aFromAccessible)) {
    nsRefPtr<nsAccessNode> accessNode =
      nsAccUtils::QueryAccessNode(aFromAccessible);

    nsIFrame *frame = accessNode->GetFrame();
    NS_ENSURE_TRUE(frame, -1);

    if (frame->GetType() == nsAccessibilityAtoms::textFrame) {
      rv = RenderedToContentOffset(frame, aFromOffset, &contentOffset);
      NS_ENSURE_SUCCESS(rv, -1);
    }
  }

  pos.SetData(aAmount, aDirection, contentOffset,
              0, kIsJumpLinesOk, kIsScrollViewAStop, kIsKeyboardSelect, kIsVisualBidi,
              wordMovementType);
  rv = aFromFrame->PeekOffset(&pos);
  if (NS_FAILED(rv)) {
    if (aDirection == eDirPrevious) {
      // Use passed-in frame as starting point in failure case for now,
      // this is a hack to deal with starting on a list bullet frame,
      // which fails in PeekOffset() because the line iterator doesn't see it.
      // XXX Need to look at our overall handling of list bullets, which are an odd case
      pos.mResultContent = aFromFrame->GetContent();
      PRInt32 endOffsetUnused;
      aFromFrame->GetOffsets(pos.mContentOffset, endOffsetUnused);
    }
    else {
      return -1;
    }
  }

  // Turn the resulting node and offset into a hyperTextOffset
  PRInt32 hyperTextOffset;
  nsCOMPtr<nsIDOMNode> resultNode = do_QueryInterface(pos.mResultContent);
  NS_ENSURE_TRUE(resultNode, -1);

  nsCOMPtr<nsIAccessible> finalAccessible;
  rv = DOMPointToHypertextOffset(resultNode, pos.mContentOffset, &hyperTextOffset,
                                 getter_AddRefs(finalAccessible),
                                 aDirection == eDirNext);
  // If finalAccessible == nsnull, then DOMPointToHypertextOffset() searched through the hypertext
  // children without finding the node/offset position
  NS_ENSURE_SUCCESS(rv, -1);

  if (!finalAccessible && aDirection == eDirPrevious) {
    // If we reached the end during search, this means we didn't find the DOM point
    // and we're actually at the start of the paragraph
    hyperTextOffset = 0;
  }  
  else if (aAmount == eSelectBeginLine) {
    // For line selection with needsStart, set start of line exactly to line break
    if (pos.mContentOffset == 0 && mFirstChild && 
        nsAccUtils::Role(mFirstChild) == nsIAccessibleRole::ROLE_STATICTEXT &&
        nsAccUtils::TextLength(mFirstChild) == hyperTextOffset) {
      // XXX Bullet hack -- we should remove this once list bullets use anonymous content
      hyperTextOffset = 0;
    }
    if (!aNeedsStart && hyperTextOffset > 0) {
      -- hyperTextOffset;
    }
  }
  else if (aAmount == eSelectEndLine && finalAccessible) { 
    // If not at very end of hypertext, we may need change the end of line offset by 1, 
    // to make sure we are in the right place relative to the line ending
    if (nsAccUtils::Role(finalAccessible) == nsIAccessibleRole::ROLE_WHITESPACE) {  // Landed on <br> hard line break
      // if aNeedsStart, set end of line exactly 1 character past line break
      // XXX It would be cleaner if we did not have to have the hard line break check,
      // and just got the correct results from PeekOffset() for the <br> case -- the returned offset should
      // come after the new line, as it does in other cases.
      ++ hyperTextOffset;  // Get past hard line break
    }
    // We are now 1 character past the line break
    if (!aNeedsStart) {
      -- hyperTextOffset;
    }
  }

  return hyperTextOffset;
}

/*
Gets the specified text relative to aBoundaryType, which means:
BOUNDARY_CHAR             The character before/at/after the offset is returned.
BOUNDARY_WORD_START       From the word start before/at/after the offset to the next word start.
BOUNDARY_WORD_END         From the word end before/at/after the offset to the next work end.
BOUNDARY_LINE_START       From the line start before/at/after the offset to the next line start.
BOUNDARY_LINE_END         From the line end before/at/after the offset to the next line start.
*/

nsresult nsHyperTextAccessible::GetTextHelper(EGetTextType aType, nsAccessibleTextBoundary aBoundaryType,
                                              PRInt32 aOffset, PRInt32 *aStartOffset, PRInt32 *aEndOffset,
                                              nsAString &aText)
{
  aText.Truncate();

  NS_ENSURE_ARG_POINTER(aStartOffset);
  NS_ENSURE_ARG_POINTER(aEndOffset);
  *aStartOffset = *aEndOffset = 0;

  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }

  if (aOffset == nsIAccessibleText::TEXT_OFFSET_END_OF_TEXT) {
    GetCharacterCount(&aOffset);
  }
  if (aOffset == nsIAccessibleText::TEXT_OFFSET_CARET) {
    GetCaretOffset(&aOffset);
    if (aOffset > 0 && (aBoundaryType == BOUNDARY_LINE_START ||
                        aBoundaryType == BOUNDARY_LINE_END)) {
      // It is the same character offset when the caret is visually at the very end of a line
      // or the start of a new line. Getting text at the line should provide the line with the visual caret,
      // otherwise screen readers will announce the wrong line as the user presses up or down arrow and land
      // at the end of a line.
      nsCOMPtr<nsISelection> domSel;
      nsresult rv = GetSelections(nsISelectionController::SELECTION_NORMAL,
                                  nsnull, getter_AddRefs(domSel));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsISelectionPrivate> privateSelection(do_QueryInterface(domSel));
      nsCOMPtr<nsFrameSelection> frameSelection;
      rv = privateSelection->GetFrameSelection(getter_AddRefs(frameSelection));
      NS_ENSURE_SUCCESS(rv, rv);

      if (frameSelection->GetHint() == nsFrameSelection::HINTLEFT) {
        -- aOffset;  // We are at the start of a line
      }
    }
  }
  else if (aOffset < 0) {
    return NS_ERROR_FAILURE;
  }

  nsSelectionAmount amount;
  PRBool needsStart = PR_FALSE;
  switch (aBoundaryType) {
    case BOUNDARY_CHAR:
      amount = eSelectCharacter;
      if (aType == eGetAt)
        aType = eGetAfter; // Avoid returning 2 characters
      break;

    case BOUNDARY_WORD_START:
      needsStart = PR_TRUE;
      amount = eSelectWord;
      break;

    case BOUNDARY_WORD_END:
      amount = eSelectWord;
      break;

    case BOUNDARY_LINE_START:
      // Newlines are considered at the end of a line. Since getting
      // the BOUNDARY_LINE_START gets the text from the line-start to the next
      // line-start, the newline is included at the end of the string.
      needsStart = PR_TRUE;
      amount = eSelectLine;
      break;

    case BOUNDARY_LINE_END:
      // Newlines are considered at the end of a line. Since getting
      // the BOUNDARY_END_START gets the text from the line-end to the next
      //line-end, the newline is included at the beginning of the string.
      amount = eSelectLine;
      break;

    case BOUNDARY_ATTRIBUTE_RANGE:
    {
      nsresult rv = GetTextAttributes(PR_FALSE, aOffset,
                                      aStartOffset, aEndOffset, nsnull);
      NS_ENSURE_SUCCESS(rv, rv);
      
      return GetText(*aStartOffset, *aEndOffset, aText);
    }

    default:  // Note, sentence support is deprecated and falls through to here
      return NS_ERROR_INVALID_ARG;
  }

  PRInt32 startOffset = aOffset + (aBoundaryType == BOUNDARY_LINE_END);  // Avoid getting the previous line
  PRInt32 endOffset = startOffset;

  // Convert offsets to frame-relative
  nsCOMPtr<nsIAccessible> startAcc;
  nsIFrame *startFrame = GetPosAndText(startOffset, endOffset, nsnull, nsnull,
                                       nsnull, getter_AddRefs(startAcc));

  if (!startFrame) {
    PRInt32 textLength;
    GetCharacterCount(&textLength);
    if (aBoundaryType == BOUNDARY_LINE_START && aOffset > 0 && aOffset == textLength) {
      // Asking for start of line, while on last character
      if (startAcc) {
        nsRefPtr<nsAccessNode> startAccessNode =
          nsAccUtils::QueryAccessNode(startAcc);
        startFrame = startAccessNode->GetFrame();
      }
    }
    if (!startFrame) {
      return aOffset > textLength ? NS_ERROR_FAILURE : NS_OK;
    }
    else {
      // We're on the last continuation since we're on the last character
      startFrame = startFrame->GetLastContinuation();
    }
  }

  PRInt32 finalStartOffset, finalEndOffset;

  // If aType == eGetAt we'll change both the start and end offset from
  // the original offset
  if (aType == eGetAfter) {
    finalStartOffset = aOffset;
  }
  else {
    finalStartOffset = GetRelativeOffset(presShell, startFrame, startOffset,
                                         startAcc, amount, eDirPrevious,
                                         needsStart);
    NS_ENSURE_TRUE(finalStartOffset >= 0, NS_ERROR_FAILURE);
  }

  if (aType == eGetBefore) {
    endOffset = aOffset;
  }
  else {
    // Start moving forward from the start so that we don't get 
    // 2 words/lines if the offset occured on whitespace boundary
    // Careful, startOffset and endOffset are passed by reference to GetPosAndText() and changed
    // For BOUNDARY_LINE_END, make sure we start of this line
    startOffset = endOffset = finalStartOffset + (aBoundaryType == BOUNDARY_LINE_END);
    nsCOMPtr<nsIAccessible> endAcc;
    nsIFrame *endFrame = GetPosAndText(startOffset, endOffset, nsnull, nsnull,
                                       nsnull, getter_AddRefs(endAcc));
    if (nsAccUtils::Role(endAcc) == nsIAccessibleRole::ROLE_STATICTEXT) {
      // Static text like list bullets will ruin our forward calculation,
      // since the caret cannot be in the static text. Start just after the static text.
      startOffset = endOffset = finalStartOffset +
                                (aBoundaryType == BOUNDARY_LINE_END) +
                                nsAccUtils::TextLength(endAcc);

      endFrame = GetPosAndText(startOffset, endOffset, nsnull, nsnull,
                               nsnull, getter_AddRefs(endAcc));
    }
    if (!endFrame) {
      return NS_ERROR_FAILURE;
    }
    finalEndOffset = GetRelativeOffset(presShell, endFrame, endOffset, endAcc,
                                       amount, eDirNext, needsStart);
    NS_ENSURE_TRUE(endOffset >= 0, NS_ERROR_FAILURE);
    if (finalEndOffset == aOffset) {
      if (aType == eGetAt && amount == eSelectWord) { 
        // Fix word error for the first character in word: PeekOffset() will return the previous word when 
        // aOffset points to the first character of the word, but accessibility APIs want the current word 
        // that the first character is in
        return GetTextHelper(eGetAfter, aBoundaryType, aOffset, aStartOffset, aEndOffset, aText);
      }
      PRInt32 textLength;
      GetCharacterCount(&textLength);
      if (finalEndOffset < textLength) {
        // This happens sometimes when current character at finalStartOffset 
        // is an embedded object character representing another hypertext, that
        // the AT really needs to dig into separately
        ++ finalEndOffset;
      }
    }
  }

  *aStartOffset = finalStartOffset;
  *aEndOffset = finalEndOffset;

  NS_ASSERTION((finalStartOffset < aOffset && finalEndOffset >= aOffset) || aType != eGetBefore, "Incorrect results for GetTextHelper");
  NS_ASSERTION((finalStartOffset <= aOffset && finalEndOffset > aOffset) || aType == eGetBefore, "Incorrect results for GetTextHelper");

  GetPosAndText(finalStartOffset, finalEndOffset, &aText);
  return NS_OK;
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

// nsIPersistentProperties
// nsIAccessibleText::getTextAttributes(in boolean includeDefAttrs,
//                                      in long offset,
//                                      out long rangeStartOffset,
//                                      out long rangeEndOffset);
NS_IMETHODIMP
nsHyperTextAccessible::GetTextAttributes(PRBool aIncludeDefAttrs,
                                         PRInt32 aOffset,
                                         PRInt32 *aStartOffset,
                                         PRInt32 *aEndOffset,
                                         nsIPersistentProperties **aAttributes)
{
  // 1. First we get spell check, then language, then the set of CSS-based
  //    attributes.
  // 2. As we get each new attribute, we pass the current start and end offsets
  //    as in/out parameters. In other words, as attributes are collected,
  //    the attribute range itself can only stay the same or get smaller.
  //
  // Example:
  //  Current: range 5-10
  //  Adding:  range 7-12
  //  Result:  range 7-10

  NS_ENSURE_ARG_POINTER(aStartOffset);
  *aStartOffset = 0;

  NS_ENSURE_ARG_POINTER(aEndOffset);
  nsresult rv = GetCharacterCount(aEndOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aAttributes) {
    *aAttributes = nsnull;

    nsCOMPtr<nsIPersistentProperties> attributes =
      do_CreateInstance(NS_PERSISTENTPROPERTIES_CONTRACTID);
    NS_ENSURE_TRUE(attributes, NS_ERROR_OUT_OF_MEMORY);

    NS_ADDREF(*aAttributes = attributes);
  }

  if (!mDOMNode)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNode> node;
  PRInt32 nodeOffset = 0;
  rv = HypertextOffsetToDOMPoint(aOffset, getter_AddRefs(node), &nodeOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  // Set 'misspelled' text attribute.
  rv = GetSpellTextAttribute(node, nodeOffset, aStartOffset, aEndOffset,
                             aAttributes ? *aAttributes : nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIContent> content(do_QueryInterface(node));
  if (content && content->IsNodeOfType(nsINode::eELEMENT))
    node = do_QueryInterface(content->GetChildAt(nodeOffset));

  if (!node)
    return NS_OK;

  nsTextAttrsMgr textAttrsMgr(this, mDOMNode, aIncludeDefAttrs, node);
  return textAttrsMgr.GetAttributes(*aAttributes, aStartOffset, aEndOffset);
}

// nsIPersistentProperties
// nsIAccessibleText::defaultTextAttributes
NS_IMETHODIMP
nsHyperTextAccessible::GetDefaultTextAttributes(nsIPersistentProperties **aAttributes)
{
  NS_ENSURE_ARG_POINTER(aAttributes);
  *aAttributes = nsnull;

  nsCOMPtr<nsIPersistentProperties> attributes =
    do_CreateInstance(NS_PERSISTENTPROPERTIES_CONTRACTID);
  NS_ENSURE_TRUE(attributes, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*aAttributes = attributes);

  if (!mDOMNode)
    return NS_ERROR_FAILURE;

  nsTextAttrsMgr textAttrsMgr(this, mDOMNode, PR_TRUE, nsnull);
  return textAttrsMgr.GetAttributes(*aAttributes);
}

nsresult
nsHyperTextAccessible::GetAttributesInternal(nsIPersistentProperties *aAttributes)
{
  if (!mDOMNode) {
    return NS_ERROR_FAILURE;  // Node already shut down
  }

  nsresult rv = nsAccessibleWrap::GetAttributesInternal(aAttributes);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIContent> content = nsCoreUtils::GetRoleContent(mDOMNode);
  NS_ENSURE_TRUE(content, NS_ERROR_UNEXPECTED);
  nsIAtom *tag = content->Tag();

  PRInt32 headLevel = 0;
  if (tag == nsAccessibilityAtoms::h1)
    headLevel = 1;
  else if (tag == nsAccessibilityAtoms::h2)
    headLevel = 2;
  else if (tag == nsAccessibilityAtoms::h3)
    headLevel = 3;
  else if (tag == nsAccessibilityAtoms::h4)
    headLevel = 4;
  else if (tag == nsAccessibilityAtoms::h5)
    headLevel = 5;
  else if (tag == nsAccessibilityAtoms::h6)
    headLevel = 6;

  if (headLevel) {
    nsAutoString strHeadLevel;
    strHeadLevel.AppendInt(headLevel);
    nsAccUtils::SetAccAttr(aAttributes, nsAccessibilityAtoms::level,
                           strHeadLevel);
  }
  
  // Indicate when the current object uses block-level formatting
  // via formatting: block
  // XXX: 'formatting' attribute is deprecated and will be removed in Mozilla2,
  // use 'display' attribute instead.
  nsIFrame *frame = GetFrame();
  if (frame && frame->GetType() == nsAccessibilityAtoms::blockFrame) {
    nsAutoString oldValueUnused;
    aAttributes->SetStringProperty(NS_LITERAL_CSTRING("formatting"), NS_LITERAL_STRING("block"),
                                   oldValueUnused);
  }

  if (gLastFocusedNode == mDOMNode) {
    PRInt32 lineNumber = GetCaretLineNumber();
    if (lineNumber >= 1) {
      nsAutoString strLineNumber;
      strLineNumber.AppendInt(lineNumber);
      nsAccUtils::SetAccAttr(aAttributes, nsAccessibilityAtoms::lineNumber,
                             strLineNumber);
    }
  }

  return  NS_OK;
}

/*
 * Given an offset, the x, y, width, and height values are filled appropriately.
 */
NS_IMETHODIMP nsHyperTextAccessible::GetCharacterExtents(PRInt32 aOffset, PRInt32 *aX, PRInt32 *aY,
                                                         PRInt32 *aWidth, PRInt32 *aHeight,
                                                         PRUint32 aCoordType)
{
  return GetRangeExtents(aOffset, aOffset + 1, aX, aY, aWidth, aHeight, aCoordType);
}

/*
 * Given a start & end offset, the x, y, width, and height values are filled appropriately.
 */
NS_IMETHODIMP nsHyperTextAccessible::GetRangeExtents(PRInt32 aStartOffset, PRInt32 aEndOffset,
                                                     PRInt32 *aX, PRInt32 *aY,
                                                     PRInt32 *aWidth, PRInt32 *aHeight,
                                                     PRUint32 aCoordType)
{
  nsIntRect boundsRect;
  nsIFrame *endFrameUnused;
  if (!GetPosAndText(aStartOffset, aEndOffset, nsnull, &endFrameUnused, &boundsRect) ||
      boundsRect.IsEmpty()) {
    return NS_ERROR_FAILURE;
  }

  *aX = boundsRect.x;
  *aY = boundsRect.y;
  *aWidth = boundsRect.width;
  *aHeight = boundsRect.height;

  return nsAccUtils::ConvertScreenCoordsTo(aX, aY, aCoordType, this);
}

/*
 * Gets the offset of the character located at coordinates x and y. x and y are interpreted as being relative to
 * the screen or this widget's window depending on coords.
 */
NS_IMETHODIMP
nsHyperTextAccessible::GetOffsetAtPoint(PRInt32 aX, PRInt32 aY,
                                        PRUint32 aCoordType, PRInt32 *aOffset)
{
  *aOffset = -1;
  nsCOMPtr<nsIPresShell> shell = GetPresShell();
  if (!shell) {
    return NS_ERROR_FAILURE;
  }
  nsIFrame *hyperFrame = GetFrame();
  if (!hyperFrame) {
    return NS_ERROR_FAILURE;
  }
  nsIntRect frameScreenRect = hyperFrame->GetScreenRectExternal();

  nsIntPoint coords;
  nsresult rv = nsAccUtils::ConvertToScreenCoords(aX, aY, aCoordType,
                                                  this, &coords);
  NS_ENSURE_SUCCESS(rv, rv);

  // coords are currently screen coordinates, and we need to turn them into
  // frame coordinates relative to the current accessible
  if (!frameScreenRect.Contains(coords.x, coords.y)) {
    return NS_OK;   // Not found, will return -1
  }
  nsIntPoint pxInHyperText(coords.x - frameScreenRect.x,
                           coords.y - frameScreenRect.y);
  nsPresContext *context = GetPresContext();
  NS_ENSURE_TRUE(context, NS_ERROR_FAILURE);
  nsPoint pointInHyperText(context->DevPixelsToAppUnits(pxInHyperText.x),
                           context->DevPixelsToAppUnits(pxInHyperText.y));

  // Go through the frames to check if each one has the point.
  // When one does, add up the character offsets until we have a match

  // We have an point in an accessible child of this, now we need to add up the
  // offsets before it to what we already have
  nsCOMPtr<nsIAccessible> accessible;
  PRInt32 offset = 0;

  while (NextChild(accessible)) {
    nsRefPtr<nsAccessNode> accessNode = nsAccUtils::QueryAccessNode(accessible);

    nsIFrame *primaryFrame = accessNode->GetFrame();
    NS_ENSURE_TRUE(primaryFrame, NS_ERROR_FAILURE);

    nsIFrame *frame = primaryFrame;
    while (frame) {
      nsIContent *content = frame->GetContent();
      NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);
      nsPoint pointInFrame = pointInHyperText - frame->GetOffsetToExternal(hyperFrame);
      nsSize frameSize = frame->GetSize();
      if (pointInFrame.x < frameSize.width && pointInFrame.y < frameSize.height) {
        // Finished
        if (frame->GetType() == nsAccessibilityAtoms::textFrame) {
          nsIFrame::ContentOffsets contentOffsets = frame->GetContentOffsetsFromPointExternal(pointInFrame, PR_TRUE);
          if (contentOffsets.IsNull() || contentOffsets.content != content) {
            return NS_OK; // Not found, will return -1
          }
          PRUint32 addToOffset;
          nsresult rv = ContentToRenderedOffset(primaryFrame,
                                                contentOffsets.offset,
                                                &addToOffset);
          NS_ENSURE_SUCCESS(rv, rv);
          offset += addToOffset;
        }
        *aOffset = offset;
        return NS_OK;
      }
      frame = frame->GetNextContinuation();
    }
    PRInt32 textLength = nsAccUtils::TextLength(accessible);
    NS_ENSURE_TRUE(textLength >= 0, NS_ERROR_FAILURE);
    offset += textLength;
  }

  return NS_OK; // Not found, will return -1
}

// ------- nsIAccessibleHyperText ---------------
NS_IMETHODIMP
nsHyperTextAccessible::GetLinkCount(PRInt32 *aLinkCount)
{
  NS_ENSURE_ARG_POINTER(aLinkCount);
  *aLinkCount = 0;
  if (!mDOMNode) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIAccessible> accessible;

  while (NextChild(accessible)) {
    if (nsAccUtils::IsEmbeddedObject(accessible))
      ++*aLinkCount;
  }
  return NS_OK;
}


NS_IMETHODIMP
nsHyperTextAccessible::GetLink(PRInt32 aLinkIndex, nsIAccessibleHyperLink **aLink)
{
  NS_ENSURE_ARG_POINTER(aLink);
  *aLink = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  PRInt32 linkIndex = aLinkIndex;
  nsCOMPtr<nsIAccessible> accessible;
  while (NextChild(accessible)) {
    if (nsAccUtils::IsEmbeddedObject(accessible) && linkIndex-- == 0)
      return CallQueryInterface(accessible, aLink);
  }

  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
nsHyperTextAccessible::GetLinkIndex(PRInt32 aCharIndex, PRInt32 *aLinkIndex)
{
  NS_ENSURE_ARG_POINTER(aLinkIndex);
  *aLinkIndex = -1; // API says this magic value means 'not found'

  PRInt32 characterCount = 0;
  PRInt32 linkIndex = 0;
  if (!mDOMNode) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIAccessible> accessible;

  while (NextChild(accessible) && characterCount <= aCharIndex) {
    PRUint32 role = nsAccUtils::Role(accessible);
    if (role == nsIAccessibleRole::ROLE_TEXT_LEAF ||
        role == nsIAccessibleRole::ROLE_STATICTEXT) {
      PRInt32 textLength = nsAccUtils::TextLength(accessible);
      NS_ENSURE_TRUE(textLength >= 0, NS_ERROR_FAILURE);
      characterCount += textLength;
    }
    else {
      if (characterCount ++ == aCharIndex) {
        *aLinkIndex = linkIndex;
        break;
      }
      if (role != nsIAccessibleRole::ROLE_WHITESPACE) {
        ++ linkIndex;
      }
    }
  }
  return NS_OK;
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
    nsCOMPtr<nsIEditor> editor;
    GetAssociatedEditor(getter_AddRefs(editor));
    nsCOMPtr<nsIPlaintextEditor> peditor(do_QueryInterface(editor));
    return peditor ? peditor->InsertText(aText) : NS_ERROR_FAILURE;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHyperTextAccessible::CopyText(PRInt32 aStartPos, PRInt32 aEndPos)
{
  nsCOMPtr<nsIEditor> editor;
  GetAssociatedEditor(getter_AddRefs(editor));
  if (editor && NS_SUCCEEDED(SetSelectionRange(aStartPos, aEndPos)))
    return editor->Copy();

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHyperTextAccessible::CutText(PRInt32 aStartPos, PRInt32 aEndPos)
{
  nsCOMPtr<nsIEditor> editor;
  GetAssociatedEditor(getter_AddRefs(editor));
  if (editor && NS_SUCCEEDED(SetSelectionRange(aStartPos, aEndPos)))
    return editor->Cut();

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHyperTextAccessible::DeleteText(PRInt32 aStartPos, PRInt32 aEndPos)
{
  nsCOMPtr<nsIEditor> editor;
  GetAssociatedEditor(getter_AddRefs(editor));
  if (editor && NS_SUCCEEDED(SetSelectionRange(aStartPos, aEndPos)))
    return editor->DeleteSelection(nsIEditor::eNone);

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHyperTextAccessible::PasteText(PRInt32 aPosition)
{
  nsCOMPtr<nsIEditor> editor;
  GetAssociatedEditor(getter_AddRefs(editor));
  if (editor && NS_SUCCEEDED(SetCaretOffset(aPosition)))
    return editor->Paste(nsIClipboard::kGlobalClipboard);

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsHyperTextAccessible::GetAssociatedEditor(nsIEditor **aEditor)
{
  NS_ENSURE_ARG_POINTER(aEditor);

  *aEditor = nsnull;
  nsCOMPtr<nsIContent> content = do_QueryInterface(mDOMNode);
  NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);

  if (!content->HasFlag(NODE_IS_EDITABLE)) {
    // If we're inside an editable container, then return that container's editor
    nsCOMPtr<nsIAccessible> ancestor, current = this;
    while (NS_SUCCEEDED(current->GetParent(getter_AddRefs(ancestor))) && ancestor) {
      nsRefPtr<nsHyperTextAccessible> ancestorTextAccessible;
      ancestor->QueryInterface(NS_GET_IID(nsHyperTextAccessible),
                               getter_AddRefs(ancestorTextAccessible));
      if (ancestorTextAccessible) {
        // Recursion will stop at container doc because it has its own impl
        // of GetAssociatedEditor()
        return ancestorTextAccessible->GetAssociatedEditor(aEditor);
      }
      current = ancestor;
    }
    return NS_OK;
  }

  nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem =
    nsCoreUtils::GetDocShellTreeItemFor(mDOMNode);
  nsCOMPtr<nsIEditingSession> editingSession(do_GetInterface(docShellTreeItem));
  if (!editingSession)
    return NS_OK; // No editing session interface

  nsCOMPtr<nsIPresShell> shell = GetPresShell();
  NS_ENSURE_TRUE(shell, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocument> doc = shell->GetDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  nsCOMPtr<nsIEditor> editor;
  return editingSession->GetEditorForWindow(doc->GetWindow(), aEditor);
}

/**
  * =================== Caret & Selection ======================
  */

nsresult nsHyperTextAccessible::SetSelectionRange(PRInt32 aStartPos, PRInt32 aEndPos)
{
  // Set the selection
  nsresult rv = SetSelectionBounds(0, aStartPos, aEndPos);
  NS_ENSURE_SUCCESS(rv, rv);

  // If range 0 was successfully set, clear any additional selection 
  // ranges remaining from previous selection
  nsCOMPtr<nsISelection> domSel;
  nsCOMPtr<nsISelectionController> selCon;
  GetSelections(nsISelectionController::SELECTION_NORMAL,
                getter_AddRefs(selCon), getter_AddRefs(domSel));
  if (domSel) {
    PRInt32 numRanges;
    domSel->GetRangeCount(&numRanges);

    for (PRInt32 count = 0; count < numRanges - 1; count ++) {
      nsCOMPtr<nsIDOMRange> range;
      domSel->GetRangeAt(1, getter_AddRefs(range));
      domSel->RemoveRange(range);
    }
  }
  
  if (selCon) {
    // XXX I'm not sure this can do synchronous scrolling. If the last param is
    // set to true, this calling might flush the pending reflow. See bug 418470.
    selCon->ScrollSelectionIntoView(nsISelectionController::SELECTION_NORMAL,
      nsISelectionController::SELECTION_FOCUS_REGION, PR_FALSE);
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
NS_IMETHODIMP
nsHyperTextAccessible::GetCaretOffset(PRInt32 *aCaretOffset)
{
  *aCaretOffset = -1;

  // No caret if the focused node is not inside this DOM node and this DOM node
  // is not inside of focused node.
  PRBool isInsideOfFocusedNode =
    nsCoreUtils::IsAncestorOf(gLastFocusedNode, mDOMNode);

  if (!isInsideOfFocusedNode && mDOMNode != gLastFocusedNode &&
      !nsCoreUtils::IsAncestorOf(mDOMNode, gLastFocusedNode))
    return NS_OK;

  // Turn the focus node and offset of the selection into caret hypretext
  // offset.
  nsCOMPtr<nsISelection> domSel;
  nsresult rv = GetSelections(nsISelectionController::SELECTION_NORMAL,
                              nsnull, getter_AddRefs(domSel));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> focusNode;
  rv = domSel->GetFocusNode(getter_AddRefs(focusNode));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 focusOffset;
  rv = domSel->GetFocusOffset(&focusOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  // No caret if this DOM node is inside of focused node but the selection's
  // focus point is not inside of this DOM node.
  if (isInsideOfFocusedNode) {
    nsCOMPtr<nsIDOMNode> resultNode =
      nsCoreUtils::GetDOMNodeFromDOMPoint(focusNode, focusOffset);
    if (resultNode != mDOMNode &&
        !nsCoreUtils::IsAncestorOf(mDOMNode, resultNode))
      return NS_OK;
  }

  return DOMPointToHypertextOffset(focusNode, focusOffset, aCaretOffset);
}

PRInt32 nsHyperTextAccessible::GetCaretLineNumber()
{
  // Provide the line number for the caret, relative to the
  // currently focused node. Use a 1-based index
  nsCOMPtr<nsISelection> domSel;
  GetSelections(nsISelectionController::SELECTION_NORMAL, nsnull,
                getter_AddRefs(domSel));
  nsCOMPtr<nsISelectionPrivate> privateSelection(do_QueryInterface(domSel));
  NS_ENSURE_TRUE(privateSelection, -1);
  nsCOMPtr<nsFrameSelection> frameSelection;
  privateSelection->GetFrameSelection(getter_AddRefs(frameSelection));
  NS_ENSURE_TRUE(frameSelection, -1);

  nsCOMPtr<nsIDOMNode> caretNode;
  domSel->GetFocusNode(getter_AddRefs(caretNode));
  nsCOMPtr<nsIContent> caretContent = do_QueryInterface(caretNode);
  if (!caretContent || !nsCoreUtils::IsAncestorOf(mDOMNode, caretNode)) {
    return -1;
  }

  PRInt32 caretOffset, returnOffsetUnused;
  domSel->GetFocusOffset(&caretOffset);
  nsFrameSelection::HINT hint = frameSelection->GetHint();
  nsIFrame *caretFrame = frameSelection->GetFrameForNodeOffset(caretContent, caretOffset,
                                                               hint, &returnOffsetUnused);
  NS_ENSURE_TRUE(caretFrame, -1);

  PRInt32 lineNumber = 1;
  nsAutoLineIterator lineIterForCaret;
  nsCOMPtr<nsIContent> hyperTextContent = do_QueryInterface(mDOMNode);
  while (caretFrame) {
    if (hyperTextContent == caretFrame->GetContent()) {
      return lineNumber; // Must be in a single line hyper text, there is no line iterator
    }
    nsIFrame *parentFrame = caretFrame->GetParent();
    if (!parentFrame)
      break;

    // Add lines for the sibling frames before the caret
    nsIFrame *sibling = parentFrame->GetFirstChild(nsnull);
    while (sibling && sibling != caretFrame) {
      nsAutoLineIterator lineIterForSibling = sibling->GetLineIterator();
      if (lineIterForSibling) {
        // For the frames before that grab all the lines
        PRInt32 addLines = lineIterForSibling->GetNumLines();
        lineNumber += addLines;
      }
      sibling = sibling->GetNextSibling();
    }

    // Get the line number relative to the container with lines
    if (!lineIterForCaret) {   // Add the caret line just once
      lineIterForCaret = parentFrame->GetLineIterator();
      if (lineIterForCaret) {
        // Ancestor of caret
        PRInt32 addLines = lineIterForCaret->FindLineContaining(caretFrame);
        lineNumber += addLines;
      }
    }

    caretFrame = parentFrame;
  }

  NS_NOTREACHED("DOM ancestry had this hypertext but frame ancestry didn't");
  return lineNumber;
}

nsresult
nsHyperTextAccessible::GetSelections(PRInt16 aType,
                                     nsISelectionController **aSelCon,
                                     nsISelection **aDomSel,
                                     nsCOMArray<nsIDOMRange>* aRanges)
{
  if (!mDOMNode) {
    return NS_ERROR_FAILURE;
  }
  if (aSelCon) {
    *aSelCon = nsnull;
  }
  if (aDomSel) {
    *aDomSel = nsnull;
  }
  if (aRanges) {
    aRanges->Clear();
  }
  
  nsCOMPtr<nsISelection> domSel;
  nsCOMPtr<nsISelectionController> selCon;

  nsCOMPtr<nsIEditor> editor;
  GetAssociatedEditor(getter_AddRefs(editor));
  nsCOMPtr<nsIPlaintextEditor> peditor(do_QueryInterface(editor));
  if (peditor) {
    // Case 1: plain text editor
    // This is for form controls which have their own
    // selection controller separate from the document, for example
    // HTML:input, HTML:textarea, XUL:textbox, etc.
    editor->GetSelectionController(getter_AddRefs(selCon));
  }
  else {
    // Case 2: rich content subtree (can be rich editor)
    // This uses the selection controller from the entire document
    nsIFrame *frame = GetFrame();
    NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

    // Get the selection and selection controller
    frame->GetSelectionController(GetPresContext(),
                                  getter_AddRefs(selCon));
  }
  NS_ENSURE_TRUE(selCon, NS_ERROR_FAILURE);

  selCon->GetSelection(aType, getter_AddRefs(domSel));
  NS_ENSURE_TRUE(domSel, NS_ERROR_FAILURE);

  if (aSelCon) {
    NS_ADDREF(*aSelCon = selCon);
  }
  if (aDomSel) {
    NS_ADDREF(*aDomSel = domSel);
  }

  if (aRanges) {
    nsCOMPtr<nsISelection2> selection2(do_QueryInterface(domSel));
    NS_ENSURE_TRUE(selection2, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDOMNode> startNode(mDOMNode);
    if (peditor) {
      nsCOMPtr<nsIDOMElement> editorRoot;
      editor->GetRootElement(getter_AddRefs(editorRoot));
      startNode = do_QueryInterface(editorRoot);
    }
    NS_ENSURE_STATE(startNode);

    nsCOMPtr<nsIDOMNodeList> childNodes;
    nsresult rv = startNode->GetChildNodes(getter_AddRefs(childNodes));
    NS_ENSURE_SUCCESS(rv, rv);
    PRUint32 numChildren; 
    rv = childNodes->GetLength(&numChildren);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = selection2->GetRangesForIntervalCOMArray(startNode, 0,
                                                  startNode, numChildren,
                                                  PR_TRUE, aRanges);
    NS_ENSURE_SUCCESS(rv, rv);
    // Remove collapsed ranges
    PRInt32 numRanges = aRanges->Count();
    for (PRInt32 count = 0; count < numRanges; count ++) {
      PRBool isCollapsed;
      (*aRanges)[count]->GetCollapsed(&isCollapsed);
      if (isCollapsed) {
        aRanges->RemoveObjectAt(count);
        -- numRanges;
        -- count;
      }
    }
  }

  return NS_OK;
}

/*
 * Gets the number of selected regions.
 */
NS_IMETHODIMP nsHyperTextAccessible::GetSelectionCount(PRInt32 *aSelectionCount)
{
  nsCOMPtr<nsISelection> domSel;
  nsCOMArray<nsIDOMRange> ranges;
  nsresult rv = GetSelections(nsISelectionController::SELECTION_NORMAL,
                              nsnull, nsnull, &ranges);
  NS_ENSURE_SUCCESS(rv, rv);

  *aSelectionCount = ranges.Count();

  return NS_OK;
}

/*
 * Gets the start and end offset of the specified selection.
 */
NS_IMETHODIMP nsHyperTextAccessible::GetSelectionBounds(PRInt32 aSelectionNum, PRInt32 *aStartOffset, PRInt32 *aEndOffset)
{
  *aStartOffset = *aEndOffset = 0;

  nsCOMPtr<nsISelection> domSel;
  nsCOMArray<nsIDOMRange> ranges;
  nsresult rv = GetSelections(nsISelectionController::SELECTION_NORMAL,
                              nsnull, getter_AddRefs(domSel), &ranges);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 rangeCount = ranges.Count();
  if (aSelectionNum < 0 || aSelectionNum >= rangeCount)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIDOMRange> range = ranges[aSelectionNum];

  // Get start point
  nsCOMPtr<nsIDOMNode> startNode;
  range->GetStartContainer(getter_AddRefs(startNode));
  PRInt32 startOffset;
  range->GetStartOffset(&startOffset);

  // Get end point
  nsCOMPtr<nsIDOMNode> endNode;
  range->GetEndContainer(getter_AddRefs(endNode));
  PRInt32 endOffset;
  range->GetEndOffset(&endOffset);

  PRInt16 rangeCompareResult;
  rv = range->CompareBoundaryPoints(nsIDOMRange::START_TO_END, range, &rangeCompareResult);
  NS_ENSURE_SUCCESS(rv, rv);

  if (rangeCompareResult < 0) {
    // Make sure start is before end, by swapping offsets
    // This occurs when the user selects backwards in the text
    startNode.swap(endNode);
    PRInt32 tempOffset = startOffset;
    startOffset = endOffset;
    endOffset = tempOffset;
  }

  nsCOMPtr<nsIAccessible> startAccessible;
  rv = DOMPointToHypertextOffset(startNode, startOffset, aStartOffset, getter_AddRefs(startAccessible));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!startAccessible) {
    *aStartOffset = 0; // Could not find start point within this hypertext, so starts before
  }

  return DOMPointToHypertextOffset(endNode, endOffset, aEndOffset, nsnull, PR_TRUE);
}

/*
 * Changes the start and end offset of the specified selection.
 */
NS_IMETHODIMP
nsHyperTextAccessible::SetSelectionBounds(PRInt32 aSelectionNum,
                                          PRInt32 aStartOffset,
                                          PRInt32 aEndOffset)
{
  nsCOMPtr<nsISelection> domSel;
  nsresult rv = GetSelections(nsISelectionController::SELECTION_NORMAL,
                              nsnull, getter_AddRefs(domSel));
  NS_ENSURE_SUCCESS(rv, rv);

  // Caret is a collapsed selection
  PRBool isOnlyCaret = (aStartOffset == aEndOffset);

  PRInt32 rangeCount;
  domSel->GetRangeCount(&rangeCount);
  nsCOMPtr<nsIDOMRange> range;
  if (aSelectionNum == rangeCount) { // Add a range
    range = do_CreateInstance(kRangeCID);
    NS_ENSURE_TRUE(range, NS_ERROR_OUT_OF_MEMORY);
  }
  else if (aSelectionNum < 0 || aSelectionNum > rangeCount) {
    return NS_ERROR_INVALID_ARG;
  }
  else {
    domSel->GetRangeAt(aSelectionNum, getter_AddRefs(range));
    NS_ENSURE_TRUE(range, NS_ERROR_FAILURE);
  }

  PRInt32 startOffset, endOffset;
  nsCOMPtr<nsIDOMNode> startNode, endNode;

  rv = HypertextOffsetsToDOMRange(aStartOffset, aEndOffset,
                                  getter_AddRefs(startNode), &startOffset,
                                  getter_AddRefs(endNode), &endOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = range->SetStart(startNode, startOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = isOnlyCaret ? range->Collapse(PR_TRUE) :
                     range->SetEnd(endNode, endOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aSelectionNum == rangeCount) { // Add successfully created new range
    return domSel->AddRange(range);
  }
  return NS_OK;
}

/*
 * Adds a selection bounded by the specified offsets.
 */
NS_IMETHODIMP nsHyperTextAccessible::AddSelection(PRInt32 aStartOffset, PRInt32 aEndOffset)
{
  nsCOMPtr<nsISelection> domSel;
  nsresult rv = GetSelections(nsISelectionController::SELECTION_NORMAL,
                              nsnull, getter_AddRefs(domSel));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 rangeCount;
  domSel->GetRangeCount(&rangeCount);

  return SetSelectionBounds(rangeCount, aStartOffset, aEndOffset);
}

/*
 * Removes the specified selection.
 */
NS_IMETHODIMP nsHyperTextAccessible::RemoveSelection(PRInt32 aSelectionNum)
{
  nsCOMPtr<nsISelection> domSel;
  nsresult rv = GetSelections(nsISelectionController::SELECTION_NORMAL,
                              nsnull, getter_AddRefs(domSel));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 rangeCount;
  domSel->GetRangeCount(&rangeCount);
  if (aSelectionNum < 0 || aSelectionNum >= rangeCount)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIDOMRange> range;
  domSel->GetRangeAt(aSelectionNum, getter_AddRefs(range));
  return domSel->RemoveRange(range);
}

// void nsIAccessibleText::
//   scrollSubstringTo(in long startIndex, in long endIndex,
//                     in unsigned long scrollType);
NS_IMETHODIMP
nsHyperTextAccessible::ScrollSubstringTo(PRInt32 aStartIndex, PRInt32 aEndIndex,
                                         PRUint32 aScrollType)
{
  PRInt32 startOffset, endOffset;
  nsCOMPtr<nsIDOMNode> startNode, endNode;

  nsresult rv = HypertextOffsetsToDOMRange(aStartIndex, aEndIndex,
                                           getter_AddRefs(startNode),
                                           &startOffset,
                                           getter_AddRefs(endNode),
                                           &endOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  return nsCoreUtils::ScrollSubstringTo(GetFrame(), startNode, startOffset,
                                        endNode, endOffset, aScrollType);
}

// void nsIAccessibleText::
//   scrollSubstringToPoint(in long startIndex, in long endIndex,
//                          in unsigned long coordinateType,
//                          in long x, in long y);
NS_IMETHODIMP
nsHyperTextAccessible::ScrollSubstringToPoint(PRInt32 aStartIndex,
                                              PRInt32 aEndIndex,
                                              PRUint32 aCoordinateType,
                                              PRInt32 aX, PRInt32 aY)
{
  nsIFrame *frame = GetFrame();
  if (!frame)
    return NS_ERROR_FAILURE;

  nsIntPoint coords;
  nsresult rv = nsAccUtils::ConvertToScreenCoords(aX, aY, aCoordinateType,
                                                  this, &coords);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 startOffset, endOffset;
  nsCOMPtr<nsIDOMNode> startNode, endNode;

  rv = HypertextOffsetsToDOMRange(aStartIndex, aEndIndex,
                                  getter_AddRefs(startNode), &startOffset,
                                  getter_AddRefs(endNode), &endOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  nsPresContext *presContext = frame->PresContext();

  PRBool initialScrolled = PR_FALSE;
  nsIFrame *parentFrame = frame;
  while ((parentFrame = parentFrame->GetParent())) {
    nsIScrollableFrame *scrollableFrame = do_QueryFrame(parentFrame);
    if (scrollableFrame) {
      if (!initialScrolled) {
        // Scroll substring to the given point. Turn the point into percents
        // relative scrollable area to use nsCoreUtils::ScrollSubstringTo.
        nsIntRect frameRect = parentFrame->GetScreenRectExternal();
        PRInt32 devOffsetX = coords.x - frameRect.x;
        PRInt32 devOffsetY = coords.y - frameRect.y;

        nsPoint offsetPoint(presContext->DevPixelsToAppUnits(devOffsetX),
                            presContext->DevPixelsToAppUnits(devOffsetY));

        nsSize size(parentFrame->GetSize());
        PRInt16 hPercent = offsetPoint.x * 100 / size.width;
        PRInt16 vPercent = offsetPoint.y * 100 / size.height;

        rv = nsCoreUtils::ScrollSubstringTo(GetFrame(), startNode, startOffset,
                                            endNode, endOffset,
                                            vPercent, hPercent);
        NS_ENSURE_SUCCESS(rv, rv);

        initialScrolled = PR_TRUE;
      } else {
        // Substring was scrolled to the given point already inside its closest
        // scrollable area. If there are nested scrollable areas then make
        // sure we scroll lower areas to the given point inside currently
        // traversed scrollable area.
        nsCoreUtils::ScrollFrameToPoint(parentFrame, frame, coords);
      }
    }
    frame = parentFrame;
  }

  return NS_OK;
}

nsresult nsHyperTextAccessible::ContentToRenderedOffset(nsIFrame *aFrame, PRInt32 aContentOffset,
                                                        PRUint32 *aRenderedOffset)
{
  if (!aFrame) {
    // Current frame not rendered -- this can happen if text is set on
    // something with display: none
    *aRenderedOffset = 0;
    return NS_OK;
  }
  NS_ASSERTION(aFrame->GetType() == nsAccessibilityAtoms::textFrame,
               "Need text frame for offset conversion");
  NS_ASSERTION(aFrame->GetPrevContinuation() == nsnull,
               "Call on primary frame only");

  gfxSkipChars skipChars;
  gfxSkipCharsIterator iter;
  // Only get info up to original offset, we know that will be larger than skipped offset
  nsresult rv = aFrame->GetRenderedText(nsnull, &skipChars, &iter, 0, aContentOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 ourRenderedStart = iter.GetSkippedOffset();
  PRInt32 ourContentStart = iter.GetOriginalOffset();

  *aRenderedOffset = iter.ConvertOriginalToSkipped(aContentOffset + ourContentStart) -
                    ourRenderedStart;

  return NS_OK;
}

nsresult nsHyperTextAccessible::RenderedToContentOffset(nsIFrame *aFrame, PRUint32 aRenderedOffset,
                                                        PRInt32 *aContentOffset)
{
  *aContentOffset = 0;
  NS_ENSURE_TRUE(aFrame, NS_ERROR_FAILURE);

  NS_ASSERTION(aFrame->GetType() == nsAccessibilityAtoms::textFrame,
               "Need text frame for offset conversion");
  NS_ASSERTION(aFrame->GetPrevContinuation() == nsnull,
               "Call on primary frame only");

  gfxSkipChars skipChars;
  gfxSkipCharsIterator iter;
  // We only need info up to skipped offset -- that is what we're converting to original offset
  nsresult rv = aFrame->GetRenderedText(nsnull, &skipChars, &iter, 0, aRenderedOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 ourRenderedStart = iter.GetSkippedOffset();
  PRInt32 ourContentStart = iter.GetOriginalOffset();

  *aContentOffset = iter.ConvertSkippedToOriginal(aRenderedOffset + ourRenderedStart) - ourContentStart;

  return NS_OK;
}

nsresult
nsHyperTextAccessible::GetDOMPointByFrameOffset(nsIFrame *aFrame,
                                                PRInt32 aOffset,
                                                nsIAccessible *aAccessible,
                                                nsIDOMNode **aNode,
                                                PRInt32 *aNodeOffset)
{
  NS_ENSURE_ARG(aAccessible);

  nsCOMPtr<nsIDOMNode> node;

  if (!aFrame) {
    // If the given frame is null then set offset after the DOM node of the
    // given accessible.
    nsCOMPtr<nsIAccessNode> accessNode(do_QueryInterface(aAccessible));

    nsCOMPtr<nsIDOMNode> DOMNode;
    accessNode->GetDOMNode(getter_AddRefs(DOMNode));
    nsCOMPtr<nsIContent> content(do_QueryInterface(DOMNode));
    NS_ENSURE_STATE(content);

    nsCOMPtr<nsIContent> parent(content->GetParent());
    NS_ENSURE_STATE(parent);

    *aNodeOffset = parent->IndexOf(content) + 1;
    node = do_QueryInterface(parent);

  } else if (aFrame->GetType() == nsAccessibilityAtoms::textFrame) {
    nsCOMPtr<nsIContent> content(aFrame->GetContent());
    NS_ENSURE_STATE(content);

    nsCOMPtr<nsIPresShell> shell(GetPresShell());
    NS_ENSURE_STATE(shell);

    nsIFrame *primaryFrame = shell->GetPrimaryFrameFor(content);
    nsresult rv = RenderedToContentOffset(primaryFrame, aOffset, aNodeOffset);
    NS_ENSURE_SUCCESS(rv, rv);

    node = do_QueryInterface(content);

  } else {
    nsCOMPtr<nsIContent> content(aFrame->GetContent());
    NS_ENSURE_STATE(content);

    nsCOMPtr<nsIContent> parent(content->GetParent());
    NS_ENSURE_STATE(parent);

    *aNodeOffset = parent->IndexOf(content);
    node = do_QueryInterface(parent);
  }

  NS_IF_ADDREF(*aNode = node);
  return NS_OK;
}

// nsHyperTextAccessible
nsresult
nsHyperTextAccessible::DOMRangeBoundToHypertextOffset(nsIDOMRange *aRange,
                                                      PRBool aIsStartBound,
                                                      PRBool aIsStartHTOffset,
                                                      PRInt32 *aHTOffset)
{
  nsCOMPtr<nsIDOMNode> node;
  PRInt32 nodeOffset = 0;

  nsresult rv;
  if (aIsStartBound) {
    rv = aRange->GetStartContainer(getter_AddRefs(node));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aRange->GetStartOffset(&nodeOffset);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    rv = aRange->GetEndContainer(getter_AddRefs(node));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aRange->GetEndOffset(&nodeOffset);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIAccessible> startAcc;
  rv = DOMPointToHypertextOffset(node, nodeOffset, aHTOffset,
                                 getter_AddRefs(startAcc));
  NS_ENSURE_SUCCESS(rv, rv);

  if (aIsStartHTOffset && !startAcc)
    *aHTOffset = 0;

  return NS_OK;
}

// nsHyperTextAccessible
nsresult
nsHyperTextAccessible::GetSpellTextAttribute(nsIDOMNode *aNode,
                                             PRInt32 aNodeOffset,
                                             PRInt32 *aHTStartOffset,
                                             PRInt32 *aHTEndOffset,
                                             nsIPersistentProperties *aAttributes)
{
  nsCOMArray<nsIDOMRange> ranges;
  nsresult rv = GetSelections(nsISelectionController::SELECTION_SPELLCHECK,
                              nsnull, nsnull, &ranges);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 rangeCount = ranges.Count();
  if (!rangeCount)
    return NS_OK;

  for (PRInt32 index = 0; index < rangeCount; index++) {
    nsCOMPtr<nsIDOMRange> range = ranges[index];
    nsCOMPtr<nsIDOMNSRange> nsrange(do_QueryInterface(range));
    NS_ENSURE_STATE(nsrange);

    PRInt16 result;
    rv = nsrange->ComparePoint(aNode, aNodeOffset, &result);
    NS_ENSURE_SUCCESS(rv, rv);
    // ComparePoint checks boundary points, but we need to check that
    // text at aNodeOffset is inside the range.
    // See also bug 460690.
    if (result == 0) {
      nsCOMPtr<nsIDOMNode> end;
      rv = range->GetEndContainer(getter_AddRefs(end));
      NS_ENSURE_SUCCESS(rv, rv);
      PRInt32 endOffset;
      rv = range->GetEndOffset(&endOffset);
      NS_ENSURE_SUCCESS(rv, rv);
      if (aNode == end && aNodeOffset == endOffset) {
        result = 1;
      }
    }

    if (result == 1) { // range is before point
      PRInt32 startHTOffset = 0;
      rv = DOMRangeBoundToHypertextOffset(range, PR_FALSE, PR_TRUE,
                                          &startHTOffset);
      NS_ENSURE_SUCCESS(rv, rv);

      if (startHTOffset > *aHTStartOffset)
        *aHTStartOffset = startHTOffset;

    } else if (result == -1) { // range is after point
      PRInt32 endHTOffset = 0;
      rv = DOMRangeBoundToHypertextOffset(range, PR_TRUE, PR_FALSE,
                                          &endHTOffset);
      NS_ENSURE_SUCCESS(rv, rv);

      if (endHTOffset < *aHTEndOffset)
        *aHTEndOffset = endHTOffset;

    } else { // point is in range
      PRInt32 startHTOffset = 0;
      rv = DOMRangeBoundToHypertextOffset(range, PR_TRUE, PR_TRUE,
                                          &startHTOffset);
      NS_ENSURE_SUCCESS(rv, rv);

      PRInt32 endHTOffset = 0;
      rv = DOMRangeBoundToHypertextOffset(range, PR_FALSE, PR_FALSE,
                                          &endHTOffset);
      NS_ENSURE_SUCCESS(rv, rv);

      if (startHTOffset > *aHTStartOffset)
        *aHTStartOffset = startHTOffset;
      if (endHTOffset < *aHTEndOffset)
        *aHTEndOffset = endHTOffset;

      if (aAttributes) {
        nsAccUtils::SetAccAttr(aAttributes, nsAccessibilityAtoms::invalid,
                               NS_LITERAL_STRING("spelling"));
      }

      return NS_OK;
    }
  }

  return NS_OK;
}
