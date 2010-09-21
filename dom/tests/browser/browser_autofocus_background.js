function test() {
  waitForExplicitFinish();

  let fm = Components.classes["@mozilla.org/focus-manager;1"]
                     .getService(Components.interfaces.nsIFocusManager);

  let tabs = [ gBrowser.selectedTab, gBrowser.addTab() ];

  // The first tab has an autofocused element.
  // The second tab is exactly like the first one without the autofocus.
  let testingList = [
    { uri: "data:text/html,<!DOCTYPE html><html><body><input autofocus id='target'></body></html>",
      tagName: "INPUT"},
  ];

  function runTest() {
    // Set the focus to the first tab.
    tabs[0].linkedBrowser.focus();

    // Load the first tab on background.
    tabs[1].linkedBrowser.addEventListener("load", onLoadBackgroundTab, true);
    tabs[1].linkedBrowser.loadURI(testingList[0].uri);
  }

  function onLoadBackgroundTab() {
    tabs[1].linkedBrowser.removeEventListener("load", onLoadBackgroundTab, true);

    // The focus event (from autofocus) has been sent,
    // let's test with executeSoon so we are sure the test is done
    // after the focus event is processed.
    executeSoon(doTest);
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
    for (let i = 1; i < tabs.length; i++) {
      gBrowser.removeTab(tabs[i]);
    }
    finish();
  }

  runTest();
}
