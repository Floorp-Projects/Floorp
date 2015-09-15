"use strict";

var uniqueName = "bug 465215";
var uniqueValue1 = "as good as unique: " + Date.now();
var uniqueValue2 = "as good as unique: " + Math.random();

add_task(function* () {
  // set a unique value on a new, blank tab
  let tab1 = gBrowser.addTab("about:blank");
  yield promiseBrowserLoaded(tab1.linkedBrowser);
  ss.setTabValue(tab1, uniqueName, uniqueValue1);

  // duplicate the tab with that value
  let tab2 = ss.duplicateTab(window, tab1);
  yield promiseTabRestored(tab2);
  is(ss.getTabValue(tab2, uniqueName), uniqueValue1, "tab value was duplicated");

  ss.setTabValue(tab2, uniqueName, uniqueValue2);
  isnot(ss.getTabValue(tab1, uniqueName), uniqueValue2, "tab values aren't sync'd");

  // overwrite the tab with the value which should remove it
  yield promiseTabState(tab1, {entries: []});
  is(ss.getTabValue(tab1, uniqueName), "", "tab value was cleared");

  // clean up
  gBrowser.removeTab(tab2);
  gBrowser.removeTab(tab1);
});
