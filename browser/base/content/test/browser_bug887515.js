function numClosedTabs()
  Cc["@mozilla.org/browser/sessionstore;1"].
    getService(Ci.nsISessionStore).
    getNumberOfTabsClosedLast(window);

var originalTab;
var tab1Loaded = false;
var tab2Loaded = false;

function verifyUndoMultipleClose() {
  if (!tab1Loaded || !tab2Loaded)
    return;

  gBrowser.removeAllTabsBut(originalTab);
  updateTabContextMenu();
  let undoCloseTabElement = document.getElementById("context_undoCloseTab");
  ok(!undoCloseTabElement.disabled, "Undo Close Tabs should be enabled.");
  is(numClosedTabs(), 2, "There should be 2 closed tabs.");
  is(gBrowser.tabs.length, 1, "There should only be 1 open tab");
  updateTabContextMenu();
  is(undoCloseTabElement.label, undoCloseTabElement.getAttribute("multipletablabel"),
     "The label should be showing that the command will restore multiple tabs");
  undoCloseTab();

  is(gBrowser.tabs.length, 3, "There should be 3 open tabs");
  updateTabContextMenu();
  is(undoCloseTabElement.label, undoCloseTabElement.getAttribute("singletablabel"),
     "The label should be showing that the command will restore a single tab");

  gBrowser.removeTabsToTheEndFrom(originalTab);
  updateTabContextMenu();
  ok(!undoCloseTabElement.disabled, "Undo Close Tabs should be enabled.");
  is(numClosedTabs(), 2, "There should be 2 closed tabs.");
  is(gBrowser.tabs.length, 1, "There should only be 1 open tab");
  updateTabContextMenu();
  is(undoCloseTabElement.label, undoCloseTabElement.getAttribute("multipletablabel"),
     "The label should be showing that the command will restore multiple tabs");

  finish();
}

function test() {
  waitForExplicitFinish();

  registerCleanupFunction(function() {
    originalTab.linkedBrowser.loadURI("about:blank");
    originalTab = null;
  });

  let undoCloseTabElement = document.getElementById("context_undoCloseTab");
  updateTabContextMenu();
  is(undoCloseTabElement.label, undoCloseTabElement.getAttribute("singletablabel"),
     "The label should be showing that the command will restore a single tab");

  originalTab = gBrowser.selectedTab;
  gBrowser.selectedBrowser.loadURI("http://mochi.test:8888/");
  var tab1 = gBrowser.addTab("http://mochi.test:8888/");
  var tab2 = gBrowser.addTab("http://mochi.test:8888/");
  var browser1 = gBrowser.getBrowserForTab(tab1);
  browser1.addEventListener("load", function onLoad1() {
    browser1.removeEventListener("load", onLoad1, true);
    tab1Loaded = true;
    tab1 = null;

    verifyUndoMultipleClose();
  }, true);
  var browser2 = gBrowser.getBrowserForTab(tab2);
  browser2.addEventListener("load", function onLoad2() {
    browser2.removeEventListener("load", onLoad2, true);
    tab2Loaded = true;
    tab2 = null;

    verifyUndoMultipleClose();
  }, true);
}
