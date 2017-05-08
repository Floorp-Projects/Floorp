/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ButtonInputTypes_h__
#define ButtonInputTypes_h__

#include "InputType.h"

class ButtonInputTypeBase : public ::InputType
{
public:
  ~ButtonInputTypeBase() override {}

protected:
  explicit ButtonInputTypeBase(mozilla::dom::HTMLInputElement* aInputElement)
    : InputType(aInputElement)
  {}
};

// input type=button
class ButtonInputType : public ButtonInputTypeBase
{
public:
  static InputType*
  Create(mozilla::dom::HTMLInputElement* aInputElement, void* aMemory)
  {
    return new (aMemory) ButtonInputType(aInputElement);
  }

private:
  explicit ButtonInputType(mozilla::dom::HTMLInputElement* aInputElement)
    : ButtonInputTypeBase(aInputElement)
  {}
};

// input type=image
class ImageInputType : public ButtonInputTypeBase
{
public:
  static InputType*
  Create(mozilla::dom::HTMLInputElement* aInputElement, void* aMemory)
  {
    return new (aMemory) ImageInputType(aInputElement);
  }

private:
  explicit ImageInputType(mozilla::dom::HTMLInputElement* aInputElement)
    : ButtonInputTypeBase(aInputElement)
  {}
};

// input type=reset
class ResetInputType : public ButtonInputTypeBase
{
public:
  static InputType*
  Create(mozilla::dom::HTMLInputElement* aInputElement, void* aMemory)
  {
    return new (aMemory) ResetInputType(aInputElement);
  }

private:
  explicit ResetInputType(mozilla::dom::HTMLInputElement* aInputElement)
    : ButtonInputTypeBase(aInputElement)
  {}
};

// input type=submit
class SubmitInputType : public ButtonInputTypeBase
{
public:
  static InputType*
  Create(mozilla::dom::HTMLInputElement* aInputElement, void* aMemory)
  {
    return new (aMemory) SubmitInputType(aInputElement);
  }

private:
  explicit SubmitInputType(mozilla::dom::HTMLInputElement* aInputElement)
    : ButtonInputTypeBase(aInputElement)
  {}
};

#endif /* ButtonInputTypes_h__ */
