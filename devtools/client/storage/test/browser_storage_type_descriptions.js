/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Basic test to assert that the descriptions for the different storage types
// are correctly displayed and the links referring to pages with further
// information are set.

"use strict";

const getStorageTypeURL = require("resource://devtools/client/storage/utils/doc-utils.js");

const storeItems = [
  "Cache",
  "cookies",
  "indexedDB",
  "localStorage",
  "sessionStorage",
];

/**
 * Test that the desired number of tree items are present
 */
function testTree() {
  const doc = gPanelWindow.document;
  for (const type of storeItems) {
    ok(
      doc.querySelector(`[data-id='${JSON.stringify([type])}']`),
      `Tree item ${type} should be present in the storage tree`
    );
  }
}

/**
 * Test that description is shown for each of the tree items
 */
const testDescriptions = async function () {
  const doc = gPanelWindow.document;
  const win = doc.defaultView;
  // Expand all nodes so that the synthesized click event actually works
  gUI.tree.expandAll();

  // Click the tree items and wait for the content to be updated
  for (const type of storeItems) {
    await selectTreeItem([type]);

    // Check whether the table is hidden
    is(
      win.getComputedStyle(doc.querySelector(".table-widget-body")).display,
      "none",
      "Table must be hidden"
    );

    // Check whether the description shown
    is(
      win.getComputedStyle(doc.querySelector(".table-widget-empty-text"))
        .display,
      "block",
      "Description for the type must be shown"
    );

    // Check learn more link
    const learnMoreLink = doc.querySelector(".table-widget-empty-text > a");
    ok(learnMoreLink, "There is a [Learn more] link");
    const expectedURL = getStorageTypeURL(type);
    is(
      learnMoreLink.href,
      expectedURL,
      `Learn more link refers to ${expectedURL}`
    );
  }
};

add_task(async function () {
  await openTabAndSetupStorage(MAIN_DOMAIN + "storage-empty-objectstores.html");

  testTree();
  await testDescriptions();
});
