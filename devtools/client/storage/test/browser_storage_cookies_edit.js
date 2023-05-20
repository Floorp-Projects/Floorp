/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Basic test to check the editing of cookies.

"use strict";

add_task(async function () {
  await openTabAndSetupStorage(MAIN_DOMAIN + "storage-cookies.html");
  showAllColumns(true);

  let id = getCookieId("test3", ".test1.example.org", "/browser");
  await editCell(id, "name", "newTest3");

  id = getCookieId("newTest3", ".test1.example.org", "/browser");
  await editCell(id, "host", "test1.example.org");

  id = getCookieId("newTest3", "test1.example.org", "/browser");
  await editCell(id, "path", "/");

  id = getCookieId("newTest3", "test1.example.org", "/");
  await editCell(id, "expires", "Tue, 14 Feb 2040 17:41:14 GMT");
  await editCell(id, "value", "newValue3");
  await editCell(id, "isSecure", "true");
  await editCell(id, "isHttpOnly", "true");
});
