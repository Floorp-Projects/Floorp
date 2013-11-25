function test() {
  /** Test for Bug 393716 **/

  waitForExplicitFinish();

  /////////////////
  // getTabState //
  /////////////////
  let key = "Unique key: " + Date.now();
  let value = "Unique value: " + Math.random();
  let testURL = "about:config";

  // create a new tab
  let tab = gBrowser.addTab(testURL);
  ss.setTabValue(tab, key, value);
  whenBrowserLoaded(tab.linkedBrowser, function() {
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
    gBrowser.removeTab(tab);
  });

  //////////////////////////////////
  // setTabState and duplicateTab //
  //////////////////////////////////
  let key2 = "key2";
  let value2 = "Value " + Math.random();
  let value3 = "Another value: " + Date.now();
  let state = { entries: [{ url: testURL }], extData: { key2: value2 } };

  // create a new tab
  let tab2 = gBrowser.addTab();
  // set the tab's state
  ss.setTabState(tab2, JSON.stringify(state));
  whenTabRestored(tab2, function() {
    // verify the correctness of the restored tab
    ok(ss.getTabValue(tab2, key2) == value2 && tab2.linkedBrowser.currentURI.spec == testURL,
       "the tab's state was correctly restored");

    // add text data
    let textbox = tab2.linkedBrowser.contentDocument.getElementById("textbox");
    textbox.value = value3;

    // duplicate the tab
    let duplicateTab = ss.duplicateTab(window, tab2);
    gBrowser.removeTab(tab2);

    whenTabRestored(duplicateTab, function() {
      // verify the correctness of the duplicated tab
      ok(ss.getTabValue(duplicateTab, key2) == value2 &&
         duplicateTab.linkedBrowser.currentURI.spec == testURL,
         "correctly duplicated the tab's state");
      let textbox = duplicateTab.linkedBrowser.contentDocument.getElementById("textbox");
      is(textbox.value, value3, "also duplicated text data");

      // clean up
      gBrowser.removeTab(duplicateTab);
      finish();
    });
  });
}
