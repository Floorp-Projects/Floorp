/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import helpers
/* import-globals-from fullscreen_helpers.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/dom/base/test/fullscreen/fullscreen_helpers.js",
  this
);

// This test tends to trigger a race in the fullscreen time telemetry,
// where the fullscreen enter and fullscreen exit events (which use the
// same histogram ID) overlap. That causes TelemetryStopwatch to log an
// error, https://bugzilla.mozilla.org/show_bug.cgi?id=1742890.
SimpleTest.ignoreAllUncaughtExceptions(true);

add_setup(async function () {
  await pushPrefs(
    ["full-screen-api.transition-duration.enter", "0 0"],
    ["full-screen-api.transition-duration.leave", "0 0"],
    ["full-screen-api.allow-trusted-requests-only", false]
  );
});

async function waitAndCheckFullscreenState(aWindow) {
  // Wait fullscreen exit event if browser is still in fullscreen mode.
  if (
    aWindow.fullScreen ||
    aWindow.document.documentElement.hasAttribute("inFullscreen")
  ) {
    info("The widget is still in fullscreen, wait again");
    await waitWidgetFullscreenEvent(aWindow, false, true);
  }
  if (aWindow.document.documentElement.hasAttribute("inDOMFullscreen")) {
    info("The chrome document is still in fullscreen, wait again");
    await waitForFullScreenObserver(aWindow, false, true);
  }

  // Ensure the browser exits fullscreen state.
  ok(!aWindow.fullScreen, "The widget should not be in fullscreen");
  ok(
    !aWindow.document.documentElement.hasAttribute("inFullscreen"),
    "The chrome window should not be in fullscreen"
  );
  ok(
    !aWindow.document.documentElement.hasAttribute("inDOMFullscreen"),
    "The chrome document should not be in fullscreen"
  );
}

add_task(async () => {
  const URL =
    "http://mochi.test:8888/browser/dom/base/test/fullscreen/file_fullscreen-bug-1798219.html";
  // We need this dummy tab which load the same URL as test tab to keep the
  // original content process alive after test page navigates away.
  let dummyTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: URL,
    },
    async function (browser) {
      await SpecialPowers.spawn(browser, [], function () {
        content.document.querySelector("button").click();
      });

      // Test requests fullscreen and performs navigation simultaneously,
      // the fullscreen request might be rejected directly if navigation happens
      // first, so there might be no reliable state that we can wait. So give
      // some time for possible fullscreen transition instead and ensure window
      // should end up exiting fullscreen.
      await new Promise(aResolve => {
        SimpleTest.executeSoon(() => {
          SimpleTest.executeSoon(aResolve);
        });
      });
      await waitAndCheckFullscreenState(window);
    }
  );

  let dummyTabClosed = BrowserTestUtils.waitForTabClosing(dummyTab);
  BrowserTestUtils.removeTab(dummyTab);
  await dummyTabClosed;
});

add_task(async () => {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "http://mochi.test:8888/browser/dom/base/test/fullscreen/file_fullscreen-bug-1798219-2.html",
    },
    async function (browser) {
      // Open a new window to run the tests, the original window will keep the
      // original content process alive after the test window navigates away.
      let promiseWin = BrowserTestUtils.waitForNewWindow();
      await SpecialPowers.spawn(browser, [], function () {
        content.document.querySelector("button").click();
      });
      let newWindow = await promiseWin;

      await SpecialPowers.spawn(
        newWindow.gBrowser.selectedTab.linkedBrowser,
        [],
        function () {
          content.document.querySelector("button").click();
        }
      );

      // Test requests fullscreen and performs navigation simultaneously,
      // the fullscreen request might be rejected directly if navigation happens
      // first, so there might be no reliable state that we can wait. So give
      // some time for possible fullscreen transition instead and ensure window
      // should end up exiting fullscreen.
      await new Promise(aResolve => {
        SimpleTest.executeSoon(() => {
          SimpleTest.executeSoon(aResolve);
        });
      });
      await waitAndCheckFullscreenState(newWindow);

      newWindow.close();
    }
  );
});
