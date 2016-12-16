/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_sdnTextAccessible_h_
#define mozilla_a11y_sdnTextAccessible_h_

#include "ISimpleDOMText.h"
#include "IUnknownImpl.h"

#include "AccessibleWrap.h"

class nsIFrame;
struct nsPoint;

namespace mozilla {
namespace a11y {

class sdnTextAccessible final : public ISimpleDOMText
{
public:
  explicit sdnTextAccessible(AccessibleWrap* aAccessible) : mAccessible(aAccessible) {};
  ~sdnTextAccessible() {}

  DECL_IUNKNOWN

  // ISimpleDOMText

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_domText(
    /* [retval][out] */ BSTR __RPC_FAR *aText);

  virtual HRESULT STDMETHODCALLTYPE get_clippedSubstringBounds(
    /* [in] */ unsigned int startIndex,
    /* [in] */ unsigned int endIndex,
    /* [out] */ int __RPC_FAR* aX,
    /* [out] */ int __RPC_FAR* aY,
    /* [out] */ int __RPC_FAR* aWidth,
    /* [out] */ int __RPC_FAR* aHeight);

  virtual HRESULT STDMETHODCALLTYPE get_unclippedSubstringBounds(
    /* [in] */ unsigned int aStartIndex,
    /* [in] */ unsigned int aEndIndex,
    /* [out] */ int __RPC_FAR* aX,
    /* [out] */ int __RPC_FAR* aY,
    /* [out] */ int __RPC_FAR* aWidth,
    /* [out] */ int __RPC_FAR* aHeight);

  virtual HRESULT STDMETHODCALLTYPE scrollToSubstring(
    /* [in] */ unsigned int aStartIndex,
    /* [in] */ unsigned int aEndIndex);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_fontFamily(
    /* [retval][out] */ BSTR __RPC_FAR* aFontFamily);

private:
  /**
   *  Return child frame containing offset on success.
   */
  nsIFrame* GetPointFromOffset(nsIFrame* aContainingFrame,
                               int32_t aOffset, bool aPreferNext,
                               nsPoint& aOutPoint);

  RefPtr<AccessibleWrap> mAccessible;
};

} // namespace a11y
} // namespace mozilla

#endif
