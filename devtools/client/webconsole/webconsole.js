/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cu} = require("chrome");

const {Utils: WebConsoleUtils, CONSOLE_WORKER_IDS} =
  require("devtools/shared/webconsole/utils");
const { getSourceNames } = require("devtools/client/shared/source-utils");
const BrowserLoaderModule = {};
Cu.import("resource://devtools/client/shared/browser-loader.js", BrowserLoaderModule);

const promise = require("promise");
const Services = require("Services");
const ErrorDocs = require("devtools/server/actors/errordocs");
const Telemetry = require("devtools/client/shared/telemetry")

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
loader.lazyRequireGetter(this, "Timers", "sdk/timers");
loader.lazyRequireGetter(this, "JSTerm", "devtools/client/webconsole/jsterm", true);
loader.lazyRequireGetter(this, "gSequenceId", "devtools/client/webconsole/jsterm", true);
loader.lazyImporter(this, "VariablesView", "resource://devtools/client/shared/widgets/VariablesView.jsm");
loader.lazyImporter(this, "VariablesViewController", "resource://devtools/client/shared/widgets/VariablesViewController.jsm");
loader.lazyRequireGetter(this, "gDevTools", "devtools/client/framework/devtools", true);
loader.lazyImporter(this, "PluralForm", "resource://gre/modules/PluralForm.jsm");

const STRINGS_URI = "chrome://devtools/locale/webconsole.properties";
var l10n = new WebConsoleUtils.L10n(STRINGS_URI);

const XHTML_NS = "http://www.w3.org/1999/xhtml";

const MIXED_CONTENT_LEARN_MORE = "https://developer.mozilla.org/docs/Security/MixedContent";

const TRACKING_PROTECTION_LEARN_MORE = "https://developer.mozilla.org/Firefox/Privacy/Tracking_Protection";

const INSECURE_PASSWORDS_LEARN_MORE = "https://developer.mozilla.org/docs/Security/InsecurePasswords";

const PUBLIC_KEY_PINS_LEARN_MORE = "https://developer.mozilla.org/docs/Web/Security/Public_Key_Pinning";

const STRICT_TRANSPORT_SECURITY_LEARN_MORE = "https://developer.mozilla.org/docs/Security/HTTP_Strict_Transport_Security";

const WEAK_SIGNATURE_ALGORITHM_LEARN_MORE = "https://developer.mozilla.org/docs/Security/Weak_Signature_Algorithm";

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

/**
 * A WebConsoleFrame instance is an interactive console initialized *per target*
 * that displays console log data as well as provides an interactive terminal to
 * manipulate the target's document content.
 *
 * The WebConsoleFrame is responsible for the actual Web Console UI
 * implementation.
 *
 * @constructor
 * @param object webConsoleOwner
 *        The WebConsole owner object.
 */
function WebConsoleFrame(webConsoleOwner) {
  this.owner = webConsoleOwner;
  this.hudId = this.owner.hudId;
  this.window = this.owner.iframeWindow;

  this._repeatNodes = {};
  this._outputQueue = [];
  this._itemDestroyQueue = [];
  this._pruneCategoriesQueue = {};
  this.filterPrefs = {};

  this.output = new ConsoleOutput(this);

  this.unmountMessage = this.unmountMessage.bind(this);
  this._toggleFilter = this._toggleFilter.bind(this);
  this.resize = this.resize.bind(this);
  this._onPanelSelected = this._onPanelSelected.bind(this);
  this._flushMessageQueue = this._flushMessageQueue.bind(this);
  this._onToolboxPrefChanged = this._onToolboxPrefChanged.bind(this);
  this._onUpdateListeners = this._onUpdateListeners.bind(this);

  this._outputTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  this._outputTimerInitialized = false;

  let require = BrowserLoaderModule.BrowserLoader({
    window: this.window,
    useOnlyShared: true
  }).require;

  this.React = require("devtools/client/shared/vendor/react");
  this.ReactDOM = require("devtools/client/shared/vendor/react-dom");
  this.FrameView = this.React.createFactory(require("devtools/client/shared/components/frame"));

  this._telemetry = new Telemetry();

  EventEmitter.decorate(this);
}
exports.WebConsoleFrame = WebConsoleFrame;

