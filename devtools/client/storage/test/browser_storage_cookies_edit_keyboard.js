/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Basic test to check the editing of cookies with the keyboard.

"use strict";

add_task(function* () {
  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-cookies.html");
  showAllColumns(true);
  showColumn("uniqueKey", false);

  let id = getCookieId("test4", "test1.example.org", "/browser");
  yield startCellEdit(id, "name");
  yield typeWithTerminator("test6", "KEY_Tab");
  yield typeWithTerminator(".example.org", "KEY_Tab");
  yield typeWithTerminator("/", "KEY_Tab");
  yield typeWithTerminator("Tue, 25 Dec 2040 12:00:00 GMT", "KEY_Tab");
  yield typeWithTerminator("test6value", "KEY_Tab");
  yield typeWithTerminator("false", "KEY_Tab");
  yield typeWithTerminator("false", "KEY_Tab");

  yield finishTests();
});
