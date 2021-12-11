/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Parent config file for all devtools xpcshell files.
module.exports = {
  extends: ["plugin:mozilla/xpcshell-test"],
  rules: {
    // Allow non-camelcase so that run_test doesn't produce a warning.
    camelcase: "off",
    "block-scoped-var": "off",
    // Tests don't have to cleanup observers
    "mozilla/balanced-observers": 0,
    // Tests can always import anything.
    "mozilla/reject-some-requires": "off",
  },
};
