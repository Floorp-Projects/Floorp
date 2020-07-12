/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

const l10n = require("devtools/client/webconsole/utils/l10n");

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
   * @param {Boolean} needContentProcessMessagesListener
   *        Set to true to specifically add a ContentProcessMessages listener. This is
   *        needed for non-fission Browser Console for example.
   */
  constructor(
    webConsoleUI,
    target,
    needContentProcessMessagesListener = false
  ) {
    this.webConsoleUI = webConsoleUI;
    this.target = target;
    this.needContentProcessMessagesListener = needContentProcessMessagesListener;
    this._connecter = null;

    this._onTabNavigated = this._onTabNavigated.bind(this);
    this._onTabWillNavigate = this._onTabWillNavigate.bind(this);
    this._onLastPrivateContextExited = this._onLastPrivateContextExited.bind(
      this
    );
    this._clearLogpointMessages = this._clearLogpointMessages.bind(this);
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

    if (!this.target.client) {
      return Promise.reject("target was destroyed");
    }

    this.target.on("will-navigate", this._onTabWillNavigate);
    this.target.on("navigate", this._onTabNavigated);

    const connection = (async () => {
      this.client = this.target.client;
      this.webConsoleFront = await this.target.getFront("console");

      await this._attachConsole();

      // There is no way to view response bodies from the Browser Console, so do
      // not waste the memory.
      const saveBodies =
        !this.webConsoleUI.isBrowserConsole &&
        Services.prefs.getBoolPref(
          "devtools.netmonitor.saveRequestAndResponseBodies"
        );
      await this.webConsoleUI.setSaveRequestAndResponseBodies(saveBodies);

      this._addWebConsoleFrontEventListeners();

      if (this.webConsoleFront && !this.webConsoleFront.hasNativeConsoleAPI) {
        await this.webConsoleUI.logWarningAboutReplacedAPI();
      }
    })();

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

  getConnectionPromise() {
    return this._connecter;
  }

  /**
   * Attach to the Web Console actor.
   * @private
   * @returns Promise
   */
  _attachConsole() {
    if (!this.webConsoleFront || !this.needContentProcessMessagesListener) {
      return null;
    }

    // Enable the forwarding of console messages to the parent process
    // when we open the Browser Console or Toolbox without fission support. If Fission
    // is enabled, we don't use the ContentProcessMessages listener, but attach to the
    // content processes directly.
    return this.webConsoleFront.startListeners(["ContentProcessMessages"]);
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
    this.webConsoleFront.on(
      "clearLogpointMessages",
      this._clearLogpointMessages
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
    this.webConsoleFront.off(
      "clearLogpointMessages",
      this._clearLogpointMessages
    );
  }

  _clearLogpointMessages(logpointId) {
    // Some message might try to update while we are closing the toolbox.
    if (this.webConsoleUI?.wrapper) {
      this.webConsoleUI.wrapper.dispatchClearLogpointMessages(logpointId);
    }
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
   * The "navigate" event handlers. We redirect any message to the UI for displaying.
   *
   * @private
   * @param object packet
   *        The message received from the server.
   */
  _onTabNavigated(packet) {
    // Some message might try to update while we are closing the toolbox.
    if (!this.webConsoleUI) {
      return;
    }
    this.webConsoleUI.handleTabNavigated(packet);
  }

  /**
   * The "will-navigate" event handlers. We redirect any message to the UI for displaying.
   *
   * @private
   * @param object packet
   *        The message received from the server.
   */
  _onTabWillNavigate(packet) {
    // Some message might try to update while we are closing the toolbox.
    if (!this.webConsoleUI) {
      return;
    }
    this.webConsoleUI.handleTabWillNavigate(packet);
  }

  /**
   * Disconnect the Web Console from the remote server.
   *
   * @return object
   *         A promise object that is resolved when disconnect completes.
   */
  disconnect() {
    if (!this.client) {
      return;
    }

    this._removeWebConsoleFrontEventListeners();
    this.target.off("will-navigate", this._onTabWillNavigate);
    this.target.off("navigate", this._onTabNavigated);

    this.client = null;
    this.webConsoleFront = null;
  }
}

exports.WebConsoleConnectionProxy = WebConsoleConnectionProxy;
