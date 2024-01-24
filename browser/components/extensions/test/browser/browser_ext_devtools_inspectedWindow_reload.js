/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// Like most of the mochitest-browser devtools test,
// on debug test machine, it takes about 50s to run the test.
requestLongerTimeout(4);

loadTestSubscript("head_devtools.js");

// Allow rejections related to closing the devtools toolbox too soon after the test
// has already verified the details that were relevant for that test case
// (e.g. this was triggering an intermittent failure in shippable optimized
// builds, tracked Bug 1707644).
PromiseTestUtils.allowMatchingRejectionsGlobally(
  /Connection closed, pending request to/
);

const TEST_ORIGIN = "http://mochi.test:8888";
const TEST_BASE = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  ""
);
const TEST_PATH = `${TEST_BASE}file_inspectedwindow_reload_target.sjs`;

// Small helper which provides the common steps to the following reload test cases.
async function runReloadTestCase({
  urlParams,
  background,
  devtoolsPage,
  testCase,
  closeToolbox = true,
}) {
  const TEST_TARGET_URL = `${TEST_ORIGIN}/${TEST_PATH}?${urlParams}`;
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_TARGET_URL
  );

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      devtools_page: "devtools_page.html",
      permissions: ["webNavigation", "<all_urls>"],
    },
    files: {
      "devtools_page.html": `<!DOCTYPE html>
      <html>
       <head>
         <meta charset="utf-8">
         <script type="text/javascript" src="devtools_page.js"></script>
       </head>
       <body>
       </body>
      </html>`,
      "devtools_page.js": devtoolsPage,
    },
  });

  await extension.startup();

  const toolbox = await openToolboxForTab(tab);

  // Wait the test extension to be ready.
  await extension.awaitMessage("devtools_inspected_window_reload.ready");

  info("devtools page ready");

  // Run the test case.
  await testCase(extension, tab, toolbox);

  if (closeToolbox) {
    await closeToolboxForTab(tab);
  }

  BrowserTestUtils.removeTab(tab);

  await extension.unload();
}

add_task(async function test_devtools_inspectedWindow_reload_ignore_cache() {
  function background() {
    // Wait until the devtools page is ready to run the test.
    browser.runtime.onMessage.addListener(async msg => {
      if (msg !== "devtools_page.ready") {
        browser.test.fail(`Unexpected message received: ${msg}`);
        return;
      }

      const tabs = await browser.tabs.query({ active: true });
      const activeTabId = tabs[0].id;
      let reloads = 0;

      browser.webNavigation.onCompleted.addListener(async details => {
        if (details.tabId == activeTabId && details.frameId == 0) {
          reloads++;

          // This test expects two `devtools.inspectedWindow.reload` calls:
          // the first one without any options and the second one with
          // `ignoreCache=true`.
          let expectedContent;
          let enabled;

          switch (reloads) {
            case 1:
              enabled = false;
              expectedContent = "empty cache headers";
              break;
            case 2:
              enabled = true;
              expectedContent = "no-cache:no-cache";
              break;
          }

          if (!expectedContent) {
            browser.test.fail(`Unexpected number of tab reloads: ${reloads}`);
          } else {
            try {
              const code = `document.body.textContent`;
              const [text] = await browser.tabs.executeScript(activeTabId, {
                code,
              });

              browser.test.assertEq(
                text,
                expectedContent,
                `Got the expected cache headers with ignoreCache=${enabled}`
              );
            } catch (err) {
              browser.test.fail(`Error: ${err.message} - ${err.stack}`);
            }
          }

          browser.test.sendMessage(
            "devtools_inspectedWindow_reload_checkIgnoreCache.done"
          );
        }
      });

      browser.test.sendMessage("devtools_inspected_window_reload.ready");
    });
  }

  async function devtoolsPage() {
    browser.test.onMessage.addListener(msg => {
      switch (msg) {
        case "no-ignore-cache":
          browser.devtools.inspectedWindow.reload();
          break;
        case "ignore-cache":
          browser.devtools.inspectedWindow.reload({ ignoreCache: true });
          break;
        default:
          browser.test.fail(`Unexpected test message received: ${msg}`);
      }
    });

    browser.runtime.sendMessage("devtools_page.ready");
  }

  await runReloadTestCase({
    urlParams: "test=cache",
    background,
    devtoolsPage,
    testCase: async function (extension) {
      for (const testMessage of ["no-ignore-cache", "ignore-cache"]) {
        extension.sendMessage(testMessage);
        await extension.awaitMessage(
          "devtools_inspectedWindow_reload_checkIgnoreCache.done"
        );
      }
    },
  });
});

