/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_MsaaXULMenuAccessible_h__
#define mozilla_a11y_MsaaXULMenuAccessible_h__

#include "XULMenuAccessible.h"

namespace mozilla {
namespace a11y {

// XXX This should inherit from MsaaAccessible. Inheriting from
// XULMenuitemAccessible is a necessary hack until we remove the inheritance of
// XULMenuitemAccessibleWrap.
class MsaaXULMenuitemAccessible : public XULMenuitemAccessible {
 public:
  using XULMenuitemAccessible::XULMenuitemAccessible;

  virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accKeyboardShortcut(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR* pszKeyboardShortcut) override;
};

}  // namespace a11y
}  // namespace mozilla

#endif
