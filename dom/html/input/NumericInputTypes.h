/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NumericInputTypes_h__
#define NumericInputTypes_h__

#include "InputType.h"

class NumericInputTypeBase : public ::InputType
{
public:
  ~NumericInputTypeBase() override {}

protected:
  explicit NumericInputTypeBase(mozilla::dom::HTMLInputElement* aInputElement)
    : InputType(aInputElement)
  {}
};

// input type=number
class NumberInputType : public NumericInputTypeBase
{
public:
  static InputType*
  Create(mozilla::dom::HTMLInputElement* aInputElement, void* aMemory)
  {
    return new (aMemory) NumberInputType(aInputElement);
  }

private:
  explicit NumberInputType(mozilla::dom::HTMLInputElement* aInputElement)
    : NumericInputTypeBase(aInputElement)
  {}
};

// input type=range
class RangeInputType : public NumericInputTypeBase
{
public:
  static InputType*
  Create(mozilla::dom::HTMLInputElement* aInputElement, void* aMemory)
  {
    return new (aMemory) RangeInputType(aInputElement);
  }

private:
  explicit RangeInputType(mozilla::dom::HTMLInputElement* aInputElement)
    : NumericInputTypeBase(aInputElement)
  {}
};

#endif /* NumericInputTypes_h__ */
