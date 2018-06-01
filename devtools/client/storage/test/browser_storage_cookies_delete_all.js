/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../shared/test/shared-head.js */

"use strict";

// Test deleting all cookies

async function performDelete(store, rowName, action) {
  const contextMenu = gPanelWindow.document.getElementById(
    "storage-table-popup");
  const menuDeleteAllItem = contextMenu.querySelector(
    "#storage-table-popup-delete-all");
  const menuDeleteAllSessionCookiesItem = contextMenu.querySelector(
    "#storage-table-popup-delete-all-session-cookies");
  const menuDeleteAllFromItem = contextMenu.querySelector(
    "#storage-table-popup-delete-all-from");

  const storeName = store.join(" > ");

  await selectTreeItem(store);

  const eventWait = gUI.once("store-objects-edit");
  const cells = getRowCells(rowName, true);

  await waitForContextMenu(contextMenu, cells.name, () => {
    info(`Opened context menu in ${storeName}, row '${rowName}'`);
    switch (action) {
      case "deleteAll":
        menuDeleteAllItem.click();
        break;
      case "deleteAllSessionCookies":
        menuDeleteAllSessionCookiesItem.click();
        break;
      case "deleteAllFrom":
        menuDeleteAllFromItem.click();
        const hostName = cells.host.value;
        ok(menuDeleteAllFromItem.getAttribute("label").includes(hostName),
        `Context menu item label contains '${hostName}'`);
        break;
    }
  });

  await eventWait;
}

add_task(async function() {
  await openTabAndSetupStorage(MAIN_DOMAIN + "storage-listings.html");

  info("test state before delete");
  await checkState([
    [
      ["cookies", "http://test1.example.org"], [
        getCookieId("c1", "test1.example.org", "/browser"),
        getCookieId("c3", "test1.example.org", "/"),
        getCookieId("cs2", ".example.org", "/"),
        getCookieId("c4", ".example.org", "/"),
        getCookieId("uc1", ".example.org", "/"),
        getCookieId("uc2", ".example.org", "/")
      ]
    ],
    [
      ["cookies", "https://sectest1.example.org"], [
        getCookieId("cs2", ".example.org", "/"),
        getCookieId("c4", ".example.org", "/"),
        getCookieId("sc1", "sectest1.example.org",
                    "/browser/devtools/client/storage/test/"),
        getCookieId("sc2", "sectest1.example.org",
                    "/browser/devtools/client/storage/test/"),
        getCookieId("uc1", ".example.org", "/"),
        getCookieId("uc2", ".example.org", "/")
      ]
    ],
  ]);

  info("delete all from domain");
  // delete only cookies that match the host exactly
  let id = getCookieId("c1", "test1.example.org", "/browser");
  await performDelete(["cookies", "http://test1.example.org"], id, "deleteAllFrom");

  info("test state after delete all from domain");
  await checkState([
    // Domain cookies (.example.org) must not be deleted.
    [
      ["cookies", "http://test1.example.org"],
      [
        getCookieId("cs2", ".example.org", "/"),
        getCookieId("c4", ".example.org", "/"),
        getCookieId("uc1", ".example.org", "/"),
        getCookieId("uc2", ".example.org", "/")
      ]
    ],
    [
      ["cookies", "https://sectest1.example.org"],
      [
        getCookieId("cs2", ".example.org", "/"),
        getCookieId("c4", ".example.org", "/"),
        getCookieId("uc1", ".example.org", "/"),
        getCookieId("uc2", ".example.org", "/"),
        getCookieId("sc1", "sectest1.example.org",
                    "/browser/devtools/client/storage/test/"),
        getCookieId("sc2", "sectest1.example.org",
                    "/browser/devtools/client/storage/test/")
      ]
    ],
  ]);

  info("delete all session cookies");
  // delete only session cookies
  id = getCookieId("cs2", ".example.org", "/");
  await performDelete(["cookies", "http://sectest1.example.org"], id,
    "deleteAllSessionCookies");

  info("test state after delete all session cookies");
  await checkState([
    // Cookies with expiry date must not be deleted.
    [
      ["cookies", "http://test1.example.org"],
      [
        getCookieId("c4", ".example.org", "/"),
        getCookieId("uc2", ".example.org", "/")
      ]
    ],
    [
      ["cookies", "https://sectest1.example.org"],
      [
        getCookieId("c4", ".example.org", "/"),
        getCookieId("uc2", ".example.org", "/"),
        getCookieId("sc2", "sectest1.example.org",
        "/browser/devtools/client/storage/test/")
      ]
    ],
  ]);

  info("delete all");
  // delete all cookies for host, including domain cookies
  id = getCookieId("uc2", ".example.org", "/");
  await performDelete(["cookies", "http://sectest1.example.org"], id, "deleteAll");

  info("test state after delete all");
  await checkState([
    // Domain cookies (.example.org) are deleted too, so deleting in sectest1
    // also removes stuff from test1.
    [["cookies", "http://test1.example.org"], []],
    [["cookies", "https://sectest1.example.org"], []],
  ]);

  await finishTests();
});
