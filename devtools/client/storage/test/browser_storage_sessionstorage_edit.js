/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Basic test to check the editing of localStorage.

"use strict";

add_task(async function () {
  await openTabAndSetupStorage(
    MAIN_DOMAIN_SECURED + "storage-sessionstorage.html"
  );

  await selectTreeItem(["sessionStorage", "https://test1.example.org"]);

  await editCell("TestSS1", "name", "newTestSS1");
  await editCell("newTestSS1", "value", "newValueSS1");

  await editCell("TestSS3", "name", "newTestSS3");
  await editCell("newTestSS3", "value", "newValueSS3");

  await editCell("TestSS5", "name", "newTestSS5");
  await editCell("newTestSS5", "value", "newValueSS5");
});
