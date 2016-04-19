/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * This file contains tests for the window.LoopUI active tab trackers.
 */
"use strict";

var [, gHandlers] = LoopAPI.inspect();

var handlers = [
  { windowId: null }, { windowId: null }
];

var listenerCount = 41;
var listenerIds = [];

function promiseWindowId() {
  return new Promise(resolve => {
    LoopAPI.stub([{
      sendAsyncMessage: function(messageName, data) {
        let [name, windowId] = data;
        if (name == "BrowserSwitch") {
          LoopAPI.restore();
          resolve(windowId);
        }
      }
    }]);
    listenerIds.push(++listenerCount);
    gHandlers.AddBrowserSharingListener({ data: [listenerCount] }, () => {});
  });
}

function* promiseWindowIdReceivedOnAdd(handler) {
  handler.windowId = yield promiseWindowId();
}

var createdTabs = [];

function* promiseWindowIdReceivedNewTab(handlersParam = []) {
  let createdTab = gBrowser.selectedTab = gBrowser.addTab("about:mozilla");
  createdTabs.push(createdTab);

  let windowId = yield promiseWindowId();

  for (let handler of handlersParam) {
    handler.windowId = windowId;
  }
}

function promiseNewTabLocation() {
  BrowserOpenTab();
  let tab = gBrowser.selectedTab;
  let browser = tab.linkedBrowser;
  createdTabs.push(tab);

  // If we're already loaded, then just get the location.
  if (browser.contentDocument.readyState === "complete") {
    return ContentTask.spawn(browser, null, () => content.location.href);
  }

  // Otherwise, wait for the load to complete.
  return BrowserTestUtils.browserLoaded(browser);
}

function* removeTabs() {
  for (let createdTab of createdTabs) {
    yield BrowserTestUtils.removeTab(createdTab);
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
  gHandlers.RemoveBrowserSharingListener({ data: [listenerIds.pop()] }, function() {});

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
  gHandlers.RemoveBrowserSharingListener({ data: [listenerIds.pop()] }, function() {});

  // Check that a new tab updates the window id.
  yield promiseWindowIdReceivedNewTab([handlers[1]]);

  let nextWindowId0 = handlers[0].windowId;
  let nextWindowId1 = handlers[1].windowId;

  Assert.ok(nextWindowId0, "windowId should not be null anymore");
  Assert.equal(newWindowId0, nextWindowId0, "First listener shouldn't have updated");
  Assert.notEqual(newWindowId1, nextWindowId1, "Second listener should have updated");

  // Cleanup.
  for (let listenerId of listenerIds) {
    gHandlers.RemoveBrowserSharingListener({ data: [listenerId] }, function() {});
  }

  yield removeTabs();
});

add_task(function* test_newtabLocation() {
  // Check location before sharing
  let locationBeforeSharing = yield promiseNewTabLocation();
  Assert.equal(locationBeforeSharing, "about:newtab");

  // Check location after sharing
  yield promiseWindowIdReceivedOnAdd(handlers[0]);
  let locationAfterSharing = yield promiseNewTabLocation();
  info("Location after sharing: " + locationAfterSharing);
  Assert.ok(locationAfterSharing.match(/about:?home/));

  // Check location after stopping sharing
  gHandlers.RemoveBrowserSharingListener({ data: [listenerIds.pop()] }, function() {});
  let locationAfterStopping = yield promiseNewTabLocation();
  Assert.equal(locationAfterStopping, "about:newtab");

  yield removeTabs();
});
