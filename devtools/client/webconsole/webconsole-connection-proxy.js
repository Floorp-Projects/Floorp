/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const defer = require("devtools/shared/defer");
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
   * @param RemoteTarget target
   *        The target that the console will connect to.
   */
  constructor(webConsoleUI, target, isBrowserConsole, fissionSupport) {
    this.webConsoleUI = webConsoleUI;
    this.target = target;
    this.webConsoleClient = target.activeConsole;
    this.isBrowserConsole = isBrowserConsole;
    this.fissionSupport = fissionSupport;

    /**
     * The DebuggerClient object.
     *
     * @see DebuggerClient
     * @type object
     */
    this.client = null;

    /**
     * Tells if the connection is established.
     * @type boolean
     */
    this.connected = false;

    /**
     * Timer used for the connection.
     * @private
     * @type object
     */
    this._connectTimer = null;

    this._connectDefer = null;
    this._disconnecter = null;

    this._onPageError = this._onPageError.bind(this);
    this._onLogMessage = this._onLogMessage.bind(this);
    this._onConsoleAPICall = this._onConsoleAPICall.bind(this);
    this._onNetworkEvent = this._onNetworkEvent.bind(this);
    this._onNetworkEventUpdate = this._onNetworkEventUpdate.bind(this);
    this._onTabNavigated = this._onTabNavigated.bind(this);
    this._onTabWillNavigate = this._onTabWillNavigate.bind(this);
    this._onAttachConsole = this._onAttachConsole.bind(this);
    this._onCachedMessages = this._onCachedMessages.bind(this);
    this._connectionTimeout = this._connectionTimeout.bind(this);
    this._onLastPrivateContextExited = this._onLastPrivateContextExited.bind(
      this
    );
    this._clearLogpointMessages = this._clearLogpointMessages.bind(this);
  }

  /**
   * Initialize a debugger client and connect it to the debugger server.
   *
   * @return object
   *         A promise object that is resolved/rejected based on the success of
   *         the connection initialization.
   */
  connect() {
    if (this._connectDefer) {
      return this._connectDefer.promise;
    }

    this._connectDefer = defer();

    const timeout = Services.prefs.getIntPref(PREF_CONNECTION_TIMEOUT);
    this._connectTimer = setTimeout(this._connectionTimeout, timeout);

    const connPromise = this._connectDefer.promise;
    connPromise.then(
      () => {
        clearTimeout(this._connectTimer);
        this._connectTimer = null;
      },
      () => {
        clearTimeout(this._connectTimer);
        this._connectTimer = null;
      }
    );
    this.client = this.target.client;

    this.target.on("will-navigate", this._onTabWillNavigate);
    this.target.on("navigate", this._onTabNavigated);

    this._attachConsole();

    return connPromise;
  }

  /**
   * Connection timeout handler.
   * @private
   */
  _connectionTimeout() {
    const error = {
      error: "timeout",
      message: l10n.getStr("connectionTimeout"),
    };

    this._connectDefer.reject(error);
  }

  /**
   * Attach to the Web Console actor.
   * @private
   * @returns Promise
   */
  _attachConsole() {
    const listeners = ["PageError", "ConsoleAPI", "NetworkActivity"];
    // Enable the forwarding of console messages to the parent process
    // when we open the Browser Console or Toolbox.
    if (this.target.chrome && !this.target.isAddon) {
      listeners.push("ContentProcessMessages");
    }
    return this.webConsoleClient
      .startListeners(listeners)
      .then(this._onAttachConsole, error => {
        console.error("attachConsole failed: " + error);
        this._connectDefer.reject(error);
      });
  }

  /**
   * The "attachConsole" response handler.
   *
   * @private
   * @param object response
   *        The JSON response object received from the server.
   * @param object webConsoleClient
   *        The WebConsoleClient instance for the attached console, for the
   *        specific tab we work with.
   */
  async _onAttachConsole(response) {
    let saveBodies = Services.prefs.getBoolPref(
      "devtools.netmonitor.saveRequestAndResponseBodies"
    );

    // There is no way to view response bodies from the Browser Console, so do
    // not waste the memory.
    if (this.webConsoleUI.isBrowserConsole) {
      saveBodies = false;
    }

    this.webConsoleUI.setSaveRequestAndResponseBodies(saveBodies);

    this.webConsoleClient.on("networkEvent", this._onNetworkEvent);
    this.webConsoleClient.on("networkEventUpdate", this._onNetworkEventUpdate);
    this.webConsoleClient.on("logMessage", this._onLogMessage);
    this.webConsoleClient.on("pageError", this._onPageError);
    this.webConsoleClient.on("consoleAPICall", this._onConsoleAPICall);
    this.webConsoleClient.on(
      "lastPrivateContextExited",
      this._onLastPrivateContextExited
    );
    this.webConsoleClient.on(
      "clearLogpointMessages",
      this._clearLogpointMessages
    );

    const msgs = ["PageError", "ConsoleAPI"];
    const cachedMessages = await this.webConsoleClient.getCachedMessages(msgs);
    this._onCachedMessages(cachedMessages);
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

  dispatchRequestUpdate(id, data) {
    // Some request might try to update while we are closing the toolbox.
    if (!this.webConsoleUI) {
      return Promise.resolve();
    }

    return this.webConsoleUI.wrapper.dispatchRequestUpdate(id, data);
  }

  /**
   * The "cachedMessages" response handler.
   *
   * @private
   * @param object response
   *        The JSON response object received from the server.
   */
  async _onCachedMessages(response) {
    if (response.error) {
      console.error(
        "Web Console getCachedMessages error: " +
          response.error +
          " " +
          response.message
      );
      this._connectDefer.reject(response);
      return;
    }

    if (!this._connectTimer) {
      // This happens if the promise is rejected (eg. a timeout), but the
      // connection attempt is successful, nonetheless.
      console.error("Web Console getCachedMessages error: invalid state.");
    }

    const messages = response.messages.concat(
      ...this.webConsoleClient.getNetworkEvents()
    );
    messages.sort((a, b) => a.timeStamp - b.timeStamp);

    this.dispatchMessagesAdd(messages);
    if (!this.webConsoleClient.hasNativeConsoleAPI) {
      await this.webConsoleUI.logWarningAboutReplacedAPI();
    }

    this.connected = true;
    this._connectDefer.resolve(this);
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
   * Release an object actor.
   *
   * @param string actor
   *        The actor ID to send the request to.
   */
  releaseActor(actor) {
    if (this.client) {
      this.client.release(actor).catch(() => {});
    }
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

    this.webConsoleClient.off("logMessage", this._onLogMessage);
    this.webConsoleClient.off("pageError", this._onPageError);
    this.webConsoleClient.off("consoleAPICall", this._onConsoleAPICall);
    this.webConsoleClient.off(
      "lastPrivateContextExited",
      this._onLastPrivateContextExited
    );
    this.webConsoleClient.off("networkEvent", this._onNetworkEvent);
    this.webConsoleClient.off("networkEventUpdate", this._onNetworkEventUpdate);
    this.webConsoleClient.off(
      "clearLogpointMessages",
      this._clearLogpointMessages
    );
    this.target.off("will-navigate", this._onTabWillNavigate);
    this.target.off("navigate", this._onTabNavigated);

    this.client = null;
    this.webConsoleClient = null;
    this.target = null;
    this.connected = false;
    this.webConsoleUI = null;
  }
}

exports.WebConsoleConnectionProxy = WebConsoleConnectionProxy;
