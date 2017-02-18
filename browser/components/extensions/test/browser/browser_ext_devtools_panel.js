/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "devtools",
                                  "resource://devtools/shared/Loader.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "gDevTools",
                                  "resource://devtools/client/framework/gDevTools.jsm");

/**
 * This test file ensures that:
 *
 * - ensures that devtools.panel.create is able to create a devtools panel
 */

add_task(function* test_devtools_page_panels_create() {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/");

  async function devtools_page() {
    const result = {
      devtoolsPageTabId: browser.devtools.inspectedWindow.tabId,
      panelCreated: 0,
      panelShown: 0,
      panelHidden: 0,
    };

    try {
      const panel = await browser.devtools.panels.create(
        "Test Panel", "fake-icon.png", "devtools_panel.html"
      );

      result.panelCreated++;

      panel.onShown.addListener(contentWindow => {
        result.panelShown++;
        browser.test.assertEq("complete", contentWindow.document.readyState,
                              "Got the expected 'complete' panel document readyState");
        browser.test.assertEq("test_panel_global", contentWindow.TEST_PANEL_GLOBAL,
                              "Got the expected global in the panel contentWindow");
        browser.test.sendMessage("devtools_panel_shown", result);
      });

      panel.onHidden.addListener(() => {
        result.panelHidden++;

        browser.test.sendMessage("devtools_panel_hidden", result);
      });

      browser.test.sendMessage("devtools_panel_created");
    } catch (err) {
      // Make the test able to fail fast when it is going to be a failure.
      browser.test.sendMessage("devtools_panel_created");
      throw err;
    }
  }

  function devtools_panel() {
    // Set a property in the global and check that it is defined
    // and accessible from the devtools_page when the panel.onShown
    // event has been received.
    window.TEST_PANEL_GLOBAL = "test_panel_global";
    browser.test.sendMessage("devtools_panel_inspectedWindow_tabId",
                             browser.devtools.inspectedWindow.tabId);
  }

  let extension = ExtensionTestUtils.loadExtension({
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
      "devtools_panel.html":  `<!DOCTYPE html>
      <html>
       <head>
         <meta charset="utf-8">
       </head>
       <body>
         DEVTOOLS PANEL
         <script src="devtools_panel.js"></script>
       </body>
      </html>`,
      "devtools_panel.js": devtools_panel,
    },
  });

  yield extension.startup();

  let target = devtools.TargetFactory.forTab(tab);

  const toolbox = yield gDevTools.showToolbox(target, "webconsole");
  info("developer toolbox opened");

  yield extension.awaitMessage("devtools_panel_created");

  const toolboxAdditionalTools = toolbox.getAdditionalTools();

  is(toolboxAdditionalTools.length, 1,
     "Got the expected number of toolbox specific panel registered.");

  const panelId = toolboxAdditionalTools[0].id;

  yield gDevTools.showToolbox(target, panelId);
  const {devtoolsPageTabId} = yield extension.awaitMessage("devtools_panel_shown");
  const devtoolsPanelTabId = yield extension.awaitMessage("devtools_panel_inspectedWindow_tabId");
  is(devtoolsPanelTabId, devtoolsPageTabId,
     "Got the same devtools.inspectedWindow.tabId from devtools page and panel");
  info("Addon Devtools Panel shown");

  yield gDevTools.showToolbox(target, "webconsole");
  const results = yield extension.awaitMessage("devtools_panel_hidden");
  info("Addon Devtools Panel hidden");

  is(results.panelCreated, 1, "devtools.panel.create callback has been called once");
  is(results.panelShown, 1, "panel.onShown listener has been called once");
  is(results.panelHidden, 1, "panel.onHidden listener has been called once");

  yield gDevTools.showToolbox(target, panelId);
  yield extension.awaitMessage("devtools_panel_shown");
  info("Addon Devtools Panel shown - second cycle");

  yield gDevTools.showToolbox(target, "webconsole");
  const secondCycleResults = yield extension.awaitMessage("devtools_panel_hidden");
  info("Addon Devtools Panel hidden - second cycle");

  is(secondCycleResults.panelCreated, 1, "devtools.panel.create callback has been called once");
  is(secondCycleResults.panelShown, 2, "panel.onShown listener has been called twice");
  is(secondCycleResults.panelHidden, 2, "panel.onHidden listener has been called twice");

  yield gDevTools.closeToolbox(target);

  yield target.destroy();

  yield extension.unload();

  yield BrowserTestUtils.removeTab(tab);
});