WebConsoleFrame.prototype = {
  /**
   * The WebConsole instance that owns this frame.
   * @see hudservice.js::WebConsole
   * @type object
   */
  owner: null,

  /**
   * Proxy between the Web Console and the remote Web Console instance. This
   * object holds methods used for connecting, listening and disconnecting from
   * the remote server, using the remote debugging protocol.
   *
   * @see WebConsoleConnectionProxy
   * @type object
   */
  proxy: null,

  /**
   * Getter for the xul:popupset that holds any popups we open.
   * @type nsIDOMElement
   */
  get popupset() {
    return this.owner.mainPopupSet;
  },

  /**
   * Holds the initialization promise object.
   * @private
   * @type object
   */
  _initDefer: null,

  /**
   * Last time when we displayed any message in the output.
   *
   * @private
   * @type number
   *       Timestamp in milliseconds since the Unix epoch.
   */
  _lastOutputFlush: 0,

  /**
   * Message nodes are stored here in a queue for later display.
   *
   * @private
   * @type array
   */
  _outputQueue: null,

  /**
   * Keep track of the categories we need to prune from time to time.
   *
   * @private
   * @type array
   */
  _pruneCategoriesQueue: null,

  /**
   * Function invoked whenever the output queue is emptied. This is used by some
   * tests.
   *
   * @private
   * @type function
   */
  _flushCallback: null,

  /**
   * Timer used for flushing the messages output queue.
   *
   * @private
   * @type nsITimer
   */
  _outputTimer: null,
  _outputTimerInitialized: null,

  /**
   * Store for tracking repeated nodes.
   * @private
   * @type object
   */
  _repeatNodes: null,

  /**
   * Preferences for filtering messages by type.
   * @see this._initDefaultFilterPrefs()
   * @type object
   */
  filterPrefs: null,

  /**
   * Prefix used for filter preferences.
   * @private
   * @type string
   */
  _filterPrefsPrefix: FILTER_PREFS_PREFIX,

  /**
   * The nesting depth of the currently active console group.
   */
  groupDepth: 0,

  /**
   * The current target location.
   * @type string
   */
  contentLocation: "",

  /**
   * The JSTerm object that manage the console's input.
   * @see JSTerm
   * @type object
   */
  jsterm: null,

  /**
   * The element that holds all of the messages we display.
   * @type nsIDOMElement
   */
  outputNode: null,

  /**
   * The ConsoleOutput instance that manages all output.
   * @type object
   */
  output: null,

  /**
   * The input element that allows the user to filter messages by string.
   * @type nsIDOMElement
   */
  filterBox: null,

  /**
   * Getter for the debugger WebConsoleClient.
   * @type object
   */
  get webConsoleClient() {
    return this.proxy ? this.proxy.webConsoleClient : null;
  },

  _destroyer: null,

  _saveRequestAndResponseBodies: true,

  // Chevron width at the starting of Web Console's input box.
  _chevronWidth: 0,
  // Width of the monospace characters in Web Console's input box.
  _inputCharWidth: 0,

  /**
   * Setter for saving of network request and response bodies.
   *
   * @param boolean value
   *        The new value you want to set.
   */
  setSaveRequestAndResponseBodies: function(value) {
    if (!this.webConsoleClient) {
      // Don't continue if the webconsole disconnected.
      return promise.resolve(null);
    }

    let deferred = promise.defer();
    let newValue = !!value;
    let toSet = {
      "NetworkMonitor.saveRequestAndResponseBodies": newValue,
    };

    // Make sure the web console client connection is established first.
    this.webConsoleClient.setPreferences(toSet, response => {
      if (!response.error) {
        this._saveRequestAndResponseBodies = newValue;
        deferred.resolve(response);
      } else {
        deferred.reject(response.error);
      }
    });

    return deferred.promise;
  },

  /**
   * Getter for the persistent logging preference.
   * @type boolean
   */
  get persistLog() {
    // For the browser console, we receive tab navigation
    // when the original top level window we attached to is closed,
    // but we don't want to reset console history and just switch to
    // the next available window.
    return this.owner._browserConsole ||
           Services.prefs.getBoolPref(PREF_PERSISTLOG);
  },

  /**
   * Initialize the WebConsoleFrame instance.
   * @return object
   *         A promise object that resolves once the frame is ready to use.
   */
  init: function() {
    this._initUI();
    let connectionInited = this._initConnection();

    // Don't reject if the history fails to load for some reason.
    // This would be fine, the panel will just start with empty history.
    let allReady = this.jsterm.historyLoaded.catch(() => {}).then(() => {
      return connectionInited;
    });

    // This notification is only used in tests. Don't chain it onto
    // the returned promise because the console panel needs to be attached
    // to the toolbox before the web-console-created event is receieved.
    let notifyObservers = () => {
      let id = WebConsoleUtils.supportsString(this.hudId);
      Services.obs.notifyObservers(id, "web-console-created", null);
    };
    allReady.then(notifyObservers, notifyObservers);

    return allReady;
  },

  /**
   * Connect to the server using the remote debugging protocol.
   *
   * @private
   * @return object
   *         A promise object that is resolved/reject based on the connection
   *         result.
   */
  _initConnection: function() {
    if (this._initDefer) {
      return this._initDefer.promise;
    }

    this._initDefer = promise.defer();
    this.proxy = new WebConsoleConnectionProxy(this, this.owner.target);

    this.proxy.connect().then(() => {
      // on success
      this._initDefer.resolve(this);
    }, (reason) => {
      // on failure
      let node = this.createMessageNode(CATEGORY_JS, SEVERITY_ERROR,
                                        reason.error + ": " + reason.message);
      this.outputMessage(CATEGORY_JS, node, [reason]);
      this._initDefer.reject(reason);
    });

    return this._initDefer.promise;
  },

  /**
   * Find the Web Console UI elements and setup event listeners as needed.
   * @private
   */
  _initUI: function() {
    this.document = this.window.document;
    this.rootElement = this.document.documentElement;
    this.NEW_CONSOLE_OUTPUT_ENABLED = !this.owner._browserConsole &&
      Services.prefs.getBoolPref(PREF_NEW_FRONTEND_ENABLED);

    this._initDefaultFilterPrefs();

    // Register the controller to handle "select all" properly.
    this._commandController = new CommandController(this);
    this.window.controllers.insertControllerAt(0, this._commandController);

    this._contextMenuHandler = new ConsoleContextMenu(this);

    let doc = this.document;

    if (system.constants.platform === "macosx") {
      doc.querySelector("#key_clearOSX").removeAttribute("disabled");
    } else {
      doc.querySelector("#key_clear").removeAttribute("disabled");
    }

    this.filterBox = doc.querySelector(".hud-filter-box");
    this.outputNode = doc.getElementById("output-container");
    this.outputWrapper = doc.getElementById("output-wrapper");

    this.completeNode = doc.querySelector(".jsterm-complete-node");
    this.inputNode = doc.querySelector(".jsterm-input-node");

    this._setFilterTextBoxEvents();
    this._initFilterButtons();

    let fontSize = this.owner._browserConsole ?
                   Services.prefs.getIntPref("devtools.webconsole.fontSize") :
                   0;

    if (fontSize != 0) {
      fontSize = Math.max(MIN_FONT_SIZE, fontSize);

      this.outputNode.style.fontSize = fontSize + "px";
      this.completeNode.style.fontSize = fontSize + "px";
      this.inputNode.style.fontSize = fontSize + "px";
    }

    if (this.owner._browserConsole) {
      for (let id of ["Enlarge", "Reduce", "Reset"]) {
        this.document.getElementById("cmd_fullZoom" + id)
                     .removeAttribute("disabled");
      }
    }

    // Update the character width and height needed for the popup offset
    // calculations.
    this._updateCharSize();

    let clearButton =
      doc.getElementsByClassName("webconsole-clear-console-button")[0];
    clearButton.addEventListener("command", () => {
      this.owner._onClearButton();
      this.jsterm.clearOutput(true);
    });

    this.jsterm = new JSTerm(this);
    this.jsterm.init();

    if (this.NEW_CONSOLE_OUTPUT_ENABLED) {
      // @TODO Remove this once JSTerm is handled with React/Redux.
      this.window.jsterm = this.jsterm;
      console.log("Entering experimental mode for console frontend");

      // XXX: We should actually stop output from happening on old output
      // panel, but for now let's just hide it.
      this.experimentalOutputNode = this.outputNode.cloneNode();
      this.outputNode.hidden = true;
      this.outputNode.parentNode.appendChild(this.experimentalOutputNode);
      // @TODO Once the toolbox has been converted to React, see if passing
      // in JSTerm is still necessary.
      this.newConsoleOutput = new this.window.NewConsoleOutput(this.experimentalOutputNode, this.jsterm);
      console.log("Created newConsoleOutput", this.newConsoleOutput);
    }

    this.resize();
    this.window.addEventListener("resize", this.resize, true);
    this.jsterm.on("sidebar-opened", this.resize);
    this.jsterm.on("sidebar-closed", this.resize);

    let toolbox = gDevTools.getToolbox(this.owner.target);
    if (toolbox) {
      toolbox.on("webconsole-selected", this._onPanelSelected);
    }

    /*
     * Focus the input line whenever the output area is clicked.
     */
    this.outputWrapper.addEventListener("click", (event) => {
      // Do not focus on middle/right-click or 2+ clicks.
      if (event.detail !== 1 || event.button !== 0) {
        return;
      }

      // Do not focus if something is selected
      let selection = this.window.getSelection();
      if (selection && !selection.isCollapsed) {
        return;
      }

      // Do not focus if a link was clicked
      if (event.target.nodeName.toLowerCase() === "a" ||
          event.target.parentNode.nodeName.toLowerCase() === "a") {
        return;
      }

      this.jsterm.focus();
    });

    // Toggle the timestamp on preference change
    gDevTools.on("pref-changed", this._onToolboxPrefChanged);
    this._onToolboxPrefChanged("pref-changed", {
      pref: PREF_MESSAGE_TIMESTAMP,
      newValue: Services.prefs.getBoolPref(PREF_MESSAGE_TIMESTAMP),
    });

    // focus input node
    this.jsterm.focus();
  },

  /**
   * Resizes the output node to fit the output wrapped.
   * We need this because it makes the layout a lot faster than
   * using -moz-box-flex and 100% width.  See Bug 1237368.
   */
  resize: function() {
    this.outputNode.style.width = this.outputWrapper.clientWidth + "px";
  },

  /**
   * Sets the focus to JavaScript input field when the web console tab is
   * selected or when there is a split console present.
   * @private
   */
  _onPanelSelected: function() {
    this.jsterm.focus();
  },

  /**
   * Initialize the default filter preferences.
   * @private
   */
  _initDefaultFilterPrefs: function() {
    let prefs = ["network", "networkinfo", "csserror", "cssparser", "csslog",
                 "exception", "jswarn", "jslog", "error", "info", "warn", "log",
                 "secerror", "secwarn", "netwarn", "netxhr", "sharedworkers",
                 "serviceworkers", "windowlessworkers", "servererror",
                 "serverwarn", "serverinfo", "serverlog"];

    for (let pref of prefs) {
      this.filterPrefs[pref] = Services.prefs.getBoolPref(
        this._filterPrefsPrefix + pref);
    }
  },

  /**
   * Attach / detach reflow listeners depending on the checked status
   * of the `CSS > Log` menuitem.
   *
   * @param function [callback=null]
   *        Optional function to invoke when the listener has been
   *        added/removed.
   */
  _updateReflowActivityListener: function(callback) {
    if (this.webConsoleClient) {
      let pref = this._filterPrefsPrefix + "csslog";
      if (Services.prefs.getBoolPref(pref)) {
        this.webConsoleClient.startListeners(["ReflowActivity"], callback);
      } else {
        this.webConsoleClient.stopListeners(["ReflowActivity"], callback);
      }
    }
  },

  /**
   * Attach / detach server logging listener depending on the filter
   * preferences. If the user isn't interested in the server logs at
   * all the listener is not registered.
   *
   * @param function [callback=null]
   *        Optional function to invoke when the listener has been
   *        added/removed.
   */
  _updateServerLoggingListener: function(callback) {
    if (!this.webConsoleClient) {
      return null;
    }

    let startListener = false;
    let prefs = ["servererror", "serverwarn", "serverinfo", "serverlog"];
    for (let i = 0; i < prefs.length; i++) {
      if (this.filterPrefs[prefs[i]]) {
        startListener = true;
        break;
      }
    }

    if (startListener) {
      this.webConsoleClient.startListeners(["ServerLogging"], callback);
    } else {
      this.webConsoleClient.stopListeners(["ServerLogging"], callback);
    }
  },

  /**
   * Sets the events for the filter input field.
   * @private
   */
  _setFilterTextBoxEvents: function() {
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    let timerEvent = this.adjustVisibilityOnSearchStringChange.bind(this);

    let onChange = function _onChange() {
      // To improve responsiveness, we let the user finish typing before we
      // perform the search.
      timer.cancel();
      timer.initWithCallback(timerEvent, SEARCH_DELAY,
                             Ci.nsITimer.TYPE_ONE_SHOT);
    };

    this.filterBox.addEventListener("command", onChange, false);
    this.filterBox.addEventListener("input", onChange, false);
  },

  /**
   * Creates one of the filter buttons on the toolbar.
   *
   * @private
   * @param nsIDOMNode aParent
   *        The node to which the filter button should be appended.
   * @param object aDescriptor
   *        A descriptor that contains info about the button. Contains "name",
   *        "category", and "prefKey" properties, and optionally a "severities"
   *        property.
   */
  _initFilterButtons: function() {
    let categories = this.document
                     .querySelectorAll(".webconsole-filter-button[category]");
    Array.forEach(categories, function(button) {
      button.addEventListener("contextmenu", () => {
        button.open = true;
      }, false);
      button.addEventListener("click", this._toggleFilter, false);

      let someChecked = false;
      let severities = button.querySelectorAll("menuitem[prefKey]");
      Array.forEach(severities, function(menuItem) {
        menuItem.addEventListener("command", this._toggleFilter, false);

        let prefKey = menuItem.getAttribute("prefKey");
        let checked = this.filterPrefs[prefKey];
        menuItem.setAttribute("checked", checked);
        someChecked = someChecked || checked;
      }, this);

      button.setAttribute("checked", someChecked);
      button.setAttribute("aria-pressed", someChecked);
    }, this);

    if (!this.owner._browserConsole) {
      // The Browser Console displays nsIConsoleMessages which are messages that
      // end up in the JS category, but they are not errors or warnings, they
      // are just log messages. The Web Console does not show such messages.
      let jslog = this.document.querySelector("menuitem[prefKey=jslog]");
      jslog.hidden = true;
    }

    if (Services.appinfo.OS == "Darwin") {
      let net = this.document.querySelector("toolbarbutton[category=net]");
      let accesskey = net.getAttribute("accesskeyMacOSX");
      net.setAttribute("accesskey", accesskey);

      let logging =
        this.document.querySelector("toolbarbutton[category=logging]");
      logging.removeAttribute("accesskey");

      let serverLogging =
        this.document.querySelector("toolbarbutton[category=server]");
      serverLogging.removeAttribute("accesskey");
    }
  },

  /**
   * Increase, decrease or reset the font size.
   *
   * @param string size
   *        The size of the font change. Accepted values are "+" and "-".
   *        An unmatched size assumes a font reset.
   */
  changeFontSize: function(size) {
    let fontSize = this.window
                   .getComputedStyle(this.outputNode, null)
                   .getPropertyValue("font-size").replace("px", "");

    if (this.outputNode.style.fontSize) {
      fontSize = this.outputNode.style.fontSize.replace("px", "");
    }

    if (size == "+" || size == "-") {
      fontSize = parseInt(fontSize, 10);

      if (size == "+") {
        fontSize += 1;
      } else {
        fontSize -= 1;
      }

      if (fontSize < MIN_FONT_SIZE) {
        fontSize = MIN_FONT_SIZE;
      }

      Services.prefs.setIntPref("devtools.webconsole.fontSize", fontSize);
      fontSize = fontSize + "px";

      this.completeNode.style.fontSize = fontSize;
      this.inputNode.style.fontSize = fontSize;
      this.outputNode.style.fontSize = fontSize;
    } else {
      this.completeNode.style.fontSize = "";
      this.inputNode.style.fontSize = "";
      this.outputNode.style.fontSize = "";
      Services.prefs.clearUserPref("devtools.webconsole.fontSize");
    }
    this._updateCharSize();
  },

  /**
   * Calculates the width and height of a single character of the input box.
   * This will be used in opening the popup at the correct offset.
   *
   * @private
   */
  _updateCharSize: function() {
    let doc = this.document;
    let tempLabel = doc.createElementNS(XHTML_NS, "span");
    let style = tempLabel.style;
    style.position = "fixed";
    style.padding = "0";
    style.margin = "0";
    style.width = "auto";
    style.color = "transparent";
    WebConsoleUtils.copyTextStyles(this.inputNode, tempLabel);
    tempLabel.textContent = "x";
    doc.documentElement.appendChild(tempLabel);
    this._inputCharWidth = tempLabel.offsetWidth;
    tempLabel.parentNode.removeChild(tempLabel);
    // Calculate the width of the chevron placed at the beginning of the input
    // box. Remove 4 more pixels to accomodate the padding of the popup.
    this._chevronWidth = +doc.defaultView.getComputedStyle(this.inputNode)
                             .paddingLeft.replace(/[^0-9.]/g, "") - 4;
  },

  /**
   * The event handler that is called whenever a user switches a filter on or
   * off.
   *
   * @private
   * @param nsIDOMEvent event
   *        The event that triggered the filter change.
   */
  _toggleFilter: function(event) {
    let target = event.target;
    let tagName = target.tagName;
    // Prevent toggle if generated from a contextmenu event (right click)
    let isRightClick = (event.button === 2);
    if (tagName != event.currentTarget.tagName || isRightClick) {
      return;
    }

    switch (tagName) {
      case "toolbarbutton": {
        let originalTarget = event.originalTarget;
        let classes = originalTarget.classList;

        if (originalTarget.localName !== "toolbarbutton") {
          // Oddly enough, the click event is sent to the menu button when
          // selecting a menu item with the mouse. Detect this case and bail
          // out.
          break;
        }

        if (!classes.contains("toolbarbutton-menubutton-button") &&
            originalTarget.getAttribute("type") === "menu-button") {
          // This is a filter button with a drop-down. The user clicked the
          // drop-down, so do nothing. (The menu will automatically appear
          // without our intervention.)
          break;
        }

        // Toggle on the targeted filter button, and if the user alt clicked,
        // toggle off all other filter buttons and their associated filters.
        let state = target.getAttribute("checked") !== "true";
        if (event.getModifierState("Alt")) {
          let buttons = this.document
                        .querySelectorAll(".webconsole-filter-button");
          Array.forEach(buttons, (button) => {
            if (button !== target) {
              button.setAttribute("checked", false);
              button.setAttribute("aria-pressed", false);
              this._setMenuState(button, false);
            }
          });
          state = true;
        }
        target.setAttribute("checked", state);
        target.setAttribute("aria-pressed", state);

        // This is a filter button with a drop-down, and the user clicked the
        // main part of the button. Go through all the severities and toggle
        // their associated filters.
        this._setMenuState(target, state);

        // CSS reflow logging can decrease web page performance.
        // Make sure the option is always unchecked when the CSS filter button
        // is selected. See bug 971798.
        if (target.getAttribute("category") == "css" && state) {
          let csslogMenuItem = target.querySelector("menuitem[prefKey=csslog]");
          csslogMenuItem.setAttribute("checked", false);
          this.setFilterState("csslog", false);
        }

        break;
      }

      case "menuitem": {
        let state = target.getAttribute("checked") !== "true";
        target.setAttribute("checked", state);

        let prefKey = target.getAttribute("prefKey");
        this.setFilterState(prefKey, state);

        // Adjust the state of the button appropriately.
        let menuPopup = target.parentNode;

        let someChecked = false;
        let menuItem = menuPopup.firstChild;
        while (menuItem) {
          if (menuItem.hasAttribute("prefKey") &&
              menuItem.getAttribute("checked") === "true") {
            someChecked = true;
            break;
          }
          menuItem = menuItem.nextSibling;
        }
        let toolbarButton = menuPopup.parentNode;
        toolbarButton.setAttribute("checked", someChecked);
        toolbarButton.setAttribute("aria-pressed", someChecked);
        break;
      }
    }
  },

  /**
   * Set the menu attributes for a specific toggle button.
   *
   * @private
   * @param XULElement target
   *        Button with drop down items to be toggled.
   * @param boolean state
   *        True if the menu item is being toggled on, and false otherwise.
   */
  _setMenuState: function(target, state) {
    let menuItems = target.querySelectorAll("menuitem");
    Array.forEach(menuItems, (item) => {
      item.setAttribute("checked", state);
      let prefKey = item.getAttribute("prefKey");
      this.setFilterState(prefKey, state);
    });
  },

  /**
   * Set the filter state for a specific toggle button.
   *
   * @param string toggleType
   * @param boolean state
   * @returns void
   */
  setFilterState: function(toggleType, state) {
    this.filterPrefs[toggleType] = state;
    this.adjustVisibilityForMessageType(toggleType, state);

    Services.prefs.setBoolPref(this._filterPrefsPrefix + toggleType, state);

    if (this._updateListenersTimeout) {
      Timers.clearTimeout(this._updateListenersTimeout);
    }

    this._updateListenersTimeout = Timers.setTimeout(
      this._onUpdateListeners, 200);
  },

  /**
   * Get the filter state for a specific toggle button.
   *
   * @param string toggleType
   * @returns boolean
   */
  getFilterState: function(toggleType) {
    return this.filterPrefs[toggleType];
  },

  /**
   * Called when a logging filter changes. Allows to stop/start
   * listeners according to the current filter state.
   */
  _onUpdateListeners: function() {
    this._updateReflowActivityListener();
    this._updateServerLoggingListener();
  },

  /**
   * Check that the passed string matches the filter arguments.
   *
   * @param String str
   *        to search for filter words in.
   * @param String filter
   *        is a string containing all of the words to filter on.
   * @returns boolean
   */
  stringMatchesFilters: function(str, filter) {
    if (!filter || !str) {
      return true;
    }

    let searchStr = str.toLowerCase();
    let filterStrings = filter.toLowerCase().split(/\s+/);
    return !filterStrings.some(function(f) {
      return searchStr.indexOf(f) == -1;
    });
  },

  /**
   * Turns the display of log nodes on and off appropriately to reflect the
   * adjustment of the message type filter named by @prefKey.
   *
   * @param string prefKey
   *        The preference key for the message type being filtered: one of the
   *        values in the MESSAGE_PREFERENCE_KEYS table.
   * @param boolean state
   *        True if the filter named by @messageType is being turned on; false
   *        otherwise.
   * @returns void
   */
  adjustVisibilityForMessageType: function(prefKey, state) {
    let outputNode = this.outputNode;
    let doc = this.document;

    // Look for message nodes (".message") with the given preference key
    // (filter="error", filter="cssparser", etc.) and add or remove the
    // "filtered-by-type" class, which turns on or off the display.

    let attribute = WORKERTYPES_PREFKEYS.indexOf(prefKey) == -1
                      ? "filter" : "workerType";

    let xpath = ".//*[contains(@class, 'message') and " +
      "@" + attribute + "='" + prefKey + "']";
    let result = doc.evaluate(xpath, outputNode, null,
      Ci.nsIDOMXPathResult.UNORDERED_NODE_SNAPSHOT_TYPE, null);
    for (let i = 0; i < result.snapshotLength; i++) {
      let node = result.snapshotItem(i);
      if (state) {
        node.classList.remove("filtered-by-type");
      } else {
        node.classList.add("filtered-by-type");
      }
    }
  },

  /**
   * Turns the display of log nodes on and off appropriately to reflect the
   * adjustment of the search string.
   */
  adjustVisibilityOnSearchStringChange: function() {
    let nodes = this.outputNode.getElementsByClassName("message");
    let searchString = this.filterBox.value;

    for (let i = 0, n = nodes.length; i < n; ++i) {
      let node = nodes[i];

      // hide nodes that match the strings
      let text = node.textContent;

      // if the text matches the words in aSearchString...
      if (this.stringMatchesFilters(text, searchString)) {
        node.classList.remove("filtered-by-string");
      } else {
        node.classList.add("filtered-by-string");
      }
    }

    this.resize();
  },

  /**
   * Applies the user's filters to a newly-created message node via CSS
   * classes.
   *
   * @param nsIDOMNode node
   *        The newly-created message node.
   * @return boolean
   *         True if the message was filtered or false otherwise.
   */
  filterMessageNode: function(node) {
    let isFiltered = false;

    // Filter by the message type.
    let prefKey = MESSAGE_PREFERENCE_KEYS[node.category][node.severity];
    if (prefKey && !this.getFilterState(prefKey)) {
      // The node is filtered by type.
      node.classList.add("filtered-by-type");
      isFiltered = true;
    }

    // Filter by worker type
    if ("workerType" in node && !this.getFilterState(node.workerType)) {
      node.classList.add("filtered-by-type");
      isFiltered = true;
    }

    // Filter on the search string.
    let search = this.filterBox.value;
    let text = node.clipboardText;

    // if string matches the filter text
    if (!this.stringMatchesFilters(text, search)) {
      node.classList.add("filtered-by-string");
      isFiltered = true;
    }

    if (isFiltered && node.classList.contains("inlined-variables-view")) {
      node.classList.add("hidden-message");
    }

    return isFiltered;
  },

  /**
   * Merge the attributes of repeated nodes.
   *
   * @param nsIDOMNode original
   *        The Original Node. The one being merged into.
   */
  mergeFilteredMessageNode: function(original) {
    let repeatNode = original.getElementsByClassName("message-repeats")[0];
    if (!repeatNode) {
      // no repeat node, return early.
      return;
    }

    let occurrences = parseInt(repeatNode.getAttribute("value"), 10) + 1;
    repeatNode.setAttribute("value", occurrences);
    repeatNode.textContent = occurrences;
    let str = l10n.getStr("messageRepeats.tooltip2");
    repeatNode.title = PluralForm.get(occurrences, str)
                       .replace("#1", occurrences);
  },

  /**
   * Filter the message node from the output if it is a repeat.
   *
   * @private
   * @param nsIDOMNode node
   *        The message node to be filtered or not.
   * @returns nsIDOMNode|null
   *          Returns the duplicate node if the message was filtered, null
   *          otherwise.
   */
  _filterRepeatedMessage: function(node) {
    let repeatNode = node.getElementsByClassName("message-repeats")[0];
    if (!repeatNode) {
      return null;
    }

    let uid = repeatNode._uid;
    let dupeNode = null;

    if (node.category == CATEGORY_CSS ||
        node.category == CATEGORY_SECURITY) {
      dupeNode = this._repeatNodes[uid];
      if (!dupeNode) {
        this._repeatNodes[uid] = node;
      }
    } else if ((node.category == CATEGORY_WEBDEV ||
                node.category == CATEGORY_JS) &&
               node.category != CATEGORY_NETWORK &&
               !node.classList.contains("inlined-variables-view")) {
      let lastMessage = this.outputNode.lastChild;
      if (!lastMessage) {
        return null;
      }

      let lastRepeatNode =
        lastMessage.getElementsByClassName("message-repeats")[0];
      if (lastRepeatNode && lastRepeatNode._uid == uid) {
        dupeNode = lastMessage;
      }
    }

    if (dupeNode) {
      this.mergeFilteredMessageNode(dupeNode);
      // Even though this node was never rendered, we create the location
      // nodes before rendering, so we still have to clean up any
      // React components
      this.unmountMessage(node);
      return dupeNode;
    }

    return null;
  },

  /**
   * Display cached messages that may have been collected before the UI is
   * displayed.
   *
   * @param array remoteMessages
   *        Array of cached messages coming from the remote Web Console
   *        content instance.
   */
  displayCachedMessages: function(remoteMessages) {
    if (!remoteMessages.length) {
      return;
    }

    remoteMessages.forEach(function(message) {
      switch (message._type) {
        case "PageError": {
          let category = Utils.categoryForScriptError(message);
          this.outputMessage(category, this.reportPageError,
                             [category, message]);
          break;
        }
        case "LogMessage":
          this.handleLogMessage(message);
          break;
        case "ConsoleAPI":
          this.outputMessage(CATEGORY_WEBDEV, this.logConsoleAPIMessage,
                             [message]);
          break;
        case "NetworkEvent":
          this.outputMessage(CATEGORY_NETWORK, this.logNetEvent, [message]);
          break;
      }
    }, this);
  },

  /**
   * Logs a message to the Web Console that originates from the Web Console
   * server.
   *
   * @param object message
   *        The message received from the server.
   * @return nsIDOMElement|null
   *         The message element to display in the Web Console output.
   */
  logConsoleAPIMessage: function(message) {
    let body = null;
    let clipboardText = null;
    let sourceURL = message.filename;
    let sourceLine = message.lineNumber;
    let level = message.level;
    let args = message.arguments;
    let objectActors = new Set();
    let node = null;

    // Gather the actor IDs.
    args.forEach((value) => {
      if (WebConsoleUtils.isActorGrip(value)) {
        objectActors.add(value.actor);
      }
    });

    switch (level) {
      case "log":
      case "info":
      case "warn":
      case "error":
      case "exception":
      case "assert":
      case "debug": {
        let msg = new Messages.ConsoleGeneric(message);
        node = msg.init(this.output).render().element;
        break;
      }
      case "table": {
        let msg = new Messages.ConsoleTable(message);
        node = msg.init(this.output).render().element;
        break;
      }
      case "trace": {
        let msg = new Messages.ConsoleTrace(message);
        node = msg.init(this.output).render().element;
        break;
      }
      case "clear": {
        body = l10n.getStr("consoleCleared");
        clipboardText = body;
        break;
      }
      case "dir": {
        body = { arguments: args };
        let clipboardArray = [];
        args.forEach((value) => {
          clipboardArray.push(VariablesView.getString(value));
        });
        clipboardText = clipboardArray.join(" ");
        break;
      }
      case "dirxml": {
        // We just alias console.dirxml() with console.log().
        message.level = "log";
        return this.logConsoleAPIMessage(message);
      }
      case "group":
      case "groupCollapsed":
        clipboardText = body = message.groupName;
        this.groupDepth++;
        break;

      case "groupEnd":
        if (this.groupDepth > 0) {
          this.groupDepth--;
        }
        break;

      case "time": {
        let timer = message.timer;
        if (!timer) {
          return null;
        }
        if (timer.error) {
          Cu.reportError(l10n.getStr(timer.error));
          return null;
        }
        body = l10n.getFormatStr("timerStarted", [timer.name]);
        clipboardText = body;
        break;
      }

      case "timeEnd": {
        let timer = message.timer;
        if (!timer) {
          return null;
        }
        let duration = Math.round(timer.duration * 100) / 100;
        body = l10n.getFormatStr("timeEnd", [timer.name, duration]);
        clipboardText = body;
        break;
      }

      case "count": {
        let counter = message.counter;
        if (!counter) {
          return null;
        }
        if (counter.error) {
          Cu.reportError(l10n.getStr(counter.error));
          return null;
        }
        let msg = new Messages.ConsoleGeneric(message);
        node = msg.init(this.output).render().element;
        break;
      }

      case "timeStamp": {
        // console.timeStamp() doesn't need to display anything.
        return null;
      }

      default:
        Cu.reportError("Unknown Console API log level: " + level);
        return null;
    }

    // Release object actors for arguments coming from console API methods that
    // we ignore their arguments.
    switch (level) {
      case "group":
      case "groupCollapsed":
      case "groupEnd":
      case "time":
      case "timeEnd":
      case "count":
        for (let actor of objectActors) {
          this._releaseObject(actor);
        }
        objectActors.clear();
    }

    if (level == "groupEnd") {
      // no need to continue
      return null;
    }

    if (!node) {
      node = this.createMessageNode(CATEGORY_WEBDEV, LEVELS[level], body,
                                    sourceURL, sourceLine, clipboardText,
                                    level, message.timeStamp);
      if (message.private) {
        node.setAttribute("private", true);
      }
    }

    if (objectActors.size > 0) {
      node._objectActors = objectActors;

      if (!node._messageObject) {
        let repeatNode = node.getElementsByClassName("message-repeats")[0];
        repeatNode._uid += [...objectActors].join("-");
      }
    }

    let workerTypeID = CONSOLE_WORKER_IDS.indexOf(message.workerType);
    if (workerTypeID != -1) {
      node.workerType = WORKERTYPES_PREFKEYS[workerTypeID];
      node.setAttribute("workerType", WORKERTYPES_PREFKEYS[workerTypeID]);
    }

    return node;
  },

  /**
   * Handle ConsoleAPICall objects received from the server. This method outputs
   * the window.console API call.
   *
   * @param object message
   *        The console API message received from the server.
   */
  handleConsoleAPICall: function(message) {
    this.outputMessage(CATEGORY_WEBDEV, this.logConsoleAPIMessage, [message]);
  },

  /**
   * Reports an error in the page source, either JavaScript or CSS.
   *
   * @param nsIScriptError scriptError
   *        The error message to report.
   * @return nsIDOMElement|undefined
   *         The message element to display in the Web Console output.
   */
  reportPageError: function(category, scriptError) {
    // Warnings and legacy strict errors become warnings; other types become
    // errors.
    let severity = "error";
    if (scriptError.warning || scriptError.strict) {
      severity = "warning";
    } else if (scriptError.info) {
      severity = "log";
    }

    switch (category) {
      case CATEGORY_CSS:
        category = "css";
        break;
      case CATEGORY_SECURITY:
        category = "security";
        break;
      default:
        category = "js";
        break;
    }

    let objectActors = new Set();

    // Gather the actor IDs.
    for (let prop of ["errorMessage", "lineText"]) {
      let grip = scriptError[prop];
      if (WebConsoleUtils.isActorGrip(grip)) {
        objectActors.add(grip.actor);
      }
    }

    let errorMessage = scriptError.errorMessage;
    if (errorMessage.type && errorMessage.type == "longString") {
      errorMessage = errorMessage.initial;
    }

    let displayOrigin = scriptError.sourceName;

    // TLS errors are related to the connection and not the resource; therefore
    // it makes sense to only display the protcol, host and port (prePath).
    // This also means messages are grouped for a single origin.
    if (scriptError.category && scriptError.category == "SHA-1 Signature") {
      let sourceURI = Services.io.newURI(scriptError.sourceName, null, null)
                      .QueryInterface(Ci.nsIURL);
      displayOrigin = sourceURI.prePath;
    }

    // Create a new message
    let msg = new Messages.Simple(errorMessage, {
      location: {
        url: displayOrigin,
        line: scriptError.lineNumber,
        column: scriptError.columnNumber
      },
      stack: scriptError.stacktrace,
      category: category,
      severity: severity,
      timestamp: scriptError.timeStamp,
      private: scriptError.private,
      filterDuplicates: true
    });

    let node = msg.init(this.output).render().element;

    // Select the body of the message node that is displayed in the console
    let msgBody = node.getElementsByClassName("message-body")[0];

    // Add the more info link node to messages that belong to certain categories
    this.addMoreInfoLink(msgBody, scriptError);

    // Collect telemetry data regarding JavaScript errors
    this._telemetry.logKeyed("DEVTOOLS_JAVASCRIPT_ERROR_DISPLAYED",
                             scriptError.errorMessageName,
                             true);

    if (objectActors.size > 0) {
      node._objectActors = objectActors;
    }

    return node;
  },

  /**
   * Handle PageError objects received from the server. This method outputs the
   * given error.
   *
   * @param nsIScriptError pageError
   *        The error received from the server.
   */
  handlePageError: function(pageError) {
    let category = Utils.categoryForScriptError(pageError);
    this.outputMessage(category, this.reportPageError, [category, pageError]);
  },

  /**
   * Handle log messages received from the server. This method outputs the given
   * message.
   *
   * @param object packet
   *        The message packet received from the server.
   */
  handleLogMessage: function(packet) {
    if (packet.message) {
      this.outputMessage(CATEGORY_JS, this._reportLogMessage, [packet]);
    }
  },

  /**
   * Display log messages received from the server.
   *
   * @private
   * @param object packet
   *        The message packet received from the server.
   * @return nsIDOMElement
   *         The message element to render for the given log message.
   */
  _reportLogMessage: function(packet) {
    let msg = packet.message;
    if (msg.type && msg.type == "longString") {
      msg = msg.initial;
    }
    let node = this.createMessageNode(CATEGORY_JS, SEVERITY_LOG, msg, null,
                                      null, null, null, packet.timeStamp);
    if (WebConsoleUtils.isActorGrip(packet.message)) {
      node._objectActors = new Set([packet.message.actor]);
    }
    return node;
  },

  /**
   * Log network event.
   *
   * @param object networkInfo
   *        The network request information to log.
   * @return nsIDOMElement|null
   *         The message element to display in the Web Console output.
   */
  logNetEvent: function(networkInfo) {
    let actorId = networkInfo.actor;
    let request = networkInfo.request;
    let clipboardText = request.method + " " + request.url;
    let severity = SEVERITY_LOG;
    if (networkInfo.isXHR) {
      clipboardText = request.method + " XHR " + request.url;
      severity = SEVERITY_INFO;
    }
    let mixedRequest =
      WebConsoleUtils.isMixedHTTPSRequest(request.url, this.contentLocation);
    if (mixedRequest) {
      severity = SEVERITY_WARNING;
    }

    let methodNode = this.document.createElementNS(XHTML_NS, "span");
    methodNode.className = "method";
    methodNode.textContent = request.method + " ";

    let messageNode = this.createMessageNode(CATEGORY_NETWORK, severity,
                                             methodNode, null, null,
                                             clipboardText, null,
                                             networkInfo.timeStamp);
    if (networkInfo.private) {
      messageNode.setAttribute("private", true);
    }
    messageNode._connectionId = actorId;
    messageNode.url = request.url;

    let body = methodNode.parentNode;
    body.setAttribute("aria-haspopup", true);

    if (networkInfo.isXHR) {
      let xhrNode = this.document.createElementNS(XHTML_NS, "span");
      xhrNode.className = "xhr";
      xhrNode.textContent = l10n.getStr("webConsoleXhrIndicator");
      body.appendChild(xhrNode);
      body.appendChild(this.document.createTextNode(" "));
    }

    let displayUrl = request.url;
    let pos = displayUrl.indexOf("?");
    if (pos > -1) {
      displayUrl = displayUrl.substr(0, pos);
    }

    let urlNode = this.document.createElementNS(XHTML_NS, "a");
    urlNode.className = "url";
    urlNode.setAttribute("title", request.url);
    urlNode.href = request.url;
    urlNode.textContent = displayUrl;
    urlNode.draggable = false;
    body.appendChild(urlNode);
    body.appendChild(this.document.createTextNode(" "));

    if (mixedRequest) {
      messageNode.classList.add("mixed-content");
      this.makeMixedContentNode(body);
    }

    let statusNode = this.document.createElementNS(XHTML_NS, "a");
    statusNode.className = "status";
    body.appendChild(statusNode);

    let onClick = () => this.openNetworkPanel(networkInfo.actor);

    this._addMessageLinkCallback(urlNode, onClick);
    this._addMessageLinkCallback(statusNode, onClick);

    networkInfo.node = messageNode;

    this._updateNetMessage(actorId);

    if (this.window.NetRequest) {
      this.window.NetRequest.onNetworkEvent({
        client: this.webConsoleClient,
        response: networkInfo,
        node: messageNode,
        update: false
      });
    }

    return messageNode;
  },

  /**
   * Create a mixed content warning Node.
   *
   * @param linkNode
   *        Parent to the requested urlNode.
   */
  makeMixedContentNode: function(linkNode) {
    let mixedContentWarning =
      "[" + l10n.getStr("webConsoleMixedContentWarning") + "]";

    // Mixed content warning message links to a Learn More page
    let mixedContentWarningNode = this.document.createElementNS(XHTML_NS, "a");
    mixedContentWarningNode.title = MIXED_CONTENT_LEARN_MORE;
    mixedContentWarningNode.href = MIXED_CONTENT_LEARN_MORE;
    mixedContentWarningNode.className = "learn-more-link";
    mixedContentWarningNode.textContent = mixedContentWarning;
    mixedContentWarningNode.draggable = false;

    linkNode.appendChild(mixedContentWarningNode);

    this._addMessageLinkCallback(mixedContentWarningNode, (event) => {
      event.stopPropagation();
      this.owner.openLink(MIXED_CONTENT_LEARN_MORE);
    });
  },

  /**
   * Adds a more info link node to messages based on the nsIScriptError object
   * that we need to report to the console
   *
   * @param node
   *        The node to which we will be adding the more info link node
   * @param scriptError
   *        The script error object that we are reporting to the console
   */
  addMoreInfoLink: function(node, scriptError) {
    let url;
    switch (scriptError.category) {
      case "Insecure Password Field":
        url = INSECURE_PASSWORDS_LEARN_MORE;
        break;
      case "Mixed Content Message":
      case "Mixed Content Blocker":
        url = MIXED_CONTENT_LEARN_MORE;
        break;
      case "Invalid HPKP Headers":
        url = PUBLIC_KEY_PINS_LEARN_MORE;
        break;
      case "Invalid HSTS Headers":
        url = STRICT_TRANSPORT_SECURITY_LEARN_MORE;
        break;
      case "SHA-1 Signature":
        url = WEAK_SIGNATURE_ALGORITHM_LEARN_MORE;
        break;
      case "Tracking Protection":
        url = TRACKING_PROTECTION_LEARN_MORE;
        break;
      default:
        // If all else fails check for an error doc URL.
        url = ErrorDocs.GetURL(scriptError.errorMessageName);
        break;
    }

    if (url) {
      this.addLearnMoreWarningNode(node, url);
    }
  },

  /*
   * Appends a clickable warning node to the node passed
   * as a parameter to the function. When a user clicks on the appended
   * warning node, the browser navigates to the provided url.
   *
   * @param node
   *        The node to which we will be adding a clickable warning node.
   * @param url
   *        The url which points to the page where the user can learn more
   *        about security issues associated with the specific message that's
   *        being logged.
   */
  addLearnMoreWarningNode: function(node, url) {
    let moreInfoLabel = "[" + l10n.getStr("webConsoleMoreInfoLabel") + "]";

    let warningNode = this.document.createElementNS(XHTML_NS, "a");
    warningNode.title = url;
    warningNode.href = url;
    warningNode.draggable = false;
    warningNode.textContent = moreInfoLabel;
    warningNode.className = "learn-more-link";

    this._addMessageLinkCallback(warningNode, (event) => {
      event.stopPropagation();
      this.owner.openLink(url);
    });

    node.appendChild(warningNode);
  },

  /**
   * Log file activity.
   *
   * @param string fileURI
   *        The file URI that was loaded.
   * @return nsIDOMElement|undefined
   *         The message element to display in the Web Console output.
   */
  logFileActivity: function(fileURI) {
    let urlNode = this.document.createElementNS(XHTML_NS, "a");
    urlNode.setAttribute("title", fileURI);
    urlNode.className = "url";
    urlNode.textContent = fileURI;
    urlNode.draggable = false;
    urlNode.href = fileURI;

    let outputNode = this.createMessageNode(CATEGORY_NETWORK, SEVERITY_LOG,
                                            urlNode, null, null, fileURI);

    this._addMessageLinkCallback(urlNode, () => {
      this.owner.viewSource(fileURI);
    });

    return outputNode;
  },

  /**
   * Handle the file activity messages coming from the remote Web Console.
   *
   * @param string fileURI
   *        The file URI that was requested.
   */
  handleFileActivity: function(fileURI) {
    this.outputMessage(CATEGORY_NETWORK, this.logFileActivity, [fileURI]);
  },

  /**
   * Handle the reflow activity messages coming from the remote Web Console.
   *
   * @param object msg
   *        An object holding information about a reflow batch.
   */
  logReflowActivity: function(message) {
    let {start, end, sourceURL, sourceLine} = message;
    let duration = Math.round((end - start) * 100) / 100;
    let node = this.document.createElementNS(XHTML_NS, "span");
    if (sourceURL) {
      node.textContent =
        l10n.getFormatStr("reflow.messageWithLink", [duration]);
      let a = this.document.createElementNS(XHTML_NS, "a");
      a.href = "#";
      a.draggable = "false";
      let filename = getSourceNames(sourceURL).short;
      let functionName = message.functionName ||
                         l10n.getStr("stacktrace.anonymousFunction");
      a.textContent = l10n.getFormatStr("reflow.messageLinkText",
                         [functionName, filename, sourceLine]);
      this._addMessageLinkCallback(a, () => {
        this.owner.viewSourceInDebugger(sourceURL, sourceLine);
      });
      node.appendChild(a);
    } else {
      node.textContent =
        l10n.getFormatStr("reflow.messageWithNoLink", [duration]);
    }
    return this.createMessageNode(CATEGORY_CSS, SEVERITY_LOG, node);
  },

  handleReflowActivity: function(message) {
    this.outputMessage(CATEGORY_CSS, this.logReflowActivity, [message]);
  },

  /**
   * Inform user that the window.console API has been replaced by a script
   * in a content page.
   */
  logWarningAboutReplacedAPI: function() {
    let node = this.createMessageNode(CATEGORY_JS, SEVERITY_WARNING,
                                      l10n.getStr("ConsoleAPIDisabled"));
    this.outputMessage(CATEGORY_JS, node);
  },

  /**
   * Handle the network events coming from the remote Web Console.
   *
   * @param object networkInfo
   *        The network request information.
   */
  handleNetworkEvent: function(networkInfo) {
    this.outputMessage(CATEGORY_NETWORK, this.logNetEvent, [networkInfo]);
  },

  /**
   * Handle network event updates coming from the server.
   *
   * @param object networkInfo
   *        The network request information.
   * @param object packet
   *        Update details.
   */
  handleNetworkEventUpdate: function(networkInfo, packet) {
    if (networkInfo.node && this._updateNetMessage(packet.from)) {
      if (this.window.NetRequest) {
        this.window.NetRequest.onNetworkEvent({
          client: this.webConsoleClient,
          response: packet,
          node: networkInfo.node,
          update: true
        });
      }

      this.emit("new-messages", new Set([{
        update: true,
        node: networkInfo.node,
        response: packet,
      }]));
    }

    // For unit tests we pass the HTTP activity object to the test callback,
    // once requests complete.
    if (this.owner.lastFinishedRequestCallback &&
        networkInfo.updates.indexOf("responseContent") > -1 &&
        networkInfo.updates.indexOf("eventTimings") > -1) {
      this.owner.lastFinishedRequestCallback(networkInfo, this);
    }
  },

  /**
   * Update an output message to reflect the latest state of a network request,
   * given a network event actor ID.
   *
   * @private
   * @param string actorId
   *        The network event actor ID for which you want to update the message.
   * @return boolean
   *         |true| if the message node was updated, or |false| otherwise.
   */
  _updateNetMessage: function(actorId) {
    let networkInfo = this.webConsoleClient.getNetworkRequest(actorId);
    if (!networkInfo || !networkInfo.node) {
      return false;
    }

    let messageNode = networkInfo.node;
    let updates = networkInfo.updates;
    let hasEventTimings = updates.indexOf("eventTimings") > -1;
    let hasResponseStart = updates.indexOf("responseStart") > -1;
    let request = networkInfo.request;
    let methodText = (networkInfo.isXHR) ?
                     request.method + " XHR" : request.method;
    let response = networkInfo.response;
    let updated = false;

    if (hasEventTimings || hasResponseStart) {
      let status = [];
      if (response.httpVersion && response.status) {
        status = [response.httpVersion, response.status, response.statusText];
      }
      if (hasEventTimings) {
        status.push(l10n.getFormatStr("NetworkPanel.durationMS",
                                      [networkInfo.totalTime]));
      }
      let statusText = "[" + status.join(" ") + "]";

      let statusNode = messageNode.getElementsByClassName("status")[0];
      statusNode.textContent = statusText;

      messageNode.clipboardText = [methodText, request.url, statusText]
                                  .join(" ");

      if (hasResponseStart && response.status >= MIN_HTTP_ERROR_CODE &&
          response.status <= MAX_HTTP_ERROR_CODE) {
        this.setMessageType(messageNode, CATEGORY_NETWORK, SEVERITY_ERROR);
      }

      updated = true;
    }

    if (messageNode._netPanel) {
      messageNode._netPanel.update();
    }

    return updated;
  },

  /**
   * Opens the network monitor and highlights the specified request.
   *
   * @param string requestId
   *        The actor ID of the network request.
   */
  openNetworkPanel: function(requestId) {
    let toolbox = gDevTools.getToolbox(this.owner.target);
    // The browser console doesn't have a toolbox.
    if (!toolbox) {
      return;
    }
    return toolbox.selectTool("netmonitor").then(panel => {
      return panel.panelWin.NetMonitorController.inspectRequest(requestId);
    });
  },

  /**
   * Handler for page location changes.
   *
   * @param string uri
   *        New page location.
   * @param string title
   *        New page title.
   */
  onLocationChange: function(uri, title) {
    this.contentLocation = uri;
    if (this.owner.onLocationChange) {
      this.owner.onLocationChange(uri, title);
    }
  },

  /**
   * Handler for the tabNavigated notification.
   *
   * @param string event
   *        Event name.
   * @param object packet
   *        Notification packet received from the server.
   */
  handleTabNavigated: function(event, packet) {
    if (event == "will-navigate") {
      if (this.persistLog) {
        let marker = new Messages.NavigationMarker(packet, Date.now());
        this.output.addMessage(marker);
      } else {
        this.jsterm.clearOutput();
      }
    }

    if (packet.url) {
      this.onLocationChange(packet.url, packet.title);
    }

    if (event == "navigate" && !packet.nativeConsoleAPI) {
      this.logWarningAboutReplacedAPI();
    }
  },

  /**
   * Output a message node. This filters a node appropriately, then sends it to
   * the output, regrouping and pruning output as necessary.
   *
   * Note: this call is async - the given message node may not be displayed when
   * you call this method.
   *
   * @param integer category
   *        The category of the message you want to output. See the CATEGORY_*
   *        constants.
   * @param function|nsIDOMElement methodOrNode
   *        The method that creates the message element to send to the output or
   *        the actual element. If a method is given it will be bound to the HUD
   *        object and the arguments will be |args|.
   * @param array [args]
   *        If a method is given to output the message element then the method
   *        will be invoked with the list of arguments given here. The last
   *        object in this array should be the packet received from the
   *        back end.
   */
  outputMessage: function(category, methodOrNode, args) {
    if (!this._outputQueue.length) {
      // If the queue is empty we consider that now was the last output flush.
      // This avoid an immediate output flush when the timer executes.
      this._lastOutputFlush = Date.now();
    }

    this._outputQueue.push([category, methodOrNode, args]);

    this._initOutputTimer();
  },

  /**
   * Try to flush the output message queue. This takes the messages in the
   * output queue and displays them. Outputting stops at MESSAGES_IN_INTERVAL.
   * Further output is queued to happen later - see OUTPUT_INTERVAL.
   *
   * @private
   */
  _flushMessageQueue: function() {
    this._outputTimerInitialized = false;
    if (!this._outputTimer) {
      return;
    }

    let startTime = Date.now();
    let timeSinceFlush = startTime - this._lastOutputFlush;
    let shouldThrottle = this._outputQueue.length > MESSAGES_IN_INTERVAL &&
        timeSinceFlush < THROTTLE_UPDATES;

    // Determine how many messages we can display now.
    let toDisplay = Math.min(this._outputQueue.length, MESSAGES_IN_INTERVAL);

    // If there aren't any messages to display (because of throttling or an
    // empty queue), then take care of some cleanup. Destroy items that were
    // pruned from the outputQueue before being displayed.
    if (shouldThrottle || toDisplay < 1) {
      while (this._itemDestroyQueue.length) {
        if ((Date.now() - startTime) > MAX_CLEANUP_TIME) {
          break;
        }
        this._destroyItem(this._itemDestroyQueue.pop());
      }

      this._initOutputTimer();
      return;
    }

    // Try to prune the message queue.
    let shouldPrune = false;
    if (this._outputQueue.length > toDisplay && this._pruneOutputQueue()) {
      toDisplay = Math.min(this._outputQueue.length, toDisplay);
      shouldPrune = true;
    }

    let batch = this._outputQueue.splice(0, toDisplay);
    let outputNode = this.outputNode;
    let lastVisibleNode = null;
    let scrollNode = this.outputWrapper;
    let hudIdSupportsString = WebConsoleUtils.supportsString(this.hudId);

    // We won't bother to try to restore scroll position if this is showing
    // a lot of messages at once (and there are still items in the queue).
    // It is going to purge whatever you were looking at anyway.
    let scrolledToBottom =
      shouldPrune || Utils.isOutputScrolledToBottom(outputNode, scrollNode);

    // Output the current batch of messages.
    let messages = new Set();
    for (let i = 0; i < batch.length; i++) {
      let item = batch[i];
      let result = this._outputMessageFromQueue(hudIdSupportsString, item);
      if (result) {
        messages.add({
          node: result.isRepeated ? result.isRepeated : result.node,
          response: result.message,
          update: !!result.isRepeated,
        });

        if (result.visible && result.node == this.outputNode.lastChild) {
          lastVisibleNode = result.node;
        }
      }
    }

    let oldScrollHeight = 0;
    let removedNodes = 0;

    // Prune messages from the DOM, but only if needed.
    if (shouldPrune || !this._outputQueue.length) {
      // Only bother measuring the scrollHeight if not scrolled to bottom,
      // since the oldScrollHeight will not be used if it is.
      if (!scrolledToBottom) {
        oldScrollHeight = scrollNode.scrollHeight;
      }

      let categories = Object.keys(this._pruneCategoriesQueue);
      categories.forEach(function _pruneOutput(category) {
        removedNodes += this.pruneOutputIfNecessary(category);
      }, this);
      this._pruneCategoriesQueue = {};
    }

    let isInputOutput = lastVisibleNode &&
                        (lastVisibleNode.category == CATEGORY_INPUT ||
                         lastVisibleNode.category == CATEGORY_OUTPUT);

    // Scroll to the new node if it is not filtered, and if the output node is
    // scrolled at the bottom or if the new node is a jsterm input/output
    // message.
    if (lastVisibleNode && (scrolledToBottom || isInputOutput)) {
      Utils.scrollToVisible(lastVisibleNode);
    } else if (!scrolledToBottom && removedNodes > 0 &&
               oldScrollHeight != scrollNode.scrollHeight) {
      // If there were pruned messages and if scroll is not at the bottom, then
      // we need to adjust the scroll location.
      scrollNode.scrollTop -= oldScrollHeight - scrollNode.scrollHeight;
    }

    if (messages.size) {
      this.emit("new-messages", messages);
    }

    // If the output queue is empty, then run _flushCallback.
    if (this._outputQueue.length === 0 && this._flushCallback) {
      if (this._flushCallback() === false) {
        this._flushCallback = null;
      }
    }

    this._initOutputTimer();

    // Resize the output area in case a vertical scrollbar has been added
    this.resize();

    this._lastOutputFlush = Date.now();
  },

  /**
   * Initialize the output timer.
   * @private
   */
  _initOutputTimer: function() {
    let panelIsDestroyed = !this._outputTimer;
    let alreadyScheduled = this._outputTimerInitialized;
    let nothingToDo = !this._itemDestroyQueue.length &&
                      !this._outputQueue.length;

    // Don't schedule a callback in the following cases:
    if (panelIsDestroyed || alreadyScheduled || nothingToDo) {
      return;
    }

    this._outputTimerInitialized = true;
    this._outputTimer.initWithCallback(this._flushMessageQueue,
                                       OUTPUT_INTERVAL,
                                       Ci.nsITimer.TYPE_ONE_SHOT);
  },

  /**
   * Output a message from the queue.
   *
   * @private
   * @param nsISupportsString hudIdSupportsString
   *        The HUD ID as an nsISupportsString.
   * @param array item
   *        An item from the output queue - this item represents a message.
   * @return object
   *         An object that holds the following properties:
   *         - node: the DOM element of the message.
   *         - isRepeated: the DOM element of the original message, if this is
   *         a repeated message, otherwise null.
   *         - visible: boolean that tells if the message is visible.
   */
  _outputMessageFromQueue: function(hudIdSupportsString, item) {
    let [, methodOrNode, args] = item;

    // The last object in the args array should be message
    // object or response packet received from the server.
    let message = (args && args.length) ? args[args.length - 1] : null;

    let node = typeof methodOrNode == "function" ?
               methodOrNode.apply(this, args || []) :
               methodOrNode;
    if (!node) {
      return null;
    }

    let isFiltered = this.filterMessageNode(node);

    let isRepeated = this._filterRepeatedMessage(node);

    // If a clear message is processed while the webconsole is opened, the UI
    // should be cleared.
    if (message && message.level == "clear") {
      // Do not clear the consoleStorage here as it has been cleared already
      // by the clear method, only clear the UI.
      this.jsterm.clearOutput(false);
    }

    let visible = !isRepeated && !isFiltered;
    if (!isRepeated) {
      this.outputNode.appendChild(node);
      this._pruneCategoriesQueue[node.category] = true;

      let nodeID = node.getAttribute("id");
      Services.obs.notifyObservers(hudIdSupportsString,
                                   "web-console-message-created", nodeID);
    }

    if (node._onOutput) {
      node._onOutput();
      delete node._onOutput;
    }

    return {
      visible: visible,
      node: node,
      isRepeated: isRepeated,
      message: message
    };
  },

  /**
   * Prune the queue of messages to display. This avoids displaying messages
   * that will be removed at the end of the queue anyway.
   * @private
   */
  _pruneOutputQueue: function() {
    let nodes = {};

    // Group the messages per category.
    this._outputQueue.forEach(function(item, index) {
      let [category] = item;
      if (!(category in nodes)) {
        nodes[category] = [];
      }
      nodes[category].push(index);
    }, this);

    let pruned = 0;

    // Loop through the categories we found and prune if needed.
    for (let category in nodes) {
      let limit = Utils.logLimitForCategory(category);
      let indexes = nodes[category];
      if (indexes.length > limit) {
        let n = Math.max(0, indexes.length - limit);
        pruned += n;
        for (let i = n - 1; i >= 0; i--) {
          this._itemDestroyQueue.push(this._outputQueue[indexes[i]]);
          this._outputQueue.splice(indexes[i], 1);
        }
      }
    }

    return pruned;
  },

  /**
   * Destroy an item that was once in the outputQueue but isn't needed
   * after all.
   *
   * @private
   * @param array item
   *        The item you want to destroy.  Does not remove it from the output
   *        queue.
   */
  _destroyItem: function(item) {
    // TODO: handle object releasing in a more elegant way once all console
    // messages use the new API - bug 778766.
    let [category, methodOrNode, args] = item;
    if (typeof methodOrNode != "function" && methodOrNode._objectActors) {
      for (let actor of methodOrNode._objectActors) {
        this._releaseObject(actor);
      }
      methodOrNode._objectActors.clear();
    }

    if (methodOrNode == this.output._flushMessageQueue &&
        args[0]._objectActors) {
      for (let arg of args) {
        if (!arg._objectActors) {
          continue;
        }
        for (let actor of arg._objectActors) {
          this._releaseObject(actor);
        }
        arg._objectActors.clear();
      }
    }

    if (category == CATEGORY_NETWORK) {
      let connectionId = null;
      if (methodOrNode == this.logNetEvent) {
        connectionId = args[0].actor;
      } else if (typeof methodOrNode != "function") {
        connectionId = methodOrNode._connectionId;
      }
      if (connectionId &&
          this.webConsoleClient.hasNetworkRequest(connectionId)) {
        this.webConsoleClient.removeNetworkRequest(connectionId);
        this._releaseObject(connectionId);
      }
    } else if (category == CATEGORY_WEBDEV &&
               methodOrNode == this.logConsoleAPIMessage) {
      args[0].arguments.forEach((value) => {
        if (WebConsoleUtils.isActorGrip(value)) {
          this._releaseObject(value.actor);
        }
      });
    } else if (category == CATEGORY_JS &&
               methodOrNode == this.reportPageError) {
      let pageError = args[1];
      for (let prop of ["errorMessage", "lineText"]) {
        let grip = pageError[prop];
        if (WebConsoleUtils.isActorGrip(grip)) {
          this._releaseObject(grip.actor);
        }
      }
    } else if (category == CATEGORY_JS &&
               methodOrNode == this._reportLogMessage) {
      if (WebConsoleUtils.isActorGrip(args[0].message)) {
        this._releaseObject(args[0].message.actor);
      }
    }
  },

  /**
   * Cleans up a message via a node that may or may not
   * have actually been rendered in the DOM. Currently, only
   * cleans up React components.
   *
   * @param nsIDOMNode node
   *        The message node you want to clean up.
   */
  unmountMessage(node) {
    // Select all `.message-location` within this node to ensure we get
    // messages of stacktraces, which contain multiple location nodes.
    for (let locationNode of node.querySelectorAll(".message-location")) {
      this.ReactDOM.unmountComponentAtNode(locationNode);
    }
  },

  /**
   * Ensures that the number of message nodes of type category don't exceed that
   * category's line limit by removing old messages as needed.
   *
   * @param integer category
   *        The category of message nodes to prune if needed.
   * @return number
   *         The number of removed nodes.
   */
  pruneOutputIfNecessary: function(category) {
    let logLimit = Utils.logLimitForCategory(category);
    let messageNodes = this.outputNode.querySelectorAll(".message[category=" +
                       CATEGORY_CLASS_FRAGMENTS[category] + "]");
    let n = Math.max(0, messageNodes.length - logLimit);
    [...messageNodes].slice(0, n).forEach(this.removeOutputMessage, this);
    return n;
  },

  /**
   * Remove a given message from the output.
   *
   * @param nsIDOMNode node
   *        The message node you want to remove.
   */
  removeOutputMessage: function(node) {
    if (node._messageObject) {
      node._messageObject.destroy();
    }

    if (node._objectActors) {
      for (let actor of node._objectActors) {
        this._releaseObject(actor);
      }
      node._objectActors.clear();
    }

    if (node.category == CATEGORY_CSS ||
        node.category == CATEGORY_SECURITY) {
      let repeatNode = node.getElementsByClassName("message-repeats")[0];
      if (repeatNode && repeatNode._uid) {
        delete this._repeatNodes[repeatNode._uid];
      }
    } else if (node._connectionId &&
               node.category == CATEGORY_NETWORK) {
      this.webConsoleClient.removeNetworkRequest(node._connectionId);
      this._releaseObject(node._connectionId);
    } else if (node.classList.contains("inlined-variables-view")) {
      let view = node._variablesView;
      if (view) {
        view.controller.releaseActors();
      }
      node._variablesView = null;
    }

    this.unmountMessage(node);

    node.remove();
  },

  /**
   * Given a category and message body, creates a DOM node to represent an
   * incoming message. The timestamp is automatically added.
   *
   * @param number category
   *        The category of the message: one of the CATEGORY_* constants.
   * @param number severity
   *        The severity of the message: one of the SEVERITY_* constants;
   * @param string|nsIDOMNode body
   *        The body of the message, either a simple string or a DOM node.
   * @param string sourceURL [optional]
   *        The URL of the source file that emitted the error.
   * @param number sourceLine [optional]
   *        The line number on which the error occurred. If zero or omitted,
   *        there is no line number associated with this message.
   * @param string clipboardText [optional]
   *        The text that should be copied to the clipboard when this node is
   *        copied. If omitted, defaults to the body text. If `body` is not
   *        a string, then the clipboard text must be supplied.
   * @param number level [optional]
   *        The level of the console API message.
   * @param number timestamp [optional]
   *        The timestamp to use for this message node. If omitted, the current
   *        date and time is used.
   * @return nsIDOMNode
   *         The message node: a DIV ready to be inserted into the Web Console
   *         output node.
   */
  createMessageNode: function(category, severity, body, sourceURL, sourceLine,
                              clipboardText, level, timestamp) {
    if (typeof body != "string" && clipboardText == null && body.innerText) {
      clipboardText = body.innerText;
    }

    let indentNode = this.document.createElementNS(XHTML_NS, "span");
    indentNode.className = "indent";

    // Apply the current group by indenting appropriately.
    let indent = this.groupDepth * GROUP_INDENT;
    indentNode.style.width = indent + "px";

    // Make the icon container, which is a vertical box. Its purpose is to
    // ensure that the icon stays anchored at the top of the message even for
    // long multi-line messages.
    let iconContainer = this.document.createElementNS(XHTML_NS, "span");
    iconContainer.className = "icon";

    // Create the message body, which contains the actual text of the message.
    let bodyNode = this.document.createElementNS(XHTML_NS, "span");
    bodyNode.className = "message-body-wrapper message-body devtools-monospace";

    // Store the body text, since it is needed later for the variables view.
    let storedBody = body;
    // If a string was supplied for the body, turn it into a DOM node and an
    // associated clipboard string now.
    clipboardText = clipboardText ||
                     (body + (sourceURL ? " @ " + sourceURL : "") +
                              (sourceLine ? ":" + sourceLine : ""));

    timestamp = timestamp || Date.now();

    // Create the containing node and append all its elements to it.
    let node = this.document.createElementNS(XHTML_NS, "div");
    node.id = "console-msg-" + gSequenceId();
    node.className = "message";
    node.clipboardText = clipboardText;
    node.timestamp = timestamp;
    this.setMessageType(node, category, severity);

    if (body instanceof Ci.nsIDOMNode) {
      bodyNode.appendChild(body);
    } else {
      let str = undefined;
      if (level == "dir") {
        str = VariablesView.getString(body.arguments[0]);
      } else {
        str = body;
      }

      if (str !== undefined) {
        body = this.document.createTextNode(str);
        bodyNode.appendChild(body);
      }
    }

    // Add the message repeats node only when needed.
    let repeatNode = null;
    if (category != CATEGORY_INPUT &&
        category != CATEGORY_OUTPUT &&
        category != CATEGORY_NETWORK &&
        !(category == CATEGORY_CSS && severity == SEVERITY_LOG)) {
      repeatNode = this.document.createElementNS(XHTML_NS, "span");
      repeatNode.setAttribute("value", "1");
      repeatNode.className = "message-repeats";
      repeatNode.textContent = 1;
      repeatNode._uid = [bodyNode.textContent, category, severity, level,
                         sourceURL, sourceLine].join(":");
    }

    // Create the timestamp.
    let timestampNode = this.document.createElementNS(XHTML_NS, "span");
    timestampNode.className = "timestamp devtools-monospace";

    let timestampString = l10n.timestampString(timestamp);
    timestampNode.textContent = timestampString + " ";

    // Create the source location (e.g. www.example.com:6) that sits on the
    // right side of the message, if applicable.
    let locationNode;
    if (sourceURL && IGNORED_SOURCE_URLS.indexOf(sourceURL) == -1) {
      locationNode = this.createLocationNode({url: sourceURL,
                                              line: sourceLine});
    }

    node.appendChild(timestampNode);
    node.appendChild(indentNode);
    node.appendChild(iconContainer);

    // Display the variables view after the message node.
    if (level == "dir") {
      let options = {
        objectActor: storedBody.arguments[0],
        targetElement: bodyNode,
        hideFilterInput: true,
      };
      this.jsterm.openVariablesView(options).then((view) => {
        node._variablesView = view;
        if (node.classList.contains("hidden-message")) {
          node.classList.remove("hidden-message");
        }
      });

      node.classList.add("inlined-variables-view");
    }

    node.appendChild(bodyNode);
    if (repeatNode) {
      node.appendChild(repeatNode);
    }
    if (locationNode) {
      node.appendChild(locationNode);
    }
    node.appendChild(this.document.createTextNode("\n"));

    return node;
  },

  /**
   * Creates the anchor that displays the textual location of an incoming
   * message.
   *
   * @param {Object} aLocation
   *        An object containing url, line and column number of the message
   *        source (destructured).
   * @return {Element}
   *         The new anchor element, ready to be added to the message node.
   */
  createLocationNode: function({url, line, column}) {
    if (!url) {
      url = "";
    }

    let fullURL = url.split(" -> ").pop();
    let locationNode = this.document.createElementNS(XHTML_NS, "a");
    locationNode.draggable = false;
    locationNode.className = "message-location devtools-monospace";

    // Make the location clickable.
    let onClick = () => {
      let category = locationNode.parentNode.category;
      let target = null;

      if (category === CATEGORY_CSS) {
        target = "styleeditor";
      } else if (category === CATEGORY_JS || category === CATEGORY_WEBDEV) {
        target = "jsdebugger";
      } else if (/^Scratchpad\/\d+$/.test(url)) {
        target = "scratchpad";
      } else if (/\.js$/.test(fullURL)) {
        // If it ends in .js, let's attempt to open in debugger
        // anyway, as this falls back to normal view-source.
        target = "jsdebugger";
      }

      switch (target) {
        case "scratchpad":
          this.owner.viewSourceInScratchpad(url, line);
          return;
        case "jsdebugger":
          this.owner.viewSourceInDebugger(fullURL, line);
          return;
        case "styleeditor":
          this.owner.viewSourceInStyleEditor(fullURL, line);
          return;
      }
      // No matching tool found; use old school view-source
      this.owner.viewSource(fullURL, line);
    };

    this.ReactDOM.render(this.FrameView({
      frame: {
        source: fullURL,
        line,
        column,
      },
      onClick
    }), locationNode);

    return locationNode;
  },

  /**
   * Adjusts the category and severity of the given message.
   *
   * @param nsIDOMNode messageNode
   *        The message node to alter.
   * @param number category
   *        The category for the message; one of the CATEGORY_ constants.
   * @param number severity
   *        The severity for the message; one of the SEVERITY_ constants.
   * @return void
   */
  setMessageType: function(messageNode, category, severity) {
    messageNode.category = category;
    messageNode.severity = severity;
    messageNode.setAttribute("category", CATEGORY_CLASS_FRAGMENTS[category]);
    messageNode.setAttribute("severity", SEVERITY_CLASS_FRAGMENTS[severity]);
    messageNode.setAttribute("filter",
      MESSAGE_PREFERENCE_KEYS[category][severity]);
  },

  /**
   * Add the mouse event handlers needed to make a link.
   *
   * @private
   * @param nsIDOMNode node
   *        The node for which you want to add the event handlers.
   * @param function callback
   *        The function you want to invoke on click.
   */
  _addMessageLinkCallback: function(node, callback) {
    node.addEventListener("mousedown", (event) => {
      this._mousedown = true;
      this._startX = event.clientX;
      this._startY = event.clientY;
    }, false);

    node.addEventListener("click", (event) => {
      let mousedown = this._mousedown;
      this._mousedown = false;

      event.preventDefault();

      // Do not allow middle/right-click or 2+ clicks.
      if (event.detail != 1 || event.button != 0) {
        return;
      }

      // If this event started with a mousedown event and it ends at a different
      // location, we consider this text selection.
      if (mousedown &&
          (this._startX != event.clientX) &&
          (this._startY != event.clientY)) {
        this._startX = this._startY = undefined;
        return;
      }

      this._startX = this._startY = undefined;

      callback.call(this, event);
    }, false);
  },

  /**
   * Handler for the pref-changed event coming from the toolbox.
   * Currently this function only handles the timestamps preferences.
   *
   * @private
   * @param object event
   *        This parameter is a string that holds the event name
   *        pref-changed in this case.
   * @param object data
   *        This is the pref-changed data object.
  */
  _onToolboxPrefChanged: function(event, data) {
    if (data.pref == PREF_MESSAGE_TIMESTAMP) {
      if (data.newValue) {
        this.outputNode.classList.remove("hideTimestamps");
      } else {
        this.outputNode.classList.add("hideTimestamps");
      }
    }
  },

  /**
   * Copies the selected items to the system clipboard.
   *
   * @param object options
   *        - linkOnly:
   *        An optional flag to copy only URL without other meta-information.
   *        Default is false.
   *        - contextmenu:
   *        An optional flag to copy the last clicked item which brought
   *        up the context menu if nothing is selected. Default is false.
   */
  copySelectedItems: function(options) {
    options = options || { linkOnly: false, contextmenu: false };

    // Gather up the selected items and concatenate their clipboard text.
    let strings = [];

    let children = this.output.getSelectedMessages();
    if (!children.length && options.contextmenu) {
      children = [this._contextMenuHandler.lastClickedMessage];
    }

    for (let item of children) {
      // Ensure the selected item hasn't been filtered by type or string.
      if (!item.classList.contains("filtered-by-type") &&
          !item.classList.contains("filtered-by-string")) {
        if (options.linkOnly) {
          strings.push(item.url);
        } else {
          strings.push(item.clipboardText);
        }
      }
    }

    clipboardHelper.copyString(strings.join("\n"));
  },

  /**
   * Object properties provider. This function gives you the properties of the
   * remote object you want.
   *
   * @param string actor
   *        The object actor ID from which you want the properties.
   * @param function callback
   *        Function you want invoked once the properties are received.
   */
  objectPropertiesProvider: function(actor, callback) {
    this.webConsoleClient.inspectObjectProperties(actor,
      function(response) {
        if (response.error) {
          Cu.reportError("Failed to retrieve the object properties from the " +
                         "server. Error: " + response.error);
          return;
        }
        callback(response.properties);
      });
  },

  /**
   * Release an actor.
   *
   * @private
   * @param string actor
   *        The actor ID you want to release.
   */
  _releaseObject: function(actor) {
    if (this.proxy) {
      this.proxy.releaseActor(actor);
    }
  },

  /**
   * Open the selected item's URL in a new tab.
   */
  openSelectedItemInTab: function() {
    let item = this.output.getSelectedMessages(1)[0] ||
               this._contextMenuHandler.lastClickedMessage;

    if (!item || !item.url) {
      return;
    }

    this.owner.openLink(item.url);
  },

  /**
   * Destroy the WebConsoleFrame object. Call this method to avoid memory leaks
   * when the Web Console is closed.
   *
   * @return object
   *         A promise that is resolved when the WebConsoleFrame instance is
   *         destroyed.
   */
  destroy: function() {
    if (this._destroyer) {
      return this._destroyer.promise;
    }

    this._destroyer = promise.defer();

    let toolbox = gDevTools.getToolbox(this.owner.target);
    if (toolbox) {
      toolbox.off("webconsole-selected", this._onPanelSelected);
    }

    gDevTools.off("pref-changed", this._onToolboxPrefChanged);
    this.window.removeEventListener("resize", this.resize, true);

    this._repeatNodes = {};
    this._outputQueue.forEach(this._destroyItem, this);
    this._outputQueue = [];
    this._itemDestroyQueue.forEach(this._destroyItem, this);
    this._itemDestroyQueue = [];
    this._pruneCategoriesQueue = {};
    this.webConsoleClient.clearNetworkRequests();

    // Unmount any currently living frame components in DOM, since
    // currently we only clean up messages in `this.removeOutputMessage`,
    // via `this.pruneOutputIfNecessary`.
    let liveMessages = this.outputNode.querySelectorAll(".message");
    Array.prototype.forEach.call(liveMessages, this.unmountMessage);

    if (this._outputTimerInitialized) {
      this._outputTimerInitialized = false;
      this._outputTimer.cancel();
    }
    this._outputTimer = null;
    if (this.jsterm) {
      this.jsterm.off("sidebar-opened", this.resize);
      this.jsterm.off("sidebar-closed", this.resize);
      this.jsterm.destroy();
      this.jsterm = null;
    }
    this.output.destroy();
    this.output = null;

    this.React = this.ReactDOM = this.FrameView = null;

    if (this._contextMenuHandler) {
      this._contextMenuHandler.destroy();
      this._contextMenuHandler = null;
    }

    this._commandController = null;

    let onDestroy = () => {
      this._destroyer.resolve(null);
    };

    if (this.proxy) {
      this.proxy.disconnect().then(onDestroy);
      this.proxy = null;
    } else {
      onDestroy();
    }

    return this._destroyer.promise;
  },
};

