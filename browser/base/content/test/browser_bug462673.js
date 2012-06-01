var runs = [
  function (win, tabbrowser, tab) {
    is(tabbrowser.browsers.length, 2, "test_bug462673.html has opened a second tab");
    is(tabbrowser.selectedTab, tab.nextSibling, "dependent tab is selected");
    tabbrowser.removeTab(tab);
    ok(win.closed, "Window is closed");
  },
  function (win, tabbrowser, tab) {
    var newTab = tabbrowser.addTab();
    var newBrowser = newTab.linkedBrowser;
    tabbrowser.removeTab(tab);
    ok(!win.closed, "Window stays open");
    if (!win.closed) {
      is(tabbrowser.tabContainer.childElementCount, 1, "Window has one tab");
      is(tabbrowser.browsers.length, 1, "Window has one browser");
      is(tabbrowser.selectedTab, newTab, "Remaining tab is selected");
      is(tabbrowser.selectedBrowser, newBrowser, "Browser for remaining tab is selected");
      is(tabbrowser.mTabBox.selectedPanel, newBrowser.parentNode.parentNode.parentNode, "Panel for remaining tab is selected");
    }
  }
];

function test() {
  waitForExplicitFinish();
  runOneTest();
}

function runOneTest() {
  var win = openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no");

  win.addEventListener("load", function () {
    win.removeEventListener("load", arguments.callee, false);

    var tab = win.gBrowser.tabContainer.firstChild;
    var browser = tab.linkedBrowser;

    browser.addEventListener("load", function () {
      browser.removeEventListener("load", arguments.callee, true);

      executeSoon(function () {
        runs.shift()(win, win.gBrowser, tab);
        win.close();
        if (runs.length)
          runOneTest();
        else
          finish();
      });
    }, true);

    var rootDir = getRootDirectory(gTestPath);
    browser.contentWindow.location = rootDir + "test_bug462673.html"
  }, false);
}
