/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");

/**
 * A WebProgressListener that listens for file loads.
 *
 * @constructor
 * @param object window
 *        The window for which we need to track file loads.
 * @param object owner
 *        The listener owner which needs to implement:
 *        - onFileActivity(aFileURI)
 */
function ConsoleFileActivityListener(window, owner) {
  this.window = window;
  this.owner = owner;
}
exports.ConsoleFileActivityListener = ConsoleFileActivityListener;

ConsoleFileActivityListener.prototype = {
  /**
   * Tells if the console progress listener is initialized or not.
   * @private
   * @type boolean
   */
  _initialized: false,

  _webProgress: null,

  QueryInterface: ChromeUtils.generateQI([
    "nsIWebProgressListener",
    "nsISupportsWeakReference",
  ]),

  /**
   * Initialize the ConsoleFileActivityListener.
   * @private
   */
  _init() {
    if (this._initialized) {
      return;
    }

    this._webProgress = this.window.docShell.QueryInterface(Ci.nsIWebProgress);
    this._webProgress.addProgressListener(
      this,
      Ci.nsIWebProgress.NOTIFY_STATE_ALL
    );

    this._initialized = true;
  },

  /**
   * Start a monitor/tracker related to the current nsIWebProgressListener
   * instance.
   */
  startMonitor() {
    this._init();
  },

  /**
   * Stop monitoring.
   */
  stopMonitor() {
    this.destroy();
  },

  onStateChange(progress, request, state, status) {
    if (!this.owner) {
      return;
    }

    this._checkFileActivity(progress, request, state, status);
  },

  /**
   * Check if there is any file load, given the arguments of
   * nsIWebProgressListener.onStateChange. If the state change tells that a file
   * URI has been loaded, then the remote Web Console instance is notified.
   * @private
   */
  _checkFileActivity(progress, request, state, status) {
    if (!(state & Ci.nsIWebProgressListener.STATE_START)) {
      return;
    }

    let uri = null;
    if (request instanceof Ci.imgIRequest) {
      const imgIRequest = request.QueryInterface(Ci.imgIRequest);
      uri = imgIRequest.URI;
    } else if (request instanceof Ci.nsIChannel) {
      const nsIChannel = request.QueryInterface(Ci.nsIChannel);
      uri = nsIChannel.URI;
    }

    if (!uri || (!uri.schemeIs("file") && !uri.schemeIs("ftp"))) {
      return;
    }

    this.owner.onFileActivity(uri.spec);
  },

  /**
   * Destroy the ConsoleFileActivityListener.
   */
  destroy() {
    if (!this._initialized) {
      return;
    }

    this._initialized = false;

    try {
      this._webProgress.removeProgressListener(this);
    } catch (ex) {
      // This can throw during browser shutdown.
    }

    this._webProgress = null;
    this.window = null;
    this.owner = null;
  },
};
