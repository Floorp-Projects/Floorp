/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HyperTextAccessible.h"

#include "Accessible-inl.h"
#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "DocAccessible.h"
#include "Role.h"
#include "States.h"
#include "TextAttrs.h"
#include "TreeWalker.h"

#include "nsIClipboard.h"
#include "nsContentUtils.h"
#include "nsFocusManager.h"
#include "nsIDOMRange.h"
#include "nsIEditingSession.h"
#include "nsIEditor.h"
#include "nsIFrame.h"
#include "nsFrameSelection.h"
#include "nsILineIterator.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPersistentProperties2.h"
#include "nsIPlaintextEditor.h"
#include "nsIScrollableFrame.h"
#include "nsIServiceManager.h"
#include "nsTextFragment.h"
#include "mozilla/Selection.h"
#include "mozilla/MathAlgorithms.h"
#include "gfxSkipChars.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// HyperTextAccessible
////////////////////////////////////////////////////////////////////////////////

HyperTextAccessible::
  HyperTextAccessible(nsIContent* aNode, DocAccessible* aDoc) :
  AccessibleWrap(aNode, aDoc)
{
  mGenericTypes |= eHyperText;
}

NS_IMPL_ADDREF_INHERITED(HyperTextAccessible, AccessibleWrap)
NS_IMPL_RELEASE_INHERITED(HyperTextAccessible, AccessibleWrap)

nsresult
HyperTextAccessible::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  *aInstancePtr = nullptr;

  // ARIA roles that these interfaces are not appropriate for.
  if (!IsTextRole())
    return Accessible::QueryInterface(aIID, aInstancePtr);

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

  return Accessible::QueryInterface(aIID, aInstancePtr);
}

role
HyperTextAccessible::NativeRole()
{
  nsIAtom *tag = mContent->Tag();

  if (tag == nsGkAtoms::dd)
    return roles::DEFINITION;

  if (tag == nsGkAtoms::form)
    return roles::FORM;

  if (tag == nsGkAtoms::blockquote || tag == nsGkAtoms::div ||
      tag == nsGkAtoms::section || tag == nsGkAtoms::nav)
    return roles::SECTION;

  if (tag == nsGkAtoms::h1 || tag == nsGkAtoms::h2 ||
      tag == nsGkAtoms::h3 || tag == nsGkAtoms::h4 ||
      tag == nsGkAtoms::h5 || tag == nsGkAtoms::h6)
    return roles::HEADING;

  if (tag == nsGkAtoms::article)
    return roles::DOCUMENT;
        
  // Deal with html landmark elements
  if (tag == nsGkAtoms::header)
    return roles::HEADER;

  if (tag == nsGkAtoms::footer)
    return roles::FOOTER;

  if (tag == nsGkAtoms::aside)
    return roles::NOTE;

  // Treat block frames as paragraphs
  nsIFrame *frame = GetFrame();
  if (frame && frame->GetType() == nsGkAtoms::blockFrame)
    return roles::PARAGRAPH;

  return roles::TEXT_CONTAINER; // In ATK this works
}

uint64_t
HyperTextAccessible::NativeState()
{
  uint64_t states = AccessibleWrap::NativeState();

  nsCOMPtr<nsIEditor> editor = GetEditor();
  if (editor) {
    states |= states::EDITABLE;

  } else if (mContent->Tag() == nsGkAtoms::article) {
    // We want <article> to behave like a document in terms of readonly state.
    states |= states::READONLY;
  }

  if (HasChildren())
    states |= states::SELECTABLE_TEXT;

  return states;
}

// Substring must be entirely within the same text node
nsIntRect
HyperTextAccessible::GetBoundsForString(nsIFrame* aFrame, uint32_t aStartRenderedOffset,
                                        uint32_t aEndRenderedOffset)
{
  nsPresContext* presContext = mDoc->PresContext();
  if (aFrame->GetType() != nsGkAtoms::textFrame) {
    // XXX fallback for non-text frames, happens for bullets right now
    // but in the future bullets will have proper text frames
    return aFrame->GetScreenRectInAppUnits().
      ToNearestPixels(presContext->AppUnitsPerDevPixel());
  }

  int32_t startContentOffset, endContentOffset;
  nsresult rv = RenderedToContentOffset(aFrame, aStartRenderedOffset, &startContentOffset);
  NS_ENSURE_SUCCESS(rv, nsIntRect());
  rv = RenderedToContentOffset(aFrame, aEndRenderedOffset, &endContentOffset);
  NS_ENSURE_SUCCESS(rv, nsIntRect());

  nsIFrame *frame;
  int32_t startContentOffsetInFrame;
  // Get the right frame continuation -- not really a child, but a sibling of
  // the primary frame passed in
  rv = aFrame->GetChildFrameContainingOffset(startContentOffset, false,
                                             &startContentOffsetInFrame, &frame);
  NS_ENSURE_SUCCESS(rv, nsIntRect());

  nsRect screenRect;
  while (frame && startContentOffset < endContentOffset) {
    // Start with this frame's screen rect, which we will 
    // shrink based on the substring we care about within it.
    // We will then add that frame to the total screenRect we
    // are returning.
    nsRect frameScreenRect = frame->GetScreenRectInAppUnits();

    // Get the length of the substring in this frame that we want the bounds for
    int32_t startFrameTextOffset, endFrameTextOffset;
    frame->GetOffsets(startFrameTextOffset, endFrameTextOffset);
    int32_t frameTotalTextLength = endFrameTextOffset - startFrameTextOffset;
    int32_t seekLength = endContentOffset - startContentOffset;
    int32_t frameSubStringLength = std::min(frameTotalTextLength - startContentOffsetInFrame, seekLength);

    // Add the point where the string starts to the frameScreenRect
    nsPoint frameTextStartPoint;
    rv = frame->GetPointFromOffset(startContentOffset, &frameTextStartPoint);
    NS_ENSURE_SUCCESS(rv, nsIntRect());

    // Use the point for the end offset to calculate the width
    nsPoint frameTextEndPoint;
    rv = frame->GetPointFromOffset(startContentOffset + frameSubStringLength, &frameTextEndPoint);
    NS_ENSURE_SUCCESS(rv, nsIntRect());

    frameScreenRect.x += std::min(frameTextStartPoint.x, frameTextEndPoint.x);
    frameScreenRect.width = mozilla::Abs(frameTextStartPoint.x - frameTextEndPoint.x);

    screenRect.UnionRect(frameScreenRect, screenRect);

    // Get ready to loop back for next frame continuation
    startContentOffset += frameSubStringLength;
    startContentOffsetInFrame = 0;
    frame = frame->GetNextContinuation();
  }

  return screenRect.ToNearestPixels(presContext->AppUnitsPerDevPixel());
}

/*
 * Gets the specified text.
 */
