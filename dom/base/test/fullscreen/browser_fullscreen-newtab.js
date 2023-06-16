"use strict";

// This test tends to trigger a race in the fullscreen time telemetry,
// where the fullscreen enter and fullscreen exit events (which use the
// same histogram ID) overlap. That causes TelemetryStopwatch to log an
// error.
SimpleTest.ignoreAllUncaughtExceptions(true);

const kPage =
  "https://example.org/browser/" +
  "dom/base/test/fullscreen/file_fullscreen-newtab.html";

function getSizeMode() {
  return document.documentElement.getAttribute("sizemode");
}

async function runTest() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: kPage,
    },
    async function (browser) {
      let promiseFsEvents = SpecialPowers.spawn(browser, [], function () {
        return new Promise(resolve => {
          let countFsChange = 0;
          let countFsError = 0;
          function checkAndResolve() {
            if (countFsChange > 0 && countFsError > 0) {
              Assert.ok(
                false,
                "Got both fullscreenchange and fullscreenerror events"
              );
            } else if (countFsChange > 2) {
              Assert.ok(false, "Got too many fullscreenchange events");
            } else if (countFsError > 1) {
              Assert.ok(false, "Got too many fullscreenerror events");
            } else if (countFsChange == 2 || countFsError == 1) {
              resolve();
            }
          }

          content.document.addEventListener("fullscreenchange", () => {
            ++countFsChange;
            checkAndResolve();
          });
          content.document.addEventListener("fullscreenerror", () => {
            ++countFsError;
            checkAndResolve();
          });
        });
      });
      let promiseNewTab = BrowserTestUtils.waitForNewTab(
        gBrowser,
        "about:blank"
      );
      await BrowserTestUtils.synthesizeMouseAtCenter("#link", {}, browser);
      let [newtab] = await Promise.all([promiseNewTab, promiseFsEvents]);
      await BrowserTestUtils.removeTab(newtab);

      // Ensure the browser exits fullscreen state in reasonable time.
      await Promise.race([
        BrowserTestUtils.waitForCondition(() => getSizeMode() == "normal"),
        // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
        new Promise(resolve => setTimeout(resolve, 2000)),
      ]);

      ok(!window.fullScreen, "The chrome window should not be in fullscreen");
      ok(
        !document.fullscreen,
        "The chrome document should not be in fullscreen"
      );
    }
  );
}

add_task(async function () {
  await pushPrefs(
    ["full-screen-api.transition-duration.enter", "0 0"],
    ["full-screen-api.transition-duration.leave", "0 0"]
  );
  await runTest();
});

add_task(async function () {
  await pushPrefs(
    ["full-screen-api.transition-duration.enter", "200 200"],
    ["full-screen-api.transition-duration.leave", "200 200"]
  );
  await runTest();
});
