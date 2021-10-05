/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

const l10n = require("devtools/client/webconsole/utils/l10n");
const { safeAsyncMethod } = require("devtools/shared/async-utils");

const PREF_CONNECTION_TIMEOUT = "devtools.debugger.remote-timeout";
// Web Console connection proxy

/**
 * The WebConsoleConnectionProxy handles the connection between the Web Console
 * and the application we connect to through the remote debug protocol.
 */
class WebConsoleConnectionProxy {
  /**
   * @constructor
   * @param {WebConsoleUI} webConsoleUI
   *        A WebConsoleUI instance that owns this connection proxy.
   * @param {RemoteTarget} target
   *        The target that the console will connect to.
   */
  constructor(webConsoleUI, target) {
    this.webConsoleUI = webConsoleUI;
    this.target = target;
    this._connecter = null;

    this._onLastPrivateContextExited = this._onLastPrivateContextExited.bind(
      this
    );

    // Swallow async errors to _asyncConnect if the target was destroyed in the
    // meantime.
    this._asyncConnect = safeAsyncMethod(this._asyncConnect.bind(this), () =>
      this.target.isDestroyed()
    );
  }

  /**
   * Initialize a devtools client and connect it to the devtools server.
   *
   * @return object
   *         A promise object that is resolved/rejected based on the success of
   *         the connection initialization.
   */
  connect() {
    if (this._connecter) {
      return this._connecter;
    }

    if (this.target.isDestroyed()) {
      return Promise.reject("target was destroyed");
    }

    const connection = this._asyncConnect();

    let timeoutId;
    const connectionTimeout = new Promise((_, reject) => {
      timeoutId = setTimeout(() => {
        reject({
          error: "timeout",
          message: l10n.getStr("connectionTimeout"),
        });
      }, Services.prefs.getIntPref(PREF_CONNECTION_TIMEOUT));
    });

    // If the connectionTimeout rejects before the connection promise settles, then
    // this._connecter will reject with the same arguments connectionTimeout was
    // rejected with.
    this._connecter = Promise.race([connection, connectionTimeout]);

    // In case the connection was successful, cancel the setTimeout.
    connection.then(() => clearTimeout(timeoutId));

    return this._connecter;
  }

  async _asyncConnect() {
    this.webConsoleFront = await this.target.getFront("console");

    // @backward-compat { version 93 } 93 now supports setting this via the NetworkParent actor
    //                  Once we support only server watcher for NETWORK_EVENT,
    //                  we will be able to drop this in favor of code from WebConsoleUI._attachTargets.
    //                  We have to wait for the fully enabling of NETWORK_EVENT watchers, especially on the Browser Toolbox.
    //                  We can remove the trait check via hasTargetWatcherSupport once we drop support for 92.
    const { targetCommand, resourceCommand } = this.webConsoleUI.hud.commands;
    const hasNetworkResourceCommandSupport = resourceCommand.hasResourceCommandSupport(
      resourceCommand.TYPES.NETWORK_EVENT
    );
    const supportsWatcherRequest = targetCommand.hasTargetWatcherSupport(
      "saveRequestAndResponseBodies"
    );
    if (!hasNetworkResourceCommandSupport || !supportsWatcherRequest) {
      // There is no way to view response bodies from the Browser Console, so do
      // not waste the memory.
      const saveBodies =
        !this.webConsoleUI.isBrowserConsole &&
        Services.prefs.getBoolPref(
          "devtools.netmonitor.saveRequestAndResponseBodies"
        );
      await this.setSaveRequestAndResponseBodies(saveBodies);
    }

    this._addWebConsoleFrontEventListeners();
  }

  getConnectionPromise() {
    return this._connecter;
  }

  /**
   * Setter for saving of network request and response bodies.
   *
   * @param boolean value
   *        The new value you want to set.
   */
  async setSaveRequestAndResponseBodies(value) {
    if (!this.webConsoleFront) {
      // Don't continue if the webconsole disconnected.
      return null;
    }

    const newValue = !!value;
    const toSet = {
      "NetworkMonitor.saveRequestAndResponseBodies": newValue,
    };

    // Make sure the web console client connection is established first.
    return this.webConsoleFront.setPreferences(toSet);
  }

  /**
   * Sets all the relevant event listeners on the webconsole client.
   *
   * @private
   */
  _addWebConsoleFrontEventListeners() {
    if (!this.webConsoleFront) {
      return;
    }

    this.webConsoleFront.on(
      "lastPrivateContextExited",
      this._onLastPrivateContextExited
    );
  }

  /**
   * Remove all set event listeners on the webconsole client.
   *
   * @private
   */
  _removeWebConsoleFrontEventListeners() {
    this.webConsoleFront.off(
      "lastPrivateContextExited",
      this._onLastPrivateContextExited
    );
  }

  /**
   * The "lastPrivateContextExited" message type handler. When this message is
   * received the Web Console UI is cleared.
   *
   * @private
   * @param string type
   *        Message type.
   * @param object packet
   *        The message received from the server.
   */
  _onLastPrivateContextExited(packet) {
    if (this.webConsoleUI) {
      this.webConsoleUI.clearPrivateMessages();
    }
  }

  /**
   * Disconnect the Web Console from the remote server.
   *
   * @return object
   *         A promise object that is resolved when disconnect completes.
   */
  disconnect() {
    if (!this.webConsoleFront) {
      return;
    }

    this._removeWebConsoleFrontEventListeners();

    this.webConsoleFront = null;
  }
}

exports.WebConsoleConnectionProxy = WebConsoleConnectionProxy;
