/* -*- Mode: js2; js2-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "clipboardHelper",
                                   "@mozilla.org/widget/clipboardhelper;1",
                                   "nsIClipboardHelper");

XPCOMUtils.defineLazyModuleGetter(this, "GripClient",
                                  "resource://gre/modules/devtools/dbg-client.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetworkPanel",
                                  "resource:///modules/NetworkPanel.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AutocompletePopup",
                                  "resource:///modules/devtools/AutocompletePopup.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "WebConsoleUtils",
                                  "resource://gre/modules/devtools/WebConsoleUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "promise",
                                  "resource://gre/modules/commonjs/sdk/core/promise.js", "Promise");

XPCOMUtils.defineLazyModuleGetter(this, "VariablesView",
                                  "resource:///modules/devtools/VariablesView.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "VariablesViewController",
                                  "resource:///modules/devtools/VariablesViewController.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "EventEmitter",
                                  "resource:///modules/devtools/shared/event-emitter.js");

XPCOMUtils.defineLazyModuleGetter(this, "devtools",
                                  "resource://gre/modules/devtools/Loader.jsm");

const STRINGS_URI = "chrome://browser/locale/devtools/webconsole.properties";
let l10n = new WebConsoleUtils.l10n(STRINGS_URI);


// The XUL namespace.
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

const MIXED_CONTENT_LEARN_MORE = "https://developer.mozilla.org/en/Security/MixedContent";

const HELP_URL = "https://developer.mozilla.org/docs/Tools/Web_Console/Helpers";

const VARIABLES_VIEW_URL = "chrome://browser/content/devtools/widgets/VariablesView.xul";

const CONSOLE_DIR_VIEW_HEIGHT = 0.6;

const IGNORED_SOURCE_URLS = ["debugger eval code", "self-hosted"];

// The amount of time in milliseconds that must pass between messages to
// trigger the display of a new group.
const NEW_GROUP_DELAY = 5000;

// The amount of time in milliseconds that we wait before performing a live
// search.
const SEARCH_DELAY = 200;

// The number of lines that are displayed in the console output by default, for
// each category. The user can change this number by adjusting the hidden
// "devtools.hud.loglimit.{network,cssparser,exception,console}" preferences.
const DEFAULT_LOG_LIMIT = 200;

// The various categories of messages. We start numbering at zero so we can
// use these as indexes into the MESSAGE_PREFERENCE_KEYS matrix below.
const CATEGORY_NETWORK = 0;
const CATEGORY_CSS = 1;
const CATEGORY_JS = 2;
const CATEGORY_WEBDEV = 3;
const CATEGORY_INPUT = 4;   // always on
const CATEGORY_OUTPUT = 5;  // always on
const CATEGORY_SECURITY = 6;

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
//  Error         Warning   Info    Log
  [ "network",    "netwarn",    null,   "networkinfo", ],  // Network
  [ "csserror",   "cssparser",  null,   null,          ],  // CSS
  [ "exception",  "jswarn",     null,   "jslog",       ],  // JS
  [ "error",      "warn",       "info", "log",         ],  // Web Developer
  [ null,         null,         null,   null,          ],  // Input
  [ null,         null,         null,   null,          ],  // Output
  [ "secerror",   "secwarn",    null,   null,          ],  // Security
];

// A mapping from the console API log event levels to the Web Console
// severities.
const LEVELS = {
  error: SEVERITY_ERROR,
  warn: SEVERITY_WARNING,
  info: SEVERITY_INFO,
  log: SEVERITY_LOG,
  trace: SEVERITY_LOG,
  debug: SEVERITY_LOG,
  dir: SEVERITY_LOG,
  group: SEVERITY_LOG,
  groupCollapsed: SEVERITY_LOG,
  groupEnd: SEVERITY_LOG,
  time: SEVERITY_LOG,
  timeEnd: SEVERITY_LOG
};

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
// too many messages at once we slow the Firefox UI too much.
const MESSAGES_IN_INTERVAL = DEFAULT_LOG_LIMIT;

// The delay between display updates - tells how often we should *try* to push
// new messages to screen. This value is optimistic, updates won't always
// happen. Keep this low so the Web Console output feels live.
const OUTPUT_INTERVAL = 50; // milliseconds

// When the output queue has more than MESSAGES_IN_INTERVAL items we throttle
// output updates to this number of milliseconds. So during a lot of output we
// update every N milliseconds given here.
const THROTTLE_UPDATES = 1000; // milliseconds

// The preference prefix for all of the Web Console filters.
const FILTER_PREFS_PREFIX = "devtools.webconsole.filter.";

// The minimum font size.
const MIN_FONT_SIZE = 10;

// The maximum length of strings to be displayed by the Web Console.
const MAX_LONG_STRING_LENGTH = 200000;

const PREF_CONNECTION_TIMEOUT = "devtools.debugger.remote-timeout";
const PREF_PERSISTLOG = "devtools.webconsole.persistlog";

/**
 * A WebConsoleFrame instance is an interactive console initialized *per target*
 * that displays console log data as well as provides an interactive terminal to
 * manipulate the target's document content.
 *
 * The WebConsoleFrame is responsible for the actual Web Console UI
 * implementation.
 *
 * @param object aWebConsoleOwner
 *        The WebConsole owner object.
 */
function WebConsoleFrame(aWebConsoleOwner)
{
  this.owner = aWebConsoleOwner;
  this.hudId = this.owner.hudId;

  this._repeatNodes = {};
  this._outputQueue = [];
  this._pruneCategoriesQueue = {};
  this._networkRequests = {};
  this.filterPrefs = {};

  this._toggleFilter = this._toggleFilter.bind(this);
  this._flushMessageQueue = this._flushMessageQueue.bind(this);

  this._outputTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  this._outputTimerInitialized = false;

  EventEmitter.decorate(this);
}

