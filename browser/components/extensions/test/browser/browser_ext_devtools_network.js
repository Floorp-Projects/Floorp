/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
const {gDevTools} = require("devtools/client/framework/devtools");

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
    browser.test.sendMessage("onNavigatedFired", eventCount);

    if (eventCount === 2) {
      eventCount = 0;
      browser.devtools.network.onNavigated.removeListener(listener);
    }
  };
  browser.devtools.network.onNavigated.addListener(listener);

  let harLogCount = 0;
  let harListener = async msg => {
    if (msg !== "getHAR") {
      return;
    }

    harLogCount++;

    const harLog = await browser.devtools.network.getHAR();
    browser.test.sendMessage("getHAR-result", harLog);

    if (harLogCount === 2) {
      harLogCount = 0;
      browser.test.onMessage.removeListener(harListener);
    }
  };
  browser.test.onMessage.addListener(harListener);
}

let extData = {
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
};

/**
 * Test for `chrome.devtools.network.onNavigate()` API
 */
add_task(async function test_devtools_network_on_navigated() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/");
  let extension = ExtensionTestUtils.loadExtension(extData);

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

/**
 * Test for `chrome.devtools.network.getHAR()` API
 */
add_task(async function test_devtools_network_get_har() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/");
  let extension = ExtensionTestUtils.loadExtension(extData);

  await extension.startup();
  await extension.awaitMessage("ready");

  let target = gDevTools.getTargetForTab(tab);

  // Open the Toolbox
  let toolbox = await gDevTools.showToolbox(target, "webconsole");
  info("Developer toolbox opened.");

  // Get HAR, it should be empty since the Net panel wasn't selected.
  const getHAREmptyPromise = extension.awaitMessage("getHAR-result");
  extension.sendMessage("getHAR");
  const getHAREmptyResult = await getHAREmptyPromise;
  is(getHAREmptyResult.log.entries.length, 0, "HAR log should be empty");

  // Select the Net panel.
  await toolbox.selectTool("netmonitor");

  // Reload the page to collect some HTTP requests.
  extension.sendMessage("navigate");
  await extension.awaitMessage("tabUpdated");
  await extension.awaitMessage("onNavigatedFired");

  // Get HAR, it should not be empty now.
  const getHARPromise = extension.awaitMessage("getHAR-result");
  extension.sendMessage("getHAR");
  const getHARResult = await getHARPromise;
  is(getHARResult.log.entries.length, 1, "HAR log should not be empty");

  // Shutdown
  await gDevTools.closeToolbox(target);

  await target.destroy();

  await extension.unload();

  await BrowserTestUtils.removeTab(tab);
});
