/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Cc, Ci} = require("chrome");
const {getMostRecentBrowserWindow, open} = require("sdk/window/utils");
const tabUtils = require("sdk/tabs/utils");
const {when} = require("sdk/dom/events");


var observerService = Cc["@mozilla.org/observer-service;1"]
                      .getService(Ci.nsIObserverService);

const { ShimWaiver } = Cu.import("resource://gre/modules/ShimWaiver.jsm");
const addObserver = ShimWaiver.getProperty(observerService, "addObserver");
const removeObserver = ShimWaiver.getProperty(observerService, "removeObserver");

const getActiveTab = (window=getMostRecentBrowserWindow()) =>
  tabUtils.getActiveTab(window)

const openWindow = () => {
  const window = open();
  return new Promise((resolve) => {
    addObserver({
      observe(subject, topic) {
        if (subject === window) {
          removeObserver(this, topic);
          resolve(subject);
        }
      }
    }, "browser-delayed-startup-finished", false);
  });
};
exports.openWindow = openWindow;

const closeWindow = (window) => {
  const closed = when(window, "unload", true).then(_ => window);
  window.close();
  return closed;
};
exports.closeWindow = closeWindow;

const openTab = (url, window=getMostRecentBrowserWindow()) => {
  const tab = tabUtils.openTab(window, url);
  const browser = tabUtils.getBrowserForTab(tab);

  return when(browser, "load", true).then(_ => tab);
};
exports.openTab = openTab;

const closeTab = (tab) => {
  const result = when(tab, "TabClose").then(_ => tab);
  tabUtils.closeTab(tab);

  return result;
};
exports.closeTab = closeTab;

const withTab = (test, uri="about:blank") => function*(assert) {
  const tab = yield openTab(uri);
  try {
    yield* test(assert, tab);
  }
  finally {
    yield closeTab(tab);
  }
};
exports.withTab = withTab;

const withWindow = () => function*(assert) {
  const window = yield openWindow();
  try {
    yield* test(assert, window);
  }
  finally {
    yield closeWindow(window);
  }
};
exports.withWindow = withWindow;

const receiveMessage = (manager, name) => new Promise((resolve) => {
  manager.addMessageListener(name, {
    receiveMessage(message) {
      manager.removeMessageListener(name, this);
      resolve(message);
    }
  });
});
exports.receiveMessage = receiveMessage;
