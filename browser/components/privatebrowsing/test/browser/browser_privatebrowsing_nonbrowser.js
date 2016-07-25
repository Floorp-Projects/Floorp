"use strict";

/**
 * Tests that we fire the last-pb-context-exited observer notification
 * when the last private browsing window closes, even if a chrome window
 * was opened from that private browsing window.
 */
add_task(function* () {
  let win = yield BrowserTestUtils.openNewBrowserWindow({private: true});
  let chromeWin = win.open("chrome://browser/content/places/places.xul", "_blank",
    "chrome,extrachrome,menubar,resizable,scrollbars,status,toolbar");
  yield BrowserTestUtils.waitForEvent(chromeWin, "load");
  let obsPromise = TestUtils.topicObserved("last-pb-context-exited");
  yield BrowserTestUtils.closeWindow(win);
  yield obsPromise;
  Assert.ok(true, "Got the last-pb-context-exited notification");
  chromeWin.close();
});

