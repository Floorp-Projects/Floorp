/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Import helpers
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/dom/base/test/fullscreen/fullscreen_helpers.js",
  this
);

// This test tends to trigger a race in the fullscreen time telemetry,
// where the fullscreen enter and fullscreen exit events (which use the
// same histogram ID) overlap. That causes TelemetryStopwatch to log an
// error.
SimpleTest.ignoreAllUncaughtExceptions(true);

add_setup(async function () {
  await pushPrefs(
    ["full-screen-api.transition-duration.enter", "0 0"],
    ["full-screen-api.transition-duration.leave", "0 0"],
    ["full-screen-api.allow-trusted-requests-only", false]
  );
});

async function startTests(setupFun, name) {
  TEST_URLS.forEach(url => {
    add_task(async () => {
      info(`Test ${name}, url: ${url}`);
      await BrowserTestUtils.withNewTab(
        {
          gBrowser,
          url,
        },
        async function (browser) {
          let promiseFsState = waitForFullscreenExit(document);
          setupFun(browser);
          // Trigger click event in inner most iframe
          SpecialPowers.spawn(
            browser.browsingContext.children[0].children[0],
            [],
            function () {
              content.setTimeout(() => {
                content.document.getElementById("div").click();
              }, 0);
            }
          );
          await promiseFsState;

          // Ensure the browser exits fullscreen state.
          ok(
            !window.fullScreen,
            "The chrome window should not be in fullscreen"
          );
          ok(
            !document.documentElement.hasAttribute("inDOMFullscreen"),
            "The chrome document should not be in fullscreen"
          );
        }
      );
    });
  });
}

async function WaitRemoveDocumentAndCloseTab(aBrowser, aBrowsingContext) {
  await SpecialPowers.spawn(aBrowsingContext, [], async function () {
    return new Promise(resolve => {
      content.document.addEventListener(
        "fullscreenchange",
        e => {
          resolve();
        },
        { once: true }
      );
    });
  });

  // This should exit fullscreen
  let tab = gBrowser.getTabForBrowser(aBrowser);
  BrowserTestUtils.removeTab(tab);
}

startTests(async browser => {
  // toplevel
  WaitRemoveDocumentAndCloseTab(browser, browser.browsingContext);
}, "tab_close_toplevel");

startTests(browser => {
  // middle iframe
  WaitRemoveDocumentAndCloseTab(browser, browser.browsingContext.children[0]);
}, "tab_close_middle_frame");

startTests(async browser => {
  // innermost iframe
  WaitRemoveDocumentAndCloseTab(
    browser,
    browser.browsingContext.children[0].children[0]
  );
}, "tab_close_inner_frame");