add_task(
  async function test_devtools_inspectedWindow_reload_custom_user_agent() {
    const CUSTOM_USER_AGENT = "CustomizedUserAgent";

    function background() {
      browser.runtime.onMessage.addListener(async msg => {
        if (msg !== "devtools_page.ready") {
          browser.test.fail(`Unexpected message received: ${msg}`);
          return;
        }

        browser.test.sendMessage("devtools_inspected_window_reload.ready");
      });
    }

    function devtoolsPage() {
      browser.test.onMessage.addListener(async msg => {
        switch (msg) {
          case "no-custom-user-agent":
            await browser.devtools.inspectedWindow.reload({});
            break;
          case "custom-user-agent":
            await browser.devtools.inspectedWindow.reload({
              userAgent: "CustomizedUserAgent",
            });
            break;
          default:
            browser.test.fail(`Unexpected test message received: ${msg}`);
        }
      });

      browser.runtime.sendMessage("devtools_page.ready");
    }

    async function checkUserAgent(expectedUA) {
      const contexts =
        gBrowser.selectedBrowser.browsingContext.getAllBrowsingContextsInSubtree();

      const uniqueRemoteTypes = new Set();
      for (const context of contexts) {
        uniqueRemoteTypes.add(context.currentRemoteType);
      }

      info(
        `Got ${contexts.length} with remoteTypes: ${Array.from(
          uniqueRemoteTypes
        )}`
      );
      Assert.greaterOrEqual(
        contexts.length,
        2,
        "There should be at least 2 browsing contexts"
      );

      if (Services.appinfo.fissionAutostart) {
        Assert.greaterOrEqual(
          uniqueRemoteTypes.size,
          2,
          "Expect at least one cross origin sub frame"
        );
      }

      for (const context of contexts) {
        const url = context.currentURI?.spec?.replace(
          context.currentURI?.query,
          "…"
        );
        info(
          `Checking user agent on ${url} (remoteType: ${context.currentRemoteType})`
        );
        await SpecialPowers.spawn(context, [expectedUA], async _expectedUA => {
          is(
            content.navigator.userAgent,
            _expectedUA,
            `expected navigator.userAgent value`
          );
          is(
            content.wrappedJSObject.initialUserAgent,
            _expectedUA,
            `expected navigator.userAgent value at startup`
          );
          if (content.wrappedJSObject.userAgentHeader) {
            is(
              content.wrappedJSObject.userAgentHeader,
              _expectedUA,
              `user agent header has expected value`
            );
          }
        });
      }
    }

    await runReloadTestCase({
      urlParams: "test=user-agent",
      background,
      devtoolsPage,
      closeToolbox: false,
      testCase: async function (extension, tab, toolbox) {
        info("Get the initial user agent");
        const initialUserAgent = await SpecialPowers.spawn(
          gBrowser.selectedBrowser,
          [],
          () => {
            return content.navigator.userAgent;
          }
        );

        info(
          "Check that calling inspectedWindow.reload without userAgent does not change the user agent of the page"
        );
        let onPageLoaded = BrowserTestUtils.browserLoaded(
          gBrowser.selectedBrowser,
          true
        );
        extension.sendMessage("no-custom-user-agent");
        await onPageLoaded;

        await checkUserAgent(initialUserAgent);

        info(
          "Check that calling inspectedWindow.reload with userAgent does change the user agent of the page"
        );
        onPageLoaded = BrowserTestUtils.browserLoaded(
          gBrowser.selectedBrowser,
          true
        );
        extension.sendMessage("custom-user-agent");
        await onPageLoaded;

        await checkUserAgent(CUSTOM_USER_AGENT);

        info("Check that the user agent persists after a reload");
        await BrowserTestUtils.reloadTab(tab, /* includeSubFrames */ true);
        await checkUserAgent(CUSTOM_USER_AGENT);

        info(
          "Check that the user agent persists after navigating to a new browsing context"
        );
        const previousBrowsingContextId =
          gBrowser.selectedBrowser.browsingContext.id;

        // Navigate to a different origin
        await navigateToWithDevToolsOpen(
          tab,
          `https://example.com/${TEST_PATH}?test=user-agent&crossOriginIsolated=true`
        );

        isnot(
          gBrowser.selectedBrowser.browsingContext.id,
          previousBrowsingContextId,
          "A new browsing context was created"
        );

        await checkUserAgent(CUSTOM_USER_AGENT);

        info(
          "Check that closing DevTools resets the user agent of the page to its initial value"
        );

        await closeToolboxForTab(tab);

        // XXX: This is needed at the moment since Navigator.cpp retrieves the UserAgent from the
        // headers (when there's no custom user agent). And here, since we reloaded the page once
        // we set the custom user agent, the header was set accordingly and still holds the custom
        // user agent value. This should be fixed by Bug 1705326.
        is(
          gBrowser.selectedBrowser.browsingContext.customUserAgent,
          "",
          "The flag on the browsing context was reset"
        );
        await checkUserAgent(CUSTOM_USER_AGENT);
        await BrowserTestUtils.reloadTab(tab, /* includeSubFrames */ true);
        await checkUserAgent(initialUserAgent);
      },
    });
  }
);