nsIFrame*
HyperTextAccessible::GetPosAndText(int32_t& aStartOffset, int32_t& aEndOffset,
                                   nsAString* aText, nsIFrame** aEndFrame,
                                   nsIntRect* aBoundsRect,
                                   Accessible** aStartAcc,
                                   Accessible** aEndAcc)
{
  aStartOffset = ConvertMagicOffset(aStartOffset);
  aEndOffset = ConvertMagicOffset(aEndOffset);

  int32_t startOffset = aStartOffset;
  int32_t endOffset = aEndOffset;
  // XXX this prevents text interface usage on <input type="password">
  bool isPassword = (Role() == roles::PASSWORD_TEXT);

  // Clear out parameters and set up loop
  if (aText) {
    aText->Truncate();
  }
  if (endOffset < 0) {
    const int32_t kMaxTextLength = 32767;
    endOffset = kMaxTextLength; // Max end offset
  }
  else if (startOffset > endOffset) {
    return nullptr;
  }

  nsIFrame *startFrame = nullptr;
 nsIFrame* endFrame = nullptr;
  if (aEndFrame) {
    *aEndFrame = nullptr;
  }
  if (aBoundsRect) {
    aBoundsRect->SetEmpty();
  }
  if (aStartAcc)
    *aStartAcc = nullptr;
  if (aEndAcc)
    *aEndAcc = nullptr;

  nsIntRect unionRect;
  Accessible* lastAccessible = nullptr;

  gfxSkipChars skipChars;
  gfxSkipCharsIterator iter;

  // Loop through children and collect valid offsets, text and bounds
  // depending on what we need for out parameters.
  uint32_t childCount = ChildCount();
  for (uint32_t childIdx = 0; childIdx < childCount; childIdx++) {
    Accessible* childAcc = mChildren[childIdx];
    lastAccessible = childAcc;

    nsIFrame *frame = childAcc->GetFrame();
    if (!frame) {
      continue;
    }
    nsIFrame *primaryFrame = frame;
    endFrame = frame;
    if (!nsAccUtils::IsEmbeddedObject(childAcc)) {
      // We only need info up to rendered offset -- that is what we're
      // converting to content offset
      int32_t substringEndOffset = -1;
      uint32_t ourRenderedStart = 0;
      int32_t ourContentStart = 0;
      if (frame->GetType() == nsGkAtoms::textFrame) {
        nsresult rv = frame->GetRenderedText(nullptr, &skipChars, &iter);
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
        substringEndOffset = nsAccUtils::TextLength(childAcc);
      }
      if (startOffset < substringEndOffset ||
          (startOffset == substringEndOffset && (childIdx == childCount - 1))) {
        // Our start is within this substring
        if (startOffset > 0 || endOffset < substringEndOffset) {
          // We don't want the whole string for this accessible
          // Get out the continuing text frame with this offset
          int32_t outStartLineUnused;
          int32_t contentOffset;
          if (frame->GetType() == nsGkAtoms::textFrame) {
            contentOffset = iter.ConvertSkippedToOriginal(startOffset) +
                            ourRenderedStart - ourContentStart;
          }
          else {
            contentOffset = startOffset;
          }
          frame->GetChildFrameContainingOffset(contentOffset, true,
                                               &outStartLineUnused, &frame);
          if (aEndFrame) {
            *aEndFrame = frame; // We ended in the current frame
            if (aEndAcc)
              NS_ADDREF(*aEndAcc = childAcc);
          }
          if (substringEndOffset > endOffset) {
            // Need to stop before the end of the available text
            substringEndOffset = endOffset;
          }
          aEndOffset = endOffset;
        }
        if (aText) {
          if (isPassword) {
            for (int32_t count = startOffset; count < substringEndOffset; count ++)
              *aText += '*'; // Show *'s only for password text
          }
          else {
            childAcc->AppendTextTo(*aText, startOffset,
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
            NS_ADDREF(*aStartAcc = childAcc);
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
            // XXX: should use nsIAccessible::AppendTextTo.
            if (frame->GetType() == nsGkAtoms::brFrame) {
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
            nsIntRect frameScreenRect = frame->GetScreenRectInAppUnits().
              ToNearestPixels(frame->PresContext()->AppUnitsPerDevPixel());
            aBoundsRect->UnionRect(*aBoundsRect, frameScreenRect);
          }
        }
        if (!startFrame) {
          startFrame = frame;
          aStartOffset = 0;
          if (aStartAcc)
            NS_ADDREF(*aStartAcc = childAcc);
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
    *aEndFrame = endFrame;
    if (aEndAcc && !*aEndAcc)
      NS_IF_ADDREF(*aEndAcc = lastAccessible);
  }

  return startFrame;
}

NS_IMETHODIMP
HyperTextAccessible::GetText(int32_t aStartOffset, int32_t aEndOffset,
                             nsAString& aText)
{
  aText.Truncate();

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  int32_t startOffset = ConvertMagicOffset(aStartOffset);
  int32_t endOffset = ConvertMagicOffset(aEndOffset);

  int32_t startChildIdx = GetChildIndexAtOffset(startOffset);
  if (startChildIdx == -1) {
    // 0 offsets are considered valid for empty text.
    return (startOffset == 0 && endOffset == 0) ? NS_OK : NS_ERROR_INVALID_ARG;
  }

  int32_t endChildIdx = GetChildIndexAtOffset(endOffset);
  if (endChildIdx == -1)
    return NS_ERROR_INVALID_ARG;

  if (startChildIdx == endChildIdx) {
    int32_t childOffset =  GetChildOffset(startChildIdx);
    NS_ENSURE_STATE(childOffset != -1);

    Accessible* child = GetChildAt(startChildIdx);
    child->AppendTextTo(aText, startOffset - childOffset,
                        endOffset - startOffset);

    return NS_OK;
  }

  int32_t startChildOffset =  GetChildOffset(startChildIdx);
  NS_ENSURE_STATE(startChildOffset != -1);

  Accessible* startChild = GetChildAt(startChildIdx);
  startChild->AppendTextTo(aText, startOffset - startChildOffset);

  for (int32_t childIdx = startChildIdx + 1; childIdx < endChildIdx; childIdx++) {
    Accessible* child = GetChildAt(childIdx);
    child->AppendTextTo(aText);
  }

  int32_t endChildOffset =  GetChildOffset(endChildIdx);
  NS_ENSURE_STATE(endChildOffset != -1);

  Accessible* endChild = GetChildAt(endChildIdx);
  endChild->AppendTextTo(aText, 0, endOffset - endChildOffset);

  return NS_OK;
}

/*
 * Gets the character count.
 */
NS_IMETHODIMP
HyperTextAccessible::GetCharacterCount(int32_t* aCharacterCount)
{
  NS_ENSURE_ARG_POINTER(aCharacterCount);
  *aCharacterCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aCharacterCount = CharacterCount();
  return NS_OK;
}

/*
 * Gets the specified character.
 */
NS_IMETHODIMP
HyperTextAccessible::GetCharacterAtOffset(int32_t aOffset, PRUnichar* aCharacter)
{
  NS_ENSURE_ARG_POINTER(aCharacter);
  *aCharacter = L'\0';

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsAutoString character;
  if (GetCharAt(aOffset, eGetAt, character)) {
    *aCharacter = character.First();
    return NS_OK;
  }

  return NS_ERROR_INVALID_ARG;
}

Accessible*
HyperTextAccessible::DOMPointToHypertextOffset(nsINode* aNode,
                                               int32_t aNodeOffset,
                                               int32_t* aHyperTextOffset,
                                               bool aIsEndOffset)
{
  if (!aHyperTextOffset)
    return nullptr;
  *aHyperTextOffset = 0;

  if (!aNode)
    return nullptr;

  uint32_t addTextOffset = 0;
  nsINode* findNode = nullptr;

  if (aNodeOffset == -1) {
    findNode = aNode;

  } else if (aNode->IsNodeOfType(nsINode::eTEXT)) {
    // For text nodes, aNodeOffset comes in as a character offset
    // Text offset will be added at the end, if we find the offset in this hypertext
    // We want the "skipped" offset into the text (rendered text without the extra whitespace)
    nsIFrame *frame = aNode->AsContent()->GetPrimaryFrame();
    NS_ENSURE_TRUE(frame, nullptr);
    nsresult rv = ContentToRenderedOffset(frame, aNodeOffset, &addTextOffset);
    NS_ENSURE_SUCCESS(rv, nullptr);
    // Get the child node and 
    findNode = aNode;

  } else {
    // findNode could be null if aNodeOffset == # of child nodes, which means
    // one of two things:
    // 1) there are no children, and the passed-in node is not mContent -- use
    //    parentContent for the node to find
    // 2) there are no children and the passed-in node is mContent, which means
    //    we're an empty nsIAccessibleText
    // 3) there are children and we're at the end of the children

    findNode = aNode->GetChildAt(aNodeOffset);
    if (!findNode) {
      if (aNodeOffset == 0) {
        if (aNode == GetNode()) {
          // Case #1: this accessible has no children and thus has empty text,
          // we can only be at hypertext offset 0.
          *aHyperTextOffset = 0;
          return nullptr;
        }

        // Case #2: there are no children, we're at this node.
        findNode = aNode;
      } else if (aNodeOffset == aNode->GetChildCount()) {
        // Case #3: we're after the last child, get next node to this one.
        for (nsINode* tmpNode = aNode;
             !findNode && tmpNode && tmpNode != mContent;
             tmpNode = tmpNode->GetParent()) {
          findNode = tmpNode->GetNextSibling();
        }
      }
    }
  }

  // Get accessible for this findNode, or if that node isn't accessible, use the
  // accessible for the next DOM node which has one (based on forward depth first search)
  Accessible* descendantAcc = nullptr;
  if (findNode) {
    nsCOMPtr<nsIContent> findContent(do_QueryInterface(findNode));
    if (findContent && findContent->IsHTML() &&
        findContent->NodeInfo()->Equals(nsGkAtoms::br) &&
        findContent->AttrValueIs(kNameSpaceID_None,
                                 nsGkAtoms::mozeditorbogusnode,
                                 nsGkAtoms::_true,
                                 eIgnoreCase)) {
      // This <br> is the hacky "bogus node" used when there is no text in a control
      *aHyperTextOffset = 0;
      return nullptr;
    }
    descendantAcc = GetFirstAvailableAccessible(findNode);
  }

  // From the descendant, go up and get the immediate child of this hypertext
  Accessible* childAccAtOffset = nullptr;
  while (descendantAcc) {
    Accessible* parentAcc = descendantAcc->Parent();
    if (parentAcc == this) {
      childAccAtOffset = descendantAcc;
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
    } else
      addTextOffset = 0;

    descendantAcc = parentAcc;
  }

  // Loop through, adding offsets until we reach childAccessible
  // If childAccessible is null we will end up adding up the entire length of
  // the hypertext, which is good -- it just means our offset node
  // came after the last accessible child's node
  uint32_t childCount = ChildCount();

  uint32_t childIdx = 0;
  Accessible* childAcc = nullptr;
  for (; childIdx < childCount; childIdx++) {
    childAcc = mChildren[childIdx];
    if (childAcc == childAccAtOffset)
      break;

    *aHyperTextOffset += nsAccUtils::TextLength(childAcc);
  }

  if (childIdx < childCount) {
    *aHyperTextOffset += addTextOffset;
    NS_ASSERTION(childAcc == childAccAtOffset,
                 "These should be equal whenever we exit loop and childAcc != nullptr");

    if (childIdx < childCount - 1 ||
        addTextOffset < nsAccUtils::TextLength(childAccAtOffset)) {
      // If not at end of last text node, we will return the accessible we were in
      return childAccAtOffset;
    }
  }

  return nullptr;
}

nsresult
HyperTextAccessible::HypertextOffsetsToDOMRange(int32_t aStartHTOffset,
                                                int32_t aEndHTOffset,
                                                nsRange* aRange)
{
  // If the given offsets are 0 and associated editor is empty then return
  // collapsed range with editor root element as range container.
  if (aStartHTOffset == 0 && aEndHTOffset == 0) {
    nsCOMPtr<nsIEditor> editor = GetEditor();
    if (editor) {
      bool isEmpty = false;
      editor->GetDocumentIsEmpty(&isEmpty);
      if (isEmpty) {
        nsCOMPtr<nsIDOMElement> editorRootElm;
        editor->GetRootElement(getter_AddRefs(editorRootElm));

        nsCOMPtr<nsINode> editorRoot(do_QueryInterface(editorRootElm));
        if (editorRoot) {
          aRange->SetStart(editorRoot, 0);
          aRange->SetEnd(editorRoot, 0);

          return NS_OK;
        }
      }
    }
  }

  nsRefPtr<Accessible> startAcc, endAcc;
  int32_t startOffset = aStartHTOffset, endOffset = aEndHTOffset;
  nsIFrame *startFrame = nullptr, *endFrame = nullptr;

  startFrame = GetPosAndText(startOffset, endOffset, nullptr, &endFrame, nullptr,
                             getter_AddRefs(startAcc), getter_AddRefs(endAcc));
  if (!startAcc || !endAcc)
    return NS_ERROR_FAILURE;

  DOMPoint startPoint, endPoint;
  nsresult rv = GetDOMPointByFrameOffset(startFrame, startOffset, startAcc,
                                         &startPoint);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aRange->SetStart(startPoint.node, startPoint.idx);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aStartHTOffset == aEndHTOffset)
    return aRange->SetEnd(startPoint.node, startPoint.idx);

  rv = GetDOMPointByFrameOffset(endFrame, endOffset, endAcc, &endPoint);
  NS_ENSURE_SUCCESS(rv, rv);

  return aRange->SetEnd(endPoint.node, endPoint.idx);
}

int32_t
HyperTextAccessible::GetRelativeOffset(nsIPresShell* aPresShell,
                                       nsIFrame* aFromFrame,
                                       int32_t aFromOffset,
                                       Accessible* aFromAccessible,
                                       nsSelectionAmount aAmount,
                                       nsDirection aDirection,
                                       bool aNeedsStart,
                                       EWordMovementType aWordMovementType)
{
  const bool kIsJumpLinesOk = true;          // okay to jump lines
  const bool kIsScrollViewAStop = false;     // do not stop at scroll views
  const bool kIsKeyboardSelect = true;       // is keyboard selection
  const bool kIsVisualBidi = false;          // use visual order for bidi text

  // Ask layout for the new node and offset, after moving the appropriate amount

  nsresult rv;
  int32_t contentOffset = aFromOffset;
  nsIFrame *frame = aFromAccessible->GetFrame();
  NS_ENSURE_TRUE(frame, -1);

  if (frame->GetType() == nsGkAtoms::textFrame) {
    rv = RenderedToContentOffset(frame, aFromOffset, &contentOffset);
    NS_ENSURE_SUCCESS(rv, -1);
  }

  nsPeekOffsetStruct pos(aAmount, aDirection, contentOffset,
                         0, kIsJumpLinesOk, kIsScrollViewAStop, kIsKeyboardSelect, kIsVisualBidi,
                         aWordMovementType);
  rv = aFromFrame->PeekOffset(&pos);

  // PeekOffset fails on last/first lines of the text in certain cases.
  if (NS_FAILED(rv) && aAmount == eSelectLine) {
    pos.mAmount = (aDirection == eDirNext) ? eSelectEndLine : eSelectBeginLine;
    aFromFrame->PeekOffset(&pos);
  }
  if (!pos.mResultContent)
    return -1;

  // Turn the resulting node and offset into a hyperTextOffset
  // If finalAccessible is nullptr, then DOMPointToHypertextOffset() searched
  // through the hypertext children without finding the node/offset position.
  int32_t hyperTextOffset;
  Accessible* finalAccessible =
    DOMPointToHypertextOffset(pos.mResultContent, pos.mContentOffset,
                              &hyperTextOffset, aDirection == eDirNext);

  if (!finalAccessible && aDirection == eDirPrevious) {
    // If we reached the end during search, this means we didn't find the DOM point
    // and we're actually at the start of the paragraph
    hyperTextOffset = 0;
  }  
  else if (aAmount == eSelectBeginLine) {
    Accessible* firstChild = mChildren.SafeElementAt(0, nullptr);
    // For line selection with needsStart, set start of line exactly to line break
    if (pos.mContentOffset == 0 && firstChild &&
        firstChild->Role() == roles::STATICTEXT &&
        static_cast<int32_t>(nsAccUtils::TextLength(firstChild)) == hyperTextOffset) {
      // XXX Bullet hack -- we should remove this once list bullets use anonymous content
      hyperTextOffset = 0;
    }
    if (!aNeedsStart && hyperTextOffset > 0) {
      -- hyperTextOffset;
    }
  }

  return hyperTextOffset;
}

int32_t
HyperTextAccessible::FindOffset(int32_t aOffset, nsDirection aDirection,
                                nsSelectionAmount aAmount,
                                EWordMovementType aWordMovementType)
{
  // Convert hypertext offset to frame-relative offset.
  int32_t offsetInFrame = aOffset, notUsedOffset = aOffset;
  nsRefPtr<Accessible> accAtOffset;
  nsIFrame* frameAtOffset =
    GetPosAndText(offsetInFrame, notUsedOffset, nullptr, nullptr,
                  nullptr, getter_AddRefs(accAtOffset));
  if (!frameAtOffset) {
    if (aOffset == CharacterCount()) {
      // Asking for start of line, while on last character.
      if (accAtOffset)
        frameAtOffset = accAtOffset->GetFrame();
    }
    NS_ASSERTION(frameAtOffset, "No start frame for text getting!");
    if (!frameAtOffset)
      return -1;

    // We're on the last continuation since we're on the last character.
    frameAtOffset = frameAtOffset->LastContinuation();
  }

  // Return hypertext offset of the boundary of the found word.
  return GetRelativeOffset(mDoc->PresShell(), frameAtOffset, offsetInFrame,
                           accAtOffset, aAmount, aDirection,
                           (aWordMovementType == eStartWord || aAmount == eSelectBeginLine),
                           aWordMovementType);
}

int32_t
HyperTextAccessible::FindLineBoundary(int32_t aOffset,
                                      EWhichLineBoundary aWhichLineBoundary)
{
  // Note: empty last line doesn't have own frame (a previous line contains '\n'
  // character instead) thus when it makes a difference we need to process this
  // case separately (otherwise operations are performed on previous line).
  switch (aWhichLineBoundary) {
    case ePrevLineBegin: {
      // Fetch a previous line and move to its start (as arrow up and home keys
      // were pressed).
      if (IsEmptyLastLineOffset(aOffset))
        return FindOffset(aOffset, eDirPrevious, eSelectBeginLine);

      int32_t tmpOffset = FindOffset(aOffset, eDirPrevious, eSelectLine);
      return FindOffset(tmpOffset, eDirPrevious, eSelectBeginLine);
    }

    case ePrevLineEnd: {
      if (IsEmptyLastLineOffset(aOffset))
        return aOffset - 1;

      // If offset is at first line then return 0 (first line start).
      int32_t tmpOffset = FindOffset(aOffset, eDirPrevious, eSelectBeginLine);
      if (tmpOffset == 0)
        return 0;

      // Otherwise move to end of previous line (as arrow up and end keys were
      // pressed).
      tmpOffset = FindOffset(aOffset, eDirPrevious, eSelectLine);
      return FindOffset(tmpOffset, eDirNext, eSelectEndLine);
    }

    case eThisLineBegin:
      if (IsEmptyLastLineOffset(aOffset))
        return aOffset;

      // Move to begin of the current line (as home key was pressed).
      return FindOffset(aOffset, eDirPrevious, eSelectBeginLine);

    case eThisLineEnd:
      if (IsEmptyLastLineOffset(aOffset))
        return aOffset;

      // Move to end of the current line (as end key was pressed).
      return FindOffset(aOffset, eDirNext, eSelectEndLine);

    case eNextLineBegin: {
      if (IsEmptyLastLineOffset(aOffset))
        return aOffset;

      // Move to begin of the next line if any (arrow down and home keys),
      // otherwise end of the current line (arrow down only).
      int32_t tmpOffset = FindOffset(aOffset, eDirNext, eSelectLine);
      if (tmpOffset == CharacterCount())
        return tmpOffset;

      return FindOffset(tmpOffset, eDirPrevious, eSelectBeginLine);
    }

    case eNextLineEnd: {
      if (IsEmptyLastLineOffset(aOffset))
        return aOffset;

      // Move to next line end (as down arrow and end key were pressed).
      int32_t tmpOffset = FindOffset(aOffset, eDirNext, eSelectLine);
      if (tmpOffset != CharacterCount())
        return FindOffset(tmpOffset, eDirNext, eSelectEndLine);
      return tmpOffset;
    }
  }

  return -1;
}

/**
  * nsIAccessibleText impl.
  */
NS_IMETHODIMP
HyperTextAccessible::GetTextBeforeOffset(int32_t aOffset,
                                         AccessibleTextBoundary aBoundaryType,
                                         int32_t* aStartOffset,
                                         int32_t* aEndOffset, nsAString& aText)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  if (aBoundaryType == BOUNDARY_CHAR) {
    GetCharAt(aOffset, eGetBefore, aText, aStartOffset, aEndOffset);
    return NS_OK;
  }

  int32_t adjustedOffset = ConvertMagicOffset(aOffset);
  if (adjustedOffset < 0)
    return NS_ERROR_INVALID_ARG;

  if (aOffset == nsIAccessibleText::TEXT_OFFSET_CARET)
    adjustedOffset = AdjustCaretOffset(adjustedOffset);

  switch (aBoundaryType) {
    case BOUNDARY_CHAR:
      MOZ_ASSUME_UNREACHABLE("Already handled!");
      return NS_ERROR_FAILURE;

    case BOUNDARY_WORD_START: {
      // If the offset is a word start (except text length offset) then move
      // backward to find a start offset (end offset is the given offset).
      // Otherwise move backward twice to find both start and end offsets.
      if (adjustedOffset == CharacterCount()) {
        *aEndOffset = FindWordBoundary(adjustedOffset, eDirPrevious, eStartWord);
        *aStartOffset = FindWordBoundary(*aEndOffset, eDirPrevious, eStartWord);
      } else {
        *aStartOffset = FindWordBoundary(adjustedOffset, eDirPrevious, eStartWord);
        *aEndOffset = FindWordBoundary(*aStartOffset, eDirNext, eStartWord);
        if (*aEndOffset != adjustedOffset) {
          *aEndOffset = *aStartOffset;
          *aStartOffset = FindWordBoundary(*aEndOffset, eDirPrevious, eStartWord);
        }
      }
      return GetText(*aStartOffset, *aEndOffset, aText);
    }

    case BOUNDARY_WORD_END: {
      // Move word backward twice to find start and end offsets.
      *aEndOffset = FindWordBoundary(adjustedOffset, eDirPrevious, eEndWord);
      *aStartOffset = FindWordBoundary(*aEndOffset, eDirPrevious, eEndWord);
      return GetText(*aStartOffset, *aEndOffset, aText);
    }

    case BOUNDARY_LINE_START:
      *aStartOffset = FindLineBoundary(adjustedOffset, ePrevLineBegin);
      *aEndOffset = FindLineBoundary(adjustedOffset, eThisLineBegin);
      return GetText(*aStartOffset, *aEndOffset, aText);

    case BOUNDARY_LINE_END: {
      *aEndOffset = FindLineBoundary(adjustedOffset, ePrevLineEnd);
      int32_t tmpOffset = *aEndOffset;
      // Adjust offset if line is wrapped.
      if (*aEndOffset != 0 && !IsLineEndCharAt(*aEndOffset))
        tmpOffset--;

      *aStartOffset = FindLineBoundary(tmpOffset, ePrevLineEnd);
      return GetText(*aStartOffset, *aEndOffset, aText);
    }

    default:
      return NS_ERROR_INVALID_ARG;
  }
}

NS_IMETHODIMP
HyperTextAccessible::GetTextAtOffset(int32_t aOffset,
                                     AccessibleTextBoundary aBoundaryType,
                                     int32_t* aStartOffset,
                                     int32_t* aEndOffset, nsAString& aText)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  int32_t adjustedOffset = ConvertMagicOffset(aOffset);
  if (adjustedOffset < 0)
    return NS_ERROR_INVALID_ARG;

  switch (aBoundaryType) {
    case BOUNDARY_CHAR:
      return GetCharAt(aOffset, eGetAt, aText, aStartOffset, aEndOffset) ?
        NS_OK : NS_ERROR_INVALID_ARG;

    case BOUNDARY_WORD_START:
      if (aOffset == nsIAccessibleText::TEXT_OFFSET_CARET)
        adjustedOffset = AdjustCaretOffset(adjustedOffset);

      *aEndOffset = FindWordBoundary(adjustedOffset, eDirNext, eStartWord);
      *aStartOffset = FindWordBoundary(*aEndOffset, eDirPrevious, eStartWord);
      return GetText(*aStartOffset, *aEndOffset, aText);

    case BOUNDARY_WORD_END:
      // Ignore the spec and follow what WebKitGtk does because Orca expects it,
      // i.e. return a next word at word end offset of the current word
      // (WebKitGtk behavior) instead the current word (AKT spec).
      *aEndOffset = FindWordBoundary(adjustedOffset, eDirNext, eEndWord);
      *aStartOffset = FindWordBoundary(*aEndOffset, eDirPrevious, eEndWord);
      return GetText(*aStartOffset, *aEndOffset, aText);

    case BOUNDARY_LINE_START:
      if (aOffset == nsIAccessibleText::TEXT_OFFSET_CARET)
        adjustedOffset = AdjustCaretOffset(adjustedOffset);

      *aStartOffset = FindLineBoundary(adjustedOffset, eThisLineBegin);
      *aEndOffset = FindLineBoundary(adjustedOffset, eNextLineBegin);
      return GetText(*aStartOffset, *aEndOffset, aText);

    case BOUNDARY_LINE_END:
      if (aOffset == nsIAccessibleText::TEXT_OFFSET_CARET)
        adjustedOffset = AdjustCaretOffset(adjustedOffset);

      // In contrast to word end boundary we follow the spec here.
      *aStartOffset = FindLineBoundary(adjustedOffset, ePrevLineEnd);
      *aEndOffset = FindLineBoundary(adjustedOffset, eThisLineEnd);
      return GetText(*aStartOffset, *aEndOffset, aText);

    default:
      return NS_ERROR_INVALID_ARG;
  }
}

NS_IMETHODIMP
HyperTextAccessible::GetTextAfterOffset(int32_t aOffset,
                                        AccessibleTextBoundary aBoundaryType,
                                        int32_t* aStartOffset,
                                        int32_t* aEndOffset, nsAString& aText)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  if (aBoundaryType == BOUNDARY_CHAR) {
    GetCharAt(aOffset, eGetAfter, aText, aStartOffset, aEndOffset);
    return NS_OK;
  }

  int32_t adjustedOffset = ConvertMagicOffset(aOffset);
  if (adjustedOffset < 0)
    return NS_ERROR_INVALID_ARG;

  if (aOffset == nsIAccessibleText::TEXT_OFFSET_CARET)
    adjustedOffset = AdjustCaretOffset(adjustedOffset);

  switch (aBoundaryType) {
    case BOUNDARY_CHAR:
      MOZ_ASSUME_UNREACHABLE("Already handled!");
      return NS_ERROR_FAILURE;

    case BOUNDARY_WORD_START:
      // Move word forward twice to find start and end offsets.
      *aStartOffset = FindWordBoundary(adjustedOffset, eDirNext, eStartWord);
      *aEndOffset = FindWordBoundary(*aStartOffset, eDirNext, eStartWord);
      return GetText(*aStartOffset, *aEndOffset, aText);

    case BOUNDARY_WORD_END:
      // If the offset is a word end (except 0 offset) then move forward to find
      // end offset (start offset is the given offset). Otherwise move forward
      // twice to find both start and end offsets.
      if (adjustedOffset == 0) {
        *aStartOffset = FindWordBoundary(adjustedOffset, eDirNext, eEndWord);
        *aEndOffset = FindWordBoundary(*aStartOffset, eDirNext, eEndWord);
      } else {
        *aEndOffset = FindWordBoundary(adjustedOffset, eDirNext, eEndWord);
        *aStartOffset = FindWordBoundary(*aEndOffset, eDirPrevious, eEndWord);
        if (*aStartOffset != adjustedOffset) {
          *aStartOffset = *aEndOffset;
          *aEndOffset = FindWordBoundary(*aStartOffset, eDirNext, eEndWord);
        }
      }
      return GetText(*aStartOffset, *aEndOffset, aText);

    case BOUNDARY_LINE_START:
      *aStartOffset = FindLineBoundary(adjustedOffset, eNextLineBegin);
      *aEndOffset = FindLineBoundary(*aStartOffset, eNextLineBegin);
      return GetText(*aStartOffset, *aEndOffset, aText);

    case BOUNDARY_LINE_END:
      *aStartOffset = FindLineBoundary(adjustedOffset, eThisLineEnd);
      *aEndOffset = FindLineBoundary(adjustedOffset, eNextLineEnd);
      return GetText(*aStartOffset, *aEndOffset, aText);

    default:
      return NS_ERROR_INVALID_ARG;
  }
}

// nsIPersistentProperties
// nsIAccessibleText::getTextAttributes(in boolean includeDefAttrs,
//                                      in long offset,
//                                      out long rangeStartOffset,
//                                      out long rangeEndOffset);
NS_IMETHODIMP
HyperTextAccessible::GetTextAttributes(bool aIncludeDefAttrs,
                                       int32_t aOffset,
                                       int32_t* aStartOffset,
                                       int32_t* aEndOffset,
                                       nsIPersistentProperties** aAttributes)
{
  // 1. Get each attribute and its ranges one after another.
  // 2. As we get each new attribute, we pass the current start and end offsets
  //    as in/out parameters. In other words, as attributes are collected,
  //    the attribute range itself can only stay the same or get smaller.

  NS_ENSURE_ARG_POINTER(aStartOffset);
  *aStartOffset = 0;

  NS_ENSURE_ARG_POINTER(aEndOffset);
  *aEndOffset = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  int32_t offset = ConvertMagicOffset(aOffset);

  if (aAttributes) {
    *aAttributes = nullptr;

    nsCOMPtr<nsIPersistentProperties> attributes =
      do_CreateInstance(NS_PERSISTENTPROPERTIES_CONTRACTID);
    NS_ENSURE_TRUE(attributes, NS_ERROR_OUT_OF_MEMORY);

    NS_ADDREF(*aAttributes = attributes);
  }

  Accessible* accAtOffset = GetChildAtOffset(offset);
  if (!accAtOffset) {
    // Offset 0 is correct offset when accessible has empty text. Include
    // default attributes if they were requested, otherwise return empty set.
    if (offset == 0) {
      if (aIncludeDefAttrs) {
        TextAttrsMgr textAttrsMgr(this);
        textAttrsMgr.GetAttributes(*aAttributes);
      }
      return NS_OK;
    }
    return NS_ERROR_INVALID_ARG;
  }

  int32_t accAtOffsetIdx = accAtOffset->IndexInParent();
  int32_t startOffset = GetChildOffset(accAtOffsetIdx);
  int32_t endOffset = GetChildOffset(accAtOffsetIdx + 1);
  int32_t offsetInAcc = offset - startOffset;

  TextAttrsMgr textAttrsMgr(this, aIncludeDefAttrs, accAtOffset,
                            accAtOffsetIdx);
  textAttrsMgr.GetAttributes(*aAttributes, &startOffset, &endOffset);

  // Compute spelling attributes on text accessible only.
  nsIFrame *offsetFrame = accAtOffset->GetFrame();
  if (offsetFrame && offsetFrame->GetType() == nsGkAtoms::textFrame) {
    int32_t nodeOffset = 0;
    nsresult rv = RenderedToContentOffset(offsetFrame, offsetInAcc,
                                          &nodeOffset);
    NS_ENSURE_SUCCESS(rv, rv);

    // Set 'misspelled' text attribute.
    rv = GetSpellTextAttribute(accAtOffset->GetNode(), nodeOffset,
                               &startOffset, &endOffset,
                               aAttributes ? *aAttributes : nullptr);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *aStartOffset = startOffset;
  *aEndOffset = endOffset;
  return NS_OK;
}

// nsIPersistentProperties
// nsIAccessibleText::defaultTextAttributes
NS_IMETHODIMP
HyperTextAccessible::GetDefaultTextAttributes(nsIPersistentProperties** aAttributes)
{
  NS_ENSURE_ARG_POINTER(aAttributes);
  *aAttributes = nullptr;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPersistentProperties> attributes =
    do_CreateInstance(NS_PERSISTENTPROPERTIES_CONTRACTID);
  NS_ENSURE_TRUE(attributes, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*aAttributes = attributes);

  TextAttrsMgr textAttrsMgr(this);
  textAttrsMgr.GetAttributes(*aAttributes);
  return NS_OK;
}

int32_t
HyperTextAccessible::GetLevelInternal()
{
  nsIAtom *tag = mContent->Tag();
  if (tag == nsGkAtoms::h1)
    return 1;
  if (tag == nsGkAtoms::h2)
    return 2;
  if (tag == nsGkAtoms::h3)
    return 3;
  if (tag == nsGkAtoms::h4)
    return 4;
  if (tag == nsGkAtoms::h5)
    return 5;
  if (tag == nsGkAtoms::h6)
    return 6;

  return AccessibleWrap::GetLevelInternal();
}

already_AddRefed<nsIPersistentProperties>
HyperTextAccessible::NativeAttributes()
{
  nsCOMPtr<nsIPersistentProperties> attributes =
    AccessibleWrap::NativeAttributes();

  // 'formatting' attribute is deprecated, 'display' attribute should be
  // instead.
  nsIFrame *frame = GetFrame();
  if (frame && frame->GetType() == nsGkAtoms::blockFrame) {
    nsAutoString unused;
    attributes->SetStringProperty(NS_LITERAL_CSTRING("formatting"),
                                  NS_LITERAL_STRING("block"), unused);
  }

  if (FocusMgr()->IsFocused(this)) {
    int32_t lineNumber = CaretLineNumber();
    if (lineNumber >= 1) {
      nsAutoString strLineNumber;
      strLineNumber.AppendInt(lineNumber);
      nsAccUtils::SetAccAttr(attributes, nsGkAtoms::lineNumber, strLineNumber);
    }
  }

  if (!HasOwnContent())
    return attributes.forget();

  // For the html landmark elements we expose them like we do aria landmarks to
  // make AT navigation schemes "just work".
  nsIAtom* tag = mContent->Tag();
  if (tag == nsGkAtoms::nav) {
    nsAccUtils::SetAccAttr(attributes, nsGkAtoms::xmlroles,
                           NS_LITERAL_STRING("navigation"));
  } else if (tag == nsGkAtoms::section)  {
    nsAccUtils::SetAccAttr(attributes, nsGkAtoms::xmlroles,
                           NS_LITERAL_STRING("region"));
  } else if (tag == nsGkAtoms::header || tag == nsGkAtoms::footer) {
    // Only map header and footer if they are not descendants
    // of an article or section tag.
    nsIContent* parent = mContent->GetParent();
    while (parent) {
      if (parent->Tag() == nsGkAtoms::article ||
          parent->Tag() == nsGkAtoms::section)
        break;
      parent = parent->GetParent();
    }

    // No article or section elements found.
    if (!parent) {
      if (tag == nsGkAtoms::header) {
        nsAccUtils::SetAccAttr(attributes, nsGkAtoms::xmlroles,
                               NS_LITERAL_STRING("banner"));
      } else if (tag == nsGkAtoms::footer) {
        nsAccUtils::SetAccAttr(attributes, nsGkAtoms::xmlroles,
                               NS_LITERAL_STRING("contentinfo"));
      }
    }
  } else if (tag == nsGkAtoms::aside) {
    nsAccUtils::SetAccAttr(attributes, nsGkAtoms::xmlroles,
                           NS_LITERAL_STRING("complementary"));
  } else if (tag == nsGkAtoms::article) {
    nsAccUtils::SetAccAttr(attributes, nsGkAtoms::xmlroles,
                           NS_LITERAL_STRING("article"));
  } else if (tag == nsGkAtoms::main) {
    nsAccUtils::SetAccAttr(attributes, nsGkAtoms::xmlroles,
                           NS_LITERAL_STRING("main"));
  }

  return attributes.forget();
}

/*
 * Given an offset, the x, y, width, and height values are filled appropriately.
 */
NS_IMETHODIMP
HyperTextAccessible::GetCharacterExtents(int32_t aOffset, int32_t* aX, int32_t* aY,
                                         int32_t* aWidth, int32_t* aHeight,
                                         uint32_t aCoordType)
{
  return GetRangeExtents(aOffset, aOffset + 1, aX, aY, aWidth, aHeight, aCoordType);
}

/*
 * Given a start & end offset, the x, y, width, and height values are filled appropriately.
 */
NS_IMETHODIMP
HyperTextAccessible::GetRangeExtents(int32_t aStartOffset, int32_t aEndOffset,
                                     int32_t* aX, int32_t* aY,
                                     int32_t* aWidth, int32_t* aHeight,
                                     uint32_t aCoordType)
{
  nsIntRect boundsRect;
  nsIFrame *endFrameUnused;
  if (!GetPosAndText(aStartOffset, aEndOffset, nullptr, &endFrameUnused, &boundsRect) ||
      boundsRect.IsEmpty()) {
    return NS_ERROR_FAILURE;
  }

  *aX = boundsRect.x;
  *aY = boundsRect.y;
  *aWidth = boundsRect.width;
  *aHeight = boundsRect.height;

  nsAccUtils::ConvertScreenCoordsTo(aX, aY, aCoordType, this);
  return NS_OK;
}

/*
 * Gets the offset of the character located at coordinates x and y. x and y are interpreted as being relative to
 * the screen or this widget's window depending on coords.
 */
NS_IMETHODIMP
HyperTextAccessible::GetOffsetAtPoint(int32_t aX, int32_t aY,
                                      uint32_t aCoordType, int32_t* aOffset)
{
  *aOffset = -1;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsIFrame *hyperFrame = GetFrame();
  if (!hyperFrame) {
    return NS_ERROR_FAILURE;
  }

  nsIntPoint coords = nsAccUtils::ConvertToScreenCoords(aX, aY, aCoordType,
                                                        this);

  nsPresContext* presContext = mDoc->PresContext();
  nsPoint coordsInAppUnits =
    coords.ToAppUnits(presContext->AppUnitsPerDevPixel());

  nsRect frameScreenRect = hyperFrame->GetScreenRectInAppUnits();
  if (!frameScreenRect.Contains(coordsInAppUnits.x, coordsInAppUnits.y))
    return NS_OK;   // Not found, will return -1

  nsPoint pointInHyperText(coordsInAppUnits.x - frameScreenRect.x,
                           coordsInAppUnits.y - frameScreenRect.y);

  // Go through the frames to check if each one has the point.
  // When one does, add up the character offsets until we have a match

  // We have an point in an accessible child of this, now we need to add up the
  // offsets before it to what we already have
  int32_t offset = 0;
  uint32_t childCount = ChildCount();
  for (uint32_t childIdx = 0; childIdx < childCount; childIdx++) {
    Accessible* childAcc = mChildren[childIdx];

    nsIFrame *primaryFrame = childAcc->GetFrame();
    NS_ENSURE_TRUE(primaryFrame, NS_ERROR_FAILURE);

    nsIFrame *frame = primaryFrame;
    while (frame) {
      nsIContent *content = frame->GetContent();
      NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);
      nsPoint pointInFrame = pointInHyperText - frame->GetOffsetTo(hyperFrame);
      nsSize frameSize = frame->GetSize();
      if (pointInFrame.x < frameSize.width && pointInFrame.y < frameSize.height) {
        // Finished
        if (frame->GetType() == nsGkAtoms::textFrame) {
          nsIFrame::ContentOffsets contentOffsets =
            frame->GetContentOffsetsFromPointExternal(pointInFrame, nsIFrame::IGNORE_SELECTION_STYLE);
          if (contentOffsets.IsNull() || contentOffsets.content != content) {
            return NS_OK; // Not found, will return -1
          }
          uint32_t addToOffset;
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

    offset += nsAccUtils::TextLength(childAcc);
  }

  return NS_OK; // Not found, will return -1
}


////////////////////////////////////////////////////////////////////////////////
// nsIAccessibleHyperText

NS_IMETHODIMP
HyperTextAccessible::GetLinkCount(int32_t* aLinkCount)
{
  NS_ENSURE_ARG_POINTER(aLinkCount);
  *aLinkCount = 0;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aLinkCount = GetLinkCount();
  return NS_OK;
}

NS_IMETHODIMP
HyperTextAccessible::GetLinkAt(int32_t aIndex, nsIAccessibleHyperLink** aLink)
{
  NS_ENSURE_ARG_POINTER(aLink);
  *aLink = nullptr;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  Accessible* link = GetLinkAt(aIndex);
  if (link)
    CallQueryInterface(link, aLink);

  return NS_OK;
}

NS_IMETHODIMP
HyperTextAccessible::GetLinkIndex(nsIAccessibleHyperLink* aLink,
                                  int32_t* aIndex)
{
  NS_ENSURE_ARG_POINTER(aLink);

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsRefPtr<Accessible> link(do_QueryObject(aLink));
  *aIndex = GetLinkIndex(link);
  return NS_OK;
}

NS_IMETHODIMP
HyperTextAccessible::GetLinkIndexAtOffset(int32_t aOffset, int32_t* aLinkIndex)
{
  NS_ENSURE_ARG_POINTER(aLinkIndex);
  *aLinkIndex = -1; // API says this magic value means 'not found'

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  *aLinkIndex = GetLinkIndexAtOffset(aOffset);
  return NS_OK;
}

/**
  * nsIAccessibleEditableText impl.
  */
NS_IMETHODIMP
HyperTextAccessible::SetAttributes(int32_t aStartPos, int32_t aEndPos,
                                   nsISupports* aAttributes)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
HyperTextAccessible::SetTextContents(const nsAString& aText)
{
  int32_t numChars = CharacterCount();
  if (numChars == 0 || NS_SUCCEEDED(DeleteText(0, numChars))) {
    return InsertText(aText, 0);
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
HyperTextAccessible::InsertText(const nsAString& aText, int32_t aPosition)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIEditor> editor = GetEditor();

  nsCOMPtr<nsIPlaintextEditor> peditor(do_QueryInterface(editor));
  NS_ENSURE_STATE(peditor);

  nsresult rv = SetSelectionRange(aPosition, aPosition);
  NS_ENSURE_SUCCESS(rv, rv);

  return peditor->InsertText(aText);
}

NS_IMETHODIMP
HyperTextAccessible::CopyText(int32_t aStartPos, int32_t aEndPos)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIEditor> editor = GetEditor();
  NS_ENSURE_STATE(editor);

  nsresult rv = SetSelectionRange(aStartPos, aEndPos);
  NS_ENSURE_SUCCESS(rv, rv);

  return editor->Copy();
}

NS_IMETHODIMP
HyperTextAccessible::CutText(int32_t aStartPos, int32_t aEndPos)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIEditor> editor = GetEditor();
  NS_ENSURE_STATE(editor);

  nsresult rv = SetSelectionRange(aStartPos, aEndPos);
  NS_ENSURE_SUCCESS(rv, rv);

  return editor->Cut();
}

NS_IMETHODIMP
HyperTextAccessible::DeleteText(int32_t aStartPos, int32_t aEndPos)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIEditor> editor = GetEditor();
  NS_ENSURE_STATE(editor);

  nsresult rv = SetSelectionRange(aStartPos, aEndPos);
  NS_ENSURE_SUCCESS(rv, rv);

  return editor->DeleteSelection(nsIEditor::eNone, nsIEditor::eStrip);
}

NS_IMETHODIMP
HyperTextAccessible::PasteText(int32_t aPosition)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIEditor> editor = GetEditor();
  NS_ENSURE_STATE(editor);

  nsresult rv = SetSelectionRange(aPosition, aPosition);
  NS_ENSURE_SUCCESS(rv, rv);

  return editor->Paste(nsIClipboard::kGlobalClipboard);
}

already_AddRefed<nsIEditor>
HyperTextAccessible::GetEditor() const
{
  if (!mContent->HasFlag(NODE_IS_EDITABLE)) {
    // If we're inside an editable container, then return that container's editor
    Accessible* ancestor = Parent();
    while (ancestor) {
      HyperTextAccessible* hyperText = ancestor->AsHyperText();
      if (hyperText) {
        // Recursion will stop at container doc because it has its own impl
        // of GetEditor()
        return hyperText->GetEditor();
      }

      ancestor = ancestor->Parent();
    }

    return nullptr;
  }

  nsCOMPtr<nsIDocShell> docShell = nsCoreUtils::GetDocShellFor(mContent);
  nsCOMPtr<nsIEditingSession> editingSession(do_GetInterface(docShell));
  if (!editingSession)
    return nullptr; // No editing session interface

  nsCOMPtr<nsIEditor> editor;
  nsIDocument* docNode = mDoc->DocumentNode();
  editingSession->GetEditorForWindow(docNode->GetWindow(),
                                     getter_AddRefs(editor));
  return editor.forget();
}

/**
  * =================== Caret & Selection ======================
  */

nsresult
HyperTextAccessible::SetSelectionRange(int32_t aStartPos, int32_t aEndPos)
{
  // Before setting the selection range, we need to ensure that the editor
  // is initialized. (See bug 804927.)
  // Otherwise, it's possible that lazy editor initialization will override
  // the selection we set here and leave the caret at the end of the text.
  // By calling GetEditor here, we ensure that editor initialization is
  // completed before we set the selection.
  nsCOMPtr<nsIEditor> editor = GetEditor();

  bool isFocusable = InteractiveState() & states::FOCUSABLE;

  // If accessible is focusable then focus it before setting the selection to
  // neglect control's selection changes on focus if any (for example, inputs
  // that do select all on focus).
  // some input controls
  if (isFocusable)
    TakeFocus();

  // Set the selection
  SetSelectionBounds(0, aStartPos, aEndPos);

  // If range 0 was successfully set, clear any additional selection 
  // ranges remaining from previous selection
  nsRefPtr<nsFrameSelection> frameSelection = FrameSelection();
  NS_ENSURE_STATE(frameSelection);

  Selection* domSel =
    frameSelection->GetSelection(nsISelectionController::SELECTION_NORMAL);
  NS_ENSURE_STATE(domSel);

  for (int32_t idx = domSel->GetRangeCount() - 1; idx > 0; idx--)
    domSel->RemoveRange(domSel->GetRangeAt(idx));

  // When selection is done, move the focus to the selection if accessible is
  // not focusable. That happens when selection is set within hypertext
  // accessible.
  if (isFocusable)
    return NS_OK;

  nsFocusManager* DOMFocusManager = nsFocusManager::GetFocusManager();
  if (DOMFocusManager) {
    NS_ENSURE_TRUE(mDoc, NS_ERROR_FAILURE);
    nsIDocument* docNode = mDoc->DocumentNode();
    NS_ENSURE_TRUE(docNode, NS_ERROR_FAILURE);
    nsCOMPtr<nsPIDOMWindow> window = docNode->GetWindow();
    nsCOMPtr<nsIDOMElement> result;
    DOMFocusManager->MoveFocus(window, nullptr, nsIFocusManager::MOVEFOCUS_CARET,
                               nsIFocusManager::FLAG_BYMOVEFOCUS, getter_AddRefs(result));
  }

  return NS_OK;
}

NS_IMETHODIMP
HyperTextAccessible::SetCaretOffset(int32_t aCaretOffset)
{
  return SetSelectionRange(aCaretOffset, aCaretOffset);
}

/*
 * Gets the offset position of the caret (cursor).
 */
NS_IMETHODIMP
HyperTextAccessible::GetCaretOffset(int32_t* aCaretOffset)
{
  NS_ENSURE_ARG_POINTER(aCaretOffset);
  *aCaretOffset = -1;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // Not focused focusable accessible except document accessible doesn't have
  // a caret.
  if (!IsDoc() && !FocusMgr()->IsFocused(this) &&
      (InteractiveState() & states::FOCUSABLE)) {
    return NS_OK;
  }

  // No caret if the focused node is not inside this DOM node and this DOM node
  // is not inside of focused node.
  FocusManager::FocusDisposition focusDisp =
    FocusMgr()->IsInOrContainsFocus(this);
  if (focusDisp == FocusManager::eNone)
    return NS_OK;

  // Turn the focus node and offset of the selection into caret hypretext
  // offset.
  nsRefPtr<nsFrameSelection> frameSelection = FrameSelection();
  NS_ENSURE_STATE(frameSelection);

  Selection* domSel =
    frameSelection->GetSelection(nsISelectionController::SELECTION_NORMAL);
  NS_ENSURE_STATE(domSel);

  nsINode* focusNode = domSel->GetFocusNode();
  int32_t focusOffset = domSel->GetFocusOffset();

  // No caret if this DOM node is inside of focused node but the selection's
  // focus point is not inside of this DOM node.
  if (focusDisp == FocusManager::eContainedByFocus) {
    nsINode *resultNode =
      nsCoreUtils::GetDOMNodeFromDOMPoint(focusNode, focusOffset);

    nsINode* thisNode = GetNode();
    if (resultNode != thisNode &&
        !nsCoreUtils::IsAncestorOf(thisNode, resultNode))
      return NS_OK;
  }

  DOMPointToHypertextOffset(focusNode, focusOffset, aCaretOffset);
  return NS_OK;
}

int32_t
HyperTextAccessible::CaretLineNumber()
{
  // Provide the line number for the caret, relative to the
  // currently focused node. Use a 1-based index
  nsRefPtr<nsFrameSelection> frameSelection = FrameSelection();
  if (!frameSelection)
    return -1;

  Selection* domSel =
    frameSelection->GetSelection(nsISelectionController::SELECTION_NORMAL);
  if (!domSel)
    return - 1;

  nsINode* caretNode = domSel->GetFocusNode();
  if (!caretNode || !caretNode->IsContent())
    return -1;

  nsIContent* caretContent = caretNode->AsContent();
  if (!nsCoreUtils::IsAncestorOf(GetNode(), caretContent))
    return -1;

  int32_t returnOffsetUnused;
  int32_t caretOffset = domSel->GetFocusOffset();
  nsFrameSelection::HINT hint = frameSelection->GetHint();
  nsIFrame *caretFrame = frameSelection->GetFrameForNodeOffset(caretContent, caretOffset,
                                                               hint, &returnOffsetUnused);
  NS_ENSURE_TRUE(caretFrame, -1);

  int32_t lineNumber = 1;
  nsAutoLineIterator lineIterForCaret;
  nsIContent *hyperTextContent = IsContent() ? mContent.get() : nullptr;
  while (caretFrame) {
    if (hyperTextContent == caretFrame->GetContent()) {
      return lineNumber; // Must be in a single line hyper text, there is no line iterator
    }
    nsIFrame *parentFrame = caretFrame->GetParent();
    if (!parentFrame)
      break;

    // Add lines for the sibling frames before the caret
    nsIFrame *sibling = parentFrame->GetFirstPrincipalChild();
    while (sibling && sibling != caretFrame) {
      nsAutoLineIterator lineIterForSibling = sibling->GetLineIterator();
      if (lineIterForSibling) {
        // For the frames before that grab all the lines
        int32_t addLines = lineIterForSibling->GetNumLines();
        lineNumber += addLines;
      }
      sibling = sibling->GetNextSibling();
    }

    // Get the line number relative to the container with lines
    if (!lineIterForCaret) {   // Add the caret line just once
      lineIterForCaret = parentFrame->GetLineIterator();
      if (lineIterForCaret) {
        // Ancestor of caret
        int32_t addLines = lineIterForCaret->FindLineContaining(caretFrame);
        lineNumber += addLines;
      }
    }

    caretFrame = parentFrame;
  }

  NS_NOTREACHED("DOM ancestry had this hypertext but frame ancestry didn't");
  return lineNumber;
}

already_AddRefed<nsFrameSelection>
HyperTextAccessible::FrameSelection()
{
  nsIFrame* frame = GetFrame();
  return frame ? frame->GetFrameSelection() : nullptr;
}

void
HyperTextAccessible::GetSelectionDOMRanges(int16_t aType,
                                           nsTArray<nsRange*>* aRanges)
{
  // Ignore selection if it is not visible.
  nsRefPtr<nsFrameSelection> frameSelection = FrameSelection();
  if (!frameSelection ||
      frameSelection->GetDisplaySelection() <= nsISelectionController::SELECTION_HIDDEN)
    return;

  Selection* domSel = frameSelection->GetSelection(aType);
  if (!domSel)
    return;

  nsCOMPtr<nsINode> startNode = GetNode();

  nsCOMPtr<nsIEditor> editor = GetEditor();
  if (editor) {
    nsCOMPtr<nsIDOMElement> editorRoot;
    editor->GetRootElement(getter_AddRefs(editorRoot));
    startNode = do_QueryInterface(editorRoot);
  }

  if (!startNode)
    return;

  uint32_t childCount = startNode->GetChildCount();
  nsresult rv = domSel->
    GetRangesForIntervalArray(startNode, 0, startNode, childCount, true, aRanges);
  NS_ENSURE_SUCCESS_VOID(rv);

  // Remove collapsed ranges
  uint32_t numRanges = aRanges->Length();
  for (uint32_t idx = 0; idx < numRanges; idx ++) {
    if ((*aRanges)[idx]->Collapsed()) {
      aRanges->RemoveElementAt(idx);
      --numRanges;
      --idx;
    }
  }
}

/*
 * Gets the number of selected regions.
 */
NS_IMETHODIMP
HyperTextAccessible::GetSelectionCount(int32_t* aSelectionCount)
{
  NS_ENSURE_ARG_POINTER(aSelectionCount);
  *aSelectionCount = 0;

  nsTArray<nsRange*> ranges;
  GetSelectionDOMRanges(nsISelectionController::SELECTION_NORMAL, &ranges);
  *aSelectionCount = int32_t(ranges.Length());

  return NS_OK;
}

/*
 * Gets the start and end offset of the specified selection.
 */
NS_IMETHODIMP
HyperTextAccessible::GetSelectionBounds(int32_t aSelectionNum,
                                        int32_t* aStartOffset,
                                        int32_t* aEndOffset)
{
  NS_ENSURE_ARG_POINTER(aStartOffset);
  NS_ENSURE_ARG_POINTER(aEndOffset);
  *aStartOffset = *aEndOffset = 0;

  nsTArray<nsRange*> ranges;
  GetSelectionDOMRanges(nsISelectionController::SELECTION_NORMAL, &ranges);

  uint32_t rangeCount = ranges.Length();
  if (aSelectionNum < 0 || aSelectionNum >= rangeCount)
    return NS_ERROR_INVALID_ARG;

  nsRange* range = ranges[aSelectionNum];

  // Get start and end points.
  nsINode* startNode = range->GetStartParent();
  nsINode* endNode = range->GetEndParent();
  int32_t startOffset = range->StartOffset(), endOffset = range->EndOffset();

  // Make sure start is before end, by swapping DOM points.  This occurs when
  // the user selects backwards in the text.
  int32_t rangeCompare = nsContentUtils::ComparePoints(endNode, endOffset,
                                                       startNode, startOffset);
  if (rangeCompare < 0) {
    nsINode* tempNode = startNode;
    startNode = endNode;
    endNode = tempNode;
    int32_t tempOffset = startOffset;
    startOffset = endOffset;
    endOffset = tempOffset;
  }

  Accessible* startAccessible =
    DOMPointToHypertextOffset(startNode, startOffset, aStartOffset);
  if (!startAccessible) {
    *aStartOffset = 0; // Could not find start point within this hypertext, so starts before
  }

  DOMPointToHypertextOffset(endNode, endOffset, aEndOffset, true);
  return NS_OK;
}

/*
 * Changes the start and end offset of the specified selection.
 */
NS_IMETHODIMP
HyperTextAccessible::SetSelectionBounds(int32_t aSelectionNum,
                                        int32_t aStartOffset,
                                        int32_t aEndOffset)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  if (aSelectionNum < 0)
    return NS_ERROR_INVALID_ARG;

  int32_t startOffset = ConvertMagicOffset(aStartOffset);
  int32_t endOffset = ConvertMagicOffset(aEndOffset);

  nsRefPtr<nsFrameSelection> frameSelection = FrameSelection();
  NS_ENSURE_STATE(frameSelection);

  Selection* domSel =
    frameSelection->GetSelection(nsISelectionController::SELECTION_NORMAL);
  NS_ENSURE_STATE(domSel);

  uint32_t rangeCount = domSel->GetRangeCount();
  if (rangeCount < static_cast<uint32_t>(aSelectionNum))
    return NS_ERROR_INVALID_ARG;

  nsRefPtr<nsRange> range;
  if (aSelectionNum == rangeCount)
    range = new nsRange(mContent);
  else
    range = domSel->GetRangeAt(aSelectionNum);

  nsresult rv = HypertextOffsetsToDOMRange(startOffset, endOffset, range);
  NS_ENSURE_SUCCESS(rv, rv);

  // If new range was created then add it, otherwise notify selection listeners
  // that existing selection range was changed.
  if (aSelectionNum == rangeCount)
    return domSel->AddRange(range);

  domSel->RemoveRange(range);
  domSel->AddRange(range);
  return NS_OK;
}

