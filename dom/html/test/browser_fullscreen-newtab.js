"use strict";

const kPage = "http://example.org/browser/" +
              "dom/html/test/file_fullscreen-navigate.html";

function* runTest() {
  yield BrowserTestUtils.withNewTab({ gBrowser, url: kPage }, function* (browser) {
    yield ContentTask.spawn(browser, null, function() {
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
        ++countFsChange;
        checkAndResolve();
      });
      mm.addMessageListener("Test:FullscreenError", () => {
        ++countFsError;
        checkAndResolve();
      });
    });
    let promiseNewTab =
      BrowserTestUtils.waitForNewTab(gBrowser, "about:blank");
    let promiseSizeMode =
      BrowserTestUtils.waitForEvent(window, "sizemodechange")
        .then(() => BrowserTestUtils.waitForEvent(window, "sizemodechange"));
    yield BrowserTestUtils.synthesizeMouseAtCenter("#link", {}, browser);
    let [newtab] = yield Promise.all(
      [promiseNewTab, promiseSizeMode, promiseFsEvents]);
    yield BrowserTestUtils.removeTab(newtab);

    ok(!window.fullScreen, "The chrome window should not be in fullscreen");
    ok(!document.fullscreen, "The chrome document should not be in fullscreen");
  });
}

add_task(function* () {
  yield pushPrefs(
    ["full-screen-api.unprefix.enabled", true],
    ["full-screen-api.transition-duration.enter", "0 0"],
    ["full-screen-api.transition-duration.leave", "0 0"]);
  yield runTest();
});

add_task(function* () {
  yield pushPrefs(
    ["full-screen-api.unprefix.enabled", true],
    ["full-screen-api.transition-duration.enter", "200 200"],
    ["full-screen-api.transition-duration.leave", "200 200"]);
  yield runTest();
});
