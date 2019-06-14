"use strict";

// This test tends to trigger a race in the fullscreen time telemetry,
// where the fullscreen enter and fullscreen exit events (which use the
// same histogram ID) overlap. That causes TelemetryStopwatch to log an
// error.
SimpleTest.ignoreAllUncaughtExceptions(true);

const kPage = "http://example.org/browser/" +
              "dom/html/test/file_fullscreen-newtab.html";

function getSizeMode() {
  return document.documentElement.getAttribute("sizemode");
}

async function runTest() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: kPage
  }, async function (browser) {
    await ContentTask.spawn(browser, null, function() {
      content.document.addEventListener("fullscreenchange", () => {
        sendAsyncMessage("Test:FullscreenChange");
      });
      content.document.addEventListener("fullscreenerror", () => {
        sendAsyncMessage("Test:FullscreenError");
      });
    });
    let promiseFsEvents = new Promise(resolve => {
      let countFsChange = 0;
      let countFsError = 0;
      function checkAndResolve() {
        if (countFsChange > 0 && countFsError > 0) {
          ok(false, "Got both fullscreenchange and fullscreenerror events");
        } else if (countFsChange > 2) {
          ok(false, "Got too many fullscreenchange events");
        } else if (countFsError > 1) {
          ok(false, "Got too many fullscreenerror events");
        } else if (countFsChange == 2 || countFsError == 1) {
          resolve();
        }
      }
      let mm = browser.messageManager;
      mm.addMessageListener("Test:FullscreenChange", () => {
        info("Got fullscreenchange event");
        ++countFsChange;
        checkAndResolve();
      });
      mm.addMessageListener("Test:FullscreenError", () => {
        info("Got fullscreenerror event");
        ++countFsError;
        checkAndResolve();
      });
    });
    let promiseNewTab =
      BrowserTestUtils.waitForNewTab(gBrowser, "about:blank");
    await BrowserTestUtils.synthesizeMouseAtCenter("#link", {}, browser);
    let [newtab] = await Promise.all([promiseNewTab, promiseFsEvents]);
    await BrowserTestUtils.removeTab(newtab);

    // Ensure the browser exits fullscreen state in reasonable time.
    await Promise.race([
      BrowserTestUtils.waitForCondition(() => getSizeMode() == "normal"),
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      new Promise(resolve => setTimeout(resolve, 2000))
    ]);

    ok(!window.fullScreen, "The chrome window should not be in fullscreen");
    ok(!document.fullscreen, "The chrome document should not be in fullscreen");
  });
}

add_task(async function () {
  await pushPrefs(
    ["full-screen-api.unprefix.enabled", true],
    ["full-screen-api.transition-duration.enter", "0 0"],
    ["full-screen-api.transition-duration.leave", "0 0"]);
  await runTest();
});

add_task(async function () {
  await pushPrefs(
    ["full-screen-api.unprefix.enabled", true],
    ["full-screen-api.transition-duration.enter", "200 200"],
    ["full-screen-api.transition-duration.leave", "200 200"]);
  await runTest();
});
