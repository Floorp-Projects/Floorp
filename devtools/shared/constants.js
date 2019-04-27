/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Constants used in various panels, shared between client and the server.
 */

/* Accessibility Panel ====================================================== */

exports.accessibility = {
  // List of audit types.
  AUDIT_TYPE: {
    CONTRAST: "CONTRAST",
  },
  // Constants associated with WCAG guidelines score system:
  // failing -> AA -> AAA;
  SCORES: {
    FAIL: "fail",
    AA: "AA",
    AAA: "AAA",
  },
};
