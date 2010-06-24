var tabs;

function index(tab) Array.indexOf(gBrowser.tabs, tab);

function indexTest(tab, expectedIndex, msg) {
  var diag = "tab " + tab + " should be at index " + expectedIndex;
  if (msg)
    msg = msg + " (" + diag + ")";
  else
    msg = diag;
  is(index(tabs[tab]), expectedIndex, msg);
}

function test() {
  tabs = [gBrowser.selectedTab, gBrowser.addTab(), gBrowser.addTab(), gBrowser.addTab()];
  indexTest(0, 0);
  indexTest(1, 1);
  indexTest(2, 2);
  indexTest(3, 3);

  gBrowser.pinTab(tabs[3]);
  indexTest(0, 1);
  indexTest(1, 2);
  indexTest(2, 3);
  indexTest(3, 0);

  gBrowser.pinTab(tabs[1]);
  indexTest(0, 2);
  indexTest(1, 1);
  indexTest(2, 3);
  indexTest(3, 0);

  gBrowser.moveTabTo(tabs[3], 3);
  indexTest(3, 1, "shouldn't be able to mix a pinned tab into normal tabs");

  gBrowser.moveTabTo(tabs[2], 0);
  indexTest(2, 2, "shouldn't be able to mix a normal tab into pinned tabs");

  gBrowser.unpinTab(tabs[1]);
  indexTest(1, 1, "unpinning a tab should move a tab to the start of normal tabs");

  gBrowser.unpinTab(tabs[3]);
  indexTest(3, 0, "unpinning a tab should move a tab to the start of normal tabs");

  gBrowser.removeTab(tabs[1]);
  gBrowser.removeTab(tabs[2]);
  gBrowser.removeTab(tabs[3]);
}
