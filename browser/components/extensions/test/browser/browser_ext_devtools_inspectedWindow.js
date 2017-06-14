/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const {DevToolsShim} = Cu.import("chrome://devtools-shim/content/DevToolsShim.jsm", {});
const {gDevTools} = DevToolsShim;

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
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/");

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

  await extension.startup();

  let backgroundPageCurrentTabId = await extension.awaitMessage("current-tab-id");

  let target = gDevTools.getTargetForTab(tab);

  await gDevTools.showToolbox(target, "webconsole");
  info("developer toolbox opened");

  let devtoolsInspectedWindowTabId = await extension.awaitMessage("inspectedWindow-tab-id");

  is(devtoolsInspectedWindowTabId, backgroundPageCurrentTabId,
     "Got the expected tabId from devtool.inspectedWindow.tabId");

  let devtoolsPageIframeTabId = await extension.awaitMessage("devtools_page_iframe.inspectedWindow-tab-id");

  is(devtoolsPageIframeTabId, backgroundPageCurrentTabId,
     "Got the expected tabId from devtool.inspectedWindow.tabId called in a devtool_page iframe");

  await gDevTools.closeToolbox(target);

  await target.destroy();

  await extension.unload();

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_devtools_inspectedWindow_eval() {
  const TEST_TARGET_URL = "http://mochi.test:8888/";
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_TARGET_URL);

  function devtools_page() {
    browser.test.onMessage.addListener(async (msg, ...args) => {
      if (msg !== "inspectedWindow-eval-request") {
        browser.test.fail(`Unexpected test message received: ${msg}`);
        return;
      }

      try {
        const [evalResult, errorResult] = await browser.devtools.inspectedWindow.eval(...args);
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

  let target = gDevTools.getTargetForTab(tab);

  await gDevTools.showToolbox(target, "webconsole");
  info("developer toolbox opened");

  const evalTestCases = [
    // Successful evaluation results.
    {
      args: ["window.location.href"],
      expectedResults: {evalResult: TEST_TARGET_URL, errorResult: undefined},
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
          details: [
            "TypeError: cyclic object value",
          ],
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

    const {args, expectedResults} = testCase;

    extension.sendMessage(`inspectedWindow-eval-request`, ...args);

    const {evalResult, errorResult} = await extension.awaitMessage(`inspectedWindow-eval-result`);

    Assert.deepEqual(evalResult, expectedResults.evalResult, "Got the expected eval result");

    if (errorResult) {
      for (const errorPropName of Object.keys(expectedResults.errorResult)) {
        const expected = expectedResults.errorResult[errorPropName];
        const actual = errorResult[errorPropName];

        if (expected instanceof RegExp) {
          ok(expected.test(actual),
             `Got exceptionInfo.${errorPropName} value ${actual} matches ${expected}`);
        } else {
          Assert.deepEqual(actual, expected,
                           `Got the expected exceptionInfo.${errorPropName} value`);
        }
      }
    }
  }

  await gDevTools.closeToolbox(target);

  await target.destroy();

  await extension.unload();

  await BrowserTestUtils.removeTab(tab);
});
