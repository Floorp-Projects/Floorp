/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
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
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "CAccessibleText.h"

#include "Accessible2.h"
#include "AccessibleText_i.c"

#include "nsIAccessible.h"
#include "nsIAccessibleText.h"
#include "nsIAccessibleTypes.h"
#include "nsIWinAccessNode.h"

#include "nsCOMPtr.h"
#include "nsString.h"

#define GET_NSIACCESSIBLETEXT \
nsCOMPtr<nsIAccessibleText> textAcc(do_QueryInterface(this));\
NS_ASSERTION(textAcc,\
             "Subclass of CAccessibleText doesn't implement nsIAccessibleText");\
if (!textAcc)\
  return E_FAIL;\

// IUnknown

STDMETHODIMP
CAccessibleText::QueryInterface(REFIID iid, void** ppv)
{
  *ppv = NULL;

  if (IID_IAccessibleText == iid) {
    nsCOMPtr<nsIAccessibleText> textAcc(do_QueryInterface(this));
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
  GET_NSIACCESSIBLETEXT

  nsresult rv = textAcc->AddSelection(aStartOffset, aEndOffset);
  return NS_FAILED(rv) ? E_FAIL : S_OK;
}

STDMETHODIMP
CAccessibleText::get_attributes(long aOffset, long *aStartOffset,
                                long *aEndOffset, BSTR *aTextAttributes)
{
  GET_NSIACCESSIBLETEXT

  nsCOMPtr<nsIAccessible> accessible;
  PRInt32 startOffset = 0, endOffset = 0;
  textAcc->GetAttributeRange(aOffset, &startOffset, &endOffset,
                             getter_AddRefs(accessible));
  if (!accessible)
    return E_FAIL;

  nsCOMPtr<nsIWinAccessNode> winAccessNode(do_QueryInterface(accessible));
  if (!winAccessNode)
    return E_FAIL;

  void **instancePtr = 0;
  winAccessNode->QueryNativeInterface(IID_IAccessible2, instancePtr);
  if (!instancePtr)
    return E_FAIL;

  IAccessible2 *pAccessible2 = static_cast<IAccessible2*>(*instancePtr);
  HRESULT hr = pAccessible2->get_attributes(aTextAttributes);
  pAccessible2->Release();

  *aStartOffset = startOffset;
  *aEndOffset = endOffset;

  return hr;
}

STDMETHODIMP
CAccessibleText::get_caretOffset(long *aOffset)
{
  GET_NSIACCESSIBLETEXT

  PRInt32 offset = 0;
  nsresult rv = textAcc->GetCaretOffset(&offset);
  *aOffset = offset;

  return NS_FAILED(rv) ? E_FAIL : S_OK;
}

STDMETHODIMP
CAccessibleText::get_characterExtents(long aOffset,
                                      enum IA2CoordinateType aCoordType,
                                      long *aX, long *aY,
                                      long *aWidth, long *aHeight)
{
  GET_NSIACCESSIBLETEXT

  PRUint32 geckoCoordType = (aCoordType == IA2_COORDTYPE_SCREEN_RELATIVE) ?
    nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE :
    nsIAccessibleCoordinateType::COORDTYPE_PARENT_RELATIVE;

  PRInt32 x = 0, y =0, width = 0, height = 0;
  nsresult rv = textAcc->GetCharacterExtents (aOffset, &x, &y, &width, &height,
                                              geckoCoordType);
  *aX = x;
  *aY = y;
  *aWidth = width;
  *aHeight = height;

  return NS_FAILED(rv) ? E_FAIL : S_OK;
}

STDMETHODIMP
CAccessibleText::get_nSelections(long *aNSelections)
{
  GET_NSIACCESSIBLETEXT

  PRInt32 selCount = 0;
  nsresult rv = textAcc->GetSelectionCount(&selCount);
  *aNSelections = selCount;

  return NS_FAILED(rv) ? E_FAIL : S_OK;
}

STDMETHODIMP
CAccessibleText::get_offsetAtPoint(long aX, long aY,
                                   enum IA2CoordinateType aCoordType,
                                   long *aOffset)
{
  GET_NSIACCESSIBLETEXT

  PRUint32 geckoCoordType = (aCoordType == IA2_COORDTYPE_SCREEN_RELATIVE) ?
    nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE :
    nsIAccessibleCoordinateType::COORDTYPE_PARENT_RELATIVE;

  PRInt32 offset = 0;
  nsresult rv = textAcc->GetOffsetAtPoint(aX, aY, geckoCoordType, &offset);
  *aOffset = offset;

  return NS_FAILED(rv) ? E_FAIL : S_OK;
}

STDMETHODIMP
CAccessibleText::get_selection(long aSelectionIndex, long *aStartOffset,
                               long *aEndOffset)
{
  GET_NSIACCESSIBLETEXT

  PRInt32 startOffset = 0, endOffset = 0;
  nsresult rv = textAcc->GetSelectionBounds(aSelectionIndex,
                                            &startOffset, &endOffset);
  *aStartOffset = startOffset;
  *aEndOffset = endOffset;

  return NS_FAILED(rv) ? E_FAIL : S_OK;
}

STDMETHODIMP
CAccessibleText::get_text(long aStartOffset, long aEndOffset, BSTR *aText)
{
  GET_NSIACCESSIBLETEXT

  nsAutoString text;
  nsresult rv = textAcc->GetText(aStartOffset, aEndOffset, text);
  if (NS_FAILED(rv))
    return E_FAIL;

  INT result = ::SysReAllocStringLen(aText, text.get(), text.Length());
  return result ? NS_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
CAccessibleText::get_textBeforeOffset(long aOffset,
                                      enum IA2TextBoundaryType aBoundaryType,
                                      long *aStartOffset, long *aEndOffset,
                                      BSTR *aText)
{
  GET_NSIACCESSIBLETEXT

  nsresult rv = NS_OK;
  nsAutoString text;
  PRInt32 startOffset = 0, endOffset = 0;

  if (aBoundaryType == IA2_TEXT_BOUNDARY_ALL) {
    startOffset = 0;
    textAcc->GetCharacterCount(&endOffset);
    rv = textAcc->GetText(startOffset, endOffset, text);
  } else {
    nsAccessibleTextBoundary boundaryType = GetGeckoTextBoundary(aBoundaryType);
    if (boundaryType != -1) {
      rv = textAcc->GetTextBeforeOffset(aOffset, boundaryType,
                                        &startOffset, &endOffset, text);
    }
  }

  if (NS_FAILED(rv))
    return E_FAIL;

  *aStartOffset = startOffset;
  *aEndOffset = endOffset;

  INT result = ::SysReAllocStringLen(aText, text.get(), text.Length());
  return result ? NS_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
CAccessibleText::get_textAfterOffset(long aOffset,
                                     enum IA2TextBoundaryType aBoundaryType,
                                     long *aStartOffset, long *aEndOffset,
                                     BSTR *aText)
{
  GET_NSIACCESSIBLETEXT

  nsresult rv = NS_OK;
  nsAutoString text;
  PRInt32 startOffset = 0, endOffset = 0;

  if (aBoundaryType == IA2_TEXT_BOUNDARY_ALL) {
    startOffset = 0;
    textAcc->GetCharacterCount(&endOffset);
    rv = textAcc->GetText(startOffset, endOffset, text);
  } else {
    nsAccessibleTextBoundary boundaryType = GetGeckoTextBoundary(aBoundaryType);
    if (boundaryType != -1) {
      rv = textAcc->GetTextAfterOffset(aOffset, boundaryType,
                                       &startOffset, &endOffset, text);
    }
  }

  if (NS_FAILED(rv))
    return E_FAIL;

  *aStartOffset = startOffset;
  *aEndOffset = endOffset;

  INT result = ::SysReAllocStringLen(aText, text.get(), text.Length());
  return result ? NS_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
CAccessibleText::get_textAtOffset(long aOffset,
                                  enum IA2TextBoundaryType aBoundaryType,
                                  long *aStartOffset, long *aEndOffset,
                                  BSTR *aText)
{
  GET_NSIACCESSIBLETEXT

  nsresult rv = NS_OK;
  nsAutoString text;
  PRInt32 startOffset = 0, endOffset = 0;

  if (aBoundaryType == IA2_TEXT_BOUNDARY_ALL) {
    startOffset = 0;
    textAcc->GetCharacterCount(&endOffset);
    rv = textAcc->GetText(startOffset, endOffset, text);
  } else {
    nsAccessibleTextBoundary boundaryType = GetGeckoTextBoundary(aBoundaryType);
    if (boundaryType != -1) {
      rv = textAcc->GetTextAtOffset(aOffset, boundaryType,
                                    &startOffset, &endOffset, text);
    }
  }

  if (NS_FAILED(rv))
    return E_FAIL;

  *aStartOffset = startOffset;
  *aEndOffset = endOffset;

  INT result = ::SysReAllocStringLen(aText, text.get(), text.Length());
  return result ? NS_OK : E_OUTOFMEMORY;
}

STDMETHODIMP
CAccessibleText::removeSelection(long aSelectionIndex)
{
  GET_NSIACCESSIBLETEXT

  nsresult rv = textAcc->RemoveSelection(aSelectionIndex);
  return NS_FAILED(rv) ? E_FAIL : S_OK;
}

STDMETHODIMP
CAccessibleText::setCaretOffset(long aOffset)
{
  GET_NSIACCESSIBLETEXT

  nsresult rv = textAcc->SetCaretOffset(aOffset);
  return NS_FAILED(rv) ? E_FAIL : S_OK;
}

STDMETHODIMP
CAccessibleText::setSelection(long aSelectionIndex, long aStartOffset,
                              long aEndOffset)
{
  GET_NSIACCESSIBLETEXT

  nsresult rv = textAcc->SetSelectionBounds(aSelectionIndex,
                                            aStartOffset, aEndOffset);
  return NS_FAILED(rv) ? E_FAIL : S_OK;
}

STDMETHODIMP
CAccessibleText::get_nCharacters(long *aNCharacters)
{
  GET_NSIACCESSIBLETEXT

  PRInt32 charCount = 0;
  nsresult rv = textAcc->GetCharacterCount(&charCount);
  *aNCharacters = charCount;

  return NS_FAILED(rv) ? E_FAIL : S_OK;
}

STDMETHODIMP
CAccessibleText::scrollSubstringTo(long aStartIndex, long aEndIndex,
                                   enum IA2ScrollType aScrollType)
{
  GET_NSIACCESSIBLETEXT

  nsresult rv = textAcc->ScrollSubstringTo(aStartIndex, aEndIndex, aScrollType);
  return NS_FAILED(rv) ? E_FAIL : S_OK;
}

STDMETHODIMP
CAccessibleText::scrollSubstringToPoint(long aStartIndex, long aEndIndex,
                                        enum IA2CoordinateType aCoordinateType,
                                        long aX, long aY)
{
  GET_NSIACCESSIBLETEXT

  nsCOMPtr<nsIAccessible> accessible;
  PRInt32 startOffset = 0, endOffset = 0;

  // XXX: aEndIndex isn't used.
  textAcc->GetAttributeRange(aStartIndex, &startOffset, &endOffset,
                             getter_AddRefs(accessible));
  if (!accessible)
    return E_FAIL;

  nsCOMPtr<nsIWinAccessNode> winAccessNode(do_QueryInterface(accessible));
  if (!winAccessNode)
    return E_FAIL;

  void **instancePtr = 0;
  winAccessNode->QueryNativeInterface(IID_IAccessible2, instancePtr);
  if (!instancePtr)
    return E_FAIL;

  IAccessible2 *pAccessible2 = static_cast<IAccessible2*>(*instancePtr);
  HRESULT hr = pAccessible2->scrollToPoint(aCoordinateType, aX, aY);
  pAccessible2->Release();

  return hr;
}

STDMETHODIMP
CAccessibleText::get_newText(IA2TextSegment *aNewText)
{
  return GetModifiedText(PR_TRUE, aNewText);
}

STDMETHODIMP
CAccessibleText::get_oldText(IA2TextSegment *aOldText)
{
  return GetModifiedText(PR_FALSE, aOldText);
}

// CAccessibleText

HRESULT
CAccessibleText::GetModifiedText(PRBool aGetInsertedText,
                                 IA2TextSegment *aText)
{
  PRUint32 startOffset = 0, endOffset = 0;
  nsAutoString text;

  nsresult rv = GetModifiedText(aGetInsertedText, text,
                                &startOffset, &endOffset);
  if (NS_FAILED(rv))
    return E_FAIL;

  aText->start = startOffset;
  aText->end = endOffset;

  INT result = ::SysReAllocStringLen(&(aText->text), text.get(), text.Length());
  return result ? NS_OK : E_OUTOFMEMORY;
}

nsAccessibleTextBoundary
CAccessibleText::GetGeckoTextBoundary(enum IA2TextBoundaryType aBoundaryType)
{
  switch (aBoundaryType) {
    case IA2_TEXT_BOUNDARY_CHAR:
      return nsIAccessibleText::BOUNDARY_CHAR;
    case IA2_TEXT_BOUNDARY_WORD:
      return nsIAccessibleText::BOUNDARY_WORD_START;
    case IA2_TEXT_BOUNDARY_SENTENCE:
      return nsIAccessibleText::BOUNDARY_SENTENCE_START;
    case IA2_TEXT_BOUNDARY_PARAGRAPH:
      // XXX: not implemented
      return nsIAccessibleText::BOUNDARY_LINE_START;
    case IA2_TEXT_BOUNDARY_LINE:
      return nsIAccessibleText::BOUNDARY_LINE_START;
    default:
      return -1;
  }
}

