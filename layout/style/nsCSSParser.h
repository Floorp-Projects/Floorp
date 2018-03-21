/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* parsing of CSS stylesheets, based on a token stream from the CSS scanner */

#ifndef nsCSSParser_h___
#define nsCSSParser_h___


namespace mozilla {
namespace css {
class Loader;
} // namespace css
} // namespace mozilla

// Define this dummy class so there are fewer call sites to change when the old
// style system code is compiled out.
class nsCSSParser
{
public:
  explicit nsCSSParser(mozilla::css::Loader* aLoader = nullptr) {}
};


#endif /* nsCSSParser_h___ */
