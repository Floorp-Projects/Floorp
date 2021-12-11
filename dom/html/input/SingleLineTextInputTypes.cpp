/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SingleLineTextInputTypes.h"

#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/TextUtils.h"
#include "HTMLSplitOnSpacesTokenizer.h"
#include "nsContentUtils.h"
#include "nsCRTGlue.h"
#include "nsIIDNService.h"
#include "nsIIOService.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"

using namespace mozilla;
using namespace mozilla::dom;

bool SingleLineTextInputTypeBase::IsMutable() const {
  return !mInputElement->IsDisabled() &&
         !mInputElement->HasAttr(kNameSpaceID_None, nsGkAtoms::readonly);
}

bool SingleLineTextInputTypeBase::IsTooLong() const {
  int32_t maxLength = mInputElement->MaxLength();

  // Maxlength of -1 means attribute isn't set or parsing error.
  if (maxLength == -1) {
    return false;
  }

  int32_t textLength = mInputElement->InputTextLength(CallerType::System);

  return textLength > maxLength;
}

bool SingleLineTextInputTypeBase::IsTooShort() const {
  int32_t minLength = mInputElement->MinLength();

  // Minlength of -1 means attribute isn't set or parsing error.
  if (minLength == -1) {
    return false;
  }

  int32_t textLength = mInputElement->InputTextLength(CallerType::System);

  return textLength && textLength < minLength;
}

bool SingleLineTextInputTypeBase::IsValueMissing() const {
  if (!mInputElement->IsRequired()) {
    return false;
  }

  if (!IsMutable()) {
    return false;
  }

  return IsValueEmpty();
}

Maybe<bool> SingleLineTextInputTypeBase::HasPatternMismatch() const {
  if (!mInputElement->HasPatternAttribute()) {
    return Some(false);
  }

  nsAutoString pattern;
  if (!mInputElement->GetAttr(kNameSpaceID_None, nsGkAtoms::pattern, pattern)) {
    return Some(false);
  }

  nsAutoString value;
  GetNonFileValueInternal(value);

  if (value.IsEmpty()) {
    return Some(false);
  }

  Document* doc = mInputElement->OwnerDoc();
  Maybe<bool> result = nsContentUtils::IsPatternMatching(value, pattern, doc);
  return result ? Some(!*result) : Nothing();
}

/* input type=url */

bool URLInputType::HasTypeMismatch() const {
  nsAutoString value;
  GetNonFileValueInternal(value);

  if (value.IsEmpty()) {
    return false;
  }

  /**
   * TODO:
   * The URL is not checked as the HTML5 specifications want it to be because
   * there is no code to check for a valid URI/IRI according to 3986 and 3987
   * RFC's at the moment, see bug 561586.
   *
   * RFC 3987 (IRI) implementation: bug 42899
   *
   * HTML5 specifications:
   * http://dev.w3.org/html5/spec/infrastructure.html#valid-url
   */
  nsCOMPtr<nsIIOService> ioService = do_GetIOService();
  nsCOMPtr<nsIURI> uri;

  return !NS_SUCCEEDED(ioService->NewURI(NS_ConvertUTF16toUTF8(value), nullptr,
                                         nullptr, getter_AddRefs(uri)));
}

nsresult URLInputType::GetTypeMismatchMessage(nsAString& aMessage) {
  return nsContentUtils::GetMaybeLocalizedString(
      nsContentUtils::eDOM_PROPERTIES, "FormValidationInvalidURL",
      mInputElement->OwnerDoc(), aMessage);
}

/* input type=email */

bool EmailInputType::HasTypeMismatch() const {
  nsAutoString value;
  GetNonFileValueInternal(value);

  if (value.IsEmpty()) {
    return false;
  }

  return mInputElement->HasAttr(kNameSpaceID_None, nsGkAtoms::multiple)
             ? !IsValidEmailAddressList(value)
             : !IsValidEmailAddress(value);
}

bool EmailInputType::HasBadInput() const {
  // With regards to suffering from bad input the spec says that only the
  // punycode conversion works, so we don't care whether the email address is
  // valid or not here. (If the email address is invalid then we will be
  // suffering from a type mismatch.)
  nsAutoString value;
  nsAutoCString unused;
  uint32_t unused2;
  GetNonFileValueInternal(value);
  HTMLSplitOnSpacesTokenizer tokenizer(value, ',');
  while (tokenizer.hasMoreTokens()) {
    if (!PunycodeEncodeEmailAddress(tokenizer.nextToken(), unused, &unused2)) {
      return true;
    }
  }
  return false;
}

