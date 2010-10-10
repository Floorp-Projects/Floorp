function test() {
  /** Test for Bug 393716 **/
  
  // set up the basics (SessionStore service, tabbrowser)
  try {
    var ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);
  }
  catch (ex) { }
  ok(ss, "SessionStore service is available");
  let tabbrowser = gBrowser;
  waitForExplicitFinish();
  
  /////////////////
  // getTabState //
  /////////////////
  let key = "Unique key: " + Date.now();
  let value = "Unique value: " + Math.random();
  let testURL = "about:config";
  
  // create a new tab
  let tab = tabbrowser.addTab(testURL);
  ss.setTabValue(tab, key, value);
  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    this.removeEventListener("load", arguments.callee, true);
    // get the tab's state
    let state = ss.getTabState(tab);
    ok(state, "get the tab's state");
    
    // verify the tab state's integrity
    state = JSON.parse(state);
    ok(state instanceof Object && state.entries instanceof Array && state.entries.length > 0,
       "state object seems valid");
    ok(state.entries.length == 1 && state.entries[0].url == testURL,
       "Got the expected state object (test URL)");
    ok(state.extData && state.extData[key] == value,
       "Got the expected state object (test manually set tab value)");
    
    // clean up
    tabbrowser.removeTab(tab);
  }, true);
  
  //////////////////////////////////
  // setTabState and duplicateTab //
  //////////////////////////////////
  let key2 = "key2";
  let value2 = "Value " + Math.random();
  let value3 = "Another value: " + Date.now();
  let state = { entries: [{ url: testURL }], extData: { key2: value2 } };
  
  // create a new tab
  let tab2 = tabbrowser.addTab();
  // set the tab's state
  ss.setTabState(tab2, JSON.stringify(state));
  tab2.linkedBrowser.addEventListener("load", function(aEvent) {
    this.removeEventListener("load", arguments.callee, true);
    // verify the correctness of the restored tab
    ok(ss.getTabValue(tab2, key2) == value2 && this.currentURI.spec == testURL,
       "the tab's state was correctly restored");
    
    // add text data
    let textbox = this.contentDocument.getElementById("textbox");
    textbox.value = value3;
    
    // duplicate the tab
    let duplicateTab = ss.duplicateTab(window, tab2);
    tabbrowser.removeTab(tab2);
    
    duplicateTab.linkedBrowser.addEventListener("load", function(aEvent) {
      this.removeEventListener("load", arguments.callee, true);
      // verify the correctness of the duplicated tab
      ok(ss.getTabValue(duplicateTab, key2) == value2 && this.currentURI.spec == testURL,
         "correctly duplicated the tab's state");
      let textbox = this.contentDocument.getElementById("textbox");
      is(textbox.value, value3, "also duplicated text data");
      
      // clean up
      tabbrowser.removeTab(duplicateTab);
      finish();
    }, true);
  }, true);
}
