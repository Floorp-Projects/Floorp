/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Import helpers
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/dom/html/test/fullscreen_helpers.js",
  this
);

// This test tends to trigger a race in the fullscreen time telemetry,
// where the fullscreen enter and fullscreen exit events (which use the
// same histogram ID) overlap. That causes TelemetryStopwatch to log an
// error.
SimpleTest.ignoreAllUncaughtExceptions(true);

add_task(async function init() {
  await pushPrefs(
    ["full-screen-api.transition-duration.enter", "0 0"],
    ["full-screen-api.transition-duration.leave", "0 0"],
    ["full-screen-api.allow-trusted-requests-only", false]
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
        async function(browser) {
          let promiseFsState = waitForFullscreenState(document, true);
          // Trigger click event in inner most iframe
          SpecialPowers.spawn(
            browser.browsingContext.children[0].children[0],
            [],
            function() {
              content.setTimeout(() => {
                content.document.getElementById("div").click();
              }, 0);
            }
          );
          await promiseFsState;

          // This should exit fullscreen
          promiseFsState = waitForFullscreenState(document, false, true);
          await testFun(browser);
          await promiseFsState;

          // This test triggers a fullscreen request during the fullscreen exit
          // process, so it could be possible that document goes into fullscreen
          // mode again, but it should end up leave fullscreen mode again.
          if (window.fullScreen) {
            info("still in fullscreen, wait again");
            await waitForFullscreenState(document, false, true);
          }

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

function MutateAndNavigateFromRemoteDocument(
  aBrowsingContext,
  aElementId,
  aURL
) {
  return SpecialPowers.spawn(
    aBrowsingContext,
    [aElementId, aURL],
    async function(id, url) {
      let element = content.document.getElementById(id);
      element.requestFullscreen();
      content.document.body.appendChild(element);
      content.location.href = url;
    }
  );
}

startTests(async browser => {
  // toplevel
  await MutateAndNavigateFromRemoteDocument(
    browser.browsingContext,
    "div",
    "about:blank"
  );
}, "document_mutation_navigation_toplevel");

startTests(async browser => {
  let promiseRemoteFsState = waitRemoteFullscreenExitEvents([
    // browsingContext, name
    [browser.browsingContext, "toplevel"],
  ]);
  // middle iframe
  await MutateAndNavigateFromRemoteDocument(
    browser.browsingContext.children[0],
    "div",
    "about:blank"
  );
  await promiseRemoteFsState;
}, "document_mutation_navigation_middle_frame");

startTests(async browser => {
  let promiseRemoteFsState = waitRemoteFullscreenExitEvents([
    // browsingContext, name
    [browser.browsingContext, "toplevel"],
    [browser.browsingContext.children[0], "middle"],
  ]);
  // innermost iframe
  await MutateAndNavigateFromRemoteDocument(
    browser.browsingContext.children[0].children[0],
    "div",
    "about:blank"
  );
  await promiseRemoteFsState;
}, "document_mutation_navigation_inner_frame");
