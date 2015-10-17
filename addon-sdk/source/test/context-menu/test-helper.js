/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 "use strict";

const { Ci } = require("chrome");
const { getMostRecentBrowserWindow } = require('sdk/window/utils');
const { Loader } = require('sdk/test/loader');
const { merge } = require("sdk/util/object");
const observers = require("sdk/system/events");
const { defer } = require("sdk/core/promise");
const timer = require("sdk/timers");

// These should match the same constants in the module.
const ITEM_CLASS = "addon-context-menu-item";
const SEPARATOR_CLASS = "addon-context-menu-separator";
const OVERFLOW_THRESH_DEFAULT = 10;
const OVERFLOW_THRESH_PREF =
  "extensions.addon-sdk.context-menu.overflowThreshold";
const OVERFLOW_MENU_CLASS = "addon-content-menu-overflow-menu";
const OVERFLOW_POPUP_CLASS = "addon-content-menu-overflow-popup";

const TEST_DOC_URL = module.uri.replace(/context-menu\/test-helper\.js$/, "test-context-menu.html");

// This makes it easier to run tests by handling things like opening the menu,
// opening new windows, making assertions, etc.  Methods on |test| can be called
// on instances of this class.  Don't forget to call done() to end the test!
// WARNING: This looks up items in popups by comparing labels, so don't give two
// items the same label.
function TestHelper(assert, done) {
  // Methods on the wrapped test can be called on this object.
  for (var prop in assert)
    this[prop] = (...args) => assert[prop].apply(assert, args);
  this.assert = assert;
  this.end = done;
  this.loaders = [];
  this.browserWindow = getMostRecentBrowserWindow();
  this.overflowThreshValue = require("sdk/preferences/service").
                             get(OVERFLOW_THRESH_PREF, OVERFLOW_THRESH_DEFAULT);
  this.done = this.done.bind(this);
}

