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

add_task(async () => {
  const url =
    "https://example.com/browser/dom/base/test/fullscreen/dummy_page.html";
  const name = "foo";

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async function (browser) {
      info("open new window");
      SpecialPowers.spawn(browser, [url, name], function (u, n) {
        content.document.notifyUserGestureActivation();
        content.window.open(u, n, "width=100,height=100");
      });
      let newWin = await BrowserTestUtils.waitForNewWindow({ url });
      await SimpleTest.promiseFocus(newWin);

      info("re-focusing main window");
      await SimpleTest.promiseFocus(window);

      info("open an existing window and request fullscreen");
      await SpecialPowers.spawn(browser, [url, name], function (u, n) {
        content.document.notifyUserGestureActivation();
        content.window.open(u, n);
        content.document.body.requestFullscreen();
      });

      // We call window.open() first than requestFullscreen() in a row on
      // content page, but given that focus sync-up takes several IPC exchanges,
      // so parent process ends up processing the requests in a reverse order,
      // which should reject the fullscreen request and leave fullscreen.
      await waitWidgetFullscreenEvent(window, false, true);

      // Ensure the browser exits fullscreen state.
      ok(!window.fullScreen, "The chrome window should not be in fullscreen");
      ok(
        !document.documentElement.hasAttribute("inDOMFullscreen"),
        "The chrome document should not be in fullscreen"
      );

      await BrowserTestUtils.closeWindow(newWin);
    }
  );
});
