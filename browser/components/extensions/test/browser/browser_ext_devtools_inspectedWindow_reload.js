/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// Like most of the mochitest-browser devtools test,
// on debug test slave, it takes about 50s to run the test.
requestLongerTimeout(4);

loadTestSubscript("head_devtools.js");

// Small helper which provides the common steps to the following reload test cases.
async function runReloadTestCase({
  urlParams,
  background,
  devtoolsPage,
  testCase,
}) {
  const BASE =
    "http://mochi.test:8888/browser/browser/components/extensions/test/browser/";
  const TEST_TARGET_URL = `${BASE}/file_inspectedwindow_reload_target.sjs?${urlParams}`;
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

  await openToolboxForTab(tab);

  // Wait the test extension to be ready.
  await extension.awaitMessage("devtools_inspected_window_reload.ready");

  info("devtools page ready");

  // Run the test case.
  await testCase(extension);

  await closeToolboxForTab(tab);

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
    testCase: async function(extension) {
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
    function background() {
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
                expectedContent = window.navigator.userAgent;
                break;
              case 2:
                enabled = true;
                expectedContent = "CustomizedUserAgent";
                break;
            }

            if (!expectedContent) {
              browser.test.fail(`Unexpected number of tab reloads: ${reloads}`);
            } else {
              const code = `document.body.textContent`;
              try {
                const [text] = await browser.tabs.executeScript(activeTabId, {
                  code,
                });
                browser.test.assertEq(
                  expectedContent,
                  text,
                  `Got the expected userAgent with userAgent=${enabled}`
                );
              } catch (err) {
                browser.test.fail(`Error: ${err.message} - ${err.stack}`);
              }
            }

            browser.test.sendMessage(
              "devtools_inspectedWindow_reload_checkUserAgent.done"
            );
          }
        });

        browser.test.sendMessage("devtools_inspected_window_reload.ready");
      });
    }

    function devtoolsPage() {
      browser.test.onMessage.addListener(msg => {
        switch (msg) {
          case "no-custom-user-agent":
            browser.devtools.inspectedWindow.reload({});
            break;
          case "custom-user-agent":
            browser.devtools.inspectedWindow.reload({
              userAgent: "CustomizedUserAgent",
            });
            break;
          default:
            browser.test.fail(`Unexpected test message received: ${msg}`);
        }
      });

      browser.runtime.sendMessage("devtools_page.ready");
    }

    await runReloadTestCase({
      urlParams: "test=user-agent",
      background,
      devtoolsPage,
      testCase: async function(extension) {
        extension.sendMessage("no-custom-user-agent");

        await extension.awaitMessage(
          "devtools_inspectedWindow_reload_checkUserAgent.done"
        );

        extension.sendMessage("custom-user-agent");

        await extension.awaitMessage(
          "devtools_inspectedWindow_reload_checkUserAgent.done"
        );
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
    testCase: async function(extension) {
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
