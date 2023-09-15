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
// error, bug 1742890.
SimpleTest.ignoreAllUncaughtExceptions(true);

add_setup(async function () {
  await pushPrefs(
    ["full-screen-api.transition-duration.enter", "0 0"],
    ["full-screen-api.transition-duration.leave", "0 0"],
    ["full-screen-api.allow-trusted-requests-only", false]
  );
});

function preventBFCache(aBrowsingContext, aPrevent) {
  return SpecialPowers.spawn(aBrowsingContext, [aPrevent], prevent => {
    if (prevent) {
      // Using a dummy onunload listener to disable the bfcache.
      content.window.addEventListener("unload", () => {});
    }
    content.window.addEventListener(
      "pagehide",
      e => {
        // XXX checking persisted property causes intermittent failures, so we
        // dump the value instead, bug 1822263.
        // is(e.persisted, !prevent, `Check BFCache state`);
        info(`Check BFCache state: e.persisted is ${e.persisted}`);
      },
      { once: true }
    );
  });
}

[true, false].forEach(crossOrigin => {
  [true, false].forEach(initialPagePreventsBFCache => {
    [true, false].forEach(fullscreenPagePreventsBFCache => {
      add_task(async function navigation_history() {
        info(
          `crossOrigin: ${crossOrigin}, initialPagePreventsBFCache: ${initialPagePreventsBFCache}, fullscreenPagePreventsBFCache: ${fullscreenPagePreventsBFCache}`
        );
        await BrowserTestUtils.withNewTab(
          {
            gBrowser,
            url: "http://mochi.test:8888/browser/dom/base/test/fullscreen/dummy_page.html",
          },
          async function (browser) {
            // Maybe prevent BFCache on initial page.
            await preventBFCache(
              browser.browsingContext,
              initialPagePreventsBFCache
            );

            // Navigate to fullscreen page.
            const url = crossOrigin
              ? "https://example.org/browser/dom/base/test/fullscreen/file_fullscreen-iframe-inner.html"
              : "http://mochi.test:8888/browser/dom/base/test/fullscreen/file_fullscreen-iframe-inner.html";
            const loaded = BrowserTestUtils.browserLoaded(browser, false, url);
            BrowserTestUtils.startLoadingURIString(browser, url);
            await loaded;

            // Maybe prevent BFCache on fullscreen test page.
            await preventBFCache(
              browser.browsingContext,
              fullscreenPagePreventsBFCache
            );

            // Trigger click event to enter fullscreen.
            let promiseFsState = waitForFullscreenState(document, true);
            SpecialPowers.spawn(browser.browsingContext, [], () => {
              content.setTimeout(() => {
                content.document.getElementById("div").click();
              }, 0);
            });
            await promiseFsState;

            // Navigate back to the previous page should exit fullscreen.
            promiseFsState = waitForFullscreenState(document, false);
            await SpecialPowers.spawn(browser.browsingContext, [], () => {
              content.window.history.back();
            });
            await promiseFsState;
          }
        );
      });
    });
  });
});