add_task(async function test_devtools_inspectedWindow_reload_injected_script() {
  function background() {
    function getIframesTextContent() {
      let docs = [];
      for (
        let iframe, doc = document;
        doc;
        doc = iframe && iframe.contentDocument
      ) {
        docs.push(doc);
        iframe = doc.querySelector("iframe");
      }

      return docs.map(doc => doc.querySelector("pre").textContent);
    }

    browser.runtime.onMessage.addListener(async msg => {
      if (msg !== "devtools_page.ready") {
        browser.test.fail(`Unexpected message received: ${msg}`);
        return;
      }

      const tabs = await browser.tabs.query({ active: true });
      const activeTabId = tabs[0].id;
      let reloads = 0;

      browser.webNavigation.onCompleted.addListener(async details => {
        if (details.tabId == activeTabId && details.frameId == 0) {
          reloads++;

          let expectedContent;
          let enabled;

          switch (reloads) {
            case 1:
              enabled = false;
              expectedContent = "injected script NOT executed";
              break;
            case 2:
              enabled = true;
              expectedContent = "injected script executed first";
              break;
            default:
              browser.test.fail(`Unexpected number of tab reloads: ${reloads}`);
          }

          if (!expectedContent) {
            browser.test.fail(`Unexpected number of tab reloads: ${reloads}`);
          } else {
            let expectedResults = new Array(4).fill(expectedContent);
            let code = `(${getIframesTextContent})()`;

            try {
              let [results] = await browser.tabs.executeScript(activeTabId, {
                code,
              });

              browser.test.assertEq(
                JSON.stringify(expectedResults),
                JSON.stringify(results),
                `Got the expected result with injectScript=${enabled}`
              );
            } catch (err) {
              browser.test.fail(`Error: ${err.message} - ${err.stack}`);
            }
          }

          browser.test.sendMessage(
            `devtools_inspectedWindow_reload_injectedScript.done`
          );
        }
      });

      browser.test.sendMessage("devtools_inspected_window_reload.ready");
    });
  }

  function devtoolsPage() {
    function injectedScript() {
      if (!window.pageScriptExecutedFirst) {
        window.addEventListener(
          "DOMContentLoaded",
          function listener() {
            document.querySelector("pre").textContent =
              "injected script executed first";
          },
          { once: true }
        );
      }
    }

    browser.test.onMessage.addListener(msg => {
      switch (msg) {
        case "no-injected-script":
          browser.devtools.inspectedWindow.reload({});
          break;
        case "injected-script":
          browser.devtools.inspectedWindow.reload({
            injectedScript: `new ${injectedScript}`,
          });
          break;
        default:
          browser.test.fail(`Unexpected test message received: ${msg}`);
      }
    });

    browser.runtime.sendMessage("devtools_page.ready");
  }

  await runReloadTestCase({
    urlParams: "test=injected-script&frames=3",
    background,
    devtoolsPage,
    testCase: async function (extension) {
      extension.sendMessage("no-injected-script");

      await extension.awaitMessage(
        "devtools_inspectedWindow_reload_injectedScript.done"
      );

      extension.sendMessage("injected-script");

      await extension.awaitMessage(
        "devtools_inspectedWindow_reload_injectedScript.done"
      );
    },
  });
});
