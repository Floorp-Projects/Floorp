/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function () {
  const URL1 = buildURLWithContent(
    "example.com",
    `<h1>example.com</h1>` +
      `<script>
        caches.open("lorem").then(cache => {
          cache.add("${URL_ROOT_COM_SSL}storage-blank.html");
        });
        function clear() {
          caches.delete("lorem");
        }
      </script>`
  );
  const URL2 = buildURLWithContent(
    "example.net",
    `<h1>example.net</h1>` +
      `<script>
        caches.open("foo").then(cache => {
          cache.add("${URL_ROOT_NET_SSL}storage-blank.html");
        });
        function clear() {
          caches.delete("foo");
        }
      </script>`
  );

  // open tab
  await openTabAndSetupStorage(URL1);
  const doc = gPanelWindow.document;

  // Check first domain
  // check that host appears in the storage tree
  checkTree(doc, ["Cache", "https://example.com", "lorem"]);
  // check the table for values
  await selectTreeItem(["Cache", "https://example.com", "lorem"]);
  checkCacheData(URL_ROOT_COM_SSL + "storage-blank.html", "OK");

  // clear up the cache before navigating
  info("Cleaning up cache…");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    const win = content.wrappedJSObject;
    await win.clear();
  });

  // Check second domain
  await navigateTo(URL2);

  info("Waiting until Cache > example.net is available in the UI");
  await waitUntil(() => gUI.tree.exists(["Cache", "https://example.net"]));

  info("Select the Cache view in order to force updating it");
  await selectTreeItem(["Cache", "https://example.net"]);

  // wait for storage tree refresh, and check host
  info("Waiting for storage tree to update…");
  await waitUntil(() => isInTree(doc, ["Cache", "https://example.net", "foo"]));

  ok(
    !isInTree(doc, ["Cache", "https://example.com"]),
    "example.com item is not in the tree anymore"
  );

  // check the table for values
  await selectTreeItem(["Cache", "https://example.net", "foo"]);
  checkCacheData(URL_ROOT_NET_SSL + "storage-blank.html", "OK");

  info("Check that the Cache node still has the expected label");
  is(
    getTreeNodeLabel(doc, ["Cache"]),
    "Cache Storage",
    "Cache item is properly displayed"
  );
});

function checkCacheData(url, status) {
  is(
    gUI.table.items.get(url)?.status,
    status,
    `Table row has an entry for: ${url} with status: ${status}`
  );
}
