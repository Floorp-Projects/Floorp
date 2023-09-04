/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Basic test to check the adding of cookies.

"use strict";

add_task(async function () {
  const TEST_URL = MAIN_DOMAIN + "storage-cookies.html";
  await openTabAndSetupStorage(TEST_URL);
  showAllColumns(true);

  const rowId = await performAdd(["cookies", "http://test1.example.org"]);
  checkCookieData(rowId);

  await performAdd(["cookies", "http://test1.example.org"]);
  await performAdd(["cookies", "http://test1.example.org"]);
  await performAdd(["cookies", "http://test1.example.org"]);
  await performAdd(["cookies", "http://test1.example.org"]);

  info("Check it does work in private tabs too");
  const privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  ok(PrivateBrowsingUtils.isWindowPrivate(privateWindow), "window is private");
  const privateTab = await addTab(TEST_URL, { window: privateWindow });
  await openStoragePanel({ tab: privateTab });
  const privateTabRowId = await performAdd([
    "cookies",
    "http://test1.example.org",
  ]);
  checkCookieData(privateTabRowId);

  await performAdd(["cookies", "http://test1.example.org"]);
  privateWindow.close();
});

function checkCookieData(rowId) {
  is(getCellValue(rowId, "value"), "value", "value is correct");
  is(getCellValue(rowId, "host"), "test1.example.org", "host is correct");
  is(getCellValue(rowId, "path"), "/", "path is correct");
  const actualExpiry = Math.floor(
    new Date(getCellValue(rowId, "expires")) / 1000
  );
  const ONE_DAY_IN_SECONDS = 60 * 60 * 24;
  const time = Math.floor(new Date(getCellValue(rowId, "creationTime")) / 1000);
  const expectedExpiry = time + ONE_DAY_IN_SECONDS;
  ok(actualExpiry - expectedExpiry <= 2, "expiry is in expected range");
  is(getCellValue(rowId, "size"), "43", "size is correct");
  is(getCellValue(rowId, "isHttpOnly"), "false", "httpOnly is not set");
  is(getCellValue(rowId, "isSecure"), "false", "secure is not set");
  is(getCellValue(rowId, "sameSite"), "Lax", "sameSite is Lax");
  is(getCellValue(rowId, "hostOnly"), "true", "hostOnly is not set");
}
