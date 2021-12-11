/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_MsaaXULMenuAccessible_h__
#define mozilla_a11y_MsaaXULMenuAccessible_h__

#include "MsaaAccessible.h"

namespace mozilla {
namespace a11y {

class MsaaXULMenuitemAccessible : public MsaaAccessible {
 public:
  virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_accKeyboardShortcut(
      /* [optional][in] */ VARIANT varChild,
      /* [retval][out] */ BSTR __RPC_FAR* pszKeyboardShortcut) override;

 protected:
  using MsaaAccessible::MsaaAccessible;
};

}  // namespace a11y
}  // namespace mozilla

#endif
