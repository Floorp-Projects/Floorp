/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function test() {
  waitForExplicitFinish();
  let deferred = Promise.defer();
  gBrowser.selectedTab = gBrowser.addTab("about:blank");
  openPreferences("paneContent");
  let newTabBrowser = gBrowser.getBrowserForTab(gBrowser.selectedTab);

  newTabBrowser.addEventListener("Initialized", function PrefInit() {
    newTabBrowser.removeEventListener("Initialized", PrefInit, true);
    newTabBrowser.contentWindow.addEventListener("load", function prefLoad() {
      newTabBrowser.contentWindow.removeEventListener("load", prefLoad);
      let sel = gBrowser.contentWindow.history.state;
      is(sel, "paneContent", "Content pane was selected");
      deferred.resolve();
      gBrowser.removeCurrentTab();
    });
  }, true);

  yield deferred.promise;

  finish();
});
