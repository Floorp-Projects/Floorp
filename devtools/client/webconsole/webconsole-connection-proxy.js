/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const defer = require("devtools/shared/defer");
const Services = require("Services");

const l10n = require("devtools/client/webconsole/webconsole-l10n");

const PREF_CONNECTION_TIMEOUT = "devtools.debugger.remote-timeout";
// Web Console connection proxy

/**
 * The WebConsoleConnectionProxy handles the connection between the Web Console
 * and the application we connect to through the remote debug protocol.
 *
 * @constructor
 * @param object webConsoleFrame
 *        The WebConsoleFrame object that owns this connection proxy.
 * @param RemoteTarget target
 *        The target that the console will connect to.
 */
function WebConsoleConnectionProxy(webConsoleFrame, target) {
  this.webConsoleFrame = webConsoleFrame;
  this.target = target;
  this.webConsoleClient = target.activeConsole;

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
  this._onLastPrivateContextExited =
    this._onLastPrivateContextExited.bind(this);
}

WebConsoleConnectionProxy.prototype = {
  /**
   * The owning Web Console Frame instance.
   *
   * @see WebConsoleFrame
   * @type object
   */
  webConsoleFrame: null,

  /**
   * The target that the console connects to.
   * @type RemoteTarget
   */
  target: null,

  /**
   * The DebuggerClient object.
   *
   * @see DebuggerClient
   * @type object
   */
  client: null,

  /**
   * The WebConsoleClient object.
   *
   * @see WebConsoleClient
   * @type object
   */
  webConsoleClient: null,

  /**
   * Tells if the connection is established.
   * @type boolean
   */
  connected: false,

  /**
   * Tells if the console is attached.
   * @type boolean
   */
  isAttached: null,

  /**
   * Timer used for the connection.
   * @private
   * @type object
   */
  _connectTimer: null,

  _connectDefer: null,
  _disconnecter: null,

  /**
   * Initialize a debugger client and connect it to the debugger server.
   *
   * @return object
   *         A promise object that is resolved/rejected based on the success of
   *         the connection initialization.
   */
  connect: function() {
    if (this._connectDefer) {
      return this._connectDefer.promise;
    }

    this._connectDefer = defer();

    const timeout = Services.prefs.getIntPref(PREF_CONNECTION_TIMEOUT);
    this._connectTimer = setTimeout(this._connectionTimeout, timeout);

    const connPromise = this._connectDefer.promise;
    connPromise.then(() => {
      clearTimeout(this._connectTimer);
      this._connectTimer = null;
    }, () => {
      clearTimeout(this._connectTimer);
      this._connectTimer = null;
    });

    const client = this.client = this.target.client;

    client.addListener("logMessage", this._onLogMessage);
    client.addListener("pageError", this._onPageError);
    client.addListener("consoleAPICall", this._onConsoleAPICall);
    client.addListener("lastPrivateContextExited",
                       this._onLastPrivateContextExited);

    this.target.on("will-navigate", this._onTabWillNavigate);
    this.target.on("navigate", this._onTabNavigated);

    if (this.target.isBrowsingContext) {
      this.webConsoleFrame.onLocationChange(this.target.url, this.target.title);
    }
    this.isAttached = this._attachConsole();

    return connPromise;
  },

  /**
   * Connection timeout handler.
   * @private
   */
  _connectionTimeout: function() {
    const error = {
      error: "timeout",
      message: l10n.getStr("connectionTimeout"),
    };

    this._connectDefer.reject(error);
  },

  /**
   * Attach to the Web Console actor.
   * @private
   * @returns Promise
   */
  _attachConsole: function() {
    const listeners = ["PageError", "ConsoleAPI", "NetworkActivity"];
    // Enable the forwarding of console messages to the parent process
    // when we open the Browser Console or Toolbox.
    if (this.target.chrome && !this.target.isAddon) {
      listeners.push("ContentProcessMessages");
    }
    return this.webConsoleClient.startListeners(listeners)
      .then(this._onAttachConsole, error => {
        console.error("attachConsole failed: " + error);
        this._connectDefer.reject(error);
      });
  },

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
  _onAttachConsole: async function(response) {
    let saveBodies = Services.prefs.getBoolPref(
      "devtools.netmonitor.saveRequestAndResponseBodies");

    // There is no way to view response bodies from the Browser Console, so do
    // not waste the memory.
    if (this.webConsoleFrame.isBrowserConsole) {
      saveBodies = false;
    }

    this.webConsoleFrame.setSaveRequestAndResponseBodies(saveBodies);

    this.webConsoleClient.on("networkEvent", this._onNetworkEvent);
    this.webConsoleClient.on("networkEventUpdate", this._onNetworkEventUpdate);

    const msgs = ["PageError", "ConsoleAPI"];
    const cachedMessages = await this.webConsoleClient.getCachedMessages(msgs);
    this._onCachedMessages(cachedMessages);

    this.webConsoleFrame._onUpdateListeners();
  },

  /**
   * Dispatch a message add on the new frontend and emit an event for tests.
   */
  dispatchMessageAdd: function(packet) {
    this.webConsoleFrame.consoleOutput.dispatchMessageAdd(packet);
  },

  /**
   * Batched dispatch of messages.
   */
  dispatchMessagesAdd: function(packets) {
    this.webConsoleFrame.consoleOutput.dispatchMessagesAdd(packets);
  },

  /**
   * Dispatch a message event on the new frontend and emit an event for tests.
   */
  dispatchMessageUpdate: function(networkInfo, response) {
    this.webConsoleFrame.consoleOutput.dispatchMessageUpdate(networkInfo, response);
  },

  dispatchRequestUpdate: function(id, data) {
    this.webConsoleFrame.consoleOutput.dispatchRequestUpdate(id, data);
  },

  /**
   * The "cachedMessages" response handler.
   *
   * @private
   * @param object response
   *        The JSON response object received from the server.
   */
  _onCachedMessages: async function(response) {
    if (response.error) {
      console.error("Web Console getCachedMessages error: " + response.error +
                    " " + response.message);
      this._connectDefer.reject(response);
      return;
    }

    if (!this._connectTimer) {
      // This happens if the promise is rejected (eg. a timeout), but the
      // connection attempt is successful, nonetheless.
      console.error("Web Console getCachedMessages error: invalid state.");
    }

    const messages =
      response.messages.concat(...this.webConsoleClient.getNetworkEvents());
    messages.sort((a, b) => a.timeStamp - b.timeStamp);

    this.dispatchMessagesAdd(messages);
    if (!this.webConsoleClient.hasNativeConsoleAPI) {
      await this.webConsoleFrame.logWarningAboutReplacedAPI();
    }

    this.connected = true;
    this._connectDefer.resolve(this);
  },

  /**
   * The "pageError" message type handler. We redirect any page errors to the UI
   * for displaying.
   *
   * @private
   * @param string type
   *        Message type.
   * @param object packet
   *        The message received from the server.
   */
  _onPageError: function(type, packet) {
    if (!this.webConsoleFrame || packet.from != this.webConsoleClient.actor) {
      return;
    }
    this.dispatchMessageAdd(packet);
  },
  /**
   * The "logMessage" message type handler. We redirect any message to the UI
   * for displaying.
   *
   * @private
   * @param string type
   *        Message type.
   * @param object packet
   *        The message received from the server.
   */
  _onLogMessage: function(type, packet) {
    if (!this.webConsoleFrame || packet.from != this.webConsoleClient.actor) {
      return;
    }
    this.dispatchMessageAdd(packet);
  },
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
  _onConsoleAPICall: function(type, packet) {
    if (!this.webConsoleFrame || packet.from != this.webConsoleClient.actor) {
      return;
    }
    this.dispatchMessageAdd(packet);
  },
  /**
   * The "networkEvent" message type handler. We redirect any message to
   * the UI for displaying.
   *
   * @private
   * @param object networkInfo
   *        The network request information.
   */
  _onNetworkEvent: function(networkInfo) {
    if (!this.webConsoleFrame) {
      return;
    }
    this.dispatchMessageAdd(networkInfo);
  },
  /**
   * The "networkEventUpdate" message type handler. We redirect any message to
   * the UI for displaying.
   *
   * @private
   * @param object response
   *        The update response received from the server.
   */
  _onNetworkEventUpdate: function(response) {
    if (!this.webConsoleFrame) {
      return;
    }
    this.dispatchMessageUpdate(response.networkInfo, response);
  },
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
  _onLastPrivateContextExited: function(type, packet) {
    if (this.webConsoleFrame && packet.from == this.webConsoleClient.actor) {
      this.webConsoleFrame.clearPrivateMessages();
    }
  },

  /**
   * The "navigate" event handlers. We redirect any message to the UI for displaying.
   *
   * @private
   * @param object packet
   *        The message received from the server.
   */
  _onTabNavigated: function(packet) {
    if (!this.webConsoleFrame) {
      return;
    }

    this.webConsoleFrame.handleTabNavigated(packet);
  },

  /**
   * The "will-navigate" event handlers. We redirect any message to the UI for displaying.
   *
   * @private
   * @param object packet
   *        The message received from the server.
   */
  _onTabWillNavigate: function(packet) {
    if (!this.webConsoleFrame) {
      return;
    }

    this.webConsoleFrame.handleTabWillNavigate(packet);
  },

  /**
   * Release an object actor.
   *
   * @param string actor
   *        The actor ID to send the request to.
   */
  releaseActor: function(actor) {
    if (this.client) {
      this.client.release(actor);
    }
  },

  /**
   * Disconnect the Web Console from the remote server.
   *
   * @return object
   *         A promise object that is resolved when disconnect completes.
   */
  disconnect: async function() {
    if (this._disconnecter) {
      return this._disconnecter.promise;
    }

    this._disconnecter = defer();

    // allow the console to finish attaching if it started.
    if (this.isAttached) {
      await this.isAttached;
    }
    if (!this.client) {
      this._disconnecter.resolve(null);
      return this._disconnecter.promise;
    }

    this.client.removeListener("logMessage", this._onLogMessage);
    this.client.removeListener("pageError", this._onPageError);
    this.client.removeListener("consoleAPICall", this._onConsoleAPICall);
    this.client.removeListener("lastPrivateContextExited",
                               this._onLastPrivateContextExited);
    this.webConsoleClient.off("networkEvent", this._onNetworkEvent);
    this.webConsoleClient.off("networkEventUpdate", this._onNetworkEventUpdate);
    this.target.off("will-navigate", this._onTabWillNavigate);
    this.target.off("navigate", this._onTabNavigated);

    this.client = null;
    this.webConsoleClient = null;
    this.target = null;
    this.connected = false;
    this.webConsoleFrame = null;
    this._disconnecter.resolve(null);

    return this._disconnecter.promise;
  },
};

exports.WebConsoleConnectionProxy = WebConsoleConnectionProxy;
