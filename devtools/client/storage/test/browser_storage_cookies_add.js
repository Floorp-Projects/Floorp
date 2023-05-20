/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Basic test to check the adding of cookies.

"use strict";

add_task(async function () {
  const TEST_URL = MAIN_DOMAIN + "storage-cookies.html";
  await openTabAndSetupStorage(TEST_URL);
  showAllColumns(true);

  await performAdd(["cookies", "http://test1.example.org"]);
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
  await performAdd(["cookies", "http://test1.example.org"]);
  await performAdd(["cookies", "http://test1.example.org"]);
  privateWindow.close();
});
