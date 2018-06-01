/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Basic test to check the editing of cookies with the keyboard.

"use strict";

add_task(async function() {
  await openTabAndSetupStorage(MAIN_DOMAIN + "storage-cookies.html");
  showAllColumns(true);
  showColumn("uniqueKey", false);

  const id = getCookieId("test4", "test1.example.org", "/browser");
  await startCellEdit(id, "name");
  await typeWithTerminator("test6", "KEY_Tab");
  await typeWithTerminator(".example.org", "KEY_Tab");
  await typeWithTerminator("/", "KEY_Tab");
  await typeWithTerminator("Tue, 25 Dec 2040 12:00:00 GMT", "KEY_Tab");
  await typeWithTerminator("test6value", "KEY_Tab");
  await typeWithTerminator("false", "KEY_Tab");
  await typeWithTerminator("false", "KEY_Tab");

  await finishTests();
});
