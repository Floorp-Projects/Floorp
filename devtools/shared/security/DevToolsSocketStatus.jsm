/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Singleton that should be updated whenever a socket is opened or closed for
 * incoming connections.
 *
 * Notifies observers via "devtools-socket" when the status might have have
 * changed.
 *
 * Currently observed by browser/base/content/browser.js in order to display
 * the "remote control" visual cue also used for Marionette and Remote Agent.
 */
const DevToolsSocketStatus = {
  _browserToolboxSockets: 0,
  _otherSockets: 0,

  /**
   * Check if there are opened DevTools sockets.
   *
   * @param {Object=} options
   * @param {boolean=} options.excludeBrowserToolboxSockets
   *     Exclude sockets opened by local Browser Toolbox sessions. Defaults to
   *     false.
   *
   * @return {boolean}
   *     Returns true if there are DevTools sockets opened and matching the
   *     provided options if any.
   */
  hasSocketOpened(options = {}) {
    const { excludeBrowserToolboxSockets = false } = options;
    if (excludeBrowserToolboxSockets) {
      return this._otherSockets > 0;
    }
    return this._browserToolboxSockets + this._otherSockets > 0;
  },

  notifySocketOpened(options) {
    const { fromBrowserToolbox } = options;
    if (fromBrowserToolbox) {
      this._browserToolboxSockets++;
    } else {
      this._otherSockets++;
    }

    Services.obs.notifyObservers(this, "devtools-socket");
  },

  notifySocketClosed(options) {
    const { fromBrowserToolbox } = options;
    if (fromBrowserToolbox) {
      this._browserToolboxSockets--;
    } else {
      this._otherSockets--;
    }

    Services.obs.notifyObservers(this, "devtools-socket");
  },
};

var EXPORTED_SYMBOLS = ["DevToolsSocketStatus"];
