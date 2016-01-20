/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Class } = require('../core/heritage');
const { observer } = require('./observer');
const { isBrowser, getMostRecentBrowserWindow, windows, open, getInnerId,
        getWindowTitle, getToplevelWindow, isFocused, isWindowPrivate } = require('../window/utils');
const { List, addListItem, removeListItem } = require('../util/list');
const { viewFor } = require('../view/core');
const { modelFor } = require('../model/core');
const { emit, emitOnObject, setListeners } = require('../event/core');
const { once } = require('../dom/events');
const { EventTarget } = require('../event/target');
const { getSelectedTab } = require('../tabs/utils');
const { Cc, Ci } = require('chrome');
const { Options } = require('../tabs/common');
const system = require('../system/events');
const { ignoreWindow, isPrivate, isWindowPBSupported } = require('../private-browsing/utils');
const { data, isPrivateBrowsingSupported } = require('../self');
const { setImmediate } = require('../timers');

const supportPrivateWindows = isPrivateBrowsingSupported && isWindowPBSupported;

const modelsFor = new WeakMap();
const viewsFor = new WeakMap();

const Window = Class({
  implements: [EventTarget],
  initialize: function(domWindow) {
    modelsFor.set(domWindow, this);
    viewsFor.set(this, domWindow);
  },

  get title() {
    return getWindowTitle(viewsFor.get(this));
  },

  activate: function() {
    viewsFor.get(this).focus();
  },

  close: function(callback) {
    let domWindow = viewsFor.get(this);

    if (callback) {
      // We want to catch the close event immediately after the close events are
      // emitted everywhere but without letting the event loop spin. Registering
      // for the same events as windowEventListener but afterwards does this
      let listener = (event, closedWin) => {
        if (event != "close" || closedWin != domWindow)
          return;

        observer.off("*", listener);
        callback();
      }

      observer.on("*", listener);
    }

    domWindow.close();
  }
});

const windowTabs = new WeakMap();

const BrowserWindow = Class({
  extends: Window,

  get tabs() {
    let tabs = windowTabs.get(this);
    if (tabs)
      return tabs;

    return new WindowTabs(this);
  }
});

const WindowTabs = Class({
  implements: [EventTarget],
  extends: List,
  initialize: function(window) {
    List.prototype.initialize.call(this);
    windowTabs.set(window, this);
    viewsFor.set(this, viewsFor.get(window));

    // Make sure the tabs module has loaded and found all existing tabs
    const tabs = require('../tabs');

    for (let tab of tabs) {
      if (tab.window == window)
        addListItem(this, tab);
    }
  },

  get activeTab() {
    return modelFor(getSelectedTab(viewsFor.get(this)));
  },

  open: function(options) {
    options = Options(options);

    let domWindow = viewsFor.get(this);
    let { Tab } = require('../tabs/tab-firefox');

    // The capturing listener will see the TabOpen event before
    // sdk/tabs/observer giving us time to set up the tab and listeners before
    // the real open event is fired
    let listener = event => {
      new Tab(event.target, options);
    };

    once(domWindow, "TabOpen", listener, true);
    domWindow.gBrowser.addTab(options.url);
  }
});

const BrowserWindows = Class({
  implements: [EventTarget],
  extends: List,
  initialize: function() {
    List.prototype.initialize.call(this);
  },

  get activeWindow() {
    let domWindow = getMostRecentBrowserWindow();
    if (ignoreWindow(domWindow))
      return null;
    return modelsFor.get(domWindow);
  },

  open: function(options) {
    if (typeof options == "string")
      options = { url: options };

    let { url, isPrivate } = options;
    if (url)
      url = data.url(url);

    let args = Cc["@mozilla.org/supports-string;1"].
               createInstance(Ci.nsISupportsString);
    args.data = url;

    let features = {
      chrome: true,
      all: true,
      dialog: false
    };
    features.private = supportPrivateWindows && isPrivate;

    let domWindow = open(null, {
      parent: null,
      name: "_blank",
      features,
      args
    })

    let window = makeNewWindow(domWindow, true);
    setListeners(window, options);
    return window;
  }
});

const browserWindows = new BrowserWindows();
exports.browserWindows = browserWindows;

function windowEmit(window, event, ...args) {
  if (window instanceof BrowserWindow && (event == "open" || event == "close"))
    emitOnObject(window, event, browserWindows, window, ...args);
  else
    emit(window, event, window, ...args);

  if (window instanceof BrowserWindow)
    emit(browserWindows, event, window, ...args);
}

function makeNewWindow(domWindow, browserHint = false) {
  if (browserHint || isBrowser(domWindow))
    return new BrowserWindow(domWindow);
  else
    return new Window(domWindow);
}

for (let domWindow of windows()) {
  let window = makeNewWindow(domWindow);
  if (window instanceof BrowserWindow)
    addListItem(browserWindows, window);
}

var windowEventListener = (event, domWindow, ...args) => {
  let toplevelWindow = getToplevelWindow(domWindow);

  if (ignoreWindow(toplevelWindow))
    return;

  let window = modelsFor.get(toplevelWindow);
  if (!window)
    window = makeNewWindow(toplevelWindow);

  if (isBrowser(toplevelWindow)) {
    if (event == "open")
      addListItem(browserWindows, window);
    else if (event == "close")
      removeListItem(browserWindows, window);
  }

  windowEmit(window, event, ...args);

  // The window object shouldn't be reachable after closed
  if (event == "close") {
    viewsFor.delete(window);
    modelsFor.delete(toplevelWindow);
  }
};
observer.on("*", windowEventListener);

viewFor.define(BrowserWindow, window => {
  return viewsFor.get(window);
})

const isBrowserWindow = (x) => x instanceof BrowserWindow;
isPrivate.when(isBrowserWindow, (w) => isWindowPrivate(viewsFor.get(w)));
isFocused.when(isBrowserWindow, (w) => isFocused(viewsFor.get(w)));