TestHelper.prototype = {
  get contextMenuPopup() {
    return this.browserWindow.document.getElementById("contentAreaContextMenu");
  },

  get contextMenuSeparator() {
    return this.browserWindow.document.querySelector("." + SEPARATOR_CLASS);
  },

  get overflowPopup() {
    return this.browserWindow.document.querySelector("." + OVERFLOW_POPUP_CLASS);
  },

  get overflowSubmenu() {
    return this.browserWindow.document.querySelector("." + OVERFLOW_MENU_CLASS);
  },

  get tabBrowser() {
    return this.browserWindow.gBrowser;
  },

  // Asserts that elt, a DOM element representing item, looks OK.
  checkItemElt: function (elt, item) {
    let itemType = this.getItemType(item);

    switch (itemType) {
    case "Item":
      this.assert.equal(elt.localName, "menuitem",
                            "Item DOM element should be a xul:menuitem");
      if (typeof(item.data) === "string") {
        this.assert.equal(elt.getAttribute("value"), item.data,
                              "Item should have correct data");
      }
      break
    case "Menu":
      this.assert.equal(elt.localName, "menu",
                            "Menu DOM element should be a xul:menu");
      let subPopup = elt.firstChild;
      this.assert.ok(subPopup, "xul:menu should have a child");
      this.assert.equal(subPopup.localName, "menupopup",
                            "xul:menu's first child should be a menupopup");
      break;
    case "Separator":
      this.assert.equal(elt.localName, "menuseparator",
                         "Separator DOM element should be a xul:menuseparator");
      break;
    }

    if (itemType === "Item" || itemType === "Menu") {
      this.assert.equal(elt.getAttribute("label"), item.label,
                            "Item should have correct title");

      // validate accesskey prop
      if (item.accesskey) {
        this.assert.equal(elt.getAttribute("accesskey"),
                          item.accesskey,
                          "Item should have correct accesskey");
      }
      else {
        this.assert.equal(elt.getAttribute("accesskey"),
                          "",
                          "Item should not have accesskey");
      }

      // validate image prop
      if (typeof(item.image) === "string") {
        this.assert.equal(elt.getAttribute("image"), item.image,
                              "Item should have correct image");
        if (itemType === "Menu")
          this.assert.ok(elt.classList.contains("menu-iconic"),
                           "Menus with images should have the correct class")
        else
          this.assert.ok(elt.classList.contains("menuitem-iconic"),
                           "Items with images should have the correct class")
      }
      else {
        this.assert.ok(!elt.getAttribute("image"),
                         "Item should not have image");
        this.assert.ok(!elt.classList.contains("menu-iconic") && !elt.classList.contains("menuitem-iconic"),
                         "The iconic classes should not be present")
      }
    }
  },

  // Asserts that the context menu looks OK given the arguments.  presentItems
  // are items that have been added to the menu.  absentItems are items that
  // shouldn't match the current context.  removedItems are items that have been
  // removed from the menu.
  checkMenu: function (presentItems, absentItems, removedItems) {
    // Count up how many top-level items there are
    let total = 0;
    for (let item of presentItems) {
      if (absentItems.indexOf(item) < 0 && removedItems.indexOf(item) < 0)
        total++;
    }

    let separator = this.contextMenuSeparator;
    if (total == 0) {
      this.assert.ok(!separator || separator.hidden,
                       "separator should not be present");
    }
    else {
      this.assert.ok(separator && !separator.hidden,
                       "separator should be present");
    }

    let mainNodes = this.browserWindow.document.querySelectorAll("#contentAreaContextMenu > ." + ITEM_CLASS);
    let overflowNodes = this.browserWindow.document.querySelectorAll("." + OVERFLOW_POPUP_CLASS + " > ." + ITEM_CLASS);

    this.assert.ok(mainNodes.length == 0 || overflowNodes.length == 0,
                     "Should only see nodes at the top level or in overflow");

    let overflow = this.overflowSubmenu;
    if (this.shouldOverflow(total)) {
      this.assert.ok(overflow && !overflow.hidden,
                       "overflow menu should be present");
      this.assert.equal(mainNodes.length, 0,
                            "should be no items in the main context menu");
    }
    else {
      this.assert.ok(!overflow || overflow.hidden,
                       "overflow menu should not be present");
      // When visible nodes == 0 they could be in overflow or top level
      if (total > 0) {
        this.assert.equal(overflowNodes.length, 0,
                              "should be no items in the overflow context menu");
      }
    }

    // Iterate over wherever the nodes have ended up
    let nodes = mainNodes.length ? mainNodes : overflowNodes;
    this.checkNodes(nodes, presentItems, absentItems, removedItems)
    let pos = 0;
  },

  // Recurses through the item hierarchy of presentItems comparing it to the
  // node hierarchy of nodes. Any items in removedItems will be skipped (so
  // should not exist in the XUL), any items in absentItems must exist and be
  // hidden
  checkNodes: function (nodes, presentItems, absentItems, removedItems) {
    let pos = 0;
    for (let item of presentItems) {
      // Removed items shouldn't be in the list
      if (removedItems.indexOf(item) >= 0)
        continue;

      if (nodes.length <= pos) {
        this.assert.ok(false, "Not enough nodes");
        return;
      }

      let hidden = absentItems.indexOf(item) >= 0;

      this.checkItemElt(nodes[pos], item);
      this.assert.equal(nodes[pos].hidden, hidden,
                            "hidden should be set correctly");

      // The contents of hidden menus doesn't matter so much
      if (!hidden && this.getItemType(item) == "Menu") {
        this.assert.equal(nodes[pos].firstChild.localName, "menupopup",
                              "menu XUL should contain a menupopup");
        this.checkNodes(nodes[pos].firstChild.childNodes, item.items, absentItems, removedItems);
      }

      if (pos > 0)
        this.assert.equal(nodes[pos].previousSibling, nodes[pos - 1],
                              "nodes should all be in the same group");
      pos++;
    }

    this.assert.equal(nodes.length, pos,
                          "should have checked all the XUL nodes");
  },

  // Attaches an event listener to node.  The listener is automatically removed
  // when it's fired (so it's assumed it will fire), and callback is called
  // after a short delay.  Since the module we're testing relies on the same
  // event listeners to do its work, this is to give them a little breathing
  // room before callback runs.  Inside callback |this| is this object.
  // Optionally you can pass a function to test if the event is the event you
  // want.
  delayedEventListener: function (node, event, callback, useCapture, isValid) {
    const self = this;
    node.addEventListener(event, function handler(evt) {
      if (isValid && !isValid(evt))
        return;
      node.removeEventListener(event, handler, useCapture);
      timer.setTimeout(function () {
        try {
          callback.call(self, evt);
        }
        catch (err) {
          self.assert.fail(err);
          self.end();
        }
      }, 20);
    }, useCapture);
  },

  // Call to finish the test.
  done: function () {
    const self = this;
    function commonDone() {
      this.closeTab();

      while (this.loaders.length) {
        this.loaders[0].unload();
      }

      require("sdk/preferences/service").set(OVERFLOW_THRESH_PREF, self.overflowThreshValue);

      this.end();
    }

    function closeBrowserWindow() {
      if (this.oldBrowserWindow) {
        this.delayedEventListener(this.browserWindow, "unload", commonDone,
                                  false);
        this.browserWindow.close();
        this.browserWindow = this.oldBrowserWindow;
        delete this.oldBrowserWindow;
      }
      else {
        commonDone.call(this);
      }
    };

    if (this.contextMenuPopup.state == "closed") {
      closeBrowserWindow.call(this);
    }
    else {
      this.delayedEventListener(this.contextMenuPopup, "popuphidden",
                                function () {
                                  return closeBrowserWindow.call(this);
                                },
                                false);
      this.contextMenuPopup.hidePopup();
    }
  },

  closeTab: function() {
    if (this.tab) {
      this.tabBrowser.removeTab(this.tab);
      this.tabBrowser.selectedTab = this.oldSelectedTab;
      this.tab = null;
    }
  },

  // Returns the DOM element in popup corresponding to item.
  // WARNING: The element is found by comparing labels, so don't give two items
  // the same label.
  getItemElt: function (popup, item) {
    let nodes = popup.childNodes;
    for (let i = nodes.length - 1; i >= 0; i--) {
      if (this.getItemType(item) === "Separator") {
        if (nodes[i].localName === "menuseparator")
          return nodes[i];
      }
      else if (nodes[i].getAttribute("label") === item.label)
        return nodes[i];
    }
    return null;
  },

  // Returns "Item", "Menu", or "Separator".
  getItemType: function (item) {
    // Could use instanceof here, but that would require accessing the loader
    // that created the item, and I don't want to A) somehow search through the
    // this.loaders list to find it, and B) assume there are any live loaders at
    // all.
    return /^\[object (Item|Menu|Separator)/.exec(item.toString())[1];
  },

  // Returns a wrapper around a new loader: { loader, cm, unload, globalScope }.
  // loader is a Cuddlefish sandboxed loader, cm is the context menu module,
  // globalScope is the context menu module's global scope, and unload is a
  // function that unloads the loader and associated resources.
  newLoader: function () {
    const self = this;
    const selfModule = require('sdk/self');
    let loader = Loader(module, null, null, {
      modules: {
        "sdk/self": merge({}, selfModule, {
          data: merge({}, selfModule.data, require("../fixtures"))
        })
      }
    });

    let wrapper = {
      loader: loader,
      cm: loader.require("sdk/context-menu"),
      globalScope: loader.sandbox("sdk/context-menu"),
      unload: function () {
        loader.unload();
        let idx = self.loaders.indexOf(wrapper);
        if (idx < 0)
          throw new Error("Test error: tried to unload nonexistent loader");
        self.loaders.splice(idx, 1);
      }
    };
    this.loaders.push(wrapper);
    return wrapper;
  },

  // As above but the loader has private-browsing support enabled.
  newPrivateLoader: function() {
    let base = require("@loader/options");

    // Clone current loader's options adding the private-browsing permission
    let options = merge({}, base, {
      metadata: merge({}, base.metadata || {}, {
        permissions: merge({}, base.metadata.permissions || {}, {
          'private-browsing': true
        })
      })
    });

    const self = this;
    let loader = Loader(module, null, options);
    let wrapper = {
      loader: loader,
      cm: loader.require("sdk/context-menu"),
      globalScope: loader.sandbox("sdk/context-menu"),
      unload: function () {
        loader.unload();
        let idx = self.loaders.indexOf(wrapper);
        if (idx < 0)
          throw new Error("Test error: tried to unload nonexistent loader");
        self.loaders.splice(idx, 1);
      }
    };
    this.loaders.push(wrapper);
    return wrapper;
  },

  // Returns true if the count crosses the overflow threshold.
  shouldOverflow: function (count) {
    return count >
           (this.loaders.length ?
            this.loaders[0].loader.require("sdk/preferences/service").
              get(OVERFLOW_THRESH_PREF, OVERFLOW_THRESH_DEFAULT) :
            OVERFLOW_THRESH_DEFAULT);
  },

  // Loads scripts necessary in the content process
  loadFrameScript: function(browser = this.browserWindow.gBrowser.selectedBrowser) {
    function frame_script() {
      let { interfaces: Ci } = Components;
      addMessageListener('test:contextmenu', ({ data: { selectors } }) => {
        let targetNode = null;
        let contentWin = content;
        if (selectors) {
          while (selectors.length) {
            targetNode = contentWin.document.querySelector(selectors.shift());
            if (selectors.length)
              contentWin = targetNode.contentWindow;
          }
        }

        let rect = targetNode ?
                   targetNode.getBoundingClientRect() :
                   { left: 0, top: 0, width: 0, height: 0 };
        contentWin.QueryInterface(Ci.nsIInterfaceRequestor)
                  .getInterface(Ci.nsIDOMWindowUtils)
                  .sendMouseEvent('contextmenu',
                  rect.left + (rect.width / 2),
                  rect.top + (rect.height / 2),
                  2, 1, 0);
      });

      addMessageListener('test:ping', () => {
        sendAsyncMessage('test:pong');
      });

      addMessageListener('test:select', ({ data: { selector, start, end } }) => {
        let element = content.document.querySelector(selector);
        element.focus();
        if (end === null)
          end = element.value.length;
        element.setSelectionRange(start, end);
      });
    }

    let messageManager = browser.messageManager;
    messageManager.loadFrameScript("data:,(" + frame_script.toString() + ")();", true);
  },

  selectRange: function(selector, start, end) {
    let messageManager = this.browserWindow.gBrowser.selectedBrowser.messageManager;
    messageManager.sendAsyncMessage('test:select', { selector, start, end });
  },

  // Opens the context menu on the current page.  If selectors is null, the
  // menu is opened in the top-left corner.  onShowncallback is passed the
  // popup. selectors is an array of selectors. Starting from the main document
  // each selector points to an iframe, the last selector gives the target node.
  // In the simple case of a single selector just that string can be passed
  // instead of an array
  showMenu: function(selectors, onshownCallback) {
    let { promise, resolve } = defer();

    if (selectors && !Array.isArray(selectors))
      selectors = [selectors];

    let sendEvent = () => {
      let menu = this.browserWindow.document.getElementById("contentAreaContextMenu");
      this.delayedEventListener(menu, "popupshowing",
        function (e) {
          let popup = e.target;
          if (onshownCallback) {
            onshownCallback.call(this, popup);
          }
          resolve(popup);
        }, false);

      let messageManager = this.browserWindow.gBrowser.selectedBrowser.messageManager;
      messageManager.sendAsyncMessage('test:contextmenu', { selectors });
    }

    // Bounces an asynchronous message through the browser message manager.
    // This ensures that any pending messages have been delivered to the frame
    // scripts and so the remote proxies have been updated
    let flushMessages = () => {
      let listener = () => {
        messageManager.removeMessageListener('test:pong', listener);
        sendEvent();
      };

      let messageManager = this.browserWindow.gBrowser.selectedBrowser.messageManager;
      messageManager.addMessageListener('test:pong', listener);
      messageManager.sendAsyncMessage('test:ping');
    }

    // If a new tab or window has not yet been opened, open a new tab now.  For
    // some reason using the tab already opened when the test starts causes
    // leaks.  See bug 566351 for details.
    if (!selectors && !this.oldSelectedTab && !this.oldBrowserWindow) {
      this.oldSelectedTab = this.tabBrowser.selectedTab;
      this.tab = this.tabBrowser.addTab("about:blank");
      let browser = this.tabBrowser.getBrowserForTab(this.tab);

      this.delayedEventListener(browser, "load", function () {
        this.tabBrowser.selectedTab = this.tab;
        this.loadFrameScript();
        flushMessages();
      }, true);
    }
    else {
      flushMessages();
    }

    return promise;
  },

  hideMenu: function(onhiddenCallback) {
    this.delayedEventListener(this.browserWindow, "popuphidden", onhiddenCallback);

    this.contextMenuPopup.hidePopup();
  },

  // Opens a new browser window.  The window will be closed automatically when
  // done() is called.
  withNewWindow: function (onloadCallback, makePrivate = false) {
    let win = this.browserWindow.OpenBrowserWindow({ private: makePrivate });
    observers.once("browser-delayed-startup-finished", () => {
      // Open a new tab so we can make sure it is remote and loaded
      win.gBrowser.selectedTab = win.gBrowser.addTab();
      this.loadFrameScript();
      this.delayedEventListener(win.gBrowser.selectedBrowser, "load", onloadCallback, true);
    });
    this.oldBrowserWindow = this.browserWindow;
    this.browserWindow = win;
  },

  // Opens a new private browser window.  The window will be closed
  // automatically when done() is called.
  withNewPrivateWindow: function (onloadCallback) {
    this.withNewWindow(onloadCallback, true);
  },

  // Opens a new tab with our test page in the current window.  The tab will
  // be closed automatically when done() is called.
  withTestDoc: function (onloadCallback) {
    this.oldSelectedTab = this.tabBrowser.selectedTab;
    this.tab = this.tabBrowser.addTab(TEST_DOC_URL);
    let browser = this.tabBrowser.getBrowserForTab(this.tab);

    this.delayedEventListener(browser, "load", function () {
      this.tabBrowser.selectedTab = this.tab;
      this.loadFrameScript();
      onloadCallback.call(this, browser.contentWindow, browser.contentDocument);
    }, true, function(evt) {
      return evt.target.location == TEST_DOC_URL;
    });
  }
};
exports.TestHelper = TestHelper;
