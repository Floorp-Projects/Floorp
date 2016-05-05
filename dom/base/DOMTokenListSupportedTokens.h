/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Definitions of supported tokens data types for nsDOMTokenList.  This is in a
 * separate header so Element.h can include it too.
 */

#ifndef mozilla_dom_DOMTokenListSupportedTokens_h
#define mozilla_dom_DOMTokenListSupportedTokens_h

namespace mozilla {
namespace dom {

// A single supported token.
typedef const char* const DOMTokenListSupportedToken;

// An array of supported tokens.  This should end with a null
// DOMTokenListSupportedToken to indicate array termination.  A null value for
// the DOMTokenListSupportedTokenArray means there is no definition of supported
// tokens for the given DOMTokenList.  This should generally be a static table,
// or at least outlive the DOMTokenList whose constructor it's passed to.
typedef DOMTokenListSupportedToken* DOMTokenListSupportedTokenArray;

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_DOMTokenListSupportedTokens_h