/**
 * Utils: a collection of globally used functions.
 */
var Utils = {
  /**
   * Scrolls a node so that it's visible in its containing element.
   *
   * @param nsIDOMNode node
   *        The node to make visible.
   * @returns void
   */
  scrollToVisible: function(node) {
    node.scrollIntoView(false);
  },

  /**
   * Check if the given output node is scrolled to the bottom.
   *
   * @param nsIDOMNode outputNode
   * @param nsIDOMNode scrollNode
   * @return boolean
   *         True if the output node is scrolled to the bottom, or false
   *         otherwise.
   */
  isOutputScrolledToBottom: function(outputNode, scrollNode) {
    let lastNodeHeight = outputNode.lastChild ?
                         outputNode.lastChild.clientHeight : 0;
    return scrollNode.scrollTop + scrollNode.clientHeight >=
           scrollNode.scrollHeight - lastNodeHeight / 2;
  },

  /**
   * Determine the category of a given nsIScriptError.
   *
   * @param nsIScriptError scriptError
   *        The script error you want to determine the category for.
   * @return CATEGORY_JS|CATEGORY_CSS|CATEGORY_SECURITY
   *         Depending on the script error CATEGORY_JS, CATEGORY_CSS, or
   *         CATEGORY_SECURITY can be returned.
   */
  categoryForScriptError: function(scriptError) {
    let category = scriptError.category;

    if (/^(?:CSS|Layout)\b/.test(category)) {
      return CATEGORY_CSS;
    }

    switch (category) {
      case "Mixed Content Blocker":
      case "Mixed Content Message":
      case "CSP":
      case "Invalid HSTS Headers":
      case "Invalid HPKP Headers":
      case "SHA-1 Signature":
      case "Insecure Password Field":
      case "SSL":
      case "CORS":
      case "Iframe Sandbox":
      case "Tracking Protection":
      case "Sub-resource Integrity":
        return CATEGORY_SECURITY;

      default:
        return CATEGORY_JS;
    }
  },

  /**
   * Retrieve the limit of messages for a specific category.
   *
   * @param number category
   *        The category of messages you want to retrieve the limit for. See the
   *        CATEGORY_* constants.
   * @return number
   *         The number of messages allowed for the specific category.
   */
  logLimitForCategory: function(category) {
    let logLimit = DEFAULT_LOG_LIMIT;

    try {
      let prefName = CATEGORY_CLASS_FRAGMENTS[category];
      logLimit = Services.prefs.getIntPref("devtools.hud.loglimit." + prefName);
      logLimit = Math.max(logLimit, 1);
    } catch (e) {
      // Ignore any exceptions
    }

    return logLimit;
  },
};

