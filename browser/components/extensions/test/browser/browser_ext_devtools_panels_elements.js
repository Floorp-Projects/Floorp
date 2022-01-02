/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

loadTestSubscript("head_devtools.js");

add_task(async function test_devtools_panels_elements_onSelectionChanged() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://mochi.test:8888/"
  );

  function devtools_page() {
    browser.devtools.panels.elements.onSelectionChanged.addListener(
      async () => {
        const [
          evalResult,
          exceptionInfo,
        ] = await browser.devtools.inspectedWindow.eval("$0 && $0.tagName");

        if (exceptionInfo) {
          browser.test.fail(
            "Unexpected exceptionInfo on inspectedWindow.eval: " +
              JSON.stringify(exceptionInfo)
          );
        }

        browser.test.sendMessage("devtools_eval_result", evalResult);
      }
    );

    browser.test.onMessage.addListener(msg => {
      switch (msg) {
        case "inspectedWindow_reload": {
          // Force a reload to test that the expected onSelectionChanged events are sent
          // while the page is navigating and once it has been fully reloaded.
          browser.devtools.inspectedWindow.eval("window.location.reload();");
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

  const toolbox = await openToolboxForTab(tab);

  await extension.awaitMessage("devtools_page_loaded");

  await toolbox.selectTool("inspector");

  const inspector = toolbox.getPanel("inspector");

  info(
    "Waiting for the first onSelectionChanged event to be fired once the inspector is open"
  );

  const evalResult = await extension.awaitMessage("devtools_eval_result");
  is(
    evalResult,
    "BODY",
    "Got the expected onSelectionChanged once the inspector is selected"
  );

  // Reload the inspected tab and wait for the inspector markup view to have been
  // fully reloaded.
  const onceMarkupReloaded = inspector.once("markuploaded");
  extension.sendMessage("inspectedWindow_reload");
  await onceMarkupReloaded;

  info(
    "Waiting for the two onSelectionChanged events fired before and after the navigation"
  );

  // Expect the eval result to be undefined on the first onSelectionChanged event
  // (fired when the page is navigating away, and so the current selection is undefined).
  const evalResultNavigating = await extension.awaitMessage(
    "devtools_eval_result"
  );
  is(
    evalResultNavigating,
    undefined,
    "Got the expected onSelectionChanged once the tab is navigating"
  );

  // Expect the eval result to be related to the body element on the second onSelectionChanged
  // event (fired when the page have been navigated to the new page).
  const evalResultOnceMarkupReloaded = await extension.awaitMessage(
    "devtools_eval_result"
  );
  is(
    evalResultOnceMarkupReloaded,
    "BODY",
    "Got the expected onSelectionChanged once the tab has been completely reloaded"
  );

  await closeToolboxForTab(tab);

  await extension.unload();

  BrowserTestUtils.removeTab(tab);
});
