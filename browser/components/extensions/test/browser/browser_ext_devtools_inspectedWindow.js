/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

loadTestSubscript("head_devtools.js");

/**
 * Helper that returns the id of the last additional/extension tool for a provided
 * toolbox.
 *
 * @param {Object} toolbox
 *        The DevTools toolbox object.
 * @param {string} label
 *        The expected label for the additional tool.
 * @returns {string} the id of the last additional panel.
 */
function getAdditionalPanelId(toolbox, label) {
  // Copy the tools array and pop the last element from it.
  const panelDef = toolbox
    .getAdditionalTools()
    .slice()
    .pop();
  is(panelDef.label, label, "Additional panel label is the expected label");
  return panelDef.id;
}

/**
 * Helper that returns the number of existing target actors for the content browserId
 * @param {Tab} tab
 * @returns {Integer} the number of targets
 */
function getTargetActorsCount(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    const { TargetActorRegistry } = ChromeUtils.import(
      "resource://devtools/server/actors/targets/target-actor-registry.jsm"
    );

    // Retrieve the target actor instances
    const targets = TargetActorRegistry.getTargetActors(
      content.browsingContext.browserId
    );
    return targets.length;
  });
}

/**
 * this test file ensures that:
 *
 * - the devtools page gets only a subset of the runtime API namespace.
 * - devtools.inspectedWindow.tabId is the same tabId that we can retrieve
 *   in the background page using the tabs API namespace.
 * - devtools API is available in the devtools page sub-frames when a valid
 *   extension URL has been loaded.
 * - devtools.inspectedWindow.eval:
 *   - returns a serialized version of the evaluation result.
 *   - returns the expected error object when the return value serialization raises a
 *     "TypeError: cyclic object value" exception.
 *   - returns the expected exception when an exception has been raised from the evaluated
 *     javascript code.
 */
