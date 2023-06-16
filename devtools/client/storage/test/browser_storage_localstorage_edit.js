/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Basic test to check the editing of localStorage.

"use strict";

add_task(async function () {
  await openTabAndSetupStorage(
    MAIN_DOMAIN_SECURED + "storage-localstorage.html"
  );

  await selectTreeItem(["localStorage", "https://test1.example.org"]);

  await editCell("TestLS1", "name", "newTestLS1");
  await editCell("newTestLS1", "value", "newValueLS1");

  await editCell("TestLS3", "name", "newTestLS3");
  await editCell("newTestLS3", "value", "newValueLS3");

  await editCell("TestLS5", "name", "newTestLS5");
  await editCell("newTestLS5", "value", "newValueLS5");
});
