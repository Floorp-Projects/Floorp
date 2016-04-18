/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Basic test to check the editing of localStorage.

"use strict";

add_task(function* () {
  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-sessionstorage.html");

  yield selectTreeItem(["sessionStorage", "http://test1.example.org"]);

  yield editCell("TestSS1", "name", "newTestSS1");
  yield editCell("newTestSS1", "value", "newValueSS1");

  yield editCell("TestSS3", "name", "newTestSS3");
  yield editCell("newTestSS3", "value", "newValueSS3");

  yield editCell("TestSS5", "name", "newTestSS5");
  yield editCell("newTestSS5", "value", "newValueSS5");

  yield finishTests();
});
