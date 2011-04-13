function test() {
  /** Test for Bug 350525 **/
  
  function test(aLambda) {
    try {
      return aLambda() || true;
    }
    catch (ex) { }
    return false;
  }
  
  // test setup
  let tabbrowser = gBrowser;
  waitForExplicitFinish();
  
  // component
  let ssComponent = test(function() Cc["@mozilla.org/browser/sessionstore;1"]);
  ok(ssComponent, "reference the sessionstore component");
  
  // service
  let ss = test(function() ssComponent.getService(Ci.nsISessionStore));
  ok(ss, "reference the sessionstore service");
  
  ////////////////////////////
  // setWindowValue, et al. //
  ////////////////////////////
  let key = "Unique name: " + Date.now();
  let value = "Unique value: " + Math.random();
  
  // test adding
  ok(test(function() ss.setWindowValue(window, key, value)), "set a window value");
  
  // test retrieving
  is(ss.getWindowValue(window, key), value, "stored window value matches original");
  
  // test deleting 
  ok(test(function() ss.deleteWindowValue(window, key)), "delete the window value");
  
  // value should not exist post-delete
  is(ss.getWindowValue(window, key), "", "window value was deleted");
  
  /////////////////////////
  // setTabValue, et al. //
  /////////////////////////
  key = "Unique name: " + Math.random();
  value = "Unique value: " + Date.now();
  let tab = tabbrowser.addTab();
  tab.linkedBrowser.stop();
  
  // test adding
  ok(test(function() ss.setTabValue(tab, key, value)), "store a tab value");
  
  // test retrieving
  is(ss.getTabValue(tab, key), value, "stored tab value match original");
  
  // test deleting 
  ok(test(function() ss.deleteTabValue(tab, key)), "delete the tab value");
  // value should not exist post-delete
  is(ss.getTabValue(tab, key), "", "tab value was deleted");
  
  // clean up
  tabbrowser.removeTab(tab);
  
  /////////////////////////////////////
  // getClosedTabCount, undoCloseTab //
  /////////////////////////////////////
  
  // get closed tab count
  let count = ss.getClosedTabCount(window);
  let max_tabs_undo = gPrefService.getIntPref("browser.sessionstore.max_tabs_undo");
  ok(0 <= count && count <= max_tabs_undo,
     "getClosedTabCount returns zero or at most max_tabs_undo");
  
  // create a new tab
  let testURL = "about:";
  tab = tabbrowser.addTab(testURL);
  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    this.removeEventListener("load", arguments.callee, true);
    // make sure that the next closed tab will increase getClosedTabCount
    gPrefService.setIntPref("browser.sessionstore.max_tabs_undo", max_tabs_undo + 1);
    
    // remove tab
    tabbrowser.removeTab(tab);
    
    // getClosedTabCount
    var newcount = ss.getClosedTabCount(window);
    ok(newcount > count, "after closing a tab, getClosedTabCount has been incremented");
    
    // undoCloseTab
    tab = test(function() ss.undoCloseTab(window, 0));
    ok(tab, "undoCloseTab doesn't throw")
    
    tab.linkedBrowser.addEventListener("load", function(aEvent) {
      this.removeEventListener("load", arguments.callee, true);
      is(this.currentURI.spec, testURL, "correct tab was reopened");
      
      // clean up
      if (gPrefService.prefHasUserValue("browser.sessionstore.max_tabs_undo"))
        gPrefService.clearUserPref("browser.sessionstore.max_tabs_undo");
      tabbrowser.removeTab(tab);
      finish();
    }, true);
  }, true);
}
