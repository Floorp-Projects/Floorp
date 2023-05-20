/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that cookies with domain equal to full host name are listed.
// E.g., ".example.org" vs. example.org). Bug 1149497.

add_task(async function () {
  await openTabAndSetupStorage(MAIN_DOMAIN + "storage-cookies.html");

  await checkState([
    [
      ["cookies", "http://test1.example.org"],
      [
        getCookieId("test1", ".test1.example.org", "/browser"),
        getCookieId("test2", "test1.example.org", "/browser"),
        getCookieId("test3", ".test1.example.org", "/browser"),
        getCookieId("test4", "test1.example.org", "/browser"),
        getCookieId("test5", ".test1.example.org", "/browser"),
      ],
    ],
  ]);
});
