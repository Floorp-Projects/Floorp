/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * This file contains tests for the window.LoopUI active tab trackers.
 */
"use strict";

const {injectLoopAPI} = Cu.import("resource:///modules/loop/MozLoopAPI.jsm", {});
gMozLoopAPI = injectLoopAPI({});

let handlers = [
  {
    resolve: null,
    windowId: null,
    listener: function(err, windowId) {
      handlers[0].windowId = windowId;
      handlers[0].resolve();
    }
  },
  {
    resolve: null,
    windowId: null,
    listener: function(err, windowId) {
      handlers[1].windowId = windowId;
      handlers[1].resolve();
    }
  }
];

function promiseWindowIdReceivedOnAdd(handler) {
  return new Promise(resolve => {
    handler.resolve = resolve;
    gMozLoopAPI.addBrowserSharingListener(handler.listener);
  });
};

let createdTabs = [];

function promiseWindowIdReceivedNewTab(handlers = []) {
  let promiseHandlers = [];

  handlers.forEach(handler => {
    promiseHandlers.push(new Promise(resolve => {
      handler.resolve = resolve;
    }));
  });

  let createdTab = gBrowser.selectedTab = gBrowser.addTab();
  createdTabs.push(createdTab);

  promiseHandlers.push(promiseTabLoadEvent(createdTab, "about:mozilla"));

  return Promise.all(promiseHandlers);
};

function promiseRemoveTab(tab) {
  return new Promise(resolve => {
    gBrowser.tabContainer.addEventListener("TabClose", function onTabClose() {
      gBrowser.tabContainer.removeEventListener("TabClose", onTabClose);
      resolve();
    });
    gBrowser.removeTab(tab);
  });
}

function* removeTabs() {
  for (let createdTab of createdTabs) {
    yield promiseRemoveTab(createdTab);
  }

  createdTabs = [];
}

add_task(function* test_singleListener() {
  yield promiseWindowIdReceivedOnAdd(handlers[0]);

  let initialWindowId = handlers[0].windowId;

  Assert.notEqual(initialWindowId, null, "window id should be valid");

  // Check that a new tab updates the window id.
  yield promiseWindowIdReceivedNewTab([handlers[0]]);

  let newWindowId = handlers[0].windowId;

  Assert.notEqual(initialWindowId, newWindowId, "Tab contentWindow IDs shouldn't be the same");

  // Now remove the listener.
  gMozLoopAPI.removeBrowserSharingListener(handlers[0].listener);

  yield removeTabs();
});

add_task(function* test_multipleListener() {
  yield promiseWindowIdReceivedOnAdd(handlers[0]);

  let initialWindowId0 = handlers[0].windowId;

  Assert.notEqual(initialWindowId0, null, "window id should be valid");

  yield promiseWindowIdReceivedOnAdd(handlers[1]);

  let initialWindowId1 = handlers[1].windowId;

  Assert.notEqual(initialWindowId1, null, "window id should be valid");
  Assert.equal(initialWindowId0, initialWindowId1, "window ids should be the same");

  // Check that a new tab updates the window id.
  yield promiseWindowIdReceivedNewTab(handlers);

  let newWindowId0 = handlers[0].windowId;
  let newWindowId1 = handlers[1].windowId;

  Assert.ok(newWindowId0, "windowId should not be null anymore");
  Assert.equal(newWindowId0, newWindowId1, "Listeners should have the same windowId");
  Assert.notEqual(initialWindowId0, newWindowId0, "Tab contentWindow IDs shouldn't be the same");

  // Now remove the first listener.
  gMozLoopAPI.removeBrowserSharingListener(handlers[0].listener);

  // Check that a new tab updates the window id.
  yield promiseWindowIdReceivedNewTab([handlers[1]]);

  let nextWindowId0 = handlers[0].windowId;
  let nextWindowId1 = handlers[1].windowId;

  Assert.ok(nextWindowId0, "windowId should not be null anymore");
  Assert.equal(newWindowId0, nextWindowId0, "First listener shouldn't have updated");
  Assert.notEqual(newWindowId1, nextWindowId1, "Second listener should have updated");

  // Cleanup.
  gMozLoopAPI.removeBrowserSharingListener(handlers[1].listener);

  yield removeTabs();
});

add_task(function* test_infoBar() {
  const kNSXUL = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
  const kBrowserSharingNotificationId = "loop-sharing-notification";
  const kPrefBrowserSharingInfoBar = "loop.browserSharing.showInfoBar";

  Services.prefs.setBoolPref(kPrefBrowserSharingInfoBar, true);

  // First we add two tabs.
  yield promiseWindowIdReceivedNewTab();
  yield promiseWindowIdReceivedNewTab();
  Assert.strictEqual(gBrowser.selectedTab, createdTabs[1],
    "The second tab created should be selected now");

  // Add one sharing listener, which should show the infobar.
  yield promiseWindowIdReceivedOnAdd(handlers[0]);

  let getInfoBar = function() {
    let box = gBrowser.getNotificationBox(gBrowser.selectedBrowser);
    return box.getNotificationWithValue(kBrowserSharingNotificationId);
  };

  let testBarProps = function() {
    let bar = getInfoBar();

    // Start with some basic assertions for the bar.
    Assert.ok(bar, "The notification bar should be visible");
    Assert.strictEqual(bar.hidden, false, "Again, the notification bar should be visible");

    let button = bar.querySelector(".notification-button");
    Assert.ok(button, "There should be a button present");
    Assert.strictEqual(button.type, "menu-button", "We're expecting a menu-button");
    Assert.strictEqual(button.getAttribute("anchor"), "dropmarker",
      "The popup should be opening anchored to the dropmarker");
    Assert.strictEqual(button.getElementsByTagNameNS(kNSXUL, "menupopup").length, 1,
      "There should be a popup attached to the button");
  }

  testBarProps();

  // When we switch tabs, the infobar should move along with it. We use `selectedIndex`
  // here, because that's the setter that triggers the 'select' event. This event
  // is what LoopUI listens to and moves the infobar.
  gBrowser.selectedIndex = Array.indexOf(gBrowser.tabs, createdTabs[0]);

  // We now know that the second tab is selected and should be displaying the
  // infobar.
  testBarProps();

  // Test hiding the infoBar.
  getInfoBar().querySelector(".notification-button")
              .getElementsByTagNameNS(kNSXUL, "menuitem")[0].click();
  Assert.equal(getInfoBar(), null, "The notification should be hidden now");
  Assert.strictEqual(Services.prefs.getBoolPref(kPrefBrowserSharingInfoBar), false,
    "The pref should be set to false when the menu item is clicked");

  gBrowser.selectedIndex = Array.indexOf(gBrowser.tabs, createdTabs[1]);

  Assert.equal(getInfoBar(), null, "The notification should still be hidden");

  // Cleanup.
  gMozLoopAPI.removeBrowserSharingListener(handlers[0].listener);
  yield removeTabs();
  Services.prefs.clearUserPref(kPrefBrowserSharingInfoBar);
});
