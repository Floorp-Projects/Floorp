/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_css_DocumentMatchingFunction_h
#define mozilla_css_DocumentMatchingFunction_h

namespace mozilla {
namespace css {

/**
 * Enum defining the type of matching function for a @-moz-document rule
 * condition.
 */
enum class DocumentMatchingFunction {
  URL = 0,
  URLPrefix,
  Domain,
  RegExp,
};

} // namespace css
} // namespace mozilla

#endif // mozilla_css_DocumentMatchingFunction_h
