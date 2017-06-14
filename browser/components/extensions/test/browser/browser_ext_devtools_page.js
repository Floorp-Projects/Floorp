/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const {DevToolsShim} = Cu.import("chrome://devtools-shim/content/DevToolsShim.jsm", {});
const {gDevTools} = DevToolsShim;

/**
 * This test file ensures that:
 *
 * - the devtools_page property creates a new WebExtensions context
 * - the devtools_page can exchange messages with the background page
 */

add_task(async function test_devtools_page_runtime_api_messaging() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/");

  function background() {
    browser.runtime.onConnect.addListener((port) => {
      let portMessageReceived = false;

      port.onDisconnect.addListener(() => {
        browser.test.assertTrue(portMessageReceived,
                                "Got a port message before the port disconnect event");
        browser.test.notifyPass("devtools_page_connect.done");
      });

      port.onMessage.addListener((msg) => {
        portMessageReceived = true;
        browser.test.assertEq("devtools -> background port message", msg,
                              "Got the expected message from the devtools page");
        port.postMessage("background -> devtools port message");
      });
    });
  }

  function devtools_page() {
    const port = browser.runtime.connect();
    port.onMessage.addListener((msg) => {
      browser.test.assertEq("background -> devtools port message", msg,
                            "Got the expected message from the background page");
      port.disconnect();
    });
    port.postMessage("devtools -> background port message");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      devtools_page: "devtools_page.html",
    },
    files: {
      "devtools_page.html": `<!DOCTYPE html>
      <html>
       <head>
         <meta charset="utf-8">
       </head>
       <body>
         <script src="devtools_page.js"></script>
       </body>
      </html>`,
      "devtools_page.js": devtools_page,
    },
  });

  await extension.startup();

  let target = gDevTools.getTargetForTab(tab);

  await gDevTools.showToolbox(target, "webconsole");
  info("developer toolbox opened");

  await extension.awaitFinish("devtools_page_connect.done");

  await gDevTools.closeToolbox(target);

  await target.destroy();

  await extension.unload();

  await BrowserTestUtils.removeTab(tab);
});
