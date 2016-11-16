/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cu} = require("chrome");

const {Utils: WebConsoleUtils, CONSOLE_WORKER_IDS} =
  require("devtools/client/webconsole/utils");
const { getSourceNames } = require("devtools/client/shared/source-utils");
const BrowserLoaderModule = {};
Cu.import("resource://devtools/client/shared/browser-loader.js", BrowserLoaderModule);

const promise = require("promise");
const Services = require("Services");
const ErrorDocs = require("devtools/server/actors/errordocs");
const Telemetry = require("devtools/client/shared/telemetry");

loader.lazyServiceGetter(this, "clipboardHelper",
                         "@mozilla.org/widget/clipboardhelper;1",
                         "nsIClipboardHelper");
loader.lazyRequireGetter(this, "EventEmitter", "devtools/shared/event-emitter");
loader.lazyRequireGetter(this, "AutocompletePopup", "devtools/client/shared/autocomplete-popup", true);
loader.lazyRequireGetter(this, "ToolSidebar", "devtools/client/framework/sidebar", true);
loader.lazyRequireGetter(this, "ConsoleOutput", "devtools/client/webconsole/console-output", true);
loader.lazyRequireGetter(this, "Messages", "devtools/client/webconsole/console-output", true);
loader.lazyRequireGetter(this, "EnvironmentClient", "devtools/shared/client/main", true);
loader.lazyRequireGetter(this, "ObjectClient", "devtools/shared/client/main", true);
loader.lazyRequireGetter(this, "system", "devtools/shared/system");
loader.lazyRequireGetter(this, "JSTerm", "devtools/client/webconsole/jsterm", true);
loader.lazyRequireGetter(this, "gSequenceId", "devtools/client/webconsole/jsterm", true);
loader.lazyImporter(this, "VariablesView", "resource://devtools/client/shared/widgets/VariablesView.jsm");
loader.lazyImporter(this, "VariablesViewController", "resource://devtools/client/shared/widgets/VariablesViewController.jsm");
loader.lazyRequireGetter(this, "gDevTools", "devtools/client/framework/devtools", true);
loader.lazyRequireGetter(this, "KeyShortcuts", "devtools/client/shared/key-shortcuts", true);
loader.lazyRequireGetter(this, "ZoomKeys", "devtools/client/shared/zoom-keys");

const {PluralForm} = require("devtools/shared/plural-form");
const STRINGS_URI = "devtools/client/locales/webconsole.properties";
var l10n = new WebConsoleUtils.L10n(STRINGS_URI);

const XHTML_NS = "http://www.w3.org/1999/xhtml";

const MIXED_CONTENT_LEARN_MORE = "https://developer.mozilla.org/docs/Web/Security/Mixed_content";

const IGNORED_SOURCE_URLS = ["debugger eval code"];

// The amount of time in milliseconds that we wait before performing a live
// search.
const SEARCH_DELAY = 200;

// The number of lines that are displayed in the console output by default, for
// each category. The user can change this number by adjusting the hidden
// "devtools.hud.loglimit.{network,cssparser,exception,console}" preferences.
const DEFAULT_LOG_LIMIT = 1000;

// The various categories of messages. We start numbering at zero so we can
// use these as indexes into the MESSAGE_PREFERENCE_KEYS matrix below.
const CATEGORY_NETWORK = 0;
const CATEGORY_CSS = 1;
const CATEGORY_JS = 2;
const CATEGORY_WEBDEV = 3;
// always on
const CATEGORY_INPUT = 4;
// always on
const CATEGORY_OUTPUT = 5;
const CATEGORY_SECURITY = 6;
const CATEGORY_SERVER = 7;

// The possible message severities. As before, we start at zero so we can use
// these as indexes into MESSAGE_PREFERENCE_KEYS.
const SEVERITY_ERROR = 0;
const SEVERITY_WARNING = 1;
const SEVERITY_INFO = 2;
const SEVERITY_LOG = 3;

// The fragment of a CSS class name that identifies each category.
const CATEGORY_CLASS_FRAGMENTS = [
  "network",
  "cssparser",
  "exception",
  "console",
  "input",
  "output",
  "security",
  "server",
];

// The fragment of a CSS class name that identifies each severity.
const SEVERITY_CLASS_FRAGMENTS = [
  "error",
  "warn",
  "info",
  "log",
];

// The preference keys to use for each category/severity combination, indexed
// first by category (rows) and then by severity (columns) in the following
// order:
//
// [ Error, Warning, Info, Log ]
//
// Most of these rather idiosyncratic names are historical and predate the
// division of message type into "category" and "severity".
const MESSAGE_PREFERENCE_KEYS = [
  // Network
  [ "network", "netwarn", "netxhr", "networkinfo", ],
  // CSS
  [ "csserror", "cssparser", null, "csslog", ],
  // JS
  [ "exception", "jswarn", null, "jslog", ],
  // Web Developer
  [ "error", "warn", "info", "log", ],
  // Input
  [ null, null, null, null, ],
  // Output
  [ null, null, null, null, ],
  // Security
  [ "secerror", "secwarn", null, null, ],
  // Server Logging
  [ "servererror", "serverwarn", "serverinfo", "serverlog", ],
];

