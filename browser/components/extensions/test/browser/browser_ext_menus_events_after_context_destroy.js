/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This test does verify that the menus API events are still emitted when
// there are extension context alive with subscribed listeners
// (See Bug 1602384).
add_task(async function test_subscribed_events_fired_after_context_destroy() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["menus"],
    },
    files: {
      "page.html": `<!DOCTYPE html>
        <meta charset="uft-8"><script src="page.js"></script>
        Extension Page
      `,
      "page.js": async function() {
        browser.menus.onShown.addListener(() => {
          browser.test.sendMessage("menu-onShown");
        });
        browser.menus.onHidden.addListener(() => {
          browser.test.sendMessage("menu-onHidden");
        });
        // Call an API method implemented in the parent process
        // to ensure the menu listeners are subscribed in the
        // parent process.
        await browser.runtime.getBrowserInfo();
        browser.test.sendMessage("page-loaded");
      },
    },
  });

  await extension.startup();
  const pageURL = `moz-extension://${extension.uuid}/page.html`;

  info("Loading extension page in a tab");
  const tab1 = BrowserTestUtils.addTab(gBrowser, pageURL);
  await extension.awaitMessage("page-loaded");

  info("Loading extension page in another tab");
  const tab2 = BrowserTestUtils.addTab(gBrowser, pageURL);
  await extension.awaitMessage("page-loaded");

  info("Select the first tab");
  gBrowser.selectedTab = tab1;

  info("Remove the second tab");
  BrowserTestUtils.removeTab(tab2);

  info("Open a context menu and expect menu.onShown to be fired");
  await window.promiseDocumentFlushed(() => {});
  await openContextMenu("body");
  await extension.awaitMessage("menu-onShown");

  info("Close context menu and expect menu.onHidden to be fired");
  await closeExtensionContextMenu();
  await extension.awaitMessage("menu-onHidden");

  ok(true, "The expected menu events have been fired");

  BrowserTestUtils.removeTab(tab1);

  await extension.unload();
});
