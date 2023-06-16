/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test handling errors in CacheStorage

add_task(async function () {
  // Open the URL in a private browsing window.
  const win = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  const tab = win.gBrowser.selectedBrowser;
  const triggeringPrincipal =
    Services.scriptSecurityManager.getSystemPrincipal();
  tab.loadURI(
    Services.io.newURI(ALT_DOMAIN_SECURED + "storage-cache-error.html"),
    { triggeringPrincipal }
  );
  await BrowserTestUtils.browserLoaded(tab);

  // On enumerating cache storages, CacheStorage::Keys would throw a
  // DOM security exception. We'd like to verify storage panel still work in
  // this case.
  await openStoragePanel({ tab: win.gBrowser.selectedTab });

  const cacheItemId = ["Cache", "https://test2.example.org"];

  await selectTreeItem(cacheItemId);
  ok(
    gUI.tree.isSelected(cacheItemId),
    `The item ${cacheItemId.join(" > ")} is present in the tree`
  );

  await BrowserTestUtils.closeWindow(win);
});
