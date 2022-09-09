/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const WebConsole = require("devtools/client/webconsole/webconsole");
const { Utils } = require("devtools/client/webconsole/utils");

loader.lazyRequireGetter(this, "Telemetry", "devtools/client/shared/telemetry");
loader.lazyRequireGetter(
  this,
  "BrowserConsoleManager",
  "devtools/client/webconsole/browser-console-manager",
  true
);

/**
 * A BrowserConsole instance is an interactive console initialized *per commands*
 * that displays console log data as well as provides an interactive terminal to
 * manipulate all browser debuggable context and targeted by default at the current
 * top-level window's document.
 *
 * This object only wraps the iframe that holds the Browser Console UI. This is
 * meant to be an integration point between the Firefox UI and the Browser Console
 * UI and features.
 *
 * This object extends the WebConsole object located in webconsole.js
 */
class BrowserConsole extends WebConsole {
  #bcInitializer = null;
  #bcDestroyer = null;
  #telemetry;
  /*
   * @constructor
   * @param object commands
   *        The commands object with all interfaces defined from devtools/shared/commands/
   * @param nsIDOMWindow iframeWindow
   *        The window where the browser console UI is already loaded.
   * @param nsIDOMWindow chromeWindow
   *        The window of the browser console owner.
   */
  constructor(commands, iframeWindow, chromeWindow) {
    super(null, commands, iframeWindow, chromeWindow, true);

    this.#telemetry = new Telemetry();
  }

  /**
   * Initialize the Browser Console instance.
   *
   * @return object
   *         A promise for the initialization.
   */
  init() {
    if (this.#bcInitializer) {
      return this.#bcInitializer;
    }

    this.#bcInitializer = (async () => {
      // Only add the shutdown observer if we've opened a Browser Console window.
      ShutdownObserver.init();

      // browserconsole is not connected with a toolbox so we pass -1 as the
      // toolbox session id.
      this.#telemetry.toolOpened("browserconsole", -1, this);

      await super.init(false);

      // Reports the console as created only after everything is done,
      // including TargetCommand.startListening.
      const id = Utils.supportsString(this.hudId);
      Services.obs.notifyObservers(id, "web-console-created");
    })();
    return this.#bcInitializer;
  }

  /**
   * Destroy the object.
   *
   * @return object
   *         A promise object that is resolved once the Browser Console is closed.
   */
  destroy() {
    if (this.#bcDestroyer) {
      return this.#bcDestroyer;
    }

    this.#bcDestroyer = (async () => {
      // browserconsole is not connected with a toolbox so we pass -1 as the
      // toolbox session id.
      this.#telemetry.toolClosed("browserconsole", -1, this);

      this.commands.targetCommand.destroy();
      await super.destroy();
      await this.currentTarget.destroy();
      this.chromeWindow.close();
    })();

    return this.#bcDestroyer;
  }

  updateWindowTitle() {
    BrowserConsoleManager.updateWindowTitle(this.chromeWindow);
  }
}

/**
 * The ShutdownObserver listens for app shutdown and saves the current state
 * of the Browser Console for session restore.
 */
var ShutdownObserver = {
  _initialized: false,

  init() {
    if (this._initialized) {
      return;
    }

    Services.obs.addObserver(this, "quit-application-granted");

    this._initialized = true;
  },

  observe(message, topic) {
    if (topic == "quit-application-granted") {
      BrowserConsoleManager.storeBrowserConsoleSessionState();
      this.uninit();
    }
  },

  uninit() {
    Services.obs.removeObserver(this, "quit-application-granted");
  },
};

module.exports = BrowserConsole;