// ////////////////////////////////////////////////////////////////////////////
// CommandController
// ////////////////////////////////////////////////////////////////////////////

/**
 * A controller (an instance of nsIController) that makes editing actions
 * behave appropriately in the context of the Web Console.
 */
function CommandController(webConsole) {
  this.owner = webConsole;
}

CommandController.prototype = {
  /**
   * Selects all the text in the HUD output.
   */
  selectAll: function() {
    this.owner.output.selectAllMessages();
  },

  /**
   * Open the URL of the selected message in a new tab.
   */
  openURL: function() {
    this.owner.openSelectedItemInTab();
  },

  copyURL: function() {
    this.owner.copySelectedItems({ linkOnly: true, contextmenu: true });
  },

  /**
   * Copies the last clicked message.
   */
  copyLastClicked: function() {
    this.owner.copySelectedItems({ linkOnly: false, contextmenu: true });
  },

  supportsCommand: function(command) {
    if (!this.owner || !this.owner.output) {
      return false;
    }
    return this.isCommandEnabled(command);
  },

  isCommandEnabled: function(command) {
    switch (command) {
      case "consoleCmd_openURL":
      case "consoleCmd_copyURL": {
        // Only enable URL-related actions if node is Net Activity.
        let selectedItem = this.owner.output.getSelectedMessages(1)[0] ||
                           this.owner._contextMenuHandler.lastClickedMessage;
        return selectedItem && "url" in selectedItem;
      }
      case "cmd_copy": {
        // Only copy if we right-clicked the console and there's no selected
        // text. With text selected, we want to fall back onto the default
        // copy behavior.
        return this.owner._contextMenuHandler.lastClickedMessage &&
              !this.owner.output.getSelectedMessages(1)[0];
      }
      case "consoleCmd_clearOutput":
      case "cmd_selectAll":
      case "cmd_find":
        return true;
      case "cmd_fontSizeEnlarge":
      case "cmd_fontSizeReduce":
      case "cmd_fontSizeReset":
      case "cmd_close":
        return this.owner.owner._browserConsole;
    }
    return false;
  },

  doCommand: function(command) {
    switch (command) {
      case "consoleCmd_openURL":
        this.openURL();
        break;
      case "consoleCmd_copyURL":
        this.copyURL();
        break;
      case "consoleCmd_clearOutput":
        this.owner.jsterm.clearOutput(true);
        break;
      case "cmd_copy":
        this.copyLastClicked();
        break;
      case "cmd_find":
        this.owner.filterBox.focus();
        break;
      case "cmd_selectAll":
        this.selectAll();
        break;
      case "cmd_fontSizeEnlarge":
        this.owner.changeFontSize("+");
        break;
      case "cmd_fontSizeReduce":
        this.owner.changeFontSize("-");
        break;
      case "cmd_fontSizeReset":
        this.owner.changeFontSize("");
        break;
      case "cmd_close":
        this.owner.window.close();
        break;
    }
  }
};

