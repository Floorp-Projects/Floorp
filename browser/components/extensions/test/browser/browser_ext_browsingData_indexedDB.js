/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

add_task(async function testIndexedDB() {
  function background() {
    const PAGE =
      "/browser/browser/components/extensions/test/browser/file_indexedDB.html";

    browser.test.onMessage.addListener(async msg => {
      await browser.browsingData.remove(
        { hostnames: msg.hostnames },
        { indexedDB: true }
      );
      browser.test.sendMessage("indexedDBRemoved");
    });

    // Create two tabs.
    browser.tabs.create({ url: `http://mochi.test:8888${PAGE}` });
    browser.tabs.create({ url: `http://example.com${PAGE}` });
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
            request.result[i].origin.startsWith("http://example.com")
          ) {
            origins.push(request.result[i].origin);
          }
        }
        resolve(origins);
      });
    });
  }

  let origins = await getOrigins();
  is(origins.length, 2, "IndexedDB databases have been populated.");

  extension.sendMessage({ hostnames: ["example.com"] });
  await extension.awaitMessage("indexedDBRemoved");

  origins = await getOrigins();
  is(origins.length, 1, "IndexedDB data only for only one domain left");
  ok(
    origins[0].startsWith("http://mochi.test"),
    "IndexedDB data for 'example.com' has been removed."
  );

  extension.sendMessage({});
  await extension.awaitMessage("indexedDBRemoved");

  origins = await getOrigins();
  is(origins.length, 0, "All IndexedDB data has been removed.");

  await extension.unload();
  await BrowserTestUtils.closeWindow(win);
});
