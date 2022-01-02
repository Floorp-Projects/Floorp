/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

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
  _openedSockets: 0,

  notifySocketOpened() {
    this._openedSockets++;
    Services.obs.notifyObservers(this, "devtools-socket");
  },

  notifySocketClosed() {
    this._openedSockets--;
    Services.obs.notifyObservers(this, "devtools-socket");
  },

  /**
   * Returns true if any socket is currently opened for connections.
   */
  get opened() {
    return this._openedSockets > 0;
  },
};

var EXPORTED_SYMBOLS = ["DevToolsSocketStatus"];
