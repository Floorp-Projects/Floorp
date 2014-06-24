/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcAccessibleHyperText.h"

#include "HyperTextAccessible-inl.h"
#include "TextRange.h"
#include "xpcAccessibleTextRange.h"

#include "nsIPersistentProperties2.h"
#include "nsIMutableArray.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsISupports

nsresult
xpcAccessibleHyperText::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  *aInstancePtr = nullptr;

  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (!text->IsTextRole())
    return NS_ERROR_NO_INTERFACE;

  if (aIID.Equals(NS_GET_IID(nsIAccessibleText)))
    *aInstancePtr = static_cast<nsIAccessibleText*>(text);
  else if (aIID.Equals(NS_GET_IID(nsIAccessibleEditableText)))
    *aInstancePtr = static_cast<nsIAccessibleEditableText*>(text);
  else if (aIID.Equals(NS_GET_IID(nsIAccessibleHyperText)))
    *aInstancePtr = static_cast<nsIAccessibleHyperText*>(text);
  else
    return NS_ERROR_NO_INTERFACE;

  NS_ADDREF(text);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibleText

NS_IMETHODIMP
xpcAccessibleHyperText::GetCharacterCount(int32_t* aCharacterCount)
{
  NS_ENSURE_ARG_POINTER(aCharacterCount);
  *aCharacterCount = 0;

  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  *aCharacterCount = text->CharacterCount();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetText(int32_t aStartOffset, int32_t aEndOffset,
                                nsAString& aText)
{
  aText.Truncate();

  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  text->TextSubstring(aStartOffset, aEndOffset, aText);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetTextBeforeOffset(int32_t aOffset,
                                            AccessibleTextBoundary aBoundaryType,
                                            int32_t* aStartOffset,
                                            int32_t* aEndOffset,
                                            nsAString& aText)
{
  NS_ENSURE_ARG_POINTER(aStartOffset);
  NS_ENSURE_ARG_POINTER(aEndOffset);
  *aStartOffset = *aEndOffset = 0;
  aText.Truncate();

  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  text->TextBeforeOffset(aOffset, aBoundaryType, aStartOffset, aEndOffset, aText);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetTextAtOffset(int32_t aOffset,
                                        AccessibleTextBoundary aBoundaryType,
                                        int32_t* aStartOffset,
                                        int32_t* aEndOffset, nsAString& aText)
{
  NS_ENSURE_ARG_POINTER(aStartOffset);
  NS_ENSURE_ARG_POINTER(aEndOffset);
  *aStartOffset = *aEndOffset = 0;
  aText.Truncate();

  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  text->TextAtOffset(aOffset, aBoundaryType, aStartOffset, aEndOffset, aText);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetTextAfterOffset(int32_t aOffset,
                                           AccessibleTextBoundary aBoundaryType,
                                           int32_t* aStartOffset,
                                           int32_t* aEndOffset, nsAString& aText)
{
  NS_ENSURE_ARG_POINTER(aStartOffset);
  NS_ENSURE_ARG_POINTER(aEndOffset);
  *aStartOffset = *aEndOffset = 0;
  aText.Truncate();

  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  text->TextAfterOffset(aOffset, aBoundaryType, aStartOffset, aEndOffset, aText);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetCharacterAtOffset(int32_t aOffset,
                                             char16_t* aCharacter)
{
  NS_ENSURE_ARG_POINTER(aCharacter);
  *aCharacter = L'\0';

  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  *aCharacter = text->CharAt(aOffset);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetTextAttributes(bool aIncludeDefAttrs,
                                          int32_t aOffset,
                                          int32_t* aStartOffset,
                                          int32_t* aEndOffset,
                                          nsIPersistentProperties** aAttributes)
{
  NS_ENSURE_ARG_POINTER(aStartOffset);
  NS_ENSURE_ARG_POINTER(aEndOffset);
  NS_ENSURE_ARG_POINTER(aAttributes);
  *aStartOffset = *aEndOffset = 0;
  *aAttributes = nullptr;

  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPersistentProperties> attrs =
   text->TextAttributes(aIncludeDefAttrs, aOffset, aStartOffset, aEndOffset);
  attrs.swap(*aAttributes);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetDefaultTextAttributes(nsIPersistentProperties** aAttributes)
{
  NS_ENSURE_ARG_POINTER(aAttributes);
  *aAttributes = nullptr;

  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPersistentProperties> attrs = text->DefaultTextAttributes();
  attrs.swap(*aAttributes);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetCharacterExtents(int32_t aOffset,
                                            int32_t* aX, int32_t* aY,
                                            int32_t* aWidth, int32_t* aHeight,
                                            uint32_t aCoordType)
{
  NS_ENSURE_ARG_POINTER(aX);
  NS_ENSURE_ARG_POINTER(aY);
  NS_ENSURE_ARG_POINTER(aWidth);
  NS_ENSURE_ARG_POINTER(aHeight);
  *aX = *aY = *aWidth = *aHeight;

  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  nsIntRect rect = text->CharBounds(aOffset, aCoordType);
  *aX = rect.x; *aY = rect.y;
  *aWidth = rect.width; *aHeight = rect.height;
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetRangeExtents(int32_t aStartOffset, int32_t aEndOffset,
                                        int32_t* aX, int32_t* aY,
                                        int32_t* aWidth, int32_t* aHeight,
                                        uint32_t aCoordType)
{
  NS_ENSURE_ARG_POINTER(aX);
  NS_ENSURE_ARG_POINTER(aY);
  NS_ENSURE_ARG_POINTER(aWidth);
  NS_ENSURE_ARG_POINTER(aHeight);
  *aX = *aY = *aWidth = *aHeight = 0;

  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  nsIntRect rect = text->TextBounds(aStartOffset, aEndOffset, aCoordType);
  *aX = rect.x; *aY = rect.y;
  *aWidth = rect.width; *aHeight = rect.height;
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetOffsetAtPoint(int32_t aX, int32_t aY,
                                         uint32_t aCoordType, int32_t* aOffset)
{
  NS_ENSURE_ARG_POINTER(aOffset);
  *aOffset = -1;

  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  *aOffset = text->OffsetAtPoint(aX, aY, aCoordType);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetScriptableCaretOffset(int32_t* aCaretOffset)
{
  NS_ENSURE_ARG_POINTER(aCaretOffset);
  *aCaretOffset = -1;

  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  *aCaretOffset = text->CaretOffset();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::SetScriptableCaretOffset(int32_t aCaretOffset)
{
  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  text->SetCaretOffset(aCaretOffset);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetSelectionCount(int32_t* aSelectionCount)
{
  NS_ENSURE_ARG_POINTER(aSelectionCount);
  *aSelectionCount = 0;

  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  *aSelectionCount = text->SelectionCount();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetSelectionBounds(int32_t aSelectionNum,
                                           int32_t* aStartOffset,
                                           int32_t* aEndOffset)
{
  NS_ENSURE_ARG_POINTER(aStartOffset);
  NS_ENSURE_ARG_POINTER(aEndOffset);
  *aStartOffset = *aEndOffset = 0;

  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  if (aSelectionNum < 0 || aSelectionNum >= text->SelectionCount())
    return NS_ERROR_INVALID_ARG;

  text->SelectionBoundsAt(aSelectionNum, aStartOffset, aEndOffset);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::SetSelectionBounds(int32_t aSelectionNum,
                                           int32_t aStartOffset,
                                           int32_t aEndOffset)
{
  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  if (aSelectionNum < 0 ||
      !text->SetSelectionBoundsAt(aSelectionNum, aStartOffset, aEndOffset))
    return NS_ERROR_INVALID_ARG;

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::AddSelection(int32_t aStartOffset, int32_t aEndOffset)
{
  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  text->AddToSelection(aStartOffset, aEndOffset);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::RemoveSelection(int32_t aSelectionNum)
{
  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  text->RemoveFromSelection(aSelectionNum);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::ScriptableScrollSubstringTo(int32_t aStartOffset,
                                                    int32_t aEndOffset,
                                                    uint32_t aScrollType)
{
  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  text->ScrollSubstringTo(aStartOffset, aEndOffset, aScrollType);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::ScriptableScrollSubstringToPoint(int32_t aStartOffset,
                                                         int32_t aEndOffset,
                                                         uint32_t aCoordinateType,
                                                         int32_t aX, int32_t aY)
{
  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  text->ScrollSubstringToPoint(aStartOffset, aEndOffset, aCoordinateType, aX, aY);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetEnclosingRange(nsIAccessibleTextRange** aRange)
{
  NS_ENSURE_ARG_POINTER(aRange);
  *aRange = nullptr;

  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  nsRefPtr<xpcAccessibleTextRange> range = new xpcAccessibleTextRange;
  text->EnclosingRange(range->mRange);
  NS_ASSERTION(range->mRange.IsValid(),
               "Should always have an enclosing range!");

  range.forget(aRange);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetSelectionRanges(nsIArray** aRanges)
{
  NS_ENSURE_ARG_POINTER(aRanges);
  *aRanges = nullptr;

  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  nsCOMPtr<nsIMutableArray> xpcRanges =
    do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoTArray<TextRange, 1> ranges;
  text->SelectionRanges(&ranges);
  uint32_t len = ranges.Length();
  for (uint32_t idx = 0; idx < len; idx++)
    xpcRanges->AppendElement(new xpcAccessibleTextRange(Move(ranges[idx])),
                             false);

  xpcRanges.forget(aRanges);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetVisibleRanges(nsIArray** aRanges)
{
  NS_ENSURE_ARG_POINTER(aRanges);
  *aRanges = nullptr;

  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  nsCOMPtr<nsIMutableArray> xpcRanges =
    do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsTArray<TextRange> ranges;
  text->VisibleRanges(&ranges);
  uint32_t len = ranges.Length();
  for (uint32_t idx = 0; idx < len; idx++)
    xpcRanges->AppendElement(new xpcAccessibleTextRange(Move(ranges[idx])),
                             false);

  xpcRanges.forget(aRanges);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetRangeByChild(nsIAccessible* aChild,
                                        nsIAccessibleTextRange** aRange)
{
  NS_ENSURE_ARG_POINTER(aRange);
  *aRange = nullptr;

  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  nsRefPtr<Accessible> child = do_QueryObject(aChild);
  if (child) {
    nsRefPtr<xpcAccessibleTextRange> range = new xpcAccessibleTextRange;
    text->RangeByChild(child, range->mRange);
    if (range->mRange.IsValid())
      range.forget(aRange);
  }

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetRangeAtPoint(int32_t aX, int32_t aY,
                                        nsIAccessibleTextRange** aRange)
{
  NS_ENSURE_ARG_POINTER(aRange);
  *aRange = nullptr;

  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  nsRefPtr<xpcAccessibleTextRange> range = new xpcAccessibleTextRange;
  text->RangeAtPoint(aX, aY, range->mRange);
  if (range->mRange.IsValid())
    range.forget(aRange);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibleEditableText

NS_IMETHODIMP
xpcAccessibleHyperText::SetTextContents(const nsAString& aText)
{
  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  text->ReplaceText(aText);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::ScriptableInsertText(const nsAString& aText,
                                             int32_t aOffset)
{
  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  text->InsertText(aText, aOffset);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::ScriptableCopyText(int32_t aStartOffset,
                                           int32_t aEndOffset)
{
  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  text->CopyText(aStartOffset, aEndOffset);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::ScriptableCutText(int32_t aStartOffset,
                                          int32_t aEndOffset)
{
  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  text->CutText(aStartOffset, aEndOffset);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::ScriptableDeleteText(int32_t aStartOffset,
                                             int32_t aEndOffset)
{
  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  text->DeleteText(aStartOffset, aEndOffset);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::ScriptablePasteText(int32_t aOffset)
{
  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  text->PasteText(aOffset);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibleHyperText

NS_IMETHODIMP
xpcAccessibleHyperText::GetLinkCount(int32_t* aLinkCount)
{
  NS_ENSURE_ARG_POINTER(aLinkCount);
  *aLinkCount = 0;

  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  *aLinkCount = text->LinkCount();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetLinkAt(int32_t aIndex, nsIAccessibleHyperLink** aLink)
{
  NS_ENSURE_ARG_POINTER(aLink);
  *aLink = nullptr;

  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAccessibleHyperLink> link = text->LinkAt(aIndex);
  link.forget(aLink);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetLinkIndex(nsIAccessibleHyperLink* aLink,
                                     int32_t* aIndex)
{
  NS_ENSURE_ARG_POINTER(aLink);
  NS_ENSURE_ARG_POINTER(aIndex);
  *aIndex = -1;

  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  nsRefPtr<Accessible> link(do_QueryObject(aLink));
  *aIndex = text->LinkIndexOf(link);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetLinkIndexAtOffset(int32_t aOffset,
                                             int32_t* aLinkIndex)
{
  NS_ENSURE_ARG_POINTER(aLinkIndex);
  *aLinkIndex = -1; // API says this magic value means 'not found'

  HyperTextAccessible* text = static_cast<HyperTextAccessible*>(this);
  if (text->IsDefunct())
    return NS_ERROR_FAILURE;

  *aLinkIndex = text->LinkIndexAtOffset(aOffset);
  return NS_OK;
}
