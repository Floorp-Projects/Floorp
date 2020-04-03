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

    this._onPageError = this._onPageError.bind(this);
    this._onConsoleAPICall = this._onConsoleAPICall.bind(this);
    this._onLogMessage = this._onLogMessage.bind(this);
    this._onNetworkEvent = this._onNetworkEvent.bind(this);
    this._onNetworkEventUpdate = this._onNetworkEventUpdate.bind(this);
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

      const cachedMessages = await this._getCachedMessages();
      const networkMessages = this._getNetworkMessages();
      const messages = cachedMessages.concat(networkMessages);
      messages.sort((a, b) => a.timeStamp - b.timeStamp);
      this.dispatchMessagesAdd(messages);

      this._addWebConsoleFrontEventListeners();

      if (!this.webConsoleFront.hasNativeConsoleAPI) {
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

  /**
   * Attach to the Web Console actor.
   * @private
   * @returns Promise
   */
  _attachConsole() {
    const listeners = ["PageError", "ConsoleAPI", "NetworkActivity"];
    // Enable the forwarding of console messages to the parent process
    // when we open the Browser Console or Toolbox without fission support. If Fission
    // is enabled, we don't use the ContentProcessMessages listener, but attach to the
    // content processes directly.
    if (this.needContentProcessMessagesListener) {
      listeners.push("ContentProcessMessages");
    }
    return this.webConsoleFront.startListeners(listeners);
  }

  /**
   * Sets all the relevant event listeners on the webconsole client.
   *
   * @private
   */
  _addWebConsoleFrontEventListeners() {
    this.webConsoleFront.on("networkEvent", this._onNetworkEvent);
    this.webConsoleFront.on("networkEventUpdate", this._onNetworkEventUpdate);
    this.webConsoleFront.on("logMessage", this._onLogMessage);
    this.webConsoleFront.on("pageError", this._onPageError);
    this.webConsoleFront.on("consoleAPICall", this._onConsoleAPICall);
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
    this.webConsoleFront.off("networkEvent", this._onNetworkEvent);
    this.webConsoleFront.off("networkEventUpdate", this._onNetworkEventUpdate);
    this.webConsoleFront.off("logMessage", this._onLogMessage);
    this.webConsoleFront.off("pageError", this._onPageError);
    this.webConsoleFront.off("consoleAPICall", this._onConsoleAPICall);
    this.webConsoleFront.off(
      "lastPrivateContextExited",
      this._onLastPrivateContextExited
    );
    this.webConsoleFront.off(
      "clearLogpointMessages",
      this._clearLogpointMessages
    );
  }

  /**
   * Get cached messages from the server.
   *
   * @private
   * @returns A Promise that resolves with the cached messages, or reject if something
   *          went wront.
   */
  async _getCachedMessages() {
    const response = await this.webConsoleFront.getCachedMessages([
      "PageError",
      "ConsoleAPI",
    ]);

    if (response.error) {
      throw new Error(
        `Web Console getCachedMessages error: ${response.error} ${response.message}`
      );
    }

    return response.messages;
  }

  /**
   * Get network messages from the server.
   *
   * @private
   * @returns An array of network messages.
   */
  _getNetworkMessages() {
    return Array.from(this.webConsoleFront.getNetworkEvents());
  }

  /**
   * The "pageError" message type handler. We redirect any page errors to the UI
   * for displaying.
   *
   * @private
   * @param object packet
   *        The message received from the server.
   */
  _onPageError(packet) {
    if (!this.webConsoleUI) {
      return;
    }
    packet.type = "pageError";
    this.dispatchMessageAdd(packet);
  }

  /**
   * The "logMessage" message type handler. We redirect any message to the UI
   * for displaying.
   *
   * @private
   * @param object packet
   *        The message received from the server.
   */
  _onLogMessage(packet) {
    if (!this.webConsoleUI) {
      return;
    }
    packet.type = "logMessage";
    this.dispatchMessageAdd(packet);
  }

  /**
   * The "consoleAPICall" message type handler. We redirect any message to
   * the UI for displaying.
   *
   * @private
   * @param string type
   *        Message type.
   * @param object packet
   *        The message received from the server.
   */
  _onConsoleAPICall(packet) {
    if (!this.webConsoleUI) {
      return;
    }
    packet.type = "consoleAPICall";
    this.dispatchMessageAdd(packet);
  }

  _clearLogpointMessages(logpointId) {
    if (this.webConsoleUI) {
      this.webConsoleUI.wrapper.dispatchClearLogpointMessages(logpointId);
    }
  }

  /**
   * The "networkEvent" message type handler. We redirect any message to
   * the UI for displaying.
   *
   * @private
   * @param object networkInfo
   *        The network request information.
   */
  _onNetworkEvent(networkInfo) {
    if (!this.webConsoleUI) {
      return;
    }
    this.dispatchMessageAdd(networkInfo);
  }

  /**
   * The "networkEventUpdate" message type handler. We redirect any message to
   * the UI for displaying.
   *
   * @private
   * @param object response
   *        The update response received from the server.
   */
  _onNetworkEventUpdate(response) {
    if (!this.webConsoleUI) {
      return;
    }
    this.dispatchMessageUpdate(response.networkInfo, response);
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
    this.webConsoleUI.handleTabWillNavigate(packet);
  }

  /**
   * Dispatch a message add on the new frontend and emit an event for tests.
   */
  dispatchMessageAdd(packet) {
    this.webConsoleUI.wrapper.dispatchMessageAdd(packet);
  }

  /**
   * Batched dispatch of messages.
   */
  dispatchMessagesAdd(packets) {
    this.webConsoleUI.wrapper.dispatchMessagesAdd(packets);
  }

  /**
   * Dispatch a message event on the new frontend and emit an event for tests.
   */
  dispatchMessageUpdate(networkInfo, response) {
    // Some message might try to update while we are closing the toolbox.
    if (!this.webConsoleUI) {
      return;
    }
    this.webConsoleUI.wrapper.dispatchMessageUpdate(networkInfo, response);
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
