/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CAccessibleText.h"

#include "Accessible2.h"
#include "AccessibleText_i.c"

#include "nsHyperTextAccessible.h"

#include "nsIPersistentProperties2.h"

// IUnknown

STDMETHODIMP
CAccessibleText::QueryInterface(REFIID iid, void** ppv)
{
  *ppv = NULL;

  if (IID_IAccessibleText == iid) {
    nsCOMPtr<nsIAccessibleText> textAcc(do_QueryObject(this));
    if (!textAcc) {
      return E_NOINTERFACE;
    }
    *ppv = static_cast<IAccessibleText*>(this);
    (reinterpret_cast<IUnknown*>(*ppv))->AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

// IAccessibleText

STDMETHODIMP
CAccessibleText::addSelection(long aStartOffset, long aEndOffset)
{
__try {
  nsRefPtr<nsHyperTextAccessible> textAcc(do_QueryObject(this));
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsresult rv = textAcc->AddSelection(aStartOffset, aEndOffset);
  return GetHRESULT(rv);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleText::get_attributes(long aOffset, long *aStartOffset,
                                long *aEndOffset, BSTR *aTextAttributes)
{
__try {
  if (!aStartOffset || !aEndOffset || !aTextAttributes)
    return E_INVALIDARG;

  *aStartOffset = 0;
  *aEndOffset = 0;
  *aTextAttributes = NULL;

  nsRefPtr<nsHyperTextAccessible> textAcc(do_QueryObject(this));
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  PRInt32 startOffset = 0, endOffset = 0;
  nsCOMPtr<nsIPersistentProperties> attributes;
  nsresult rv = textAcc->GetTextAttributes(true, aOffset,
                                           &startOffset, &endOffset,
                                           getter_AddRefs(attributes));
  if (NS_FAILED(rv))
    return GetHRESULT(rv);
  
  HRESULT hr = nsAccessibleWrap::ConvertToIA2Attributes(attributes,
                                                        aTextAttributes);
  if (FAILED(hr))
    return hr;

  *aStartOffset = startOffset;
  *aEndOffset = endOffset;

  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleText::get_caretOffset(long *aOffset)
{
__try {
  *aOffset = -1;

  nsRefPtr<nsHyperTextAccessible> textAcc(do_QueryObject(this));
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  PRInt32 offset = 0;
  nsresult rv = textAcc->GetCaretOffset(&offset);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aOffset = offset;
  return offset != -1 ? S_OK : S_FALSE;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleText::get_characterExtents(long aOffset,
                                      enum IA2CoordinateType aCoordType,
                                      long *aX, long *aY,
                                      long *aWidth, long *aHeight)
{
__try {
  *aX = 0;
  *aY = 0;
  *aWidth = 0;
  *aHeight = 0;

  nsRefPtr<nsHyperTextAccessible> textAcc(do_QueryObject(this));
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  PRUint32 geckoCoordType = (aCoordType == IA2_COORDTYPE_SCREEN_RELATIVE) ?
    nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE :
    nsIAccessibleCoordinateType::COORDTYPE_PARENT_RELATIVE;

  PRInt32 x = 0, y =0, width = 0, height = 0;
  nsresult rv = textAcc->GetCharacterExtents (aOffset, &x, &y, &width, &height,
                                              geckoCoordType);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aX = x;
  *aY = y;
  *aWidth = width;
  *aHeight = height;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleText::get_nSelections(long *aNSelections)
{
__try {
  *aNSelections = 0;

  nsRefPtr<nsHyperTextAccessible> textAcc(do_QueryObject(this));
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  PRInt32 selCount = 0;
  nsresult rv = textAcc->GetSelectionCount(&selCount);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aNSelections = selCount;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleText::get_offsetAtPoint(long aX, long aY,
                                   enum IA2CoordinateType aCoordType,
                                   long *aOffset)
{
__try {
  *aOffset = 0;

  nsRefPtr<nsHyperTextAccessible> textAcc(do_QueryObject(this));
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  PRUint32 geckoCoordType = (aCoordType == IA2_COORDTYPE_SCREEN_RELATIVE) ?
    nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE :
    nsIAccessibleCoordinateType::COORDTYPE_PARENT_RELATIVE;

  PRInt32 offset = 0;
  nsresult rv = textAcc->GetOffsetAtPoint(aX, aY, geckoCoordType, &offset);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aOffset = offset;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleText::get_selection(long aSelectionIndex, long *aStartOffset,
                               long *aEndOffset)
{
__try {
  *aStartOffset = 0;
  *aEndOffset = 0;

  nsRefPtr<nsHyperTextAccessible> textAcc(do_QueryObject(this));
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  PRInt32 startOffset = 0, endOffset = 0;
  nsresult rv = textAcc->GetSelectionBounds(aSelectionIndex,
                                            &startOffset, &endOffset);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aStartOffset = startOffset;
  *aEndOffset = endOffset;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleText::get_text(long aStartOffset, long aEndOffset, BSTR *aText)
{
__try {
  *aText = NULL;

  nsRefPtr<nsHyperTextAccessible> textAcc(do_QueryObject(this));
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

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleText::get_textBeforeOffset(long aOffset,
                                      enum IA2TextBoundaryType aBoundaryType,
                                      long *aStartOffset, long *aEndOffset,
                                      BSTR *aText)
{
__try {
  *aStartOffset = 0;
  *aEndOffset = 0;
  *aText = NULL;

  nsRefPtr<nsHyperTextAccessible> textAcc(do_QueryObject(this));
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsresult rv = NS_OK;
  nsAutoString text;
  PRInt32 startOffset = 0, endOffset = 0;

  if (aBoundaryType == IA2_TEXT_BOUNDARY_ALL) {
    startOffset = 0;
    endOffset = textAcc->CharacterCount();
    rv = textAcc->GetText(startOffset, endOffset, text);
  } else {
    nsAccessibleTextBoundary boundaryType = GetGeckoTextBoundary(aBoundaryType);
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

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleText::get_textAfterOffset(long aOffset,
                                     enum IA2TextBoundaryType aBoundaryType,
                                     long *aStartOffset, long *aEndOffset,
                                     BSTR *aText)
{
__try {
  *aStartOffset = 0;
  *aEndOffset = 0;
  *aText = NULL;

  nsRefPtr<nsHyperTextAccessible> textAcc(do_QueryObject(this));
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsresult rv = NS_OK;
  nsAutoString text;
  PRInt32 startOffset = 0, endOffset = 0;

  if (aBoundaryType == IA2_TEXT_BOUNDARY_ALL) {
    startOffset = 0;
    endOffset = textAcc->CharacterCount();
    rv = textAcc->GetText(startOffset, endOffset, text);
  } else {
    nsAccessibleTextBoundary boundaryType = GetGeckoTextBoundary(aBoundaryType);
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

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleText::get_textAtOffset(long aOffset,
                                  enum IA2TextBoundaryType aBoundaryType,
                                  long *aStartOffset, long *aEndOffset,
                                  BSTR *aText)
{
__try {
  *aStartOffset = 0;
  *aEndOffset = 0;
  *aText = NULL;

  nsRefPtr<nsHyperTextAccessible> textAcc(do_QueryObject(this));
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsresult rv = NS_OK;
  nsAutoString text;
  PRInt32 startOffset = 0, endOffset = 0;

  if (aBoundaryType == IA2_TEXT_BOUNDARY_ALL) {
    startOffset = 0;
    endOffset = textAcc->CharacterCount();
    rv = textAcc->GetText(startOffset, endOffset, text);
  } else {
    nsAccessibleTextBoundary boundaryType = GetGeckoTextBoundary(aBoundaryType);
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

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleText::removeSelection(long aSelectionIndex)
{
__try {
  nsRefPtr<nsHyperTextAccessible> textAcc(do_QueryObject(this));
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsresult rv = textAcc->RemoveSelection(aSelectionIndex);
  return GetHRESULT(rv);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleText::setCaretOffset(long aOffset)
{
__try {
  nsRefPtr<nsHyperTextAccessible> textAcc(do_QueryObject(this));
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsresult rv = textAcc->SetCaretOffset(aOffset);
  return GetHRESULT(rv);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleText::setSelection(long aSelectionIndex, long aStartOffset,
                              long aEndOffset)
{
__try {
  nsRefPtr<nsHyperTextAccessible> textAcc(do_QueryObject(this));
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsresult rv = textAcc->SetSelectionBounds(aSelectionIndex,
                                            aStartOffset, aEndOffset);
  return GetHRESULT(rv);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleText::get_nCharacters(long *aNCharacters)
{
__try {
  *aNCharacters = 0;

  nsRefPtr<nsHyperTextAccessible> textAcc(do_QueryObject(this));
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  *aNCharacters  = textAcc->CharacterCount();
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleText::scrollSubstringTo(long aStartIndex, long aEndIndex,
                                   enum IA2ScrollType aScrollType)
{
__try {
  nsRefPtr<nsHyperTextAccessible> textAcc(do_QueryObject(this));
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsresult rv = textAcc->ScrollSubstringTo(aStartIndex, aEndIndex, aScrollType);
  return GetHRESULT(rv);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleText::scrollSubstringToPoint(long aStartIndex, long aEndIndex,
                                        enum IA2CoordinateType aCoordType,
                                        long aX, long aY)
{
__try {
  nsRefPtr<nsHyperTextAccessible> textAcc(do_QueryObject(this));
  if (textAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  PRUint32 geckoCoordType = (aCoordType == IA2_COORDTYPE_SCREEN_RELATIVE) ?
    nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE :
    nsIAccessibleCoordinateType::COORDTYPE_PARENT_RELATIVE;

  nsresult rv = textAcc->ScrollSubstringToPoint(aStartIndex, aEndIndex,
                                                geckoCoordType, aX, aY);
  return GetHRESULT(rv);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleText::get_newText(IA2TextSegment *aNewText)
{
__try {
  return GetModifiedText(true, aNewText);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
CAccessibleText::get_oldText(IA2TextSegment *aOldText)
{
__try {
  return GetModifiedText(false, aOldText);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

// CAccessibleText

HRESULT
CAccessibleText::GetModifiedText(bool aGetInsertedText,
                                 IA2TextSegment *aText)
{
  PRUint32 startOffset = 0, endOffset = 0;
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

nsAccessibleTextBoundary
CAccessibleText::GetGeckoTextBoundary(enum IA2TextBoundaryType aBoundaryType)
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

