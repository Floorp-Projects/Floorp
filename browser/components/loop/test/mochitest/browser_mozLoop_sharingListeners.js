/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * This file contains tests for the window.LoopUI active tab trackers.
 */
"use strict";

const {injectLoopAPI} = Cu.import("resource:///modules/loop/MozLoopAPI.jsm");
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

function promiseWindowIdReceivedNewTab(handlers) {
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

function removeTabs() {
  for (let createdTab of createdTabs) {
    gBrowser.removeTab(createdTab);
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

  removeTabs();
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

  Assert.equal(newWindowId0, newWindowId1, "Listeners should have the same windowId");
  Assert.notEqual(initialWindowId0, newWindowId0, "Tab contentWindow IDs shouldn't be the same");

  // Now remove the first listener.
  gMozLoopAPI.removeBrowserSharingListener(handlers[0].listener);

  // Check that a new tab updates the window id.
  yield promiseWindowIdReceivedNewTab([handlers[1]]);

  let nextWindowId0 = handlers[0].windowId;
  let nextWindowId1 = handlers[1].windowId;

  Assert.equal(newWindowId0, nextWindowId0, "First listener shouldn't have updated");
  Assert.notEqual(newWindowId1, nextWindowId1, "Second listener should have updated");

  // Cleanup.
  gMozLoopAPI.removeBrowserSharingListener(handlers[1].listener);

  removeTabs();
});

