/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.exports = {
  // Extend from the devtools eslintrc.
  extends: "../../../.eslintrc.js",

  rules: {
    // This rule was introduced for the DevTools HTML initiative, the goal was
    // to avoid requiring helpers unavailable in a regular content page, to
    // use DevTools as a regular webapplication. Should be reviewed in
    // Bug 1591091.
    "mozilla/reject-some-requires": [
      "error",
      "^(chrome|chrome:.*|resource:.*|devtools/server/.*|.*\\.jsm)$",
    ],
  },
};
