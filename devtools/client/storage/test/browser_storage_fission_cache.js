/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Bug 1746646: Make mochitests work with TCP enabled (cookieBehavior = 5)
// All instances of addPermission and removePermission set up 3rd-party storage
// access in a way that allows the test to proceed with TCP enabled.

add_task(async function () {
  // open tab
  const URL = URL_ROOT_COM_SSL + "storage-cache-basic.html";
  await SpecialPowers.addPermission(
    "3rdPartyStorage^https://example.net",
    true,
    URL
  );
  await openTabAndSetupStorage(URL);
  const doc = gPanelWindow.document;

  // check that host appears in the storage tree
  checkTree(doc, ["Cache", "https://example.com", "lorem"]);
  checkTree(doc, ["Cache", "https://example.net", "foo"]);
  // Check top level page
  await selectTreeItem(["Cache", "https://example.com", "lorem"]);
  checkCacheData(URL_ROOT_COM_SSL + "storage-blank.html", "OK");
  // Check iframe
  await selectTreeItem(["Cache", "https://example.net", "foo"]);
  checkCacheData(URL_ROOT_NET_SSL + "storage-blank.html", "OK");

  await SpecialPowers.removePermission(
    "3rdPartyStorage^http://example.net",
    URL
  );
});

function checkCacheData(url, status) {
  is(
    gUI.table.items.get(url)?.status,
    status,
    `Table row has an entry for: ${url} with status: ${status}`
  );
}
