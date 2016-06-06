"use strict";

function test() {
  const isLinux = navigator.platform.indexOf("Linux") == 0;

  function assertTab(expectedTab) {
    is(gBrowser.tabContainer.selectedIndex, expectedTab,
       `tab index ${expectedTab} should be selected`);
  }

  function sendAccelKey(key) {
    // Make sure the keystroke goes to chrome.
    document.activeElement.blur();
    EventUtils.synthesizeKey(key.toString(), { altKey: isLinux, accelKey: !isLinux });
  }

  function createTabs(count) {
    for (let n = 0; n < count; n++)
      gBrowser.addTab();
  }

  function testKey(key, expectedTab) {
    sendAccelKey(key);
    assertTab(expectedTab);
  }

  function testIndex(index, expectedTab) {
    gBrowser.selectTabAtIndex(index);
    assertTab(expectedTab);
  }

  // Create fewer tabs than our 9 number keys.
  is(gBrowser.tabs.length, 1, "should have 1 tab");
  createTabs(4);
  is(gBrowser.tabs.length, 5, "should have 5 tabs");

  // Test keyboard shortcuts. Order tests so that no two test cases have the
  // same expected tab in a row. This ensures that tab selection actually
  // changed the selected tab.
  testKey(8, 4);
  testKey(1, 0);
  testKey(2, 1);
  testKey(4, 3);
  testKey(9, 4);

  // Test index selection.
  testIndex(0, 0);
  testIndex(4, 4);
  testIndex(-5, 0);
  testIndex(5, 4);
  testIndex(-4, 1);
  testIndex(1, 1);
  testIndex(-1, 4);
  testIndex(9, 4);

  // Create more tabs than our 9 number keys.
  createTabs(10);
  is(gBrowser.tabs.length, 15, "should have 15 tabs");

  // Test keyboard shortcuts.
  testKey(2, 1);
  testKey(1, 0);
  testKey(4, 3);
  testKey(8, 7);
  testKey(9, 14);

  // Test index selection.
  testIndex(-15, 0);
  testIndex(14, 14);
  testIndex(-14, 1);
  testIndex(15, 14);
  testIndex(-1, 14);
  testIndex(0, 0);
  testIndex(1, 1);
  testIndex(9, 9);

  // Clean up tabs.
  for (let n = 15; n > 1; n--)
    gBrowser.removeTab(gBrowser.selectedTab, {skipPermitUnload: true});
  is(gBrowser.tabs.length, 1, "should have 1 tab");
}
