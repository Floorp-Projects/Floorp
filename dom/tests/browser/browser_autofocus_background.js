function test() {
  waitForExplicitFinish();

  let fm = Components.classes["@mozilla.org/focus-manager;1"]
                     .getService(Components.interfaces.nsIFocusManager);

  let tabs = [ gBrowser.selectedTab, gBrowser.addTab(), gBrowser.addTab() ];

  // The first tab has an autofocused element.
  // The second tab is exactly like the first one without the autofocus.
  let testingList = [
    { uri: "data:text/html,<!DOCTYPE html><html><body><input autofocus id='target'></body></html>",
      tagName: "INPUT"},
    { uri: "data:text/html,<!DOCTYPE html><html><body><input id='target'></body></html>",
      tagName: "BODY"}
  ];

  function runTest() {
    // Set the focus to the first tab.
    tabs[0].linkedBrowser.focus();

    // Load the first tab on background.
    tabs[1].linkedBrowser.addEventListener("load", onLoadBackgroundFirstTab, true);
    tabs[1].linkedBrowser.loadURI(testingList[0].uri);
  }

  function onLoadBackgroundFirstTab() {
    tabs[1].linkedBrowser.removeEventListener("load", onLoadBackgroundFirstTab, true);

    // Now load the second tab on background.
    tabs[2].linkedBrowser.addEventListener("load", onLoadBackgroundSecondTab, true);
    tabs[2].linkedBrowser.loadURI(testingList[1].uri);
  }

  function onLoadBackgroundSecondTab() {
    tabs[2].linkedBrowser.removeEventListener("load", onLoadBackgroundSecondTab, true);

    // Wait a second to be sure all focus events are done before launching tests.
    setTimeout(doTest, 1000);
  }

  function doTest() {
    for (var i=0; i<testingList.length; ++i) {
      // Get active element in the tab.
      var e = tabs[i+1].linkedBrowser.contentDocument.activeElement;

      is(e.tagName, testingList[i].tagName,
         "The background tab's focused element should be " +
         testingList[i].tagName);
      isnot(fm.focusedElement, e,
            "The background tab's focused element should not be the focus " +
            "manager focused element");
    }

    // Cleaning up.
    gBrowser.addTab();
    for (let i = 0; i < tabs.length; i++) {
      gBrowser.removeTab(tabs[i]);
    }
    finish();
  }

  runTest();
}
