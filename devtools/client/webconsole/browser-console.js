/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Services = require("Services");
loader.lazyRequireGetter(this, "extend", "devtools/shared/extend", true);
loader.lazyRequireGetter(this, "Telemetry", "devtools/client/shared/telemetry");
loader.lazyRequireGetter(this, "WebConsole", "devtools/client/webconsole/webconsole");

// The preference prefix for all of the Browser Console filters.
const BC_FILTER_PREFS_PREFIX = "devtools.browserconsole.filter.";

/**
 * A BrowserConsole instance is an interactive console initialized *per target*
 * that displays console log data as well as provides an interactive terminal to
 * manipulate the target's document content.
 *
 * This object only wraps the iframe that holds the Browser Console UI. This is
 * meant to be an integration point between the Firefox UI and the Browser Console
 * UI and features.
 *
 * This object extends the WebConsole object located in webconsole.js
 *
 * @constructor
 * @param object target
 *        The target that the browser console will connect to.
 * @param nsIDOMWindow iframeWindow
 *        The window where the browser console UI is already loaded.
 * @param nsIDOMWindow chromeWindow
 *        The window of the browser console owner.
 * @param object hudService
 *        The parent HUD Service
 */
function BrowserConsole(target, iframeWindow, chromeWindow, hudService) {
  WebConsole.call(this, target, iframeWindow, chromeWindow, hudService);
  this._telemetry = new Telemetry();
}

BrowserConsole.prototype = extend(WebConsole.prototype, {
  _browserConsole: true,
  _bcInit: null,
  _bcDestroyer: null,

  $init: WebConsole.prototype.init,

  /**
   * Initialize the Browser Console instance.
   *
   * @return object
   *         A promise for the initialization.
   */
  init() {
    if (this._bcInit) {
      return this._bcInit;
    }

    // Only add the shutdown observer if we've opened a Browser Console window.
    ShutdownObserver.init(this.hudService);

    this.ui._filterPrefsPrefix = BC_FILTER_PREFS_PREFIX;

    const window = this.iframeWindow;

    // Make sure that the closing of the Browser Console window destroys this
    // instance.
    window.addEventListener("unload", () => {
      this.destroy();
    }, {once: true});

    this._telemetry.toolOpened("browserconsole");

    this._bcInit = this.$init();
    return this._bcInit;
  },

  $destroy: WebConsole.prototype.destroy,

  /**
   * Destroy the object.
   *
   * @return object
   *         A promise object that is resolved once the Browser Console is closed.
   */
  destroy() {
    if (this._bcDestroyer) {
      return this._bcDestroyer;
    }

    this._bcDestroyer = (async () => {
      this._telemetry.toolClosed("browserconsole");
      await this.$destroy();
      await this.target.client.close();
      this.hudService._browserConsoleID = null;
      this.chromeWindow.close();
    })();

    return this._bcDestroyer;
  },
});

/**
 * The ShutdownObserver listens for app shutdown and saves the current state
 * of the Browser Console for session restore.
 */
var ShutdownObserver = {
  _initialized: false,

  init(hudService) {
    if (this._initialized) {
      return;
    }

    Services.obs.addObserver(this, "quit-application-granted");

    this._initialized = true;
    this.hudService = hudService;
  },

  observe(message, topic) {
    if (topic == "quit-application-granted") {
      this.hudService.storeBrowserConsoleSessionState();
      this.uninit();
    }
  },

  uninit() {
    Services.obs.removeObserver(this, "quit-application-granted");
  }
};

module.exports = BrowserConsole;
