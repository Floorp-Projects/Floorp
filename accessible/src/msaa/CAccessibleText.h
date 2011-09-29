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

#ifndef _ACCESSIBLE_TEXT_H
#define _ACCESSIBLE_TEXT_H

#include "nsISupports.h"
#include "nsIAccessibleText.h"

#include "AccessibleText.h"

class CAccessibleText: public IAccessibleText
{
public:

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID, void**);

  // IAccessibleText
  virtual HRESULT STDMETHODCALLTYPE addSelection(
      /* [in] */ long startOffset,
      /* [in] */ long endOffset);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_attributes(
      /* [in] */ long offset,
      /* [out] */ long *startOffset,
      /* [out] */ long *endOffset,
      /* [retval][out] */ BSTR *textAttributes);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_caretOffset(
      /* [retval][out] */ long *offset);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_characterExtents(
      /* [in] */ long offset,
      /* [in] */ enum IA2CoordinateType coordType,
      /* [out] */ long *x,
      /* [out] */ long *y,
      /* [out] */ long *width,
      /* [retval][out] */ long *height);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_nSelections(
      /* [retval][out] */ long *nSelections);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_offsetAtPoint(
      /* [in] */ long x,
      /* [in] */ long y,
      /* [in] */ enum IA2CoordinateType coordType,
      /* [retval][out] */ long *offset);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_selection(
      /* [in] */ long selectionIndex,
      /* [out] */ long *startOffset,
      /* [retval][out] */ long *endOffset);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_text(
      /* [in] */ long startOffset,
      /* [in] */ long endOffset,
      /* [retval][out] */ BSTR *text);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_textBeforeOffset(
      /* [in] */ long offset,
      /* [in] */ enum IA2TextBoundaryType boundaryType,
      /* [out] */ long *startOffset,
      /* [out] */ long *endOffset,
      /* [retval][out] */ BSTR *text);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_textAfterOffset(
      /* [in] */ long offset,
      /* [in] */ enum IA2TextBoundaryType boundaryType,
      /* [out] */ long *startOffset,
      /* [out] */ long *endOffset,
      /* [retval][out] */ BSTR *text);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_textAtOffset(
      /* [in] */ long offset,
      /* [in] */ enum IA2TextBoundaryType boundaryType,
      /* [out] */ long *startOffset,
      /* [out] */ long *endOffset,
      /* [retval][out] */ BSTR *text);

  virtual HRESULT STDMETHODCALLTYPE removeSelection(
      /* [in] */ long selectionIndex);

  virtual HRESULT STDMETHODCALLTYPE setCaretOffset(
      /* [in] */ long offset);

  virtual HRESULT STDMETHODCALLTYPE setSelection(
      /* [in] */ long selectionIndex,
      /* [in] */ long startOffset,
      /* [in] */ long endOffset);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_nCharacters(
      /* [retval][out] */ long *nCharacters);

  virtual HRESULT STDMETHODCALLTYPE scrollSubstringTo(
      /* [in] */ long startIndex,
      /* [in] */ long endIndex,
      /* [in] */ enum IA2ScrollType scrollType);

  virtual HRESULT STDMETHODCALLTYPE scrollSubstringToPoint(
      /* [in] */ long startIndex,
      /* [in] */ long endIndex,
      /* [in] */ enum IA2CoordinateType coordinateType,
      /* [in] */ long x,
      /* [in] */ long y);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_newText(
      /* [retval][out] */ IA2TextSegment *newText);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_oldText(
      /* [retval][out] */ IA2TextSegment *oldText);

  // nsISupports
  NS_IMETHOD QueryInterface(const nsIID& uuid, void** result) = 0;

protected:
  virtual nsresult GetModifiedText(bool aGetInsertedText, nsAString& aText,
                                   PRUint32 *aStartOffset,
                                   PRUint32 *aEndOffset) = 0;

private:
  HRESULT GetModifiedText(bool aGetInsertedText, IA2TextSegment *aNewText);
  nsAccessibleTextBoundary GetGeckoTextBoundary(enum IA2TextBoundaryType coordinateType);
};


#define FORWARD_IACCESSIBLETEXT(Class)                                         \
virtual HRESULT STDMETHODCALLTYPE addSelection(long startOffset,               \
                                               long endOffset)                 \
{                                                                              \
  return Class::addSelection(startOffset, endOffset);                          \
}                                                                              \
                                                                               \
virtual HRESULT STDMETHODCALLTYPE get_attributes(long offset,                  \
                                                 long *startOffset,            \
                                                 long *endOffset,              \
                                                 BSTR *textAttributes)         \
{                                                                              \
  return Class::get_attributes(offset, startOffset, endOffset, textAttributes);\
}                                                                              \
                                                                               \
virtual HRESULT STDMETHODCALLTYPE get_caretOffset(long *offset)                \
{                                                                              \
  return Class::get_caretOffset(offset);                                       \
}                                                                              \
                                                                               \
