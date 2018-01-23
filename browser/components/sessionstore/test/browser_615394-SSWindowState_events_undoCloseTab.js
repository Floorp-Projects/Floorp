"use strict";

const testState = {
  windows: [{
    tabs: [
      { entries: [{ url: "about:blank", triggeringPrincipal_base64 }] },
      { entries: [{ url: "about:rights", triggeringPrincipal_base64 }] }
    ]
  }]
};

// Test for Bug 615394 - Session Restore should notify when it is beginning and
// ending a restore.
add_task(async function test_undoCloseTab() {
  await promiseBrowserState(testState);

  let tab = gBrowser.tabs[1];
  let busyEventCount = 0;
  let readyEventCount = 0;
  // This will be set inside the `onSSWindowStateReady` method.
  let lastTab;

  ss.setTabValue(tab, "foo", "bar");

  function onSSWindowStateBusy(aEvent) {
    busyEventCount++;
  }

  function onSSWindowStateReady(aEvent) {
    Assert.equal(gBrowser.tabs.length, 2, "Should only have 2 tabs");
    lastTab = gBrowser.tabs[1];
    readyEventCount++;
    Assert.equal(ss.getTabValue(lastTab, "foo"), "bar");
    ss.setTabValue(lastTab, "baz", "qux");
  }

  window.addEventListener("SSWindowStateBusy", onSSWindowStateBusy);
  window.addEventListener("SSWindowStateReady", onSSWindowStateReady);

  let restoredPromise = BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "SSTabRestored");

  await BrowserTestUtils.removeTab(tab);
  let reopenedTab = ss.undoCloseTab(window, 0);

  await Promise.all([restoredPromise, BrowserTestUtils.browserLoaded(reopenedTab.linkedBrowser)]);

  Assert.equal(reopenedTab, lastTab, "Tabs should be the same one.");
  Assert.equal(busyEventCount, 1);
  Assert.equal(readyEventCount, 1);
  Assert.equal(ss.getTabValue(reopenedTab, "baz"), "qux");
  Assert.equal(reopenedTab.linkedBrowser.currentURI.spec, "about:rights");

  window.removeEventListener("SSWindowStateBusy", onSSWindowStateBusy);
  window.removeEventListener("SSWindowStateReady", onSSWindowStateReady);

  await BrowserTestUtils.removeTab(reopenedTab);
});
