/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ia2AccessibleText.h"

#include "Accessible2.h"
#include "AccessibleText_i.c"

#include "HyperTextAccessibleWrap.h"

#include "nsIPersistentProperties2.h"
#include "nsIAccessibleTypes.h"

using namespace mozilla::a11y;

// IAccessibleText

STDMETHODIMP
ia2AccessibleText::addSelection(long aStartOffset, long aEndOffset)
{
  A11Y_TRYBLOCK_BEGIN

  HyperTextAccessible* textAcc = static_cast<HyperTextAccessibleWrap*>(this);
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsresult rv = textAcc->AddSelection(aStartOffset, aEndOffset);
  return GetHRESULT(rv);

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleText::get_attributes(long aOffset, long *aStartOffset,
                                  long *aEndOffset, BSTR *aTextAttributes)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aStartOffset || !aEndOffset || !aTextAttributes)
    return E_INVALIDARG;

  *aStartOffset = 0;
  *aEndOffset = 0;
  *aTextAttributes = nullptr;

  HyperTextAccessible* textAcc = static_cast<HyperTextAccessibleWrap*>(this);
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  int32_t startOffset = 0, endOffset = 0;
  nsCOMPtr<nsIPersistentProperties> attributes;
  nsresult rv = textAcc->GetTextAttributes(true, aOffset,
                                           &startOffset, &endOffset,
                                           getter_AddRefs(attributes));
  if (NS_FAILED(rv))
    return GetHRESULT(rv);
  
  HRESULT hr = AccessibleWrap::ConvertToIA2Attributes(attributes,
                                                      aTextAttributes);
  if (FAILED(hr))
    return hr;

  *aStartOffset = startOffset;
  *aEndOffset = endOffset;

  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleText::get_caretOffset(long *aOffset)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aOffset)
    return E_INVALIDARG;

  *aOffset = -1;

  HyperTextAccessible* textAcc = static_cast<HyperTextAccessibleWrap*>(this);
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  int32_t offset = 0;
  nsresult rv = textAcc->GetCaretOffset(&offset);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aOffset = offset;
  return offset != -1 ? S_OK : S_FALSE;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleText::get_characterExtents(long aOffset,
                                        enum IA2CoordinateType aCoordType,
                                        long *aX, long *aY,
                                        long *aWidth, long *aHeight)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aX || !aY || !aWidth || !aHeight)
    return E_INVALIDARG;

  *aX = 0;
  *aY = 0;
  *aWidth = 0;
  *aHeight = 0;

  HyperTextAccessible* textAcc = static_cast<HyperTextAccessibleWrap*>(this);
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  uint32_t geckoCoordType = (aCoordType == IA2_COORDTYPE_SCREEN_RELATIVE) ?
    nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE :
    nsIAccessibleCoordinateType::COORDTYPE_PARENT_RELATIVE;

  int32_t x = 0, y =0, width = 0, height = 0;
  nsresult rv = textAcc->GetCharacterExtents (aOffset, &x, &y, &width, &height,
                                              geckoCoordType);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aX = x;
  *aY = y;
  *aWidth = width;
  *aHeight = height;
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleText::get_nSelections(long *aNSelections)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aNSelections)
    return E_INVALIDARG;

  *aNSelections = 0;

  HyperTextAccessible* textAcc = static_cast<HyperTextAccessibleWrap*>(this);
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  int32_t selCount = 0;
  nsresult rv = textAcc->GetSelectionCount(&selCount);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aNSelections = selCount;
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleText::get_offsetAtPoint(long aX, long aY,
                                     enum IA2CoordinateType aCoordType,
                                     long *aOffset)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aOffset)
    return E_INVALIDARG;

  *aOffset = 0;

  HyperTextAccessible* textAcc = static_cast<HyperTextAccessibleWrap*>(this);
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  uint32_t geckoCoordType = (aCoordType == IA2_COORDTYPE_SCREEN_RELATIVE) ?
    nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE :
    nsIAccessibleCoordinateType::COORDTYPE_PARENT_RELATIVE;

  int32_t offset = 0;
  nsresult rv = textAcc->GetOffsetAtPoint(aX, aY, geckoCoordType, &offset);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aOffset = offset;
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleText::get_selection(long aSelectionIndex, long *aStartOffset,
                                 long *aEndOffset)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aStartOffset || !aEndOffset)
    return E_INVALIDARG;

  *aStartOffset = 0;
  *aEndOffset = 0;

  HyperTextAccessible* textAcc = static_cast<HyperTextAccessibleWrap*>(this);
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  int32_t startOffset = 0, endOffset = 0;
  nsresult rv = textAcc->GetSelectionBounds(aSelectionIndex,
                                            &startOffset, &endOffset);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aStartOffset = startOffset;
  *aEndOffset = endOffset;
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleText::get_text(long aStartOffset, long aEndOffset, BSTR *aText)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aText)
    return E_INVALIDARG;

  *aText = nullptr;

  HyperTextAccessible* textAcc = static_cast<HyperTextAccessibleWrap*>(this);
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsAutoString text;
  nsresult rv = textAcc->GetText(aStartOffset, aEndOffset, text);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  if (text.IsEmpty())
    return S_FALSE;

  *aText = ::SysAllocStringLen(text.get(), text.Length());
  return *aText ? S_OK : E_OUTOFMEMORY;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleText::get_textBeforeOffset(long aOffset,
                                        enum IA2TextBoundaryType aBoundaryType,
                                        long *aStartOffset, long *aEndOffset,
                                        BSTR *aText)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aStartOffset || !aEndOffset || !aText)
    return E_INVALIDARG;

  *aStartOffset = 0;
  *aEndOffset = 0;
  *aText = nullptr;

  HyperTextAccessible* textAcc = static_cast<HyperTextAccessibleWrap*>(this);
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsresult rv = NS_OK;
  nsAutoString text;
  int32_t startOffset = 0, endOffset = 0;

  if (aBoundaryType == IA2_TEXT_BOUNDARY_ALL) {
    startOffset = 0;
    endOffset = textAcc->CharacterCount();
    rv = textAcc->GetText(startOffset, endOffset, text);
  } else {
    AccessibleTextBoundary boundaryType = GetGeckoTextBoundary(aBoundaryType);
    if (boundaryType == -1)
      return S_FALSE;
    rv = textAcc->GetTextBeforeOffset(aOffset, boundaryType,
                                      &startOffset, &endOffset, text);
  }

  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aStartOffset = startOffset;
  *aEndOffset = endOffset;

  if (text.IsEmpty())
    return S_FALSE;

  *aText = ::SysAllocStringLen(text.get(), text.Length());
  return *aText ? S_OK : E_OUTOFMEMORY;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleText::get_textAfterOffset(long aOffset,
                                       enum IA2TextBoundaryType aBoundaryType,
                                       long *aStartOffset, long *aEndOffset,
                                       BSTR *aText)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aStartOffset || !aEndOffset || !aText)
    return E_INVALIDARG;

  *aStartOffset = 0;
  *aEndOffset = 0;
  *aText = nullptr;

  HyperTextAccessible* textAcc = static_cast<HyperTextAccessibleWrap*>(this);
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsresult rv = NS_OK;
  nsAutoString text;
  int32_t startOffset = 0, endOffset = 0;

  if (aBoundaryType == IA2_TEXT_BOUNDARY_ALL) {
    startOffset = 0;
    endOffset = textAcc->CharacterCount();
    rv = textAcc->GetText(startOffset, endOffset, text);
  } else {
    AccessibleTextBoundary boundaryType = GetGeckoTextBoundary(aBoundaryType);
    if (boundaryType == -1)
      return S_FALSE;
    rv = textAcc->GetTextAfterOffset(aOffset, boundaryType,
                                     &startOffset, &endOffset, text);
  }

  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aStartOffset = startOffset;
  *aEndOffset = endOffset;

  if (text.IsEmpty())
    return S_FALSE;

  *aText = ::SysAllocStringLen(text.get(), text.Length());
  return *aText ? S_OK : E_OUTOFMEMORY;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleText::get_textAtOffset(long aOffset,
                                    enum IA2TextBoundaryType aBoundaryType,
                                    long *aStartOffset, long *aEndOffset,
                                    BSTR *aText)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aStartOffset || !aEndOffset || !aText)
    return E_INVALIDARG;

  *aStartOffset = 0;
  *aEndOffset = 0;
  *aText = nullptr;

  HyperTextAccessible* textAcc = static_cast<HyperTextAccessibleWrap*>(this);
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsresult rv = NS_OK;
  nsAutoString text;
  int32_t startOffset = 0, endOffset = 0;

  if (aBoundaryType == IA2_TEXT_BOUNDARY_ALL) {
    startOffset = 0;
    endOffset = textAcc->CharacterCount();
    rv = textAcc->GetText(startOffset, endOffset, text);
  } else {
    AccessibleTextBoundary boundaryType = GetGeckoTextBoundary(aBoundaryType);
    if (boundaryType == -1)
      return S_FALSE;
    rv = textAcc->GetTextAtOffset(aOffset, boundaryType,
                                  &startOffset, &endOffset, text);
  }

  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aStartOffset = startOffset;
  *aEndOffset = endOffset;

  if (text.IsEmpty())
    return S_FALSE;

  *aText = ::SysAllocStringLen(text.get(), text.Length());
  return *aText ? S_OK : E_OUTOFMEMORY;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleText::removeSelection(long aSelectionIndex)
{
  A11Y_TRYBLOCK_BEGIN

  HyperTextAccessible* textAcc = static_cast<HyperTextAccessibleWrap*>(this);
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsresult rv = textAcc->RemoveSelection(aSelectionIndex);
  return GetHRESULT(rv);

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleText::setCaretOffset(long aOffset)
{
  A11Y_TRYBLOCK_BEGIN

  HyperTextAccessible* textAcc = static_cast<HyperTextAccessibleWrap*>(this);
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsresult rv = textAcc->SetCaretOffset(aOffset);
  return GetHRESULT(rv);

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleText::setSelection(long aSelectionIndex, long aStartOffset,
                                long aEndOffset)
{
  A11Y_TRYBLOCK_BEGIN

  HyperTextAccessible* textAcc = static_cast<HyperTextAccessibleWrap*>(this);
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsresult rv = textAcc->SetSelectionBounds(aSelectionIndex,
                                            aStartOffset, aEndOffset);
  return GetHRESULT(rv);

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleText::get_nCharacters(long *aNCharacters)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aNCharacters)
    return E_INVALIDARG;

  *aNCharacters = 0;

  HyperTextAccessible* textAcc = static_cast<HyperTextAccessibleWrap*>(this);
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  *aNCharacters  = textAcc->CharacterCount();
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleText::scrollSubstringTo(long aStartIndex, long aEndIndex,
                                     enum IA2ScrollType aScrollType)
{
  A11Y_TRYBLOCK_BEGIN

  HyperTextAccessible* textAcc = static_cast<HyperTextAccessibleWrap*>(this);
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsresult rv = textAcc->ScrollSubstringTo(aStartIndex, aEndIndex, aScrollType);
  return GetHRESULT(rv);

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleText::scrollSubstringToPoint(long aStartIndex, long aEndIndex,
                                          enum IA2CoordinateType aCoordType,
                                          long aX, long aY)
{
  A11Y_TRYBLOCK_BEGIN

  HyperTextAccessible* textAcc = static_cast<HyperTextAccessibleWrap*>(this);
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  uint32_t geckoCoordType = (aCoordType == IA2_COORDTYPE_SCREEN_RELATIVE) ?
    nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE :
    nsIAccessibleCoordinateType::COORDTYPE_PARENT_RELATIVE;

  nsresult rv = textAcc->ScrollSubstringToPoint(aStartIndex, aEndIndex,
                                                geckoCoordType, aX, aY);
  return GetHRESULT(rv);

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleText::get_newText(IA2TextSegment *aNewText)
{
  A11Y_TRYBLOCK_BEGIN

  return GetModifiedText(true, aNewText);

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleText::get_oldText(IA2TextSegment *aOldText)
{
  A11Y_TRYBLOCK_BEGIN

  return GetModifiedText(false, aOldText);

  A11Y_TRYBLOCK_END
}

// ia2AccessibleText

HRESULT
ia2AccessibleText::GetModifiedText(bool aGetInsertedText,
                                   IA2TextSegment *aText)
{
  if (!aText)
    return E_INVALIDARG;

  uint32_t startOffset = 0, endOffset = 0;
  nsAutoString text;

  nsresult rv = GetModifiedText(aGetInsertedText, text,
                                &startOffset, &endOffset);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  aText->start = startOffset;
  aText->end = endOffset;

  if (text.IsEmpty())
    return S_FALSE;

  aText->text = ::SysAllocStringLen(text.get(), text.Length());
  return aText->text ? S_OK : E_OUTOFMEMORY;
}

AccessibleTextBoundary
ia2AccessibleText::GetGeckoTextBoundary(enum IA2TextBoundaryType aBoundaryType)
{
  switch (aBoundaryType) {
    case IA2_TEXT_BOUNDARY_CHAR:
      return nsIAccessibleText::BOUNDARY_CHAR;
    case IA2_TEXT_BOUNDARY_WORD:
      return nsIAccessibleText::BOUNDARY_WORD_START;
    case IA2_TEXT_BOUNDARY_LINE:
      return nsIAccessibleText::BOUNDARY_LINE_START;
    //case IA2_TEXT_BOUNDARY_SENTENCE:
    //case IA2_TEXT_BOUNDARY_PARAGRAPH:
      // XXX: not implemented
    default:
      return -1;
  }
}

