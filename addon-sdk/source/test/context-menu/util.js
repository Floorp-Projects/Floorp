/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci } = require("chrome");
const { getMostRecentBrowserWindow } = require("sdk/window/utils");
const { open: openWindow, close: closeWindow } = require("sdk/window/helpers");
const tabUtils = require("sdk/tabs/utils");
const { map, filter, object, reduce, keys, symbols,
        pairs, values, each, some, isEvery, count } = require("sdk/util/sequence");
const { when } = require("sdk/dom/events");

const { Task } = require("resource://gre/modules/Task.jsm");

var observerService = Cc["@mozilla.org/observer-service;1"]
                      .getService(Ci.nsIObserverService);

const framescriptURI = require.resolve("./framescript");

const _target = ({target}) => target;

exports.openWindow = openWindow;
exports.closeWindow = closeWindow;

const getActiveTab = (window=getMostRecentBrowserWindow()) =>
  tabUtils.getActiveTab(window)

const openTab = (url, window=getMostRecentBrowserWindow()) => {
  const tab = tabUtils.openTab(window, url);
  const browser = tabUtils.getBrowserForTab(tab);
  browser.messageManager.loadFrameScript(framescriptURI, false);

  return when(browser, "load", true).then(_ => tab);
};
exports.openTab = openTab;

const openContextMenu = (selector, tab=getActiveTab()) => {
  const browser = tabUtils.getBrowserForTab(tab);
  browser.
    messageManager.
    sendAsyncMessage("sdk/test/context-menu/open",
                     {target: selector});

  return when(tab.ownerDocument.defaultView, "popupshown").
          then(_target);
};
exports.openContextMenu = openContextMenu;

const closeContextMenu = (menu) => {
  const result = when(menu.ownerDocument.defaultView, "popuphidden").
                  then(_target);

  menu.hidePopup();
  return result;
};
exports.closeContextMenu = closeContextMenu;

const closeTab = (tab) => {
  const result = when(tab, "TabClose").then(_ => tab);
  tabUtils.closeTab(tab);

  return result;
};
exports.closeTab = closeTab;

const select = (target, tab=getActiveTab()) =>
  new Promise(resolve => {
    const {messageManager} = tabUtils.getBrowserForTab(tab);
    messageManager.
      sendAsyncMessage("sdk/test/context-menu/select",
                       target);

    messageManager.addMessageListener("sdk/test/context-menu/selected", {
      receiveMessage({name}) {
        messageManager.removeMessageListener(name, this);
        resolve();
      }
    });
  });
exports.select = select;

const attributeBlacklist = new Set(["data-component-path"]);
const attributeRenameTable = Object.assign(Object.create(null), {
  class: "className"
});
const readAttributes = node =>
  object(...map(({name, value}) => [attributeRenameTable[name] || name, value],
                filter(({name}) => !attributeBlacklist.has(name),
                       node.attributes)));
exports.readAttributes = readAttributes;

const readNode = node =>
  Object.assign(readAttributes(node),
                {tagName: node.tagName, namespaceURI: node.namespaceURI},
                node.children.length ?
                  {children: [...map(readNode, node.children)]} :
                  {});
exports.readNode = readNode;

const captureContextMenu = (target=":root", options={}) => Task.spawn(function*() {
  const window = options.window || getMostRecentBrowserWindow();
  const tab = options.tab || tabUtils.getActiveTab(window);

  const menu = yield openContextMenu(target, tab);
  const tree = readNode(menu.querySelector(".sdk-context-menu-extension"));
  yield closeContextMenu(menu);
  return tree;
});
exports.captureContextMenu = captureContextMenu;

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

const withItems = (items, body) => function*() {
  try {
    yield* body(items);
  } finally {
      Object.keys(items).forEach(key => items[key].destroy());
  }
}();
exports.withItems = withItems;
