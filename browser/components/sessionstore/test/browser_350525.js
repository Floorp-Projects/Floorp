"use strict";

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.ipc.processCount", 1]]
  });
});

add_task(async function() {
  /** Test for Bug 350525 **/

  function test(aLambda) {
    try {
      return aLambda() || true;
    } catch (ex) { }
    return false;
  }

  /**
   * setWindowValue, et al.
   */
  let key = "Unique name: " + Date.now();
  let value = "Unique value: " + Math.random();

  // test adding
  ok(test(() => ss.setWindowValue(window, key, value)), "set a window value");

  // test retrieving
  is(ss.getWindowValue(window, key), value, "stored window value matches original");

  // test deleting
  ok(test(() => ss.deleteWindowValue(window, key)), "delete the window value");

  // value should not exist post-delete
  is(ss.getWindowValue(window, key), "", "window value was deleted");

  // test deleting a non-existent value
  ok(test(() => ss.deleteWindowValue(window, key)), "delete non-existent window value");

  /**
   * setTabValue, et al.
   */
  key = "Unique name: " + Math.random();
  value = "Unique value: " + Date.now();
  let tab = BrowserTestUtils.addTab(gBrowser);
  tab.linkedBrowser.stop();

  // test adding
  ok(test(() => ss.setTabValue(tab, key, value)), "store a tab value");

  // test retrieving
  is(ss.getTabValue(tab, key), value, "stored tab value match original");

  // test deleting
  ok(test(() => ss.deleteTabValue(tab, key)), "delete the tab value");

  // value should not exist post-delete
  is(ss.getTabValue(tab, key), "", "tab value was deleted");

  // test deleting a non-existent value
  ok(test(() => ss.deleteTabValue(tab, key)), "delete non-existent tab value");

  // clean up
  await promiseRemoveTab(tab);

  /**
   * getClosedTabCount, undoCloseTab
   */

  // get closed tab count
  let count = ss.getClosedTabCount(window);
  let max_tabs_undo = Services.prefs.getIntPref("browser.sessionstore.max_tabs_undo");
  ok(0 <= count && count <= max_tabs_undo,
     "getClosedTabCount returns zero or at most max_tabs_undo");

  // create a new tab
  let testURL = "about:mozilla";
  tab = BrowserTestUtils.addTab(gBrowser, testURL);
  await promiseBrowserLoaded(tab.linkedBrowser);

  // make sure that the next closed tab will increase getClosedTabCount
  Services.prefs.setIntPref("browser.sessionstore.max_tabs_undo", max_tabs_undo + 1);
  registerCleanupFunction(() => Services.prefs.clearUserPref("browser.sessionstore.max_tabs_undo"));

  // remove tab
  await promiseRemoveTab(tab);

  // getClosedTabCount
  let newcount = ss.getClosedTabCount(window);
  ok(newcount > count, "after closing a tab, getClosedTabCount has been incremented");

  // undoCloseTab
  tab = test(() => ss.undoCloseTab(window, 0));
  ok(tab, "undoCloseTab doesn't throw");

  await promiseTabRestored(tab);
  is(tab.linkedBrowser.currentURI.spec, testURL, "correct tab was reopened");

  // clean up
  gBrowser.removeTab(tab);
});
