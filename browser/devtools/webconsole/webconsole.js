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

XPCOMUtils.defineLazyModuleGetter(this, "DebuggerServer",
                                  "resource://gre/modules/devtools/dbg-server.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DebuggerClient",
                                  "resource://gre/modules/devtools/dbg-client.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "debuggerSocketConnect",
                                  "resource://gre/modules/devtools/dbg-client.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "clipboardHelper",
                                   "@mozilla.org/widget/clipboardhelper;1",
                                   "nsIClipboardHelper");

XPCOMUtils.defineLazyModuleGetter(this, "PropertyPanel",
                                  "resource:///modules/PropertyPanel.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PropertyTreeView",
                                  "resource:///modules/PropertyPanel.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetworkPanel",
                                  "resource:///modules/NetworkPanel.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AutocompletePopup",
                                  "resource:///modules/AutocompletePopup.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "WebConsoleUtils",
                                  "resource://gre/modules/devtools/WebConsoleUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/commonjs/promise/core.js");

const STRINGS_URI = "chrome://browser/locale/devtools/webconsole.properties";
let l10n = new WebConsoleUtils.l10n(STRINGS_URI);


// The XUL namespace.
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

const MIXED_CONTENT_LEARN_MORE = "https://developer.mozilla.org/en/Security/MixedContent";

const HELP_URL = "https://developer.mozilla.org/docs/Tools/Web_Console/Helpers";

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
  [ "network",    null,         null,   "networkinfo", ],  // Network
  [ "csserror",   "cssparser",  null,   null,          ],  // CSS
  [ "exception",  "jswarn",     null,   null,          ],  // JS
  [ "error",      "warn",       "info", "log",         ],  // Web Developer
  [ null,         null,         null,   null,          ],  // Input
  [ null,         null,         null,   null,          ],  // Output
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

