/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(MOZILLA_INTERNAL_API)
#error This code is NOT for internal Gecko use!
#endif // defined(MOZILLA_INTERNAL_API)

#include "AccessibleTextTearoff.h"

#include "AccessibleHandlerControl.h"
#include "AccessibleText_i.c"
#include "AccessibleHypertext_i.c"
#include "AccessibleHypertext2_i.c"
#include "Factory.h"

#include "mozilla/Assertions.h"

namespace mozilla {
namespace a11y {

AccessibleTextTearoff::AccessibleTextTearoff(AccessibleHandler* aHandler)
  : mHandler(aHandler)
{
  MOZ_ASSERT(aHandler);
}

HRESULT
AccessibleTextTearoff::ResolveAccHypertext()
{
  if (mAccHypertextProxy) {
    return S_OK;
  }

  RefPtr<IUnknown> proxy(mHandler->GetProxy());
  if (!proxy) {
    return E_UNEXPECTED;
  }

  return proxy->QueryInterface(IID_IAccessibleHypertext2,
                               getter_AddRefs(mAccHypertextProxy));
}

IMPL_IUNKNOWN_QUERY_HEAD(AccessibleTextTearoff)
IMPL_IUNKNOWN_QUERY_IFACE(IAccessibleText)
IMPL_IUNKNOWN_QUERY_IFACE(IAccessibleHypertext)
IMPL_IUNKNOWN_QUERY_IFACE(IAccessibleHypertext2)
IMPL_IUNKNOWN_QUERY_TAIL_AGGREGATED(mHandler)

HRESULT
AccessibleTextTearoff::addSelection(long startOffset, long endOffset)
{
  HRESULT hr = ResolveAccHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mAccHypertextProxy->addSelection(startOffset, endOffset);
}

HRESULT
AccessibleTextTearoff::get_attributes(long offset, long *startOffset,
                                      long *endOffset, BSTR *textAttributes)
{
  HRESULT hr = ResolveAccHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mAccHypertextProxy->get_attributes(offset, startOffset, endOffset,
                                       textAttributes);
}

HRESULT
AccessibleTextTearoff::get_caretOffset(long *offset)
{
  HRESULT hr = ResolveAccHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mAccHypertextProxy->get_caretOffset(offset);
}

HRESULT
AccessibleTextTearoff::get_characterExtents(long offset,
                                            enum IA2CoordinateType coordType,
                                            long *x, long *y, long *width,
                                            long *height)
{
  HRESULT hr = ResolveAccHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mAccHypertextProxy->get_characterExtents(offset, coordType, x, y, width,
                                             height);
}

HRESULT
AccessibleTextTearoff::get_nSelections(long *nSelections)
{
  HRESULT hr = ResolveAccHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mAccHypertextProxy->get_nSelections(nSelections);
}

HRESULT
AccessibleTextTearoff::get_offsetAtPoint(long x, long y,
                                         enum IA2CoordinateType coordType,
                                         long *offset)
{
  HRESULT hr = ResolveAccHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mAccHypertextProxy->get_offsetAtPoint(x, y, coordType, offset);
}

HRESULT
AccessibleTextTearoff::get_selection(long selectionIndex, long *startOffset,
                                     long *endOffset)
{
  HRESULT hr = ResolveAccHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mAccHypertextProxy->get_selection(selectionIndex, startOffset, endOffset);
}

HRESULT
AccessibleTextTearoff::get_text(long startOffset, long endOffset, BSTR *text)
{
  HRESULT hr = ResolveAccHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mAccHypertextProxy->get_text(startOffset, endOffset, text);
}

HRESULT
AccessibleTextTearoff::get_textBeforeOffset(long offset,
                                            enum IA2TextBoundaryType boundaryType,
                                            long *startOffset, long *endOffset,
                                            BSTR *text)
{
  HRESULT hr = ResolveAccHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mAccHypertextProxy->get_textBeforeOffset(offset, boundaryType, startOffset,
                                             endOffset, text);
}

HRESULT
AccessibleTextTearoff::get_textAfterOffset(long offset,
                                           enum IA2TextBoundaryType boundaryType,
                                           long *startOffset, long *endOffset,
                                           BSTR *text)
{
  HRESULT hr = ResolveAccHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mAccHypertextProxy->get_textAfterOffset(offset, boundaryType,
                                            startOffset, endOffset, text);
}

HRESULT
AccessibleTextTearoff::get_textAtOffset(long offset,
                                        enum IA2TextBoundaryType boundaryType,
                                        long *startOffset, long *endOffset,
                                        BSTR *text)
{
  HRESULT hr = ResolveAccHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mAccHypertextProxy->get_textAtOffset(offset, boundaryType, startOffset,
                                         endOffset, text);
}

HRESULT
AccessibleTextTearoff::removeSelection(long selectionIndex)
{
  HRESULT hr = ResolveAccHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mAccHypertextProxy->removeSelection(selectionIndex);
}

HRESULT
AccessibleTextTearoff::setCaretOffset(long offset)
{
  HRESULT hr = ResolveAccHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mAccHypertextProxy->setCaretOffset(offset);
}

HRESULT
AccessibleTextTearoff::setSelection(long selectionIndex, long startOffset,
                                    long endOffset)
{
  HRESULT hr = ResolveAccHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mAccHypertextProxy->setSelection(selectionIndex, startOffset, endOffset);
}

HRESULT
AccessibleTextTearoff::get_nCharacters(long *nCharacters)
{
  HRESULT hr = ResolveAccHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mAccHypertextProxy->get_nCharacters(nCharacters);
}

HRESULT
AccessibleTextTearoff::scrollSubstringTo(long startIndex, long endIndex,
                                         enum IA2ScrollType scrollType)
{
  HRESULT hr = ResolveAccHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mAccHypertextProxy->scrollSubstringTo(startIndex, endIndex, scrollType);
}

HRESULT
AccessibleTextTearoff::scrollSubstringToPoint(long startIndex, long endIndex,
                                              enum IA2CoordinateType coordinateType,
                                              long x, long y)
{
  HRESULT hr = ResolveAccHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mAccHypertextProxy->scrollSubstringToPoint(startIndex, endIndex,
                                               coordinateType, x, y);
}

HRESULT
AccessibleTextTearoff::get_newText(IA2TextSegment *newText)
{
  if (!newText) {
    return E_INVALIDARG;
  }

  RefPtr<AccessibleHandlerControl> ctl(gControlFactory.GetSingleton());
  MOZ_ASSERT(ctl);
  if (!ctl) {
    return S_OK;
  }

  long id;
  HRESULT hr = mHandler->get_uniqueID(&id);
  if (FAILED(hr)) {
    return hr;
  }

  return ctl->GetNewText(id, WrapNotNull(newText));
}

HRESULT
AccessibleTextTearoff::get_oldText(IA2TextSegment *oldText)
{
  if (!oldText) {
    return E_INVALIDARG;
  }

  RefPtr<AccessibleHandlerControl> ctl(gControlFactory.GetSingleton());
  MOZ_ASSERT(ctl);
  if (!ctl) {
    return S_OK;
  }

  long id;
  HRESULT hr = mHandler->get_uniqueID(&id);
  if (FAILED(hr)) {
    return hr;
  }

  return ctl->GetOldText(id, WrapNotNull(oldText));
}

HRESULT
AccessibleTextTearoff::get_nHyperlinks(long *hyperlinkCount)
{
  HRESULT hr = ResolveAccHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mAccHypertextProxy->get_nHyperlinks(hyperlinkCount);
}

HRESULT
AccessibleTextTearoff::get_hyperlink(long index,
                                     IAccessibleHyperlink **hyperlink)
{
  HRESULT hr = ResolveAccHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mAccHypertextProxy->get_hyperlink(index, hyperlink);
}

HRESULT
AccessibleTextTearoff::get_hyperlinkIndex(long charIndex, long *hyperlinkIndex)
{
  HRESULT hr = ResolveAccHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mAccHypertextProxy->get_hyperlinkIndex(charIndex, hyperlinkIndex);
}

HRESULT
AccessibleTextTearoff::get_hyperlinks(IAccessibleHyperlink*** hyperlinks,
                                      long* nHyperlinks)
{
  HRESULT hr = ResolveAccHypertext();
  if (FAILED(hr)) {
    return hr;
  }

  return mAccHypertextProxy->get_hyperlinks(hyperlinks, nHyperlinks);
}

} // namespace a11y
} // namespace mozilla
