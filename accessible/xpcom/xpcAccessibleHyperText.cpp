/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcAccessibleHyperText.h"

#include "LocalAccessible-inl.h"
#include "HyperTextAccessible-inl.h"
#include "mozilla/a11y/PDocAccessible.h"
#include "mozilla/StaticPrefs_accessibility.h"
#include "TextRange.h"
#include "AccAttributes.h"
#include "nsComponentManagerUtils.h"
#include "nsPersistentProperties.h"
#include "xpcAccessibleDocument.h"
#include "xpcAccessibleTextRange.h"

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

#if defined(XP_WIN)
  if (mIntl->IsRemote() &&
      !StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
#endif

  *aCharacterCount = static_cast<int32_t>(Intl()->CharacterCount());
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetText(int32_t aStartOffset, int32_t aEndOffset,
                                nsAString& aText) {
  aText.Truncate();

  if (!mIntl) return NS_ERROR_FAILURE;

  Intl()->TextSubstring(aStartOffset, aEndOffset, aText);
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

  Intl()->TextBeforeOffset(aOffset, aBoundaryType, aStartOffset, aEndOffset,
                           aText);
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

  Intl()->TextAtOffset(aOffset, aBoundaryType, aStartOffset, aEndOffset, aText);
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

  Intl()->TextAfterOffset(aOffset, aBoundaryType, aStartOffset, aEndOffset,
                          aText);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetCharacterAtOffset(int32_t aOffset,
                                             char16_t* aCharacter) {
  NS_ENSURE_ARG_POINTER(aCharacter);
  *aCharacter = L'\0';

  if (!mIntl) return NS_ERROR_FAILURE;

  if (mIntl->IsLocal()) {
    *aCharacter = IntlLocal()->CharAt(aOffset);
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

  if (mIntl->IsRemote() &&
      !StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  RefPtr<AccAttributes> attributes = Intl()->TextAttributes(
      aIncludeDefAttrs, aOffset, aStartOffset, aEndOffset);
  RefPtr<nsPersistentProperties> props = new nsPersistentProperties();
  nsAutoString unused;
  for (auto iter : *attributes) {
    nsAutoString name;
    iter.NameAsString(name);

    nsAutoString value;
    iter.ValueAsString(value);

    props->SetStringProperty(NS_ConvertUTF16toUTF8(name), value, unused);
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

  if (mIntl->IsRemote() &&
      !StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  RefPtr<AccAttributes> attributes = Intl()->DefaultTextAttributes();
  RefPtr<nsPersistentProperties> props = new nsPersistentProperties();
  nsAutoString unused;
  for (auto iter : *attributes) {
    nsAutoString name;
    iter.NameAsString(name);

    nsAutoString value;
    iter.ValueAsString(value);

    props->SetStringProperty(NS_ConvertUTF16toUTF8(name), value, unused);
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

  LayoutDeviceIntRect rect;
  if (mIntl->IsLocal()) {
    rect = IntlLocal()->CharBounds(aOffset, aCoordType);
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

  LayoutDeviceIntRect rect;
  if (mIntl->IsLocal()) {
    rect = IntlLocal()->TextBounds(aStartOffset, aEndOffset, aCoordType);
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
    *aOffset = IntlLocal()->OffsetAtPoint(aX, aY, aCoordType);
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
    *aCaretOffset = IntlLocal()->CaretOffset();
  } else {
    *aCaretOffset = mIntl->AsRemote()->CaretOffset();
  }
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::SetCaretOffset(int32_t aCaretOffset) {
  if (!mIntl) return NS_ERROR_FAILURE;

  if (mIntl->IsLocal()) {
    IntlLocal()->SetCaretOffset(aCaretOffset);
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

#if defined(XP_WIN)
  if (mIntl->IsRemote() &&
      !StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
#endif

  *aSelectionCount = Intl()->SelectionCount();
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

  if (mIntl->IsLocal() ||
      StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    if (aSelectionNum >= Intl()->SelectionCount()) {
      return NS_ERROR_INVALID_ARG;
    }

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
    if (!IntlLocal()->SetSelectionBoundsAt(aSelectionNum, aStartOffset,
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
    IntlLocal()->AddToSelection(aStartOffset, aEndOffset);
  } else {
    mIntl->AsRemote()->AddToSelection(aStartOffset, aEndOffset);
  }
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::RemoveSelection(int32_t aSelectionNum) {
  if (!mIntl) return NS_ERROR_FAILURE;

  if (mIntl->IsLocal()) {
    IntlLocal()->RemoveFromSelection(aSelectionNum);
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
    IntlLocal()->ScrollSubstringTo(aStartOffset, aEndOffset, aScrollType);
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
    IntlLocal()->ScrollSubstringToPoint(aStartOffset, aEndOffset,
                                        aCoordinateType, aX, aY);
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

  if (!IntlLocal()) return NS_ERROR_FAILURE;

  TextRange range;
  IntlLocal()->EnclosingRange(range);
  NS_ASSERTION(range.IsValid(), "Should always have an enclosing range!");
  RefPtr<xpcAccessibleTextRange> xpcRange = new xpcAccessibleTextRange(range);

  xpcRange.forget(aRange);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetSelectionRanges(nsIArray** aRanges) {
  NS_ENSURE_ARG_POINTER(aRanges);
  *aRanges = nullptr;

  if (!IntlLocal() && !StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = NS_OK;
  nsCOMPtr<nsIMutableArray> xpcRanges =
      do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  AutoTArray<TextRange, 1> ranges;
  Intl()->SelectionRanges(&ranges);
  uint32_t len = ranges.Length();
  for (uint32_t idx = 0; idx < len; idx++) {
    xpcRanges->AppendElement(new xpcAccessibleTextRange(ranges[idx]));
  }

  xpcRanges.forget(aRanges);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetVisibleRanges(nsIArray** aRanges) {
  NS_ENSURE_ARG_POINTER(aRanges);
  *aRanges = nullptr;

  if (!IntlLocal()) return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  nsCOMPtr<nsIMutableArray> xpcRanges =
      do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsTArray<TextRange> ranges;
  IntlLocal()->VisibleRanges(&ranges);
  uint32_t len = ranges.Length();
  for (uint32_t idx = 0; idx < len; idx++) {
    xpcRanges->AppendElement(new xpcAccessibleTextRange(ranges[idx]));
  }

  xpcRanges.forget(aRanges);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetRangeByChild(nsIAccessible* aChild,
                                        nsIAccessibleTextRange** aRange) {
  NS_ENSURE_ARG_POINTER(aRange);
  *aRange = nullptr;

  if (!IntlLocal()) return NS_ERROR_FAILURE;

  LocalAccessible* child = aChild->ToInternalAccessible();
  if (child) {
    TextRange range;
    IntlLocal()->RangeByChild(child, range);
    if (range.IsValid()) {
      RefPtr<xpcAccessibleTextRange> xpcRange =
          new xpcAccessibleTextRange(range);
      xpcRange.forget(aRange);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetRangeAtPoint(int32_t aX, int32_t aY,
                                        nsIAccessibleTextRange** aRange) {
  NS_ENSURE_ARG_POINTER(aRange);
  *aRange = nullptr;

  if (!IntlLocal()) return NS_ERROR_FAILURE;

  TextRange range;
  IntlLocal()->RangeAtPoint(aX, aY, range);
  if (range.IsValid()) {
    RefPtr<xpcAccessibleTextRange> xpcRange = new xpcAccessibleTextRange(range);
    xpcRange.forget(aRange);
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibleEditableText

NS_IMETHODIMP
xpcAccessibleHyperText::SetTextContents(const nsAString& aText) {
  if (!mIntl) return NS_ERROR_FAILURE;

  if (mIntl->IsLocal()) {
    IntlLocal()->ReplaceText(aText);
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
    IntlLocal()->InsertText(aText, aOffset);
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
    IntlLocal()->CopyText(aStartOffset, aEndOffset);
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
    IntlLocal()->CutText(aStartOffset, aEndOffset);
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
    IntlLocal()->DeleteText(aStartOffset, aEndOffset);
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
    RefPtr<HyperTextAccessible> acc = IntlLocal();
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
    *aLinkCount = static_cast<int32_t>(IntlLocal()->LinkCount());
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

#if defined(XP_WIN)
  if (mIntl->IsRemote() &&
      !StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
#endif

  NS_IF_ADDREF(*aLink = ToXPC(Intl()->LinkAt(aIndex)));
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
  Accessible* accLink = xpcLink->ToInternalGeneric();
#if defined(XP_WIN)
  if (accLink->IsRemote() &&
      !StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
#endif
  *aIndex = Intl()->LinkIndexOf(accLink);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleHyperText::GetLinkIndexAtOffset(int32_t aOffset,
                                             int32_t* aLinkIndex) {
  NS_ENSURE_ARG_POINTER(aLinkIndex);
  *aLinkIndex = -1;  // API says this magic value means 'not found'

  if (!mIntl) return NS_ERROR_FAILURE;

#if defined(XP_WIN)
  if (mIntl->IsRemote() &&
      !StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
#endif

  *aLinkIndex = Intl()->LinkIndexAtOffset(aOffset);
  return NS_OK;
}
