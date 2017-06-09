/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Utils: WebConsoleUtils} = require("devtools/client/webconsole/utils");
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

  this._onPageError = this._onPageError.bind(this);
  this._onLogMessage = this._onLogMessage.bind(this);
  this._onConsoleAPICall = this._onConsoleAPICall.bind(this);
  this._onNetworkEvent = this._onNetworkEvent.bind(this);
  this._onNetworkEventUpdate = this._onNetworkEventUpdate.bind(this);
  this._onFileActivity = this._onFileActivity.bind(this);
  this._onReflowActivity = this._onReflowActivity.bind(this);
  this._onServerLogCall = this._onServerLogCall.bind(this);
  this._onTabNavigated = this._onTabNavigated.bind(this);
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
   * Timer used for the connection.
   * @private
   * @type object
   */
  _connectTimer: null,

  _connectDefer: null,
  _disconnecter: null,

  /**
   * The WebConsoleActor ID.
   *
   * @private
   * @type string
   */
  _consoleActor: null,

  /**
   * Tells if the window.console object of the remote web page is the native
   * object or not.
   * @private
   * @type boolean
   */
  _hasNativeConsoleAPI: false,

  /**
   * Initialize a debugger client and connect it to the debugger server.
   *
   * @return object
   *         A promise object that is resolved/rejected based on the success of
   *         the connection initialization.
   */
  connect: function () {
    if (this._connectDefer) {
      return this._connectDefer.promise;
    }

    this._connectDefer = defer();

    let timeout = Services.prefs.getIntPref(PREF_CONNECTION_TIMEOUT);
    this._connectTimer = setTimeout(this._connectionTimeout, timeout);

    let connPromise = this._connectDefer.promise;
    connPromise.then(() => {
      clearTimeout(this._connectTimer);
      this._connectTimer = null;
    }, () => {
      clearTimeout(this._connectTimer);
      this._connectTimer = null;
    });

    let client = this.client = this.target.client;

    client.addListener("logMessage", this._onLogMessage);
    client.addListener("pageError", this._onPageError);
    client.addListener("consoleAPICall", this._onConsoleAPICall);
    client.addListener("fileActivity", this._onFileActivity);
    client.addListener("reflowActivity", this._onReflowActivity);
    client.addListener("serverLogCall", this._onServerLogCall);
    client.addListener("lastPrivateContextExited",
                       this._onLastPrivateContextExited);

    this.target.on("will-navigate", this._onTabNavigated);
    this.target.on("navigate", this._onTabNavigated);

    this._consoleActor = this.target.form.consoleActor;
    if (this.target.isTabActor) {
      let tab = this.target.form;
      this.webConsoleFrame.onLocationChange(tab.url, tab.title);
    }
    this._attachConsole();

    return connPromise;
  },

  /**
   * Connection timeout handler.
   * @private
   */
  _connectionTimeout: function () {
    let error = {
      error: "timeout",
      message: l10n.getStr("connectionTimeout"),
    };

    this._connectDefer.reject(error);
  },

  /**
   * Attach to the Web Console actor.
   * @private
   */
  _attachConsole: function () {
    let listeners = ["PageError", "ConsoleAPI", "NetworkActivity",
                     "FileActivity"];
    this.client.attachConsole(this._consoleActor, listeners,
                              this._onAttachConsole);
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
  _onAttachConsole: function (response, webConsoleClient) {
    if (response.error) {
      console.error("attachConsole failed: " + response.error + " " +
                    response.message);
      this._connectDefer.reject(response);
      return;
    }

    this.webConsoleClient = webConsoleClient;
    this._hasNativeConsoleAPI = response.nativeConsoleAPI;

    // There is no way to view response bodies from the Browser Console, so do
    // not waste the memory.
    let saveBodies = !this.webConsoleFrame.isBrowserConsole;
    this.webConsoleFrame.setSaveRequestAndResponseBodies(saveBodies);

    this.webConsoleClient.on("networkEvent", this._onNetworkEvent);
    this.webConsoleClient.on("networkEventUpdate", this._onNetworkEventUpdate);

    let msgs = ["PageError", "ConsoleAPI"];
    this.webConsoleClient.getCachedMessages(msgs, this._onCachedMessages);

    this.webConsoleFrame._onUpdateListeners();
  },

  /**
   * Dispatch a message add on the new frontend and emit an event for tests.
   */
  dispatchMessageAdd: function (packet) {
    this.webConsoleFrame.newConsoleOutput.dispatchMessageAdd(packet);
  },

  /**
   * Batched dispatch of messages.
   */
  dispatchMessagesAdd: function (packets) {
    this.webConsoleFrame.newConsoleOutput.dispatchMessagesAdd(packets);
  },

  /**
   * Dispatch a message event on the new frontend and emit an event for tests.
   */
  dispatchMessageUpdate: function (networkInfo, response) {
    this.webConsoleFrame.newConsoleOutput.dispatchMessageUpdate(networkInfo, response);
  },

  /**
   * The "cachedMessages" response handler.
   *
   * @private
   * @param object response
   *        The JSON response object received from the server.
   */
  _onCachedMessages: function (response) {
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

    let messages =
      response.messages.concat(...this.webConsoleClient.getNetworkEvents());
    messages.sort((a, b) => a.timeStamp - b.timeStamp);

    if (this.webConsoleFrame.NEW_CONSOLE_OUTPUT_ENABLED) {
      this.dispatchMessagesAdd(messages);
    } else {
      this.webConsoleFrame.displayCachedMessages(messages);
      if (!this._hasNativeConsoleAPI) {
        this.webConsoleFrame.logWarningAboutReplacedAPI();
      }
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
  _onPageError: function (type, packet) {
    if (!this.webConsoleFrame || packet.from != this._consoleActor) {
      return;
    }
    if (this.webConsoleFrame.NEW_CONSOLE_OUTPUT_ENABLED) {
      this.dispatchMessageAdd(packet);
    } else {
      this.webConsoleFrame.handlePageError(packet.pageError);
    }
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
  _onLogMessage: function (type, packet) {
    if (!this.webConsoleFrame || packet.from != this._consoleActor) {
      return;
    }
    if (this.webConsoleFrame.NEW_CONSOLE_OUTPUT_ENABLED) {
      this.dispatchMessageAdd(packet);
    } else {
      this.webConsoleFrame.handleLogMessage(packet);
    }
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
  _onConsoleAPICall: function (type, packet) {
    if (!this.webConsoleFrame || packet.from != this._consoleActor) {
      return;
    }
    if (this.webConsoleFrame.NEW_CONSOLE_OUTPUT_ENABLED) {
      this.dispatchMessageAdd(packet);
    } else {
      this.webConsoleFrame.handleConsoleAPICall(packet.message);
    }
  },

  /**
   * The "networkEvent" message type handler. We redirect any message to
   * the UI for displaying.
   *
   * @private
   * @param string type
   *        Message type.
   * @param object networkInfo
   *        The network request information.
   */
  _onNetworkEvent: function (type, networkInfo) {
    if (!this.webConsoleFrame) {
      return;
    }
    if (this.webConsoleFrame.NEW_CONSOLE_OUTPUT_ENABLED) {
      this.dispatchMessageAdd(networkInfo);
    } else {
      this.webConsoleFrame.handleNetworkEvent(networkInfo);
    }
  },

  /**
   * The "networkEventUpdate" message type handler. We redirect any message to
   * the UI for displaying.
   *
   * @private
   * @param string type
   *        Message type.
   * @param object response
   *        The update response received from the server.
   */
  _onNetworkEventUpdate: function (type, response) {
    if (!this.webConsoleFrame) {
      return;
    }
    let { packet, networkInfo } = response;
    if (this.webConsoleFrame.NEW_CONSOLE_OUTPUT_ENABLED) {
      this.dispatchMessageUpdate(networkInfo, response);
    } else {
      this.webConsoleFrame.handleNetworkEventUpdate(networkInfo, packet);
    }
  },

  /**
   * The "fileActivity" message type handler. We redirect any message to
   * the UI for displaying.
   *
   * @private
   * @param string type
   *        Message type.
   * @param object packet
   *        The message received from the server.
   */
  _onFileActivity: function (type, packet) {
    if (!this.webConsoleFrame || packet.from != this._consoleActor) {
      return;
    }
    if (this.webConsoleFrame.NEW_CONSOLE_OUTPUT_ENABLED) {
      // TODO: Implement for new console
    } else {
      this.webConsoleFrame.handleFileActivity(packet.uri);
    }
  },

  _onReflowActivity: function (type, packet) {
    if (!this.webConsoleFrame || packet.from != this._consoleActor) {
      return;
    }
    if (this.webConsoleFrame.NEW_CONSOLE_OUTPUT_ENABLED) {
      // TODO: Implement for new console
    } else {
      this.webConsoleFrame.handleReflowActivity(packet);
    }
  },

  /**
   * The "serverLogCall" message type handler. We redirect any message to
   * the UI for displaying.
   *
   * @private
   * @param string type
   *        Message type.
   * @param object packet
   *        The message received from the server.
   */
  _onServerLogCall: function (type, packet) {
    if (!this.webConsoleFrame || packet.from != this._consoleActor) {
      return;
    }
    if (this.webConsoleFrame.NEW_CONSOLE_OUTPUT_ENABLED) {
      // TODO: Implement for new console
    } else {
      this.webConsoleFrame.handleConsoleAPICall(packet.message);
    }
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
  _onLastPrivateContextExited: function (type, packet) {
    if (this.webConsoleFrame && packet.from == this._consoleActor) {
      this.webConsoleFrame.jsterm.clearPrivateMessages();
    }
  },

  /**
   * The "will-navigate" and "navigate" event handlers. We redirect any message
   * to the UI for displaying.
   *
   * @private
   * @param string event
   *        Event type.
   * @param object packet
   *        The message received from the server.
   */
  _onTabNavigated: function (event, packet) {
    if (!this.webConsoleFrame) {
      return;
    }

    this.webConsoleFrame.handleTabNavigated(event, packet);
  },

  /**
   * Release an object actor.
   *
   * @param string actor
   *        The actor ID to send the request to.
   */
  releaseActor: function (actor) {
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
  disconnect: function () {
    if (this._disconnecter) {
      return this._disconnecter.promise;
    }

    this._disconnecter = defer();

    if (!this.client) {
      this._disconnecter.resolve(null);
      return this._disconnecter.promise;
    }

    this.client.removeListener("logMessage", this._onLogMessage);
    this.client.removeListener("pageError", this._onPageError);
    this.client.removeListener("consoleAPICall", this._onConsoleAPICall);
    this.client.removeListener("fileActivity", this._onFileActivity);
    this.client.removeListener("reflowActivity", this._onReflowActivity);
    this.client.removeListener("serverLogCall", this._onServerLogCall);
    this.client.removeListener("lastPrivateContextExited",
                               this._onLastPrivateContextExited);
    this.webConsoleClient.off("networkEvent", this._onNetworkEvent);
    this.webConsoleClient.off("networkEventUpdate", this._onNetworkEventUpdate);
    this.target.off("will-navigate", this._onTabNavigated);
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
