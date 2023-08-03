/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _ACCESSIBLE_HYPERTEXT_H
#define _ACCESSIBLE_HYPERTEXT_H

#include "nsISupports.h"

#include "ia2AccessibleEditableText.h"
#include "ia2AccessibleText.h"
#include "ia2AccessibleTextSelectionContainer.h"
#include "AccessibleHypertext2.h"
#include "IUnknownImpl.h"
#include "MsaaAccessible.h"

namespace mozilla {
namespace a11y {
class HyperTextAccessibleBase;

class ia2AccessibleHypertext : public ia2AccessibleText,
                               public IAccessibleHypertext2,
                               public ia2AccessibleEditableText,
                               public ia2AccessibleTextSelectionContainer,
                               public MsaaAccessible {
 public:
  // IUnknown
  DECL_IUNKNOWN_INHERITED
  IMPL_IUNKNOWN_REFCOUNTING_INHERITED(MsaaAccessible)

  // IAccessible2
  // We indirectly inherit IAccessible2, which has a get_attributes method,
  // but IAccessibleText also has a get_attributes method with a different
  // signature. We want both.
  using MsaaAccessible::get_attributes;

  // IAccessibleText
  FORWARD_IACCESSIBLETEXT(ia2AccessibleText)

  // IAccessibleHypertext
  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_nHyperlinks(
      /* [retval][out] */ long* hyperlinkCount);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_hyperlink(
      /* [in] */ long index,
      /* [retval][out] */ IAccessibleHyperlink** hyperlink);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_hyperlinkIndex(
      /* [in] */ long charIndex,
      /* [retval][out] */ long* hyperlinkIndex);

  // IAccessibleHypertext2
  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_hyperlinks(
      /* [out, size_is(,*nHyperlinks)] */ IAccessibleHyperlink*** hyperlinks,
      /* [out, retval] */ long* nHyperlinks);

 protected:
  using MsaaAccessible::MsaaAccessible;

 private:
  HyperTextAccessibleBase* TextAcc();
};

}  // namespace a11y
}  // namespace mozilla

#endif
