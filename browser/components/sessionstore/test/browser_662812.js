/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  window.addEventListener("SSWindowStateBusy", function() {
    let state = JSON.parse(ss.getWindowState(window));
    ok(state.windows[0].busy, "window is busy");

    window.addEventListener("SSWindowStateReady", function() {
      let state2 = JSON.parse(ss.getWindowState(window));
      ok(!state2.windows[0].busy, "window is not busy");

      executeSoon(() => {
        gBrowser.removeTab(gBrowser.tabs[1]);
        finish();
      });
    }, {once: true});
  }, {once: true});

  // create a new tab
  let tab = BrowserTestUtils.addTab(gBrowser, "about:mozilla");
  let browser = tab.linkedBrowser;

  // close and restore it
  browser.addEventListener("load", function() {
    gBrowser.removeTab(tab);
    ss.undoCloseTab(window, 0);
  }, {capture: true, once: true});
}
