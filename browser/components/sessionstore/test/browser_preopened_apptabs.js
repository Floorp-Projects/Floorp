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

add_task(async function testPrefSynced() {
  Services.prefs.setIntPref(PREF_NUM_PINNED_TABS, 3);

  let state = { windows: [{ tabs: [
    { entries: [{ url: "http://example.org/#1", triggeringPrincipal_base64 }], pinned: true },
    { entries: [{ url: "http://example.org/#2", triggeringPrincipal_base64 }], pinned: true },
    { entries: [{ url: "http://example.org/#3", triggeringPrincipal_base64 }], pinned: true },
  ], selected: 3 }] };

  muffleMainWindowType();
  let win = await BrowserTestUtils.openNewBrowserWindow();
  Assert.equal([...win.gBrowser.tabs].filter(t => t.pinned).length, 3);
  Assert.equal([...win.gBrowser.tabs].filter(t => !!t.linkedPanel).length, 1);
  await setWindowState(win, state, false, true);
  Assert.equal([...win.gBrowser.tabs].filter(t => t.pinned).length, 3);
  Assert.equal([...win.gBrowser.tabs].filter(t => !!t.linkedPanel).length, 4);
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function testPrefSyncedSelected() {
  Services.prefs.setIntPref(PREF_NUM_PINNED_TABS, 3);

  let state = { windows: [{ tabs: [
    { entries: [{ url: "http://example.org/#1", triggeringPrincipal_base64 }], pinned: true },
    { entries: [{ url: "http://example.org/#2", triggeringPrincipal_base64 }], pinned: true },
    { entries: [{ url: "http://example.org/#3", triggeringPrincipal_base64 }], pinned: true },
  ], selected: 1 }] };

  muffleMainWindowType();
  let win = await BrowserTestUtils.openNewBrowserWindow();
  Assert.equal([...win.gBrowser.tabs].filter(t => t.pinned).length, 3);
  Assert.equal([...win.gBrowser.tabs].filter(t => !!t.linkedPanel).length, 1);
  await setWindowState(win, state, false, true);
  Assert.equal([...win.gBrowser.tabs].filter(t => t.pinned).length, 3);
  Assert.equal([...win.gBrowser.tabs].filter(t => !!t.linkedPanel).length, 4);
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function testPrefTooHigh() {
  Services.prefs.setIntPref(PREF_NUM_PINNED_TABS, 5);

  let state = { windows: [{ tabs: [
    { entries: [{ url: "http://example.org/#1", triggeringPrincipal_base64 }], pinned: true },
    { entries: [{ url: "http://example.org/#2", triggeringPrincipal_base64 }], pinned: true },
    { entries: [{ url: "http://example.org/#3", triggeringPrincipal_base64 }], pinned: true },
  ], selected: 3 }] };

  muffleMainWindowType();
  let win = await BrowserTestUtils.openNewBrowserWindow();
  Assert.equal([...win.gBrowser.tabs].filter(t => t.pinned).length, 5);
  Assert.equal([...win.gBrowser.tabs].filter(t => !!t.linkedPanel).length, 1);
  await setWindowState(win, state, false, true);
  Assert.equal([...win.gBrowser.tabs].filter(t => t.pinned).length, 3);
  Assert.equal([...win.gBrowser.tabs].filter(t => !!t.linkedPanel).length, 4);
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function testPrefTooLow() {
  Services.prefs.setIntPref(PREF_NUM_PINNED_TABS, 1);

  let state = { windows: [{ tabs: [
    { entries: [{ url: "http://example.org/#1", triggeringPrincipal_base64 }], pinned: true },
    { entries: [{ url: "http://example.org/#2", triggeringPrincipal_base64 }], pinned: true },
    { entries: [{ url: "http://example.org/#3", triggeringPrincipal_base64 }], pinned: true },
  ], selected: 3 }] };

  muffleMainWindowType();
  let win = await BrowserTestUtils.openNewBrowserWindow();
  Assert.equal([...win.gBrowser.tabs].filter(t => t.pinned).length, 1);
  Assert.equal([...win.gBrowser.tabs].filter(t => !!t.linkedPanel).length, 1);
  await setWindowState(win, state, false, true);
  Assert.equal([...win.gBrowser.tabs].filter(t => t.pinned).length, 3);
  Assert.equal([...win.gBrowser.tabs].filter(t => !!t.linkedPanel).length, 4);
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function testPrefTooHighSelected() {
  Services.prefs.setIntPref(PREF_NUM_PINNED_TABS, 5);

  let state = { windows: [{ tabs: [
    { entries: [{ url: "http://example.org/#1", triggeringPrincipal_base64 }], pinned: true },
    { entries: [{ url: "http://example.org/#2", triggeringPrincipal_base64 }], pinned: true },
    { entries: [{ url: "http://example.org/#3", triggeringPrincipal_base64 }], pinned: true },
  ], selected: 1 }] };

  muffleMainWindowType();
  let win = await BrowserTestUtils.openNewBrowserWindow();
  Assert.equal([...win.gBrowser.tabs].filter(t => t.pinned).length, 5);
  Assert.equal([...win.gBrowser.tabs].filter(t => !!t.linkedPanel).length, 1);
  await setWindowState(win, state, false, true);
  Assert.equal([...win.gBrowser.tabs].filter(t => t.pinned).length, 3);
  Assert.equal([...win.gBrowser.tabs].filter(t => !!t.linkedPanel).length, 4);
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function testPrefTooLowSelected() {
  Services.prefs.setIntPref(PREF_NUM_PINNED_TABS, 1);

  let state = { windows: [{ tabs: [
    { entries: [{ url: "http://example.org/#1", triggeringPrincipal_base64 }], pinned: true },
    { entries: [{ url: "http://example.org/#2", triggeringPrincipal_base64 }], pinned: true },
    { entries: [{ url: "http://example.org/#3", triggeringPrincipal_base64 }], pinned: true },
  ], selected: 1 }] };

  muffleMainWindowType();
  let win = await BrowserTestUtils.openNewBrowserWindow();
  Assert.equal([...win.gBrowser.tabs].filter(t => t.pinned).length, 1);
  Assert.equal([...win.gBrowser.tabs].filter(t => !!t.linkedPanel).length, 1);
  await setWindowState(win, state, false, true);
  Assert.equal([...win.gBrowser.tabs].filter(t => t.pinned).length, 3);
  Assert.equal([...win.gBrowser.tabs].filter(t => !!t.linkedPanel).length, 4);
  await BrowserTestUtils.closeWindow(win);
});
