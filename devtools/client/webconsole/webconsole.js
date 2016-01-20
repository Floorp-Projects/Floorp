/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cu} = require("chrome");

const {Utils: WebConsoleUtils, CONSOLE_WORKER_IDS} = require("devtools/shared/webconsole/utils");
const promise = require("promise");

loader.lazyServiceGetter(this, "clipboardHelper",
                         "@mozilla.org/widget/clipboardhelper;1",
                         "nsIClipboardHelper");
loader.lazyImporter(this, "Services", "resource://gre/modules/Services.jsm");
loader.lazyRequireGetter(this, "EventEmitter", "devtools/shared/event-emitter");
loader.lazyRequireGetter(this, "AutocompletePopup", "devtools/client/shared/autocomplete-popup", true);
loader.lazyRequireGetter(this, "ToolSidebar", "devtools/client/framework/sidebar", true);
loader.lazyRequireGetter(this, "ConsoleOutput", "devtools/client/webconsole/console-output", true);
loader.lazyRequireGetter(this, "Messages", "devtools/client/webconsole/console-output", true);
loader.lazyRequireGetter(this, "asyncStorage", "devtools/shared/async-storage");
loader.lazyRequireGetter(this, "EnvironmentClient", "devtools/shared/client/main", true);
loader.lazyRequireGetter(this, "ObjectClient", "devtools/shared/client/main", true);
loader.lazyRequireGetter(this, "system", "devtools/shared/system");
loader.lazyRequireGetter(this, "Timers", "sdk/timers");
loader.lazyImporter(this, "VariablesView", "resource://devtools/client/shared/widgets/VariablesView.jsm");
loader.lazyImporter(this, "VariablesViewController", "resource://devtools/client/shared/widgets/VariablesViewController.jsm");
loader.lazyImporter(this, "PluralForm", "resource://gre/modules/PluralForm.jsm");
loader.lazyImporter(this, "gDevTools", "resource://devtools/client/framework/gDevTools.jsm");

const STRINGS_URI = "chrome://devtools/locale/webconsole.properties";
var l10n = new WebConsoleUtils.l10n(STRINGS_URI);

const XHTML_NS = "http://www.w3.org/1999/xhtml";

const MIXED_CONTENT_LEARN_MORE = "https://developer.mozilla.org/docs/Security/MixedContent";

const TRACKING_PROTECTION_LEARN_MORE = "https://developer.mozilla.org/Firefox/Privacy/Tracking_Protection";

const INSECURE_PASSWORDS_LEARN_MORE = "https://developer.mozilla.org/docs/Security/InsecurePasswords";

const PUBLIC_KEY_PINS_LEARN_MORE = "https://developer.mozilla.org/docs/Web/Security/Public_Key_Pinning";

const STRICT_TRANSPORT_SECURITY_LEARN_MORE = "https://developer.mozilla.org/docs/Security/HTTP_Strict_Transport_Security";

const WEAK_SIGNATURE_ALGORITHM_LEARN_MORE = "https://developer.mozilla.org/docs/Security/Weak_Signature_Algorithm";

const HELP_URL = "https://developer.mozilla.org/docs/Tools/Web_Console/Helpers";

