/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function () {
  const URL1 = buildURLWithContent(
    "example.com",
    `<h1>example.com</h1>` +
      `<script>sessionStorage.setItem("lorem", "ipsum");</script>`
  );
  const URL2 = buildURLWithContent(
    "example.net",
    `<h1>example.net</h1>` +
      `<script>sessionStorage.setItem("foo", "bar");</script>`
  );

  // open tab
  await openTabAndSetupStorage(URL1);
  const doc = gPanelWindow.document;

  // Check first domain
  // check that both host appear in the storage tree
  checkTree(doc, ["sessionStorage", "https://example.com"]);
  // check the table for values
  await selectTreeItem(["sessionStorage", "https://example.com"]);
  checkStorageData("lorem", "ipsum");

  // clear up session storage data before navigating
  info("Cleaning up sessionStorage…");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    const win = content.wrappedJSObject;
    await win.sessionStorage.clear();
  });

  // Check second domain
  await navigateTo(URL2);
  // wait for storage tree refresh, and check host
  info("Waiting for storage tree to refresh and show correct host…");
  await waitUntil(() =>
    isInTree(doc, ["sessionStorage", "https://example.net"])
  );

  ok(
    !isInTree(doc, ["sessionStorage", "https://example.com"]),
    "example.com item is not in the tree anymore"
  );

  // check the table for values
  await selectTreeItem(["sessionStorage", "https://example.net"]);
  checkStorageData("foo", "bar");

  info("Check that the sessionStorage node still has the expected label");
  is(
    getTreeNodeLabel(doc, ["sessionStorage"]),
    "Session Storage",
    "sessionStorage item is properly displayed"
  );
});
