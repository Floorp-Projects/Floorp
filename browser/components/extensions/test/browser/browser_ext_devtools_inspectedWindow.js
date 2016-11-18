/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "gDevTools",
                                  "resource://devtools/client/framework/gDevTools.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "devtools",
                                  "resource://devtools/shared/Loader.jsm");

/**
 * this test file ensures that:
 *
 * - the devtools page gets only a subset of the runtime API namespace.
 * - devtools.inspectedWindow.tabId is the same tabId that we can retrieve
 *   in the background page using the tabs API namespace.
 * - devtools API is available in the devtools page sub-frames when a valid
 *   extension URL has been loaded.
 */
add_task(function* test_devtools_inspectedWindow_tabId() {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/");

  async function background() {
    browser.test.assertEq(undefined, browser.devtools,
                          "No devtools APIs should be available in the background page");

    const tabs = await browser.tabs.query({active: true, lastFocusedWindow: true});
    browser.test.sendMessage("current-tab-id", tabs[0].id);
  }

  function devtools_page() {
    browser.test.assertEq(undefined, browser.runtime.getBackgroundPage,
      "The `runtime.getBackgroundPage` API method should be missing in a devtools_page context"
    );

    try {
      let tabId = browser.devtools.inspectedWindow.tabId;
      browser.test.sendMessage("inspectedWindow-tab-id", tabId);
    } catch (err) {
      browser.test.sendMessage("inspectedWindow-tab-id", undefined);
      throw err;
    }
  }

  function devtools_page_iframe() {
    try {
      let tabId = browser.devtools.inspectedWindow.tabId;
      browser.test.sendMessage("devtools_page_iframe.inspectedWindow-tab-id", tabId);
    } catch (err) {
      browser.test.fail(`Error: ${err} :: ${err.stack}`);
      browser.test.sendMessage("devtools_page_iframe.inspectedWindow-tab-id", undefined);
    }
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
         <iframe src="/devtools_page_iframe.html"></iframe>
         <script src="devtools_page.js"></script>
       </body>
      </html>`,
      "devtools_page.js": devtools_page,
      "devtools_page_iframe.html": `<!DOCTYPE html>
      <html>
       <head>
         <meta charset="utf-8">
       </head>
       <body>
         <script src="devtools_page_iframe.js"></script>
       </body>
      </html>`,
      "devtools_page_iframe.js": devtools_page_iframe,
    },
  });

  yield extension.startup();

  let backgroundPageCurrentTabId = yield extension.awaitMessage("current-tab-id");

  let target = devtools.TargetFactory.forTab(tab);

  yield gDevTools.showToolbox(target, "webconsole");
  info("developer toolbox opened");

  let devtoolsInspectedWindowTabId = yield extension.awaitMessage("inspectedWindow-tab-id");

  is(devtoolsInspectedWindowTabId, backgroundPageCurrentTabId,
     "Got the expected tabId from devtool.inspectedWindow.tabId");

  let devtoolsPageIframeTabId = yield extension.awaitMessage("devtools_page_iframe.inspectedWindow-tab-id");

  is(devtoolsPageIframeTabId, backgroundPageCurrentTabId,
     "Got the expected tabId from devtool.inspectedWindow.tabId called in a devtool_page iframe");

  yield gDevTools.closeToolbox(target);

  yield target.destroy();

  yield extension.unload();

  yield BrowserTestUtils.removeTab(tab);
});
