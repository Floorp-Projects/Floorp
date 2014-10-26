/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CORSMode_h_
#define CORSMode_h_

namespace mozilla {

enum CORSMode {
  /**
   * The default of not using CORS to validate cross-origin loads.
   */
  CORS_NONE,

  /**
   * Validate cross-site loads using CORS, but do not send any credentials
   * (cookies, HTTP auth logins, etc) along with the request.
   */
  CORS_ANONYMOUS,

  /**
   * Validate cross-site loads using CORS, and send credentials such as cookies
   * and HTTP auth logins along with the request.
   */
  CORS_USE_CREDENTIALS
};

} // namespace mozilla

#endif /* CORSMode_h_ */
