/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

add_task(async function testIndexedDB() {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.userContext.enabled", true]],
  });

  function background() {
    browser.test.onMessage.addListener(async msg => {
      await browser.browsingData.remove(msg, { indexedDB: true });
      browser.test.sendMessage("indexedDBRemoved");
    });
  }

  function contentScript() {
    // eslint-disable-next-line mozilla/balanced-listeners
    window.addEventListener(
      "message",
      msg => {
        browser.test.sendMessage("indexedDBCreated");
      },
      true
    );
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["browsingData", "tabs"],
      content_scripts: [
        {
          matches: [
            "http://mochi.test/*/file_indexedDB.html",
            "http://example.com/*/file_indexedDB.html",
            "http://example.net/*/file_indexedDB.html",
          ],
          js: ["script.js"],
          run_at: "document_start",
        },
      ],
    },
    files: {
      "script.js": contentScript,
    },
  });

  let win = await BrowserTestUtils.openNewBrowserWindow();
  await focusWindow(win);

  await extension.startup();

  const PAGE =
    "/browser/browser/components/extensions/test/browser/file_indexedDB.html";

  // Create two tabs.
  BrowserTestUtils.addTab(win.gBrowser, `http://mochi.test:8888${PAGE}`);
  BrowserTestUtils.addTab(win.gBrowser, `http://example.com${PAGE}`);
  // Create tab with cookieStoreId "firefox-container-1"
  BrowserTestUtils.addTab(win.gBrowser, `http://example.net${PAGE}`, {
    userContextId: 1,
  });

  await extension.awaitMessage("indexedDBCreated");
  await extension.awaitMessage("indexedDBCreated");
  await extension.awaitMessage("indexedDBCreated");

  function getOrigins() {
    return new Promise(resolve => {
      let origins = [];
      Services.qms.getUsage(request => {
        for (let i = 0; i < request.result.length; ++i) {
          if (request.result[i].usage === 0) {
            continue;
          }
          if (
            request.result[i].origin.startsWith("http://mochi.test") ||
            request.result[i].origin.startsWith("http://example.com") ||
            request.result[i].origin.startsWith("http://example.net")
          ) {
            origins.push(request.result[i].origin);
          }
        }
        resolve(origins.sort());
      });
    });
  }

  let origins = await getOrigins();
  is(origins.length, 3, "IndexedDB databases have been populated.");

  // Deleting private browsing mode data is silently ignored.
  extension.sendMessage({ cookieStoreId: "firefox-private" });
  await extension.awaitMessage("indexedDBRemoved");

  origins = await getOrigins();
  is(origins.length, 3, "All indexedDB remains after clearing firefox-private");

  // Delete by hostname
  extension.sendMessage({ hostnames: ["example.com"] });
  await extension.awaitMessage("indexedDBRemoved");

  origins = await getOrigins();
  is(origins.length, 2, "IndexedDB data only for only two domains left");
  ok(origins[0].startsWith("http://example.net"), "example.net not deleted");
  ok(origins[1].startsWith("http://mochi.test"), "mochi.test not deleted");

  // Delete by cookieStoreId
  extension.sendMessage({ cookieStoreId: "firefox-container-1" });
  await extension.awaitMessage("indexedDBRemoved");

  origins = await getOrigins();
  is(origins.length, 1, "IndexedDB data only for only one domain");
  ok(origins[0].startsWith("http://mochi.test"), "mochi.test not deleted");

  // Delete all
  extension.sendMessage({});
  await extension.awaitMessage("indexedDBRemoved");

  origins = await getOrigins();
  is(origins.length, 0, "All IndexedDB data has been removed.");

  await extension.unload();
  await BrowserTestUtils.closeWindow(win);
});
