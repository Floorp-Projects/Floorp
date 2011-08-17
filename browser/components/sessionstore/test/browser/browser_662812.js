/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  window.addEventListener("SSWindowStateBusy", function onBusy() {
    window.removeEventListener("SSWindowStateBusy", onBusy, false);

    let state = JSON.parse(ss.getWindowState(window));
    ok(state.windows[0].busy, "window is busy");

    window.addEventListener("SSWindowStateReady", function onReady() {
      window.removeEventListener("SSWindowStateReady", onReady, false);

      let state = JSON.parse(ss.getWindowState(window));
      ok(!state.windows[0].busy, "window is not busy");

      gBrowser.removeTab(gBrowser.tabs[1]);
      executeSoon(finish);
    }, false);
  }, false);

  // create a new tab
  let tab = gBrowser.addTab("about:mozilla");
  let browser = tab.linkedBrowser;

  // close and restore it
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    gBrowser.removeTab(tab);
    ss.undoCloseTab(window, 0);
  }, true);
}
