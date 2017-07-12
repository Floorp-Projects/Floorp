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
  bool IsValueMissing() const override;
  bool HasPatternMismatch() const override;

protected:
  explicit SingleLineTextInputTypeBase(
    mozilla::dom::HTMLInputElement* aInputElement)
      : InputType(aInputElement)
  {}

  bool IsMutable() const override;
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

  bool HasTypeMismatch() const override;

  nsresult GetTypeMismatchMessage(nsXPIDLString& aMessage) override;

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

  bool HasTypeMismatch() const override;
  bool HasBadInput() const override;

  nsresult GetTypeMismatchMessage(nsXPIDLString& aMessage) override;
  nsresult GetBadInputMessage(nsXPIDLString& aMessage) override;

private:
  explicit EmailInputType(mozilla::dom::HTMLInputElement* aInputElement)
    : SingleLineTextInputTypeBase(aInputElement)
  {}

  /**
   * This helper method returns true if aValue is a valid email address.
   * This is following the HTML5 specification:
   * http://dev.w3.org/html5/spec/forms.html#valid-e-mail-address
   *
   * @param aValue  the email address to check.
   * @result        whether the given string is a valid email address.
   */
  static bool IsValidEmailAddress(const nsAString& aValue);

  /**
   * This helper method returns true if aValue is a valid email address list.
   * Email address list is a list of email address separated by comas (,) which
   * can be surrounded by space charecters.
   * This is following the HTML5 specification:
   * http://dev.w3.org/html5/spec/forms.html#valid-e-mail-address-list
   *
   * @param aValue  the email address list to check.
   * @result        whether the given string is a valid email address list.
   */
  static bool IsValidEmailAddressList(const nsAString& aValue);

  /**
   * Takes aEmail and attempts to convert everything after the first "@"
   * character (if anything) to punycode before returning the complete result
   * via the aEncodedEmail out-param. The aIndexOfAt out-param is set to the
   * index of the "@" character.
   *
   * If no "@" is found in aEmail, aEncodedEmail is simply set to aEmail and
   * the aIndexOfAt out-param is set to kNotFound.
   *
   * Returns true in all cases unless an attempt to punycode encode fails. If
   * false is returned, aEncodedEmail has not been set.
   *
   * This function exists because ConvertUTF8toACE() splits on ".", meaning that
   * for 'user.name@sld.tld' it would treat "name@sld" as a label. We want to
   * encode the domain part only.
   */
 static bool PunycodeEncodeEmailAddress(const nsAString& aEmail,
                                        nsAutoCString& aEncodedEmail,
                                        uint32_t* aIndexOfAt);
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
