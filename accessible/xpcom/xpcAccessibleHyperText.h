/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_xpcAccessibleHyperText_h_
#define mozilla_a11y_xpcAccessibleHyperText_h_

#include "nsIAccessibleText.h"
#include "nsIAccessibleHyperText.h"
#include "nsIAccessibleEditableText.h"

#include "HyperTextAccessible.h"
#include "xpcAccessibleGeneric.h"

namespace mozilla {
namespace a11y {

class xpcAccessibleHyperText : public xpcAccessibleGeneric,
                               public nsIAccessibleText,
                               public nsIAccessibleEditableText,
                               public nsIAccessibleHyperText
{
public:
  explicit xpcAccessibleHyperText(Accessible* aIntl) :
    xpcAccessibleGeneric(aIntl)
  {
    if (mIntl->IsHyperText() && mIntl->AsHyperText()->IsTextRole())
      mSupportedIfaces |= eText;
  }

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIACCESSIBLETEXT
  NS_DECL_NSIACCESSIBLEHYPERTEXT
  NS_DECL_NSIACCESSIBLEEDITABLETEXT

protected:
  virtual ~xpcAccessibleHyperText() {}

private:
  HyperTextAccessible* Intl() { return mIntl->AsHyperText(); }

  xpcAccessibleHyperText(const xpcAccessibleHyperText&) MOZ_DELETE;
  xpcAccessibleHyperText& operator =(const xpcAccessibleHyperText&) MOZ_DELETE;
};

} // namespace a11y
} // namespace mozilla

#endif // mozilla_a11y_xpcAccessibleHyperText_h_
