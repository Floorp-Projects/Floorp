"use strict";

/**
 * These tests are for the auto-focus behaviour on the initial browser
 * when a window is opened from content.
 */

const PAGE = `data:text/html,<a id="target" href="%23" onclick="window.open('http://www.example.com', '_blank', 'width=100,height=100');">Click me</a>`;

/**
 * Returns a Promise that resolves when a new window has
 * opened, and the "load" event has fired in that window.
 * We can't use BrowserTestUtils.domWindowOpened directly,
 * because by the time the "then" on the Promise runs,
 * DOMContentLoaded and load may have already run in the new
 * window. However, we want to be very explicit about what
 * events we're waiting for, and not rely on a quirk of our
 * Promises infrastructure.
 */
function promiseNewWindow() {
  return new Promise((resolve) => {
    let observer = (subject, topic, data) => {
      if (topic == "domwindowopened") {
        Services.ww.unregisterNotification(observer);
        let win = subject.QueryInterface(Ci.nsIDOMWindow);
        win.addEventListener("load", function onLoad() {
          win.removeEventListener("load", onLoad);
          resolve(win);
        });
      }
    };

    Services.ww.registerNotification(observer);
  });
}

/**
 * Test that when a new window is opened from content, focus moves
 * to the initial browser in that window once the window has finished
 * painting.
 */
add_task(function* test_focus_browser() {
  yield BrowserTestUtils.withNewTab({
    url: PAGE,
    gBrowser,
  }, function*(browser) {
    let newWinPromise = promiseNewWindow();
    let delayedStartupPromise = BrowserTestUtils.waitForNewWindow();

    yield BrowserTestUtils.synthesizeMouseAtCenter("#target", {}, browser);
    let newWin = yield newWinPromise;
    yield BrowserTestUtils.contentPainted(newWin.gBrowser.selectedBrowser);
    yield delayedStartupPromise;

    let focusedElement =
      Services.focus.getFocusedElementForWindow(newWin, false, {});

    Assert.equal(focusedElement, newWin.gBrowser.selectedBrowser,
                 "Initial browser should be focused");

    yield BrowserTestUtils.closeWindow(newWin);
  });
});

/**
 * Test that when a new window is opened from content and focus
 * shifts in that window before the content has a chance to paint
 * that we _don't_ steal focus once content has painted.
 */
add_task(function* test_no_steal_focus() {
  yield BrowserTestUtils.withNewTab({
    url: PAGE,
    gBrowser,
  }, function*(browser) {
    let newWinPromise = promiseNewWindow();
    let delayedStartupPromise = BrowserTestUtils.waitForNewWindow();

    yield BrowserTestUtils.synthesizeMouseAtCenter("#target", {}, browser);
    let newWin = yield newWinPromise;

    // Because we're switching focus, we shouldn't steal it once
    // content paints.
    newWin.gURLBar.focus();

    yield BrowserTestUtils.contentPainted(newWin.gBrowser.selectedBrowser);
    yield delayedStartupPromise;

    let focusedElement =
      Services.focus.getFocusedElementForWindow(newWin, false, {});

    Assert.equal(focusedElement, newWin.gURLBar.inputField,
                 "URLBar should be focused");

    yield BrowserTestUtils.closeWindow(newWin);
  });
});