/*
 * Adds a selection bounded by the specified offsets.
 */
NS_IMETHODIMP
HyperTextAccessible::AddSelection(int32_t aStartOffset, int32_t aEndOffset)
{
  nsRefPtr<nsFrameSelection> frameSelection = FrameSelection();
  NS_ENSURE_STATE(frameSelection);

  Selection* domSel =
    frameSelection->GetSelection(nsISelectionController::SELECTION_NORMAL);
  NS_ENSURE_STATE(domSel);

  return SetSelectionBounds(domSel->GetRangeCount(), aStartOffset, aEndOffset);
}

/*
 * Removes the specified selection.
 */
NS_IMETHODIMP
HyperTextAccessible::RemoveSelection(int32_t aSelectionNum)
{
  nsRefPtr<nsFrameSelection> frameSelection = FrameSelection();
  NS_ENSURE_STATE(frameSelection);

  Selection* domSel =
    frameSelection->GetSelection(nsISelectionController::SELECTION_NORMAL);
  NS_ENSURE_STATE(domSel);

  if (aSelectionNum < 0 || aSelectionNum >= domSel->GetRangeCount())
    return NS_ERROR_INVALID_ARG;

  return domSel->RemoveRange(domSel->GetRangeAt(aSelectionNum));
}

// void nsIAccessibleText::
//   scrollSubstringTo(in long startIndex, in long endIndex,
//                     in unsigned long scrollType);
NS_IMETHODIMP
HyperTextAccessible::ScrollSubstringTo(int32_t aStartIndex, int32_t aEndIndex,
                                       uint32_t aScrollType)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsRefPtr<nsRange> range = new nsRange(mContent);
  nsresult rv = HypertextOffsetsToDOMRange(aStartIndex, aEndIndex, range);
  NS_ENSURE_SUCCESS(rv, rv);

  return nsCoreUtils::ScrollSubstringTo(GetFrame(), range, aScrollType);
}

