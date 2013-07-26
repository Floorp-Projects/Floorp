/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

// TODO: BUG 792670 - remove dependency below
const { browserWindows: windows } = require('../windows');
const { tabs } = require('../windows/tabs-firefox');
const { isPrivate } = require('../private-browsing');
const { isWindowPBSupported } = require('../private-browsing/utils')
const { isPrivateBrowsingSupported } = require('sdk/self');

const supportPrivateTabs = isPrivateBrowsingSupported && isWindowPBSupported;

function newTabWindow(options) {
  // `tabs` option is under review and may be removed.
  return windows.open({
    tabs: [ options ],
    isPrivate: options.isPrivate
  });
}

Object.defineProperties(tabs, {
  open: { value: function open(options) {
    if (options.inNewWindow) {
        newTabWindow(options);
        return undefined;
    }
    // Open in active window if new window was not required.

    let activeWindow = windows.activeWindow;
    let privateState = !!options.isPrivate;

    // if the active window is in the state that we need then use it
    if (activeWindow && (!supportPrivateTabs || privateState === isPrivate(activeWindow))) {
      activeWindow.tabs.open(options);
    }
    else {
      // find a window in the state that we need
      let window = getWindow(privateState);
      if (window) {
        window.tabs.open(options);
      }
      // open a window in the state that we need
      else {
        newTabWindow(options);
      }
    }

    return undefined;
  }}
});

function getWindow(privateState) {
  for each (let window in windows) {
    if (privateState === isPrivate(window)) {
      return window;
    }
  }
  return null;
}

// Workaround for bug 674195. Freezing objects from other compartments fail,
// so we use `Object.freeze` from the same component as objects
// `hasOwnProperty`. Since `hasOwnProperty` here will be from other component
// we override it to support our workaround.
module.exports = Object.create(tabs, {
  isPrototypeOf: { value: Object.prototype.isPrototypeOf }
});