nsresult EmailInputType::GetTypeMismatchMessage(nsAString& aMessage) {
  return nsContentUtils::GetMaybeLocalizedString(
      nsContentUtils::eDOM_PROPERTIES, "FormValidationInvalidEmail",
      mInputElement->OwnerDoc(), aMessage);
}

nsresult EmailInputType::GetBadInputMessage(nsAString& aMessage) {
  return nsContentUtils::GetMaybeLocalizedString(
      nsContentUtils::eDOM_PROPERTIES, "FormValidationInvalidEmail",
      mInputElement->OwnerDoc(), aMessage);
}

/* static */
bool EmailInputType::IsValidEmailAddressList(const nsAString& aValue) {
  HTMLSplitOnSpacesTokenizer tokenizer(aValue, ',');

  while (tokenizer.hasMoreTokens()) {
    if (!IsValidEmailAddress(tokenizer.nextToken())) {
      return false;
    }
  }

  return !tokenizer.separatorAfterCurrentToken();
}

/* static */
bool EmailInputType::IsValidEmailAddress(const nsAString& aValue) {
  // Email addresses can't be empty and can't end with a '.' or '-'.
  if (aValue.IsEmpty() || aValue.Last() == '.' || aValue.Last() == '-') {
    return false;
  }

  uint32_t atPos;
  nsAutoCString value;
  if (!PunycodeEncodeEmailAddress(aValue, value, &atPos) ||
      atPos == (uint32_t)kNotFound || atPos == 0 ||
      atPos == value.Length() - 1) {
    // Could not encode, or "@" was not found, or it was at the start or end
    // of the input - in all cases, not a valid email address.
    return false;
  }

  uint32_t length = value.Length();
  uint32_t i = 0;

  // Parsing the username.
  for (; i < atPos; ++i) {
    char16_t c = value[i];

    // The username characters have to be in this list to be valid.
    if (!(IsAsciiAlpha(c) || IsAsciiDigit(c) || c == '.' || c == '!' ||
          c == '#' || c == '$' || c == '%' || c == '&' || c == '\'' ||
          c == '*' || c == '+' || c == '-' || c == '/' || c == '=' ||
          c == '?' || c == '^' || c == '_' || c == '`' || c == '{' ||
          c == '|' || c == '}' || c == '~')) {
      return false;
    }
  }

  // Skip the '@'.
  ++i;

  // The domain name can't begin with a dot or a dash.
  if (value[i] == '.' || value[i] == '-') {
    return false;
  }

  // Parsing the domain name.
  for (; i < length; ++i) {
    char16_t c = value[i];

    if (c == '.') {
      // A dot can't follow a dot or a dash.
      if (value[i - 1] == '.' || value[i - 1] == '-') {
        return false;
      }
    } else if (c == '-') {
      // A dash can't follow a dot.
      if (value[i - 1] == '.') {
        return false;
      }
    } else if (!(IsAsciiAlpha(c) || IsAsciiDigit(c) || c == '-')) {
      // The domain characters have to be in this list to be valid.
      return false;
    }
  }

  return true;
}

/* static */
bool EmailInputType::PunycodeEncodeEmailAddress(const nsAString& aEmail,
                                                nsAutoCString& aEncodedEmail,
                                                uint32_t* aIndexOfAt) {
  nsAutoCString value = NS_ConvertUTF16toUTF8(aEmail);
  *aIndexOfAt = (uint32_t)value.FindChar('@');

  if (*aIndexOfAt == (uint32_t)kNotFound || *aIndexOfAt == value.Length() - 1) {
    aEncodedEmail = value;
    return true;
  }

  nsCOMPtr<nsIIDNService> idnSrv = do_GetService(NS_IDNSERVICE_CONTRACTID);
  if (!idnSrv) {
    NS_ERROR("nsIIDNService isn't present!");
    return false;
  }

  uint32_t indexOfDomain = *aIndexOfAt + 1;

  const nsDependentCSubstring domain = Substring(value, indexOfDomain);
  bool ace;
  if (NS_SUCCEEDED(idnSrv->IsACE(domain, &ace)) && !ace) {
    nsAutoCString domainACE;
    if (NS_FAILED(idnSrv->ConvertUTF8toACE(domain, domainACE))) {
      return false;
    }
    value.Replace(indexOfDomain, domain.Length(), domainACE);
  }

  aEncodedEmail = value;
  return true;
}
