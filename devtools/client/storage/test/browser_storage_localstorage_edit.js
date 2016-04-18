/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Basic test to check the editing of localStorage.

"use strict";

add_task(function* () {
  yield openTabAndSetupStorage(MAIN_DOMAIN + "storage-localstorage.html");

  yield selectTreeItem(["localStorage", "http://test1.example.org"]);

  yield editCell("TestLS1", "name", "newTestLS1");
  yield editCell("newTestLS1", "value", "newValueLS1");

  yield editCell("TestLS3", "name", "newTestLS3");
  yield editCell("newTestLS3", "value", "newValueLS3");

  yield editCell("TestLS5", "name", "newTestLS5");
  yield editCell("newTestLS5", "value", "newValueLS5");

  yield finishTests();
});
