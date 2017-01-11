/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_U2FAuthenticator_h
#define mozilla_dom_U2FAuthenticator_h

#include "nsIU2FToken.h"
#include "USBToken.h"

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

typedef nsCOMPtr<nsIU2FToken> Authenticator;

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_U2FAuthenticator_h