const PREF_CONNECTION_TIMEOUT = "devtools.debugger.remote-timeout";

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

  this._cssNodes = {};
  this._outputQueue = [];
  this._pruneCategoriesQueue = {};
  this._networkRequests = {};

  this._toggleFilter = this._toggleFilter.bind(this);
  this._flushMessageQueue = this._flushMessageQueue.bind(this);

  this._outputTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  this._outputTimerInitialized = false;
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
   * Store for tracking repeated CSS nodes.
   * @private
   * @type object
   */
  _cssNodes: null,

  /**
   * Preferences for filtering messages by type.
   * @see this._initDefaultFilterPrefs()
   * @type object
   */
  filterPrefs: null,

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

  _saveRequestAndResponseBodies: false,

  /**
   * Tells whether to save the bodies of network requests and responses.
   * Disabled by default to save memory.
   * @type boolean
   */
  get saveRequestAndResponseBodies() this._saveRequestAndResponseBodies,

  /**
   * Setter for saving of network request and response bodies.
   *
   * @param boolean aValue
   *        The new value you want to set.
   */
  set saveRequestAndResponseBodies(aValue) {
    let newValue = !!aValue;
    let preferences = {
      "NetworkMonitor.saveRequestAndResponseBodies": newValue,
    };

    this.webConsoleClient.setPreferences(preferences, function(aResponse) {
      if (!aResponse.error) {
        this._saveRequestAndResponseBodies = newValue;
      }
    }.bind(this));
  },

  /**
   * Initialize the WebConsoleFrame instance.
   * @return object
   *         A Promise object for the initialization.
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
   *         A Promise object that is resolved/reject based on the connection
   *         result.
   */
  _initConnection: function WCF__initConnection()
  {
    let deferred = Promise.defer();

    this.proxy = new WebConsoleConnectionProxy(this, this.owner.target);

    let onSuccess = function() {
      this.saveRequestAndResponseBodies = this._saveRequestAndResponseBodies;
      deferred.resolve(this);
    }.bind(this);

    let onFailure = function(aReason) {
      let node = this.createMessageNode(CATEGORY_JS, SEVERITY_ERROR,
                                        aReason.error + ": " + aReason.message);
      this.outputMessage(CATEGORY_JS, node);
      deferred.reject(aReason);
    }.bind(this);

    let sendNotification = function() {
      let id = WebConsoleUtils.supportsString(this.hudId);
      Services.obs.notifyObservers(id, "web-console-created", null);
    }.bind(this);

    this.proxy.connect().then(onSuccess, onFailure).then(sendNotification);

    return deferred.promise;
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

    let saveBodies = doc.getElementById("saveBodies");
    saveBodies.addEventListener("command", function() {
      this.saveRequestAndResponseBodies = !this.saveRequestAndResponseBodies;
    }.bind(this));
    saveBodies.setAttribute("checked", this.saveRequestAndResponseBodies);
    saveBodies.disabled = !this.getFilterState("networkinfo") &&
                          !this.getFilterState("network");

    saveBodies.parentNode.addEventListener("popupshowing", function() {
      saveBodies.setAttribute("checked", this.saveRequestAndResponseBodies);
      saveBodies.disabled = !this.getFilterState("networkinfo") &&
                            !this.getFilterState("network");
    }.bind(this));

    // Remove this part when context menu entry is removed.
    let saveBodiesContextMenu = doc.getElementById("saveBodiesContextMenu");
    saveBodiesContextMenu.addEventListener("command", function() {
      this.saveRequestAndResponseBodies = !this.saveRequestAndResponseBodies;
    }.bind(this));
    saveBodiesContextMenu.setAttribute("checked",
                                       this.saveRequestAndResponseBodies);
    saveBodiesContextMenu.disabled = !this.getFilterState("networkinfo") &&
                                     !this.getFilterState("network");

    saveBodiesContextMenu.parentNode.addEventListener("popupshowing", function() {
      saveBodiesContextMenu.setAttribute("checked",
                                         this.saveRequestAndResponseBodies);
      saveBodiesContextMenu.disabled = !this.getFilterState("networkinfo") &&
                                       !this.getFilterState("network");
    }.bind(this));

    let clearButton = doc.getElementsByClassName("webconsole-clear-console-button")[0];
    clearButton.addEventListener("command", function WCF__onClearButton() {
      this.owner._onClearButton();
      this.jsterm.clearOutput(true);
    }.bind(this));

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
    this.filterPrefs = {
      network: Services.prefs.getBoolPref(FILTER_PREFS_PREFIX + "network"),
      networkinfo: Services.prefs.getBoolPref(FILTER_PREFS_PREFIX + "networkinfo"),
      csserror: Services.prefs.getBoolPref(FILTER_PREFS_PREFIX + "csserror"),
      cssparser: Services.prefs.getBoolPref(FILTER_PREFS_PREFIX + "cssparser"),
      exception: Services.prefs.getBoolPref(FILTER_PREFS_PREFIX + "exception"),
      jswarn: Services.prefs.getBoolPref(FILTER_PREFS_PREFIX + "jswarn"),
      error: Services.prefs.getBoolPref(FILTER_PREFS_PREFIX + "error"),
      info: Services.prefs.getBoolPref(FILTER_PREFS_PREFIX + "info"),
      warn: Services.prefs.getBoolPref(FILTER_PREFS_PREFIX + "warn"),
      log: Services.prefs.getBoolPref(FILTER_PREFS_PREFIX + "log"),
    };
  },

  /**
   * Sets the click events for all binary toggle filter buttons.
   * @private
   */
  _setFilterTextBoxEvents: function WCF__setFilterTextBoxEvents()
  {
    let timer = null;
    let timerEvent = this.adjustVisibilityOnSearchStringChange.bind(this);

    let onChange = function _onChange() {
      let timer;

      // To improve responsiveness, we let the user finish typing before we
      // perform the search.
      if (timer == null) {
        let timerClass = Cc["@mozilla.org/timer;1"];
        timer = timerClass.createInstance(Ci.nsITimer);
      }
      else {
        timer.cancel();
      }

      timer.initWithCallback(timerEvent, SEARCH_DELAY,
                             Ci.nsITimer.TYPE_ONE_SHOT);
    }.bind(this);

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
    Services.prefs.setBoolPref(FILTER_PREFS_PREFIX + aToggleType, aState);
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
   * Filter the css node from the output node if it is a repeat. CSS messages
   * are merged with previous messages if they occurred in the past.
   *
   * @param nsIDOMNode aNode
   *        The message node to be filtered or not.
   * @returns boolean
   *          True if the message is filtered, false otherwise.
   */
  filterRepeatedCSS: function WCF_filterRepeatedCSS(aNode)
  {
    // childNodes[2] is the description node containing the text of the message.
    let description = aNode.childNodes[2].textContent;
    let location;

    // childNodes[4] represents the location (source URL) of the error message.
    // The full source URL is stored in the title attribute.
    if (aNode.childNodes[4]) {
      // browser_webconsole_bug_595934_message_categories.js
      location = aNode.childNodes[4].getAttribute("title");
    }
    else {
      location = "";
    }

    let dupe = this._cssNodes[description + location];
    if (!dupe) {
      // no matching nodes
      this._cssNodes[description + location] = aNode;
      return false;
    }

    this.mergeFilteredMessageNode(dupe, aNode);

    return true;
  },

  /**
   * Filter the console node from the output node if it is a repeat. Console
   * messages are filtered from the output if they match the immediately
   * preceding message that came from the same source. The output node's
   * last occurrence should have its timestamp updated.
   *
   * @param nsIDOMNode aNode
   *        The message node to be filtered or not.
   * @return boolean
   *         True if the message is filtered, false otherwise.
   */
  filterRepeatedConsole: function WCF_filterRepeatedConsole(aNode)
  {
    let lastMessage = this.outputNode.lastChild;

    if (!lastMessage) {
      return false;
    }

    let body = aNode.querySelector(".webconsole-msg-body");
    let lastBody = lastMessage.querySelector(".webconsole-msg-body");

    if (aNode.classList.contains("webconsole-msg-inspector")) {
      return false;
    }

    if (!body || !lastBody) {
      return false;
    }

    if (body.textContent == lastBody.textContent) {
      let loc = aNode.querySelector(".webconsole-location");
      let lastLoc = lastMessage.querySelector(".webconsole-location");

      if (loc && lastLoc) {
        if (loc.getAttribute("value") !== lastLoc.getAttribute("value")) {
          return false;
        }
      }

      this.mergeFilteredMessageNode(lastMessage, aNode);
      return true;
    }

    return false;
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
   * @return nsIDOMElement|undefined
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
    let objectActors = [];

    // Gather the actor IDs.
    args.forEach(function(aValue) {
      if (aValue && typeof aValue == "object" && aValue.actor) {
        objectActors.push(aValue.actor);
        let displayStringIsLong = typeof aValue.displayString == "object" &&
                                  aValue.displayString.type == "longString";
        if (displayStringIsLong) {
          objectActors.push(aValue.displayString.actor);
        }
      }
    }, this);

    switch (level) {
      case "log":
      case "info":
      case "warn":
      case "error":
      case "debug":
      case "dir":
      case "groupEnd": {
        body = { arguments: args };
        let clipboardArray = [];
        args.forEach(function(aValue) {
          clipboardArray.push(WebConsoleUtils.objectActorGripToString(aValue));
          if (aValue && typeof aValue == "object" && aValue.actor) {
            let displayStringIsLong = typeof aValue.displayString == "object" &&
                                      aValue.displayString.type == "longString";
            if (aValue.type == "longString" || displayStringIsLong) {
              clipboardArray.push(l10n.getStr("longStringEllipsis"));
            }
          }
        }, this);
        clipboardText = clipboardArray.join(" ");

        if (level == "dir") {
          body.objectProperties = aMessage.objectProperties;
        }
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

      case "time": {
        let timer = aMessage.timer;
        if (!timer) {
          return;
        }
        if (timer.error) {
          Cu.reportError(l10n.getStr(timer.error));
          return;
        }
        body = l10n.getFormatStr("timerStarted", [timer.name]);
        clipboardText = body;
        break;
      }

      case "timeEnd": {
        let timer = aMessage.timer;
        if (!timer) {
          return;
        }
        body = l10n.getFormatStr("timeEnd", [timer.name, timer.duration]);
        clipboardText = body;
        break;
      }

      default:
        Cu.reportError("Unknown Console API log level: " + level);
        return;
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
        objectActors.forEach(this._releaseObject, this);
        objectActors = [];
    }

    if (level == "groupEnd") {
      if (this.groupDepth > 0) {
        this.groupDepth--;
      }
      return; // no need to continue
    }

    let node = this.createMessageNode(CATEGORY_WEBDEV, LEVELS[level], body,
                                      sourceURL, sourceLine, clipboardText,
                                      level, aMessage.timeStamp);

    if (objectActors.length) {
      node._objectActors = objectActors;
    }

    // Make the node bring up the property panel, to allow the user to inspect
    // the stack trace.
    if (level == "trace") {
      node._stacktrace = aMessage.stacktrace;

      this.makeOutputMessageLink(node, function _traceNodeClickCallback() {
        if (node._panelOpen) {
          return;
        }

        let options = {
          anchor: node,
          data: { object: node._stacktrace },
        };

        let propPanel = this.jsterm.openPropertyPanel(options);
        propPanel.panel.setAttribute("hudId", this.hudId);
      }.bind(this));
    }

    if (level == "dir") {
      // Initialize the inspector message node, by setting the PropertyTreeView
      // object on the tree view. This has to be done *after* the node is
      // shown, because the tree binding must be attached first.
      node._onOutput = function _onMessageOutput() {
        node.querySelector("tree").view = node.propertyTreeView;
      };
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
   * @param nsIDOMNode aMessage
   *        The message element this handler corresponds to.
   * @param nsIDOMNode aAnchor
   *        The object inspector anchor element. This is the clickable element
   *        in the console.log message we display.
   * @param object aObjectActor
   *        The object actor grip.
   */
  _consoleLogClick:
  function WCF__consoleLogClick(aMessage, aAnchor, aObjectActor)
  {
    if (aAnchor._panelOpen) {
      return;
    }

    let options = {
      title: aAnchor.textContent,
      anchor: aAnchor,

      // Data to inspect.
      data: {
        objectPropertiesProvider: this.objectPropertiesProvider.bind(this),
        releaseObject: this._releaseObject.bind(this),
      },
    };

    let propPanel;
    let onPopupHide = function _onPopupHide() {
      propPanel.panel.removeEventListener("popuphiding", onPopupHide, false);

      if (!aMessage.parentNode && aMessage._objectActors) {
        aMessage._objectActors.forEach(this._releaseObject, this);
        aMessage._objectActors = null;
      }
    }.bind(this);

    this.objectPropertiesProvider(aObjectActor.actor,
      function _onObjectProperties(aProperties) {
        options.data.objectProperties = aProperties;
        propPanel = this.jsterm.openPropertyPanel(options);
        propPanel.panel.setAttribute("hudId", this.hudId);
        propPanel.panel.addEventListener("popuphiding", onPopupHide, false);
      }.bind(this));
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

    let node = this.createMessageNode(aCategory, severity,
                                      aScriptError.errorMessage,
                                      aScriptError.sourceName,
                                      aScriptError.lineNumber, null, null,
                                      aScriptError.timeStamp);
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
   * Log network event.
   *
   * @param object aActorId
   *        The network event actor ID to log.
   * @return nsIDOMElement|undefined
   *         The message element to display in the Web Console output.
   */
  logNetEvent: function WCF_logNetEvent(aActorId)
  {
    let networkInfo = this._networkRequests[aActorId];
    if (!networkInfo) {
      return;
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
   * Inform user that the Web Console API has been replaced by a script
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
    for (let item of batch) {
      let node = this._outputMessageFromQueue(hudIdSupportsString, item);
      if (node) {
        lastVisibleNode = node;
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
   * @return nsIDOMElement|undefined
   *         The DOM element of the message if the message is visible, undefined
   *         otherwise.
   */
  _outputMessageFromQueue:
  function WCF__outputMessageFromQueue(aHudIdSupportsString, aItem)
  {
    let [category, methodOrNode, args] = aItem;

    let node = typeof methodOrNode == "function" ?
               methodOrNode.apply(this, args || []) :
               methodOrNode;
    if (!node) {
      return;
    }

    let afterNode = node._outputAfterNode;
    if (afterNode) {
      delete node._outputAfterNode;
    }

    let isFiltered = this.filterMessageNode(node);

    let isRepeated = false;
    if (node.classList.contains("webconsole-msg-cssparser")) {
      isRepeated = this.filterRepeatedCSS(node);
    }

    if (!isRepeated &&
        !node.classList.contains("webconsole-msg-network") &&
        (node.classList.contains("webconsole-msg-console") ||
         node.classList.contains("webconsole-msg-exception") ||
         node.classList.contains("webconsole-msg-error"))) {
      isRepeated = this.filterRepeatedConsole(node);
    }

    let lastVisible = !isRepeated && !isFiltered;
    if (!isRepeated) {
      this.outputNode.insertBefore(node,
                                   afterNode ? afterNode.nextSibling : null);
      this._pruneCategoriesQueue[node.category] = true;
      if (afterNode) {
        lastVisible = this.outputNode.lastChild == node;
      }
    }

    if (node._onOutput) {
      node._onOutput();
      delete node._onOutput;
    }

    let nodeID = node.getAttribute("id");
    Services.obs.notifyObservers(aHudIdSupportsString,
                                 "web-console-message-created", nodeID);

    return lastVisible ? node : null;
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
    if (typeof methodOrNode != "function" &&
        methodOrNode._objectActors && !methodOrNode._panelOpen) {
      methodOrNode._objectActors.forEach(this._releaseObject, this);
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
      let level = args[0].level;
      let releaseObject = function _releaseObject(aValue) {
        if (aValue && typeof aValue == "object" && aValue.actor) {
          this._releaseObject(aValue.actor);
        }
      }.bind(this);
      switch (level) {
        case "log":
        case "info":
        case "warn":
        case "error":
        case "debug":
        case "dir":
        case "groupEnd": {
          args[0].arguments.forEach(releaseObject);
          if (level == "dir") {
            args[0].objectProperties.forEach(function(aObject) {
              ["value", "get", "set"].forEach(function(aProp) {
                releaseObject(aObject[aProp]);
              });
            });
          }
        }
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
   * Destroy the property inspector message node. This performs the necessary
   * cleanup for the tree widget and removes it from the DOM.
   *
   * @param nsIDOMNode aMessageNode
   *        The message node that contains the property inspector from a
   *        console.dir call.
   */
  pruneConsoleDirNode: function WCF_pruneConsoleDirNode(aMessageNode)
  {
    if (aMessageNode.parentNode) {
      aMessageNode.parentNode.removeChild(aMessageNode);
    }

    let tree = aMessageNode.querySelector("tree");
    tree.parentNode.removeChild(tree);
    aMessageNode.propertyTreeView.data = null;
    aMessageNode.propertyTreeView = null;
    tree.view = null;
  },

  /**
   * Remove a given message from the output.
   *
   * @param nsIDOMNode aNode
   *        The message node you want to remove.
   */
  removeOutputMessage: function WCF_removeOutputMessage(aNode)
  {
    if (aNode._objectActors && !aNode._panelOpen) {
      aNode._objectActors.forEach(this._releaseObject, this);
    }

    if (aNode.classList.contains("webconsole-msg-cssparser")) {
      let desc = aNode.childNodes[2].textContent;
      let location = "";
      if (aNode.childNodes[4]) {
        location = aNode.childNodes[4].getAttribute("title");
      }
      delete this._cssNodes[desc + location];
    }
    else if (aNode._connectionId &&
             aNode.classList.contains("webconsole-msg-network")) {
      delete this._networkRequests[aNode._connectionId];
      this._releaseObject(aNode._connectionId);
    }
    else if (aNode.classList.contains("webconsole-msg-inspector")) {
      this.pruneConsoleDirNode(aNode);
      return;
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

    // Store the body text, since it is needed later for the property tree
    // case.
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
        str = WebConsoleUtils.objectActorGripToString(aBody.arguments[0]);
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
    if (aSourceURL) {
      locationNode = this.createLocationNode(aSourceURL, aSourceLine);
    }

    node.clipboardText = aClipboardText;
    node.classList.add("hud-msg-node");

    node.timestamp = timestamp;
    this.setMessageType(node, aCategory, aSeverity);

    node.appendChild(timestampNode);
    node.appendChild(iconContainer);
    // Display the object tree after the message node.
    if (aLevel == "dir") {
      // Make the body container, which is a vertical box, for grouping the text
      // and tree widgets.
      let bodyContainer = this.document.createElement("vbox");
      bodyContainer.flex = 1;
      bodyContainer.appendChild(bodyNode);
      // Create the tree.
      let tree = this.document.createElement("tree");
      tree.setAttribute("hidecolumnpicker", "true");
      tree.flex = 1;

      let treecols = this.document.createElement("treecols");
      let treecol = this.document.createElement("treecol");
      treecol.setAttribute("primary", "true");
      treecol.setAttribute("hideheader", "true");
      treecol.setAttribute("ignoreincolumnpicker", "true");
      treecol.flex = 1;
      treecols.appendChild(treecol);
      tree.appendChild(treecols);

      tree.appendChild(this.document.createElement("treechildren"));

      bodyContainer.appendChild(tree);
      node.appendChild(bodyContainer);
      node.classList.add("webconsole-msg-inspector");
      // Create the treeView object.
      let treeView = node.propertyTreeView = new PropertyTreeView();

      treeView.data = {
        objectPropertiesProvider: this.objectPropertiesProvider.bind(this),
        releaseObject: this._releaseObject.bind(this),
        objectProperties: body.objectProperties,
      };

      tree.setAttribute("rows", treeView.rowCount);
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

      let text = WebConsoleUtils.objectActorGripToString(aItem);

      if (aItem && typeof aItem != "object" || !aItem.inspectable) {
        aContainer.appendChild(this.document.createTextNode(text));

        let longString = null;
        if (aItem.type == "longString") {
          longString = aItem;
        }
        else if (!aItem.inspectable &&
                 typeof aItem.displayString == "object" &&
                 aItem.displayString.type == "longString") {
          longString = aItem.displayString;
        }

        if (longString) {
          let ellipsis = this.document.createElement("description");
          ellipsis.classList.add("hud-clickable");
          ellipsis.classList.add("longStringEllipsis");
          ellipsis.textContent = l10n.getStr("longStringEllipsis");

          this._addMessageLinkCallback(ellipsis,
            this._longStringClick.bind(this, aMessage, longString, null));

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
        this._consoleLogClick.bind(this, aMessage, elem, aItem));

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
    longString.substring(longString.initial.length, longString.length,
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
      });
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
    let text;

    if (/^Scratchpad\/\d+$/.test(aSourceURL)) {
      text = aSourceURL;
    }
    else {
      text = WebConsoleUtils.abbreviateSourceURL(aSourceURL);
    }

    if (aSourceLine) {
      text += ":" + aSourceLine;
      locationNode.sourceLine = aSourceLine;
    }

    locationNode.setAttribute("value", text);

    // Style appropriately.
    locationNode.setAttribute("crop", "center");
    locationNode.setAttribute("title", aSourceURL);
    locationNode.setAttribute("tooltiptext", aSourceURL);
    locationNode.classList.add("webconsole-location");
    locationNode.classList.add("text-link");

    // Make the location clickable.
    locationNode.addEventListener("click", function() {
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
        this.owner.viewSourceInStyleEditor(aSourceURL, aSourceLine);
      }
      else {
        this.owner.viewSource(aSourceURL, aSourceLine);
      }
    }.bind(this), true);

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
   *         A Promise that is resolved when the WebConsoleFrame instance is
   *         destroyed.
   */
  destroy: function WCF_destroy()
  {
    if (this._destroyer) {
      return this._destroyer.promise;
    }

    this._destroyer = Promise.defer();

    this._cssNodes = {};
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
  this.historyIndex = 0;
  this.historyPlaceHolder = 0;  // this.history.length;
  this._keyPress = this.keyPress.bind(this);
  this._inputEventHandler = this.inputEventHandler.bind(this);
}

JSTerm.prototype = {
  /**
   * Stores the data for the last completion.
   * @type object
   */
  lastCompletion: null,

  /**
   * Last input value.
   * @type string
   */
  lastInputValue: "",

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
    let chromeDocument = this.hud.owner.chromeDocument;
    this.autocompletePopup = new AutocompletePopup(chromeDocument);
    this.autocompletePopup.onSelect = this.onAutocompleteSelect.bind(this);
    this.autocompletePopup.onClick = this.acceptProposedCompletion.bind(this);

    let doc = this.hud.document;
    this.completeNode = doc.querySelector(".jsterm-complete-node");
    this.inputNode = doc.querySelector(".jsterm-input-node");
    this.inputNode.addEventListener("keypress", this._keyPress, false);
    this.inputNode.addEventListener("input", this._inputEventHandler, false);
    this.inputNode.addEventListener("keyup", this._inputEventHandler, false);

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

    let errorMessage = aResponse.errorMessage;
    let result = aResponse.result;
    let inspectable = result && typeof result == "object" && result.inspectable;
    let helperResult = aResponse.helperResult;
    let helperHasRawOutput = !!(helperResult || {}).rawOutput;
    let resultString =
      WebConsoleUtils.objectActorGripToString(result,
                                              !helperHasRawOutput);

    if (helperResult && helperResult.type) {
      switch (helperResult.type) {
        case "clearOutput":
          this.clearOutput();
          break;
        case "inspectObject":
          this.handleInspectObject(helperResult.input, helperResult.object);
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

    if (result && typeof result == "object" && result.actor) {
      node._objectActors = [result.actor];
      if (typeof result.displayString == "object" &&
          result.displayString.type == "longString") {
        node._objectActors.push(result.displayString.actor);
      }

      // Add an ellipsis to expand the short string if the object is not
      // inspectable.
      let longString = null;
      let formatter = null;
      if (result.type == "longString") {
        longString = result;
        if (!helperHasRawOutput) {
          formatter = WebConsoleUtils.formatResultString.bind(WebConsoleUtils);
        }
      }
      else if (!inspectable && !errorMessage &&
               typeof result.displayString == "object" &&
               result.displayString.type == "longString") {
        longString = result.displayString;
      }

      if (longString) {
        let body = node.querySelector(".webconsole-msg-body");
        let ellipsis = this.hud.document.createElement("description");
        ellipsis.classList.add("hud-clickable");
        ellipsis.classList.add("longStringEllipsis");
        ellipsis.textContent = l10n.getStr("longStringEllipsis");

        this.hud._addMessageLinkCallback(ellipsis,
          this.hud._longStringClick.bind(this.hud, node, longString, formatter));

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
      this.writeOutput(l10n.getStr("executeEmptyInput"), CATEGORY_OUTPUT,
                       SEVERITY_LOG);
      return;
    }

    let node = this.writeOutput(aExecuteString, CATEGORY_INPUT, SEVERITY_LOG);
    let onResult = this._executeResultCallback.bind(this, node, aCallback);

    this.webConsoleClient.evaluateJS(aExecuteString, onResult);

    this.history.push(aExecuteString);
    this.historyIndex++;
    this.historyPlaceHolder = this.history.length;
    this.setInputValue("");
    this.clearCompletion();
  },

  /**
   * Opens a new property panel that allows the inspection of the given object.
   * The object information can be retrieved both async and sync, depending on
   * the given options.
   *
   * @param object aOptions
   *        Property panel options:
   *        - title:
   *        Panel title.
   *        - anchor:
   *        The DOM element you want the panel to be anchored to.
   *        - updateButtonCallback:
   *        An optional function you want invoked when the user clicks the
   *        Update button. If this function is not provided the Update button is
   *        not shown.
   *        - data:
   *        An object that represents the object you want to inspect. Please see
   *        the PropertyPanel documentation - this object is passed to the
   *        PropertyPanel constructor
   * @return object
   *         The new instance of PropertyPanel.
   */
  openPropertyPanel: function JST_openPropertyPanel(aOptions)
  {
    // The property panel has one button:
    //    `Update`: reexecutes the string executed on the command line. The
    //    result will be inspected by this panel.
    let buttons = [];

    if (aOptions.updateButtonCallback) {
      buttons.push({
        label: l10n.getStr("update.button"),
        accesskey: l10n.getStr("update.accesskey"),
        oncommand: aOptions.updateButtonCallback,
      });
    }

    let parent = this.hud.popupset;
    let title = aOptions.title ?
                l10n.getFormatStr("jsPropertyInspectTitle", [aOptions.title]) :
                l10n.getStr("jsPropertyTitle");

    let propPanel = new PropertyPanel(parent, title, aOptions.data, buttons);

    propPanel.panel.openPopup(aOptions.anchor, "after_pointer", 0, 0, false, false);
    propPanel.panel.sizeTo(350, 450);

    if (aOptions.anchor) {
      propPanel.panel.addEventListener("popuphiding", function onPopupHide() {
        propPanel.panel.removeEventListener("popuphiding", onPopupHide, false);
        aOptions.anchor._panelOpen = false;
      }, false);
      aOptions.anchor._panelOpen = true;
    }

    return propPanel;
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
    hud._cssNodes = {};

    if (aClearStorage) {
      this.webConsoleClient.clearMessagesCache();
    }
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
  },

  /**
   * The inputNode "input" and "keyup" event handler.
   *
   * @param nsIDOMEvent aEvent
   */
  inputEventHandler: function JSTF_inputEventHandler(aEvent)
  {
    if (this.lastInputValue != this.inputNode.value) {
      this.resizeInput();
      this.complete(this.COMPLETE_HINT_ONLY);
      this.lastInputValue = this.inputNode.value;
    }
  },

  /**
   * The inputNode "keypress" event handler.
   *
   * @param nsIDOMEvent aEvent
   */
  keyPress: function JSTF_keyPress(aEvent)
  {
    if (aEvent.ctrlKey) {
      let inputNode = this.inputNode;
      let closePopup = false;
      switch (aEvent.charCode) {
        case 97:
          // control-a
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
          closePopup = true;
          break;
        case 101:
          // control-e
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
          break;
        case 110:
          // Control-N differs from down arrow: it ignores autocomplete state.
          // Note that we preserve the default 'down' navigation within
          // multiline text.
          if (Services.appinfo.OS == "Darwin" &&
              this.canCaretGoNext() &&
              this.historyPeruse(HISTORY_FORWARD)) {
            aEvent.preventDefault();
          }
          closePopup = true;
          break;
        case 112:
          // Control-P differs from up arrow: it ignores autocomplete state.
          // Note that we preserve the default 'up' navigation within
          // multiline text.
          if (Services.appinfo.OS == "Darwin" &&
              this.canCaretGoPrevious() &&
              this.historyPeruse(HISTORY_BACK)) {
            aEvent.preventDefault();
          }
          closePopup = true;
          break;
        default:
          break;
      }
      if (closePopup) {
        if (this.autocompletePopup.isOpen) {
          this.clearCompletion();
        }
      }
      return;
    }
    else if (aEvent.shiftKey &&
        aEvent.keyCode == Ci.nsIDOMKeyEvent.DOM_VK_RETURN) {
      // shift return
      // TODO: expand the inputNode height by one line
      return;
    }

    let inputUpdated = false;

    switch(aEvent.keyCode) {
      case Ci.nsIDOMKeyEvent.DOM_VK_ESCAPE:
        if (this.autocompletePopup.isOpen) {
          this.clearCompletion();
          aEvent.preventDefault();
        }
        break;

      case Ci.nsIDOMKeyEvent.DOM_VK_RETURN:
        if (this.autocompletePopup.isOpen && this.autocompletePopup.selectedIndex > -1) {
          this.acceptProposedCompletion();
        }
        else {
          this.execute();
        }
        aEvent.preventDefault();
        break;

      case Ci.nsIDOMKeyEvent.DOM_VK_UP:
        if (this.autocompletePopup.isOpen) {
          inputUpdated = this.complete(this.COMPLETE_BACKWARD);
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
        }
        else if (this.canCaretGoNext()) {
          inputUpdated = this.historyPeruse(HISTORY_FORWARD);
        }
        if (inputUpdated) {
          aEvent.preventDefault();
        }
        break;

      case Ci.nsIDOMKeyEvent.DOM_VK_TAB:
        // Generate a completion and accept the first proposed value.
        if (this.complete(this.COMPLETE_HINT_ONLY) &&
            this.lastCompletion &&
            this.acceptProposedCompletion()) {
          aEvent.preventDefault();
        }
        else {
          this.updateCompleteNode(l10n.getStr("Autocomplete.blank"));
          aEvent.preventDefault();
        }
        break;

      default:
        break;
    }
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
      if (inputVal){
        this.setInputValue(inputVal);
      }
    }
    // Down Arrow key
    else if (aDirection == HISTORY_FORWARD) {
      if (this.historyPlaceHolder == this.history.length - 1) {
        this.historyPlaceHolder ++;
        this.setInputValue("");
      }
      else if (this.historyPlaceHolder >= (this.history.length)) {
        return false;
      }
      else {
        let inputVal = this.history[++this.historyPlaceHolder];
        if (inputVal){
          this.setInputValue(inputVal);
        }
      }
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

    // Only complete if the selection is empty and at the end of the input.
    if (inputNode.selectionStart == inputNode.selectionEnd &&
        inputNode.selectionEnd != inputValue.length) {
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
    let input = this.inputNode.value;
    let cursor = this.inputNode.selectionStart;

    // TODO: Bug 787986 - throttle/disable updates, deal with slow/high latency
    // network connections.
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

    let matches = aMessage.matches;
    if (!matches.length) {
      this.clearCompletion();
      return;
    }

    let items = matches.map(function(aMatch) {
      return { label: aMatch };
    });

    let popup = this.autocompletePopup;
    popup.setItems(items);

    let completionType = this.lastCompletion.completionType;
    this.lastCompletion = {
      value: inputValue,
      matchProp: aMessage.matchProp,
    };

    if (items.length > 1 && !popup.isOpen) {
      popup.openPopup(inputNode);
    }
    else if (items.length < 2 && popup.isOpen) {
      popup.hidePopup();
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
      this.setInputValue(this.inputNode.value + suffix);
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
   * The JSTerm InspectObject remote message handler. This allows the remote
   * process to open the Property Panel for a given object.
   *
   * @param object aRequest
   *        The request message from the content process. This message includes
   *        the user input string that was evaluated to inspect an object and
   *        the result object which is to be inspected.
   */
  handleInspectObject: function JST_handleInspectObject(aInput, aActor)
  {
    let options = {
      title: aInput,

      data: {
        objectPropertiesProvider: this.hud.objectPropertiesProvider.bind(this.hud),
        releaseObject: this.hud._releaseObject.bind(this.hud),
      },
    };

    let propPanel;

    let onPopupHide = function JST__onPopupHide() {
      propPanel.panel.removeEventListener("popuphiding", onPopupHide, false);
      this.hud._releaseObject(aActor.actor);
    }.bind(this);

    this.hud.objectPropertiesProvider(aActor.actor,
      function _onObjectProperties(aProperties) {
        options.data.objectProperties = aProperties;
        propPanel = this.openPropertyPanel(options);
        propPanel.panel.setAttribute("hudId", this.hudId);
        propPanel.panel.addEventListener("popuphiding", onPopupHide, false);
      }.bind(this));
  },

  /**
   * The click event handler for evaluation results in the output.
   *
   * @private
   * @param object aResponse
   *        The JavaScript evaluation response received from the server.
   * @param nsIDOMNode aLink
   *        The message node for which we are handling events.
   */
  _evalOutputClick: function JST__evalOutputClick(aResponse, aLinkNode)
  {
    if (aLinkNode._panelOpen) {
      return;
    }

    let options = {
      title: aResponse.input,
      anchor: aLinkNode,

      // Data to inspect.
      data: {
        objectPropertiesProvider: this.hud.objectPropertiesProvider.bind(this.hud),
        releaseObject: this.hud._releaseObject.bind(this.hud),
      },
    };

    let propPanel;

    options.updateButtonCallback = function JST__evalUpdateButton() {
      let onResult =
        this._evalOutputUpdatePanelCallback.bind(this, options, propPanel,
                                                 aResponse);
      this.webConsoleClient.evaluateJS(aResponse.input, onResult);
    }.bind(this);

    let onPopupHide = function JST__evalInspectPopupHide() {
      propPanel.panel.removeEventListener("popuphiding", onPopupHide, false);

      if (!aLinkNode.parentNode && aLinkNode._objectActors) {
        aLinkNode._objectActors.forEach(this.hud._releaseObject, this.hud);
        aLinkNode._objectActors = null;
      }
    }.bind(this);

    this.hud.objectPropertiesProvider(aResponse.result.actor,
      function _onObjectProperties(aProperties) {
        options.data.objectProperties = aProperties;
        propPanel = this.openPropertyPanel(options);
        propPanel.panel.setAttribute("hudId", this.hudId);
        propPanel.panel.addEventListener("popuphiding", onPopupHide, false);
      }.bind(this));
  },

  /**
   * The callback used for updating the Property Panel when the user clicks the
   * Update button.
   *
   * @private
   * @param object aOptions
   *        The options object used for opening the initial Property Panel.
   * @param object aPropPanel
   *        The Property Panel instance.
   * @param object aOldResponse
   *        The previous JSTerm:EvalResult message received from the content
   *        process.
   * @param object aNewResponse
   *        The new JSTerm:EvalResult message received after the user clicked
   *        the Update button.
   */
  _evalOutputUpdatePanelCallback:
  function JST__updatePanelCallback(aOptions, aPropPanel, aOldResponse,
                                    aNewResponse)
  {
    if (aNewResponse.errorMessage) {
      this.writeOutput(aNewResponse.errorMessage, CATEGORY_OUTPUT,
                       SEVERITY_ERROR);
      return;
    }

    let result = aNewResponse.result;
    let inspectable = result && typeof result == "object" && result.inspectable;
    let newActor = result && typeof result == "object" ? result.actor : null;

    let anchor = aOptions.anchor;
    if (anchor && newActor) {
      if (!anchor._objectActors) {
        anchor._objectActors = [];
      }
      if (anchor._objectActors.indexOf(newActor) == -1) {
        anchor._objectActors.push(newActor);
      }
    }

    if (!inspectable) {
      this.writeOutput(l10n.getStr("JSTerm.updateNotInspectable"), CATEGORY_OUTPUT, SEVERITY_ERROR);
      return;
    }

    // Update the old response object such that when the panel is reopen, the
    // user sees the new response.
    aOldResponse.result = aNewResponse.result;
    aOldResponse.error = aNewResponse.error;
    aOldResponse.errorMessage = aNewResponse.errorMessage;
    aOldResponse.timestamp = aNewResponse.timestamp;

    this.hud.objectPropertiesProvider(newActor,
      function _onObjectProperties(aProperties) {
        aOptions.data.objectProperties = aProperties;
        // TODO: This updates the value of the tree.
        // However, the states of open nodes is not saved.
        // See bug 586246.
        aPropPanel.treeView.data = aOptions.data;
      }.bind(this));
  },

  /**
   * Destroy the JSTerm object. Call this method to avoid memory leaks.
   */
  destroy: function JST_destroy()
  {
    this.clearCompletion();
    this.clearOutput();

    this.autocompletePopup.destroy();
    this.autocompletePopup = null;

    let popup = this.hud.owner.chromeDocument
                .getElementById("webConsole_autocompletePopup");
    if (popup) {
      popup.parentNode.removeChild(popup);
    }

    this.inputNode.removeEventListener("keypress", this._keyPress, false);
    this.inputNode.removeEventListener("input", this._inputEventHandler, false);
    this.inputNode.removeEventListener("keyup", this._inputEventHandler, false);

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
   * @return CATEGORY_JS|CATEGORY_CSS
   *         Depending on the script error CATEGORY_JS or CATEGORY_CSS can be
   *         returned.
   */
  categoryForScriptError: function Utils_categoryForScriptError(aScriptError)
  {
    switch (aScriptError.category) {
      case "CSS Parser":
      case "CSS Loader":
        return CATEGORY_CSS;

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
        return selectedItem && selectedItem.url;
      }
      case "cmd_fontSizeEnlarge":
      case "cmd_fontSizeReduce":
      case "cmd_fontSizeReset":
      case "cmd_selectAll":
        return true;
    }
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
  this._onConsoleAPICall = this._onConsoleAPICall.bind(this);
  this._onNetworkEvent = this._onNetworkEvent.bind(this);
  this._onNetworkEventUpdate = this._onNetworkEventUpdate.bind(this);
  this._onFileActivity = this._onFileActivity.bind(this);
  this._onTabNavigated = this._onTabNavigated.bind(this);
  this._onListTabs = this._onListTabs.bind(this);
  this._onAttachTab = this._onAttachTab.bind(this);
  this._onAttachConsole = this._onAttachConsole.bind(this);
  this._onCachedMessages = this._onCachedMessages.bind(this);
  this._connectionTimeout = this._connectionTimeout.bind(this);
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
   * The TabClient instance we use.
   * @type object
   */
  tabClient: null,

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
   * The TabActor ID.
   *
   * @private
   * @type string
   */
  _tabActor: null,

  /**
   * Tells if the window.console object of the remote web page is the native
   * object or not.
   * @private
   * @type boolean
   */
  _hasNativeConsoleAPI: false,

  /**
   * Initialize the debugger server.
   */
  initServer: function WCCP_initServer()
  {
    if (!DebuggerServer.initialized) {
      DebuggerServer.init();
      DebuggerServer.addBrowserActors();
    }
  },

  /**
   * Initialize a debugger client and connect it to the debugger server.
   *
   * @return object
   *         A Promise object that is resolved/rejected based on the success of
   *         the connection initialization.
   */
  connect: function WCCP_connect()
  {
    if (this._connectDefer) {
      return this._connectDefer.promise;
    }

    this._connectDefer = Promise.defer();

    let timeout = Services.prefs.getIntPref(PREF_CONNECTION_TIMEOUT);
    this._connectTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this._connectTimer.initWithCallback(this._connectionTimeout,
                                        timeout, Ci.nsITimer.TYPE_ONE_SHOT);

    let promise = this._connectDefer.promise;
    promise.then(function _onSucess() {
      this._connectTimer.cancel();
      this._connectTimer = null;
    }.bind(this), function _onFailure() {
      this._connectTimer = null;
    }.bind(this));

    // TODO: convert the non-remote path to use the target API as well.
    let transport, client;
    if (this.target.isRemote) {
      client = this.client = this.target.client;
    }
    else {
      this.initServer();
      transport = DebuggerServer.connectPipe();
      client = this.client = new DebuggerClient(transport);
    }

    client.addListener("pageError", this._onPageError);
    client.addListener("consoleAPICall", this._onConsoleAPICall);
    client.addListener("networkEvent", this._onNetworkEvent);
    client.addListener("networkEventUpdate", this._onNetworkEventUpdate);
    client.addListener("fileActivity", this._onFileActivity);
    client.addListener("tabNavigated", this._onTabNavigated);

    if (this.target.isRemote) {
      if (!this.target.chrome) {
        // target.form is a TabActor grip
        this._attachTab(this.target.form);
      }
      else {
        // target.form is a RootActor grip
        this._consoleActor = this.target.form.consoleActor;
        this._attachConsole();
      }
    }
    else {
      client.connect(function(aType, aTraits) {
        client.listTabs(this._onListTabs);
      }.bind(this));
    }

    return promise;
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
   * The "listTabs" response handler.
   *
   * @private
   * @param object aResponse
   *        The JSON response object received from the server.
   */
  _onListTabs: function WCCP__onListTabs(aResponse)
  {
    if (aResponse.error) {
      Cu.reportError("listTabs failed: " + aResponse.error + " " +
                     aResponse.message);
      this._connectDefer.reject(aResponse);
      return;
    }

    this._attachTab(aResponse.tabs[aResponse.selected]);
  },

  /**
   * Attach to the tab actor.
   *
   * @private
   * @param object aTab
   *        Grip for the tab to attach to.
   */
  _attachTab: function WCCP__attachTab(aTab)
  {
    this._consoleActor = aTab.consoleActor;
    this._tabActor = aTab.actor;
    this.owner.onLocationChange(aTab.url, aTab.title);
    this.client.attachTab(this._tabActor, this._onAttachTab);
  },

  /**
   * The "attachTab" response handler.
   *
   * @private
   * @param object aResponse
   *        The JSON response object received from the server.
   * @param object aTabClient
   *        The TabClient instance for the attached tab.
   */
  _onAttachTab: function WCCP__onAttachTab(aResponse, aTabClient)
  {
    if (aResponse.error) {
      Cu.reportError("attachTab failed: " + aResponse.error + " " +
                     aResponse.message);
      this._connectDefer.reject(aResponse);
      return;
    }

    this.tabClient = aTabClient;
    this._attachConsole();
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
      // This happens if the Promise is rejected (eg. a timeout), but the
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
   * The "tabNavigated" message type handler. We redirect any message to
   * the UI for displaying.
   *
   * @private
   * @param string aType
   *        Message type.
   * @param object aPacket
   *        The message received from the server.
   */
  _onTabNavigated: function WCCP__onTabNavigated(aType, aPacket)
  {
    if (!this.owner || aPacket.from != this._tabActor) {
      return;
    }

    if (aPacket.url) {
      this.owner.onLocationChange(aPacket.url, aPacket.title);
    }

    if (aPacket.state == "stop" && !aPacket.nativeConsoleAPI) {
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
   *         A Promise object that is resolved when disconnect completes.
   */
  disconnect: function WCCP_disconnect()
  {
    if (this._disconnecter) {
      return this._disconnecter.promise;
    }

    this._disconnecter = Promise.defer();

    if (!this.client) {
      this._disconnecter.resolve(null);
      return this._disconnecter.promise;
    }

    let onDisconnect = function() {
      if (timer) {
        timer.cancel();
        timer = null;
        this._disconnecter.resolve(null);
      }
    }.bind(this);

    let timer = null;
    let remoteTarget = this.target.isRemote;
    if (!remoteTarget) {
      timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      timer.initWithCallback(onDisconnect, 1500, Ci.nsITimer.TYPE_ONE_SHOT);
    }

    this.client.removeListener("pageError", this._onPageError);
    this.client.removeListener("consoleAPICall", this._onConsoleAPICall);
    this.client.removeListener("networkEvent", this._onNetworkEvent);
    this.client.removeListener("networkEventUpdate", this._onNetworkEventUpdate);
    this.client.removeListener("fileActivity", this._onFileActivity);
    this.client.removeListener("tabNavigated", this._onTabNavigated);

    let client = this.client;

    this.client = null;
    this.webConsoleClient = null;
    this.tabClient = null;
    this.target = null;
    this.connected = false;
    this.owner = null;

    if (!remoteTarget) {
      try {
        client.close(onDisconnect);
      }
      catch (ex) {
        Cu.reportError("Web Console disconnect exception: " + ex);
        Cu.reportError(ex.stack);
        onDisconnect();
      }
    }
    else {
      onDisconnect();
    }

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
