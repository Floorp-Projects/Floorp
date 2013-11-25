function test() {
  waitForExplicitFinish();

  var tab1 = gBrowser.addTab("data:text/plain;charset=utf-8,foo");
  gBrowser.pinTab(tab1);

  whenBrowserLoaded(tab1.linkedBrowser, function() {
    var tab2 = gBrowser.addTab();
    gBrowser.pinTab(tab2);

    is(Array.indexOf(gBrowser.tabs, tab1), 0, "pinned tab 1 is at the first position");
    gBrowser.removeTab(tab1);
    tab1 = undoCloseTab();
    ok(tab1.pinned, "pinned tab 1 has been restored as a pinned tab");
    is(Array.indexOf(gBrowser.tabs, tab1), 0, "pinned tab 1 has been restored to the first position");

    gBrowser.removeTab(tab1);
    gBrowser.removeTab(tab2);
    finish();
  });
}
