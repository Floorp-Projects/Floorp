/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { Class } = require('../core/heritage');
const { BrowserWindow } = require('../window/browser');
const { WindowTracker } = require('../deprecated/window-utils');
const { isBrowser, getMostRecentBrowserWindow } = require('../window/utils');
const { windowNS } = require('../window/namespace');
const { on, off, once, emit } = require('../event/core');
const { method } = require('../lang/functional');
const { EventTarget } = require('../event/target');
const { List, addListItem } = require('../util/list');

const ERR_FENNEC_MSG = 'This method is not yet supported by Fennec, consider using require("tabs") instead';

// NOTE: On Fennec there is only one window.

let BrowserWindows = Class({
  implements: [ List ],
  extends: EventTarget,
  initialize: function() {
    List.prototype.initialize.apply(this);
  },
  get activeWindow() {
    let window = getMostRecentBrowserWindow();
    return window ? getBrowserWindow({window: window}) : null;
  },
  open: function open(options) {
    throw new Error(ERR_FENNEC_MSG);
    return null;
  }
});
const browserWindows = exports.browserWindows = BrowserWindows();


/**
 * Gets a `BrowserWindow` for the given `chromeWindow` if previously
 * registered, `null` otherwise.
 */
function getRegisteredWindow(chromeWindow) {
  for each (let window in browserWindows) {
    if (chromeWindow === windowNS(window).window)
      return window;
  }

  return null;
}

/**
 * Gets a `BrowserWindow` for the provided window options obj
 * @params {Object} options
 *    Options that are passed to the the `BrowserWindowTrait`
 * @returns {BrowserWindow}
 */
function getBrowserWindow(options) {
  let window = null;

  // if we have a BrowserWindow already then use it
  if ('window' in options)
    window = getRegisteredWindow(options.window);
  if (window)
    return window;

  // we don't have a BrowserWindow yet, so create one
  var window = BrowserWindow(options);
  addListItem(browserWindows, window);
  return window;
}

WindowTracker({
  onTrack: function onTrack(chromeWindow) {
    if (!isBrowser(chromeWindow)) return;
    let window = getBrowserWindow({ window: chromeWindow });
    emit(browserWindows, 'open', window);
  },
  onUntrack: function onUntrack(chromeWindow) {
    if (!isBrowser(chromeWindow)) return;
    let window = getBrowserWindow({ window: chromeWindow });
    emit(browserWindows, 'close', window);
  }
});
