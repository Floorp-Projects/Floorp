/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CheckableInputTypes.h"

#include "mozilla/dom/HTMLInputElement.h"

/* input type=checkbox */

bool
CheckboxInputType::IsValueMissing() const
{
  if (!mInputElement->HasAttr(kNameSpaceID_None, nsGkAtoms::required)) {
    return false;
  }

  if (!IsMutable()) {
    return false;
  }

  return !mInputElement->Checked();
}

nsresult
CheckboxInputType::GetValueMissingMessage(nsXPIDLString& aMessage)
{
  return nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                            "FormValidationCheckboxMissing",
                                            aMessage);
}

/* input type=radio */

nsresult
RadioInputType::GetValueMissingMessage(nsXPIDLString& aMessage)
{
  return nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                            "FormValidationRadioMissing",
                                            aMessage);
}
