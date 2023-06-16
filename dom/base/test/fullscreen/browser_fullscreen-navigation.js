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

add_task(async function navigation() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: `data:text/html,
      <button id="button">Click here</button>
      <script>
        let button = document.getElementById("button");
        button.addEventListener("click", function() {
          button.requestFullscreen();
        });
      </script>`,
    },
    async function (browser) {
      let promiseFsState = waitForFullscreenState(document, true);
      // Trigger click event
      BrowserTestUtils.synthesizeMouseAtCenter("#button", {}, browser);
      await promiseFsState;

      promiseFsState = waitForFullscreenState(document, false);
      await SpecialPowers.spawn(browser, [], async function () {
        content.location.href = "about:blank";
      });
      await promiseFsState;

      // Ensure the browser exits fullscreen state.
      ok(!window.fullScreen, "The chrome window should not be in fullscreen");
      ok(
        !document.documentElement.hasAttribute("inDOMFullscreen"),
        "The chrome document should not be in fullscreen"
      );
    }
  );
});

async function startTests(testFun, name) {
  TEST_URLS.forEach(url => {
    add_task(async () => {
      info(`Test ${name}, url: ${url}`);
      await BrowserTestUtils.withNewTab(
        {
          gBrowser,
          url,
        },
        async function (browser) {
          let promiseFsState = waitForFullscreenState(document, true);
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

          // This should exit fullscreen
          promiseFsState = waitForFullscreenState(document, false);
          await testFun(browser);
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

function NavigateRemoteDocument(aBrowsingContext, aURL) {
  return SpecialPowers.spawn(aBrowsingContext, [aURL], async function (url) {
    content.location.href = url;
  });
}

startTests(async browser => {
  // toplevel
  await NavigateRemoteDocument(browser.browsingContext, "about:blank");
}, "navigation_toplevel");

startTests(async browser => {
  let promiseRemoteFsState = waitRemoteFullscreenExitEvents([
    // browsingContext, name
    [browser.browsingContext, "toplevel"],
  ]);
  // middle iframe
  await NavigateRemoteDocument(
    browser.browsingContext.children[0],
    "about:blank"
  );
  await promiseRemoteFsState;
}, "navigation_middle_frame");

startTests(async browser => {
  let promiseRemoteFsState = waitRemoteFullscreenExitEvents([
    // browsingContext, name
    [browser.browsingContext, "toplevel"],
    [browser.browsingContext.children[0], "middle"],
  ]);
  // innermost iframe
  await NavigateRemoteDocument(
    browser.browsingContext.children[0].children[0],
    "about:blank"
  );
  await promiseRemoteFsState;
}, "navigation_inner_frame");
