/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.exports = {
  env: { browser: true },
  globals: {
    define: true,
  },
  rules: {
    // See bug 1288147, the devtools front-end wants to be able to run in
    // content privileged windows, where ownerGlobal doesn't exist.
    "mozilla/use-ownerGlobal": "off",
  },
  overrides: [
    {
      // Tests verify the exact source code of these functions
      files: ["inspector/markup/test/doc_markup_events_*.html"],
      rules: {
        "no-unused-vars": "off",
      },
    },
  ],
};
