/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function* () {
  let win = yield promiseOpenAndLoadWindow({}, true /* wait for delayed startup */);

  // Remove the navigation toolbar.
  win.document.getElementById("nav-bar").remove();

  yield new Promise(resolve => waitForFocus(resolve, win));
  yield promiseBrowserLoaded(win.gBrowser.browsers[0]);

  try {
    let PN = win.PopupNotifications;
    let panelPromise = promisePopupShown(PN.panel);
    let notification = PN.show(win.gBrowser.selectedBrowser, "some-notification", "Some message");
    ok(notification, "show() succeeded");
    yield panelPromise;

    ok(PN.isPanelOpen, "panel is open");
    is(PN.panel.anchorNode, win.gBrowser.selectedTab, "notification is correctly anchored to the tab");
    PN.panel.hidePopup();
  } catch (ex) {
    ok(false, "threw exception: " + ex);
  }

  yield promiseWindowClosed(win);
});

function promiseBrowserLoaded(browser) {
  if (browser.contentDocument.readyState == "complete") {
    return Promise.resolve();
  }

  return new Promise(resolve => {
    browser.addEventListener("load", function onLoad() {
      browser.removeEventListener("load", onLoad, true);
      resolve();
    }, true);
  });
}
