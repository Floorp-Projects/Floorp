/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_xpcAccessibleTextRange_h_
#define mozilla_a11y_xpcAccessibleTextRange_h_

#include "nsIAccessibleTextRange.h"
#include "TextRange.h"

#include "mozilla/Move.h"

namespace mozilla {
namespace a11y {

class TextRange;

class xpcAccessibleTextRange MOZ_FINAL : public nsIAccessibleTextRange
{
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD GetStartContainer(nsIAccessible** aAnchor) MOZ_FINAL MOZ_OVERRIDE;
  NS_IMETHOD GetStartOffset(int32_t* aOffset) MOZ_FINAL MOZ_OVERRIDE;
  NS_IMETHOD GetEndContainer(nsIAccessible** aAnchor) MOZ_FINAL MOZ_OVERRIDE;
  NS_IMETHOD GetEndOffset(int32_t* aOffset) MOZ_FINAL MOZ_OVERRIDE;
  NS_IMETHOD GetText(nsAString& aText) MOZ_FINAL MOZ_OVERRIDE;

private:
  xpcAccessibleTextRange(TextRange&& aRange) :
    mRange(Forward<TextRange>(aRange)) {}
  xpcAccessibleTextRange() {}
  friend class xpcAccessibleHyperText;

  xpcAccessibleTextRange(const xpcAccessibleTextRange&) MOZ_DELETE;
  xpcAccessibleTextRange& operator =(const xpcAccessibleTextRange&) MOZ_DELETE;

  TextRange mRange;
};

} // namespace a11y
} // namespace mozilla

#endif