add_task(async function test_devtools_inspectedWindow_tabId() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://mochi.test:8888/"
  );

  async function background() {
    browser.test.assertEq(
      undefined,
      browser.devtools,
      "No devtools APIs should be available in the background page"
    );

    const tabs = await browser.tabs.query({
      active: true,
      lastFocusedWindow: true,
    });
    browser.test.sendMessage("current-tab-id", tabs[0].id);
  }

  function devtools_page() {
    browser.test.assertEq(
      undefined,
      browser.runtime.getBackgroundPage,
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
      browser.test.sendMessage(
        "devtools_page_iframe.inspectedWindow-tab-id",
        tabId
      );
    } catch (err) {
      browser.test.fail(`Error: ${err} :: ${err.stack}`);
      browser.test.sendMessage(
        "devtools_page_iframe.inspectedWindow-tab-id",
        undefined
      );
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

  await extension.startup();

  let backgroundPageCurrentTabId = await extension.awaitMessage(
    "current-tab-id"
  );

  await openToolboxForTab(tab);

  let devtoolsInspectedWindowTabId = await extension.awaitMessage(
    "inspectedWindow-tab-id"
  );

  is(
    devtoolsInspectedWindowTabId,
    backgroundPageCurrentTabId,
    "Got the expected tabId from devtool.inspectedWindow.tabId"
  );

  let devtoolsPageIframeTabId = await extension.awaitMessage(
    "devtools_page_iframe.inspectedWindow-tab-id"
  );

  is(
    devtoolsPageIframeTabId,
    backgroundPageCurrentTabId,
    "Got the expected tabId from devtool.inspectedWindow.tabId called in a devtool_page iframe"
  );

  await closeToolboxForTab(tab);

  await extension.unload();

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_devtools_inspectedWindow_eval() {
  const TEST_TARGET_URL = "http://mochi.test:8888/";
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_TARGET_URL
  );

  function devtools_page() {
    browser.test.onMessage.addListener(async (msg, ...args) => {
      if (msg !== "inspectedWindow-eval-request") {
        browser.test.fail(`Unexpected test message received: ${msg}`);
        return;
      }

      try {
        const [
          evalResult,
          errorResult,
        ] = await browser.devtools.inspectedWindow.eval(...args);
        browser.test.sendMessage("inspectedWindow-eval-result", {
          evalResult,
          errorResult,
        });
      } catch (err) {
        browser.test.sendMessage("inspectedWindow-eval-result");
        browser.test.fail(`Error: ${err} :: ${err.stack}`);
      }
    });
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
         <script text="text/javascript" src="devtools_page.js"></script>
       </head>
       <body>
       </body>
      </html>`,
      "devtools_page.js": devtools_page,
    },
  });

  await extension.startup();

  await openToolboxForTab(tab);

  const evalTestCases = [
    // Successful evaluation results.
    {
      args: ["window.location.href"],
      expectedResults: { evalResult: TEST_TARGET_URL, errorResult: undefined },
    },

    // Error evaluation results.
    {
      args: ["window"],
      expectedResults: {
        evalResult: undefined,
        errorResult: {
          isError: true,
          code: "E_PROTOCOLERROR",
          description: "Inspector protocol error: %s",
          details: ["TypeError: cyclic object value"],
        },
      },
    },

    // Exception evaluation results.
    {
      args: ["throw new Error('fake eval exception');"],
      expectedResults: {
        evalResult: undefined,
        errorResult: {
          isException: true,
          value: /Error: fake eval exception\n.*moz-extension:\/\//,
        },
      },
    },
  ];

  for (let testCase of evalTestCases) {
    info(`test inspectedWindow.eval with ${JSON.stringify(testCase)}`);

    const { args, expectedResults } = testCase;

    extension.sendMessage(`inspectedWindow-eval-request`, ...args);

    const { evalResult, errorResult } = await extension.awaitMessage(
      `inspectedWindow-eval-result`
    );

    Assert.deepEqual(
      evalResult,
      expectedResults.evalResult,
      "Got the expected eval result"
    );

    if (errorResult) {
      for (const errorPropName of Object.keys(expectedResults.errorResult)) {
        const expected = expectedResults.errorResult[errorPropName];
        const actual = errorResult[errorPropName];

        if (expected instanceof RegExp) {
          ok(
            expected.test(actual),
            `Got exceptionInfo.${errorPropName} value ${actual} matches ${expected}`
          );
        } else {
          Assert.deepEqual(
            actual,
            expected,
            `Got the expected exceptionInfo.${errorPropName} value`
          );
        }
      }
    }
  }

  await closeToolboxForTab(tab);

  await extension.unload();

  BrowserTestUtils.removeTab(tab);
});

/**
 * This test asserts that both the page and the panel can use devtools.inspectedWindow.
 * See regression in Bug https://bugzilla.mozilla.org/show_bug.cgi?id=1392531
 */
add_task(async function test_devtools_inspectedWindow_eval_in_page_and_panel() {
  const TEST_TARGET_URL = "http://mochi.test:8888/";
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_TARGET_URL
  );

  async function devtools_page() {
    await browser.devtools.panels.create(
      "test-eval",
      "fake-icon.png",
      "devtools_panel.html"
    );

    browser.test.onMessage.addListener(async (msg, ...args) => {
      switch (msg) {
        case "inspectedWindow-page-eval-request": {
          const [
            evalResult,
            errorResult,
          ] = await browser.devtools.inspectedWindow.eval(...args);
          browser.test.sendMessage("inspectedWindow-page-eval-result", {
            evalResult,
            errorResult,
          });
          break;
        }
        case "inspectedWindow-panel-eval-request":
          // Ignore the test message expected by the devtools panel.
          break;
        default:
          browser.test.fail(`Unexpected test message received: ${msg}`);
      }
    });

    browser.test.sendMessage("devtools_panel_created");
  }

  function devtools_panel() {
    browser.test.onMessage.addListener(async (msg, ...args) => {
      switch (msg) {
        case "inspectedWindow-panel-eval-request": {
          const [
            evalResult,
            errorResult,
          ] = await browser.devtools.inspectedWindow.eval(...args);
          browser.test.sendMessage("inspectedWindow-panel-eval-result", {
            evalResult,
            errorResult,
          });
          break;
        }
        case "inspectedWindow-page-eval-request":
          // Ignore the test message expected by the devtools page.
          break;
        default:
          browser.test.fail(`Unexpected test message received: ${msg}`);
      }
    });
    browser.test.sendMessage("devtools_panel_initialized");
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
         <script text="text/javascript" src="devtools_page.js"></script>
       </head>
       <body>
       </body>
      </html>`,
      "devtools_page.js": devtools_page,
      "devtools_panel.html": `<!DOCTYPE html>
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

  await extension.startup();

  const toolbox = await openToolboxForTab(tab);

  info("Wait for devtools_panel_created event");
  await extension.awaitMessage("devtools_panel_created");

  info("Switch to the extension test panel");
  await openToolboxForTab(tab, getAdditionalPanelId(toolbox, "test-eval"));

  info("Wait for devtools_panel_initialized event");
  await extension.awaitMessage("devtools_panel_initialized");

  info(
    `test inspectedWindow.eval with eval(window.location.href) from the devtools page`
  );
  extension.sendMessage(
    `inspectedWindow-page-eval-request`,
    "window.location.href"
  );

  info("Wait for response from the page");
  let { evalResult } = await extension.awaitMessage(
    `inspectedWindow-page-eval-result`
  );
  Assert.deepEqual(
    evalResult,
    TEST_TARGET_URL,
    "Got the expected eval result in the page"
  );

  info(
    `test inspectedWindow.eval with eval(window.location.href) from the devtools panel`
  );
  extension.sendMessage(
    `inspectedWindow-panel-eval-request`,
    "window.location.href"
  );

  info("Wait for response from the panel");
  ({ evalResult } = await extension.awaitMessage(
    `inspectedWindow-panel-eval-result`
  ));
  Assert.deepEqual(
    evalResult,
    TEST_TARGET_URL,
    "Got the expected eval result in the panel"
  );

  // Cleanup
  await closeToolboxForTab(tab);
  await extension.unload();
  BrowserTestUtils.removeTab(tab);
});

/**
 * This test asserts that there's only one target created by the extension, and that
 * closing the DevTools toolbox destroys it.
 * See Bug 1652016
 */
add_task(async function test_devtools_inspectedWindow_eval_target_lifecycle() {
  const TEST_TARGET_URL = "http://mochi.test:8888/";
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_TARGET_URL
  );

  function devtools_page() {
    browser.test.onMessage.addListener(async (msg, ...args) => {
      if (msg !== "inspectedWindow-eval-requests") {
        browser.test.fail(`Unexpected test message received: ${msg}`);
        return;
      }

      const promises = [];
      for (let i = 0; i < 10; i++) {
        promises.push(browser.devtools.inspectedWindow.eval(`${i * 2}`));
      }

      await Promise.all(promises);
      browser.test.sendMessage("inspectedWindow-eval-requests-done");
    });
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
         <script text="text/javascript" src="devtools_page.js"></script>
       </head>
       <body>
       </body>
      </html>`,
      "devtools_page.js": devtools_page,
    },
  });

  await extension.startup();

  await openToolboxForTab(tab);

  let targetsCount = await getTargetActorsCount(tab);
  is(
    targetsCount,
    1,
    "There's only one target for the content page, the one for DevTools Toolbox"
  );

  info("Check that evaluating multiple times doesn't create multiple targets");
  const onEvalRequestsDone = extension.awaitMessage(
    `inspectedWindow-eval-requests-done`
  );
  extension.sendMessage(`inspectedWindow-eval-requests`);

  info("Wait for response from the panel");
  await onEvalRequestsDone;

  targetsCount = await getTargetActorsCount(tab);
  is(
    targetsCount,
    2,
    "Only 1 additional target was created when calling inspectedWindow.eval"
  );

  info(
    "Close the toolbox and make sure the extension gets unloaded, and the target destroyed"
  );
  await closeToolboxForTab(tab);

  targetsCount = await getTargetActorsCount(tab);
  is(targetsCount, 0, "All targets were removed as toolbox was closed");

  await extension.unload();
  BrowserTestUtils.removeTab(tab);
});
