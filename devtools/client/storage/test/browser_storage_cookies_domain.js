/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../framework/test/shared-head.js */

"use strict";

// Test that cookies with domain equal to full host name are listed.
// E.g., ".example.org" vs. example.org). Bug 1149497.

add_task(function* () {
  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-cookies.html");

  yield checkState([
    [
      ["cookies", "test1.example.org"],
      [
        getCookieId("test1", ".test1.example.org", "/browser"),
        getCookieId("test2", "test1.example.org", "/browser"),
        getCookieId("test3", ".test1.example.org", "/browser"),
        getCookieId("test4", "test1.example.org", "/browser"),
        getCookieId("test5", ".test1.example.org", "/browser")
      ]
    ],
  ]);

  yield finishTests();
});
