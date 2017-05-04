/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SingleLineTextInputTypes.h"

#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/BindingDeclarations.h"

bool
SingleLineTextInputTypeBase::IsTooLong() const
{
  int32_t maxLength = mInputElement->MaxLength();

  // Maxlength of -1 means attribute isn't set or parsing error.
  if (maxLength == -1) {
   return false;
  }

  int32_t textLength =
    mInputElement->InputTextLength(mozilla::dom::CallerType::System);

  return textLength > maxLength;
}

bool
SingleLineTextInputTypeBase::IsTooShort() const
{
  int32_t minLength = mInputElement->MinLength();

  // Minlength of -1 means attribute isn't set or parsing error.
  if (minLength == -1) {
    return false;
  }

  int32_t textLength =
    mInputElement->InputTextLength(mozilla::dom::CallerType::System);

  return textLength && textLength < minLength;
}
