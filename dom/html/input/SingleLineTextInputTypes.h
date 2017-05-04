/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SingleLineTextInputTypes_h__
#define SingleLineTextInputTypes_h__

#include "InputType.h"

class SingleLineTextInputTypeBase : public ::InputType
{
public:
  ~SingleLineTextInputTypeBase() override {}

  bool IsTooLong() const override;
  bool IsTooShort() const override;

protected:
  explicit SingleLineTextInputTypeBase(
    mozilla::dom::HTMLInputElement* aInputElement)
      : InputType(aInputElement)
  {}
};

// input type=text
class TextInputType : public SingleLineTextInputTypeBase
{
public:
  static InputType*
  Create(mozilla::dom::HTMLInputElement* aInputElement, void* aMemory)
  {
    return new (aMemory) TextInputType(aInputElement);
  }

private:
  explicit TextInputType(mozilla::dom::HTMLInputElement* aInputElement)
    : SingleLineTextInputTypeBase(aInputElement)
  {}
};

// input type=search
class SearchInputType : public SingleLineTextInputTypeBase
{
public:
  static InputType*
  Create(mozilla::dom::HTMLInputElement* aInputElement, void* aMemory)
  {
    return new (aMemory) SearchInputType(aInputElement);
  }

private:
  explicit SearchInputType(mozilla::dom::HTMLInputElement* aInputElement)
    : SingleLineTextInputTypeBase(aInputElement)
  {}
};

// input type=tel
class TelInputType : public SingleLineTextInputTypeBase
{
public:
  static InputType*
  Create(mozilla::dom::HTMLInputElement* aInputElement, void* aMemory)
  {
    return new (aMemory) TelInputType(aInputElement);
  }

private:
  explicit TelInputType(mozilla::dom::HTMLInputElement* aInputElement)
    : SingleLineTextInputTypeBase(aInputElement)
  {}
};

// input type=url
class URLInputType : public SingleLineTextInputTypeBase
{
public:
  static InputType*
  Create(mozilla::dom::HTMLInputElement* aInputElement, void* aMemory)
  {
    return new (aMemory) URLInputType(aInputElement);
  }

private:
  explicit URLInputType(mozilla::dom::HTMLInputElement* aInputElement)
    : SingleLineTextInputTypeBase(aInputElement)
  {}
};

// input type=email
class EmailInputType : public SingleLineTextInputTypeBase
{
public:
  static InputType*
  Create(mozilla::dom::HTMLInputElement* aInputElement, void* aMemory)
  {
    return new (aMemory) EmailInputType(aInputElement);
  }

private:
  explicit EmailInputType(mozilla::dom::HTMLInputElement* aInputElement)
    : SingleLineTextInputTypeBase(aInputElement)
  {}
};

// input type=password
class PasswordInputType : public SingleLineTextInputTypeBase
{
public:
  static InputType*
  Create(mozilla::dom::HTMLInputElement* aInputElement, void* aMemory)
  {
    return new (aMemory) PasswordInputType(aInputElement);
  }

private:
  explicit PasswordInputType(mozilla::dom::HTMLInputElement* aInputElement)
    : SingleLineTextInputTypeBase(aInputElement)
  {}
};

#endif /* SingleLineTextInputTypes_h__ */
