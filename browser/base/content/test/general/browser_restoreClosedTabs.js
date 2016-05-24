/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This test opens a new window for each test part, to ensure we have
// a clean slate each time.

add_task(function*() {
  info("Test Restore All Tabs, including closing the original single blank tab");

  let win = yield BrowserTestUtils.openNewBrowserWindow();
  let gBrowser = win.gBrowser;

  let blankTab = gBrowser.selectedTab;
  info("Opening tabs...");
  let tab1 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/tab1");
  let tab2 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/tab2");

  info("Closing tabs...");
  gBrowser.removeTab(tab1);
  gBrowser.removeTab(tab2);

  info("Opening PanelUI...");
  yield win.PanelUI.show();
  info("Clicking on History button...");
  let historyBtn = win.document.getElementById("history-panelmenu");
  EventUtils.synthesizeMouseAtCenter(historyBtn, {}, win);
  yield BrowserTestUtils.waitForCondition(() => !win.PanelUI.multiView.hasAttribute("transitioning"));

  let newTabPromises = [
    BrowserTestUtils.waitForNewTab(gBrowser, "http://example.com/tab1"),
    BrowserTestUtils.waitForNewTab(gBrowser, "http://example.com/tab2"),
  ];
  let closeTabPromise = BrowserTestUtils.removeTab(blankTab, {dontRemove: true});

  info("Clicking on Restore All Tabs...");
  let restoreAllTabsBtn = win.document.getElementById("PanelUI-recentlyClosedTabs").firstChild;
  EventUtils.synthesizeMouseAtCenter(restoreAllTabsBtn, {}, win);
  let reopenedTabs = yield Promise.all(newTabPromises);
  info("Tabs restored");
  yield closeTabPromise;
  info("Original blank tab closed");

  Assert.equal(gBrowser.tabs.length, 2, "Expect 2 tabs, the blank tab having been closed");

  // Cleanup
  yield BrowserTestUtils.closeWindow(win);
});


add_task(function*() {
  info("Test Restore All Tabs, not closing original tab");

  let win = yield BrowserTestUtils.openNewBrowserWindow();
  let gBrowser = win.gBrowser;

  info("Loading tab to keep");
  let originalTab = gBrowser.selectedTab;
  let originalTabLoad = BrowserTestUtils.browserLoaded(originalTab.linkedBrowser);
  originalTab.linkedBrowser.loadURI("http://example.com/originalTab");
  yield originalTabLoad;

  info("Opening tabs...");
  let tab1 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/tab1");
  let tab2 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/tab2");

  info("Closing tabs...");
  gBrowser.removeTab(tab1);
  gBrowser.removeTab(tab2);

  info("Opening PanelUI...");
  yield win.PanelUI.show();
  info("Clicking on History button...");
  let historyBtn = win.document.getElementById("history-panelmenu");
  EventUtils.synthesizeMouseAtCenter(historyBtn, {}, win);
  yield BrowserTestUtils.waitForCondition(() => !win.PanelUI.multiView.hasAttribute("transitioning"));

  let newTabPromises = [
    BrowserTestUtils.waitForNewTab(gBrowser, "http://example.com/tab1"),
    BrowserTestUtils.waitForNewTab(gBrowser, "http://example.com/tab2"),
  ];

  info("Clicking on Restore All Tabs...");
  let restoreAllTabsBtn = win.document.getElementById("PanelUI-recentlyClosedTabs").firstChild;
  EventUtils.synthesizeMouseAtCenter(restoreAllTabsBtn, {}, win);
  let reopenedTabs = yield Promise.all(newTabPromises);
  info("Tabs restored");

  Assert.equal(gBrowser.tabs.length, 3, "Expect 3 tabs, the original tab was kept open");

  // Cleanup
  yield BrowserTestUtils.closeWindow(win);
});