virtual HRESULT STDMETHODCALLTYPE get_characterExtents(long offset,            \
                                                       enum IA2CoordinateType coordType,\
                                                       long *x,                \
                                                       long *y,                \
                                                       long *width,            \
                                                       long *height)           \
{                                                                              \
  return Class::get_characterExtents(offset, coordType, x, y, width, height);  \
}                                                                              \
                                                                               \
virtual HRESULT STDMETHODCALLTYPE get_nSelections(long *nSelections)           \
{                                                                              \
  return Class::get_nSelections(nSelections);                                  \
}                                                                              \
                                                                               \
virtual HRESULT STDMETHODCALLTYPE get_offsetAtPoint(long x,                    \
                                                    long y,                    \
                                                    enum IA2CoordinateType coordType,\
                                                    long *offset)              \
{                                                                              \
  return Class::get_offsetAtPoint(x, y, coordType, offset);                    \
}                                                                              \
                                                                               \
virtual HRESULT STDMETHODCALLTYPE get_selection(long selectionIndex,           \
                                                long *startOffset,             \
                                                long *endOffset)               \
{                                                                              \
  return Class::get_selection(selectionIndex, startOffset, endOffset);         \
}                                                                              \
                                                                               \
virtual HRESULT STDMETHODCALLTYPE get_text(long startOffset,                   \
                                           long endOffset,                     \
                                           BSTR *text)                         \
{                                                                              \
  return Class::get_text(startOffset, endOffset, text);                        \
}                                                                              \
                                                                               \
virtual HRESULT STDMETHODCALLTYPE get_textBeforeOffset(long offset,            \
                                                       enum IA2TextBoundaryType boundaryType,\
                                                       long *startOffset,      \
                                                       long *endOffset,        \
                                                       BSTR *text)             \
{                                                                              \
  return Class::get_textBeforeOffset(offset, boundaryType,                     \
                                     startOffset, endOffset, text);            \
}                                                                              \
                                                                               \
virtual HRESULT STDMETHODCALLTYPE get_textAfterOffset(long offset,             \
                                                      enum IA2TextBoundaryType boundaryType,\
                                                      long *startOffset,       \
                                                      long *endOffset,         \
                                                      BSTR *text)              \
{                                                                              \
  return Class::get_textAfterOffset(offset, boundaryType,                      \
                                    startOffset, endOffset, text);             \
}                                                                              \
                                                                               \
virtual HRESULT STDMETHODCALLTYPE get_textAtOffset(long offset,                \
                                                   enum IA2TextBoundaryType boundaryType,\
                                                   long *startOffset,          \
                                                   long *endOffset,            \
                                                   BSTR *text)                 \
{                                                                              \
  return Class::get_textAtOffset(offset, boundaryType,                         \
                                 startOffset, endOffset, text);                \
}                                                                              \
                                                                               \
virtual HRESULT STDMETHODCALLTYPE removeSelection(long selectionIndex)         \
{                                                                              \
  return Class::removeSelection(selectionIndex);                               \
}                                                                              \
                                                                               \
virtual HRESULT STDMETHODCALLTYPE setCaretOffset(long offset)                  \
{                                                                              \
  return Class::setCaretOffset(offset);                                        \
}                                                                              \
                                                                               \
virtual HRESULT STDMETHODCALLTYPE setSelection(long selectionIndex,            \
                                               long startOffset,               \
                                               long endOffset)                 \
{                                                                              \
  return Class::setSelection(selectionIndex, startOffset, endOffset);          \
}                                                                              \
                                                                               \
virtual HRESULT STDMETHODCALLTYPE get_nCharacters(long *nCharacters)           \
{                                                                              \
  return Class::get_nCharacters(nCharacters);                                  \
}                                                                              \
                                                                               \
virtual HRESULT STDMETHODCALLTYPE scrollSubstringTo(long startIndex,           \
                                                    long endIndex,             \
                                                    enum IA2ScrollType scrollType)\
{                                                                              \
  return Class::scrollSubstringTo(startIndex, endIndex, scrollType);           \
}                                                                              \
                                                                               \
virtual HRESULT STDMETHODCALLTYPE scrollSubstringToPoint(long startIndex,      \
                                                         long endIndex,        \
                                                         enum IA2CoordinateType coordinateType,\
                                                         long x,               \
                                                         long y)               \
{                                                                              \
  return Class::scrollSubstringToPoint(startIndex, endIndex,                   \
                                       coordinateType, x, y);                  \
}                                                                              \
                                                                               \
virtual HRESULT STDMETHODCALLTYPE get_newText(IA2TextSegment *newText)         \
{                                                                              \
  return Class::get_newText(newText);                                          \
}                                                                              \
                                                                               \
virtual HRESULT STDMETHODCALLTYPE get_oldText(IA2TextSegment *oldText)         \
{                                                                              \
  return Class::get_oldText(oldText);                                          \
}                                                                              \

#endif

