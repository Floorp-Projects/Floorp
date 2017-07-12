/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CheckableInputTypes_h__
#define CheckableInputTypes_h__

#include "InputType.h"

class CheckableInputTypeBase : public ::InputType
{
public:
  ~CheckableInputTypeBase() override {}

protected:
  explicit CheckableInputTypeBase(mozilla::dom::HTMLInputElement* aInputElement)
    : InputType(aInputElement)
  {}
};

// input type=checkbox
class CheckboxInputType : public CheckableInputTypeBase
{
public:
  static InputType*
  Create(mozilla::dom::HTMLInputElement* aInputElement, void* aMemory)
  {
    return new (aMemory) CheckboxInputType(aInputElement);
  }

  bool IsValueMissing() const override;

  nsresult GetValueMissingMessage(nsXPIDLString& aMessage) override;

private:
  explicit CheckboxInputType(mozilla::dom::HTMLInputElement* aInputElement)
    : CheckableInputTypeBase(aInputElement)
  {}
};

// input type=radio
class RadioInputType : public CheckableInputTypeBase
{
public:
  static InputType*
  Create(mozilla::dom::HTMLInputElement* aInputElement, void* aMemory)
  {
    return new (aMemory) RadioInputType(aInputElement);
  }

  nsresult GetValueMissingMessage(nsXPIDLString& aMessage) override;

private:
  explicit RadioInputType(mozilla::dom::HTMLInputElement* aInputElement)
    : CheckableInputTypeBase(aInputElement)
  {}
};

#endif /* CheckableInputTypes_h__ */
