/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../shared/test/shared-head.js */

"use strict";

// Test that the samesite cookie attribute is displayed correctly.

add_task(async function() {
  await openTabAndSetupStorage(MAIN_DOMAIN + "storage-cookies-samesite.html");

  const id1 = getCookieId("test1", "test1.example.org",
                        "/browser/devtools/client/storage/test/");
  const id2 = getCookieId("test2", "test1.example.org",
                        "/browser/devtools/client/storage/test/");
  const id3 = getCookieId("test3", "test1.example.org",
                        "/browser/devtools/client/storage/test/");

  await checkState([
    [
      ["cookies", "http://test1.example.org"],
      [ id1, id2, id3 ]
    ]
  ]);

  const sameSite1 = getRowValues(id1).sameSite;
  const sameSite2 = getRowValues(id2).sameSite;
  const sameSite3 = getRowValues(id3).sameSite;

  is(sameSite1, "Unset", `sameSite1 is "Unset"`);
  is(sameSite2, "Lax", `sameSite2 is "Lax"`);
  is(sameSite3, "Strict", `sameSite3 is "Strict"`);

  await finishTests();
});
