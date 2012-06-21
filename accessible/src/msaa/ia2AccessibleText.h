/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _ACCESSIBLE_TEXT_H
#define _ACCESSIBLE_TEXT_H

#include "nsISupports.h"
#include "nsIAccessibleText.h"

#include "AccessibleText.h"

class ia2AccessibleText: public IAccessibleText
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
  AccessibleTextBoundary GetGeckoTextBoundary(enum IA2TextBoundaryType coordinateType);
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

