/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcAccessibleHyperText.h"

#include "Accessible-inl.h"
#include "HyperTextAccessible-inl.h"
#include "TextRange.h"
#include "xpcAccessibleDocument.h"
#include "xpcAccessibleTextRange.h"

#include "nsIPersistentProperties2.h"
#include "nsIMutableArray.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_INTERFACE_MAP_BEGIN(xpcAccessibleHyperText)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIAccessibleText,
                                     mSupportedIfaces & eText)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIAccessibleEditableText,
                                     mSupportedIfaces & eText)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIAccessibleHyperText,
                                     mSupportedIfaces & eText)
NS_INTERFACE_MAP_END_INHERITING(xpcAccessibleGeneric)

NS_IMPL_ADDREF_INHERITED(xpcAccessibleHyperText, xpcAccessibleGeneric)
NS_IMPL_RELEASE_INHERITED(xpcAccessibleHyperText, xpcAccessibleGeneric)

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibleText

NS_IMETHODIMP
xpcAccessibleHyperText::GetCharacterCount(int32_t* aCharacterCount)
{
  NS_ENSURE_ARG_POINTER(aCharacterCount);
  *aCharacterCount = 0;

  if (!Intl())
    return NS_ERROR_FAILURE;

  *aCharacterCount = Intl()->CharacterCount();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetText(int32_t aStartOffset, int32_t aEndOffset,
                                nsAString& aText)
{
  aText.Truncate();

  if (!Intl())
    return NS_ERROR_FAILURE;

  Intl()->TextSubstring(aStartOffset, aEndOffset, aText);
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

  if (!Intl())
    return NS_ERROR_FAILURE;

  Intl()->TextBeforeOffset(aOffset, aBoundaryType, aStartOffset, aEndOffset, aText);
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

  if (!Intl())
    return NS_ERROR_FAILURE;

  Intl()->TextAtOffset(aOffset, aBoundaryType, aStartOffset, aEndOffset, aText);
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

  if (!Intl())
    return NS_ERROR_FAILURE;

  Intl()->TextAfterOffset(aOffset, aBoundaryType, aStartOffset, aEndOffset, aText);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetCharacterAtOffset(int32_t aOffset,
                                             char16_t* aCharacter)
{
  NS_ENSURE_ARG_POINTER(aCharacter);
  *aCharacter = L'\0';

  if (!Intl())
    return NS_ERROR_FAILURE;

  *aCharacter = Intl()->CharAt(aOffset);
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

  if (!Intl())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPersistentProperties> attrs =
   Intl()->TextAttributes(aIncludeDefAttrs, aOffset, aStartOffset, aEndOffset);
  attrs.swap(*aAttributes);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetDefaultTextAttributes(nsIPersistentProperties** aAttributes)
{
  NS_ENSURE_ARG_POINTER(aAttributes);
  *aAttributes = nullptr;

  if (!Intl())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPersistentProperties> attrs = Intl()->DefaultTextAttributes();
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

  if (!Intl())
    return NS_ERROR_FAILURE;

  nsIntRect rect = Intl()->CharBounds(aOffset, aCoordType);
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

  if (!Intl())
    return NS_ERROR_FAILURE;

  nsIntRect rect = Intl()->TextBounds(aStartOffset, aEndOffset, aCoordType);
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

  if (!Intl())
    return NS_ERROR_FAILURE;

  *aOffset = Intl()->OffsetAtPoint(aX, aY, aCoordType);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetCaretOffset(int32_t* aCaretOffset)
{
  NS_ENSURE_ARG_POINTER(aCaretOffset);
  *aCaretOffset = -1;

  if (!Intl())
    return NS_ERROR_FAILURE;

  *aCaretOffset = Intl()->CaretOffset();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::SetCaretOffset(int32_t aCaretOffset)
{
  if (!Intl())
    return NS_ERROR_FAILURE;

  Intl()->SetCaretOffset(aCaretOffset);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetSelectionCount(int32_t* aSelectionCount)
{
  NS_ENSURE_ARG_POINTER(aSelectionCount);
  *aSelectionCount = 0;

  if (!Intl())
    return NS_ERROR_FAILURE;

  *aSelectionCount = Intl()->SelectionCount();
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

  if (!Intl())
    return NS_ERROR_FAILURE;

  if (aSelectionNum < 0 || aSelectionNum >= Intl()->SelectionCount())
    return NS_ERROR_INVALID_ARG;

  Intl()->SelectionBoundsAt(aSelectionNum, aStartOffset, aEndOffset);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::SetSelectionBounds(int32_t aSelectionNum,
                                           int32_t aStartOffset,
                                           int32_t aEndOffset)
{
  if (!Intl())
    return NS_ERROR_FAILURE;

  if (aSelectionNum < 0 ||
      !Intl()->SetSelectionBoundsAt(aSelectionNum, aStartOffset, aEndOffset))
    return NS_ERROR_INVALID_ARG;

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::AddSelection(int32_t aStartOffset, int32_t aEndOffset)
{
  if (!Intl())
    return NS_ERROR_FAILURE;

  Intl()->AddToSelection(aStartOffset, aEndOffset);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::RemoveSelection(int32_t aSelectionNum)
{
  if (!Intl())
    return NS_ERROR_FAILURE;

  Intl()->RemoveFromSelection(aSelectionNum);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::ScrollSubstringTo(int32_t aStartOffset,
                                          int32_t aEndOffset,
                                          uint32_t aScrollType)
{
  if (!Intl())
    return NS_ERROR_FAILURE;

  Intl()->ScrollSubstringTo(aStartOffset, aEndOffset, aScrollType);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::ScrollSubstringToPoint(int32_t aStartOffset,
                                               int32_t aEndOffset,
                                               uint32_t aCoordinateType,
                                               int32_t aX, int32_t aY)
{
  if (!Intl())
    return NS_ERROR_FAILURE;

  Intl()->ScrollSubstringToPoint(aStartOffset, aEndOffset, aCoordinateType, aX, aY);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetEnclosingRange(nsIAccessibleTextRange** aRange)
{
  NS_ENSURE_ARG_POINTER(aRange);
  *aRange = nullptr;

  if (!Intl())
    return NS_ERROR_FAILURE;

  nsRefPtr<xpcAccessibleTextRange> range = new xpcAccessibleTextRange;
  Intl()->EnclosingRange(range->mRange);
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

  if (!Intl())
    return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  nsCOMPtr<nsIMutableArray> xpcRanges =
    do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoTArray<TextRange, 1> ranges;
  Intl()->SelectionRanges(&ranges);
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

  if (!Intl())
    return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  nsCOMPtr<nsIMutableArray> xpcRanges =
    do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsTArray<TextRange> ranges;
  Intl()->VisibleRanges(&ranges);
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

  if (!Intl())
    return NS_ERROR_FAILURE;

  Accessible* child = aChild->ToInternalAccessible();
  if (child) {
    nsRefPtr<xpcAccessibleTextRange> range = new xpcAccessibleTextRange;
    Intl()->RangeByChild(child, range->mRange);
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

  if (!Intl())
    return NS_ERROR_FAILURE;

  nsRefPtr<xpcAccessibleTextRange> range = new xpcAccessibleTextRange;
  Intl()->RangeAtPoint(aX, aY, range->mRange);
  if (range->mRange.IsValid())
    range.forget(aRange);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibleEditableText

NS_IMETHODIMP
xpcAccessibleHyperText::SetTextContents(const nsAString& aText)
{
  if (!Intl())
    return NS_ERROR_FAILURE;

  Intl()->ReplaceText(aText);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::InsertText(const nsAString& aText, int32_t aOffset)
{
  if (!Intl())
    return NS_ERROR_FAILURE;

  Intl()->InsertText(aText, aOffset);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::CopyText(int32_t aStartOffset, int32_t aEndOffset)
{
  if (!Intl())
    return NS_ERROR_FAILURE;

  Intl()->CopyText(aStartOffset, aEndOffset);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::CutText(int32_t aStartOffset, int32_t aEndOffset)
{
  if (!Intl())
    return NS_ERROR_FAILURE;

  Intl()->CutText(aStartOffset, aEndOffset);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::DeleteText(int32_t aStartOffset, int32_t aEndOffset)
{
  if (!Intl())
    return NS_ERROR_FAILURE;

  Intl()->DeleteText(aStartOffset, aEndOffset);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::PasteText(int32_t aOffset)
{
  if (!Intl())
    return NS_ERROR_FAILURE;

  Intl()->PasteText(aOffset);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibleHyperText

NS_IMETHODIMP
xpcAccessibleHyperText::GetLinkCount(int32_t* aLinkCount)
{
  NS_ENSURE_ARG_POINTER(aLinkCount);
  *aLinkCount = 0;

  if (!Intl())
    return NS_ERROR_FAILURE;

  *aLinkCount = Intl()->LinkCount();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetLinkAt(int32_t aIndex, nsIAccessibleHyperLink** aLink)
{
  NS_ENSURE_ARG_POINTER(aLink);
  *aLink = nullptr;

  if (!Intl())
    return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aLink = ToXPC(Intl()->LinkAt(aIndex)));
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetLinkIndex(nsIAccessibleHyperLink* aLink,
                                     int32_t* aIndex)
{
  NS_ENSURE_ARG_POINTER(aLink);
  NS_ENSURE_ARG_POINTER(aIndex);
  *aIndex = -1;

  if (!Intl())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAccessible> xpcLink(do_QueryInterface(aLink));
  Accessible* link = xpcLink->ToInternalAccessible();
  if (link)
    *aIndex = Intl()->LinkIndexOf(link);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetLinkIndexAtOffset(int32_t aOffset,
                                             int32_t* aLinkIndex)
{
  NS_ENSURE_ARG_POINTER(aLinkIndex);
  *aLinkIndex = -1; // API says this magic value means 'not found'

  if (!Intl())
    return NS_ERROR_FAILURE;

  *aLinkIndex = Intl()->LinkIndexAtOffset(aOffset);
  return NS_OK;
}
