/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "gDevTools",
                                  "resource://devtools/client/framework/gDevTools.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "devtools",
                                  "resource://devtools/shared/Loader.jsm");

add_task(async function test_devtools_panels_elements_onSelectionChanged() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/");

  function devtools_page() {
    let doTabReload = true;

    browser.devtools.panels.elements.onSelectionChanged.addListener(async () => {
      const [
        evalResult, exceptionInfo,
      ] = await browser.devtools.inspectedWindow.eval("$0 && $0.tagName");

      if (exceptionInfo) {
        browser.test.fail("Unexpected exceptionInfo on inspectedWindow.eval: " +
                          JSON.stringify(exceptionInfo));
      }

      browser.test.sendMessage("devtools_eval_result", evalResult);

      if (doTabReload) {
        // Force a reload to test that the expected onSelectionChanged events are sent
        // while the page is navigating and once it has been fully reloaded.
        doTabReload = false;
        await browser.devtools.inspectedWindow.eval("window.location.reload();");
      }
    });

    browser.test.sendMessage("devtools_page_loaded");
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
    },
  });

  await extension.startup();

  let target = devtools.TargetFactory.forTab(tab);

  const toolbox = await gDevTools.showToolbox(target, "webconsole");
  info("developer toolbox opened");

  await extension.awaitMessage("devtools_page_loaded");

  toolbox.selectTool("inspector");

  const evalResult = await extension.awaitMessage("devtools_eval_result");

  is(evalResult, "BODY", "Got the expected onSelectionChanged once the inspector is selected");

  const evalResultNavigating = await extension.awaitMessage("devtools_eval_result");

  is(evalResultNavigating, undefined, "Got the expected onSelectionChanged once the tab is navigating");

  const evalResultNavigated = await extension.awaitMessage("devtools_eval_result");

  is(evalResultNavigated, undefined, "Got the expected onSelectionChanged once the tab navigated");

  const evalResultReloaded = await extension.awaitMessage("devtools_eval_result");

  is(evalResultReloaded, "BODY",
     "Got the expected onSelectionChanged once the tab has been completely reloaded");

  await gDevTools.closeToolbox(target);

  await target.destroy();

  await extension.unload();

  await BrowserTestUtils.removeTab(tab);
});
