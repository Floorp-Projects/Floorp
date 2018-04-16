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
#include "nsFrameSelection.h"

#include "mozilla/TextEditor.h"

namespace mozilla {
namespace a11y {

inline bool
HyperTextAccessible::IsValidOffset(int32_t aOffset)
{
  index_t offset = ConvertMagicOffset(aOffset);
  return offset.IsValid() && offset <= CharacterCount();
}

inline bool
HyperTextAccessible::IsValidRange(int32_t aStartOffset, int32_t aEndOffset)
{
  index_t startOffset = ConvertMagicOffset(aStartOffset);
  index_t endOffset = ConvertMagicOffset(aEndOffset);
  return startOffset.IsValid() && endOffset.IsValid() &&
    startOffset <= endOffset && endOffset <= CharacterCount();
}

inline void
HyperTextAccessible::SetCaretOffset(int32_t aOffset)
{
  SetSelectionRange(aOffset, aOffset);
  // XXX: Force cache refresh until a good solution for AT emulation of user
  // input is implemented (AccessFu caret movement).
  SelectionMgr()->UpdateCaretOffset(this, aOffset);
}

inline bool
HyperTextAccessible::AddToSelection(int32_t aStartOffset, int32_t aEndOffset)
{
  dom::Selection* domSel = DOMSelection();
  return domSel &&
    SetSelectionBoundsAt(domSel->RangeCount(), aStartOffset, aEndOffset);
}

inline void
HyperTextAccessible::ReplaceText(const nsAString& aText)
{
  // We need to call DeleteText() even if there is no contents because we need
  // to ensure to move focus to the editor via SetSelectionRange() called in
  // DeleteText().
  DeleteText(0, CharacterCount());

  RefPtr<TextEditor> textEditor = GetEditor();
  if (!textEditor) {
    return;
  }

  // DeleteText() may cause inserting <br> element in some cases. Let's
  // select all again and replace whole contents.
  textEditor->SelectAll();

  DebugOnly<nsresult> rv = textEditor->InsertTextAsAction(aText);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to insert the new text");
}

inline void
HyperTextAccessible::InsertText(const nsAString& aText, int32_t aPosition)
{
  RefPtr<TextEditor> textEditor = GetEditor();
  if (textEditor) {
    SetSelectionRange(aPosition, aPosition);
    DebugOnly<nsresult> rv = textEditor->InsertTextAsAction(aText);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to insert the text");
  }
}

inline void
HyperTextAccessible::CopyText(int32_t aStartPos, int32_t aEndPos)
  {
    RefPtr<TextEditor> textEditor = GetEditor();
    if (textEditor) {
      SetSelectionRange(aStartPos, aEndPos);
      textEditor->Copy();
    }
  }

inline void
HyperTextAccessible::CutText(int32_t aStartPos, int32_t aEndPos)
  {
    RefPtr<TextEditor> textEditor = GetEditor();
    if (textEditor) {
      SetSelectionRange(aStartPos, aEndPos);
      textEditor->Cut();
    }
  }

inline void
HyperTextAccessible::DeleteText(int32_t aStartPos, int32_t aEndPos)
{
  RefPtr<TextEditor> textEditor = GetEditor();
  if (!textEditor) {
    return;
  }
  SetSelectionRange(aStartPos, aEndPos);
  DebugOnly<nsresult> rv =
    textEditor->DeleteSelectionAsAction(nsIEditor::eNone, nsIEditor::eStrip);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to delete text");
}

inline void
HyperTextAccessible::PasteText(int32_t aPosition)
{
  RefPtr<TextEditor> textEditor = GetEditor();
  if (textEditor) {
    SetSelectionRange(aPosition, aPosition);
    textEditor->Paste(nsIClipboard::kGlobalClipboard);
  }
}

inline index_t
HyperTextAccessible::ConvertMagicOffset(int32_t aOffset) const
{
  if (aOffset == nsIAccessibleText::TEXT_OFFSET_END_OF_TEXT)
    return CharacterCount();

  if (aOffset == nsIAccessibleText::TEXT_OFFSET_CARET)
    return CaretOffset();

  return aOffset;
}

inline uint32_t
HyperTextAccessible::AdjustCaretOffset(uint32_t aOffset) const
{
  // It is the same character offset when the caret is visually at the very
  // end of a line or the start of a new line (soft line break). Getting text
  // at the line should provide the line with the visual caret, otherwise
  // screen readers will announce the wrong line as the user presses up or
  // down arrow and land at the end of a line.
  if (aOffset > 0 && IsCaretAtEndOfLine())
    return aOffset - 1;

  return aOffset;
}

inline bool
HyperTextAccessible::IsCaretAtEndOfLine() const
{
  RefPtr<nsFrameSelection> frameSelection = FrameSelection();
  return frameSelection &&
    frameSelection->GetHint() == CARET_ASSOCIATE_BEFORE;
}

inline already_AddRefed<nsFrameSelection>
HyperTextAccessible::FrameSelection() const
{
  nsIFrame* frame = GetFrame();
  return frame ? frame->GetFrameSelection() : nullptr;
}

inline dom::Selection*
HyperTextAccessible::DOMSelection() const
{
  RefPtr<nsFrameSelection> frameSelection = FrameSelection();
  return frameSelection ? frameSelection->GetSelection(SelectionType::eNormal) :
                          nullptr;
}

} // namespace a11y
} // namespace mozilla

#endif

