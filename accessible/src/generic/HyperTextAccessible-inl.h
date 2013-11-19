/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_HyperTextAccessible_inl_h__
#define mozilla_a11y_HyperTextAccessible_inl_h__

#include "HyperTextAccessible.h"

#include "nsAccUtils.h"

#include "nsIClipboard.h"
#include "nsIEditor.h"
#include "nsIPersistentProperties2.h"
#include "nsIPlaintextEditor.h"

namespace mozilla {
namespace a11y {

inline bool
HyperTextAccessible::IsValidOffset(int32_t aOffset)
{
  int32_t offset = ConvertMagicOffset(aOffset);
  return offset >= 0 && offset <= static_cast<int32_t>(CharacterCount());
}

inline bool
HyperTextAccessible::IsValidRange(int32_t aStartOffset, int32_t aEndOffset)
{
  int32_t startOffset = ConvertMagicOffset(aStartOffset);
  if (startOffset < 0)
    return false;

  int32_t endOffset = ConvertMagicOffset(aEndOffset);
  if (endOffset < 0 || startOffset > endOffset)
    return false;

  return endOffset <= static_cast<int32_t>(CharacterCount());
}

inline nsIntRect
HyperTextAccessible::TextBounds(int32_t aStartOffset, int32_t aEndOffset,
                                uint32_t aCoordType)
{
  nsIntRect bounds;
  GetPosAndText(aStartOffset, aEndOffset, nullptr, nullptr, &bounds);
  nsAccUtils::ConvertScreenCoordsTo(&bounds.x, &bounds.y, aCoordType, this);
  return bounds;
}

inline bool
HyperTextAccessible::AddToSelection(int32_t aStartOffset, int32_t aEndOffset)
{
  Selection* domSel = DOMSelection();
  return domSel &&
    SetSelectionBoundsAt(domSel->GetRangeCount(), aStartOffset, aEndOffset);
}

inline void
HyperTextAccessible::ReplaceText(const nsAString& aText)
{
  int32_t numChars = CharacterCount();
  if (numChars != 0)
    DeleteText(0, numChars);

  InsertText(aText, 0);
}

inline void
HyperTextAccessible::InsertText(const nsAString& aText, int32_t aPosition)
{
  nsCOMPtr<nsIEditor> editor = GetEditor();
  nsCOMPtr<nsIPlaintextEditor> peditor(do_QueryInterface(editor));
  if (peditor) {
    SetSelectionRange(aPosition, aPosition);
    peditor->InsertText(aText);
  }
}

inline void
HyperTextAccessible::CopyText(int32_t aStartPos, int32_t aEndPos)
  {
    nsCOMPtr<nsIEditor> editor = GetEditor();
    if (editor) {
      SetSelectionRange(aStartPos, aEndPos);
      editor->Copy();
    }
  }

inline void
HyperTextAccessible::CutText(int32_t aStartPos, int32_t aEndPos)
  {
    nsCOMPtr<nsIEditor> editor = GetEditor();
    if (editor) {
      SetSelectionRange(aStartPos, aEndPos);
      editor->Cut();
    }
  }

inline void
HyperTextAccessible::DeleteText(int32_t aStartPos, int32_t aEndPos)
{
  nsCOMPtr<nsIEditor> editor = GetEditor();
  if (editor) {
    SetSelectionRange(aStartPos, aEndPos);
    editor->DeleteSelection(nsIEditor::eNone, nsIEditor::eStrip);
  }
}

inline void
HyperTextAccessible::PasteText(int32_t aPosition)
{
  nsCOMPtr<nsIEditor> editor = GetEditor();
  if (editor) {
    SetSelectionRange(aPosition, aPosition);
    editor->Paste(nsIClipboard::kGlobalClipboard);
  }
}

inline int32_t
HyperTextAccessible::ConvertMagicOffset(int32_t aOffset)
{
  if (aOffset == nsIAccessibleText::TEXT_OFFSET_END_OF_TEXT)
    return CharacterCount();

  if (aOffset == nsIAccessibleText::TEXT_OFFSET_CARET)
    return CaretOffset();

  return aOffset;
}

inline int32_t
HyperTextAccessible::AdjustCaretOffset(int32_t aOffset) const
{
  // It is the same character offset when the caret is visually at the very
  // end of a line or the start of a new line (soft line break). Getting text
  // at the line should provide the line with the visual caret, otherwise
  // screen readers will announce the wrong line as the user presses up or
  // down arrow and land at the end of a line.
  if (aOffset > 0) {
    nsRefPtr<nsFrameSelection> frameSelection = FrameSelection();
    if (frameSelection &&
      frameSelection->GetHint() == nsFrameSelection::HINTLEFT) {
      return aOffset - 1;
    }
  }
  return aOffset;
}

inline Selection*
HyperTextAccessible::DOMSelection() const
{
  nsRefPtr<nsFrameSelection> frameSelection = FrameSelection();
  return frameSelection ?
    frameSelection->GetSelection(nsISelectionController::SELECTION_NORMAL) :
    nullptr;
}

} // namespace a11y
} // namespace mozilla

#endif

