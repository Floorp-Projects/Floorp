/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const ChromeUtils = require("ChromeUtils");

/**
 * A WebProgressListener that listens for location changes.
 *
 * This progress listener is used to track file loads and other kinds of
 * location changes.
 *
 * @constructor
 * @param object window
 *        The window for which we need to track location changes.
 * @param object owner
 *        The listener owner which needs to implement two methods:
 *        - onFileActivity(aFileURI)
 *        - onLocationChange(aState, aTabURI, aPageTitle)
 */
function ConsoleProgressListener(window, owner) {
  this.window = window;
  this.owner = owner;
}
exports.ConsoleProgressListener = ConsoleProgressListener;

ConsoleProgressListener.prototype = {
  /**
   * Constant used for startMonitor()/stopMonitor() that tells you want to
   * monitor file loads.
   */
  MONITOR_FILE_ACTIVITY: 1,

  /**
   * Constant used for startMonitor()/stopMonitor() that tells you want to
   * monitor page location changes.
   */
  MONITOR_LOCATION_CHANGE: 2,

  /**
   * Tells if you want to monitor file activity.
   * @private
   * @type boolean
   */
  _fileActivity: false,

  /**
   * Tells if you want to monitor location changes.
   * @private
   * @type boolean
   */
  _locationChange: false,

  /**
   * Tells if the console progress listener is initialized or not.
   * @private
   * @type boolean
   */
  _initialized: false,

  _webProgress: null,

  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIWebProgressListener,
    Ci.nsISupportsWeakReference,
  ]),

  /**
   * Initialize the ConsoleProgressListener.
   * @private
   */
  _init: function() {
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
   *
   * @param number monitor
   *        Tells what you want to track. Available constants:
   *        - this.MONITOR_FILE_ACTIVITY
   *          Track file loads.
   *        - this.MONITOR_LOCATION_CHANGE
   *          Track location changes for the top window.
   */
  startMonitor: function(monitor) {
    switch (monitor) {
      case this.MONITOR_FILE_ACTIVITY:
        this._fileActivity = true;
        break;
      case this.MONITOR_LOCATION_CHANGE:
        this._locationChange = true;
        break;
      default:
        throw new Error(
          "ConsoleProgressListener: unknown monitor type " + monitor + "!"
        );
    }
    this._init();
  },

  /**
   * Stop a monitor.
   *
   * @param number monitor
   *        Tells what you want to stop tracking. See this.startMonitor() for
   *        the list of constants.
   */
  stopMonitor: function(monitor) {
    switch (monitor) {
      case this.MONITOR_FILE_ACTIVITY:
        this._fileActivity = false;
        break;
      case this.MONITOR_LOCATION_CHANGE:
        this._locationChange = false;
        break;
      default:
        throw new Error(
          "ConsoleProgressListener: unknown monitor type " + monitor + "!"
        );
    }

    if (!this._fileActivity && !this._locationChange) {
      this.destroy();
    }
  },

  onStateChange: function(progress, request, state, status) {
    if (!this.owner) {
      return;
    }

    if (this._fileActivity) {
      this._checkFileActivity(progress, request, state, status);
    }

    if (this._locationChange) {
      this._checkLocationChange(progress, request, state, status);
    }
  },

  /**
   * Check if there is any file load, given the arguments of
   * nsIWebProgressListener.onStateChange. If the state change tells that a file
   * URI has been loaded, then the remote Web Console instance is notified.
   * @private
   */
  _checkFileActivity: function(progress, request, state, status) {
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
   * Check if the current window.top location is changing, given the arguments
   * of nsIWebProgressListener.onStateChange. If that is the case, the remote
   * Web Console instance is notified.
   * @private
   */
  _checkLocationChange: function(progress, request, state) {
    const isStart = state & Ci.nsIWebProgressListener.STATE_START;
    const isStop = state & Ci.nsIWebProgressListener.STATE_STOP;
    const isNetwork = state & Ci.nsIWebProgressListener.STATE_IS_NETWORK;
    const isWindow = state & Ci.nsIWebProgressListener.STATE_IS_WINDOW;

    // Skip non-interesting states.
    if (!isNetwork || !isWindow || progress.DOMWindow != this.window) {
      return;
    }

    if (isStart && request instanceof Ci.nsIChannel) {
      this.owner.onLocationChange("start", request.URI.spec, "");
    } else if (isStop) {
      this.owner.onLocationChange(
        "stop",
        this.window.location.href,
        this.window.document.title
      );
    }
  },

  /**
   * Destroy the ConsoleProgressListener.
   */
  destroy: function() {
    if (!this._initialized) {
      return;
    }

    this._initialized = false;
    this._fileActivity = false;
    this._locationChange = false;

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
