/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let tabState = {
  entries: [{url: "data:text/html;charset=utf-8,<input%20id='foo'>", formdata: { id: { "foo": "bar" } } }]
};

function test() {
  waitForExplicitFinish();
  Services.prefs.setBoolPref("browser.sessionstore.restore_on_demand", true);

  registerCleanupFunction(function () {
    if (gBrowser.tabs.length > 1)
      gBrowser.removeTab(gBrowser.tabs[1]);
    Services.prefs.clearUserPref("browser.sessionstore.restore_on_demand");
  });

  let tab = gBrowser.addTab("about:blank");
  let browser = tab.linkedBrowser;

  whenBrowserLoaded(browser, function () {
    isnot(gBrowser.selectedTab, tab, "newly created tab is not selected");

    ss.setTabState(tab, JSON.stringify(tabState));
    ok(SessionStore.isTabStateNeedsRestore(browser), "tab needs restoring");

    let state = JSON.parse(ss.getTabState(tab));
    let formdata = state.entries[0].formdata;
    is(formdata && formdata.id["foo"], "bar", "tab state's formdata is valid");

    whenTabRestored(tab, function () {
      let input = browser.contentDocument.getElementById("foo");
      is(input.value, "bar", "formdata has been restored correctly");
      finish();
    });

    // Restore the tab by selecting it.
    gBrowser.selectedTab = tab;
  });
}

function whenBrowserLoaded(aBrowser, aCallback) {
  aBrowser.addEventListener("load", function onLoad() {
    aBrowser.removeEventListener("load", onLoad, true);
    executeSoon(aCallback);
  }, true);
}

function whenTabRestored(aTab, aCallback) {
  aTab.addEventListener("SSTabRestored", function onRestored() {
    aTab.removeEventListener("SSTabRestored", onRestored);
    executeSoon(aCallback);
  });
}
