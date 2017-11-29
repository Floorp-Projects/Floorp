/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../framework/test/shared-head.js */

"use strict";

// Test that the samesite cookie attribute is displayed correctly.

add_task(function* () {
  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-cookies-samesite.html");

  let id1 = getCookieId("test1", "test1.example.org",
                        "/browser/devtools/client/storage/test/");
  let id2 = getCookieId("test2", "test1.example.org",
                        "/browser/devtools/client/storage/test/");
  let id3 = getCookieId("test3", "test1.example.org",
                        "/browser/devtools/client/storage/test/");

  yield checkState([
    [
      ["cookies", "http://test1.example.org"],
      [ id1, id2, id3 ]
    ]
  ]);

  let sameSite1 = getRowValues(id1).sameSite;
  let sameSite2 = getRowValues(id2).sameSite;
  let sameSite3 = getRowValues(id3).sameSite;

  is(sameSite1, "Unset", `sameSite1 is "Unset"`);
  is(sameSite2, "Lax", `sameSite2 is "Lax"`);
  is(sameSite3, "Strict", `sameSite3 is "Strict"`);

  yield finishTests();
});