// A mapping from the console API log event levels to the Web Console
// severities.
const LEVELS = {
  error: SEVERITY_ERROR,
  exception: SEVERITY_ERROR,
  assert: SEVERITY_ERROR,
  warn: SEVERITY_WARNING,
  info: SEVERITY_INFO,
  log: SEVERITY_LOG,
  clear: SEVERITY_LOG,
  trace: SEVERITY_LOG,
  table: SEVERITY_LOG,
  debug: SEVERITY_LOG,
  dir: SEVERITY_LOG,
  dirxml: SEVERITY_LOG,
  group: SEVERITY_LOG,
  groupCollapsed: SEVERITY_LOG,
  groupEnd: SEVERITY_LOG,
  time: SEVERITY_LOG,
  timeEnd: SEVERITY_LOG,
  count: SEVERITY_LOG
};

// This array contains the prefKey for the workers and it must keep them in the
// same order as CONSOLE_WORKER_IDS
const WORKERTYPES_PREFKEYS =
  [ "sharedworkers", "serviceworkers", "windowlessworkers" ];

// The lowest HTTP response code (inclusive) that is considered an error.
const MIN_HTTP_ERROR_CODE = 400;
// The highest HTTP response code (inclusive) that is considered an error.
const MAX_HTTP_ERROR_CODE = 599;

// The indent of a console group in pixels.
const GROUP_INDENT = 12;

// The number of messages to display in a single display update. If we display
// too many messages at once we slow down the Firefox UI too much.
const MESSAGES_IN_INTERVAL = DEFAULT_LOG_LIMIT;

// The delay (in milliseconds) between display updates - tells how often we
// should *try* to push new messages to screen. This value is optimistic,
// updates won't always happen. Keep this low so the Web Console output feels
// live.
const OUTPUT_INTERVAL = 20;

// The maximum amount of time (in milliseconds) that can be spent doing cleanup
// inside of the flush output callback.  If things don't get cleaned up in this
// time, then it will start again the next time it is called.
const MAX_CLEANUP_TIME = 10;

// When the output queue has more than MESSAGES_IN_INTERVAL items we throttle
// output updates to this number of milliseconds. So during a lot of output we
// update every N milliseconds given here.
const THROTTLE_UPDATES = 1000;

// The preference prefix for all of the Web Console filters.
const FILTER_PREFS_PREFIX = "devtools.webconsole.filter.";

// The minimum font size.
const MIN_FONT_SIZE = 10;

const PREF_CONNECTION_TIMEOUT = "devtools.debugger.remote-timeout";
const PREF_PERSISTLOG = "devtools.webconsole.persistlog";
const PREF_MESSAGE_TIMESTAMP = "devtools.webconsole.timestampMessages";
const PREF_NEW_FRONTEND_ENABLED = "devtools.webconsole.new-frontend-enabled";

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

    this._connectDefer = promise.defer();

    let timeout = Services.prefs.getIntPref(PREF_CONNECTION_TIMEOUT);
    this._connectTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this._connectTimer.initWithCallback(this._connectionTimeout,
                                        timeout, Ci.nsITimer.TYPE_ONE_SHOT);

    let connPromise = this._connectDefer.promise;
    connPromise.then(() => {
      this._connectTimer.cancel();
      this._connectTimer = null;
    }, () => {
      this._connectTimer = null;
    });

    let client = this.client = this.target.client;

    if (this.target.isWorkerTarget) {
      // XXXworkers: Not Console API yet inside of workers (Bug 1209353).
    } else {
      client.addListener("logMessage", this._onLogMessage);
      client.addListener("pageError", this._onPageError);
      client.addListener("consoleAPICall", this._onConsoleAPICall);
      client.addListener("fileActivity", this._onFileActivity);
      client.addListener("reflowActivity", this._onReflowActivity);
      client.addListener("serverLogCall", this._onServerLogCall);
      client.addListener("lastPrivateContextExited",
                         this._onLastPrivateContextExited);
    }
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
  dispatchMessageAdd: function(packet) {
    this.webConsoleFrame.newConsoleOutput.dispatchMessageAdd(packet);
  },

  /**
   * Batched dispatch of messages.
   */
  dispatchMessagesAdd: function(packets) {
    this.webConsoleFrame.newConsoleOutput.dispatchMessagesAdd(packets);
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
    if (this.webConsoleFrame && packet.from == this._consoleActor) {
      if (this.webConsoleFrame.NEW_CONSOLE_OUTPUT_ENABLED) {
        this.dispatchMessageAdd(packet);
        return;
      }
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
    if (this.webConsoleFrame && packet.from == this._consoleActor) {
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
    if (this.webConsoleFrame && packet.from == this._consoleActor) {
      if (this.webConsoleFrame.NEW_CONSOLE_OUTPUT_ENABLED) {
        this.dispatchMessageAdd(packet);
      } else {
        this.webConsoleFrame.handleConsoleAPICall(packet.message);
      }
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
    if (this.webConsoleFrame) {
      if (this.webConsoleFrame.NEW_CONSOLE_OUTPUT_ENABLED) {
        this.dispatchMessageAdd(networkInfo);
      } else {
        this.webConsoleFrame.handleNetworkEvent(networkInfo);
      }
    }
  },

  /**
   * The "networkEventUpdate" message type handler. We redirect any message to
   * the UI for displaying.
   *
   * @private
   * @param string type
   *        Message type.
   * @param object packet
   *        The message received from the server.
   * @param object networkInfo
   *        The network request information.
   */
  _onNetworkEventUpdate: function (type, { packet, networkInfo }) {
    if (this.webConsoleFrame) {
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
    if (this.webConsoleFrame && packet.from == this._consoleActor) {
      this.webConsoleFrame.handleFileActivity(packet.uri);
    }
  },

  _onReflowActivity: function (type, packet) {
    if (this.webConsoleFrame && packet.from == this._consoleActor) {
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
    if (this.webConsoleFrame && packet.from == this._consoleActor) {
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

    this._disconnecter = promise.defer();

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