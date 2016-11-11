/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */


const URL = "http://mochi.test:8888/browser/";
const PREF = "browser.sessionstore.restore_on_demand";

function test() {
  waitForExplicitFinish();

  Services.prefs.setBoolPref(PREF, true);
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref(PREF);
  });

  preparePendingTab(function(aTab) {
    let win = gBrowser.replaceTabWithWindow(aTab);

    whenDelayedStartupFinished(win, function() {
      let [tab] = win.gBrowser.tabs;

      whenLoaded(tab.linkedBrowser, function() {
        is(tab.linkedBrowser.currentURI.spec, URL, "correct url should be loaded");
        ok(!tab.hasAttribute("pending"), "tab should not be pending");

        win.close();
        finish();
      });
    });
  });
}

function preparePendingTab(aCallback) {
  let tab = gBrowser.addTab(URL);

  whenLoaded(tab.linkedBrowser, function() {
    BrowserTestUtils.removeTab(tab).then(() => {
      let [{state}] = JSON.parse(SessionStore.getClosedTabData(window));

      tab = gBrowser.addTab("about:blank");
      whenLoaded(tab.linkedBrowser, function() {
        SessionStore.setTabState(tab, JSON.stringify(state));
        ok(tab.hasAttribute("pending"), "tab should be pending");
        aCallback(tab);
      });
    });
  });
}

function whenLoaded(aElement, aCallback) {
  aElement.addEventListener("load", function onLoad() {
    aElement.removeEventListener("load", onLoad, true);
    executeSoon(aCallback);
  }, true);
}
