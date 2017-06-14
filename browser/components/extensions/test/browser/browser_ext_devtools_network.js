/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const {DevToolsShim} = Cu.import("chrome://devtools-shim/content/DevToolsShim.jsm", {});
const {gDevTools} = DevToolsShim;

add_task(async function test_devtools_network_on_navigated() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/");

  function background() {
    browser.test.onMessage.addListener(msg => {
      let code;
      if (msg === "navigate") {
        code = "window.wrappedJSObject.location.href = 'http://example.com/';";
        browser.tabs.executeScript({code});
      } else if (msg === "reload") {
        code = "window.wrappedJSObject.location.reload(true);";
        browser.tabs.executeScript({code});
      }
    });
    browser.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {
      if (changeInfo.status === "complete" && tab.url === "http://example.com/") {
        browser.test.sendMessage("tabUpdated");
      }
    });
    browser.test.sendMessage("ready");
  }

  function devtools_page() {
    let eventCount = 0;
    let listener = url => {
      eventCount++;
      browser.test.assertEq("http://example.com/", url, "onNavigated received the expected url.");
      if (eventCount === 2) {
        browser.devtools.network.onNavigated.removeListener(listener);
      }
      browser.test.sendMessage("onNavigatedFired", eventCount);
    };
    browser.devtools.network.onNavigated.addListener(listener);
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["tabs", "http://mochi.test/", "http://example.com/"],
      devtools_page: "devtools_page.html",
    },
    files: {
      "devtools_page.html": `<!DOCTYPE html>
        <html>
          <head>
            <meta charset="utf-8">
            <script src="devtools_page.js"></script>
          </head>
          <body>
          </body>
        </html>`,
      "devtools_page.js": devtools_page,
    },
  });

  await extension.startup();
  await extension.awaitMessage("ready");

  let target = gDevTools.getTargetForTab(tab);

  await gDevTools.showToolbox(target, "webconsole");
  info("Developer toolbox opened.");

  extension.sendMessage("navigate");
  await extension.awaitMessage("tabUpdated");
  let eventCount = await extension.awaitMessage("onNavigatedFired");
  is(eventCount, 1, "The expected number of events were fired.");

  extension.sendMessage("reload");
  await extension.awaitMessage("tabUpdated");
  eventCount = await extension.awaitMessage("onNavigatedFired");
  is(eventCount, 2, "The expected number of events were fired.");

  // do a reload after the listener has been removed, do not expect a message to be sent
  extension.sendMessage("reload");
  await extension.awaitMessage("tabUpdated");

  await gDevTools.closeToolbox(target);

  await target.destroy();

  await extension.unload();

  await BrowserTestUtils.removeTab(tab);
});
