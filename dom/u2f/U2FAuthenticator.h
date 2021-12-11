/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_U2FAuthenticator_h
#define mozilla_dom_U2FAuthenticator_h

#include "mozilla/MozPromise.h"

namespace mozilla {
namespace dom {

// These enumerations are defined in the FIDO U2F Javascript API under the
// interface "ErrorCode" as constant integers, and thus in the U2F.webidl file.
// Any changes to these must occur in both locations.
enum class ErrorCode {
  OK = 0,
  OTHER_ERROR = 1,
  BAD_REQUEST = 2,
  CONFIGURATION_UNSUPPORTED = 3,
  DEVICE_INELIGIBLE = 4,
  TIMEOUT = 5
};

typedef MozPromise<nsString, ErrorCode, false> U2FPromise;

constexpr auto kRequiredU2FVersion = u"U2F_V2"_ns;

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_U2FAuthenticator_h
