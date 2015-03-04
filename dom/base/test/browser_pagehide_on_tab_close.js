function test() {
  waitForExplicitFinish();

  var tab = gBrowser.addTab();
  gBrowser.selectedTab = tab;

  tab.linkedBrowser.addEventListener("load", function onload() {
    tab.linkedBrowser.removeEventListener("load", onload);

    tab.linkedBrowser.addEventListener("pagehide", function() {
      ok(true, "got page hide event");
      finish();
    });

    executeSoon(() => { gBrowser.removeTab(tab); });
  }, true);
}
