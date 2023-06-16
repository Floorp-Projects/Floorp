/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

requestLongerTimeout(3);

add_task(async function () {
  const URL1 = URL_ROOT_COM_SSL + "storage-indexeddb-simple.html";
  const URL2 = URL_ROOT_NET_SSL + "storage-indexeddb-simple-alt.html";

  // open tab
  await openTabAndSetupStorage(URL1);
  const doc = gPanelWindow.document;

  // Check first domain
  // check that host appears in the storage tree
  checkTree(doc, ["indexedDB", "https://example.com"]);
  // check the table for values
  await selectTreeItem([
    "indexedDB",
    "https://example.com",
    "db (default)",
    "store",
  ]);
  checkStorageData("lorem", JSON.stringify({ key: "lorem", value: "ipsum" }));

  // clear db before navigating to a new domain
  info("Removing database…");
  await clearStorage();

  // Check second domain
  await navigateTo(URL2);
  info("Creating database in the second domain…");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    const win = content.wrappedJSObject;
    await win.setup();
  });
  // wait for storage tree refresh, and check host
  info("Checking storage tree…");
  await waitUntil(() => isInTree(doc, ["indexedDB", "https://example.net"]));

  ok(
    !isInTree(doc, ["indexedDB", "https://example.com"]),
    "example.com item is not in the tree anymore"
  );

  // TODO: select tree and check on storage data.
  // We cannot do it yet since we do not detect newly created indexed db's when
  // navigating. See Bug 1273802

  // reload the current tab, and check again
  await reloadBrowser();
  // wait for storage tree refresh, and check host
  info("Checking storage tree…");
  await waitUntil(() => isInTree(doc, ["indexedDB", "https://example.net"]));

  info("Check that the indexedDB node still has the expected label");
  is(
    getTreeNodeLabel(doc, ["indexedDB"]),
    "Indexed DB",
    "indexedDB item is properly displayed"
  );
});

async function clearStorage() {
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    const win = content.wrappedJSObject;
    await win.clear();
  });
}
