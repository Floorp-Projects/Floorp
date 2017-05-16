/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  let initialTabsLength = gBrowser.tabs.length;

  let newTab1 = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:robots", {skipAnimation: true});
  let newTab2 = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:about", {skipAnimation: true});
  let newTab3 = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:config", {skipAnimation: true});
  registerCleanupFunction(function() {
    while (gBrowser.tabs.length > initialTabsLength) {
      gBrowser.removeTab(gBrowser.tabs[initialTabsLength]);
    }
  });

  is(gBrowser.tabs.length, initialTabsLength + 3, "new tabs are opened");
  is(gBrowser.tabs[initialTabsLength], newTab1, "newTab1 position is correct");
  is(gBrowser.tabs[initialTabsLength + 1], newTab2, "newTab2 position is correct");
  is(gBrowser.tabs[initialTabsLength + 2], newTab3, "newTab3 position is correct");

  let scriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"].
                     getService(Ci.mozIJSSubScriptLoader);
  let EventUtils = {};
  scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/EventUtils.js", EventUtils);

  async function dragAndDrop(tab1, tab2, copy) {
    let rect = tab2.getBoundingClientRect();
    let event = {
      ctrlKey: copy,
      altKey: copy,
      clientX: rect.left + rect.width / 2 + 10,
      clientY: rect.top + rect.height / 2,
    };

    let originalTPos = tab1._tPos;
    EventUtils.synthesizeDrop(tab1, tab2, null, copy ? "copy" : "move", window, window, event);
    if (!copy) {
      await BrowserTestUtils.waitForCondition(() => tab1._tPos != originalTPos,
        "Waiting for tab position to be updated");
    }
  }

  await dragAndDrop(newTab1, newTab2, false);
  is(gBrowser.tabs.length, initialTabsLength + 3, "tabs are still there");
  is(gBrowser.tabs[initialTabsLength], newTab2, "newTab2 and newTab1 are swapped");
  is(gBrowser.tabs[initialTabsLength + 1], newTab1, "newTab1 and newTab2 are swapped");
  is(gBrowser.tabs[initialTabsLength + 2], newTab3, "newTab3 stays same place");

  await dragAndDrop(newTab2, newTab1, true);
  is(gBrowser.tabs.length, initialTabsLength + 4, "a tab is duplicated");
  is(gBrowser.tabs[initialTabsLength], newTab2, "newTab2 stays same place");
  is(gBrowser.tabs[initialTabsLength + 1], newTab1, "newTab1 stays same place");
  is(gBrowser.tabs[initialTabsLength + 3], newTab3, "a new tab is inserted before newTab3");
});
