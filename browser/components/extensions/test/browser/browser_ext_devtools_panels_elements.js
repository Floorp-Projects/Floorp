/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.defineModuleGetter(this, "gDevTools",
                               "resource://devtools/client/framework/gDevTools.jsm");
ChromeUtils.defineModuleGetter(this, "devtools",
                               "resource://devtools/shared/Loader.jsm");

add_task(async function test_devtools_panels_elements_onSelectionChanged() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/");

  function devtools_page() {
    let isReloading = false;
    let collectedEvalResults = [];

    browser.devtools.panels.elements.onSelectionChanged.addListener(async () => {
      const [
        evalResult, exceptionInfo,
      ] = await browser.devtools.inspectedWindow.eval("$0 && $0.tagName");

      if (exceptionInfo) {
        browser.test.fail("Unexpected exceptionInfo on inspectedWindow.eval: " +
                          JSON.stringify(exceptionInfo));
      }

      collectedEvalResults.push(evalResult);

      // The eval results that are happening during the reload are going to
      // be retrieved all at once using the "collected_devttols_eval_results:request".
      if (!isReloading) {
        browser.test.sendMessage("devtools_eval_result", evalResult);
      }
    });

    browser.test.onMessage.addListener(msg => {
      switch (msg) {
        case "inspectedWindow_reload": {
          // Force a reload to test that the expected onSelectionChanged events are sent
          // while the page is navigating and once it has been fully reloaded.
          isReloading = true;
          collectedEvalResults = [];
          browser.devtools.inspectedWindow.eval("window.location.reload();");
          break;
        }

        case "collected_devtools_eval_results:request": {
          browser.test.sendMessage("collected_devtools_eval_results:reply",
                                   collectedEvalResults);
          break;
        }

        default: {
          browser.test.fail(`Received unexpected test.onMesssage: ${msg}`);
        }
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

  let target = await devtools.TargetFactory.forTab(tab);

  const toolbox = await gDevTools.showToolbox(target, "webconsole");
  info("developer toolbox opened");

  await extension.awaitMessage("devtools_page_loaded");

  await toolbox.selectTool("inspector");

  const inspector = toolbox.getPanel("inspector");

  const evalResult = await extension.awaitMessage("devtools_eval_result");

  is(evalResult, "BODY", "Got the expected onSelectionChanged once the inspector is selected");

  // Reload the inspected tab and wait for the inspector markup view to have been
  // fully reloaded.
  const onceMarkupReloaded = inspector.once("markuploaded");
  extension.sendMessage("inspectedWindow_reload");
  await onceMarkupReloaded;

  // Retrieve the first and last collected eval result (the first is related to the
  // page navigating away, the last one is related to the updated inspector markup view
  // fully reloaded and the selection updated).
  extension.sendMessage("collected_devtools_eval_results:request");
  const collectedEvalResults = await extension.awaitMessage("collected_devtools_eval_results:reply");
  const evalResultNavigating = collectedEvalResults.shift();
  const evalResultOnceMarkupReloaded = collectedEvalResults.pop();

  is(evalResultNavigating, undefined,
     "Got the expected onSelectionChanged once the tab is navigating");

  is(evalResultOnceMarkupReloaded, "BODY",
     "Got the expected onSelectionChanged once the tab has been completely reloaded");

  await gDevTools.closeToolbox(target);

  await target.destroy();

  await extension.unload();

  BrowserTestUtils.removeTab(tab);
});
