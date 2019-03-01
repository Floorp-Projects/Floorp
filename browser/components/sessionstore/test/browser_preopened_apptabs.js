/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PREF_RESTORE_ON_DEMAND = "browser.sessionstore.restore_on_demand";
const PREF_NUM_PINNED_TABS = "browser.tabs.firstWindowRestore.numPinnedTabs";

function muffleMainWindowType() {
  let oldWinType = document.documentElement.getAttribute("windowtype");
  // Check if we've already done this to allow calling multiple times:
  if (oldWinType != "navigator:testrunner") {
    // Make the main test window not count as a browser window any longer
    document.documentElement.setAttribute("windowtype", "navigator:testrunner");

    registerCleanupFunction(() => {
      document.documentElement.setAttribute("windowtype", oldWinType);
    });
  }
}

registerCleanupFunction(function() {
  Services.prefs.clearUserPref(PREF_NUM_PINNED_TABS);
});

let testCases = [
  {numPinnedPref: 5, selected: 3, overrideTabs: false},
  {numPinnedPref: 5, selected: 3, overrideTabs: true},
  {numPinnedPref: 5, selected: 1, overrideTabs: false},
  {numPinnedPref: 5, selected: 1, overrideTabs: true},
  {numPinnedPref: 3, selected: 3, overrideTabs: false},
  {numPinnedPref: 3, selected: 3, overrideTabs: true},
  {numPinnedPref: 3, selected: 1, overrideTabs: false},
  {numPinnedPref: 3, selected: 1, overrideTabs: true},
  {numPinnedPref: 1, selected: 3, overrideTabs: false},
  {numPinnedPref: 1, selected: 3, overrideTabs: true},
  {numPinnedPref: 1, selected: 1, overrideTabs: false},
  {numPinnedPref: 1, selected: 1, overrideTabs: true},
  {numPinnedPref: 0, selected: 3, overrideTabs: false},
  {numPinnedPref: 0, selected: 3, overrideTabs: true},
  {numPinnedPref: 0, selected: 1, overrideTabs: false},
  {numPinnedPref: 0, selected: 1, overrideTabs: true},
];

for (let {numPinnedPref, selected, overrideTabs} of testCases) {
  add_task(async function testPrefSynced() {
    Services.prefs.setIntPref(PREF_NUM_PINNED_TABS, numPinnedPref);

    let state = { windows: [{ tabs: [
      { entries: [{ url: "http://example.org/#1", triggeringPrincipal_base64 }], pinned: true, userContextId: 0 },
      { entries: [{ url: "http://example.org/#2", triggeringPrincipal_base64 }], pinned: true, userContextId: 0 },
      { entries: [{ url: "http://example.org/#3", triggeringPrincipal_base64 }], pinned: true, userContextId: 0 },
    ], selected }] };

    muffleMainWindowType();
    let win = await BrowserTestUtils.openNewBrowserWindow();
    Assert.equal([...win.gBrowser.tabs].filter(t => t.pinned).length, numPinnedPref);
    Assert.equal([...win.gBrowser.tabs].filter(t => !!t.linkedPanel).length, 1);
    await setWindowState(win, state, overrideTabs, true);
    Assert.equal([...win.gBrowser.tabs].filter(t => t.pinned).length, 3);
    Assert.equal([...win.gBrowser.tabs].filter(t => !!t.linkedPanel).length,
                 overrideTabs ? 3 : 4);
    await BrowserTestUtils.closeWindow(win);
  });
}
