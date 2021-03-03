/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from head.js */

"use strict";

add_task(async function() {
  // open tab
  const URL = URL_ROOT_COM + "storage-cache-basic.html";
  await openTabAndSetupStorage(URL);
  const doc = gPanelWindow.document;

  // check that host appears in the storage tree
  checkTree(doc, ["Cache", "http://example.com", "lorem"]);
  checkTree(doc, ["Cache", "http://example.net", "foo"]);
  // Check top level page
  await selectTreeItem(["Cache", "http://example.com", "lorem"]);
  checkCacheData(URL_ROOT_COM + "storage-blank.html", "OK");
  // Check iframe
  await selectTreeItem(["Cache", "http://example.net", "foo"]);
  checkCacheData(URL_ROOT_NET + "storage-blank.html", "OK");
});

function checkCacheData(url, status) {
  is(
    gUI.table.items.get(url)?.status,
    status,
    `Table row has an entry for: ${url} with status: ${status}`
  );
}