const VARIABLES_VIEW_URL = "chrome://devtools/content/shared/widgets/VariablesView.xul";

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
const CATEGORY_INPUT = 4;   // always on
const CATEGORY_OUTPUT = 5;  // always on
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
// first by category (rows) and then by severity (columns).
//
// Most of these rather idiosyncratic names are historical and predate the
// division of message type into "category" and "severity".
const MESSAGE_PREFERENCE_KEYS = [
//  Error          Warning       Info          Log
  [ "network",     "netwarn",    "netxhr",     "networkinfo", ],  // Network
  [ "csserror",    "cssparser",  null,         "csslog",      ],  // CSS
  [ "exception",   "jswarn",     null,         "jslog",       ],  // JS
  [ "error",       "warn",       "info",       "log",         ],  // Web Developer
  [ null,          null,         null,         null,          ],  // Input
  [ null,          null,         null,         null,          ],  // Output
  [ "secerror",    "secwarn",    null,         null,          ],  // Security
  [ "servererror", "serverwarn", "serverinfo", "serverlog",   ],  // Server Logging
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
const WORKERTYPES_PREFKEYS = [ 'sharedworkers', 'serviceworkers', 'windowlessworkers' ];

// The lowest HTTP response code (inclusive) that is considered an error.
const MIN_HTTP_ERROR_CODE = 400;
// The highest HTTP response code (inclusive) that is considered an error.
const MAX_HTTP_ERROR_CODE = 599;

// Constants used for defining the direction of JSTerm input history navigation.
const HISTORY_BACK = -1;
const HISTORY_FORWARD = 1;

// The indent of a console group in pixels.
const GROUP_INDENT = 12;

// The number of messages to display in a single display update. If we display
// too many messages at once we slow down the Firefox UI too much.
const MESSAGES_IN_INTERVAL = DEFAULT_LOG_LIMIT;

// The delay between display updates - tells how often we should *try* to push
// new messages to screen. This value is optimistic, updates won't always
// happen. Keep this low so the Web Console output feels live.
const OUTPUT_INTERVAL = 20; // milliseconds

// The maximum amount of time that can be spent doing cleanup inside of the
// flush output callback.  If things don't get cleaned up in this time,
// then it will start again the next time it is called.
const MAX_CLEANUP_TIME = 10; // milliseconds

// When the output queue has more than MESSAGES_IN_INTERVAL items we throttle
// output updates to this number of milliseconds. So during a lot of output we
// update every N milliseconds given here.
const THROTTLE_UPDATES = 1000; // milliseconds

// The preference prefix for all of the Web Console filters.
const FILTER_PREFS_PREFIX = "devtools.webconsole.filter.";

// The minimum font size.
const MIN_FONT_SIZE = 10;

const PREF_CONNECTION_TIMEOUT = "devtools.debugger.remote-timeout";
const PREF_PERSISTLOG = "devtools.webconsole.persistlog";
const PREF_MESSAGE_TIMESTAMP = "devtools.webconsole.timestampMessages";
const PREF_INPUT_HISTORY_COUNT = "devtools.webconsole.inputHistoryCount";

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
function WebConsoleFrame(webConsoleOwner)
{
  this.owner = webConsoleOwner;
  this.hudId = this.owner.hudId;
  this.window = this.owner.iframeWindow;

  this._repeatNodes = {};
  this._outputQueue = [];
  this._itemDestroyQueue = [];
  this._pruneCategoriesQueue = {};
  this.filterPrefs = {};

  this.output = new ConsoleOutput(this);

  this._toggleFilter = this._toggleFilter.bind(this);
  this._onPanelSelected = this._onPanelSelected.bind(this);
  this._flushMessageQueue = this._flushMessageQueue.bind(this);
  this._onToolboxPrefChanged = this._onToolboxPrefChanged.bind(this);
  this._onUpdateListeners = this._onUpdateListeners.bind(this);

  this._outputTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  this._outputTimerInitialized = false;

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
  setSaveRequestAndResponseBodies:
  function WCF_setSaveRequestAndResponseBodies(value) {
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
      }
      else {
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
    return this.owner._browserConsole || Services.prefs.getBoolPref(PREF_PERSISTLOG);
  },

  /**
   * Initialize the WebConsoleFrame instance.
   * @return object
   *         A promise object that resolves once the frame is ready to use.
   */
  init: function()
  {
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
  _initConnection: function WCF__initConnection()
  {
    if (this._initDefer) {
      return this._initDefer.promise;
    }

    this._initDefer = promise.defer();
    this.proxy = new WebConsoleConnectionProxy(this, this.owner.target);

    this.proxy.connect().then(() => { // on success
      this._initDefer.resolve(this);
    }, (reason) => { // on failure
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
  _initUI: function WCF__initUI()
  {
    this.document = this.window.document;
    this.rootElement = this.document.documentElement;

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
    this.completeNode = doc.querySelector(".jsterm-complete-node");
    this.inputNode = doc.querySelector(".jsterm-input-node");

    this._setFilterTextBoxEvents();
    this._initFilterButtons();

    let fontSize = this.owner._browserConsole ?
                   Services.prefs.getIntPref("devtools.webconsole.fontSize") : 0;

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

    let clearButton = doc.getElementsByClassName("webconsole-clear-console-button")[0];
    clearButton.addEventListener("command", () => {
      this.owner._onClearButton();
      this.jsterm.clearOutput(true);
    });

    this.jsterm = new JSTerm(this);
    this.jsterm.init();

    let toolbox = gDevTools.getToolbox(this.owner.target);
    if (toolbox) {
      toolbox.on("webconsole-selected", this._onPanelSelected);
    }

    /*
     * Focus input line whenever the output area is clicked.
     * Reusing _addMEssageLinkCallback since it correctly filters
     * drag and select events.
     */
    this._addFocusCallback(this.outputNode, (evt) => {
      if ((evt.target.nodeName.toLowerCase() != "a") &&
          (evt.target.parentNode.nodeName.toLowerCase() != "a")) {
        this.jsterm.inputNode.focus();
      }
    });

    // Toggle the timestamp on preference change
    gDevTools.on("pref-changed", this._onToolboxPrefChanged);
    this._onToolboxPrefChanged("pref-changed", {
      pref: PREF_MESSAGE_TIMESTAMP,
      newValue: Services.prefs.getBoolPref(PREF_MESSAGE_TIMESTAMP),
    });

    // focus input node
    this.jsterm.inputNode.focus();
  },

  /**
   * Sets the focus to JavaScript input field when the web console tab is
   * selected or when there is a split console present.
   * @private
   */
  _onPanelSelected: function WCF__onPanelSelected(evt, id)
  {
    this.jsterm.inputNode.focus();
  },

  /**
   * Initialize the default filter preferences.
   * @private
   */
  _initDefaultFilterPrefs: function WCF__initDefaultFilterPrefs()
  {
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
  _updateReflowActivityListener:
    function WCF__updateReflowActivityListener(callback)
  {
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
  _updateServerLoggingListener:
    function WCF__updateServerLoggingListener(callback)
  {
    if (!this.webConsoleClient) {
      return;
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
  _setFilterTextBoxEvents: function WCF__setFilterTextBoxEvents()
  {
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
  _initFilterButtons: function WCF__initFilterButtons()
  {
    let categories = this.document
                     .querySelectorAll(".webconsole-filter-button[category]");
    Array.forEach(categories, function(button) {
      button.addEventListener("contextmenu", (event) => {
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

      let logging = this.document.querySelector("toolbarbutton[category=logging]");
      logging.removeAttribute("accesskey");

      let serverLogging = this.document.querySelector("toolbarbutton[category=server]");
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
  changeFontSize: function WCF_changeFontSize(size)
  {
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
      }
      else {
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
    }
    else {
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
  _updateCharSize: function WCF__updateCharSize()
  {
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
  _toggleFilter: function WCF__toggleFilter(event)
  {
    let target = event.target;
    let tagName = target.tagName;
    // Prevent toggle if generated from a contextmenu event (right click)
    let isRightClick = (event.button === 2); // right click is button 2;
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
        // Make sure the option is always unchecked when the CSS filter button is selected.
        // See bug 971798.
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
  _setMenuState: function WCF__setMenuState(target, state)
  {
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
  setFilterState: function WCF_setFilterState(toggleType, state)
  {
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
  getFilterState: function WCF_getFilterState(toggleType)
  {
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
  stringMatchesFilters: function WCF_stringMatchesFilters(str, filter)
  {
    if (!filter || !str) {
      return true;
    }

    let searchStr = str.toLowerCase();
    let filterStrings = filter.toLowerCase().split(/\s+/);
    return !filterStrings.some(function (f) {
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
  adjustVisibilityForMessageType:
  function WCF_adjustVisibilityForMessageType(prefKey, state)
  {
    let outputNode = this.outputNode;
    let doc = this.document;

    // Look for message nodes (".message") with the given preference key
    // (filter="error", filter="cssparser", etc.) and add or remove the
    // "filtered-by-type" class, which turns on or off the display.

    let attribute = WORKERTYPES_PREFKEYS.indexOf(prefKey) == -1
                      ? 'filter' : 'workerType';

    let xpath = ".//*[contains(@class, 'message') and " +
      "@" + attribute + "='" + prefKey + "']";
    let result = doc.evaluate(xpath, outputNode, null,
      Ci.nsIDOMXPathResult.UNORDERED_NODE_SNAPSHOT_TYPE, null);
    for (let i = 0; i < result.snapshotLength; i++) {
      let node = result.snapshotItem(i);
      if (state) {
        node.classList.remove("filtered-by-type");
      }
      else {
        node.classList.add("filtered-by-type");
      }
    }
  },

  /**
   * Turns the display of log nodes on and off appropriately to reflect the
   * adjustment of the search string.
   */
  adjustVisibilityOnSearchStringChange:
  function WCF_adjustVisibilityOnSearchStringChange()
  {
    let nodes = this.outputNode.getElementsByClassName("message");
    let searchString = this.filterBox.value;

    for (let i = 0, n = nodes.length; i < n; ++i) {
      let node = nodes[i];

      // hide nodes that match the strings
      let text = node.textContent;

      // if the text matches the words in aSearchString...
      if (this.stringMatchesFilters(text, searchString)) {
        node.classList.remove("filtered-by-string");
      }
      else {
        node.classList.add("filtered-by-string");
      }
    }
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
  filterMessageNode: function WCF_filterMessageNode(node)
  {
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
   * Merge the attributes of the two nodes that are about to be filtered.
   * Increment the number of repeats of original.
   *
   * @param nsIDOMNode original
   *        The Original Node. The one being merged into.
   * @param nsIDOMNode filtered
   *        The node being filtered out because it is repeated.
   */
  mergeFilteredMessageNode:
  function WCF_mergeFilteredMessageNode(original, filtered)
  {
    let repeatNode = original.getElementsByClassName("message-repeats")[0];
    if (!repeatNode) {
      return; // no repeat node, return early.
    }

    let occurrences = parseInt(repeatNode.getAttribute("value")) + 1;
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
  _filterRepeatedMessage: function WCF__filterRepeatedMessage(node)
  {
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
    }
    else if ((node.category == CATEGORY_WEBDEV ||
              node.category == CATEGORY_JS) &&
             node.category != CATEGORY_NETWORK &&
             !node.classList.contains("inlined-variables-view")) {
      let lastMessage = this.outputNode.lastChild;
      if (!lastMessage) {
        return null;
      }

      let lastRepeatNode = lastMessage.getElementsByClassName("message-repeats")[0];
      if (lastRepeatNode && lastRepeatNode._uid == uid) {
        dupeNode = lastMessage;
      }
    }

    if (dupeNode) {
      this.mergeFilteredMessageNode(dupeNode, node);
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
  displayCachedMessages: function WCF_displayCachedMessages(remoteMessages)
  {
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
  logConsoleAPIMessage: function WCF_logConsoleAPIMessage(message)
  {
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
        return WCF_logConsoleAPIMessage.call(this, message);
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
      return null; // no need to continue
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
      node.setAttribute('workerType', WORKERTYPES_PREFKEYS[workerTypeID]);
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
  handleConsoleAPICall: function WCF_handleConsoleAPICall(message)
  {
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
  reportPageError: function WCF_reportPageError(category, scriptError)
  {
    // Warnings and legacy strict errors become warnings; other types become
    // errors.
    let severity = 'error';
    if (scriptError.warning || scriptError.strict) {
      severity = 'warning';
    } else if (scriptError.info) {
      severity = 'log';
    }

    switch(category) {
      case CATEGORY_CSS:
        category = 'css';
        break;
      case CATEGORY_SECURITY:
        category = 'security';
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
      let sourceURI = Services.io.newURI(scriptError.sourceName, null, null).QueryInterface(Ci.nsIURL);
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
  handlePageError: function WCF_handlePageError(pageError)
  {
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
  handleLogMessage: function WCF_handleLogMessage(packet)
  {
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
  _reportLogMessage: function WCF__reportLogMessage(packet)
  {
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
  logNetEvent: function(networkInfo)
  {
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

    return messageNode;
  },

  /**
   * Create a mixed content warning Node.
   *
   * @param linkNode
   *        Parent to the requested urlNode.
   */
  makeMixedContentNode: function WCF_makeMixedContentNode(linkNode)
  {
    let mixedContentWarning = "[" + l10n.getStr("webConsoleMixedContentWarning") + "]";

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
  addMoreInfoLink: function WCF_addMoreInfoLink(node, scriptError)
  {
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
        // Unknown category. Return without adding more info node.
        return;
    }

    this.addLearnMoreWarningNode(node, url);
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
  addLearnMoreWarningNode:
  function WCF_addLearnMoreWarningNode(node, url)
  {
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
  logFileActivity: function WCF_logFileActivity(fileURI)
  {
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
  handleFileActivity: function WCF_handleFileActivity(fileURI)
  {
    this.outputMessage(CATEGORY_NETWORK, this.logFileActivity, [fileURI]);
  },

  /**
   * Handle the reflow activity messages coming from the remote Web Console.
   *
   * @param object msg
   *        An object holding information about a reflow batch.
   */
  logReflowActivity: function WCF_logReflowActivity(message)
  {
    let {start, end, sourceURL, sourceLine} = message;
    let duration = Math.round((end - start) * 100) / 100;
    let node = this.document.createElementNS(XHTML_NS, "span");
    if (sourceURL) {
      node.textContent = l10n.getFormatStr("reflow.messageWithLink", [duration]);
      let a = this.document.createElementNS(XHTML_NS, "a");
      a.href = "#";
      a.draggable = "false";
      let filename = WebConsoleUtils.abbreviateSourceURL(sourceURL);
      let functionName = message.functionName || l10n.getStr("stacktrace.anonymousFunction");
      a.textContent = l10n.getFormatStr("reflow.messageLinkText",
                         [functionName, filename, sourceLine]);
      this._addMessageLinkCallback(a, () => {
        this.owner.viewSourceInDebugger(sourceURL, sourceLine);
      });
      node.appendChild(a);
    } else {
      node.textContent = l10n.getFormatStr("reflow.messageWithNoLink", [duration]);
    }
    return this.createMessageNode(CATEGORY_CSS, SEVERITY_LOG, node);
  },


  handleReflowActivity: function WCF_handleReflowActivity(message)
  {
    this.outputMessage(CATEGORY_CSS, this.logReflowActivity, [message]);
  },

  /**
   * Inform user that the window.console API has been replaced by a script
   * in a content page.
   */
  logWarningAboutReplacedAPI: function WCF_logWarningAboutReplacedAPI()
  {
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
  handleNetworkEvent: function(networkInfo)
  {
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
  handleNetworkEventUpdate: function(networkInfo, packet)
  {
    if (networkInfo.node && this._updateNetMessage(packet.from)) {
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
  _updateNetMessage: function WCF__updateNetMessage(actorId)
  {
    let networkInfo = this.webConsoleClient.getNetworkRequest(actorId);
    if (!networkInfo || !networkInfo.node) {
      return;
    }

    let messageNode = networkInfo.node;
    let updates = networkInfo.updates;
    let hasEventTimings = updates.indexOf("eventTimings") > -1;
    let hasResponseStart = updates.indexOf("responseStart") > -1;
    let request = networkInfo.request;
    let methodText = (networkInfo.isXHR)? request.method + ' XHR' : request.method;
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
  openNetworkPanel: function WCF_openNetworkPanel(requestId)
  {
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
  onLocationChange: function WCF_onLocationChange(uri, title)
  {
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
  handleTabNavigated: function WCF_handleTabNavigated(event, packet)
  {
    if (event == "will-navigate") {
      if (this.persistLog) {
        let marker = new Messages.NavigationMarker(packet, Date.now());
        this.output.addMessage(marker);
      }
      else {
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
  outputMessage: function WCF_outputMessage(category, methodOrNode, args)
  {
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
  _flushMessageQueue: function WCF__flushMessageQueue()
  {
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
    let scrollNode = outputNode.parentNode;
    let hudIdSupportsString = WebConsoleUtils.supportsString(this.hudId);

    // We won't bother to try to restore scroll position if this is showing
    // a lot of messages at once (and there are still items in the queue).
    // It is going to purge whatever you were looking at anyway.
    let scrolledToBottom = shouldPrune ||
                           Utils.isOutputScrolledToBottom(outputNode);

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
    }
    else if (!scrolledToBottom && removedNodes > 0 &&
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

    this._lastOutputFlush = Date.now();
  },

  /**
   * Initialize the output timer.
   * @private
   */
  _initOutputTimer: function WCF__initOutputTimer()
  {
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
  _outputMessageFromQueue:
  function WCF__outputMessageFromQueue(hudIdSupportsString, item)
  {
    let [category, methodOrNode, args] = item;

    // The last object in the args array should be message
    // object or response packet received from the server.
    let message = (args && args.length) ? args[args.length-1] : null;

    let node = typeof methodOrNode == "function" ?
               methodOrNode.apply(this, args || []) :
               methodOrNode;
    if (!node) {
      return null;
    }

    let isFiltered = this.filterMessageNode(node);

    let isRepeated = this._filterRepeatedMessage(node);

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
  _pruneOutputQueue: function WCF__pruneOutputQueue()
  {
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
  _destroyItem: function WCF__destroyItem(item)
  {
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
      }
      else if (typeof methodOrNode != "function") {
        connectionId = methodOrNode._connectionId;
      }
      if (connectionId && this.webConsoleClient.hasNetworkRequest(connectionId)) {
        this.webConsoleClient.removeNetworkRequest(connectionId);
        this._releaseObject(connectionId);
      }
    }
    else if (category == CATEGORY_WEBDEV &&
             methodOrNode == this.logConsoleAPIMessage) {
      args[0].arguments.forEach((value) => {
        if (WebConsoleUtils.isActorGrip(value)) {
          this._releaseObject(value.actor);
        }
      });
    }
    else if (category == CATEGORY_JS &&
             methodOrNode == this.reportPageError) {
      let pageError = args[1];
      for (let prop of ["errorMessage", "lineText"]) {
        let grip = pageError[prop];
        if (WebConsoleUtils.isActorGrip(grip)) {
          this._releaseObject(grip.actor);
        }
      }
    }
    else if (category == CATEGORY_JS &&
             methodOrNode == this._reportLogMessage) {
      if (WebConsoleUtils.isActorGrip(args[0].message)) {
        this._releaseObject(args[0].message.actor);
      }
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
  pruneOutputIfNecessary: function WCF_pruneOutputIfNecessary(category)
  {
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
  removeOutputMessage: function WCF_removeOutputMessage(node)
  {
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
    }
    else if (node._connectionId &&
             node.category == CATEGORY_NETWORK) {
      this.webConsoleClient.removeNetworkRequest(node._connectionId);
      this._releaseObject(node._connectionId);
    }
    else if (node.classList.contains("inlined-variables-view")) {
      let view = node._variablesView;
      if (view) {
        view.controller.releaseActors();
      }
      node._variablesView = null;
    }

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
  createMessageNode:
  function WCF_createMessageNode(category, severity, body, sourceURL,
                                 sourceLine, clipboardText, level, timestamp)
  {
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
    }
    else {
      let str = undefined;
      if (level == "dir") {
        str = VariablesView.getString(body.arguments[0]);
      }
      else {
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
   * @param object aLocation
   *        An object containing url, line and column number of the message source (destructured).
   * @param string target [optional]
   *        Tells which tool to open the link with, on click. Supported tools:
   *        jsdebugger, styleeditor, scratchpad.
   * @return nsIDOMNode
   *         The new anchor element, ready to be added to the message node.
   */
  createLocationNode:
  function WCF_createLocationNode({url, line, column}, target)
  {
    if (!url) {
      url = "";
    }
    let locationNode = this.document.createElementNS(XHTML_NS, "a");
    let filenameNode = this.document.createElementNS(XHTML_NS, "span");

    // Create the text, which consists of an abbreviated version of the URL
    // Scratchpad URLs should not be abbreviated.
    let filename;
    let fullURL;
    let isScratchpad = false;

    if (/^Scratchpad\/\d+$/.test(url)) {
      filename = url;
      fullURL = url;
      isScratchpad = true;
    }
    else {
      fullURL = url.split(" -> ").pop();
      filename = WebConsoleUtils.abbreviateSourceURL(fullURL);
    }

    filenameNode.className = "filename";
    filenameNode.textContent = " " + (filename || l10n.getStr("unknownLocation"));
    locationNode.appendChild(filenameNode);

    locationNode.href = isScratchpad || !fullURL ? "#" : fullURL;
    locationNode.draggable = false;
    if (target) {
      locationNode.target = target;
    }
    locationNode.setAttribute("title", url);
    locationNode.className = "message-location theme-link devtools-monospace";

    // Make the location clickable.
    let onClick = () => {
      let target = locationNode.target;
      if (target == "scratchpad" || isScratchpad) {
        this.owner.viewSourceInScratchpad(url, line);
        return;
      }

      let category = locationNode.parentNode.category;
      if (target == "styleeditor" || category == CATEGORY_CSS) {
        this.owner.viewSourceInStyleEditor(fullURL, line);
      }
      else if (target == "jsdebugger" ||
               category == CATEGORY_JS || category == CATEGORY_WEBDEV) {
        this.owner.viewSourceInDebugger(fullURL, line);
      }
      else {
        this.owner.viewSource(fullURL, line);
      }
    };

    if (fullURL) {
      this._addMessageLinkCallback(locationNode, onClick);
    }

    if (line) {
      let lineNumberNode = this.document.createElementNS(XHTML_NS, "span");
      lineNumberNode.className = "line-number";
      lineNumberNode.textContent = ":" + line + (column >= 0 ? ":" + column : "");
      locationNode.appendChild(lineNumberNode);
      locationNode.sourceLine = line;
    }

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
  setMessageType:
  function WCF_setMessageType(messageNode, category, severity)
  {
    messageNode.category = category;
    messageNode.severity = severity;
    messageNode.setAttribute("category", CATEGORY_CLASS_FRAGMENTS[category]);
    messageNode.setAttribute("severity", SEVERITY_CLASS_FRAGMENTS[severity]);
    messageNode.setAttribute("filter", MESSAGE_PREFERENCE_KEYS[category][severity]);
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
  _addMessageLinkCallback: function WCF__addMessageLinkCallback(node, callback)
  {
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
          (this._startY != event.clientY))
      {
        this._startX = this._startY = undefined;
        return;
      }

      this._startX = this._startY = undefined;

      callback.call(this, event);
    }, false);
  },

  _addFocusCallback: function WCF__addFocusCallback(node, callback)
  {
    node.addEventListener("mousedown", (event) => {
      this._mousedown = true;
      this._startX = event.clientX;
      this._startY = event.clientY;
    }, false);

    node.addEventListener("click", (event) => {
      let mousedown = this._mousedown;
      this._mousedown = false;

      // Do not allow middle/right-click or 2+ clicks.
      if (event.detail != 1 || event.button != 0) {
        return;
      }

      // If this event started with a mousedown event and it ends at a different
      // location, we consider this text selection.
      // Add a fuzz modifier of two pixels in any direction to account for sloppy
      // clicking.
      if (mousedown &&
          (Math.abs(event.clientX - this._startX) >= 2) &&
          (Math.abs(event.clientY - this._startY) >= 1))
      {
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
  _onToolboxPrefChanged: function WCF__onToolboxPrefChanged(event, data)
  {
    if (data.pref == PREF_MESSAGE_TIMESTAMP) {
      if (data.newValue) {
        this.outputNode.classList.remove("hideTimestamps");
      }
      else {
        this.outputNode.classList.add("hideTimestamps");
      }
    }
  },

  /**
   * Copies the selected items to the system clipboard.
   *
   * @param object options
   *        - linkOnly:
   *        An optional flag to copy only URL without timestamp and
   *        other meta-information. Default is false.
   *        - contextmenu:
   *        An optional flag to copy the last clicked item which brought
   *        up the context menu if nothing is selected. Default is false.
   */
  copySelectedItems: function WCF_copySelectedItems(options)
  {
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
        let timestampString = l10n.timestampString(item.timestamp);
        if (options.linkOnly) {
          strings.push(item.url);
        }
        else {
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
  objectPropertiesProvider:
  function WCF_objectPropertiesProvider(actor, callback)
  {
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
  _releaseObject: function WCF__releaseObject(actor)
  {
    if (this.proxy) {
      this.proxy.releaseActor(actor);
    }
  },

  /**
   * Open the selected item's URL in a new tab.
   */
  openSelectedItemInTab: function WCF_openSelectedItemInTab()
  {
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
  destroy: function WCF_destroy()
  {
    if (this._destroyer) {
      return this._destroyer.promise;
    }

    this._destroyer = promise.defer();

    let toolbox = gDevTools.getToolbox(this.owner.target);
    if (toolbox) {
      toolbox.off("webconsole-selected", this._onPanelSelected);
    }

    gDevTools.off("pref-changed", this._onToolboxPrefChanged);

    this._repeatNodes = {};
    this._outputQueue.forEach(this._destroyItem, this);
    this._outputQueue = [];
    this._itemDestroyQueue.forEach(this._destroyItem, this);
    this._itemDestroyQueue = [];
    this._pruneCategoriesQueue = {};
    this.webConsoleClient.clearNetworkRequests();

    if (this._outputTimerInitialized) {
      this._outputTimerInitialized = false;
      this._outputTimer.cancel();
    }
    this._outputTimer = null;
    if (this.jsterm) {
      this.jsterm.destroy();
      this.jsterm = null;
    }
    this.output.destroy();
    this.output = null;

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
    }
    else {
      onDestroy();
    }

    return this._destroyer.promise;
  },
};


/**
 * @see VariablesView.simpleValueEvalMacro
 */
function simpleValueEvalMacro(item, currentString)
{
  return VariablesView.simpleValueEvalMacro(item, currentString, "_self");
};


/**
 * @see VariablesView.overrideValueEvalMacro
 */
function overrideValueEvalMacro(item, currentString)
{
  return VariablesView.overrideValueEvalMacro(item, currentString, "_self");
};


/**
 * @see VariablesView.getterOrSetterEvalMacro
 */
function getterOrSetterEvalMacro(item, currentString)
{
  return VariablesView.getterOrSetterEvalMacro(item, currentString, "_self");
}



/**
 * Create a JSTerminal (a JavaScript command line). This is attached to an
 * existing HeadsUpDisplay (a Web Console instance). This code is responsible
 * with handling command line input, code evaluation and result output.
 *
 * @constructor
 * @param object webConsoleFrame
 *        The WebConsoleFrame object that owns this JSTerm instance.
 */
function JSTerm(webConsoleFrame)
{
  this.hud = webConsoleFrame;
  this.hudId = this.hud.hudId;
  this.inputHistoryCount = Services.prefs.getIntPref(PREF_INPUT_HISTORY_COUNT);

  this.lastCompletion = { value: null };
  this._loadHistory();

  this._objectActorsInVariablesViews = new Map();

  this._keyPress = this._keyPress.bind(this);
  this._inputEventHandler = this._inputEventHandler.bind(this);
  this._focusEventHandler = this._focusEventHandler.bind(this);
  this._onKeypressInVariablesView = this._onKeypressInVariablesView.bind(this);
  this._blurEventHandler = this._blurEventHandler.bind(this);

  EventEmitter.decorate(this);
}

JSTerm.prototype = {
  SELECTED_FRAME: -1,

  /**
   * Load the console history from previous sessions.
   * @private
   */
  _loadHistory: function() {
    this.history = [];
    this.historyIndex = this.historyPlaceHolder = 0;

    this.historyLoaded = asyncStorage.getItem("webConsoleHistory").then(value => {
      if (Array.isArray(value)) {
        // Since it was gotten asynchronously, there could be items already in
        // the history.  It's not likely but stick them onto the end anyway.
        this.history = value.concat(this.history);

        // Holds the number of entries in history. This value is incremented in
        // this.execute().
        this.historyIndex = this.history.length;

        // Holds the index of the history entry that the user is currently viewing.
        // This is reset to this.history.length when this.execute() is invoked.
        this.historyPlaceHolder = this.history.length;
      }
    }, console.error);
  },

  /**
   * Clear the console history altogether.  Note that this will not affect
   * other consoles that are already opened (since they have their own copy),
   * but it will reset the array for all newly-opened consoles.
   * @returns Promise
   *          Resolves once the changes have been persisted.
   */
  clearHistory: function() {
    this.history = [];
    this.historyIndex = this.historyPlaceHolder = 0;
    return this.storeHistory();
  },

  /**
   * Stores the console history for future console instances.
   * @returns Promise
   *          Resolves once the changes have been persisted.
   */
  storeHistory: function() {
    return asyncStorage.setItem("webConsoleHistory", this.history);
  },

  /**
   * Stores the data for the last completion.
   * @type object
   */
  lastCompletion: null,

  /**
   * Array that caches the user input suggestions received from the server.
   * @private
   * @type array
   */
  _autocompleteCache: null,

  /**
   * The input that caused the last request to the server, whose response is
   * cached in the _autocompleteCache array.
   * @private
   * @type string
   */
  _autocompleteQuery: null,

  /**
   * The frameActorId used in the last autocomplete query. Whenever this changes
   * the autocomplete cache must be invalidated.
   * @private
   * @type string
   */
  _lastFrameActorId: null,

  /**
   * The Web Console sidebar.
   * @see this._createSidebar()
   * @see Sidebar.jsm
   */
  sidebar: null,

  /**
   * The Variables View instance shown in the sidebar.
   * @private
   * @type object
   */
  _variablesView: null,

  /**
   * Tells if you want the variables view UI updates to be lazy or not. Tests
   * disable lazy updates.
   *
   * @private
   * @type boolean
   */
  _lazyVariablesView: true,

  /**
   * Holds a map between VariablesView instances and sets of ObjectActor IDs
   * that have been retrieved from the server. This allows us to release the
   * objects when needed.
   *
   * @private
   * @type Map
   */
  _objectActorsInVariablesViews: null,

  /**
   * Last input value.
   * @type string
   */
  lastInputValue: "",

  /**
   * Tells if the input node changed since the last focus.
   *
   * @private
   * @type boolean
   */
  _inputChanged: false,

  /**
   * Tells if the autocomplete popup was navigated since the last open.
   *
   * @private
   * @type boolean
   */
  _autocompletePopupNavigated: false,

  /**
   * History of code that was executed.
   * @type array
   */
  history: null,
  autocompletePopup: null,
  inputNode: null,
  completeNode: null,

  /**
   * Getter for the element that holds the messages we display.
   * @type nsIDOMElement
   */
  get outputNode()
  {
    return this.hud.outputNode;
  },

  /**
   * Getter for the debugger WebConsoleClient.
   * @type object
   */
  get webConsoleClient()
  {
    return this.hud.webConsoleClient;
  },

  COMPLETE_FORWARD: 0,
  COMPLETE_BACKWARD: 1,
  COMPLETE_HINT_ONLY: 2,
  COMPLETE_PAGEUP: 3,
  COMPLETE_PAGEDOWN: 4,

  /**
   * Initialize the JSTerminal UI.
   */
  init: function JST_init()
  {
    let autocompleteOptions = {
      onSelect: this.onAutocompleteSelect.bind(this),
      onClick: this.acceptProposedCompletion.bind(this),
      panelId: "webConsole_autocompletePopup",
      listBoxId: "webConsole_autocompletePopupListBox",
      position: "before_start",
      theme: "auto",
      direction: "ltr",
      autoSelect: true
    };
    this.autocompletePopup = new AutocompletePopup(this.hud.document,
                                                   autocompleteOptions);

    let doc = this.hud.document;
    let inputContainer = doc.querySelector(".jsterm-input-container");
    this.completeNode = doc.querySelector(".jsterm-complete-node");
    this.inputNode = doc.querySelector(".jsterm-input-node");

    if (this.hud.owner._browserConsole &&
        !Services.prefs.getBoolPref("devtools.chrome.enabled")) {
      inputContainer.style.display = "none";
    }
    else {
      let okstring = l10n.getStr("selfxss.okstring");
      let msg = l10n.getFormatStr("selfxss.msg", [okstring]);
      this._onPaste = WebConsoleUtils.pasteHandlerGen(this.inputNode,
                                                      doc.getElementById("webconsole-notificationbox"),
                                                      msg, okstring);
      this.inputNode.addEventListener("keypress", this._keyPress, false);
      this.inputNode.addEventListener("paste", this._onPaste);
      this.inputNode.addEventListener("drop", this._onPaste);
      this.inputNode.addEventListener("input", this._inputEventHandler, false);
      this.inputNode.addEventListener("keyup", this._inputEventHandler, false);
      this.inputNode.addEventListener("focus", this._focusEventHandler, false);
    }

    this.hud.window.addEventListener("blur", this._blurEventHandler, false);
    this.lastInputValue && this.setInputValue(this.lastInputValue);
  },

  focus: function() {
    let inputNode = this.inputNode;
    if (!inputNode.getAttribute("focused")) {
      inputNode.focus();
    }
  },

  /**
   * The JavaScript evaluation response handler.
   *
   * @private
   * @param function [callback]
   *        Optional function to invoke when the evaluation result is added to
   *        the output.
   * @param object response
   *        The message received from the server.
   */
  _executeResultCallback:
  function JST__executeResultCallback(callback, response)
  {
    if (!this.hud) {
      return;
    }
    if (response.error) {
      Cu.reportError("Evaluation error " + response.error + ": " +
                     response.message);
      return;
    }
    let errorMessage = response.exceptionMessage;
    let result = response.result;
    let helperResult = response.helperResult;
    let helperHasRawOutput = !!(helperResult || {}).rawOutput;

    if (helperResult && helperResult.type) {
      switch (helperResult.type) {
        case "clearOutput":
          this.clearOutput();
          break;
        case "clearHistory":
          this.clearHistory();
          break;
        case "inspectObject":
          this.openVariablesView({
            label: VariablesView.getString(helperResult.object, { concise: true }),
            objectActor: helperResult.object,
          });
          break;
        case "error":
          try {
            errorMessage = l10n.getStr(helperResult.message);
          }
          catch (ex) {
            errorMessage = helperResult.message;
          }
          break;
        case "help":
          this.hud.owner.openLink(HELP_URL);
          break;
        case "copyValueToClipboard":
          clipboardHelper.copyString(helperResult.value);
          break;
      }
    }

    // Hide undefined results coming from JSTerm helper functions.
    if (!errorMessage && result && typeof result == "object" &&
        result.type == "undefined" &&
        helperResult && !helperHasRawOutput) {
      callback && callback();
      return;
    }

    let msg = new Messages.JavaScriptEvalOutput(response, errorMessage);
    this.hud.output.addMessage(msg);

    if (callback) {
      let oldFlushCallback = this.hud._flushCallback;
      this.hud._flushCallback = () => {
        callback(msg.element);
        if (oldFlushCallback) {
          oldFlushCallback();
          this.hud._flushCallback = oldFlushCallback;
          return true;
        }

        return false;
      };
    }

    msg._objectActors = new Set();

    if (WebConsoleUtils.isActorGrip(response.exception)) {
      msg._objectActors.add(response.exception.actor);
    }

    if (WebConsoleUtils.isActorGrip(result)) {
      msg._objectActors.add(result.actor);
    }
  },

  /**
   * Execute a string. Execution happens asynchronously in the content process.
   *
   * @param string [executeString]
   *        The string you want to execute. If this is not provided, the current
   *        user input is used - taken from |this.getInputValue()|.
   * @param function [callback]
   *        Optional function to invoke when the result is displayed.
   *        This is deprecated - please use the promise return value instead.
   * @returns Promise
   *          Resolves with the message once the result is displayed.
   */
  execute: function JST_execute(executeString, callback)
  {
    let deferred = promise.defer();
    let resultCallback = function(msg) {
      deferred.resolve(msg);
      if (callback) {
        callback(msg);
      }
    }

    // attempt to execute the content of the inputNode
    executeString = executeString || this.getInputValue();
    if (!executeString) {
      return;
    }

    let selectedNodeActor = null;
    let inspectorSelection = this.hud.owner.getInspectorSelection();
    if (inspectorSelection && inspectorSelection.nodeFront) {
      selectedNodeActor = inspectorSelection.nodeFront.actorID;
    }

    let message = new Messages.Simple(executeString, {
      category: "input",
      severity: "log",
    });
    this.hud.output.addMessage(message);
    let onResult = this._executeResultCallback.bind(this, resultCallback);

    let options = {
      frame: this.SELECTED_FRAME,
      selectedNodeActor: selectedNodeActor,
    };

    this.requestEvaluation(executeString, options).then(onResult, onResult);

    // Append a new value in the history of executed code, or overwrite the most
    // recent entry. The most recent entry may contain the last edited input
    // value that was not evaluated yet.
    this.history[this.historyIndex++] = executeString;
    this.historyPlaceHolder = this.history.length;

    if (this.history.length > this.inputHistoryCount) {
      this.history.splice(0, this.history.length - this.inputHistoryCount);
      this.historyIndex = this.historyPlaceHolder = this.history.length;
    }
    this.storeHistory();
    WebConsoleUtils.usageCount++;
    this.setInputValue("");
    this.clearCompletion();
    return deferred.promise;
  },

  /**
   * Request a JavaScript string evaluation from the server.
   *
   * @param string str
   *        String to execute.
   * @param object [options]
   *        Options for evaluation:
   *        - bindObjectActor: tells the ObjectActor ID for which you want to do
   *        the evaluation. The Debugger.Object of the OA will be bound to
   *        |_self| during evaluation, such that it's usable in the string you
   *        execute.
   *        - frame: tells the stackframe depth to evaluate the string in. If
   *        the jsdebugger is paused, you can pick the stackframe to be used for
   *        evaluation. Use |this.SELECTED_FRAME| to always pick the
   *        user-selected stackframe.
   *        If you do not provide a |frame| the string will be evaluated in the
   *        global content window.
   *        - selectedNodeActor: tells the NodeActor ID of the current selection in
   *        the Inspector, if such a selection exists. This is used by helper
   *        functions that can evaluate on the current selection.
   * @return object
   *         A promise object that is resolved when the server response is
   *         received.
   */
  requestEvaluation: function JST_requestEvaluation(str, options = {})
  {
    let deferred = promise.defer();

    function onResult(response) {
      if (!response.error) {
        deferred.resolve(response);
      }
      else {
        deferred.reject(response);
      }
    }

    let frameActor = null;
    if ("frame" in options) {
      frameActor = this.getFrameActor(options.frame);
    }

    let evalOptions = {
      bindObjectActor: options.bindObjectActor,
      frameActor: frameActor,
      selectedNodeActor: options.selectedNodeActor,
      selectedObjectActor: options.selectedObjectActor,
    };

    this.webConsoleClient.evaluateJSAsync(str, onResult, evalOptions);
    return deferred.promise;
  },

  /**
   * Retrieve the FrameActor ID given a frame depth.
   *
   * @param number frame
   *        Frame depth.
   * @return string|null
   *         The FrameActor ID for the given frame depth.
   */
  getFrameActor: function JST_getFrameActor(frame)
  {
    let state = this.hud.owner.getDebuggerFrames();
    if (!state) {
      return null;
    }

    let grip;
    if (frame == this.SELECTED_FRAME) {
      grip = state.frames[state.selected];
    }
    else {
      grip = state.frames[frame];
    }

    return grip ? grip.actor : null;
  },

  /**
   * Opens a new variables view that allows the inspection of the given object.
   *
   * @param object options
   *        Options for the variables view:
   *        - objectActor: grip of the ObjectActor you want to show in the
   *        variables view.
   *        - rawObject: the raw object you want to show in the variables view.
   *        - label: label to display in the variables view for inspected
   *        object.
   *        - hideFilterInput: optional boolean, |true| if you want to hide the
   *        variables view filter input.
   *        - targetElement: optional nsIDOMElement to append the variables view
   *        to. An iframe element is used as a container for the view. If this
   *        option is not used, then the variables view opens in the sidebar.
   *        - autofocus: optional boolean, |true| if you want to give focus to
   *        the variables view window after open, |false| otherwise.
   * @return object
   *         A promise object that is resolved when the variables view has
   *         opened. The new variables view instance is given to the callbacks.
   */
  openVariablesView: function JST_openVariablesView(options)
  {
    let onContainerReady = (window) => {
      let container = window.document.querySelector("#variables");
      let view = this._variablesView;
      if (!view || options.targetElement) {
        let viewOptions = {
          container: container,
          hideFilterInput: options.hideFilterInput,
        };
        view = this._createVariablesView(viewOptions);
        if (!options.targetElement) {
          this._variablesView = view;
          window.addEventListener("keypress", this._onKeypressInVariablesView);
        }
      }
      options.view = view;
      this._updateVariablesView(options);

      if (!options.targetElement && options.autofocus) {
        window.focus();
      }

      this.emit("variablesview-open", view, options);
      return view;
    };

    let openPromise;
    if (options.targetElement) {
      let deferred = promise.defer();
      openPromise = deferred.promise;
      let document = options.targetElement.ownerDocument;
      let iframe = document.createElementNS(XHTML_NS, "iframe");

      iframe.addEventListener("load", function onIframeLoad(event) {
        iframe.removeEventListener("load", onIframeLoad, true);
        iframe.style.visibility = "visible";
        deferred.resolve(iframe.contentWindow);
      }, true);

      iframe.flex = 1;
      iframe.style.visibility = "hidden";
      iframe.setAttribute("src", VARIABLES_VIEW_URL);
      options.targetElement.appendChild(iframe);
    }
    else {
      if (!this.sidebar) {
        this._createSidebar();
      }
      openPromise = this._addVariablesViewSidebarTab();
    }

    return openPromise.then(onContainerReady);
  },

  /**
   * Create the Web Console sidebar.
   *
   * @see devtools/framework/sidebar.js
   * @private
   */
  _createSidebar: function JST__createSidebar()
  {
    let tabbox = this.hud.document.querySelector("#webconsole-sidebar");
    this.sidebar = new ToolSidebar(tabbox, this, "webconsole");
    this.sidebar.show();
  },

  /**
   * Add the variables view tab to the sidebar.
   *
   * @private
   * @return object
   *         A promise object for the adding of the new tab.
   */
  _addVariablesViewSidebarTab: function JST__addVariablesViewSidebarTab()
  {
    let deferred = promise.defer();

    let onTabReady = () => {
      let window = this.sidebar.getWindowForTab("variablesview");
      deferred.resolve(window);
    };

    let tabPanel = this.sidebar.getTabPanel("variablesview");
    if (tabPanel) {
      if (this.sidebar.getCurrentTabID() == "variablesview") {
        onTabReady();
      }
      else {
        this.sidebar.once("variablesview-selected", onTabReady);
        this.sidebar.select("variablesview");
      }
    }
    else {
      this.sidebar.once("variablesview-ready", onTabReady);
      this.sidebar.addTab("variablesview", VARIABLES_VIEW_URL, true);
    }

    return deferred.promise;
  },

  /**
   * The keypress event handler for the Variables View sidebar. Currently this
   * is used for removing the sidebar when Escape is pressed.
   *
   * @private
   * @param nsIDOMEvent event
   *        The keypress DOM event object.
   */
  _onKeypressInVariablesView: function JST__onKeypressInVariablesView(event)
  {
    let tag = event.target.nodeName;
    if (event.keyCode != Ci.nsIDOMKeyEvent.DOM_VK_ESCAPE || event.shiftKey ||
        event.altKey || event.ctrlKey || event.metaKey ||
        ["input", "textarea", "select", "textbox"].indexOf(tag) > -1) {
        return;
    }

    this._sidebarDestroy();
    this.inputNode.focus();
    event.stopPropagation();
  },

  /**
   * Create a variables view instance.
   *
   * @private
   * @param object options
   *        Options for the new Variables View instance:
   *        - container: the DOM element where the variables view is inserted.
   *        - hideFilterInput: boolean, if true the variables filter input is
   *        hidden.
   * @return object
   *         The new Variables View instance.
   */
  _createVariablesView: function JST__createVariablesView(options)
  {
    let view = new VariablesView(options.container);
    view.toolbox = gDevTools.getToolbox(this.hud.owner.target);
    view.searchPlaceholder = l10n.getStr("propertiesFilterPlaceholder");
    view.emptyText = l10n.getStr("emptyPropertiesList");
    view.searchEnabled = !options.hideFilterInput;
    view.lazyEmpty = this._lazyVariablesView;

    VariablesViewController.attach(view, {
      getEnvironmentClient: grip => {
        return new EnvironmentClient(this.hud.proxy.client, grip);
      },
      getObjectClient: grip => {
        return new ObjectClient(this.hud.proxy.client, grip);
      },
      getLongStringClient: grip => {
        return this.webConsoleClient.longString(grip);
      },
      releaseActor: actor => {
        this.hud._releaseObject(actor);
      },
      simpleValueEvalMacro: simpleValueEvalMacro,
      overrideValueEvalMacro: overrideValueEvalMacro,
      getterOrSetterEvalMacro: getterOrSetterEvalMacro,
    });

    // Relay events from the VariablesView.
    view.on("fetched", (event, type, variableObject) => {
      this.emit("variablesview-fetched", variableObject);
    });

    return view;
  },

  /**
   * Update the variables view.
   *
   * @private
   * @param object options
   *        Options for updating the variables view:
   *        - view: the view you want to update.
   *        - objectActor: the grip of the new ObjectActor you want to show in
   *        the view.
   *        - rawObject: the new raw object you want to show.
   *        - label: the new label for the inspected object.
   */
  _updateVariablesView: function JST__updateVariablesView(options)
  {
    let view = options.view;
    view.empty();

    // We need to avoid pruning the object inspection starting point.
    // That one is pruned when the console message is removed.
    view.controller.releaseActors(actor => {
      return view._consoleLastObjectActor != actor;
    });

    if (options.objectActor &&
        (!this.hud.owner._browserConsole ||
         Services.prefs.getBoolPref("devtools.chrome.enabled"))) {
      // Make sure eval works in the correct context.
      view.eval = this._variablesViewEvaluate.bind(this, options);
      view.switch = this._variablesViewSwitch.bind(this, options);
      view.delete = this._variablesViewDelete.bind(this, options);
    }
    else {
      view.eval = null;
      view.switch = null;
      view.delete = null;
    }

    let { variable, expanded } = view.controller.setSingleVariable(options);
    variable.evaluationMacro = simpleValueEvalMacro;

    if (options.objectActor) {
      view._consoleLastObjectActor = options.objectActor.actor;
    }
    else if (options.rawObject) {
      view._consoleLastObjectActor = null;
    }
    else {
      throw new Error("Variables View cannot open without giving it an object " +
                      "display.");
    }

    expanded.then(() => {
      this.emit("variablesview-updated", view, options);
    });
  },

  /**
   * The evaluation function used by the variables view when editing a property
   * value.
   *
   * @private
   * @param object options
   *        The options used for |this._updateVariablesView()|.
   * @param object variableObject
   *        The Variable object instance for the edited property.
   * @param string value
   *        The value the edited property was changed to.
   */
  _variablesViewEvaluate:
  function JST__variablesViewEvaluate(options, variableObject, value)
  {
    let updater = this._updateVariablesView.bind(this, options);
    let onEval = this._silentEvalCallback.bind(this, updater);
    let string = variableObject.evaluationMacro(variableObject, value);

    let evalOptions = {
      frame: this.SELECTED_FRAME,
      bindObjectActor: options.objectActor.actor,
    };

    this.requestEvaluation(string, evalOptions).then(onEval, onEval);
  },

  /**
   * The property deletion function used by the variables view when a property
   * is deleted.
   *
   * @private
   * @param object options
   *        The options used for |this._updateVariablesView()|.
   * @param object variableObject
   *        The Variable object instance for the deleted property.
   */
  _variablesViewDelete: function JST__variablesViewDelete(options, variableObject)
  {
    let onEval = this._silentEvalCallback.bind(this, null);

    let evalOptions = {
      frame: this.SELECTED_FRAME,
      bindObjectActor: options.objectActor.actor,
    };

    this.requestEvaluation("delete _self" + variableObject.symbolicName, evalOptions)
        .then(onEval, onEval);
  },

  /**
   * The property rename function used by the variables view when a property
   * is renamed.
   *
   * @private
   * @param object options
   *        The options used for |this._updateVariablesView()|.
   * @param object variableObject
   *        The Variable object instance for the renamed property.
   * @param string newName
   *        The new name for the property.
   */
  _variablesViewSwitch:
  function JST__variablesViewSwitch(options, variableObject, newName)
  {
    let updater = this._updateVariablesView.bind(this, options);
    let onEval = this._silentEvalCallback.bind(this, updater);

    let evalOptions = {
      frame: this.SELECTED_FRAME,
      bindObjectActor: options.objectActor.actor,
    };

    let newSymbolicName = variableObject.ownerView.symbolicName + '["' + newName + '"]';
    if (newSymbolicName == variableObject.symbolicName) {
      return;
    }

    let code = "_self" + newSymbolicName + " = _self" + variableObject.symbolicName + ";" +
               "delete _self" + variableObject.symbolicName;

    this.requestEvaluation(code, evalOptions).then(onEval, onEval);
  },

  /**
   * A noop callback for JavaScript evaluation. This method releases any
   * result ObjectActors that come from the server for evaluation requests. This
   * is used for editing, renaming and deleting properties in the variables
   * view.
   *
   * Exceptions are displayed in the output.
   *
   * @private
   * @param function callback
   *        Function to invoke once the response is received.
   * @param object response
   *        The response packet received from the server.
   */
  _silentEvalCallback: function JST__silentEvalCallback(callback, response)
  {
    if (response.error) {
      Cu.reportError("Web Console evaluation failed. " + response.error + ":" +
                     response.message);

      callback && callback(response);
      return;
    }

    if (response.exceptionMessage) {
      let message = new Messages.Simple(response.exceptionMessage, {
        category: "output",
        severity: "error",
        timestamp: response.timestamp,
      });
      this.hud.output.addMessage(message);
      message._objectActors = new Set();
      if (WebConsoleUtils.isActorGrip(response.exception)) {
        message._objectActors.add(response.exception.actor);
      }
    }

    let helper = response.helperResult || { type: null };
    let helperGrip = null;
    if (helper.type == "inspectObject") {
      helperGrip = helper.object;
    }

    let grips = [response.result, helperGrip];
    for (let grip of grips) {
      if (WebConsoleUtils.isActorGrip(grip)) {
        this.hud._releaseObject(grip.actor);
      }
    }

    callback && callback(response);
  },


  /**
   * Clear the Web Console output.
   *
   * This method emits the "messages-cleared" notification.
   *
   * @param boolean clearStorage
   *        True if you want to clear the console messages storage associated to
   *        this Web Console.
   */
  clearOutput: function JST_clearOutput(clearStorage)
  {
    let hud = this.hud;
    let outputNode = hud.outputNode;
    let node;
    while ((node = outputNode.firstChild)) {
      hud.removeOutputMessage(node);
    }

    hud.groupDepth = 0;
    hud._outputQueue.forEach(hud._destroyItem, hud);
    hud._outputQueue = [];
    this.webConsoleClient.clearNetworkRequests();
    hud._repeatNodes = {};

    if (clearStorage) {
      this.webConsoleClient.clearMessagesCache();
    }

    this._sidebarDestroy();

    this.emit("messages-cleared");
  },

  /**
   * Remove all of the private messages from the Web Console output.
   *
   * This method emits the "private-messages-cleared" notification.
   */
  clearPrivateMessages: function JST_clearPrivateMessages()
  {
    let nodes = this.hud.outputNode.querySelectorAll(".message[private]");
    for (let node of nodes) {
      this.hud.removeOutputMessage(node);
    }
    this.emit("private-messages-cleared");
  },

  /**
   * Updates the size of the input field (command line) to fit its contents.
   *
   * @returns void
   */
  resizeInput: function JST_resizeInput()
  {
    let inputNode = this.inputNode;

    // Reset the height so that scrollHeight will reflect the natural height of
    // the contents of the input field.
    inputNode.style.height = "auto";

    // Now resize the input field to fit its contents.
    let scrollHeight = inputNode.inputField.scrollHeight;
    if (scrollHeight > 0) {
      inputNode.style.height = scrollHeight + "px";
    }
  },

  /**
   * Sets the value of the input field (command line), and resizes the field to
   * fit its contents. This method is preferred over setting "inputNode.value"
   * directly, because it correctly resizes the field.
   *
   * @param string newValue
   *        The new value to set.
   * @returns void
   */
  setInputValue: function JST_setInputValue(newValue)
  {
    this.inputNode.value = newValue;
    this.lastInputValue = newValue;
    this.completeNode.value = "";
    this.resizeInput();
    this._inputChanged = true;
    this.emit("set-input-value");
  },

  /**
   * Gets the value from the input field
   * @returns string
   */
  getInputValue: function()
  {
    return this.inputNode.value || "";
  },

  /**
   * The inputNode "input" and "keyup" event handler.
   * @private
   */
  _inputEventHandler: function JST__inputEventHandler()
  {
    if (this.lastInputValue != this.getInputValue()) {
      this.resizeInput();
      this.complete(this.COMPLETE_HINT_ONLY);
      this.lastInputValue = this.getInputValue();
      this._inputChanged = true;
    }
  },

  /**
   * The window "blur" event handler.
   * @private
   */
  _blurEventHandler: function JST__blurEventHandler()
  {
    if (this.autocompletePopup) {
      this.clearCompletion();
    }
  },

  /**
   * The inputNode "keypress" event handler.
   *
   * @private
   * @param nsIDOMEvent event
   */
  _keyPress: function JST__keyPress(event)
  {
    let inputNode = this.inputNode;
    let inputValue = this.getInputValue();
    let inputUpdated = false;

    if (event.ctrlKey) {
      switch (event.charCode) {
        case 101:
          // control-e
          if (Services.appinfo.OS == "WINNT") {
            break;
          }
          let lineEndPos = inputValue.length;
          if (this.hasMultilineInput()) {
            // find index of closest newline >= cursor
            for (let i = inputNode.selectionEnd; i<lineEndPos; i++) {
              if (inputValue.charAt(i) == "\r" ||
                  inputValue.charAt(i) == "\n") {
                lineEndPos = i;
                break;
              }
            }
          }
          inputNode.setSelectionRange(lineEndPos, lineEndPos);
          event.preventDefault();
          this.clearCompletion();
          break;

        case 110:
          // Control-N differs from down arrow: it ignores autocomplete state.
          // Note that we preserve the default 'down' navigation within
          // multiline text.
          if (Services.appinfo.OS == "Darwin" &&
              this.canCaretGoNext() &&
              this.historyPeruse(HISTORY_FORWARD)) {
            event.preventDefault();
            // Ctrl-N is also used to focus the Network category button on MacOSX.
            // The preventDefault() call doesn't prevent the focus from moving
            // away from the input.
            inputNode.focus();
          }
          this.clearCompletion();
          break;

        case 112:
          // Control-P differs from up arrow: it ignores autocomplete state.
          // Note that we preserve the default 'up' navigation within
          // multiline text.
          if (Services.appinfo.OS == "Darwin" &&
              this.canCaretGoPrevious() &&
              this.historyPeruse(HISTORY_BACK)) {
            event.preventDefault();
            // Ctrl-P may also be used to focus some category button on MacOSX.
            // The preventDefault() call doesn't prevent the focus from moving
            // away from the input.
            inputNode.focus();
          }
          this.clearCompletion();
          break;
        default:
          break;
      }
      return;
    }
    else if (event.shiftKey &&
        event.keyCode == Ci.nsIDOMKeyEvent.DOM_VK_RETURN) {
      // shift return
      // TODO: expand the inputNode height by one line
      return;
    }

    switch (event.keyCode) {
      case Ci.nsIDOMKeyEvent.DOM_VK_ESCAPE:
        if (this.autocompletePopup.isOpen) {
          this.clearCompletion();
          event.preventDefault();
          event.stopPropagation();
        }
        else if (this.sidebar) {
          this._sidebarDestroy();
          event.preventDefault();
          event.stopPropagation();
        }
        break;

      case Ci.nsIDOMKeyEvent.DOM_VK_RETURN:
        if (this._autocompletePopupNavigated &&
            this.autocompletePopup.isOpen &&
            this.autocompletePopup.selectedIndex > -1) {
          this.acceptProposedCompletion();
        }
        else {
          this.execute();
          this._inputChanged = false;
        }
        event.preventDefault();
        break;

      case Ci.nsIDOMKeyEvent.DOM_VK_UP:
        if (this.autocompletePopup.isOpen) {
          inputUpdated = this.complete(this.COMPLETE_BACKWARD);
          if (inputUpdated) {
            this._autocompletePopupNavigated = true;
          }
        }
        else if (this.canCaretGoPrevious()) {
          inputUpdated = this.historyPeruse(HISTORY_BACK);
        }
        if (inputUpdated) {
          event.preventDefault();
        }
        break;

      case Ci.nsIDOMKeyEvent.DOM_VK_DOWN:
        if (this.autocompletePopup.isOpen) {
          inputUpdated = this.complete(this.COMPLETE_FORWARD);
          if (inputUpdated) {
            this._autocompletePopupNavigated = true;
          }
        }
        else if (this.canCaretGoNext()) {
          inputUpdated = this.historyPeruse(HISTORY_FORWARD);
        }
        if (inputUpdated) {
          event.preventDefault();
        }
        break;

      case Ci.nsIDOMKeyEvent.DOM_VK_PAGE_UP:
        if (this.autocompletePopup.isOpen) {
          inputUpdated = this.complete(this.COMPLETE_PAGEUP);
          if (inputUpdated) {
            this._autocompletePopupNavigated = true;
          }
        }
        else {
          this.hud.outputNode.parentNode.scrollTop =
            Math.max(0,
              this.hud.outputNode.parentNode.scrollTop -
              this.hud.outputNode.parentNode.clientHeight
            );
        }
        event.preventDefault();
        break;

      case Ci.nsIDOMKeyEvent.DOM_VK_PAGE_DOWN:
        if (this.autocompletePopup.isOpen) {
          inputUpdated = this.complete(this.COMPLETE_PAGEDOWN);
          if (inputUpdated) {
            this._autocompletePopupNavigated = true;
          }
        }
        else {
          this.hud.outputNode.parentNode.scrollTop =
            Math.min(this.hud.outputNode.parentNode.scrollHeight,
              this.hud.outputNode.parentNode.scrollTop +
              this.hud.outputNode.parentNode.clientHeight
            );
        }
        event.preventDefault();
        break;

      case Ci.nsIDOMKeyEvent.DOM_VK_HOME:
        if (this.autocompletePopup.isOpen) {
          this.autocompletePopup.selectedIndex = 0;
          event.preventDefault();
        } else if (inputValue.length <= 0) {
          this.hud.outputNode.parentNode.scrollTop = 0;
          event.preventDefault();
        }
        break;

      case Ci.nsIDOMKeyEvent.DOM_VK_END:
        if (this.autocompletePopup.isOpen) {
          this.autocompletePopup.selectedIndex = this.autocompletePopup.itemCount - 1;
          event.preventDefault();
        } else if (inputValue.length <= 0) {
          this.hud.outputNode.parentNode.scrollTop = this.hud.outputNode.parentNode.scrollHeight;
          event.preventDefault();
        }
        break;

      case Ci.nsIDOMKeyEvent.DOM_VK_LEFT:
        if (this.autocompletePopup.isOpen || this.lastCompletion.value) {
          this.clearCompletion();
        }
        break;

      case Ci.nsIDOMKeyEvent.DOM_VK_RIGHT: {
        let cursorAtTheEnd = this.inputNode.selectionStart ==
                             this.inputNode.selectionEnd &&
                             this.inputNode.selectionStart ==
                             inputValue.length;
        let haveSuggestion = this.autocompletePopup.isOpen ||
                             this.lastCompletion.value;
        let useCompletion = cursorAtTheEnd || this._autocompletePopupNavigated;
        if (haveSuggestion && useCompletion &&
            this.complete(this.COMPLETE_HINT_ONLY) &&
            this.lastCompletion.value &&
            this.acceptProposedCompletion()) {
          event.preventDefault();
        }
        if (this.autocompletePopup.isOpen) {
          this.clearCompletion();
        }
        break;
      }
      case Ci.nsIDOMKeyEvent.DOM_VK_TAB:
        // Generate a completion and accept the first proposed value.
        if (this.complete(this.COMPLETE_HINT_ONLY) &&
            this.lastCompletion &&
            this.acceptProposedCompletion()) {
          event.preventDefault();
        }
        else if (this._inputChanged) {
          this.updateCompleteNode(l10n.getStr("Autocomplete.blank"));
          event.preventDefault();
        }
        break;
      default:
        break;
    }
  },

  /**
   * The inputNode "focus" event handler.
   * @private
   */
  _focusEventHandler: function JST__focusEventHandler()
  {
    this._inputChanged = false;
  },

  /**
   * Go up/down the history stack of input values.
   *
   * @param number direction
   *        History navigation direction: HISTORY_BACK or HISTORY_FORWARD.
   *
   * @returns boolean
   *          True if the input value changed, false otherwise.
   */
  historyPeruse: function JST_historyPeruse(direction)
  {
    if (!this.history.length) {
      return false;
    }

    // Up Arrow key
    if (direction == HISTORY_BACK) {
      if (this.historyPlaceHolder <= 0) {
        return false;
      }
      let inputVal = this.history[--this.historyPlaceHolder];

      // Save the current input value as the latest entry in history, only if
      // the user is already at the last entry.
      // Note: this code does not store changes to items that are already in
      // history.
      if (this.historyPlaceHolder+1 == this.historyIndex) {
        this.history[this.historyIndex] = this.getInputValue() || "";
      }

      this.setInputValue(inputVal);
    }
    // Down Arrow key
    else if (direction == HISTORY_FORWARD) {
      if (this.historyPlaceHolder >= (this.history.length-1)) {
        return false;
      }

      let inputVal = this.history[++this.historyPlaceHolder];
      this.setInputValue(inputVal);
    }
    else {
      throw new Error("Invalid argument 0");
    }

    return true;
  },

  /**
   * Test for multiline input.
   *
   * @return boolean
   *         True if CR or LF found in node value; else false.
   */
  hasMultilineInput: function JST_hasMultilineInput()
  {
    return /[\r\n]/.test(this.getInputValue());
  },

  /**
   * Check if the caret is at a location that allows selecting the previous item
   * in history when the user presses the Up arrow key.
   *
   * @return boolean
   *         True if the caret is at a location that allows selecting the
   *         previous item in history when the user presses the Up arrow key,
   *         otherwise false.
   */
  canCaretGoPrevious: function JST_canCaretGoPrevious()
  {
    let node = this.inputNode;
    if (node.selectionStart != node.selectionEnd) {
      return false;
    }

    let multiline = /[\r\n]/.test(node.value);
    return node.selectionStart == 0 ? true :
           node.selectionStart == node.value.length && !multiline;
  },

  /**
   * Check if the caret is at a location that allows selecting the next item in
   * history when the user presses the Down arrow key.
   *
   * @return boolean
   *         True if the caret is at a location that allows selecting the next
   *         item in history when the user presses the Down arrow key, otherwise
   *         false.
   */
  canCaretGoNext: function JST_canCaretGoNext()
  {
    let node = this.inputNode;
    if (node.selectionStart != node.selectionEnd) {
      return false;
    }

    let multiline = /[\r\n]/.test(node.value);
    return node.selectionStart == node.value.length ? true :
           node.selectionStart == 0 && !multiline;
  },

  /**
   * Completes the current typed text in the inputNode. Completion is performed
   * only if the selection/cursor is at the end of the string. If no completion
   * is found, the current inputNode value and cursor/selection stay.
   *
   * @param int type possible values are
   *    - this.COMPLETE_FORWARD: If there is more than one possible completion
   *          and the input value stayed the same compared to the last time this
   *          function was called, then the next completion of all possible
   *          completions is used. If the value changed, then the first possible
   *          completion is used and the selection is set from the current
   *          cursor position to the end of the completed text.
   *          If there is only one possible completion, then this completion
   *          value is used and the cursor is put at the end of the completion.
   *    - this.COMPLETE_BACKWARD: Same as this.COMPLETE_FORWARD but if the
   *          value stayed the same as the last time the function was called,
   *          then the previous completion of all possible completions is used.
   *    - this.COMPLETE_PAGEUP: Scroll up one page if available or select the first
   *          item.
   *    - this.COMPLETE_PAGEDOWN: Scroll down one page if available or select the
   *          last item.
   *    - this.COMPLETE_HINT_ONLY: If there is more than one possible
   *          completion and the input value stayed the same compared to the
   *          last time this function was called, then the same completion is
   *          used again. If there is only one possible completion, then
   *          the this.getInputValue() is set to this value and the selection is set
   *          from the current cursor position to the end of the completed text.
   * @param function callback
   *        Optional function invoked when the autocomplete properties are
   *        updated.
   * @returns boolean true if there existed a completion for the current input,
   *          or false otherwise.
   */
  complete: function JSTF_complete(type, callback)
  {
    let inputNode = this.inputNode;
    let inputValue = this.getInputValue();
    let frameActor = this.getFrameActor(this.SELECTED_FRAME);

    // If the inputNode has no value, then don't try to complete on it.
    if (!inputValue) {
      this.clearCompletion();
      callback && callback(this);
      this.emit("autocomplete-updated");
      return false;
    }

    // Only complete if the selection is empty.
    if (inputNode.selectionStart != inputNode.selectionEnd) {
      this.clearCompletion();
      callback && callback(this);
      this.emit("autocomplete-updated");
      return false;
    }

    // Update the completion results.
    if (this.lastCompletion.value != inputValue || frameActor != this._lastFrameActorId) {
      this._updateCompletionResult(type, callback);
      return false;
    }

    let popup = this.autocompletePopup;
    let accepted = false;

    if (type != this.COMPLETE_HINT_ONLY && popup.itemCount == 1) {
      this.acceptProposedCompletion();
      accepted = true;
    }
    else if (type == this.COMPLETE_BACKWARD) {
      popup.selectPreviousItem();
    }
    else if (type == this.COMPLETE_FORWARD) {
      popup.selectNextItem();
    }
    else if (type == this.COMPLETE_PAGEUP) {
      popup.selectPreviousPageItem();
    }
    else if (type == this.COMPLETE_PAGEDOWN) {
      popup.selectNextPageItem();
    }

    callback && callback(this);
    this.emit("autocomplete-updated");
    return accepted || popup.itemCount > 0;
  },

  /**
   * Update the completion result. This operation is performed asynchronously by
   * fetching updated results from the content process.
   *
   * @private
   * @param int type
   *        Completion type. See this.complete() for details.
   * @param function [callback]
   *        Optional, function to invoke when completion results are received.
   */
  _updateCompletionResult:
  function JST__updateCompletionResult(type, callback)
  {
    let frameActor = this.getFrameActor(this.SELECTED_FRAME);
    if (this.lastCompletion.value == this.getInputValue() &&
        frameActor == this._lastFrameActorId) {
      return;
    }

    let requestId = gSequenceId();
    let cursor = this.inputNode.selectionStart;
    let input = this.getInputValue().substring(0, cursor);
    let cache = this._autocompleteCache;

    // If the current input starts with the previous input, then we already
    // have a list of suggestions and we just need to filter the cached
    // suggestions. When the current input ends with a non-alphanumeric
    // character we ask the server again for suggestions.

    // Check if last character is non-alphanumeric
    if (!/[a-zA-Z0-9]$/.test(input) || frameActor != this._lastFrameActorId) {
      this._autocompleteQuery = null;
      this._autocompleteCache = null;
    }

    if (this._autocompleteQuery && input.startsWith(this._autocompleteQuery)) {
      let filterBy = input;
      // Find the last non-alphanumeric other than _ or $ if it exists.
      let lastNonAlpha = input.match(/[^a-zA-Z0-9_$][a-zA-Z0-9_$]*$/);
      // If input contains non-alphanumerics, use the part after the last one
      // to filter the cache
      if (lastNonAlpha) {
        filterBy = input.substring(input.lastIndexOf(lastNonAlpha) + 1);
      }

      let newList = cache.sort().filter(function(l) {
        return l.startsWith(filterBy);
      });

      this.lastCompletion = {
        requestId: null,
        completionType: type,
        value: null,
      };

      let response = { matches: newList, matchProp: filterBy };
      this._receiveAutocompleteProperties(null, callback, response);
      return;
    }

    this._lastFrameActorId = frameActor;

    this.lastCompletion = {
      requestId: requestId,
      completionType: type,
      value: null,
    };

    let autocompleteCallback = this._receiveAutocompleteProperties.bind(this, requestId,
                                                            callback);

    this.webConsoleClient.autocomplete(input, cursor, autocompleteCallback, frameActor);
  },

  /**
   * Handler for the autocompletion results. This method takes
   * the completion result received from the server and updates the UI
   * accordingly.
   *
   * @param number requestId
   *        Request ID.
   * @param function [callback=null]
   *        Optional, function to invoke when the completion result is received.
   * @param object message
   *        The JSON message which holds the completion results received from
   *        the content process.
   */
  _receiveAutocompleteProperties:
  function JST__receiveAutocompleteProperties(requestId, callback, message)
  {
    let inputNode = this.inputNode;
    let inputValue = this.getInputValue();
    if (this.lastCompletion.value == inputValue ||
        requestId != this.lastCompletion.requestId) {
      return;
    }
    // Cache whatever came from the server if the last char is alphanumeric or '.'
    let cursor = inputNode.selectionStart;
    let inputUntilCursor = inputValue.substring(0, cursor);

    if (requestId != null && /[a-zA-Z0-9.]$/.test(inputUntilCursor)) {
      this._autocompleteCache = message.matches;
      this._autocompleteQuery = inputUntilCursor;
    }

    let matches = message.matches;
    let lastPart = message.matchProp;
    if (!matches.length) {
      this.clearCompletion();
      callback && callback(this);
      this.emit("autocomplete-updated");
      return;
    }

    let items = matches.reverse().map(function(match) {
      return { preLabel: lastPart, label: match };
    });

    let popup = this.autocompletePopup;
    popup.setItems(items);

    let completionType = this.lastCompletion.completionType;
    this.lastCompletion = {
      value: inputValue,
      matchProp: lastPart,
    };

    if (items.length > 1 && !popup.isOpen) {
      let str = this.getInputValue().substr(0, this.inputNode.selectionStart);
      let offset = str.length - (str.lastIndexOf("\n") + 1) - lastPart.length;
      let x = offset * this.hud._inputCharWidth;
      popup.openPopup(inputNode, x + this.hud._chevronWidth);
      this._autocompletePopupNavigated = false;
    }
    else if (items.length < 2 && popup.isOpen) {
      popup.hidePopup();
      this._autocompletePopupNavigated = false;
    }

    if (items.length == 1) {
      popup.selectedIndex = 0;
    }

    this.onAutocompleteSelect();

    if (completionType != this.COMPLETE_HINT_ONLY && popup.itemCount == 1) {
      this.acceptProposedCompletion();
    }
    else if (completionType == this.COMPLETE_BACKWARD) {
      popup.selectPreviousItem();
    }
    else if (completionType == this.COMPLETE_FORWARD) {
      popup.selectNextItem();
    }

    callback && callback(this);
    this.emit("autocomplete-updated");
  },

  onAutocompleteSelect: function JSTF_onAutocompleteSelect()
  {
    // Render the suggestion only if the cursor is at the end of the input.
    if (this.inputNode.selectionStart != this.getInputValue().length) {
      return;
    }

    let currentItem = this.autocompletePopup.selectedItem;
    if (currentItem && this.lastCompletion.value) {
      let suffix = currentItem.label.substring(this.lastCompletion.
                                               matchProp.length);
      this.updateCompleteNode(suffix);
    }
    else {
      this.updateCompleteNode("");
    }
  },

  /**
   * Clear the current completion information and close the autocomplete popup,
   * if needed.
   */
  clearCompletion: function JSTF_clearCompletion()
  {
    this.autocompletePopup.clearItems();
    this.lastCompletion = { value: null };
    this.updateCompleteNode("");
    if (this.autocompletePopup.isOpen) {
      this.autocompletePopup.hidePopup();
      this._autocompletePopupNavigated = false;
    }
  },

  /**
   * Accept the proposed input completion.
   *
   * @return boolean
   *         True if there was a selected completion item and the input value
   *         was updated, false otherwise.
   */
  acceptProposedCompletion: function JSTF_acceptProposedCompletion()
  {
    let updated = false;

    let currentItem = this.autocompletePopup.selectedItem;
    if (currentItem && this.lastCompletion.value) {
      let suffix = currentItem.label.substring(this.lastCompletion.
                                               matchProp.length);
      let cursor = this.inputNode.selectionStart;
      let value = this.getInputValue();
      this.setInputValue(value.substr(0, cursor) + suffix + value.substr(cursor));
      let newCursor = cursor + suffix.length;
      this.inputNode.selectionStart = this.inputNode.selectionEnd = newCursor;
      updated = true;
    }

    this.clearCompletion();

    return updated;
  },

  /**
   * Update the node that displays the currently selected autocomplete proposal.
   *
   * @param string suffix
   *        The proposed suffix for the inputNode value.
   */
  updateCompleteNode: function JSTF_updateCompleteNode(suffix)
  {
    // completion prefix = input, with non-control chars replaced by spaces
    let prefix = suffix ? this.getInputValue().replace(/[\S]/g, " ") : "";
    this.completeNode.value = prefix + suffix;
  },


  /**
   * Destroy the sidebar.
   * @private
   */
  _sidebarDestroy: function JST__sidebarDestroy()
  {
    if (this._variablesView) {
      this._variablesView.controller.releaseActors();
      this._variablesView = null;
    }

    if (this.sidebar) {
      this.sidebar.hide();
      this.sidebar.destroy();
      this.sidebar = null;
    }

    this.emit("sidebar-closed");
  },

  /**
   * Destroy the JSTerm object. Call this method to avoid memory leaks.
   */
  destroy: function JST_destroy()
  {
    this._sidebarDestroy();

    this.clearCompletion();
    this.clearOutput();

    this.autocompletePopup.destroy();
    this.autocompletePopup = null;

    let popup = this.hud.owner.chromeWindow.document
                .getElementById("webConsole_autocompletePopup");
    if (popup) {
      popup.parentNode.removeChild(popup);
    }

    if (this._onPaste) {
      this.inputNode.removeEventListener("paste", this._onPaste, false);
      this.inputNode.removeEventListener("drop", this._onPaste, false);
      this._onPaste = null;
    }

    this.inputNode.removeEventListener("keypress", this._keyPress, false);
    this.inputNode.removeEventListener("input", this._inputEventHandler, false);
    this.inputNode.removeEventListener("keyup", this._inputEventHandler, false);
    this.inputNode.removeEventListener("focus", this._focusEventHandler, false);
    this.hud.window.removeEventListener("blur", this._blurEventHandler, false);

    this.hud = null;
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
  scrollToVisible: function Utils_scrollToVisible(node)
  {
    node.scrollIntoView(false);
  },

  /**
   * Check if the given output node is scrolled to the bottom.
   *
   * @param nsIDOMNode outputNode
   * @return boolean
   *         True if the output node is scrolled to the bottom, or false
   *         otherwise.
   */
  isOutputScrolledToBottom: function Utils_isOutputScrolledToBottom(outputNode)
  {
    let lastNodeHeight = outputNode.lastChild ?
                         outputNode.lastChild.clientHeight : 0;
    let scrollNode = outputNode.parentNode;
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
  categoryForScriptError: function Utils_categoryForScriptError(scriptError)
  {
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
  logLimitForCategory: function Utils_logLimitForCategory(category)
  {
    let logLimit = DEFAULT_LOG_LIMIT;

    try {
      let prefName = CATEGORY_CLASS_FRAGMENTS[category];
      logLimit = Services.prefs.getIntPref("devtools.hud.loglimit." + prefName);
      logLimit = Math.max(logLimit, 1);
    }
    catch (e) { }

    return logLimit;
  },
};

///////////////////////////////////////////////////////////////////////////////
// CommandController
///////////////////////////////////////////////////////////////////////////////

/**
 * A controller (an instance of nsIController) that makes editing actions
 * behave appropriately in the context of the Web Console.
 */
function CommandController(webConsole)
{
  this.owner = webConsole;
}

CommandController.prototype = {
  /**
   * Selects all the text in the HUD output.
   */
  selectAll: function CommandController_selectAll()
  {
    this.owner.output.selectAllMessages();
  },

  /**
   * Open the URL of the selected message in a new tab.
   */
  openURL: function CommandController_openURL()
  {
    this.owner.openSelectedItemInTab();
  },

  copyURL: function CommandController_copyURL()
  {
    this.owner.copySelectedItems({ linkOnly: true, contextmenu: true });
  },

  /**
   * Copies the last clicked message.
   */
  copyLastClicked: function CommandController_copy()
  {
    this.owner.copySelectedItems({ linkOnly: false, contextmenu: true });
  },

  supportsCommand: function CommandController_supportsCommand(command)
  {
    if (!this.owner || !this.owner.output) {
      return false;
    }
    return this.isCommandEnabled(command);
  },

  isCommandEnabled: function CommandController_isCommandEnabled(command)
  {
    switch (command) {
      case "consoleCmd_openURL":
      case "consoleCmd_copyURL": {
        // Only enable URL-related actions if node is Net Activity.
        let selectedItem = this.owner.output.getSelectedMessages(1)[0] ||
                           this.owner._contextMenuHandler.lastClickedMessage;
        return selectedItem && "url" in selectedItem;
      }
      case "cmd_copy": {
        // Only copy if we right-clicked the console and there's no selected text.
        // With text selected, we want to fall back onto the default copy behavior.
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

  doCommand: function CommandController_doCommand(command)
  {
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

///////////////////////////////////////////////////////////////////////////////
// Web Console connection proxy
///////////////////////////////////////////////////////////////////////////////

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
function WebConsoleConnectionProxy(webConsoleFrame, target)
{
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
  this._onLastPrivateContextExited = this._onLastPrivateContextExited.bind(this);
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
  connect: function WCCP_connect()
  {
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
      client.addListener("lastPrivateContextExited", this._onLastPrivateContextExited);
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
  _connectionTimeout: function WCCP__connectionTimeout()
  {
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
  _attachConsole: function WCCP__attachConsole()
  {
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
  _onAttachConsole: function WCCP__onAttachConsole(response, webConsoleClient)
  {
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
  _onCachedMessages: function WCCP__onCachedMessages(response)
  {
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

    let messages = response.messages.concat(...this.webConsoleClient.getNetworkEvents());
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
  _onPageError: function WCCP__onPageError(type, packet)
  {
    if (this.webConsoleFrame && packet.from == this._consoleActor) {
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
  _onLogMessage: function WCCP__onLogMessage(type, packet)
  {
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
  _onConsoleAPICall: function WCCP__onConsoleAPICall(type, packet)
  {
    if (this.webConsoleFrame && packet.from == this._consoleActor) {
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
  _onNetworkEvent: function(type, networkInfo)
  {
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
  _onNetworkEventUpdate: function(type, { packet, networkInfo })
  {
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
  _onFileActivity: function WCCP__onFileActivity(type, packet)
  {
    if (this.webConsoleFrame && packet.from == this._consoleActor) {
      this.webConsoleFrame.handleFileActivity(packet.uri);
    }
  },

  _onReflowActivity: function WCCP__onReflowActivity(type, packet)
  {
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
  _onServerLogCall: function WCCP__onServerLogCall(type, packet)
  {
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
  _onLastPrivateContextExited:
  function WCCP__onLastPrivateContextExited(type, packet)
  {
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
  _onTabNavigated: function WCCP__onTabNavigated(event, packet)
  {
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
  releaseActor: function WCCP_releaseActor(actor)
  {
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
  disconnect: function WCCP_disconnect()
  {
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
    this.client.removeListener("lastPrivateContextExited", this._onLastPrivateContextExited);
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

function gSequenceId()
{
  return gSequenceId.n++;
}
gSequenceId.n = 0;

///////////////////////////////////////////////////////////////////////////////
// Context Menu
///////////////////////////////////////////////////////////////////////////////

/*
 * ConsoleContextMenu this used to handle the visibility of context menu items.
 *
 * @constructor
 * @param object owner
 *        The WebConsoleFrame instance that owns this object.
 */
function ConsoleContextMenu(owner)
{
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
  build: function CCM_build(event)
  {
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
  getSelectionMetadata: function CCM_getSelectionMetadata(clickElement)
  {
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
  shouldHideMenuItem: function CCM_shouldHideMenuItem(menuItem, metadata)
  {
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
  destroy: function CCM_destroy()
  {
    this.popup.removeEventListener("popupshowing", this.build);
    this.popup = null;
    this.owner = null;
    this.lastClickedMessage = null;
  },
};