add_task(function*() {
  info("Test undo closing a single tab via PanelUI, including closing the original single blank tab");

  let win = yield BrowserTestUtils.openNewBrowserWindow();
  let gBrowser = win.gBrowser;

  let blankTab = gBrowser.selectedTab;
  info("Opening tab...");
  let tab1 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/tab1");

  info("Closing tab...");
  gBrowser.removeTab(tab1);

  info("Opening PanelUI...");
  yield win.PanelUI.show();
  info("Clicking on History button...");
  let historyBtn = win.document.getElementById("history-panelmenu");
  EventUtils.synthesizeMouseAtCenter(historyBtn, {}, win);
  yield BrowserTestUtils.waitForCondition(() => !win.PanelUI.multiView.hasAttribute("transitioning"));

  let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, "http://example.com/tab1");
  let closeTabPromise = BrowserTestUtils.removeTab(blankTab, {dontRemove: true});

  info("Clicking on item to restore tab...");
  let restoreTabBtn = win.document.getElementById("PanelUI-recentlyClosedTabs").firstChild.nextSibling;
  EventUtils.synthesizeMouseAtCenter(restoreTabBtn, {}, win);
  let reopenedTab = yield newTabPromise;
  info("Tab restored");
  yield closeTabPromise;
  info("Original blank tab closed");

  Assert.equal(gBrowser.tabs.length, 1, "Expect 1 tab, the blank tab having been closed");

  // Cleanup
  yield BrowserTestUtils.closeWindow(win);
});


add_task(function*() {
  info("Test undo closing a single tab via PanelUI, not closing original tab");

  let win = yield BrowserTestUtils.openNewBrowserWindow();
  let gBrowser = win.gBrowser;

  info("Loading tab to keep");
  let originalTab = gBrowser.selectedTab;
  let originalTabLoad = BrowserTestUtils.browserLoaded(originalTab.linkedBrowser);
  originalTab.linkedBrowser.loadURI("http://example.com/originalTab");
  yield originalTabLoad;

  info("Opening tab...");
  let tab1 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/tab1");

  info("Closing tab...");
  gBrowser.removeTab(tab1);

  info("Opening PanelUI...");
  yield win.PanelUI.show();
  info("Clicking on History button...");
  let historyBtn = win.document.getElementById("history-panelmenu");
  EventUtils.synthesizeMouseAtCenter(historyBtn, {}, win);
  yield BrowserTestUtils.waitForCondition(() => !win.PanelUI.multiView.hasAttribute("transitioning"));

  let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, "http://example.com/tab1");

  info("Clicking on item to restore tab...");
  let restoreTabBtn = win.document.getElementById("PanelUI-recentlyClosedTabs").firstChild.nextSibling;
  EventUtils.synthesizeMouseAtCenter(restoreTabBtn, {}, win);
  let reopenedTab = yield newTabPromise;
  info("Tab restored");

  Assert.equal(gBrowser.tabs.length, 2, "Expect 2 tabs, the original tab was kept open");

  // Cleanup
  yield BrowserTestUtils.closeWindow(win);
});


add_task(function*() {
  info("Test undo closing a single tab via shortcut, including closing the original single blank tab");

  let win = yield BrowserTestUtils.openNewBrowserWindow();
  let gBrowser = win.gBrowser;

  let blankTab = gBrowser.selectedTab;
  info("Opening tab...");
  let tab1 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/tab1");

  info("Closing tab...");
  yield BrowserTestUtils.removeTab(tab1);

  let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, "http://example.com/tab1");
  let closeTabPromise = BrowserTestUtils.removeTab(blankTab, {dontRemove: true});

  info("Synthesizing key shortcut to restore tab...");
  EventUtils.synthesizeKey("t", {accelKey: true, shiftKey: true}, win);
  let reopenedTab = yield newTabPromise;
  info("Tab restored");
  yield closeTabPromise;
  info("Original blank tab closed");

  Assert.equal(gBrowser.tabs.length, 1, "Expect 1 tab, the blank tab having been closed");

  // Cleanup
  yield BrowserTestUtils.closeWindow(win);
});



add_task(function*() {
  info("Test undo closing a single tab via shortcut, not closing original tab");

  let win = yield BrowserTestUtils.openNewBrowserWindow();
  let gBrowser = win.gBrowser;

  info("Loading tab to keep");
  let originalTab = gBrowser.selectedTab;
  let originalTabLoad = BrowserTestUtils.browserLoaded(originalTab.linkedBrowser);
  originalTab.linkedBrowser.loadURI("http://example.com/originalTab");
  yield originalTabLoad;

  info("Opening tab...");
  let tab1 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/tab1");

  info("Closing tab...");
  yield BrowserTestUtils.removeTab(tab1);

  let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, "http://example.com/tab1");

  info("Synthesizing key shortcut to restore tab...");
  EventUtils.synthesizeKey("t", {accelKey: true, shiftKey: true}, win);
  let reopenedTab = yield newTabPromise;
  info("Tab restored");

  Assert.equal(gBrowser.tabs.length, 2, "Expect 2 tabs, the original tab was kept open");

  // Cleanup
  yield BrowserTestUtils.closeWindow(win);
});
