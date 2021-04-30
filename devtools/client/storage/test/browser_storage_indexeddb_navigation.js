/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from head.js */

"use strict";

requestLongerTimeout(3);

add_task(async function() {
  const URL1 = URL_ROOT_COM + "storage-indexeddb-simple.html";
  const URL2 = URL_ROOT_NET + "storage-indexeddb-simple-alt.html";

  // open tab
  await openTabAndSetupStorage(URL1);
  const doc = gPanelWindow.document;

  // Check first domain
  // check that host appears in the storage tree
  checkTree(doc, ["indexedDB", "http://example.com"]);
  // check the table for values
  await selectTreeItem([
    "indexedDB",
    "http://example.com",
    "db (default)",
    "store",
  ]);
  checkStorageData("lorem", JSON.stringify({ key: "lorem", value: "ipsum" }));

  // clear db before navigating to a new domain
  info("Removing database…");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    const win = content.wrappedJSObject;
    await win.clear();
  });

  // Check second domain
  await navigateTo(URL2);
  info("Creating database in the second domain…");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    const win = content.wrappedJSObject;
    await win.setup();
  });
  // wait for storage tree refresh, and check host
  info("Checking storage tree…");
  await waitUntil(() => isInTree(doc, ["indexedDB", "http://example.net"]));
  // TODO: select tree and check on storage data.
  // We cannot do it yet since we do not detect newly created indexed db's when
  // navigating. See Bug 1273802
});
