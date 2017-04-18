/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_css_URLMatchingFunction_h
#define mozilla_css_URLMatchingFunction_h

namespace mozilla {
namespace css {

/**
 * Enum defining the type of URL matching function for a @-moz-document rule
 * condition.
 */
enum class URLMatchingFunction {
  eURL = 0,
  eURLPrefix,
  eDomain,
  eRegExp,
};

} // namespace css
} // namespace mozilla

#endif // mozilla_css_URLMatchingFunction_h
