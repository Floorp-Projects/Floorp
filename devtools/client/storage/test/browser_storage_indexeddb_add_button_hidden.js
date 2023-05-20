/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the add button is hidden for the indexedDB storage type.
add_task(async function () {
  await openTabAndSetupStorage(
    MAIN_DOMAIN_SECURED + "storage-empty-objectstores.html"
  );

  info("Select an indexedDB item");
  const idbItem = ["indexedDB", "https://test1.example.org", "idb1 (default)"];
  await selectTreeItem(idbItem);
  checkAddButtonState({ expectHidden: true });

  // Note: test only one of the other stoage types to check that the logic to
  // find the add button is not outdated. Other storage types have more detailed
  // tests focused on the add feature.
  info("Select a cookie item");
  const cookieItem = ["cookies", "https://test1.example.org"];
  await selectTreeItem(cookieItem);
  checkAddButtonState({ expectHidden: false });
});

function checkAddButtonState({ expectHidden }) {
  const toolbar = gPanelWindow.document.getElementById("storage-toolbar");
  const addButton = toolbar.querySelector("#add-button");
  is(
    addButton.hidden,
    expectHidden,
    `The add button is ${expectHidden ? "hidden" : "displayed"}`
  );
}
