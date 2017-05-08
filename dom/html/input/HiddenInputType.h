/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HiddenInputType_h__
#define HiddenInputType_h__

#include "InputType.h"

// input type=hidden
class HiddenInputType : public ::InputType
{
public:
  static InputType*
  Create(mozilla::dom::HTMLInputElement* aInputElement, void* aMemory)
  {
    return new (aMemory) HiddenInputType(aInputElement);
  }

private:
  explicit HiddenInputType(mozilla::dom::HTMLInputElement* aInputElement)
    : InputType(aInputElement)
  {}
};

#endif /* HiddenInputType_h__ */