WebConsoleFrame.prototype = {
  /**
   * The WebConsole instance that owns this frame.
   * @see HUDService.jsm::WebConsole
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
  get popupset() this.owner.mainPopupSet,

  /**
   * Holds the initialization promise object.
   * @private
   * @type object
   */
  _initDefer: null,

  /**
   * Holds the network requests currently displayed by the Web Console. Each key
   * represents the connection ID and the value is network request information.
   * @private
   * @type object
   */
  _networkRequests: null,

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
   * The input element that allows the user to filter messages by string.
   * @type nsIDOMElement
   */
  filterBox: null,

  /**
   * Getter for the debugger WebConsoleClient.
   * @type object
   */
  get webConsoleClient() this.proxy ? this.proxy.webConsoleClient : null,

  _destroyer: null,

  // Used in tests.
  _saveRequestAndResponseBodies: false,

  /**
   * Tells whether to save the bodies of network requests and responses.
   * Disabled by default to save memory.
   *
   * @return boolean
   *         The saveRequestAndResponseBodies pref value.
   */
  getSaveRequestAndResponseBodies:
  function WCF_getSaveRequestAndResponseBodies() {
    let deferred = promise.defer();
    let toGet = [
      "NetworkMonitor.saveRequestAndResponseBodies"
    ];

    // Make sure the web console client connection is established first.
    this.webConsoleClient.getPreferences(toGet, aResponse => {
      if (!aResponse.error) {
        this._saveRequestAndResponseBodies = aResponse.preferences[toGet[0]];
        deferred.resolve(this._saveRequestAndResponseBodies);
      }
      else {
        deferred.reject(aResponse.error);
      }
    });

    return deferred.promise;
  },

  /**
   * Setter for saving of network request and response bodies.
   *
   * @param boolean aValue
   *        The new value you want to set.
   */
  setSaveRequestAndResponseBodies:
  function WCF_setSaveRequestAndResponseBodies(aValue) {
    let deferred = promise.defer();
    let newValue = !!aValue;
    let toSet = {
      "NetworkMonitor.saveRequestAndResponseBodies": newValue,
    };

    // Make sure the web console client connection is established first.
    this.webConsoleClient.setPreferences(toSet, aResponse => {
      if (!aResponse.error) {
        this._saveRequestAndResponseBodies = newValue;
        deferred.resolve(aResponse);
      }
      else {
        deferred.reject(aResponse.error);
      }
    });

    return deferred.promise;
  },

  /**
   * Getter for the persistent logging preference.
   * @type boolean
   */
  get persistLog() {
    return Services.prefs.getBoolPref(PREF_PERSISTLOG);
  },

  /**
   * Initialize the WebConsoleFrame instance.
   * @return object
   *         A promise object for the initialization.
   */
  init: function WCF_init()
  {
    this._initUI();
    return this._initConnection();
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
    }, (aReason) => { // on failure
      let node = this.createMessageNode(CATEGORY_JS, SEVERITY_ERROR,
                                        aReason.error + ": " + aReason.message);
      this.outputMessage(CATEGORY_JS, node);
      this._initDefer.reject(aReason);
    }).then(() => {
      let id = WebConsoleUtils.supportsString(this.hudId);
      Services.obs.notifyObservers(id, "web-console-created", null);
    });

    return this._initDefer.promise;
  },

  /**
   * Find the Web Console UI elements and setup event listeners as needed.
   * @private
   */
  _initUI: function WCF__initUI()
  {
    // Remember that this script is loaded in the webconsole.xul context:
    // |window| is the iframe global.
    this.window = window;
    this.document = this.window.document;
    this.rootElement = this.document.documentElement;

    this._initDefaultFilterPrefs();

    // Register the controller to handle "select all" properly.
    this._commandController = new CommandController(this);
    this.window.controllers.insertControllerAt(0, this._commandController);

    let doc = this.document;

    this.filterBox = doc.querySelector(".hud-filter-box");
    this.outputNode = doc.querySelector(".hud-output-node");
    this.completeNode = doc.querySelector(".jsterm-complete-node");
    this.inputNode = doc.querySelector(".jsterm-input-node");

    this._setFilterTextBoxEvents();
    this._initFilterButtons();

    let fontSize = Services.prefs.getIntPref("devtools.webconsole.fontSize");

    if (fontSize != 0) {
      fontSize = Math.max(MIN_FONT_SIZE, fontSize);

      this.outputNode.style.fontSize = fontSize + "px";
      this.completeNode.style.fontSize = fontSize + "px";
      this.inputNode.style.fontSize = fontSize + "px";
    }

    let updateSaveBodiesPrefUI = (aElement) => {
      this.getSaveRequestAndResponseBodies().then(aValue => {
        aElement.setAttribute("checked", aValue);
        this.emit("save-bodies-ui-toggled");
      });
    }

    let reverseSaveBodiesPref = ({ target: aElement }) => {
      this.getSaveRequestAndResponseBodies().then(aValue => {
        this.setSaveRequestAndResponseBodies(!aValue);
        aElement.setAttribute("checked", aValue);
        this.emit("save-bodies-pref-reversed");
      });
    }

    let saveBodies = doc.getElementById("saveBodies");
    saveBodies.addEventListener("click", reverseSaveBodiesPref);
    saveBodies.disabled = !this.getFilterState("networkinfo") &&
                          !this.getFilterState("network");

    let saveBodiesContextMenu = doc.getElementById("saveBodiesContextMenu");
    saveBodiesContextMenu.addEventListener("click", reverseSaveBodiesPref);
    saveBodiesContextMenu.disabled = !this.getFilterState("networkinfo") &&
                                     !this.getFilterState("network");

    saveBodies.parentNode.addEventListener("popupshowing", () => {
      updateSaveBodiesPrefUI(saveBodies);
      saveBodies.disabled = !this.getFilterState("networkinfo") &&
                            !this.getFilterState("network");
    });

    saveBodiesContextMenu.parentNode.addEventListener("popupshowing", () => {
      updateSaveBodiesPrefUI(saveBodiesContextMenu);
      saveBodiesContextMenu.disabled = !this.getFilterState("networkinfo") &&
                                       !this.getFilterState("network");
    });

    let clearButton = doc.getElementsByClassName("webconsole-clear-console-button")[0];
    clearButton.addEventListener("command", () => {
      this.owner._onClearButton();
      this.jsterm.clearOutput(true);
    });

    this.jsterm = new JSTerm(this);
    this.jsterm.init();
    this.jsterm.inputNode.focus();
  },

  /**
   * Initialize the default filter preferences.
   * @private
   */
  _initDefaultFilterPrefs: function WCF__initDefaultFilterPrefs()
  {
    let prefs = ["network", "networkinfo", "csserror", "cssparser", "exception",
                 "jswarn", "jslog", "error", "info", "warn", "log", "secerror",
                 "secwarn", "netwarn"];
    for (let pref of prefs) {
      this.filterPrefs[pref] = Services.prefs
                               .getBoolPref(this._filterPrefsPrefix + pref);
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
    Array.forEach(categories, function(aButton) {
      aButton.addEventListener("click", this._toggleFilter, false);

      let someChecked = false;
      let severities = aButton.querySelectorAll("menuitem[prefKey]");
      Array.forEach(severities, function(aMenuItem) {
        aMenuItem.addEventListener("command", this._toggleFilter, false);

        let prefKey = aMenuItem.getAttribute("prefKey");
        let checked = this.filterPrefs[prefKey];
        aMenuItem.setAttribute("checked", checked);
        someChecked = someChecked || checked;
      }, this);

      aButton.setAttribute("checked", someChecked);
    }, this);

    if (!this.owner._browserConsole) {
      // The Browser Console displays nsIConsoleMessages which are messages that
      // end up in the JS category, but they are not errors or warnings, they
      // are just log messages. The Web Console does not show such messages.
      let jslog = this.document.querySelector("menuitem[prefKey=jslog]");
      jslog.hidden = true;
    }
  },

  /**
   * Increase, decrease or reset the font size.
   *
   * @param string size
   *        The size of the font change. Accepted values are "+" and "-".
   *        An unmatched size assumes a font reset.
   */
  changeFontSize: function WCF_changeFontSize(aSize)
  {
    let fontSize = this.window
                   .getComputedStyle(this.outputNode, null)
                   .getPropertyValue("font-size").replace("px", "");

    if (this.outputNode.style.fontSize) {
      fontSize = this.outputNode.style.fontSize.replace("px", "");
    }

    if (aSize == "+" || aSize == "-") {
      fontSize = parseInt(fontSize, 10);

      if (aSize == "+") {
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
  },

  /**
   * The event handler that is called whenever a user switches a filter on or
   * off.
   *
   * @private
   * @param nsIDOMEvent aEvent
   *        The event that triggered the filter change.
   */
  _toggleFilter: function WCF__toggleFilter(aEvent)
  {
    let target = aEvent.target;
    let tagName = target.tagName;
    if (tagName != aEvent.currentTarget.tagName) {
      return;
    }

    switch (tagName) {
      case "toolbarbutton": {
        let originalTarget = aEvent.originalTarget;
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

        let state = target.getAttribute("checked") !== "true";
        target.setAttribute("checked", state);

        // This is a filter button with a drop-down, and the user clicked the
        // main part of the button. Go through all the severities and toggle
        // their associated filters.
        let menuItems = target.querySelectorAll("menuitem");
        for (let i = 0; i < menuItems.length; i++) {
          menuItems[i].setAttribute("checked", state);
          let prefKey = menuItems[i].getAttribute("prefKey");
          this.setFilterState(prefKey, state);
        }
        break;
      }

      case "menuitem": {
        let state = target.getAttribute("checked") !== "true";
        target.setAttribute("checked", state);

        let prefKey = target.getAttribute("prefKey");
        this.setFilterState(prefKey, state);

        // Disable the log response and request body if network logging is off.
        if (prefKey == "networkinfo" || prefKey == "network") {
          let checkState = !this.getFilterState("networkinfo") &&
                           !this.getFilterState("network");
          this.document.getElementById("saveBodies").disabled = checkState;
          this.document.getElementById("saveBodiesContextMenu").disabled = checkState;
        }

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
        break;
      }
    }
  },

  /**
   * Set the filter state for a specific toggle button.
   *
   * @param string aToggleType
   * @param boolean aState
   * @returns void
   */
  setFilterState: function WCF_setFilterState(aToggleType, aState)
  {
    this.filterPrefs[aToggleType] = aState;
    this.adjustVisibilityForMessageType(aToggleType, aState);
    Services.prefs.setBoolPref(this._filterPrefsPrefix + aToggleType, aState);
  },

  /**
   * Get the filter state for a specific toggle button.
   *
   * @param string aToggleType
   * @returns boolean
   */
  getFilterState: function WCF_getFilterState(aToggleType)
  {
    return this.filterPrefs[aToggleType];
  },

  /**
   * Check that the passed string matches the filter arguments.
   *
   * @param String aString
   *        to search for filter words in.
   * @param String aFilter
   *        is a string containing all of the words to filter on.
   * @returns boolean
   */
  stringMatchesFilters: function WCF_stringMatchesFilters(aString, aFilter)
  {
    if (!aFilter || !aString) {
      return true;
    }

    let searchStr = aString.toLowerCase();
    let filterStrings = aFilter.toLowerCase().split(/\s+/);
    return !filterStrings.some(function (f) {
      return searchStr.indexOf(f) == -1;
    });
  },

  /**
   * Turns the display of log nodes on and off appropriately to reflect the
   * adjustment of the message type filter named by @aPrefKey.
   *
   * @param string aPrefKey
   *        The preference key for the message type being filtered: one of the
   *        values in the MESSAGE_PREFERENCE_KEYS table.
   * @param boolean aState
   *        True if the filter named by @aMessageType is being turned on; false
   *        otherwise.
   * @returns void
   */
  adjustVisibilityForMessageType:
  function WCF_adjustVisibilityForMessageType(aPrefKey, aState)
  {
    let outputNode = this.outputNode;
    let doc = this.document;

    // Look for message nodes ("hud-msg-node") with the given preference key
    // ("hud-msg-error", "hud-msg-cssparser", etc.) and add or remove the
    // "hud-filtered-by-type" class, which turns on or off the display.

    let xpath = ".//*[contains(@class, 'hud-msg-node') and " +
      "contains(concat(@class, ' '), 'hud-" + aPrefKey + " ')]";
    let result = doc.evaluate(xpath, outputNode, null,
      Ci.nsIDOMXPathResult.UNORDERED_NODE_SNAPSHOT_TYPE, null);
    for (let i = 0; i < result.snapshotLength; i++) {
      let node = result.snapshotItem(i);
      if (aState) {
        node.classList.remove("hud-filtered-by-type");
      }
      else {
        node.classList.add("hud-filtered-by-type");
      }
    }

    this.regroupOutput();
  },

  /**
   * Turns the display of log nodes on and off appropriately to reflect the
   * adjustment of the search string.
   */
  adjustVisibilityOnSearchStringChange:
  function WCF_adjustVisibilityOnSearchStringChange()
  {
    let nodes = this.outputNode.getElementsByClassName("hud-msg-node");
    let searchString = this.filterBox.value;

    for (let i = 0, n = nodes.length; i < n; ++i) {
      let node = nodes[i];

      // hide nodes that match the strings
      let text = node.clipboardText;

      // if the text matches the words in aSearchString...
      if (this.stringMatchesFilters(text, searchString)) {
        node.classList.remove("hud-filtered-by-string");
      }
      else {
        node.classList.add("hud-filtered-by-string");
      }
    }

    this.regroupOutput();
  },

  /**
   * Applies the user's filters to a newly-created message node via CSS
   * classes.
   *
   * @param nsIDOMNode aNode
   *        The newly-created message node.
   * @return boolean
   *         True if the message was filtered or false otherwise.
   */
  filterMessageNode: function WCF_filterMessageNode(aNode)
  {
    let isFiltered = false;

    // Filter by the message type.
    let prefKey = MESSAGE_PREFERENCE_KEYS[aNode.category][aNode.severity];
    if (prefKey && !this.getFilterState(prefKey)) {
      // The node is filtered by type.
      aNode.classList.add("hud-filtered-by-type");
      isFiltered = true;
    }

    // Filter on the search string.
    let search = this.filterBox.value;
    let text = aNode.clipboardText;

    // if string matches the filter text
    if (!this.stringMatchesFilters(text, search)) {
      aNode.classList.add("hud-filtered-by-string");
      isFiltered = true;
    }

    if (isFiltered && aNode.classList.contains("webconsole-msg-inspector")) {
      aNode.classList.add("hidden-message");
    }

    return isFiltered;
  },

  /**
   * Merge the attributes of the two nodes that are about to be filtered.
   * Increment the number of repeats of aOriginal.
   *
   * @param nsIDOMNode aOriginal
   *        The Original Node. The one being merged into.
   * @param nsIDOMNode aFiltered
   *        The node being filtered out because it is repeated.
   */
  mergeFilteredMessageNode:
  function WCF_mergeFilteredMessageNode(aOriginal, aFiltered)
  {
    // childNodes[3].firstChild is the node containing the number of repetitions
    // of a node.
    let repeatNode = aOriginal.childNodes[3].firstChild;
    if (!repeatNode) {
      return; // no repeat node, return early.
    }

    let occurrences = parseInt(repeatNode.getAttribute("value")) + 1;
    repeatNode.setAttribute("value", occurrences);
  },

  /**
   * Filter the message node from the output if it is a repeat.
   *
   * @private
   * @param nsIDOMNode aNode
   *        The message node to be filtered or not.
   * @returns nsIDOMNode|null
   *          Returns the duplicate node if the message was filtered, null
   *          otherwise.
   */
  _filterRepeatedMessage: function WCF__filterRepeatedMessage(aNode)
  {
    let repeatNode = aNode.getElementsByClassName("webconsole-msg-repeat")[0];
    if (!repeatNode) {
      return null;
    }

    let uid = repeatNode._uid;
    let dupeNode = null;

    if (aNode.classList.contains("webconsole-msg-cssparser") ||
        aNode.classList.contains("webconsole-msg-security")) {
      dupeNode = this._repeatNodes[uid];
      if (!dupeNode) {
        this._repeatNodes[uid] = aNode;
      }
    }
    else if (!aNode.classList.contains("webconsole-msg-network") &&
             !aNode.classList.contains("webconsole-msg-inspector") &&
             (aNode.classList.contains("webconsole-msg-console") ||
              aNode.classList.contains("webconsole-msg-exception") ||
              aNode.classList.contains("webconsole-msg-error"))) {
      let lastMessage = this.outputNode.lastChild;
      if (!lastMessage) {
        return null;
      }

      let lastRepeatNode = lastMessage
                           .getElementsByClassName("webconsole-msg-repeat")[0];
      if (lastRepeatNode && lastRepeatNode._uid == uid) {
        dupeNode = lastMessage;
      }
    }

    if (dupeNode) {
      this.mergeFilteredMessageNode(dupeNode, aNode);
      return dupeNode;
    }

    return null;
  },

  /**
   * Display cached messages that may have been collected before the UI is
   * displayed.
   *
   * @param array aRemoteMessages
   *        Array of cached messages coming from the remote Web Console
   *        content instance.
   */
  displayCachedMessages: function WCF_displayCachedMessages(aRemoteMessages)
  {
    if (!aRemoteMessages.length) {
      return;
    }

    aRemoteMessages.forEach(function(aMessage) {
      switch (aMessage._type) {
        case "PageError": {
          let category = Utils.categoryForScriptError(aMessage);
          this.outputMessage(category, this.reportPageError,
                             [category, aMessage]);
          break;
        }
        case "LogMessage":
          this.handleLogMessage(aMessage);
          break;
        case "ConsoleAPI":
          this.outputMessage(CATEGORY_WEBDEV, this.logConsoleAPIMessage,
                             [aMessage]);
          break;
      }
    }, this);
  },

  /**
   * Logs a message to the Web Console that originates from the Web Console
   * server.
   *
   * @param object aMessage
   *        The message received from the server.
   * @return nsIDOMElement|null
   *         The message element to display in the Web Console output.
   */
  logConsoleAPIMessage: function WCF_logConsoleAPIMessage(aMessage)
  {
    let body = null;
    let clipboardText = null;
    let sourceURL = aMessage.filename;
    let sourceLine = aMessage.lineNumber;
    let level = aMessage.level;
    let args = aMessage.arguments;
    let objectActors = new Set();

    // Gather the actor IDs.
    args.forEach((aValue) => {
      if (WebConsoleUtils.isActorGrip(aValue)) {
        objectActors.add(aValue.actor);
      }
    });

    switch (level) {
      case "log":
      case "info":
      case "warn":
      case "error":
      case "debug":
      case "dir": {
        body = { arguments: args };
        let clipboardArray = [];
        args.forEach((aValue) => {
          clipboardArray.push(VariablesView.getString(aValue));
          if (aValue && typeof aValue == "object" &&
              aValue.type == "longString") {
            clipboardArray.push(l10n.getStr("longStringEllipsis"));
          }
        });
        clipboardText = clipboardArray.join(" ");
        break;
      }

      case "trace": {
        let filename = WebConsoleUtils.abbreviateSourceURL(aMessage.filename);
        let functionName = aMessage.functionName ||
                           l10n.getStr("stacktrace.anonymousFunction");

        body = l10n.getFormatStr("stacktrace.outputMessage",
                                 [filename, functionName, sourceLine]);

        clipboardText = "";

        aMessage.stacktrace.forEach(function(aFrame) {
          clipboardText += aFrame.filename + " :: " +
                           aFrame.functionName + " :: " +
                           aFrame.lineNumber + "\n";
        });

        clipboardText = clipboardText.trimRight();
        break;
      }

      case "group":
      case "groupCollapsed":
        clipboardText = body = aMessage.groupName;
        this.groupDepth++;
        break;

      case "groupEnd":
        if (this.groupDepth > 0) {
          this.groupDepth--;
        }
        break;

      case "time": {
        let timer = aMessage.timer;
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
        let timer = aMessage.timer;
        if (!timer) {
          return null;
        }
        let duration = Math.round(timer.duration * 100) / 100;
        body = l10n.getFormatStr("timeEnd", [timer.name, duration]);
        clipboardText = body;
        break;
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
      case "trace":
      case "time":
      case "timeEnd":
        for (let actor of objectActors) {
          this._releaseObject(actor);
        }
        objectActors.clear();
    }

    if (level == "groupEnd") {
      return null; // no need to continue
    }

    let node = this.createMessageNode(CATEGORY_WEBDEV, LEVELS[level], body,
                                      sourceURL, sourceLine, clipboardText,
                                      level, aMessage.timeStamp);
    if (aMessage.private) {
      node.setAttribute("private", true);
    }

    if (objectActors.size > 0) {
      node._objectActors = objectActors;

      let repeatNode = node.querySelector(".webconsole-msg-repeat");
      repeatNode._uid += [...objectActors].join("-");
    }

    // Make the node bring up the variables view, to allow the user to inspect
    // the stack trace.
    if (level == "trace") {
      node._stacktrace = aMessage.stacktrace;

      this.makeOutputMessageLink(node, () =>
        this.jsterm.openVariablesView({
          rawObject: node._stacktrace,
          autofocus: true,
        }));
    }

    return node;
  },

  /**
   * Handle ConsoleAPICall objects received from the server. This method outputs
   * the window.console API call.
   *
   * @param object aMessage
   *        The console API message received from the server.
   */
  handleConsoleAPICall: function WCF_handleConsoleAPICall(aMessage)
  {
    this.outputMessage(CATEGORY_WEBDEV, this.logConsoleAPIMessage, [aMessage]);
  },

  /**
   * The click event handler for objects shown inline coming from the
   * window.console API.
   *
   * @private
   * @param nsIDOMNode aAnchor
   *        The object inspector anchor element. This is the clickable element
   *        in the console.log message we display.
   * @param object aObjectActor
   *        The object actor grip.
   */
  _consoleLogClick: function WCF__consoleLogClick(aAnchor, aObjectActor)
  {
    this.jsterm.openVariablesView({
      label: aAnchor.textContent,
      objectActor: aObjectActor,
      autofocus: true,
    });
  },

  /**
   * Reports an error in the page source, either JavaScript or CSS.
   *
   * @param nsIScriptError aScriptError
   *        The error message to report.
   * @return nsIDOMElement|undefined
   *         The message element to display in the Web Console output.
   */
  reportPageError: function WCF_reportPageError(aCategory, aScriptError)
  {
    // Warnings and legacy strict errors become warnings; other types become
    // errors.
    let severity = SEVERITY_ERROR;
    if (aScriptError.warning || aScriptError.strict) {
      severity = SEVERITY_WARNING;
    }

    let objectActors = new Set();

    // Gather the actor IDs.
    for (let prop of ["errorMessage", "lineText"]) {
      let grip = aScriptError[prop];
      if (WebConsoleUtils.isActorGrip(grip)) {
        objectActors.add(grip.actor);
      }
    }

    let errorMessage = aScriptError.errorMessage;
    if (errorMessage.type && errorMessage.type == "longString") {
      errorMessage = errorMessage.initial;
    }

    let node = this.createMessageNode(aCategory, severity,
                                      errorMessage,
                                      aScriptError.sourceName,
                                      aScriptError.lineNumber, null, null,
                                      aScriptError.timeStamp);
    if (aScriptError.private) {
      node.setAttribute("private", true);
    }

    if (objectActors.size > 0) {
      node._objectActors = objectActors;
    }

    return node;
  },

  /**
   * Handle PageError objects received from the server. This method outputs the
   * given error.
   *
   * @param nsIScriptError aPageError
   *        The error received from the server.
   */
  handlePageError: function WCF_handlePageError(aPageError)
  {
    let category = Utils.categoryForScriptError(aPageError);
    this.outputMessage(category, this.reportPageError, [category, aPageError]);
  },

  /**
   * Handle log messages received from the server. This method outputs the given
   * message.
   *
   * @param object aPacket
   *        The message packet received from the server.
   */
  handleLogMessage: function WCF_handleLogMessage(aPacket)
  {
    if (aPacket.message) {
      this.outputMessage(CATEGORY_JS, this._reportLogMessage, [aPacket]);
    }
  },

  /**
   * Display log messages received from the server.
   *
   * @private
   * @param object aPacket
   *        The message packet received from the server.
   * @return nsIDOMElement
   *         The message element to render for the given log message.
   */
  _reportLogMessage: function WCF__reportLogMessage(aPacket)
  {
    let msg = aPacket.message;
    if (msg.type && msg.type == "longString") {
      msg = msg.initial;
    }
    let node = this.createMessageNode(CATEGORY_JS, SEVERITY_LOG, msg, null,
                                      null, null, null, aPacket.timeStamp);
    if (WebConsoleUtils.isActorGrip(aPacket.message)) {
      node._objectActors = new Set([aPacket.message.actor]);
    }
    return node;
  },

  /**
   * Log network event.
   *
   * @param object aActorId
   *        The network event actor ID to log.
   * @return nsIDOMElement|null
   *         The message element to display in the Web Console output.
   */
  logNetEvent: function WCF_logNetEvent(aActorId)
  {
    let networkInfo = this._networkRequests[aActorId];
    if (!networkInfo) {
      return null;
    }

    let request = networkInfo.request;

    let msgNode = this.document.createElementNS(XUL_NS, "hbox");

    let methodNode = this.document.createElementNS(XUL_NS, "label");
    methodNode.setAttribute("value", request.method);
    methodNode.classList.add("webconsole-msg-body-piece");
    msgNode.appendChild(methodNode);

    let linkNode = this.document.createElementNS(XUL_NS, "hbox");
    linkNode.flex = 1;
    linkNode.classList.add("webconsole-msg-body-piece");
    linkNode.classList.add("webconsole-msg-link");
    msgNode.appendChild(linkNode);

    let urlNode = this.document.createElementNS(XUL_NS, "label");
    urlNode.flex = 1;
    urlNode.setAttribute("crop", "center");
    urlNode.setAttribute("title", request.url);
    urlNode.setAttribute("tooltiptext", request.url);
    urlNode.setAttribute("value", request.url);
    urlNode.classList.add("hud-clickable");
    urlNode.classList.add("webconsole-msg-body-piece");
    urlNode.classList.add("webconsole-msg-url");
    linkNode.appendChild(urlNode);

    let severity = SEVERITY_LOG;
    let mixedRequest =
      WebConsoleUtils.isMixedHTTPSRequest(request.url, this.contentLocation);
    if (mixedRequest) {
      urlNode.classList.add("webconsole-mixed-content");
      this.makeMixedContentNode(linkNode);
      // If we define a SEVERITY_SECURITY in the future, switch this to
      // SEVERITY_SECURITY.
      severity = SEVERITY_WARNING;
    }

    let statusNode = this.document.createElementNS(XUL_NS, "label");
    statusNode.setAttribute("value", "");
    statusNode.classList.add("hud-clickable");
    statusNode.classList.add("webconsole-msg-body-piece");
    statusNode.classList.add("webconsole-msg-status");
    linkNode.appendChild(statusNode);

    let clipboardText = request.method + " " + request.url;

    let messageNode = this.createMessageNode(CATEGORY_NETWORK, severity,
                                             msgNode, null, null, clipboardText);
    if (networkInfo.private) {
      messageNode.setAttribute("private", true);
    }

    messageNode._connectionId = aActorId;
    messageNode.url = request.url;

    this.makeOutputMessageLink(messageNode, function WCF_net_message_link() {
      if (!messageNode._panelOpen) {
        this.openNetworkPanel(messageNode, networkInfo);
      }
    }.bind(this));

    networkInfo.node = messageNode;

    this._updateNetMessage(aActorId);

    return messageNode;
  },

  /**
   * Create a mixed content warning Node.
   *
   * @param aLinkNode
   *        Parent to the requested urlNode.
   */
  makeMixedContentNode: function WCF_makeMixedContentNode(aLinkNode)
  {
    let mixedContentWarning = "[" + l10n.getStr("webConsoleMixedContentWarning") + "]";

    // Mixed content warning message links to a Learn More page
    let mixedContentWarningNode = this.document.createElement("label");
    mixedContentWarningNode.setAttribute("value", mixedContentWarning);
    mixedContentWarningNode.setAttribute("title", mixedContentWarning);
    mixedContentWarningNode.classList.add("hud-clickable");
    mixedContentWarningNode.classList.add("webconsole-mixed-content-link");

    aLinkNode.appendChild(mixedContentWarningNode);

    mixedContentWarningNode.addEventListener("click", function(aEvent) {
      this.owner.openLink(MIXED_CONTENT_LEARN_MORE);
      aEvent.preventDefault();
      aEvent.stopPropagation();
    }.bind(this));
  },

  /**
   * Log file activity.
   *
   * @param string aFileURI
   *        The file URI that was loaded.
   * @return nsIDOMElement|undefined
   *         The message element to display in the Web Console output.
   */
  logFileActivity: function WCF_logFileActivity(aFileURI)
  {
    let urlNode = this.document.createElementNS(XUL_NS, "label");
    urlNode.flex = 1;
    urlNode.setAttribute("crop", "center");
    urlNode.setAttribute("title", aFileURI);
    urlNode.setAttribute("tooltiptext", aFileURI);
    urlNode.setAttribute("value", aFileURI);
    urlNode.classList.add("hud-clickable");
    urlNode.classList.add("webconsole-msg-url");

    let outputNode = this.createMessageNode(CATEGORY_NETWORK, SEVERITY_LOG,
                                            urlNode, null, null, aFileURI);

    this.makeOutputMessageLink(outputNode, function WCF__onFileClick() {
      this.owner.viewSource(aFileURI);
    }.bind(this));

    return outputNode;
  },

  /**
   * Handle the file activity messages coming from the remote Web Console.
   *
   * @param string aFileURI
   *        The file URI that was requested.
   */
  handleFileActivity: function WCF_handleFileActivity(aFileURI)
  {
    this.outputMessage(CATEGORY_NETWORK, this.logFileActivity, [aFileURI]);
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
   * Inform user that the string he tries to view is too long.
   */
  logWarningAboutStringTooLong: function WCF_logWarningAboutStringTooLong()
  {
    let node = this.createMessageNode(CATEGORY_JS, SEVERITY_WARNING,
                                      l10n.getStr("longStringTooLong"));
    this.outputMessage(CATEGORY_JS, node);
  },

  /**
   * Handle the network events coming from the remote Web Console.
   *
   * @param object aActor
   *        The NetworkEventActor grip.
   */
  handleNetworkEvent: function WCF_handleNetworkEvent(aActor)
  {
    let networkInfo = {
      node: null,
      actor: aActor.actor,
      discardRequestBody: true,
      discardResponseBody: true,
      startedDateTime: aActor.startedDateTime,
      request: {
        url: aActor.url,
        method: aActor.method,
      },
      response: {},
      timings: {},
      updates: [], // track the list of network event updates
      private: aActor.private,
    };

    this._networkRequests[aActor.actor] = networkInfo;
    this.outputMessage(CATEGORY_NETWORK, this.logNetEvent, [aActor.actor]);
  },

  /**
   * Handle network event updates coming from the server.
   *
   * @param string aActorId
   *        The network event actor ID.
   * @param string aType
   *        Update type.
   * @param object aPacket
   *        Update details.
   */
  handleNetworkEventUpdate:
  function WCF_handleNetworkEventUpdate(aActorId, aType, aPacket)
  {
    let networkInfo = this._networkRequests[aActorId];
    if (!networkInfo) {
      return;
    }

    networkInfo.updates.push(aType);

    switch (aType) {
      case "requestHeaders":
        networkInfo.request.headersSize = aPacket.headersSize;
        break;
      case "requestPostData":
        networkInfo.discardRequestBody = aPacket.discardRequestBody;
        networkInfo.request.bodySize = aPacket.dataSize;
        break;
      case "responseStart":
        networkInfo.response.httpVersion = aPacket.response.httpVersion;
        networkInfo.response.status = aPacket.response.status;
        networkInfo.response.statusText = aPacket.response.statusText;
        networkInfo.response.headersSize = aPacket.response.headersSize;
        networkInfo.discardResponseBody = aPacket.response.discardResponseBody;
        break;
      case "responseContent":
        networkInfo.response.content = {
          mimeType: aPacket.mimeType,
        };
        networkInfo.response.bodySize = aPacket.contentSize;
        networkInfo.discardResponseBody = aPacket.discardResponseBody;
        break;
      case "eventTimings":
        networkInfo.totalTime = aPacket.totalTime;
        break;
    }

    if (networkInfo.node) {
      this._updateNetMessage(aActorId);
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
   * @param string aActorId
   *        The network event actor ID for which you want to update the message.
   */
  _updateNetMessage: function WCF__updateNetMessage(aActorId)
  {
    let networkInfo = this._networkRequests[aActorId];
    if (!networkInfo || !networkInfo.node) {
      return;
    }

    let messageNode = networkInfo.node;
    let updates = networkInfo.updates;
    let hasEventTimings = updates.indexOf("eventTimings") > -1;
    let hasResponseStart = updates.indexOf("responseStart") > -1;
    let request = networkInfo.request;
    let response = networkInfo.response;

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

      let linkNode = messageNode.querySelector(".webconsole-msg-link");
      let statusNode = linkNode.querySelector(".webconsole-msg-status");
      statusNode.setAttribute("value", statusText);

      messageNode.clipboardText = [request.method, request.url, statusText]
                                  .join(" ");

      if (hasResponseStart && response.status >= MIN_HTTP_ERROR_CODE &&
          response.status <= MAX_HTTP_ERROR_CODE) {
        this.setMessageType(messageNode, CATEGORY_NETWORK, SEVERITY_ERROR);
      }
    }

    if (messageNode._netPanel) {
      messageNode._netPanel.update();
    }
  },

  /**
   * Opens a NetworkPanel.
   *
   * @param nsIDOMNode aNode
   *        The message node you want the panel to be anchored to.
   * @param object aHttpActivity
   *        The HTTP activity object that holds network request and response
   *        information. This object is given to the NetworkPanel constructor.
   * @return object
   *         The new NetworkPanel instance.
   */
  openNetworkPanel: function WCF_openNetworkPanel(aNode, aHttpActivity)
  {
    let actor = aHttpActivity.actor;

    if (actor) {
      this.webConsoleClient.getRequestHeaders(actor, function(aResponse) {
        if (aResponse.error) {
          Cu.reportError("WCF_openNetworkPanel getRequestHeaders:" +
                         aResponse.error);
          return;
        }

        aHttpActivity.request.headers = aResponse.headers;

        this.webConsoleClient.getRequestCookies(actor, onRequestCookies);
      }.bind(this));
    }

    let onRequestCookies = function(aResponse) {
      if (aResponse.error) {
        Cu.reportError("WCF_openNetworkPanel getRequestCookies:" +
                       aResponse.error);
        return;
      }

      aHttpActivity.request.cookies = aResponse.cookies;

      this.webConsoleClient.getResponseHeaders(actor, onResponseHeaders);
    }.bind(this);

    let onResponseHeaders = function(aResponse) {
      if (aResponse.error) {
        Cu.reportError("WCF_openNetworkPanel getResponseHeaders:" +
                       aResponse.error);
        return;
      }

      aHttpActivity.response.headers = aResponse.headers;

      this.webConsoleClient.getResponseCookies(actor, onResponseCookies);
    }.bind(this);

    let onResponseCookies = function(aResponse) {
      if (aResponse.error) {
        Cu.reportError("WCF_openNetworkPanel getResponseCookies:" +
                       aResponse.error);
        return;
      }

      aHttpActivity.response.cookies = aResponse.cookies;

      this.webConsoleClient.getRequestPostData(actor, onRequestPostData);
    }.bind(this);

    let onRequestPostData = function(aResponse) {
      if (aResponse.error) {
        Cu.reportError("WCF_openNetworkPanel getRequestPostData:" +
                       aResponse.error);
        return;
      }

      aHttpActivity.request.postData = aResponse.postData;
      aHttpActivity.discardRequestBody = aResponse.postDataDiscarded;

      this.webConsoleClient.getResponseContent(actor, onResponseContent);
    }.bind(this);

    let onResponseContent = function(aResponse) {
      if (aResponse.error) {
        Cu.reportError("WCF_openNetworkPanel getResponseContent:" +
                       aResponse.error);
        return;
      }

      aHttpActivity.response.content = aResponse.content;
      aHttpActivity.discardResponseBody = aResponse.contentDiscarded;

      this.webConsoleClient.getEventTimings(actor, onEventTimings);
    }.bind(this);

    let onEventTimings = function(aResponse) {
      if (aResponse.error) {
        Cu.reportError("WCF_openNetworkPanel getEventTimings:" +
                       aResponse.error);
        return;
      }

      aHttpActivity.timings = aResponse.timings;

      openPanel();
    }.bind(this);

    let openPanel = function() {
      aNode._netPanel = netPanel;

      let panel = netPanel.panel;
      panel.openPopup(aNode, "after_pointer", 0, 0, false, false);
      panel.sizeTo(450, 500);
      panel.setAttribute("hudId", this.hudId);

      panel.addEventListener("popuphiding", function WCF_netPanel_onHide() {
        panel.removeEventListener("popuphiding", WCF_netPanel_onHide);

        aNode._panelOpen = false;
        aNode._netPanel = null;
      });

      aNode._panelOpen = true;
    }.bind(this);

    let netPanel = new NetworkPanel(this.popupset, aHttpActivity, this);
    netPanel.linkNode = aNode;

    if (!actor) {
      openPanel();
    }

    return netPanel;
  },

  /**
   * Handler for page location changes.
   *
   * @param string aURI
   *        New page location.
   * @param string aTitle
   *        New page title.
   */
  onLocationChange: function WCF_onLocationChange(aURI, aTitle)
  {
    this.contentLocation = aURI;
    if (this.owner.onLocationChange) {
      this.owner.onLocationChange(aURI, aTitle);
    }
  },

  /**
   * Output a message node. This filters a node appropriately, then sends it to
   * the output, regrouping and pruning output as necessary.
   *
   * Note: this call is async - the given message node may not be displayed when
   * you call this method.
   *
   * @param integer aCategory
   *        The category of the message you want to output. See the CATEGORY_*
   *        constants.
   * @param function|nsIDOMElement aMethodOrNode
   *        The method that creates the message element to send to the output or
   *        the actual element. If a method is given it will be bound to the HUD
   *        object and the arguments will be |aArguments|.
   * @param array [aArguments]
   *        If a method is given to output the message element then the method
   *        will be invoked with the list of arguments given here.
   */
  outputMessage: function WCF_outputMessage(aCategory, aMethodOrNode, aArguments)
  {
    if (!this._outputQueue.length) {
      // If the queue is empty we consider that now was the last output flush.
      // This avoid an immediate output flush when the timer executes.
      this._lastOutputFlush = Date.now();
    }

    this._outputQueue.push([aCategory, aMethodOrNode, aArguments]);

    if (!this._outputTimerInitialized) {
      this._initOutputTimer();
    }
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
    if (!this._outputTimer) {
      return;
    }

    let timeSinceFlush = Date.now() - this._lastOutputFlush;
    if (this._outputQueue.length > MESSAGES_IN_INTERVAL &&
        timeSinceFlush < THROTTLE_UPDATES) {
      this._initOutputTimer();
      return;
    }

    // Determine how many messages we can display now.
    let toDisplay = Math.min(this._outputQueue.length, MESSAGES_IN_INTERVAL);
    if (toDisplay < 1) {
      this._outputTimerInitialized = false;
      return;
    }

    // Try to prune the message queue.
    let shouldPrune = false;
    if (this._outputQueue.length > toDisplay && this._pruneOutputQueue()) {
      toDisplay = Math.min(this._outputQueue.length, toDisplay);
      shouldPrune = true;
    }

    let batch = this._outputQueue.splice(0, toDisplay);
    if (!batch.length) {
      this._outputTimerInitialized = false;
      return;
    }

    let outputNode = this.outputNode;
    let lastVisibleNode = null;
    let scrolledToBottom = Utils.isOutputScrolledToBottom(outputNode);
    let scrollBox = outputNode.scrollBoxObject.element;

    let hudIdSupportsString = WebConsoleUtils.supportsString(this.hudId);

    // Output the current batch of messages.
    let newMessages = new Set();
    let updatedMessages = new Set();
    for (let item of batch) {
      let result = this._outputMessageFromQueue(hudIdSupportsString, item);
      if (result) {
        if (result.isRepeated) {
          updatedMessages.add(result.isRepeated);
        }
        else {
          newMessages.add(result.node);
        }
        if (result.visible && result.node == this.outputNode.lastChild) {
          lastVisibleNode = result.node;
        }
      }
    }

    let oldScrollHeight = 0;

    // Prune messages if needed. We do not do this for every flush call to
    // improve performance.
    let removedNodes = 0;
    if (shouldPrune || !this._outputQueue.length) {
      oldScrollHeight = scrollBox.scrollHeight;

      let categories = Object.keys(this._pruneCategoriesQueue);
      categories.forEach(function _pruneOutput(aCategory) {
        removedNodes += this.pruneOutputIfNecessary(aCategory);
      }, this);
      this._pruneCategoriesQueue = {};
    }

    // Regroup messages at the end of the queue.
    if (!this._outputQueue.length) {
      this.regroupOutput();
    }

    let isInputOutput = lastVisibleNode &&
      (lastVisibleNode.classList.contains("webconsole-msg-input") ||
       lastVisibleNode.classList.contains("webconsole-msg-output"));

    // Scroll to the new node if it is not filtered, and if the output node is
    // scrolled at the bottom or if the new node is a jsterm input/output
    // message.
    if (lastVisibleNode && (scrolledToBottom || isInputOutput)) {
      Utils.scrollToVisible(lastVisibleNode);
    }
    else if (!scrolledToBottom && removedNodes > 0 &&
             oldScrollHeight != scrollBox.scrollHeight) {
      // If there were pruned messages and if scroll is not at the bottom, then
      // we need to adjust the scroll location.
      scrollBox.scrollTop -= oldScrollHeight - scrollBox.scrollHeight;
    }

    if (newMessages.size) {
      this.emit("messages-added", newMessages);
    }
    if (updatedMessages.size) {
      this.emit("messages-updated", updatedMessages);
    }

    // If the queue is not empty, schedule another flush.
    if (this._outputQueue.length > 0) {
      this._initOutputTimer();
    }
    else {
      this._outputTimerInitialized = false;
      this._flushCallback && this._flushCallback();
    }

    this._lastOutputFlush = Date.now();
  },

  /**
   * Initialize the output timer.
   * @private
   */
  _initOutputTimer: function WCF__initOutputTimer()
  {
    if (!this._outputTimer) {
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
   * @param nsISupportsString aHudIdSupportsString
   *        The HUD ID as an nsISupportsString.
   * @param array aItem
   *        An item from the output queue - this item represents a message.
   * @return object
   *         An object that holds the following properties:
   *         - node: the DOM element of the message.
   *         - isRepeated: the DOM element of the original message, if this is
   *         a repeated message, otherwise null.
   *         - visible: boolean that tells if the message is visible.
   */
  _outputMessageFromQueue:
  function WCF__outputMessageFromQueue(aHudIdSupportsString, aItem)
  {
    let [category, methodOrNode, args] = aItem;

    let node = typeof methodOrNode == "function" ?
               methodOrNode.apply(this, args || []) :
               methodOrNode;
    if (!node) {
      return null;
    }

    let afterNode = node._outputAfterNode;
    if (afterNode) {
      delete node._outputAfterNode;
    }

    let isFiltered = this.filterMessageNode(node);

    let isRepeated = this._filterRepeatedMessage(node);

    let visible = !isRepeated && !isFiltered;
    if (!isRepeated) {
      this.outputNode.insertBefore(node,
                                   afterNode ? afterNode.nextSibling : null);
      this._pruneCategoriesQueue[node.category] = true;

      let nodeID = node.getAttribute("id");
      Services.obs.notifyObservers(aHudIdSupportsString,
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
    this._outputQueue.forEach(function(aItem, aIndex) {
      let [category] = aItem;
      if (!(category in nodes)) {
        nodes[category] = [];
      }
      nodes[category].push(aIndex);
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
          this._pruneItemFromQueue(this._outputQueue[indexes[i]]);
          this._outputQueue.splice(indexes[i], 1);
        }
      }
    }

    return pruned;
  },

  /**
   * Prune an item from the output queue.
   *
   * @private
   * @param array aItem
   *        The item you want to remove from the output queue.
   */
  _pruneItemFromQueue: function WCF__pruneItemFromQueue(aItem)
  {
    let [category, methodOrNode, args] = aItem;
    if (typeof methodOrNode != "function" && methodOrNode._objectActors) {
      for (let actor of methodOrNode._objectActors) {
        this._releaseObject(actor);
      }
      methodOrNode._objectActors.clear();
    }

    if (category == CATEGORY_NETWORK) {
      let connectionId = null;
      if (methodOrNode == this.logNetEvent) {
        connectionId = args[0];
      }
      else if (typeof methodOrNode != "function") {
        connectionId = methodOrNode._connectionId;
      }
      if (connectionId && connectionId in this._networkRequests) {
        delete this._networkRequests[connectionId];
        this._releaseObject(connectionId);
      }
    }
    else if (category == CATEGORY_WEBDEV &&
             methodOrNode == this.logConsoleAPIMessage) {
      args[0].arguments.forEach((aValue) => {
        if (WebConsoleUtils.isActorGrip(aValue)) {
          this._releaseObject(aValue.actor);
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
   * Ensures that the number of message nodes of type aCategory don't exceed that
   * category's line limit by removing old messages as needed.
   *
   * @param integer aCategory
   *        The category of message nodes to prune if needed.
   * @return number
   *         The number of removed nodes.
   */
  pruneOutputIfNecessary: function WCF_pruneOutputIfNecessary(aCategory)
  {
    let outputNode = this.outputNode;
    let logLimit = Utils.logLimitForCategory(aCategory);

    let messageNodes = outputNode.getElementsByClassName("webconsole-msg-" +
        CATEGORY_CLASS_FRAGMENTS[aCategory]);
    let n = Math.max(0, messageNodes.length - logLimit);
    let toRemove = Array.prototype.slice.call(messageNodes, 0, n);
    toRemove.forEach(this.removeOutputMessage, this);

    return n;
  },

  /**
   * Remove a given message from the output.
   *
   * @param nsIDOMNode aNode
   *        The message node you want to remove.
   */
  removeOutputMessage: function WCF_removeOutputMessage(aNode)
  {
    if (aNode._objectActors) {
      for (let actor of aNode._objectActors) {
        this._releaseObject(actor);
      }
      aNode._objectActors.clear();
    }

    if (aNode.classList.contains("webconsole-msg-cssparser") ||
        aNode.classList.contains("webconsole-msg-security")) {
      let repeatNode = aNode.getElementsByClassName("webconsole-msg-repeat")[0];
      if (repeatNode && repeatNode._uid) {
        delete this._repeatNodes[repeatNode._uid];
      }
    }
    else if (aNode._connectionId &&
             aNode.classList.contains("webconsole-msg-network")) {
      delete this._networkRequests[aNode._connectionId];
      this._releaseObject(aNode._connectionId);
    }
    else if (aNode.classList.contains("webconsole-msg-inspector")) {
      let view = aNode._variablesView;
      if (view) {
        view.controller.releaseActors();
      }
      aNode._variablesView = null;
    }

    if (aNode.parentNode) {
      aNode.parentNode.removeChild(aNode);
    }
  },

  /**
   * Splits the given console messages into groups based on their timestamps.
   */
  regroupOutput: function WCF_regroupOutput()
  {
    // Go through the nodes and adjust the placement of "webconsole-new-group"
    // classes.
    let nodes = this.outputNode.querySelectorAll(".hud-msg-node" +
      ":not(.hud-filtered-by-string):not(.hud-filtered-by-type)");
    let lastTimestamp;
    for (let i = 0, n = nodes.length; i < n; i++) {
      let thisTimestamp = nodes[i].timestamp;
      if (lastTimestamp != null &&
          thisTimestamp >= lastTimestamp + NEW_GROUP_DELAY) {
        nodes[i].classList.add("webconsole-new-group");
      }
      else {
        nodes[i].classList.remove("webconsole-new-group");
      }
      lastTimestamp = thisTimestamp;
    }
  },

  /**
   * Given a category and message body, creates a DOM node to represent an
   * incoming message. The timestamp is automatically added.
   *
   * @param number aCategory
   *        The category of the message: one of the CATEGORY_* constants.
   * @param number aSeverity
   *        The severity of the message: one of the SEVERITY_* constants;
   * @param string|nsIDOMNode aBody
   *        The body of the message, either a simple string or a DOM node.
   * @param string aSourceURL [optional]
   *        The URL of the source file that emitted the error.
   * @param number aSourceLine [optional]
   *        The line number on which the error occurred. If zero or omitted,
   *        there is no line number associated with this message.
   * @param string aClipboardText [optional]
   *        The text that should be copied to the clipboard when this node is
   *        copied. If omitted, defaults to the body text. If `aBody` is not
   *        a string, then the clipboard text must be supplied.
   * @param number aLevel [optional]
   *        The level of the console API message.
   * @param number aTimeStamp [optional]
   *        The timestamp to use for this message node. If omitted, the current
   *        date and time is used.
   * @return nsIDOMNode
   *         The message node: a XUL richlistitem ready to be inserted into
   *         the Web Console output node.
   */
  createMessageNode:
  function WCF_createMessageNode(aCategory, aSeverity, aBody, aSourceURL,
                                 aSourceLine, aClipboardText, aLevel, aTimeStamp)
  {
    if (typeof aBody != "string" && aClipboardText == null && aBody.innerText) {
      aClipboardText = aBody.innerText;
    }

    // Make the icon container, which is a vertical box. Its purpose is to
    // ensure that the icon stays anchored at the top of the message even for
    // long multi-line messages.
    let iconContainer = this.document.createElementNS(XUL_NS, "vbox");
    iconContainer.classList.add("webconsole-msg-icon-container");
    // Apply the curent group by indenting appropriately.
    iconContainer.style.marginLeft = this.groupDepth * GROUP_INDENT + "px";

    // Make the icon node. It's sprited and the actual region of the image is
    // determined by CSS rules.
    let iconNode = this.document.createElementNS(XUL_NS, "image");
    iconNode.classList.add("webconsole-msg-icon");
    iconContainer.appendChild(iconNode);

    // Make the spacer that positions the icon.
    let spacer = this.document.createElementNS(XUL_NS, "spacer");
    spacer.flex = 1;
    iconContainer.appendChild(spacer);

    // Create the message body, which contains the actual text of the message.
    let bodyNode = this.document.createElementNS(XUL_NS, "description");
    bodyNode.flex = 1;
    bodyNode.classList.add("webconsole-msg-body");

    // Store the body text, since it is needed later for the variables view.
    let body = aBody;
    // If a string was supplied for the body, turn it into a DOM node and an
    // associated clipboard string now.
    aClipboardText = aClipboardText ||
                     (aBody + (aSourceURL ? " @ " + aSourceURL : "") +
                              (aSourceLine ? ":" + aSourceLine : ""));

    // Create the containing node and append all its elements to it.
    let node = this.document.createElementNS(XUL_NS, "richlistitem");

    if (aBody instanceof Ci.nsIDOMNode) {
      bodyNode.appendChild(aBody);
    }
    else {
      let str = undefined;
      if (aLevel == "dir") {
        str = VariablesView.getString(aBody.arguments[0]);
      }
      else if (["log", "info", "warn", "error", "debug"].indexOf(aLevel) > -1 &&
               typeof aBody == "object") {
        this._makeConsoleLogMessageBody(node, bodyNode, aBody);
      }
      else {
        str = aBody;
      }

      if (str !== undefined) {
        aBody = this.document.createTextNode(str);
        bodyNode.appendChild(aBody);
      }
    }

    let repeatContainer = this.document.createElementNS(XUL_NS, "hbox");
    repeatContainer.setAttribute("align", "start");
    let repeatNode = this.document.createElementNS(XUL_NS, "label");
    repeatNode.setAttribute("value", "1");
    repeatNode.classList.add("webconsole-msg-repeat");
    repeatNode._uid = [bodyNode.textContent, aCategory, aSeverity, aLevel,
                       aSourceURL, aSourceLine].join(":");
    repeatContainer.appendChild(repeatNode);

    // Create the timestamp.
    let timestampNode = this.document.createElementNS(XUL_NS, "label");
    timestampNode.classList.add("webconsole-timestamp");
    let timestamp = aTimeStamp || Date.now();
    let timestampString = l10n.timestampString(timestamp);
    timestampNode.setAttribute("value", timestampString);

    // Create the source location (e.g. www.example.com:6) that sits on the
    // right side of the message, if applicable.
    let locationNode;
    if (aSourceURL && IGNORED_SOURCE_URLS.indexOf(aSourceURL) == -1) {
      locationNode = this.createLocationNode(aSourceURL, aSourceLine);
    }

    node.clipboardText = aClipboardText;
    node.classList.add("hud-msg-node");

    node.timestamp = timestamp;
    this.setMessageType(node, aCategory, aSeverity);

    node.appendChild(timestampNode);
    node.appendChild(iconContainer);

    // Display the variables view after the message node.
    if (aLevel == "dir") {
      let viewContainer = this.document.createElement("hbox");
      viewContainer.flex = 1;
      viewContainer.height = this.outputNode.clientHeight *
                             CONSOLE_DIR_VIEW_HEIGHT;

      let options = {
        objectActor: body.arguments[0],
        targetElement: viewContainer,
        hideFilterInput: true,
      };
      this.jsterm.openVariablesView(options).then((aView) => {
        node._variablesView = aView;
        if (node.classList.contains("hidden-message")) {
          node.classList.remove("hidden-message");
        }
      });

      let bodyContainer = this.document.createElement("vbox");
      bodyContainer.flex = 1;
      bodyContainer.appendChild(bodyNode);
      bodyContainer.appendChild(viewContainer);
      node.appendChild(bodyContainer);
      node.classList.add("webconsole-msg-inspector");
    }
    else {
      node.appendChild(bodyNode);
    }
    node.appendChild(repeatContainer);
    if (locationNode) {
      node.appendChild(locationNode);
    }

    node.setAttribute("id", "console-msg-" + gSequenceId());

    return node;
  },

  /**
   * Make the message body for console.log() calls.
   *
   * @private
   * @param nsIDOMElement aMessage
   *        The message element that holds the output for the given call.
   * @param nsIDOMElement aContainer
   *        The specific element that will hold each part of the console.log
   *        output.
   * @param object aBody
   *        The object given by this.logConsoleAPIMessage(). This object holds
   *        the call information that we need to display - mainly the arguments
   *        array of the given API call.
   */
  _makeConsoleLogMessageBody:
  function WCF__makeConsoleLogMessageBody(aMessage, aContainer, aBody)
  {
    Object.defineProperty(aMessage, "_panelOpen", {
      get: function() {
        let nodes = aContainer.querySelectorAll(".hud-clickable");
        return Array.prototype.some.call(nodes, function(aNode) {
          return aNode._panelOpen;
        });
      },
      enumerable: true,
      configurable: false
    });

    aBody.arguments.forEach(function(aItem) {
      if (aContainer.firstChild) {
        aContainer.appendChild(this.document.createTextNode(" "));
      }

      let text = VariablesView.getString(aItem);
      let inspectable = !VariablesView.isPrimitive({ value: aItem });

      if (aItem && typeof aItem != "object" || !inspectable) {
        aContainer.appendChild(this.document.createTextNode(text));

        if (aItem.type && aItem.type == "longString") {
          let ellipsis = this.document.createElement("description");
          ellipsis.classList.add("hud-clickable");
          ellipsis.classList.add("longStringEllipsis");
          ellipsis.textContent = l10n.getStr("longStringEllipsis");

          let formatter = function(s) '"' + s + '"';

          this._addMessageLinkCallback(ellipsis,
            this._longStringClick.bind(this, aMessage, aItem, formatter));

          aContainer.appendChild(ellipsis);
        }
        return;
      }

      // For inspectable objects.
      let elem = this.document.createElement("description");
      elem.classList.add("hud-clickable");
      elem.setAttribute("aria-haspopup", "true");
      elem.appendChild(this.document.createTextNode(text));

      this._addMessageLinkCallback(elem,
        this._consoleLogClick.bind(this, elem, aItem));

      aContainer.appendChild(elem);
    }, this);
  },

  /**
   * Click event handler for the ellipsis shown immediately after a long string.
   * This method retrieves the full string and updates the console output to
   * show it.
   *
   * @private
   * @param nsIDOMElement aMessage
   *        The message element.
   * @param object aActor
   *        The LongStringActor instance we work with.
   * @param [function] aFormatter
   *        Optional function you can use to format the string received from the
   *        server, before being displayed in the console.
   * @param nsIDOMElement aEllipsis
   *        The DOM element the user can click on to expand the string.
   * @param nsIDOMEvent aEvent
   *        The DOM click event triggered by the user.
   */
  _longStringClick:
  function WCF__longStringClick(aMessage, aActor, aFormatter, aEllipsis, aEvent)
  {
    aEvent.preventDefault();

    if (!aFormatter) {
      aFormatter = function(s) s;
    }

    let longString = this.webConsoleClient.longString(aActor);
    let toIndex = Math.min(longString.length, MAX_LONG_STRING_LENGTH);
    longString.substring(longString.initial.length, toIndex,
      function WCF__onSubstring(aResponse) {
        if (aResponse.error) {
          Cu.reportError("WCF__longStringClick substring failure: " +
                         aResponse.error);
          return;
        }

        let node = aEllipsis.previousSibling;
        node.textContent = aFormatter(longString.initial + aResponse.substring);
        aEllipsis.parentNode.removeChild(aEllipsis);

        if (aMessage.category == CATEGORY_WEBDEV ||
            aMessage.category == CATEGORY_OUTPUT) {
          aMessage.clipboardText = aMessage.textContent;
        }

        this.emit("messages-updated", new Set([aMessage]));

        if (toIndex != longString.length) {
          this.logWarningAboutStringTooLong();
        }
      }.bind(this));
  },

  /**
   * Creates the XUL label that displays the textual location of an incoming
   * message.
   *
   * @param string aSourceURL
   *        The URL of the source file responsible for the error.
   * @param number aSourceLine [optional]
   *        The line number on which the error occurred. If zero or omitted,
   *        there is no line number associated with this message.
   * @return nsIDOMNode
   *         The new XUL label node, ready to be added to the message node.
   */
  createLocationNode: function WCF_createLocationNode(aSourceURL, aSourceLine)
  {
    let locationNode = this.document.createElementNS(XUL_NS, "label");

    // Create the text, which consists of an abbreviated version of the URL
    // plus an optional line number. Scratchpad URLs should not be abbreviated.
    let displayLocation;
    let fullURL;

    if (/^Scratchpad\/\d+$/.test(aSourceURL)) {
      displayLocation = aSourceURL;
      fullURL = aSourceURL;
    }
    else {
      fullURL = aSourceURL.split(" -> ").pop();
      displayLocation = WebConsoleUtils.abbreviateSourceURL(fullURL);
    }

    if (aSourceLine) {
      displayLocation += ":" + aSourceLine;
      locationNode.sourceLine = aSourceLine;
    }

    locationNode.setAttribute("value", displayLocation);

    // Style appropriately.
    locationNode.setAttribute("crop", "center");
    locationNode.setAttribute("title", aSourceURL);
    locationNode.setAttribute("tooltiptext", aSourceURL);
    locationNode.classList.add("webconsole-location");
    locationNode.classList.add("text-link");

    // Make the location clickable.
    locationNode.addEventListener("click", () => {
      if (/^Scratchpad\/\d+$/.test(aSourceURL)) {
        let wins = Services.wm.getEnumerator("devtools:scratchpad");

        while (wins.hasMoreElements()) {
          let win = wins.getNext();

          if (win.Scratchpad.uniqueName === aSourceURL) {
            win.focus();
            return;
          }
        }
      }
      else if (locationNode.parentNode.category == CATEGORY_CSS) {
        this.owner.viewSourceInStyleEditor(fullURL, aSourceLine);
      }
      else if (locationNode.parentNode.category == CATEGORY_JS ||
               locationNode.parentNode.category == CATEGORY_WEBDEV) {
        this.owner.viewSourceInDebugger(fullURL, aSourceLine);
      }
      else {
        this.owner.viewSource(fullURL, aSourceLine);
      }
    }, true);

    return locationNode;
  },

  /**
   * Adjusts the category and severity of the given message, clearing the old
   * category and severity if present.
   *
   * @param nsIDOMNode aMessageNode
   *        The message node to alter.
   * @param number aNewCategory
   *        The new category for the message; one of the CATEGORY_ constants.
   * @param number aNewSeverity
   *        The new severity for the message; one of the SEVERITY_ constants.
   * @return void
   */
  setMessageType:
  function WCF_setMessageType(aMessageNode, aNewCategory, aNewSeverity)
  {
    // Remove the old CSS classes, if applicable.
    if ("category" in aMessageNode) {
      let oldCategory = aMessageNode.category;
      let oldSeverity = aMessageNode.severity;
      aMessageNode.classList.remove("webconsole-msg-" +
                                    CATEGORY_CLASS_FRAGMENTS[oldCategory]);
      aMessageNode.classList.remove("webconsole-msg-" +
                                    SEVERITY_CLASS_FRAGMENTS[oldSeverity]);
      let key = "hud-" + MESSAGE_PREFERENCE_KEYS[oldCategory][oldSeverity];
      aMessageNode.classList.remove(key);
    }

    // Add in the new CSS classes.
    aMessageNode.category = aNewCategory;
    aMessageNode.severity = aNewSeverity;
    aMessageNode.classList.add("webconsole-msg-" +
                               CATEGORY_CLASS_FRAGMENTS[aNewCategory]);
    aMessageNode.classList.add("webconsole-msg-" +
                               SEVERITY_CLASS_FRAGMENTS[aNewSeverity]);
    let key = "hud-" + MESSAGE_PREFERENCE_KEYS[aNewCategory][aNewSeverity];
    aMessageNode.classList.add(key);
  },

  /**
   * Make a link given an output element.
   *
   * @param nsIDOMNode aNode
   *        The message element you want to make a link for.
   * @param function aCallback
   *        The function you want invoked when the user clicks on the message
   *        element.
   */
  makeOutputMessageLink: function WCF_makeOutputMessageLink(aNode, aCallback)
  {
    let linkNode;
    if (aNode.category === CATEGORY_NETWORK) {
      linkNode = aNode.querySelector(".webconsole-msg-link, .webconsole-msg-url");
    }
    else {
      linkNode = aNode.querySelector(".webconsole-msg-body");
      linkNode.classList.add("hud-clickable");
    }

    linkNode.setAttribute("aria-haspopup", "true");

    this._addMessageLinkCallback(aNode, aCallback);
  },

  /**
   * Add the mouse event handlers needed to make a link.
   *
   * @private
   * @param nsIDOMNode aNode
   *        The node for which you want to add the event handlers.
   * @param function aCallback
   *        The function you want to invoke on click.
   */
  _addMessageLinkCallback: function WCF__addMessageLinkCallback(aNode, aCallback)
  {
    aNode.addEventListener("mousedown", function(aEvent) {
      this._startX = aEvent.clientX;
      this._startY = aEvent.clientY;
    }, false);

    aNode.addEventListener("click", function(aEvent) {
      if (aEvent.detail != 1 || aEvent.button != 0 ||
          (this._startX != aEvent.clientX &&
           this._startY != aEvent.clientY)) {
        return;
      }

      aCallback(this, aEvent);
    }, false);
  },

  /**
   * Copies the selected items to the system clipboard.
   *
   * @param object aOptions
   *        - linkOnly:
   *        An optional flag to copy only URL without timestamp and
   *        other meta-information. Default is false.
   */
  copySelectedItems: function WCF_copySelectedItems(aOptions)
  {
    aOptions = aOptions || { linkOnly: false };

    // Gather up the selected items and concatenate their clipboard text.
    let strings = [];
    let newGroup = false;

    let children = this.outputNode.children;

    for (let i = 0; i < children.length; i++) {
      let item = children[i];
      if (!item.selected) {
        continue;
      }

      // Add dashes between groups so that group boundaries show up in the
      // copied output.
      if (i > 0 && item.classList.contains("webconsole-new-group")) {
        newGroup = true;
      }

      // Ensure the selected item hasn't been filtered by type or string.
      if (!item.classList.contains("hud-filtered-by-type") &&
          !item.classList.contains("hud-filtered-by-string")) {
        let timestampString = l10n.timestampString(item.timestamp);
        if (newGroup) {
          strings.push("--");
          newGroup = false;
        }

        if (aOptions.linkOnly) {
          strings.push(item.url);
        }
        else {
          strings.push("[" + timestampString + "] " + item.clipboardText);
        }
      }
    }

    clipboardHelper.copyString(strings.join("\n"), this.document);
  },

  /**
   * Object properties provider. This function gives you the properties of the
   * remote object you want.
   *
   * @param string aActor
   *        The object actor ID from which you want the properties.
   * @param function aCallback
   *        Function you want invoked once the properties are received.
   */
  objectPropertiesProvider:
  function WCF_objectPropertiesProvider(aActor, aCallback)
  {
    this.webConsoleClient.inspectObjectProperties(aActor,
      function(aResponse) {
        if (aResponse.error) {
          Cu.reportError("Failed to retrieve the object properties from the " +
                         "server. Error: " + aResponse.error);
          return;
        }
        aCallback(aResponse.properties);
      });
  },

  /**
   * Release an actor.
   *
   * @private
   * @param string aActor
   *        The actor ID you want to release.
   */
  _releaseObject: function WCF__releaseObject(aActor)
  {
    if (this.proxy) {
      this.proxy.releaseActor(aActor);
    }
  },

  /**
   * Open the selected item's URL in a new tab.
   */
  openSelectedItemInTab: function WCF_openSelectedItemInTab()
  {
    let item = this.outputNode.selectedItem;

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

    this._repeatNodes = {};
    this._outputQueue = [];
    this._pruneCategoriesQueue = {};
    this._networkRequests = {};

    if (this._outputTimerInitialized) {
      this._outputTimerInitialized = false;
      this._outputTimer.cancel();
    }
    this._outputTimer = null;

    if (this.jsterm) {
      this.jsterm.destroy();
      this.jsterm = null;
    }

    this._commandController = null;

    let onDestroy = function() {
      this._destroyer.resolve(null);
    }.bind(this);

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
function simpleValueEvalMacro(aItem, aCurrentString)
{
  return VariablesView.simpleValueEvalMacro(aItem, aCurrentString, "_self");
};


/**
 * @see VariablesView.overrideValueEvalMacro
 */
function overrideValueEvalMacro(aItem, aCurrentString)
{
  return VariablesView.overrideValueEvalMacro(aItem, aCurrentString, "_self");
};


/**
 * @see VariablesView.getterOrSetterEvalMacro
 */
function getterOrSetterEvalMacro(aItem, aCurrentString)
{
  return VariablesView.getterOrSetterEvalMacro(aItem, aCurrentString, "_self");
}



/**
 * Create a JSTerminal (a JavaScript command line). This is attached to an
 * existing HeadsUpDisplay (a Web Console instance). This code is responsible
 * with handling command line input, code evaluation and result output.
 *
 * @constructor
 * @param object aWebConsoleFrame
 *        The WebConsoleFrame object that owns this JSTerm instance.
 */
function JSTerm(aWebConsoleFrame)
{
  this.hud = aWebConsoleFrame;
  this.hudId = this.hud.hudId;

  this.lastCompletion = { value: null };
  this.history = [];

  // Holds the number of entries in history. This value is incremented in
  // this.execute().
  this.historyIndex = 0; // incremented on this.execute()

  // Holds the index of the history entry that the user is currently viewing.
  // This is reset to this.history.length when this.execute() is invoked.
  this.historyPlaceHolder = 0;
  this._objectActorsInVariablesViews = new Map();

  this._keyPress = this._keyPress.bind(this);
  this._inputEventHandler = this._inputEventHandler.bind(this);
  this._focusEventHandler = this._focusEventHandler.bind(this);
  this._onKeypressInVariablesView = this._onKeypressInVariablesView.bind(this);

  EventEmitter.decorate(this);
}

JSTerm.prototype = {
  SELECTED_FRAME: -1,

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
  get outputNode() this.hud.outputNode,

  /**
   * Getter for the debugger WebConsoleClient.
   * @type object
   */
  get webConsoleClient() this.hud.webConsoleClient,

  COMPLETE_FORWARD: 0,
  COMPLETE_BACKWARD: 1,
  COMPLETE_HINT_ONLY: 2,

  /**
   * Initialize the JSTerminal UI.
   */
  init: function JST_init()
  {
    let chromeDocument = this.hud.owner.chromeWindow.document;
    let autocompleteOptions = {
      onSelect: this.onAutocompleteSelect.bind(this),
      onClick: this.acceptProposedCompletion.bind(this),
      panelId: "webConsole_autocompletePopup",
      listBoxId: "webConsole_autocompletePopupListBox",
      position: "before_start",
      theme: "light",
      direction: "ltr",
      autoSelect: true
    };
    this.autocompletePopup = new AutocompletePopup(chromeDocument,
                                                   autocompleteOptions);

    let doc = this.hud.document;
    this.completeNode = doc.querySelector(".jsterm-complete-node");
    this.inputNode = doc.querySelector(".jsterm-input-node");
    this.inputNode.addEventListener("keypress", this._keyPress, false);
    this.inputNode.addEventListener("input", this._inputEventHandler, false);
    this.inputNode.addEventListener("keyup", this._inputEventHandler, false);
    this.inputNode.addEventListener("focus", this._focusEventHandler, false);

    this.lastInputValue && this.setInputValue(this.lastInputValue);
  },

  /**
   * The JavaScript evaluation response handler.
   *
   * @private
   * @param nsIDOMElement [aAfterNode]
   *        Optional DOM element after which the evaluation result will be
   *        inserted.
   * @param function [aCallback]
   *        Optional function to invoke when the evaluation result is added to
   *        the output.
   * @param object aResponse
   *        The message received from the server.
   */
  _executeResultCallback:
  function JST__executeResultCallback(aAfterNode, aCallback, aResponse)
  {
    if (!this.hud) {
      return;
    }
    if (aResponse.error) {
      Cu.reportError("Evaluation error " + aResponse.error + ": " +
                     aResponse.message);
      return;
    }
    let errorMessage = aResponse.exceptionMessage;
    let result = aResponse.result;
    let inspectable = false;
    if (result && !VariablesView.isPrimitive({ value: result })) {
      inspectable = true;
    }
    let helperResult = aResponse.helperResult;
    let helperHasRawOutput = !!(helperResult || {}).rawOutput;
    let resultString = VariablesView.getString(result);

    if (helperResult && helperResult.type) {
      switch (helperResult.type) {
        case "clearOutput":
          this.clearOutput();
          break;
        case "inspectObject":
          if (aAfterNode) {
            if (!aAfterNode._objectActors) {
              aAfterNode._objectActors = new Set();
            }
            aAfterNode._objectActors.add(helperResult.object.actor);
          }
          this.openVariablesView({
            label: VariablesView.getString(helperResult.object),
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
      }
    }

    // Hide undefined results coming from JSTerm helper functions.
    if (!errorMessage && result && typeof result == "object" &&
        result.type == "undefined" &&
        helperResult && !helperHasRawOutput) {
      aCallback && aCallback();
      return;
    }

    if (aCallback) {
      let oldFlushCallback = this.hud._flushCallback;
      this.hud._flushCallback = function() {
        aCallback();
        oldFlushCallback && oldFlushCallback();
        this.hud._flushCallback = oldFlushCallback;
      }.bind(this);
    }

    let node;

    if (errorMessage) {
      node = this.writeOutput(errorMessage, CATEGORY_OUTPUT, SEVERITY_ERROR,
                              aAfterNode, aResponse.timestamp);
    }
    else if (inspectable) {
      node = this.writeOutputJS(resultString,
                                this._evalOutputClick.bind(this, aResponse),
                                aAfterNode, aResponse.timestamp);
    }
    else {
      node = this.writeOutput(resultString, CATEGORY_OUTPUT, SEVERITY_LOG,
                              aAfterNode, aResponse.timestamp);
    }

    node._objectActors = new Set();

    let error = aResponse.exception;
    if (WebConsoleUtils.isActorGrip(error)) {
      node._objectActors.add(error.actor);
    }

    if (WebConsoleUtils.isActorGrip(result)) {
      node._objectActors.add(result.actor);

      if (result.type == "longString") {
        // Add an ellipsis to expand the short string if the object is not
        // inspectable.

        let body = node.querySelector(".webconsole-msg-body");
        let ellipsis = this.hud.document.createElement("description");
        ellipsis.classList.add("hud-clickable");
        ellipsis.classList.add("longStringEllipsis");
        ellipsis.textContent = l10n.getStr("longStringEllipsis");

        let formatter = function(s) '"' + s + '"';
        let onclick = this.hud._longStringClick.bind(this.hud, node, result,
                                                    formatter);
        this.hud._addMessageLinkCallback(ellipsis, onclick);

        body.appendChild(ellipsis);

        node.clipboardText += " " + ellipsis.textContent;
      }
    }
  },

  /**
   * Execute a string. Execution happens asynchronously in the content process.
   *
   * @param string [aExecuteString]
   *        The string you want to execute. If this is not provided, the current
   *        user input is used - taken from |this.inputNode.value|.
   * @param function [aCallback]
   *        Optional function to invoke when the result is displayed.
   */
  execute: function JST_execute(aExecuteString, aCallback)
  {
    // attempt to execute the content of the inputNode
    aExecuteString = aExecuteString || this.inputNode.value;
    if (!aExecuteString) {
      return;
    }

    let node = this.writeOutput(aExecuteString, CATEGORY_INPUT, SEVERITY_LOG);
    let onResult = this._executeResultCallback.bind(this, node, aCallback);

    let options = { frame: this.SELECTED_FRAME };
    this.requestEvaluation(aExecuteString, options).then(onResult, onResult);

    // Append a new value in the history of executed code, or overwrite the most
    // recent entry. The most recent entry may contain the last edited input
    // value that was not evaluated yet.
    this.history[this.historyIndex++] = aExecuteString;
    this.historyPlaceHolder = this.history.length;
    this.setInputValue("");
    this.clearCompletion();
  },

  /**
   * Request a JavaScript string evaluation from the server.
   *
   * @param string aString
   *        String to execute.
   * @param object [aOptions]
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
   * @return object
   *         A promise object that is resolved when the server response is
   *         received.
   */
  requestEvaluation: function JST_requestEvaluation(aString, aOptions = {})
  {
    let deferred = promise.defer();

    function onResult(aResponse) {
      if (!aResponse.error) {
        deferred.resolve(aResponse);
      }
      else {
        deferred.reject(aResponse);
      }
    }

    let frameActor = null;
    if ("frame" in aOptions) {
      frameActor = this.getFrameActor(aOptions.frame);
    }

    let evalOptions = {
      bindObjectActor: aOptions.bindObjectActor,
      frameActor: frameActor,
    };

    this.webConsoleClient.evaluateJS(aString, onResult, evalOptions);
    return deferred.promise;
  },

  /**
   * Retrieve the FrameActor ID given a frame depth.
   *
   * @param number aFrame
   *        Frame depth.
   * @return string|null
   *         The FrameActor ID for the given frame depth.
   */
  getFrameActor: function JST_getFrameActor(aFrame)
  {
    let state = this.hud.owner.getDebuggerFrames();
    if (!state) {
      return null;
    }

    let grip;
    if (aFrame == this.SELECTED_FRAME) {
      grip = state.frames[state.selected];
    }
    else {
      grip = state.frames[aFrame];
    }

    return grip ? grip.actor : null;
  },

  /**
   * Opens a new variables view that allows the inspection of the given object.
   *
   * @param object aOptions
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
  openVariablesView: function JST_openVariablesView(aOptions)
  {
    let onContainerReady = (aWindow) => {
      let container = aWindow.document.querySelector("#variables");
      let view = this._variablesView;
      if (!view || aOptions.targetElement) {
        let viewOptions = {
          container: container,
          hideFilterInput: aOptions.hideFilterInput,
        };
        view = this._createVariablesView(viewOptions);
        if (!aOptions.targetElement) {
          this._variablesView = view;
          aWindow.addEventListener("keypress", this._onKeypressInVariablesView);
        }
      }
      aOptions.view = view;
      this._updateVariablesView(aOptions);

      if (!aOptions.targetElement && aOptions.autofocus) {
        aWindow.focus();
      }

      this.emit("variablesview-open", view, aOptions);
      return view;
    };

    let openPromise;
    if (aOptions.targetElement) {
      let deferred = promise.defer();
      openPromise = deferred.promise;
      let document = aOptions.targetElement.ownerDocument;
      let iframe = document.createElement("iframe");

      iframe.addEventListener("load", function onIframeLoad(aEvent) {
        iframe.removeEventListener("load", onIframeLoad, true);
        deferred.resolve(iframe.contentWindow);
      }, true);

      iframe.flex = 1;
      iframe.setAttribute("src", VARIABLES_VIEW_URL);
      aOptions.targetElement.appendChild(iframe);
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
    let ToolSidebar = devtools.require("devtools/framework/sidebar").ToolSidebar;
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

    let tab = this.sidebar.getTab("variablesview");
    if (tab) {
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
   * @param nsIDOMEvent aEvent
   *        The keypress DOM event object.
   */
  _onKeypressInVariablesView: function JST__onKeypressInVariablesView(aEvent)
  {
    let tag = aEvent.target.nodeName;
    if (aEvent.keyCode != Ci.nsIDOMKeyEvent.DOM_VK_ESCAPE || aEvent.shiftKey ||
        aEvent.altKey || aEvent.ctrlKey || aEvent.metaKey ||
        ["input", "textarea", "select", "textbox"].indexOf(tag) > -1) {
        return;
    }

    this._sidebarDestroy();
    this.inputNode.focus();
  },

  /**
   * Create a variables view instance.
   *
   * @private
   * @param object aOptions
   *        Options for the new Variables View instance:
   *        - container: the DOM element where the variables view is inserted.
   *        - hideFilterInput: boolean, if true the variables filter input is
   *        hidden.
   * @return object
   *         The new Variables View instance.
   */
  _createVariablesView: function JST__createVariablesView(aOptions)
  {
    let view = new VariablesView(aOptions.container);
    view.searchPlaceholder = l10n.getStr("propertiesFilterPlaceholder");
    view.emptyText = l10n.getStr("emptyPropertiesList");
    view.searchEnabled = !aOptions.hideFilterInput;
    view.lazyEmpty = this._lazyVariablesView;
    view.lazyAppend = this._lazyVariablesView;

    VariablesViewController.attach(view, {
      getGripClient: aGrip => {
        return new GripClient(this.hud.proxy.client, aGrip);
      },
      getLongStringClient: aGrip => {
        return this.webConsoleClient.longString(aGrip);
      },
      releaseActor: aActor => {
        this.hud._releaseObject(aActor);
      },
      simpleValueEvalMacro: simpleValueEvalMacro,
      overrideValueEvalMacro: overrideValueEvalMacro,
      getterOrSetterEvalMacro: getterOrSetterEvalMacro,
    });

    // Relay events from the VariablesView.
    view.on("fetched", (aEvent, aType, aVar) => {
      this.emit("variablesview-fetched", aVar);
    });

    return view;
  },

  /**
   * Update the variables view.
   *
   * @private
   * @param object aOptions
   *        Options for updating the variables view:
   *        - view: the view you want to update.
   *        - objectActor: the grip of the new ObjectActor you want to show in
   *        the view.
   *        - rawObject: the new raw object you want to show.
   *        - label: the new label for the inspected object.
   */
  _updateVariablesView: function JST__updateVariablesView(aOptions)
  {
    let view = aOptions.view;
    view.createHierarchy();
    view.empty();

    // We need to avoid pruning the object inspection starting point.
    // That one is pruned when the console message is removed.
    view.controller.releaseActors(aActor => {
      return view._consoleLastObjectActor != aActor;
    });

    if (aOptions.objectActor) {
      // Make sure eval works in the correct context.
      view.eval = this._variablesViewEvaluate.bind(this, aOptions);
      view.switch = this._variablesViewSwitch.bind(this, aOptions);
      view.delete = this._variablesViewDelete.bind(this, aOptions);
    }
    else {
      view.eval = null;
      view.switch = null;
      view.delete = null;
    }

    let scope = view.addScope(aOptions.label);
    scope.expanded = true;
    scope.locked = true;

    let container = scope.addItem();
    container.evaluationMacro = simpleValueEvalMacro;

    if (aOptions.objectActor) {
      view.controller.expand(container, aOptions.objectActor);
      view._consoleLastObjectActor = aOptions.objectActor.actor;
    }
    else if (aOptions.rawObject) {
      container.populate(aOptions.rawObject);
      view.commitHierarchy();
      view._consoleLastObjectActor = null;
    }
    else {
      throw new Error("Variables View cannot open without giving it an object " +
                      "display.");
    }

    this.emit("variablesview-updated", view, aOptions);
  },

  /**
   * The evaluation function used by the variables view when editing a property
   * value.
   *
   * @private
   * @param object aOptions
   *        The options used for |this._updateVariablesView()|.
   * @param string aString
   *        The string that the variables view wants to evaluate.
   */
  _variablesViewEvaluate: function JST__variablesViewEvaluate(aOptions, aString)
  {
    let updater = this._updateVariablesView.bind(this, aOptions);
    let onEval = this._silentEvalCallback.bind(this, updater);

    let evalOptions = {
      frame: this.SELECTED_FRAME,
      bindObjectActor: aOptions.objectActor.actor,
    };

    this.requestEvaluation(aString, evalOptions).then(onEval, onEval);
  },

  /**
   * The property deletion function used by the variables view when a property
   * is deleted.
   *
   * @private
   * @param object aOptions
   *        The options used for |this._updateVariablesView()|.
   * @param object aVar
   *        The Variable object instance for the deleted property.
   */
  _variablesViewDelete: function JST__variablesViewDelete(aOptions, aVar)
  {
    let onEval = this._silentEvalCallback.bind(this, null);

    let evalOptions = {
      frame: this.SELECTED_FRAME,
      bindObjectActor: aOptions.objectActor.actor,
    };

    this.requestEvaluation("delete _self" + aVar.symbolicName, evalOptions)
        .then(onEval, onEval);
  },

  /**
   * The property rename function used by the variables view when a property
   * is renamed.
   *
   * @private
   * @param object aOptions
   *        The options used for |this._updateVariablesView()|.
   * @param object aVar
   *        The Variable object instance for the renamed property.
   * @param string aNewName
   *        The new name for the property.
   */
  _variablesViewSwitch:
  function JST__variablesViewSwitch(aOptions, aVar, aNewName)
  {
    let updater = this._updateVariablesView.bind(this, aOptions);
    let onEval = this._silentEvalCallback.bind(this, updater);

    let evalOptions = {
      frame: this.SELECTED_FRAME,
      bindObjectActor: aOptions.objectActor.actor,
    };

    let newSymbolicName = aVar.ownerView.symbolicName + '["' + aNewName + '"]';
    if (newSymbolicName == aVar.symbolicName) {
      return;
    }

    let code = "_self" + newSymbolicName + " = _self" + aVar.symbolicName + ";" +
               "delete _self" + aVar.symbolicName;

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
   * @param function aCallback
   *        Function to invoke once the response is received.
   * @param object aResponse
   *        The response packet received from the server.
   */
  _silentEvalCallback: function JST__silentEvalCallback(aCallback, aResponse)
  {
    if (aResponse.error) {
      Cu.reportError("Web Console evaluation failed. " + aResponse.error + ":" +
                     aResponse.message);

      aCallback && aCallback(aResponse);
      return;
    }

    let exception = aResponse.exception;
    if (exception) {
      let node = this.writeOutput(aResponse.exceptionMessage,
                                  CATEGORY_OUTPUT, SEVERITY_ERROR,
                                  null, aResponse.timestamp);
      node._objectActors = new Set();
      if (WebConsoleUtils.isActorGrip(exception)) {
        node._objectActors.add(exception.actor);
      }
    }

    let helper = aResponse.helperResult || { type: null };
    let helperGrip = null;
    if (helper.type == "inspectObject") {
      helperGrip = helper.object;
    }

    let grips = [aResponse.result, helperGrip];
    for (let grip of grips) {
      if (WebConsoleUtils.isActorGrip(grip)) {
        this.hud._releaseObject(grip.actor);
      }
    }

    aCallback && aCallback(aResponse);
  },



  /**
   * Writes a JS object to the JSTerm outputNode.
   *
   * @param string aOutputMessage
   *        The message to display.
   * @param function [aCallback]
   *        Optional function to invoke when users click the message.
   * @param nsIDOMNode [aNodeAfter]
   *        Optional DOM node after which you want to insert the new message.
   *        This is used when execution results need to be inserted immediately
   *        after the user input.
   * @param number [aTimestamp]
   *        Optional timestamp to show for the output message (millisconds since
   *        the UNIX epoch). If no timestamp is provided then Date.now() is
   *        used.
   * @return nsIDOMNode
   *         The new message node.
   */
  writeOutputJS:
  function JST_writeOutputJS(aOutputMessage, aCallback, aNodeAfter, aTimestamp)
  {
    let node = this.writeOutput(aOutputMessage, CATEGORY_OUTPUT, SEVERITY_LOG,
                                aNodeAfter, aTimestamp);
    if (aCallback) {
      this.hud.makeOutputMessageLink(node, aCallback);
    }
    return node;
  },

  /**
   * Writes a message to the HUD that originates from the interactive
   * JavaScript console.
   *
   * @param string aOutputMessage
   *        The message to display.
   * @param number aCategory
   *        The category of message: one of the CATEGORY_ constants.
   * @param number aSeverity
   *        The severity of message: one of the SEVERITY_ constants.
   * @param nsIDOMNode [aNodeAfter]
   *        Optional DOM node after which you want to insert the new message.
   *        This is used when execution results need to be inserted immediately
   *        after the user input.
   * @param number [aTimestamp]
   *        Optional timestamp to show for the output message (millisconds since
   *        the UNIX epoch). If no timestamp is provided then Date.now() is
   *        used.
   * @return nsIDOMNode
   *         The new message node.
   */
  writeOutput:
  function JST_writeOutput(aOutputMessage, aCategory, aSeverity, aNodeAfter,
                           aTimestamp)
  {
    let node = this.hud.createMessageNode(aCategory, aSeverity, aOutputMessage,
                                          null, null, null, null, aTimestamp);
    node._outputAfterNode = aNodeAfter;
    this.hud.outputMessage(aCategory, node);
    return node;
  },

  /**
   * Clear the Web Console output.
   *
   * This method emits the "messages-cleared" notification.
   *
   * @param boolean aClearStorage
   *        True if you want to clear the console messages storage associated to
   *        this Web Console.
   */
  clearOutput: function JST_clearOutput(aClearStorage)
  {
    let hud = this.hud;
    let outputNode = hud.outputNode;
    let node;
    while ((node = outputNode.firstChild)) {
      hud.removeOutputMessage(node);
    }

    hud.groupDepth = 0;
    hud._outputQueue.forEach(hud._pruneItemFromQueue, hud);
    hud._outputQueue = [];
    hud._networkRequests = {};
    hud._repeatNodes = {};

    if (aClearStorage) {
      this.webConsoleClient.clearMessagesCache();
    }

    this.emit("messages-cleared");
  },

  /**
   * Remove all of the private messages from the Web Console output.
   *
   * This method emits the "private-messages-cleared" notification.
   */
  clearPrivateMessages: function JST_clearPrivateMessages()
  {
    let nodes = this.hud.outputNode.querySelectorAll("richlistitem[private]");
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
   * @param string aNewValue
   *        The new value to set.
   * @returns void
   */
  setInputValue: function JST_setInputValue(aNewValue)
  {
    this.inputNode.value = aNewValue;
    this.lastInputValue = aNewValue;
    this.completeNode.value = "";
    this.resizeInput();
    this._inputChanged = true;
  },

  /**
   * The inputNode "input" and "keyup" event handler.
   * @private
   */
  _inputEventHandler: function JST__inputEventHandler()
  {
    if (this.lastInputValue != this.inputNode.value) {
      this.resizeInput();
      this.complete(this.COMPLETE_HINT_ONLY);
      this.lastInputValue = this.inputNode.value;
      this._inputChanged = true;
    }
  },

  /**
   * The inputNode "keypress" event handler.
   *
   * @private
   * @param nsIDOMEvent aEvent
   */
  _keyPress: function JST__keyPress(aEvent)
  {
    let inputNode = this.inputNode;
    let inputUpdated = false;

    if (aEvent.ctrlKey) {
      switch (aEvent.charCode) {
        case 97:
          // control-a
          this.clearCompletion();

          if (Services.appinfo.OS == "WINNT") {
            // Allow Select All on Windows.
            break;
          }

          let lineBeginPos = 0;
          if (this.hasMultilineInput()) {
            // find index of closest newline <= to cursor
            for (let i = inputNode.selectionStart-1; i >= 0; i--) {
              if (inputNode.value.charAt(i) == "\r" ||
                  inputNode.value.charAt(i) == "\n") {
                lineBeginPos = i+1;
                break;
              }
            }
          }
          inputNode.setSelectionRange(lineBeginPos, lineBeginPos);
          aEvent.preventDefault();
          break;

        case 101:
          // control-e
          if (Services.appinfo.OS == "WINNT") {
            break;
          }
          let lineEndPos = inputNode.value.length;
          if (this.hasMultilineInput()) {
            // find index of closest newline >= cursor
            for (let i = inputNode.selectionEnd; i<lineEndPos; i++) {
              if (inputNode.value.charAt(i) == "\r" ||
                  inputNode.value.charAt(i) == "\n") {
                lineEndPos = i;
                break;
              }
            }
          }
          inputNode.setSelectionRange(lineEndPos, lineEndPos);
          aEvent.preventDefault();
          this.clearCompletion();
          break;

        case 110:
          // Control-N differs from down arrow: it ignores autocomplete state.
          // Note that we preserve the default 'down' navigation within
          // multiline text.
          if (Services.appinfo.OS == "Darwin" &&
              this.canCaretGoNext() &&
              this.historyPeruse(HISTORY_FORWARD)) {
            aEvent.preventDefault();
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
            aEvent.preventDefault();
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
    else if (aEvent.shiftKey &&
        aEvent.keyCode == Ci.nsIDOMKeyEvent.DOM_VK_RETURN) {
      // shift return
      // TODO: expand the inputNode height by one line
      return;
    }

    switch (aEvent.keyCode) {
      case Ci.nsIDOMKeyEvent.DOM_VK_ESCAPE:
        if (this.autocompletePopup.isOpen) {
          this.clearCompletion();
          aEvent.preventDefault();
        }
        else if (this.sidebar) {
          this._sidebarDestroy();
          aEvent.preventDefault();
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
        aEvent.preventDefault();
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
          aEvent.preventDefault();
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
          aEvent.preventDefault();
        }
        break;

      case Ci.nsIDOMKeyEvent.DOM_VK_HOME:
      case Ci.nsIDOMKeyEvent.DOM_VK_END:
      case Ci.nsIDOMKeyEvent.DOM_VK_LEFT:
        if (this.autocompletePopup.isOpen || this.lastCompletion.value) {
          this.clearCompletion();
        }
        break;

      case Ci.nsIDOMKeyEvent.DOM_VK_RIGHT: {
        let cursorAtTheEnd = this.inputNode.selectionStart ==
                             this.inputNode.selectionEnd &&
                             this.inputNode.selectionStart ==
                             this.inputNode.value.length;
        let haveSuggestion = this.autocompletePopup.isOpen ||
                             this.lastCompletion.value;
        let useCompletion = cursorAtTheEnd || this._autocompletePopupNavigated;
        if (haveSuggestion && useCompletion &&
            this.complete(this.COMPLETE_HINT_ONLY) &&
            this.lastCompletion.value &&
            this.acceptProposedCompletion()) {
          aEvent.preventDefault();
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
          aEvent.preventDefault();
        }
        else if (this._inputChanged) {
          this.updateCompleteNode(l10n.getStr("Autocomplete.blank"));
          aEvent.preventDefault();
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
   * @param number aDirection
   *        History navigation direction: HISTORY_BACK or HISTORY_FORWARD.
   *
   * @returns boolean
   *          True if the input value changed, false otherwise.
   */
  historyPeruse: function JST_historyPeruse(aDirection)
  {
    if (!this.history.length) {
      return false;
    }

    // Up Arrow key
    if (aDirection == HISTORY_BACK) {
      if (this.historyPlaceHolder <= 0) {
        return false;
      }
      let inputVal = this.history[--this.historyPlaceHolder];

      // Save the current input value as the latest entry in history, only if
      // the user is already at the last entry.
      // Note: this code does not store changes to items that are already in
      // history.
      if (this.historyPlaceHolder+1 == this.historyIndex) {
        this.history[this.historyIndex] = this.inputNode.value || "";
      }

      this.setInputValue(inputVal);
    }
    // Down Arrow key
    else if (aDirection == HISTORY_FORWARD) {
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
    return /[\r\n]/.test(this.inputNode.value);
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
   * @param int aType possible values are
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
   *    - this.COMPLETE_HINT_ONLY: If there is more than one possible
   *          completion and the input value stayed the same compared to the
   *          last time this function was called, then the same completion is
   *          used again. If there is only one possible completion, then
   *          the inputNode.value is set to this value and the selection is set
   *          from the current cursor position to the end of the completed text.
   * @param function aCallback
   *        Optional function invoked when the autocomplete properties are
   *        updated.
   * @returns boolean true if there existed a completion for the current input,
   *          or false otherwise.
   */
  complete: function JSTF_complete(aType, aCallback)
  {
    let inputNode = this.inputNode;
    let inputValue = inputNode.value;
    // If the inputNode has no value, then don't try to complete on it.
    if (!inputValue) {
      this.clearCompletion();
      return false;
    }

    // Only complete if the selection is empty.
    if (inputNode.selectionStart != inputNode.selectionEnd) {
      this.clearCompletion();
      return false;
    }

    // Update the completion results.
    if (this.lastCompletion.value != inputValue) {
      this._updateCompletionResult(aType, aCallback);
      return false;
    }

    let popup = this.autocompletePopup;
    let accepted = false;

    if (aType != this.COMPLETE_HINT_ONLY && popup.itemCount == 1) {
      this.acceptProposedCompletion();
      accepted = true;
    }
    else if (aType == this.COMPLETE_BACKWARD) {
      popup.selectPreviousItem();
    }
    else if (aType == this.COMPLETE_FORWARD) {
      popup.selectNextItem();
    }

    aCallback && aCallback(this);
    return accepted || popup.itemCount > 0;
  },

  /**
   * Update the completion result. This operation is performed asynchronously by
   * fetching updated results from the content process.
   *
   * @private
   * @param int aType
   *        Completion type. See this.complete() for details.
   * @param function [aCallback]
   *        Optional, function to invoke when completion results are received.
   */
  _updateCompletionResult:
  function JST__updateCompletionResult(aType, aCallback)
  {
    if (this.lastCompletion.value == this.inputNode.value) {
      return;
    }

    let requestId = gSequenceId();
    let cursor = this.inputNode.selectionStart;
    let input = this.inputNode.value.substring(0, cursor);
    let cache = this._autocompleteCache;

    // If the current input starts with the previous input, then we already
    // have a list of suggestions and we just need to filter the cached
    // suggestions. When the current input ends with a non-alphanumeric
    // character we ask the server again for suggestions.

    // Check if last character is non-alphanumeric
    if (!/[a-zA-Z0-9]$/.test(input)) {
      this._autocompleteQuery = null;
      this._autocompleteCache = null;
    }

    if (this._autocompleteQuery && input.startsWith(this._autocompleteQuery)) {
      let filterBy = input;
      // Find the last non-alphanumeric if exists.
      let lastNonAlpha = input.match(/[^a-zA-Z0-9][a-zA-Z0-9]*$/);
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
        completionType: aType,
        value: null,
      };

      let response = { matches: newList, matchProp: filterBy };
      this._receiveAutocompleteProperties(null, aCallback, response);
      return;
    }

    this.lastCompletion = {
      requestId: requestId,
      completionType: aType,
      value: null,
    };

    let callback = this._receiveAutocompleteProperties.bind(this, requestId,
                                                            aCallback);
    this.webConsoleClient.autocomplete(input, cursor, callback);
  },

  /**
   * Handler for the autocompletion results. This method takes
   * the completion result received from the server and updates the UI
   * accordingly.
   *
   * @param number aRequestId
   *        Request ID.
   * @param function [aCallback=null]
   *        Optional, function to invoke when the completion result is received.
   * @param object aMessage
   *        The JSON message which holds the completion results received from
   *        the content process.
   */
  _receiveAutocompleteProperties:
  function JST__receiveAutocompleteProperties(aRequestId, aCallback, aMessage)
  {
    let inputNode = this.inputNode;
    let inputValue = inputNode.value;
    if (this.lastCompletion.value == inputValue ||
        aRequestId != this.lastCompletion.requestId) {
      return;
    }

    // Cache whatever came from the server if the last char is alphanumeric or '.'
    let cursor = inputNode.selectionStart;
    let inputUntilCursor = inputValue.substring(0, cursor);

    if (aRequestId != null && /[a-zA-Z0-9.]$/.test(inputUntilCursor)) {
      this._autocompleteCache = aMessage.matches;
      this._autocompleteQuery = inputUntilCursor;
    }

    let matches = aMessage.matches;
    let lastPart = aMessage.matchProp;
    if (!matches.length) {
      this.clearCompletion();
      return;
    }

    let items = matches.reverse().map(function(aMatch) {
      return { preLabel: lastPart, label: aMatch };
    });

    let popup = this.autocompletePopup;
    popup.setItems(items);

    let completionType = this.lastCompletion.completionType;
    this.lastCompletion = {
      value: inputValue,
      matchProp: lastPart,
    };

    if (items.length > 1 && !popup.isOpen) {
      popup.openPopup(inputNode);
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

    aCallback && aCallback(this);
  },

  onAutocompleteSelect: function JSTF_onAutocompleteSelect()
  {
    // Render the suggestion only if the cursor is at the end of the input.
    if (this.inputNode.selectionStart != this.inputNode.value.length) {
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
      let value = this.inputNode.value;
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
   * @param string aSuffix
   *        The proposed suffix for the inputNode value.
   */
  updateCompleteNode: function JSTF_updateCompleteNode(aSuffix)
  {
    // completion prefix = input, with non-control chars replaced by spaces
    let prefix = aSuffix ? this.inputNode.value.replace(/[\S]/g, " ") : "";
    this.completeNode.value = prefix + aSuffix;
  },

  /**
   * The click event handler for evaluation results in the output.
   *
   * @private
   * @param object aResponse
   *        The JavaScript evaluation response received from the server.
   */
  _evalOutputClick: function JST__evalOutputClick(aResponse)
  {
    this.openVariablesView({
      label: VariablesView.getString(aResponse.result),
      objectActor: aResponse.result,
      autofocus: true,
    });
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

    this.inputNode.removeEventListener("keypress", this._keyPress, false);
    this.inputNode.removeEventListener("input", this._inputEventHandler, false);
    this.inputNode.removeEventListener("keyup", this._inputEventHandler, false);
    this.inputNode.removeEventListener("focus", this._focusEventHandler, false);

    this.hud = null;
  },
};

/**
 * Utils: a collection of globally used functions.
 */
var Utils = {
  /**
   * Flag to turn on and off scrolling.
   */
  scroll: true,

  /**
   * Scrolls a node so that it's visible in its containing XUL "scrollbox"
   * element.
   *
   * @param nsIDOMNode aNode
   *        The node to make visible.
   * @returns void
   */
  scrollToVisible: function Utils_scrollToVisible(aNode)
  {
    if (!this.scroll) {
      return;
    }

    // Find the enclosing richlistbox node.
    let richListBoxNode = aNode.parentNode;
    while (richListBoxNode.tagName != "richlistbox") {
      richListBoxNode = richListBoxNode.parentNode;
    }

    // Use the scroll box object interface to ensure the element is visible.
    let boxObject = richListBoxNode.scrollBoxObject;
    let nsIScrollBoxObject = boxObject.QueryInterface(Ci.nsIScrollBoxObject);
    nsIScrollBoxObject.ensureElementIsVisible(aNode);
  },

  /**
   * Check if the given output node is scrolled to the bottom.
   *
   * @param nsIDOMNode aOutputNode
   * @return boolean
   *         True if the output node is scrolled to the bottom, or false
   *         otherwise.
   */
  isOutputScrolledToBottom: function Utils_isOutputScrolledToBottom(aOutputNode)
  {
    let lastNodeHeight = aOutputNode.lastChild ?
                         aOutputNode.lastChild.clientHeight : 0;
    let scrollBox = aOutputNode.scrollBoxObject.element;

    return scrollBox.scrollTop + scrollBox.clientHeight >=
           scrollBox.scrollHeight - lastNodeHeight / 2;
  },

  /**
   * Determine the category of a given nsIScriptError.
   *
   * @param nsIScriptError aScriptError
   *        The script error you want to determine the category for.
   * @return CATEGORY_JS|CATEGORY_CSS|CATEGORY_SECURITY
   *         Depending on the script error CATEGORY_JS, CATEGORY_CSS, or
   *         CATEGORY_SECURITY can be returned.
   */
  categoryForScriptError: function Utils_categoryForScriptError(aScriptError)
  {
    switch (aScriptError.category) {
      case "CSS Parser":
      case "CSS Loader":
        return CATEGORY_CSS;

      case "Mixed Content Blocker":
      case "CSP":
      case "Invalid HSTS Headers":
        return CATEGORY_SECURITY;

      default:
        return CATEGORY_JS;
    }
  },

  /**
   * Retrieve the limit of messages for a specific category.
   *
   * @param number aCategory
   *        The category of messages you want to retrieve the limit for. See the
   *        CATEGORY_* constants.
   * @return number
   *         The number of messages allowed for the specific category.
   */
  logLimitForCategory: function Utils_logLimitForCategory(aCategory)
  {
    let logLimit = DEFAULT_LOG_LIMIT;

    try {
      let prefName = CATEGORY_CLASS_FRAGMENTS[aCategory];
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
function CommandController(aWebConsole)
{
  this.owner = aWebConsole;
}

CommandController.prototype = {
  /**
   * Copies the currently-selected entries in the Web Console output to the
   * clipboard.
   */
  copy: function CommandController_copy()
  {
    this.owner.copySelectedItems();
  },

  /**
   * Selects all the text in the HUD output.
   */
  selectAll: function CommandController_selectAll()
  {
    this.owner.outputNode.selectAll();
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
    this.owner.copySelectedItems({ linkOnly: true });
  },

  supportsCommand: function CommandController_supportsCommand(aCommand)
  {
    return this.isCommandEnabled(aCommand);
  },

  isCommandEnabled: function CommandController_isCommandEnabled(aCommand)
  {
    switch (aCommand) {
      case "cmd_copy":
        // Only enable "copy" if nodes are selected.
        return this.owner.outputNode.selectedCount > 0;
      case "consoleCmd_openURL":
      case "consoleCmd_copyURL": {
        // Only enable URL-related actions if node is Net Activity.
        let selectedItem = this.owner.outputNode.selectedItem;
        return selectedItem && "url" in selectedItem;
      }
      case "consoleCmd_clearOutput":
      case "cmd_fontSizeEnlarge":
      case "cmd_fontSizeReduce":
      case "cmd_fontSizeReset":
      case "cmd_selectAll":
      case "cmd_find":
        return true;
      case "cmd_close":
        return this.owner.owner._browserConsole;
    }
    return false;
  },

  doCommand: function CommandController_doCommand(aCommand)
  {
    switch (aCommand) {
      case "cmd_copy":
        this.copy();
        break;
      case "consoleCmd_openURL":
        this.openURL();
        break;
      case "consoleCmd_copyURL":
        this.copyURL();
        break;
      case "consoleCmd_clearOutput":
        this.owner.jsterm.clearOutput(true);
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
 * @param object aWebConsole
 *        The Web Console instance that owns this connection proxy.
 * @param RemoteTarget aTarget
 *        The target that the console will connect to.
 */
function WebConsoleConnectionProxy(aWebConsole, aTarget)
{
  this.owner = aWebConsole;
  this.target = aTarget;

  this._onPageError = this._onPageError.bind(this);
  this._onLogMessage = this._onLogMessage.bind(this);
  this._onConsoleAPICall = this._onConsoleAPICall.bind(this);
  this._onNetworkEvent = this._onNetworkEvent.bind(this);
  this._onNetworkEventUpdate = this._onNetworkEventUpdate.bind(this);
  this._onFileActivity = this._onFileActivity.bind(this);
  this._onTabNavigated = this._onTabNavigated.bind(this);
  this._onAttachConsole = this._onAttachConsole.bind(this);
  this._onCachedMessages = this._onCachedMessages.bind(this);
  this._connectionTimeout = this._connectionTimeout.bind(this);
  this._onLastPrivateContextExited = this._onLastPrivateContextExited.bind(this);
}

WebConsoleConnectionProxy.prototype = {
  /**
   * The owning Web Console instance.
   *
   * @see WebConsoleFrame
   * @type object
   */
  owner: null,

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
    connPromise.then(function _onSucess() {
      this._connectTimer.cancel();
      this._connectTimer = null;
    }.bind(this), function _onFailure() {
      this._connectTimer = null;
    }.bind(this));

    let client = this.client = this.target.client;

    client.addListener("logMessage", this._onLogMessage);
    client.addListener("pageError", this._onPageError);
    client.addListener("consoleAPICall", this._onConsoleAPICall);
    client.addListener("networkEvent", this._onNetworkEvent);
    client.addListener("networkEventUpdate", this._onNetworkEventUpdate);
    client.addListener("fileActivity", this._onFileActivity);
    client.addListener("lastPrivateContextExited", this._onLastPrivateContextExited);
    this.target.on("will-navigate", this._onTabNavigated);
    this.target.on("navigate", this._onTabNavigated);

    this._consoleActor = this.target.form.consoleActor;
    if (!this.target.chrome) {
      let tab = this.target.form;
      this.owner.onLocationChange(tab.url, tab.title);
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
   * @param object aResponse
   *        The JSON response object received from the server.
   * @param object aWebConsoleClient
   *        The WebConsoleClient instance for the attached console, for the
   *        specific tab we work with.
   */
  _onAttachConsole: function WCCP__onAttachConsole(aResponse, aWebConsoleClient)
  {
    if (aResponse.error) {
      Cu.reportError("attachConsole failed: " + aResponse.error + " " +
                     aResponse.message);
      this._connectDefer.reject(aResponse);
      return;
    }

    this.webConsoleClient = aWebConsoleClient;

    this._hasNativeConsoleAPI = aResponse.nativeConsoleAPI;

    let msgs = ["PageError", "ConsoleAPI"];
    this.webConsoleClient.getCachedMessages(msgs, this._onCachedMessages);
  },

  /**
   * The "cachedMessages" response handler.
   *
   * @private
   * @param object aResponse
   *        The JSON response object received from the server.
   */
  _onCachedMessages: function WCCP__onCachedMessages(aResponse)
  {
    if (aResponse.error) {
      Cu.reportError("Web Console getCachedMessages error: " + aResponse.error +
                     " " + aResponse.message);
      this._connectDefer.reject(aResponse);
      return;
    }

    if (!this._connectTimer) {
      // This happens if the promise is rejected (eg. a timeout), but the
      // connection attempt is successful, nonetheless.
      Cu.reportError("Web Console getCachedMessages error: invalid state.");
    }

    this.owner.displayCachedMessages(aResponse.messages);

    if (!this._hasNativeConsoleAPI) {
      this.owner.logWarningAboutReplacedAPI();
    }

    this.connected = true;
    this._connectDefer.resolve(this);
  },

  /**
   * The "pageError" message type handler. We redirect any page errors to the UI
   * for displaying.
   *
   * @private
   * @param string aType
   *        Message type.
   * @param object aPacket
   *        The message received from the server.
   */
  _onPageError: function WCCP__onPageError(aType, aPacket)
  {
    if (this.owner && aPacket.from == this._consoleActor) {
      this.owner.handlePageError(aPacket.pageError);
    }
  },

  /**
   * The "logMessage" message type handler. We redirect any message to the UI
   * for displaying.
   *
   * @private
   * @param string aType
   *        Message type.
   * @param object aPacket
   *        The message received from the server.
   */
  _onLogMessage: function WCCP__onLogMessage(aType, aPacket)
  {
    if (this.owner && aPacket.from == this._consoleActor) {
      this.owner.handleLogMessage(aPacket);
    }
  },

  /**
   * The "consoleAPICall" message type handler. We redirect any message to
   * the UI for displaying.
   *
   * @private
   * @param string aType
   *        Message type.
   * @param object aPacket
   *        The message received from the server.
   */
  _onConsoleAPICall: function WCCP__onConsoleAPICall(aType, aPacket)
  {
    if (this.owner && aPacket.from == this._consoleActor) {
      this.owner.handleConsoleAPICall(aPacket.message);
    }
  },

  /**
   * The "networkEvent" message type handler. We redirect any message to
   * the UI for displaying.
   *
   * @private
   * @param string aType
   *        Message type.
   * @param object aPacket
   *        The message received from the server.
   */
  _onNetworkEvent: function WCCP__onNetworkEvent(aType, aPacket)
  {
    if (this.owner && aPacket.from == this._consoleActor) {
      this.owner.handleNetworkEvent(aPacket.eventActor);
    }
  },

  /**
   * The "networkEventUpdate" message type handler. We redirect any message to
   * the UI for displaying.
   *
   * @private
   * @param string aType
   *        Message type.
   * @param object aPacket
   *        The message received from the server.
   */
  _onNetworkEventUpdate: function WCCP__onNetworkEvenUpdatet(aType, aPacket)
  {
    if (this.owner) {
      this.owner.handleNetworkEventUpdate(aPacket.from, aPacket.updateType,
                                          aPacket);
    }
  },

  /**
   * The "fileActivity" message type handler. We redirect any message to
   * the UI for displaying.
   *
   * @private
   * @param string aType
   *        Message type.
   * @param object aPacket
   *        The message received from the server.
   */
  _onFileActivity: function WCCP__onFileActivity(aType, aPacket)
  {
    if (this.owner && aPacket.from == this._consoleActor) {
      this.owner.handleFileActivity(aPacket.uri);
    }
  },

  /**
   * The "lastPrivateContextExited" message type handler. When this message is
   * received the Web Console UI is cleared.
   *
   * @private
   * @param string aType
   *        Message type.
   * @param object aPacket
   *        The message received from the server.
   */
  _onLastPrivateContextExited:
  function WCCP__onLastPrivateContextExited(aType, aPacket)
  {
    if (this.owner && aPacket.from == this._consoleActor) {
      this.owner.jsterm.clearPrivateMessages();
    }
  },

  /**
   * The "will-navigate" and "navigate" event handlers. We redirect any message
   * to the UI for displaying.
   *
   * @private
   * @param string aEvent
   *        Event type.
   * @param object aPacket
   *        The message received from the server.
   */
  _onTabNavigated: function WCCP__onTabNavigated(aEvent, aPacket)
  {
    if (!this.owner) {
      return;
    }

    if (aEvent == "will-navigate" && !this.owner.persistLog) {
      this.owner.jsterm.clearOutput();
    }

    if (aPacket.url) {
      this.owner.onLocationChange(aPacket.url, aPacket.title);
    }

    if (aEvent == "navigate" && !aPacket.nativeConsoleAPI) {
      this.owner.logWarningAboutReplacedAPI();
    }
  },

  /**
   * Release an object actor.
   *
   * @param string aActor
   *        The actor ID to send the request to.
   */
  releaseActor: function WCCP_releaseActor(aActor)
  {
    if (this.client) {
      this.client.release(aActor);
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
    this.client.removeListener("networkEvent", this._onNetworkEvent);
    this.client.removeListener("networkEventUpdate", this._onNetworkEventUpdate);
    this.client.removeListener("fileActivity", this._onFileActivity);
    this.client.removeListener("lastPrivateContextExited", this._onLastPrivateContextExited);
    this.target.off("will-navigate", this._onTabNavigated);
    this.target.off("navigate", this._onTabNavigated);

    this.client = null;
    this.webConsoleClient = null;
    this.target = null;
    this.connected = false;
    this.owner = null;
    this._disconnecter.resolve(null);

    return this._disconnecter.promise;
  },
};

function gSequenceId()
{
  return gSequenceId.n++;
}
gSequenceId.n = 0;


function goUpdateConsoleCommands() {
  goUpdateCommand("consoleCmd_openURL");
  goUpdateCommand("consoleCmd_copyURL");
}



///////////////////////////////////////////////////////////////////////////////
// Context Menu
///////////////////////////////////////////////////////////////////////////////

const CONTEXTMENU_ID = "output-contextmenu";

/*
 * ConsoleContextMenu: This handle to show/hide a context menu item.
 */
let ConsoleContextMenu = {
  /*
   * Handle to show/hide context menu item.
   *
   * @param nsIDOMEvent aEvent
   */
  build: function CCM_build(aEvent)
  {
    let popup = aEvent.target;
    if (popup.id !== CONTEXTMENU_ID) {
      return;
    }

    let view = document.querySelector(".hud-output-node");
    let metadata = this.getSelectionMetadata(view);

    for (let i = 0, l = popup.childNodes.length; i < l; ++i) {
      let element = popup.childNodes[i];
      element.hidden = this.shouldHideMenuItem(element, metadata);
    }
  },

  /*
   * Get selection information from the view.
   *
   * @param nsIDOMElement aView
   *        This should be <xul:richlistbox>.
   *
   * @return object
   *         Selection metadata.
   */
  getSelectionMetadata: function CCM_getSelectionMetadata(aView)
  {
    let metadata = {
      selectionType: "",
      selection: new Set(),
    };
    let selectedItems = aView.selectedItems;

    metadata.selectionType = (selectedItems > 1) ? "multiple" : "single";

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
      }
    }

    return metadata;
  },

  /*
   * Determine if an item should be hidden.
   *
   * @param nsIDOMElement aMenuItem
   * @param object aMetadata
   * @return boolean
   *         Whether the given item should be hidden or not.
   */
  shouldHideMenuItem: function CCM_shouldHideMenuItem(aMenuItem, aMetadata)
  {
    let selectionType = aMenuItem.getAttribute("selectiontype");
    if (selectionType && !aMetadata.selectionType == selectionType) {
      return true;
    }

    let selection = aMenuItem.getAttribute("selection");
    if (!selection) {
      return false;
    }

    let shouldHide = true;
    let itemData = selection.split("|");
    for (let type of aMetadata.selection) {
      // check whether this menu item should show or not.
      if (itemData.indexOf(type) !== -1) {
        shouldHide = false;
        break;
      }
    }

    return shouldHide;
  },
};