// ////////////////////////////////////////////////////////////////////////////
// Web Console connection proxy
// ////////////////////////////////////////////////////////////////////////////

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
  connect: function() {
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
  _connectionTimeout: function() {
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
  _attachConsole: function() {
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
  _onAttachConsole: function(response, webConsoleClient) {
    if (response.error) {
      Cu.reportError("attachConsole failed: " + response.error + " " +
                     response.message);
      this._connectDefer.reject(response);
      return;
    }

    this.webConsoleClient = webConsoleClient;
    this._hasNativeConsoleAPI = response.nativeConsoleAPI;

    // There is no way to view response bodies from the Browser Console, so do
    // not waste the memory.
    let saveBodies = !this.webConsoleFrame.owner._browserConsole;
    this.webConsoleFrame.setSaveRequestAndResponseBodies(saveBodies);

    this.webConsoleClient.on("networkEvent", this._onNetworkEvent);
    this.webConsoleClient.on("networkEventUpdate", this._onNetworkEventUpdate);

    let msgs = ["PageError", "ConsoleAPI"];
    this.webConsoleClient.getCachedMessages(msgs, this._onCachedMessages);

    this.webConsoleFrame._onUpdateListeners();
  },

  /**
   * The "cachedMessages" response handler.
   *
   * @private
   * @param object response
   *        The JSON response object received from the server.
   */
  _onCachedMessages: function(response) {
    if (response.error) {
      Cu.reportError("Web Console getCachedMessages error: " + response.error +
                     " " + response.message);
      this._connectDefer.reject(response);
      return;
    }

    if (!this._connectTimer) {
      // This happens if the promise is rejected (eg. a timeout), but the
      // connection attempt is successful, nonetheless.
      Cu.reportError("Web Console getCachedMessages error: invalid state.");
    }

    let messages =
      response.messages.concat(...this.webConsoleClient.getNetworkEvents());
    messages.sort((a, b) => a.timeStamp - b.timeStamp);

    this.webConsoleFrame.displayCachedMessages(messages);

    if (!this._hasNativeConsoleAPI) {
      this.webConsoleFrame.logWarningAboutReplacedAPI();
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
    if (this.webConsoleFrame && packet.from == this._consoleActor) {
      if (this.webConsoleFrame.NEW_CONSOLE_OUTPUT_ENABLED) {
        this.webConsoleFrame.newConsoleOutput.dispatchMessageAdd(packet);
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
  _onLogMessage: function(type, packet) {
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
  _onConsoleAPICall: function(type, packet) {
    if (this.webConsoleFrame && packet.from == this._consoleActor) {
      if (this.webConsoleFrame.NEW_CONSOLE_OUTPUT_ENABLED) {
        this.webConsoleFrame.newConsoleOutput.dispatchMessageAdd(packet);
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
  _onNetworkEvent: function(type, networkInfo) {
    if (this.webConsoleFrame) {
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
   * @param object packet
   *        The message received from the server.
   * @param object networkInfo
   *        The network request information.
   */
  _onNetworkEventUpdate: function(type, { packet, networkInfo }) {
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
  _onFileActivity: function(type, packet) {
    if (this.webConsoleFrame && packet.from == this._consoleActor) {
      this.webConsoleFrame.handleFileActivity(packet.uri);
    }
  },

  _onReflowActivity: function(type, packet) {
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
  _onServerLogCall: function(type, packet) {
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
  _onLastPrivateContextExited: function(type, packet) {
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
  _onTabNavigated: function(event, packet) {
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
  disconnect: function() {
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

// ////////////////////////////////////////////////////////////////////////////
// Context Menu
// ////////////////////////////////////////////////////////////////////////////

/*
 * ConsoleContextMenu this used to handle the visibility of context menu items.
 *
 * @constructor
 * @param object owner
 *        The WebConsoleFrame instance that owns this object.
 */
function ConsoleContextMenu(owner) {
  this.owner = owner;
  this.popup = this.owner.document.getElementById("output-contextmenu");
  this.build = this.build.bind(this);
  this.popup.addEventListener("popupshowing", this.build);
}

ConsoleContextMenu.prototype = {
  lastClickedMessage: null,

  /*
   * Handle to show/hide context menu item.
   */
  build: function(event) {
    let metadata = this.getSelectionMetadata(event.rangeParent);
    for (let element of this.popup.children) {
      element.hidden = this.shouldHideMenuItem(element, metadata);
    }
  },

  /*
   * Get selection information from the view.
   *
   * @param nsIDOMElement clickElement
   *        The DOM element the user clicked on.
   * @return object
   *         Selection metadata.
   */
  getSelectionMetadata: function(clickElement) {
    let metadata = {
      selectionType: "",
      selection: new Set(),
    };
    let selectedItems = this.owner.output.getSelectedMessages();
    if (!selectedItems.length) {
      let clickedItem = this.owner.output.getMessageForElement(clickElement);
      if (clickedItem) {
        this.lastClickedMessage = clickedItem;
        selectedItems = [clickedItem];
      }
    }

    metadata.selectionType = selectedItems.length > 1 ? "multiple" : "single";

    let selection = metadata.selection;
    for (let item of selectedItems) {
      switch (item.category) {
        case CATEGORY_NETWORK:
          selection.add("network");
          break;
        case CATEGORY_CSS:
          selection.add("css");
          break;
        case CATEGORY_JS:
          selection.add("js");
          break;
        case CATEGORY_WEBDEV:
          selection.add("webdev");
          break;
        case CATEGORY_SERVER:
          selection.add("server");
          break;
      }
    }

    return metadata;
  },

  /*
   * Determine if an item should be hidden.
   *
   * @param nsIDOMElement menuItem
   * @param object metadata
   * @return boolean
   *         Whether the given item should be hidden or not.
   */
  shouldHideMenuItem: function(menuItem, metadata) {
    let selectionType = menuItem.getAttribute("selectiontype");
    if (selectionType && !metadata.selectionType == selectionType) {
      return true;
    }

    let selection = menuItem.getAttribute("selection");
    if (!selection) {
      return false;
    }

    let shouldHide = true;
    let itemData = selection.split("|");
    for (let type of metadata.selection) {
      // check whether this menu item should show or not.
      if (itemData.indexOf(type) !== -1) {
        shouldHide = false;
        break;
      }
    }

    return shouldHide;
  },

  /**
   * Destroy the ConsoleContextMenu object instance.
   */
  destroy: function() {
    this.popup.removeEventListener("popupshowing", this.build);
    this.popup = null;
    this.owner = null;
    this.lastClickedMessage = null;
  },
};
