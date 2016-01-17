/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Class } = require('../core/heritage');
const { Tab, tabEvents } = require('./tab');
const { EventTarget } = require('../event/target');
const { emit, setListeners } = require('../event/core');
const { pipe } = require('../event/utils');
const { observer: windowObserver } = require('../windows/observer');
const { List, addListItem, removeListItem } = require('../util/list');
const { modelFor } = require('../model/core');
const { viewFor } = require('../view/core');
const { getTabs, getSelectedTab } = require('./utils');
const { getMostRecentBrowserWindow, isBrowser } = require('../window/utils');
const { Options } = require('./common');
const { isPrivate } = require('../private-browsing');
const { ignoreWindow, isWindowPBSupported } = require('../private-browsing/utils')
const { isPrivateBrowsingSupported } = require('sdk/self');

const supportPrivateTabs = isPrivateBrowsingSupported && isWindowPBSupported;

const Tabs = Class({
  implements: [EventTarget],
  extends: List,
  initialize: function() {
    List.prototype.initialize.call(this);

    // We must do the list manipulation here where the object is extensible
    this.on("open", tab => {
      addListItem(this, tab);
    });

    this.on("close", tab => {
      removeListItem(this, tab);
    });
  },

  get activeTab() {
    let activeDomWin = getMostRecentBrowserWindow();
    if (!activeDomWin)
      return null;
    return modelFor(getSelectedTab(activeDomWin));
  },

  open: function(options) {
    options = Options(options);

    // TODO: Remove the dependency on the windows module: bug 792670
    let windows = require('../windows').browserWindows;
    let activeWindow = windows.activeWindow;

    let privateState = supportPrivateTabs && options.isPrivate;
    // When no isPrivate option was passed use the private state of the active
    // window
    if (activeWindow && privateState === undefined)
      privateState = isPrivate(activeWindow);

    function getWindow(privateState) {
      for (let window of windows) {
        if (privateState === isPrivate(window)) {
          return window;
        }
      }
      return null;
    }

    function openNewWindowWithTab() {
      windows.open({
        url: options.url,
        isPrivate: privateState,
        onOpen: function(newWindow) {
          let tab = newWindow.tabs[0];
          setListeners(tab, options);

          if (options.isPinned)
            tab.pin();

          // We don't emit the open event for the first tab in a new window so
          // do it now the listeners are attached
          emit(tab, "open", tab);
        }
      });
    }

    if (options.inNewWindow)
      return openNewWindowWithTab();

    // if the active window is in the state that we need then use it
    if (activeWindow && (privateState === isPrivate(activeWindow)))
      return activeWindow.tabs.open(options);

    // find a window in the state that we need
    let window = getWindow(privateState);
    if (window)
      return window.tabs.open(options);

    return openNewWindowWithTab();
  }
});

const allTabs = new Tabs();
// Export a new object with allTabs as the prototype, otherwise allTabs becomes
// frozen and addListItem and removeListItem don't work correctly.
module.exports = Object.create(allTabs);
pipe(tabEvents, module.exports);

function addWindowTab(window, tabElement) {
  let tab = new Tab(tabElement);
  if (window)
    addListItem(window.tabs, tab);
  addListItem(allTabs, tab);
  emit(allTabs, "open", tab);
}

// Find tabs in already open windows
for (let tabElement of getTabs())
  addWindowTab(null, tabElement);

// Detect tabs in new windows
windowObserver.on('open', domWindow => {
  if (!isBrowser(domWindow) || ignoreWindow(domWindow))
    return;

  let window = null;
  try {
    modelFor(domWindow);
  }
  catch (e) { }

  for (let tabElement of getTabs(domWindow)) {
    addWindowTab(window, tabElement);
  }
});