// void nsIAccessibleText::
//   scrollSubstringToPoint(in long startIndex, in long endIndex,
//                          in unsigned long coordinateType,
//                          in long x, in long y);
NS_IMETHODIMP
HyperTextAccessible::ScrollSubstringToPoint(int32_t aStartIndex,
                                            int32_t aEndIndex,
                                            uint32_t aCoordinateType,
                                            int32_t aX, int32_t aY)
{
  nsIFrame *frame = GetFrame();
  if (!frame)
    return NS_ERROR_FAILURE;

  nsIntPoint coords = nsAccUtils::ConvertToScreenCoords(aX, aY, aCoordinateType,
                                                        this);

  nsRefPtr<nsRange> range = new nsRange(mContent);
  nsresult rv = HypertextOffsetsToDOMRange(aStartIndex, aEndIndex, range);
  NS_ENSURE_SUCCESS(rv, rv);

  nsPresContext* presContext = frame->PresContext();
  nsPoint coordsInAppUnits =
    coords.ToAppUnits(presContext->AppUnitsPerDevPixel());

  bool initialScrolled = false;
  nsIFrame *parentFrame = frame;
  while ((parentFrame = parentFrame->GetParent())) {
    nsIScrollableFrame *scrollableFrame = do_QueryFrame(parentFrame);
    if (scrollableFrame) {
      if (!initialScrolled) {
        // Scroll substring to the given point. Turn the point into percents
        // relative scrollable area to use nsCoreUtils::ScrollSubstringTo.
        nsRect frameRect = parentFrame->GetScreenRectInAppUnits();
        nscoord offsetPointX = coordsInAppUnits.x - frameRect.x;
        nscoord offsetPointY = coordsInAppUnits.y - frameRect.y;

        nsSize size(parentFrame->GetSize());

        // avoid divide by zero
        size.width = size.width ? size.width : 1;
        size.height = size.height ? size.height : 1;

        int16_t hPercent = offsetPointX * 100 / size.width;
        int16_t vPercent = offsetPointY * 100 / size.height;

        rv = nsCoreUtils::ScrollSubstringTo(frame, range, vPercent, hPercent);
        NS_ENSURE_SUCCESS(rv, rv);

        initialScrolled = true;
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

////////////////////////////////////////////////////////////////////////////////
// Accessible public

// Accessible protected
ENameValueFlag
HyperTextAccessible::NativeName(nsString& aName)
{
  // Check @alt attribute for invalid img elements.
  bool hasImgAlt = false;
  if (mContent->IsHTML(nsGkAtoms::img)) {
    hasImgAlt = mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::alt, aName);
    if (!aName.IsEmpty())
      return eNameOK;
  }

  ENameValueFlag nameFlag = AccessibleWrap::NativeName(aName);
  if (!aName.IsEmpty())
    return nameFlag;

  // Get name from title attribute for HTML abbr and acronym elements making it
  // a valid name from markup. Otherwise their name isn't picked up by recursive
  // name computation algorithm. See NS_OK_NAME_FROM_TOOLTIP.
  if (IsAbbreviation() &&
      mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::title, aName))
    aName.CompressWhitespace();

  return hasImgAlt ? eNoNameOnPurpose : eNameOK;
}

void
HyperTextAccessible::InvalidateChildren()
{
  mOffsets.Clear();

  AccessibleWrap::InvalidateChildren();
}

bool
HyperTextAccessible::RemoveChild(Accessible* aAccessible)
{
  int32_t childIndex = aAccessible->IndexInParent();
  int32_t count = mOffsets.Length() - childIndex;
  if (count > 0)
    mOffsets.RemoveElementsAt(childIndex, count);

  return Accessible::RemoveChild(aAccessible);
}

void
HyperTextAccessible::CacheChildren()
{
  // Trailing HTML br element don't play any difference. We don't need to expose
  // it to AT (see bug https://bugzilla.mozilla.org/show_bug.cgi?id=899433#c16
  // for details).

  TreeWalker walker(this, mContent);
  Accessible* child = nullptr;
  Accessible* lastChild = nullptr;
  while ((child = walker.NextChild())) {
    if (lastChild)
      AppendChild(lastChild);

    lastChild = child;
  }

  if (lastChild) {
    if (lastChild->IsHTMLBr())
      Document()->UnbindFromDocument(lastChild);
    else
      AppendChild(lastChild);
  }
}

////////////////////////////////////////////////////////////////////////////////
// HyperTextAccessible public static

nsresult
HyperTextAccessible::ContentToRenderedOffset(nsIFrame* aFrame, int32_t aContentOffset,
                                             uint32_t* aRenderedOffset)
{
  if (!aFrame) {
    // Current frame not rendered -- this can happen if text is set on
    // something with display: none
    *aRenderedOffset = 0;
    return NS_OK;
  }

  if (IsTextField()) {
    *aRenderedOffset = aContentOffset;
    return NS_OK;
  }

  NS_ASSERTION(aFrame->GetType() == nsGkAtoms::textFrame,
               "Need text frame for offset conversion");
  NS_ASSERTION(aFrame->GetPrevContinuation() == nullptr,
               "Call on primary frame only");

  gfxSkipChars skipChars;
  gfxSkipCharsIterator iter;
  // Only get info up to original offset, we know that will be larger than skipped offset
  nsresult rv = aFrame->GetRenderedText(nullptr, &skipChars, &iter, 0, aContentOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t ourRenderedStart = iter.GetSkippedOffset();
  int32_t ourContentStart = iter.GetOriginalOffset();

  *aRenderedOffset = iter.ConvertOriginalToSkipped(aContentOffset + ourContentStart) -
                    ourRenderedStart;

  return NS_OK;
}

nsresult
HyperTextAccessible::RenderedToContentOffset(nsIFrame* aFrame, uint32_t aRenderedOffset,
                                             int32_t* aContentOffset)
{
  if (IsTextField()) {
    *aContentOffset = aRenderedOffset;
    return NS_OK;
  }

  *aContentOffset = 0;
  NS_ENSURE_TRUE(aFrame, NS_ERROR_FAILURE);

  NS_ASSERTION(aFrame->GetType() == nsGkAtoms::textFrame,
               "Need text frame for offset conversion");
  NS_ASSERTION(aFrame->GetPrevContinuation() == nullptr,
               "Call on primary frame only");

  gfxSkipChars skipChars;
  gfxSkipCharsIterator iter;
  // We only need info up to skipped offset -- that is what we're converting to original offset
  nsresult rv = aFrame->GetRenderedText(nullptr, &skipChars, &iter, 0, aRenderedOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t ourRenderedStart = iter.GetSkippedOffset();
  int32_t ourContentStart = iter.GetOriginalOffset();

  *aContentOffset = iter.ConvertSkippedToOriginal(aRenderedOffset + ourRenderedStart) - ourContentStart;

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// HyperTextAccessible public

bool
HyperTextAccessible::GetCharAt(int32_t aOffset, EGetTextType aShift,
                               nsAString& aChar, int32_t* aStartOffset,
                               int32_t* aEndOffset)
{
  aChar.Truncate();

  int32_t offset = ConvertMagicOffset(aOffset) + static_cast<int32_t>(aShift);
  if (!CharAt(offset, aChar))
    return false;

  if (aStartOffset)
    *aStartOffset = offset;
  if (aEndOffset)
    *aEndOffset = aChar.IsEmpty() ? offset : offset + 1;

  return true;
}

int32_t
HyperTextAccessible::GetChildOffset(uint32_t aChildIndex,
                                    bool aInvalidateAfter)
{
  if (aChildIndex == 0) {
    if (aInvalidateAfter)
      mOffsets.Clear();

    return aChildIndex;
  }

  int32_t count = mOffsets.Length() - aChildIndex;
  if (count > 0) {
    if (aInvalidateAfter)
      mOffsets.RemoveElementsAt(aChildIndex, count);

    return mOffsets[aChildIndex - 1];
  }

  uint32_t lastOffset = mOffsets.IsEmpty() ?
    0 : mOffsets[mOffsets.Length() - 1];

  while (mOffsets.Length() < aChildIndex) {
    Accessible* child = mChildren[mOffsets.Length()];
    lastOffset += nsAccUtils::TextLength(child);
    mOffsets.AppendElement(lastOffset);
  }

  return mOffsets[aChildIndex - 1];
}

int32_t
HyperTextAccessible::GetChildIndexAtOffset(uint32_t aOffset)
{
  uint32_t lastOffset = 0;
  uint32_t offsetCount = mOffsets.Length();
  if (offsetCount > 0) {
    lastOffset = mOffsets[offsetCount - 1];
    if (aOffset < lastOffset) {
      uint32_t low = 0, high = offsetCount;
      while (high > low) {
        uint32_t mid = (high + low) >> 1;
        if (mOffsets[mid] == aOffset)
          return mid < offsetCount - 1 ? mid + 1 : mid;

        if (mOffsets[mid] < aOffset)
          low = mid + 1;
        else
          high = mid;
      }
      if (high == offsetCount)
        return -1;

      return low;
    }
  }

  uint32_t childCount = ChildCount();
  while (mOffsets.Length() < childCount) {
    Accessible* child = GetChildAt(mOffsets.Length());
    lastOffset += nsAccUtils::TextLength(child);
    mOffsets.AppendElement(lastOffset);
    if (aOffset < lastOffset)
      return mOffsets.Length() - 1;
  }

  if (aOffset == lastOffset)
    return mOffsets.Length() - 1;

  return -1;
}

////////////////////////////////////////////////////////////////////////////////
// HyperTextAccessible protected

nsresult
HyperTextAccessible::GetDOMPointByFrameOffset(nsIFrame* aFrame, int32_t aOffset,
                                              Accessible* aAccessible,
                                              DOMPoint* aPoint)
{
  NS_ENSURE_ARG(aAccessible);

  if (!aFrame) {
    // If the given frame is null then set offset after the DOM node of the
    // given accessible.
    NS_ASSERTION(!aAccessible->IsDoc(), 
                 "Shouldn't be called on document accessible!");

    nsIContent* content = aAccessible->GetContent();
    NS_ASSERTION(content, "Shouldn't operate on defunct accessible!");

    nsIContent* parent = content->GetParent();

    aPoint->idx = parent->IndexOf(content) + 1;
    aPoint->node = parent;

  } else if (aFrame->GetType() == nsGkAtoms::textFrame) {
    nsIContent* content = aFrame->GetContent();
    NS_ENSURE_STATE(content);

    nsIFrame *primaryFrame = content->GetPrimaryFrame();
    nsresult rv = RenderedToContentOffset(primaryFrame, aOffset, &(aPoint->idx));
    NS_ENSURE_SUCCESS(rv, rv);

    aPoint->node = content;

  } else {
    nsIContent* content = aFrame->GetContent();
    NS_ENSURE_STATE(content);

    nsIContent* parent = content->GetParent();
    NS_ENSURE_STATE(parent);

    aPoint->idx = parent->IndexOf(content);
    aPoint->node = parent;
  }

  return NS_OK;
}

// HyperTextAccessible
nsresult
HyperTextAccessible::RangeBoundToHypertextOffset(nsRange* aRange,
                                                 bool aIsStartBound,
                                                 bool aIsStartHTOffset,
                                                 int32_t* aHTOffset)
{
  nsINode* node = nullptr;
  int32_t nodeOffset = 0;

  if (aIsStartBound) {
    node = aRange->GetStartParent();
    nodeOffset = aRange->StartOffset();
  } else {
    node = aRange->GetEndParent();
    nodeOffset = aRange->EndOffset();
  }

  Accessible* startAcc =
    DOMPointToHypertextOffset(node, nodeOffset, aHTOffset);

  if (aIsStartHTOffset && !startAcc)
    *aHTOffset = 0;

  return NS_OK;
}

// HyperTextAccessible
nsresult
HyperTextAccessible::GetSpellTextAttribute(nsINode* aNode,
                                           int32_t aNodeOffset,
                                           int32_t* aHTStartOffset,
                                           int32_t* aHTEndOffset,
                                           nsIPersistentProperties* aAttributes)
{
  nsRefPtr<nsFrameSelection> fs = FrameSelection();
  if (!fs)
    return NS_OK;

  Selection* domSel = fs->GetSelection(nsISelectionController::SELECTION_SPELLCHECK);
  if (!domSel)
    return NS_OK;

  int32_t rangeCount = domSel->GetRangeCount();
  if (rangeCount <= 0)
    return NS_OK;

  int32_t startHTOffset = 0, endHTOffset = 0;
  nsresult rv = NS_OK;
  for (int32_t idx = 0; idx < rangeCount; idx++) {
    nsRange* range = domSel->GetRangeAt(idx);
    if (range->Collapsed())
      continue;

    // See if the point comes after the range in which case we must continue in
    // case there is another range after this one.
    nsINode* endNode = range->GetEndParent();
    int32_t endOffset = range->EndOffset();
    if (nsContentUtils::ComparePoints(aNode, aNodeOffset, endNode, endOffset) >= 0)
      continue;

    // At this point our point is either in this range or before it but after
    // the previous range.  So we check to see if the range starts before the
    // point in which case the point is in the missspelled range, otherwise it
    // must be before the range and after the previous one if any.
    nsINode* startNode = range->GetStartParent();
    int32_t startOffset = range->StartOffset();
    if (nsContentUtils::ComparePoints(startNode, startOffset, aNode,
                                      aNodeOffset) <= 0) {
      rv = RangeBoundToHypertextOffset(range, true, true, &startHTOffset);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = RangeBoundToHypertextOffset(range, false, false, &endHTOffset);
      NS_ENSURE_SUCCESS(rv, rv);

      if (startHTOffset > *aHTStartOffset)
        *aHTStartOffset = startHTOffset;

      if (endHTOffset < *aHTEndOffset)
        *aHTEndOffset = endHTOffset;

      if (aAttributes) {
        nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::invalid,
                               NS_LITERAL_STRING("spelling"));
      }

      return NS_OK;
    }

    // This range came after the point.
    rv = RangeBoundToHypertextOffset(range, true, false, &endHTOffset);
    NS_ENSURE_SUCCESS(rv, rv);

    if (idx > 0) {
      rv = RangeBoundToHypertextOffset(domSel->GetRangeAt(idx - 1), false,
                                       true, &startHTOffset);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (startHTOffset > *aHTStartOffset)
      *aHTStartOffset = startHTOffset;

    if (endHTOffset < *aHTEndOffset)
      *aHTEndOffset = endHTOffset;

    return NS_OK;
  }

  // We never found a range that ended after the point, therefore we know that
  // the point is not in a range, that we do not need to compute an end offset,
  // and that we should use the end offset of the last range to compute the
  // start offset of the text attribute range.
  rv = RangeBoundToHypertextOffset(domSel->GetRangeAt(rangeCount - 1), false,
                                   true, &startHTOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  if (startHTOffset > *aHTStartOffset)
    *aHTStartOffset = startHTOffset;

  return NS_OK;
}

bool 
HyperTextAccessible::IsTextRole()
{
  if (mRoleMapEntry &&
      (mRoleMapEntry->role == roles::GRAPHIC ||
       mRoleMapEntry->role == roles::IMAGE_MAP ||
       mRoleMapEntry->role == roles::SLIDER ||
       mRoleMapEntry->role == roles::PROGRESSBAR ||
       mRoleMapEntry->role == roles::SEPARATOR))
    return false;

  return true;
}
