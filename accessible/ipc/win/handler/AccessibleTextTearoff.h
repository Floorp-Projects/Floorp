/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(MOZILLA_INTERNAL_API)
#error This code is NOT for internal Gecko use!
#endif // defined(MOZILLA_INTERNAL_API)

#ifndef mozilla_a11y_AccessibleTextTearoff_h
#define mozilla_a11y_AccessibleTextTearoff_h

#include "AccessibleHandler.h"
#include "AccessibleHypertext.h"
#include "IUnknownImpl.h"
#include "mozilla/RefPtr.h"

namespace mozilla {
namespace a11y {

class AccessibleTextTearoff final : public IAccessibleHypertext
{
public:
  explicit AccessibleTextTearoff(AccessibleHandler* aHandler);

  DECL_IUNKNOWN

  // IAccessibleText
  STDMETHODIMP addSelection(long startOffset, long endOffset) override;
  STDMETHODIMP get_attributes(long offset, long *startOffset, long *endOffset,
                              BSTR *textAttributes) override;
  STDMETHODIMP get_caretOffset(long *offset) override;
  STDMETHODIMP get_characterExtents(long offset,
                                    enum IA2CoordinateType coordType, long *x,
                                    long *y, long *width, long *height) override;
  STDMETHODIMP get_nSelections(long *nSelections) override;
  STDMETHODIMP get_offsetAtPoint(long x, long y,
                                 enum IA2CoordinateType coordType,
                                 long *offset) override;
  STDMETHODIMP get_selection(long selectionIndex, long *startOffset,
                             long *endOffset) override;
  STDMETHODIMP get_text(long startOffset, long endOffset, BSTR *text) override;
  STDMETHODIMP get_textBeforeOffset(long offset,
                                    enum IA2TextBoundaryType boundaryType,
                                    long *startOffset, long *endOffset,
                                    BSTR *text) override;
  STDMETHODIMP get_textAfterOffset(long offset,
                                   enum IA2TextBoundaryType boundaryType,
                                   long *startOffset, long *endOffset,
                                   BSTR *text) override;
  STDMETHODIMP get_textAtOffset(long offset,
                                enum IA2TextBoundaryType boundaryType,
                                long *startOffset, long *endOffset,
                                BSTR *text) override;
  STDMETHODIMP removeSelection(long selectionIndex) override;
  STDMETHODIMP setCaretOffset(long offset) override;
  STDMETHODIMP setSelection(long selectionIndex, long startOffset,
                            long endOffset) override;
  STDMETHODIMP get_nCharacters(long *nCharacters) override;
  STDMETHODIMP scrollSubstringTo(long startIndex, long endIndex,
                                 enum IA2ScrollType scrollType) override;
  STDMETHODIMP scrollSubstringToPoint(long startIndex, long endIndex,
                                      enum IA2CoordinateType coordinateType,
                                      long x, long y) override;
  STDMETHODIMP get_newText(IA2TextSegment *newText) override;
  STDMETHODIMP get_oldText(IA2TextSegment *oldText) override;

  // IAccessibleHypertext
  STDMETHODIMP get_nHyperlinks(long *hyperlinkCount) override;
  STDMETHODIMP get_hyperlink(long index,
                             IAccessibleHyperlink **hyperlink) override;
  STDMETHODIMP get_hyperlinkIndex(long charIndex, long *hyperlinkIndex) override;

private:
  ~AccessibleTextTearoff() = default;
  HRESULT ResolveAccText();
  HRESULT ResolveAccHypertext();

  RefPtr<AccessibleHandler>     mHandler;
  RefPtr<IAccessibleText>       mAccTextProxy;
  RefPtr<IAccessibleHypertext>  mAccHypertextProxy;
};

} // namespace a11y
} // namespace mozilla

#endif // mozilla_a11y_AccessibleTextTearoff_h
