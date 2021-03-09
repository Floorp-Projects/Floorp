/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcAccessibleHyperText.h"

#include "LocalAccessible-inl.h"
#include "HyperTextAccessible-inl.h"
#include "mozilla/a11y/PDocAccessible.h"
#include "TextRange.h"
#include "nsComponentManagerUtils.h"
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
xpcAccessibleHyperText::GetCharacterCount(int32_t* aCharacterCount) {
  NS_ENSURE_ARG_POINTER(aCharacterCount);
  *aCharacterCount = 0;

  if (!mIntl) return NS_ERROR_FAILURE;

  if (mIntl->IsLocal()) {
    *aCharacterCount = Intl()->CharacterCount();
  } else {
#if defined(XP_WIN)
    return NS_ERROR_NOT_IMPLEMENTED;
#else
    *aCharacterCount = mIntl->AsRemote()->CharacterCount();
#endif
  }
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetText(int32_t aStartOffset, int32_t aEndOffset,
                                nsAString& aText) {
  aText.Truncate();

  if (!mIntl) return NS_ERROR_FAILURE;

  if (mIntl->IsLocal()) {
    Intl()->TextSubstring(aStartOffset, aEndOffset, aText);
  } else {
    nsString text;
    mIntl->AsRemote()->TextSubstring(aStartOffset, aEndOffset, text);
    aText = text;
  }
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetTextBeforeOffset(
    int32_t aOffset, AccessibleTextBoundary aBoundaryType,
    int32_t* aStartOffset, int32_t* aEndOffset, nsAString& aText) {
  NS_ENSURE_ARG_POINTER(aStartOffset);
  NS_ENSURE_ARG_POINTER(aEndOffset);
  *aStartOffset = *aEndOffset = 0;
  aText.Truncate();

  if (!mIntl) return NS_ERROR_FAILURE;

  if (mIntl->IsLocal()) {
    Intl()->TextBeforeOffset(aOffset, aBoundaryType, aStartOffset, aEndOffset,
                             aText);
  } else {
    nsString text;
    mIntl->AsRemote()->GetTextBeforeOffset(aOffset, aBoundaryType, text,
                                           aStartOffset, aEndOffset);
    aText = text;
  }
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetTextAtOffset(int32_t aOffset,
                                        AccessibleTextBoundary aBoundaryType,
                                        int32_t* aStartOffset,
                                        int32_t* aEndOffset, nsAString& aText) {
  NS_ENSURE_ARG_POINTER(aStartOffset);
  NS_ENSURE_ARG_POINTER(aEndOffset);
  *aStartOffset = *aEndOffset = 0;
  aText.Truncate();

  if (!mIntl) return NS_ERROR_FAILURE;

  if (mIntl->IsLocal()) {
    Intl()->TextAtOffset(aOffset, aBoundaryType, aStartOffset, aEndOffset,
                         aText);
  } else {
    nsString text;
    mIntl->AsRemote()->GetTextAtOffset(aOffset, aBoundaryType, text,
                                       aStartOffset, aEndOffset);
    aText = text;
  }
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetTextAfterOffset(int32_t aOffset,
                                           AccessibleTextBoundary aBoundaryType,
                                           int32_t* aStartOffset,
                                           int32_t* aEndOffset,
                                           nsAString& aText) {
  NS_ENSURE_ARG_POINTER(aStartOffset);
  NS_ENSURE_ARG_POINTER(aEndOffset);
  *aStartOffset = *aEndOffset = 0;
  aText.Truncate();

  if (!mIntl) return NS_ERROR_FAILURE;

  if (mIntl->IsLocal()) {
    Intl()->TextAfterOffset(aOffset, aBoundaryType, aStartOffset, aEndOffset,
                            aText);
  } else {
    nsString text;
    mIntl->AsRemote()->GetTextAfterOffset(aOffset, aBoundaryType, text,
                                          aStartOffset, aEndOffset);
    aText = text;
  }
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetCharacterAtOffset(int32_t aOffset,
                                             char16_t* aCharacter) {
  NS_ENSURE_ARG_POINTER(aCharacter);
  *aCharacter = L'\0';

  if (!mIntl) return NS_ERROR_FAILURE;

  if (mIntl->IsLocal()) {
    *aCharacter = Intl()->CharAt(aOffset);
  } else {
#if defined(XP_WIN)
    return NS_ERROR_NOT_IMPLEMENTED;
#else
    *aCharacter = mIntl->AsRemote()->CharAt(aOffset);
#endif
  }
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetTextAttributes(
    bool aIncludeDefAttrs, int32_t aOffset, int32_t* aStartOffset,
    int32_t* aEndOffset, nsIPersistentProperties** aAttributes) {
  NS_ENSURE_ARG_POINTER(aStartOffset);
  NS_ENSURE_ARG_POINTER(aEndOffset);
  NS_ENSURE_ARG_POINTER(aAttributes);
  *aStartOffset = *aEndOffset = 0;
  *aAttributes = nullptr;

  if (!mIntl) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPersistentProperties> props;
  if (mIntl->IsLocal()) {
    props = Intl()->TextAttributes(aIncludeDefAttrs, aOffset, aStartOffset,
                                   aEndOffset);
  } else {
#if defined(XP_WIN)
    return NS_ERROR_NOT_IMPLEMENTED;
#else
    AutoTArray<Attribute, 10> attrs;
    mIntl->AsRemote()->TextAttributes(aIncludeDefAttrs, aOffset, &attrs,
                                      aStartOffset, aEndOffset);
    uint32_t attrCount = attrs.Length();
    nsAutoString unused;
    for (uint32_t i = 0; i < attrCount; i++) {
      props->SetStringProperty(attrs[i].Name(), attrs[i].Value(), unused);
    }
#endif
  }
  props.forget(aAttributes);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetDefaultTextAttributes(
    nsIPersistentProperties** aAttributes) {
  NS_ENSURE_ARG_POINTER(aAttributes);
  *aAttributes = nullptr;

  if (!mIntl) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPersistentProperties> props;
  if (mIntl->IsLocal()) {
    props = Intl()->DefaultTextAttributes();
  } else {
#if defined(XP_WIN)
    return NS_ERROR_NOT_IMPLEMENTED;
#else
    AutoTArray<Attribute, 10> attrs;
    mIntl->AsRemote()->DefaultTextAttributes(&attrs);
    uint32_t attrCount = attrs.Length();
    nsAutoString unused;
    for (uint32_t i = 0; i < attrCount; i++) {
      props->SetStringProperty(attrs[i].Name(), attrs[i].Value(), unused);
    }
#endif
  }
  props.forget(aAttributes);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetCharacterExtents(int32_t aOffset, int32_t* aX,
                                            int32_t* aY, int32_t* aWidth,
                                            int32_t* aHeight,
                                            uint32_t aCoordType) {
  NS_ENSURE_ARG_POINTER(aX);
  NS_ENSURE_ARG_POINTER(aY);
  NS_ENSURE_ARG_POINTER(aWidth);
  NS_ENSURE_ARG_POINTER(aHeight);
  *aX = *aY = *aWidth = *aHeight;

  if (!mIntl) return NS_ERROR_FAILURE;

  nsIntRect rect;
  if (mIntl->IsLocal()) {
    rect = Intl()->CharBounds(aOffset, aCoordType);
  } else {
#if defined(XP_WIN)
    return NS_ERROR_NOT_IMPLEMENTED;
#else
    rect = mIntl->AsRemote()->CharBounds(aOffset, aCoordType);
#endif
  }
  rect.GetRect(aX, aY, aWidth, aHeight);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetRangeExtents(int32_t aStartOffset,
                                        int32_t aEndOffset, int32_t* aX,
                                        int32_t* aY, int32_t* aWidth,
                                        int32_t* aHeight, uint32_t aCoordType) {
  NS_ENSURE_ARG_POINTER(aX);
  NS_ENSURE_ARG_POINTER(aY);
  NS_ENSURE_ARG_POINTER(aWidth);
  NS_ENSURE_ARG_POINTER(aHeight);
  *aX = *aY = *aWidth = *aHeight = 0;

  if (!mIntl) return NS_ERROR_FAILURE;

  nsIntRect rect;
  if (mIntl->IsLocal()) {
    rect = Intl()->TextBounds(aStartOffset, aEndOffset, aCoordType);
  } else {
#if defined(XP_WIN)
    return NS_ERROR_NOT_IMPLEMENTED;
#else
    rect = mIntl->AsRemote()->TextBounds(aStartOffset, aEndOffset, aCoordType);
#endif
  }
  rect.GetRect(aX, aY, aWidth, aHeight);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetOffsetAtPoint(int32_t aX, int32_t aY,
                                         uint32_t aCoordType,
                                         int32_t* aOffset) {
  NS_ENSURE_ARG_POINTER(aOffset);
  *aOffset = -1;

  if (!mIntl) return NS_ERROR_FAILURE;

  if (mIntl->IsLocal()) {
    *aOffset = Intl()->OffsetAtPoint(aX, aY, aCoordType);
  } else {
    *aOffset = mIntl->AsRemote()->OffsetAtPoint(aX, aY, aCoordType);
  }
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetCaretOffset(int32_t* aCaretOffset) {
  NS_ENSURE_ARG_POINTER(aCaretOffset);
  *aCaretOffset = -1;

  if (!mIntl) return NS_ERROR_FAILURE;

  if (mIntl->IsLocal()) {
    *aCaretOffset = Intl()->CaretOffset();
  } else {
    *aCaretOffset = mIntl->AsRemote()->CaretOffset();
  }
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::SetCaretOffset(int32_t aCaretOffset) {
  if (!mIntl) return NS_ERROR_FAILURE;

  if (mIntl->IsLocal()) {
    Intl()->SetCaretOffset(aCaretOffset);
  } else {
    mIntl->AsRemote()->SetCaretOffset(aCaretOffset);
  }
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetSelectionCount(int32_t* aSelectionCount) {
  NS_ENSURE_ARG_POINTER(aSelectionCount);
  *aSelectionCount = 0;

  if (!mIntl) return NS_ERROR_FAILURE;

  if (mIntl->IsLocal()) {
    *aSelectionCount = Intl()->SelectionCount();
  } else {
#if defined(XP_WIN)
    return NS_ERROR_NOT_IMPLEMENTED;
#else
    *aSelectionCount = mIntl->AsRemote()->SelectionCount();
#endif
  }
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetSelectionBounds(int32_t aSelectionNum,
                                           int32_t* aStartOffset,
                                           int32_t* aEndOffset) {
  NS_ENSURE_ARG_POINTER(aStartOffset);
  NS_ENSURE_ARG_POINTER(aEndOffset);
  *aStartOffset = *aEndOffset = 0;

  if (!mIntl) return NS_ERROR_FAILURE;

  if (aSelectionNum < 0) return NS_ERROR_INVALID_ARG;

  if (mIntl->IsLocal()) {
    if (aSelectionNum >= Intl()->SelectionCount()) return NS_ERROR_INVALID_ARG;

    Intl()->SelectionBoundsAt(aSelectionNum, aStartOffset, aEndOffset);
  } else {
#if defined(XP_WIN)
    return NS_ERROR_NOT_IMPLEMENTED;
#else
    nsString unused;
    mIntl->AsRemote()->SelectionBoundsAt(aSelectionNum, unused, aStartOffset,
                                         aEndOffset);
#endif
  }
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::SetSelectionBounds(int32_t aSelectionNum,
                                           int32_t aStartOffset,
                                           int32_t aEndOffset) {
  if (!mIntl) return NS_ERROR_FAILURE;

  if (aSelectionNum < 0) return NS_ERROR_INVALID_ARG;

  if (mIntl->IsLocal()) {
    if (!Intl()->SetSelectionBoundsAt(aSelectionNum, aStartOffset,
                                      aEndOffset)) {
      return NS_ERROR_INVALID_ARG;
    }
  } else {
#if defined(XP_WIN)
    return NS_ERROR_NOT_IMPLEMENTED;
#else
    if (!mIntl->AsRemote()->SetSelectionBoundsAt(aSelectionNum, aStartOffset,
                                                 aEndOffset)) {
      return NS_ERROR_INVALID_ARG;
    }
#endif
  }
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::AddSelection(int32_t aStartOffset, int32_t aEndOffset) {
  if (!mIntl) return NS_ERROR_FAILURE;

  if (mIntl->IsLocal()) {
    Intl()->AddToSelection(aStartOffset, aEndOffset);
  } else {
    mIntl->AsRemote()->AddToSelection(aStartOffset, aEndOffset);
  }
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::RemoveSelection(int32_t aSelectionNum) {
  if (!mIntl) return NS_ERROR_FAILURE;

  if (mIntl->IsLocal()) {
    Intl()->RemoveFromSelection(aSelectionNum);
  } else {
    mIntl->AsRemote()->RemoveFromSelection(aSelectionNum);
  }
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::ScrollSubstringTo(int32_t aStartOffset,
                                          int32_t aEndOffset,
                                          uint32_t aScrollType) {
  if (!mIntl) return NS_ERROR_FAILURE;

  if (mIntl->IsLocal()) {
    Intl()->ScrollSubstringTo(aStartOffset, aEndOffset, aScrollType);
  } else {
    mIntl->AsRemote()->ScrollSubstringTo(aStartOffset, aEndOffset, aScrollType);
  }
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::ScrollSubstringToPoint(int32_t aStartOffset,
                                               int32_t aEndOffset,
                                               uint32_t aCoordinateType,
                                               int32_t aX, int32_t aY) {
  if (!mIntl) return NS_ERROR_FAILURE;

  if (mIntl->IsLocal()) {
    Intl()->ScrollSubstringToPoint(aStartOffset, aEndOffset, aCoordinateType,
                                   aX, aY);
  } else {
    mIntl->AsRemote()->ScrollSubstringToPoint(aStartOffset, aEndOffset,
                                              aCoordinateType, aX, aY);
  }
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetEnclosingRange(nsIAccessibleTextRange** aRange) {
  NS_ENSURE_ARG_POINTER(aRange);
  *aRange = nullptr;

  if (!Intl()) return NS_ERROR_FAILURE;

  RefPtr<xpcAccessibleTextRange> range = new xpcAccessibleTextRange;
  Intl()->EnclosingRange(range->mRange);
  NS_ASSERTION(range->mRange.IsValid(),
               "Should always have an enclosing range!");

  range.forget(aRange);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetSelectionRanges(nsIArray** aRanges) {
  NS_ENSURE_ARG_POINTER(aRanges);
  *aRanges = nullptr;

  if (!Intl()) return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  nsCOMPtr<nsIMutableArray> xpcRanges =
      do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  AutoTArray<TextRange, 1> ranges;
  Intl()->SelectionRanges(&ranges);
  uint32_t len = ranges.Length();
  for (uint32_t idx = 0; idx < len; idx++) {
    xpcRanges->AppendElement(
        new xpcAccessibleTextRange(std::move(ranges[idx])));
  }

  xpcRanges.forget(aRanges);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetVisibleRanges(nsIArray** aRanges) {
  NS_ENSURE_ARG_POINTER(aRanges);
  *aRanges = nullptr;

  if (!Intl()) return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  nsCOMPtr<nsIMutableArray> xpcRanges =
      do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsTArray<TextRange> ranges;
  Intl()->VisibleRanges(&ranges);
  uint32_t len = ranges.Length();
  for (uint32_t idx = 0; idx < len; idx++) {
    xpcRanges->AppendElement(
        new xpcAccessibleTextRange(std::move(ranges[idx])));
  }

  xpcRanges.forget(aRanges);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetRangeByChild(nsIAccessible* aChild,
                                        nsIAccessibleTextRange** aRange) {
  NS_ENSURE_ARG_POINTER(aRange);
  *aRange = nullptr;

  if (!Intl()) return NS_ERROR_FAILURE;

  LocalAccessible* child = aChild->ToInternalAccessible();
  if (child) {
    RefPtr<xpcAccessibleTextRange> range = new xpcAccessibleTextRange;
    Intl()->RangeByChild(child, range->mRange);
    if (range->mRange.IsValid()) range.forget(aRange);
  }

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetRangeAtPoint(int32_t aX, int32_t aY,
                                        nsIAccessibleTextRange** aRange) {
  NS_ENSURE_ARG_POINTER(aRange);
  *aRange = nullptr;

  if (!Intl()) return NS_ERROR_FAILURE;

  RefPtr<xpcAccessibleTextRange> range = new xpcAccessibleTextRange;
  Intl()->RangeAtPoint(aX, aY, range->mRange);
  if (range->mRange.IsValid()) range.forget(aRange);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibleEditableText

NS_IMETHODIMP
xpcAccessibleHyperText::SetTextContents(const nsAString& aText) {
  if (!mIntl) return NS_ERROR_FAILURE;

  if (mIntl->IsLocal()) {
    Intl()->ReplaceText(aText);
  } else {
#if defined(XP_WIN)
    return NS_ERROR_NOT_IMPLEMENTED;
#else
    nsString text(aText);
    mIntl->AsRemote()->ReplaceText(text);
#endif
  }
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::InsertText(const nsAString& aText, int32_t aOffset) {
  if (!mIntl) return NS_ERROR_FAILURE;

  if (mIntl->IsLocal()) {
    Intl()->InsertText(aText, aOffset);
  } else {
#if defined(XP_WIN)
    return NS_ERROR_NOT_IMPLEMENTED;
#else
    nsString text(aText);
    mIntl->AsRemote()->InsertText(text, aOffset);
#endif
  }
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::CopyText(int32_t aStartOffset, int32_t aEndOffset) {
  if (!mIntl) return NS_ERROR_FAILURE;

  if (mIntl->IsLocal()) {
    Intl()->CopyText(aStartOffset, aEndOffset);
  } else {
#if defined(XP_WIN)
    return NS_ERROR_NOT_IMPLEMENTED;
#else
    mIntl->AsRemote()->CopyText(aStartOffset, aEndOffset);
#endif
  }
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::CutText(int32_t aStartOffset, int32_t aEndOffset) {
  if (!mIntl) return NS_ERROR_FAILURE;

  if (mIntl->IsLocal()) {
    Intl()->CutText(aStartOffset, aEndOffset);
  } else {
#if defined(XP_WIN)
    return NS_ERROR_NOT_IMPLEMENTED;
#else
    mIntl->AsRemote()->CutText(aStartOffset, aEndOffset);
#endif
  }
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::DeleteText(int32_t aStartOffset, int32_t aEndOffset) {
  if (!mIntl) return NS_ERROR_FAILURE;

  if (mIntl->IsLocal()) {
    Intl()->DeleteText(aStartOffset, aEndOffset);
  } else {
#if defined(XP_WIN)
    return NS_ERROR_NOT_IMPLEMENTED;
#else
    mIntl->AsRemote()->DeleteText(aStartOffset, aEndOffset);
#endif
  }
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::PasteText(int32_t aOffset) {
  if (!mIntl) return NS_ERROR_FAILURE;

  if (mIntl->IsLocal()) {
    RefPtr<HyperTextAccessible> acc = Intl();
    acc->PasteText(aOffset);
  } else {
#if defined(XP_WIN)
    return NS_ERROR_NOT_IMPLEMENTED;
#else
    mIntl->AsRemote()->PasteText(aOffset);
#endif
  }
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibleHyperText

NS_IMETHODIMP
xpcAccessibleHyperText::GetLinkCount(int32_t* aLinkCount) {
  NS_ENSURE_ARG_POINTER(aLinkCount);
  *aLinkCount = 0;

  if (!mIntl) return NS_ERROR_FAILURE;

  if (mIntl->IsLocal()) {
    *aLinkCount = Intl()->LinkCount();
  } else {
#if defined(XP_WIN)
    return NS_ERROR_NOT_IMPLEMENTED;
#else
    *aLinkCount = mIntl->AsRemote()->LinkCount();
#endif
  }
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetLinkAt(int32_t aIndex,
                                  nsIAccessibleHyperLink** aLink) {
  NS_ENSURE_ARG_POINTER(aLink);
  *aLink = nullptr;

  if (!mIntl) return NS_ERROR_FAILURE;

  if (mIntl->IsLocal()) {
    NS_IF_ADDREF(*aLink = ToXPC(Intl()->LinkAt(aIndex)));
  } else {
#if defined(XP_WIN)
    return NS_ERROR_NOT_IMPLEMENTED;
#else
    NS_IF_ADDREF(*aLink = ToXPC(mIntl->AsRemote()->LinkAt(aIndex)));
#endif
  }
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetLinkIndex(nsIAccessibleHyperLink* aLink,
                                     int32_t* aIndex) {
  NS_ENSURE_ARG_POINTER(aLink);
  NS_ENSURE_ARG_POINTER(aIndex);
  *aIndex = -1;

  if (!mIntl) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAccessible> xpcLink(do_QueryInterface(aLink));
  if (LocalAccessible* accLink = xpcLink->ToInternalAccessible()) {
    *aIndex = Intl()->LinkIndexOf(accLink);
  } else {
#if defined(XP_WIN)
    return NS_ERROR_NOT_IMPLEMENTED;
#else
    xpcAccessibleHyperText* linkHyperText =
        static_cast<xpcAccessibleHyperText*>(xpcLink.get());
    RemoteAccessible* proxyLink = linkHyperText->mIntl->AsRemote();
    if (proxyLink) {
      *aIndex = mIntl->AsRemote()->LinkIndexOf(proxyLink);
    }
#endif
  }

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetLinkIndexAtOffset(int32_t aOffset,
                                             int32_t* aLinkIndex) {
  NS_ENSURE_ARG_POINTER(aLinkIndex);
  *aLinkIndex = -1;  // API says this magic value means 'not found'

  if (!mIntl) return NS_ERROR_FAILURE;

  if (mIntl->IsLocal()) {
    *aLinkIndex = Intl()->LinkIndexAtOffset(aOffset);
  } else {
#if defined(XP_WIN)
    return NS_ERROR_NOT_IMPLEMENTED;
#else
    *aLinkIndex = mIntl->AsRemote()->LinkIndexAtOffset(aOffset);
#endif
  }
  return NS_OK;
}
