/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from head.js */

"use strict";

// test without target switching
add_task(async function() {
  await testNavigation();
});

// test with target switching enabled
add_task(async function() {
  enableTargetSwitching();
  await testNavigation();
});

async function testNavigation() {
  const URL1 = buildURLWithContent(
    "example.com",
    `<h1>example.com</h1>` +
      `<script>localStorage.setItem("lorem", "ipsum");</script>`
  );
  const URL2 = buildURLWithContent(
    "example.net",
    `<h1>example.net</h1>` +
      `<script>localStorage.setItem("foo", "bar");</script>`
  );

  // open tab
  await openTabAndSetupStorage(URL1);
  const doc = gPanelWindow.document;

  // Check first domain
  // check that both host appear in the storage tree
  checkTree(doc, ["localStorage", "http://example.com"]);
  // check the table for values
  await selectTreeItem(["localStorage", "http://example.com"]);
  checkStorageData("lorem", "ipsum");

  // clear up local storage data before navigating
  info("Cleaning up localStorage…");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    const win = content.wrappedJSObject;
    await win.localStorage.clear();
  });

  // Check second domain
  await navigateTo(URL2);
  // wait for storage tree refresh, and check host
  info("Waiting for storage tree to refresh and show correct host…");
  await waitUntil(() => isInTree(doc, ["localStorage", "http://example.net"]));

  // reload the current tab and check data
  await refreshTab();
  // wait for storage tree refresh, and check host
  info("Waiting for storage tree to refresh and show correct host…");
  await waitUntil(() => isInTree(doc, ["localStorage", "http://example.net"]));
  // check the table for values
  await selectTreeItem(["localStorage", "http://example.net"]);
  checkStorageData("foo", "bar");
}
