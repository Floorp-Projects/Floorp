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

XPCOMUtils.defineLazyModuleGetter(this, "PropertyPanel",
                                  "resource:///modules/PropertyPanel.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PropertyTreeView",
                                  "resource:///modules/PropertyPanel.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetworkPanel",
                                  "resource:///modules/NetworkPanel.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AutocompletePopup",
                                  "resource:///modules/AutocompletePopup.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "WebConsoleUtils",
                                  "resource:///modules/WebConsoleUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "l10n", function() {
  return WebConsoleUtils.l10n;
});


// The XUL namespace.
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

const MIXED_CONTENT_LEARN_MORE = "https://developer.mozilla.org/en/Security/MixedContent";

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

/**
 * A WebConsoleFrame instance is an interactive console initialized *per tab*
 * that displays console log data as well as provides an interactive terminal to
 * manipulate the current tab's document content.
 *
 * The WebConsoleFrame is responsible for the actual Web Console UI
 * implementation.
 *
 * @param object aWebConsoleOwner
 *        The WebConsole owner object.
 * @param string aPosition
 *        Tells the UI location for the Web Console.
 */
function WebConsoleFrame(aWebConsoleOwner, aPosition)
{
  this.owner = aWebConsoleOwner;
  this.hudId = this.owner.hudId;

  this._cssNodes = {};
  this._outputQueue = [];
  this._pruneCategoriesQueue = {};
  this._networkRequests = {};

  this._toggleFilter = this._toggleFilter.bind(this);
  this._onPositionConsoleCommand = this._onPositionConsoleCommand.bind(this);

  this._initDefaultFilterPrefs();
  this._commandController = new CommandController(this);
  this.positionConsole(aPosition, window);

  this.jsterm = new JSTerm(this);
  this.jsterm.inputNode.focus();
}

WebConsoleFrame.prototype = {
  /**
   * The WebConsole instance that owns this frame.
   * @see HUDService.jsm::WebConsole
   * @type object
   */
  owner: null,

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
    this._saveRequestAndResponseBodies = aValue;

    let message = {
      preferences: {
        "NetworkMonitor.saveRequestAndResponseBodies":
          this._saveRequestAndResponseBodies,
      },
    };

    this.owner.sendMessageToContent("WebConsole:SetPreferences", message);
  },

  /**
   * Find the Web Console UI elements and setup event listeners as needed.
   * @private
   */
  _initUI: function WCF__initUI()
  {
    let doc = this.document;

    this.filterBox = doc.querySelector(".hud-filter-box");
    this.outputNode = doc.querySelector(".hud-output-node");

    this._setFilterTextBoxEvents();
    this._initPositionUI();
    this._initFilterButtons();

    let saveBodies = doc.getElementById("saveBodies");
    saveBodies.addEventListener("command", function() {
      this.saveRequestAndResponseBodies = !this.saveRequestAndResponseBodies;
    }.bind(this));
    saveBodies.setAttribute("checked", this.saveRequestAndResponseBodies);

    let contextMenuId = this.outputNode.getAttribute("context");
    let contextMenu = doc.getElementById(contextMenuId);
    contextMenu.addEventListener("popupshowing", function() {
      saveBodies.setAttribute("checked", this.saveRequestAndResponseBodies);
    }.bind(this));

    this.closeButton = doc.getElementsByClassName("webconsole-close-button")[0];
    this.closeButton.addEventListener("command",
                                      this.owner.onCloseButton.bind(this.owner));

    let clearButton = doc.getElementsByClassName("webconsole-clear-console-button")[0];
    clearButton.addEventListener("command",
                                 this.owner.onClearButton.bind(this.owner));
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
   * Initialize the UI for re-positioning the console
   * @private
   */
  _initPositionUI: function WCF__initPositionUI()
  {
    let doc = this.document;

    let itemAbove = doc.querySelector("menuitem[consolePosition='above']");
    itemAbove.addEventListener("command", this._onPositionConsoleCommand, false);

    let itemBelow = doc.querySelector("menuitem[consolePosition='below']");
    itemBelow.addEventListener("command", this._onPositionConsoleCommand, false);

    let itemWindow = doc.querySelector("menuitem[consolePosition='window']");
    itemWindow.addEventListener("command", this._onPositionConsoleCommand, false);

    this.positionMenuitems = {
      last: null,
      above: itemAbove,
      below: itemBelow,
      window: itemWindow,
    };
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
   * Handle the "command" event for the buttons that allow the user to
   * reposition the Web Console UI.
   *
   * @private
   * @param nsIDOMEvent aEvent
   */
  _onPositionConsoleCommand: function WCF__onPositionConsoleCommand(aEvent)
  {
    let position = aEvent.target.getAttribute("consolePosition");
    this.owner.positionConsole(position);
  },

  /**
   * Position the console in a different location.
   *
   * Note: you do not usually call this method. This is called by the WebConsole
   * instance that owns this iframe. You need to call this if you write
   * a different owner or you manually reposition the iframe.
   *
   * @param string aPosition
   *        The new Web Console iframe location: "above" (the page), "below" or
   *        "window".
   * @param object aNewWindow
   *        Repositioning causes the iframe to reload - bug 254144. You need to
   *        provide the new window object so we can reinitialize the UI as
   *        needed.
   */
  positionConsole: function WCF_positionConsole(aPosition, aNewWindow)
  {
    this.window = aNewWindow;
    this.document = this.window.document;
    this.rootElement = this.document.documentElement;

    // register the controller to handle "select all" properly
    this.window.controllers.insertControllerAt(0, this._commandController);

    let oldOutputNode = this.outputNode;

    this._initUI();
    this.jsterm && this.jsterm._initUI();

    this.closeButton.hidden = aPosition == "window";

    this.positionMenuitems[aPosition].setAttribute("checked", true);
    if (this.positionMenuitems.last) {
      this.positionMenuitems.last.setAttribute("checked", false);
    }
    this.positionMenuitems.last = this.positionMenuitems[aPosition];

    if (oldOutputNode && oldOutputNode.childNodes.length) {
      let parentNode = this.outputNode.parentNode;
      parentNode.replaceChild(oldOutputNode, this.outputNode);
      this.outputNode = oldOutputNode;
    }

    this.jsterm && this.jsterm.inputNode.focus();
  },

  /**
   * Handler for all of the messages coming from the Web Console content script.
   *
   * @private
   * @param object aMessage
   *        A MessageManager object that holds the remote message.
   */
  receiveMessage: function WCF_receiveMessage(aMessage)
  {
    if (!aMessage.json || aMessage.json.hudId != this.hudId) {
      return;
    }

    switch (aMessage.name) {
      case "JSTerm:EvalResult":
      case "JSTerm:EvalObject":
      case "JSTerm:AutocompleteProperties":
        this.owner._receiveMessageWithCallback(aMessage.json);
        break;
      case "JSTerm:ClearOutput":
        this.jsterm.clearOutput();
        break;
      case "JSTerm:InspectObject":
        this.jsterm.handleInspectObject(aMessage.json);
        break;
      case "WebConsole:ConsoleAPI":
        this.outputMessage(CATEGORY_WEBDEV, this.logConsoleAPIMessage,
                           [aMessage.json]);
        break;
      case "WebConsole:PageError": {
        let pageError = aMessage.json.pageError;
        let category = Utils.categoryForScriptError(pageError);
        this.outputMessage(category, this.reportPageError,
                           [category, pageError]);
        break;
      }
      case "WebConsole:CachedMessages":
        this._displayCachedConsoleMessages(aMessage.json.messages);
        this.owner._onInitComplete();
        break;
      case "WebConsole:NetworkActivity":
        this.handleNetworkActivity(aMessage.json);
        break;
      case "WebConsole:FileActivity":
        this.outputMessage(CATEGORY_NETWORK, this.logFileActivity,
                           [aMessage.json.uri]);
        break;
      case "WebConsole:LocationChange":
        this.owner.onLocationChange(aMessage.json);
        break;
      case "JSTerm:NonNativeConsoleAPI":
        this.outputMessage(CATEGORY_JS, this.logWarningAboutReplacedAPI);
        break;
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

        // Adjust the state of the button appropriately.
        let menuPopup = target.parentNode;

        let someChecked = false;
        let menuItem = menuPopup.firstChild;
        while (menuItem) {
          if (menuItem.getAttribute("checked") === "true") {
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
   * messages are filtered from the output if and only if they match the
   * immediately preceding message. The output node's last occurrence should
   * have its timestamp updated.
   *
   * @param nsIDOMNode aNode
   *        The message node to be filtered or not.
   * @return boolean
   *         True if the message is filtered, false otherwise.
   */
  filterRepeatedConsole: function WCF_filterRepeatedConsole(aNode)
  {
    let lastMessage = this.outputNode.lastChild;

    // childNodes[2] is the description element
    if (lastMessage && lastMessage.childNodes[2] &&
        !aNode.classList.contains("webconsole-msg-inspector") &&
        aNode.childNodes[2].textContent ==
        lastMessage.childNodes[2].textContent) {
      this.mergeFilteredMessageNode(lastMessage, aNode);
      return true;
    }

    return false;
  },

  /**
   * Display cached messages that may have been collected before the UI is
   * displayed.
   *
   * @private
   * @param array aRemoteMessages
   *        Array of cached messages coming from the remote Web Console
   *        content instance.
   */
  _displayCachedConsoleMessages:
  function WCF__displayCachedConsoleMessages(aRemoteMessages)
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
   * Logs a message to the Web Console that originates from the remote Web
   * Console instance.
   *
   * @param object aMessage
   *        The message received from the remote Web Console instance.
   *        console service. This object needs to hold:
   *          - hudId - the Web Console ID.
   *          - apiMessage - a representation of the object sent by the console
   *          storage service. This object holds the console message level, the
   *          arguments that were passed to the console method and other
   *          information.
   *          - argumentsToString - the array of arguments passed to the console
   *          method, each converted to a string.
   * @return nsIDOMElement|undefined
   *         The message element to display in the Web Console output.
   */
  logConsoleAPIMessage: function WCF_logConsoleAPIMessage(aMessage)
  {
    let body = null;
    let clipboardText = null;
    let sourceURL = null;
    let sourceLine = 0;
    let level = aMessage.apiMessage.level;
    let args = aMessage.apiMessage.arguments;
    let argsToString = aMessage.argumentsToString;

    switch (level) {
      case "log":
      case "info":
      case "warn":
      case "error":
      case "debug":
        body = {
          cacheId: aMessage.objectsCacheId,
          remoteObjects: args,
          argsToString: argsToString,
        };
        clipboardText = argsToString.join(" ");
        sourceURL = aMessage.apiMessage.filename;
        sourceLine = aMessage.apiMessage.lineNumber;
        break;

      case "trace":
        let filename = WebConsoleUtils.abbreviateSourceURL(args[0].filename);
        let functionName = args[0].functionName ||
                           l10n.getStr("stacktrace.anonymousFunction");
        let lineNumber = args[0].lineNumber;

        body = l10n.getFormatStr("stacktrace.outputMessage",
                                 [filename, functionName, lineNumber]);

        sourceURL = args[0].filename;
        sourceLine = args[0].lineNumber;

        clipboardText = "";

        args.forEach(function(aFrame) {
          clipboardText += aFrame.filename + " :: " +
                           aFrame.functionName + " :: " +
                           aFrame.lineNumber + "\n";
        });

        clipboardText = clipboardText.trimRight();
        break;

      case "dir":
        body = {
          cacheId: aMessage.objectsCacheId,
          resultString: argsToString[0],
          remoteObject: args[0],
          remoteObjectProvider:
            this.jsterm.remoteObjectProvider.bind(this.jsterm),
        };
        clipboardText = body.resultString;
        sourceURL = aMessage.apiMessage.filename;
        sourceLine = aMessage.apiMessage.lineNumber;
        break;

      case "group":
      case "groupCollapsed":
        clipboardText = body = args;
        sourceURL = aMessage.apiMessage.filename;
        sourceLine = aMessage.apiMessage.lineNumber;
        this.groupDepth++;
        break;

      case "groupEnd":
        if (this.groupDepth > 0) {
          this.groupDepth--;
        }
        return;

      case "time":
        if (!args) {
          return;
        }
        if (args.error) {
          Cu.reportError(l10n.getStr(args.error));
          return;
        }
        body = l10n.getFormatStr("timerStarted", [args.name]);
        clipboardText = body;
        sourceURL = aMessage.apiMessage.filename;
        sourceLine = aMessage.apiMessage.lineNumber;
        break;

      case "timeEnd":
        if (!args) {
          return;
        }
        body = l10n.getFormatStr("timeEnd", [args.name, args.duration]);
        clipboardText = body;
        sourceURL = aMessage.apiMessage.filename;
        sourceLine = aMessage.apiMessage.lineNumber;
        break;

      default:
        Cu.reportError("Unknown Console API log level: " + level);
        return;
    }

    let node = this.createMessageNode(CATEGORY_WEBDEV, LEVELS[level], body,
                                      sourceURL, sourceLine, clipboardText,
                                      level, aMessage.timeStamp);

    // Make the node bring up the property panel, to allow the user to inspect
    // the stack trace.
    if (level == "trace") {
      node._stacktrace = args;

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
      // Make sure the cached evaluated object will be purged when the node is
      // removed.
      node._evalCacheId = aMessage.objectsCacheId;

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
   * The click event handler for objects shown inline coming from the
   * window.console API.
   *
   * @private
   * @param nsIDOMNode aMessage
   *        The message element this handler corresponds to.
   * @param nsIDOMNode aAnchor
   *        The object inspector anchor element. This is the clickable element
   *        in the console.log message we display.
   * @param array aRemoteObject
   *        The remote object representation.
   */
  _consoleLogClick:
  function WCF__consoleLogClick(aMessage, aAnchor, aRemoteObject)
  {
    if (aAnchor._panelOpen) {
      return;
    }

    let options = {
      title: aAnchor.textContent,
      anchor: aAnchor,

      // Data to inspect.
      data: {
        // This is where the resultObject children are cached.
        rootCacheId: aMessage._evalCacheId,
        remoteObject: aRemoteObject,
        // This is where all objects retrieved by the panel will be cached.
        panelCacheId: "HUDPanel-" + gSequenceId(),
        remoteObjectProvider: this.jsterm.remoteObjectProvider.bind(this.jsterm),
      },
    };

    let propPanel = this.jsterm.openPropertyPanel(options);
    propPanel.panel.setAttribute("hudId", this.hudId);

    let onPopupHide = function JST__evalInspectPopupHide() {
      propPanel.panel.removeEventListener("popuphiding", onPopupHide, false);

      this.jsterm.clearObjectCache(options.data.panelCacheId);

      if (!aMessage.parentNode && aMessage._evalCacheId) {
        this.jsterm.clearObjectCache(aMessage._evalCacheId);
      }
    }.bind(this);

    propPanel.panel.addEventListener("popuphiding", onPopupHide, false);
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
    if ((aScriptError.flags & aScriptError.warningFlag) ||
        (aScriptError.flags & aScriptError.strictFlag)) {
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
   * Log network activity.
   *
   * @param object aHttpActivity
   *        The HTTP activity to log.
   * @return nsIDOMElement|undefined
   *         The message element to display in the Web Console output.
   */
  logNetActivity: function WCF_logNetActivity(aConnectionId)
  {
    let networkInfo = this._networkRequests[aConnectionId];
    if (!networkInfo) {
      return;
    }

    let entry = networkInfo.httpActivity.log.entries[0];
    let request = entry.request;

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
      WebConsoleUtils.isMixedHTTPSRequest(request.url,
                                          this.owner.contentLocation);
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

    messageNode._connectionId = entry.connection;

    this.makeOutputMessageLink(messageNode, function WCF_net_message_link() {
      if (!messageNode._panelOpen) {
        this.openNetworkPanel(messageNode, networkInfo.httpActivity);
      }
    }.bind(this));

    networkInfo.node = messageNode;

    this._updateNetMessage(entry.connection);

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
      let viewSourceUtils = this.owner.gViewSourceUtils;
      viewSourceUtils.viewSource(aFileURI, null, this.document);
    }.bind(this));

    return outputNode;
  },

  /**
   * Inform user that the Web Console API has been replaced by a script
   * in a content page.
   *
   * @return nsIDOMElement|undefined
   *         The message element to display in the Web Console output.
   */
  logWarningAboutReplacedAPI: function WCF_logWarningAboutReplacedAPI()
  {
    return this.createMessageNode(CATEGORY_JS, SEVERITY_WARNING,
                                  l10n.getStr("ConsoleAPIDisabled"));
  },

  /**
   * Handle the "WebConsole:NetworkActivity" message coming from the remote Web
   * Console.
   *
   * @param object aMessage
   *        The HTTP activity object. This object needs to hold two properties:
   *        - meta - some metadata about the request log:
   *          - stages - the stages the network request went through.
   *          - discardRequestBody and discardResponseBody - booleans that tell
   *          if the network request/response body was discarded or not.
   *        - log - the request and response information. This is a HAR-like
   *        object. See HUDService-content.js
   *        NetworkMonitor.createActivityObject().
   */
  handleNetworkActivity: function WCF_handleNetworkActivity(aMessage)
  {
    let stage = aMessage.meta.stages[aMessage.meta.stages.length - 1];
    let entry = aMessage.log.entries[0];

    if (stage == "REQUEST_HEADER") {
      let networkInfo = {
        node: null,
        httpActivity: aMessage,
      };

      this._networkRequests[entry.connection] = networkInfo;
      this.outputMessage(CATEGORY_NETWORK, this.logNetActivity,
                         [entry.connection]);
      return;
    }
    else if (!(entry.connection in this._networkRequests)) {
      return;
    }

    let networkInfo = this._networkRequests[entry.connection];
    networkInfo.httpActivity = aMessage;

    if (networkInfo.node) {
      this._updateNetMessage(entry.connection);
    }

    // For unit tests we pass the HTTP activity object to the test callback,
    // once requests complete.
    if (this.owner.lastFinishedRequestCallback &&
        aMessage.meta.stages.indexOf("REQUEST_STOP") > -1 &&
        aMessage.meta.stages.indexOf("TRANSACTION_CLOSE") > -1) {
      this.owner.lastFinishedRequestCallback(aMessage);
    }
  },

  /**
   * Update an output message to reflect the latest state of a network request,
   * given a network connection ID.
   *
   * @private
   * @param string aConnectionId
   *        The connection ID to update.
   */
  _updateNetMessage: function WCF__updateNetMessage(aConnectionId)
  {
    let networkInfo = this._networkRequests[aConnectionId];
    if (!networkInfo || !networkInfo.node) {
      return;
    }

    let messageNode = networkInfo.node;
    let httpActivity = networkInfo.httpActivity;
    let stages = httpActivity.meta.stages;
    let hasTransactionClose = stages.indexOf("TRANSACTION_CLOSE") > -1;
    let hasResponseHeader = stages.indexOf("RESPONSE_HEADER") > -1;
    let entry = httpActivity.log.entries[0];
    let request = entry.request;
    let response = entry.response;

    if (hasTransactionClose || hasResponseHeader) {
      let status = [];
      if (response.httpVersion && response.status) {
        status = [response.httpVersion, response.status, response.statusText];
      }
      if (hasTransactionClose) {
        status.push(l10n.getFormatStr("NetworkPanel.durationMS", [entry.time]));
      }
      let statusText = "[" + status.join(" ") + "]";

      let linkNode = messageNode.querySelector(".webconsole-msg-link");
      let statusNode = linkNode.querySelector(".webconsole-msg-status");
      statusNode.setAttribute("value", statusText);

      messageNode.clipboardText = [request.method, request.url, statusText]
                                  .join(" ");

      if (hasResponseHeader && response.status >= MIN_HTTP_ERROR_CODE &&
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
    let netPanel = new NetworkPanel(this.popupset, aHttpActivity);
    netPanel.linkNode = aNode;
    aNode._netPanel = netPanel;

    let panel = netPanel.panel;
    panel.openPopup(aNode, "after_pointer", 0, 0, false, false);
    panel.sizeTo(450, 500);
    panel.setAttribute("hudId", aHttpActivity.hudId);

    panel.addEventListener("popuphiding", function WCF_netPanel_onHide() {
      panel.removeEventListener("popuphiding", WCF_netPanel_onHide);

      aNode._panelOpen = false;
      aNode._netPanel = null;
    });

    aNode._panelOpen = true;

    return netPanel;
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

    if (!this._outputTimeout) {
      this._outputTimeout =
        this.window.setTimeout(this._flushMessageQueue.bind(this),
                               OUTPUT_INTERVAL);
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
    let timeSinceFlush = Date.now() - this._lastOutputFlush;
    if (this._outputQueue.length > MESSAGES_IN_INTERVAL &&
        timeSinceFlush < THROTTLE_UPDATES) {
      this._outputTimeout =
        this.window.setTimeout(this._flushMessageQueue.bind(this),
                               OUTPUT_INTERVAL);
      return;
    }

    // Determine how many messages we can display now.
    let toDisplay = Math.min(this._outputQueue.length, MESSAGES_IN_INTERVAL);
    if (toDisplay < 1) {
      this._outputTimeout = null;
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
      this._outputTimeout = null;
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
      this._outputTimeout =
        this.window.setTimeout(this._flushMessageQueue.bind(this),
                               OUTPUT_INTERVAL);
    }
    else {
      this._outputTimeout = null;
      this._flushCallback && this._flushCallback();
    }

    this._lastOutputFlush = Date.now();
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
        methodOrNode._evalCacheId && !methodOrNode._panelOpen) {
      this.jsterm.clearObjectCache(methodOrNode._evalCacheId);
    }

    if (category == CATEGORY_NETWORK) {
      let connectionId = null;
      if (methodOrNode == this.logNetActivity) {
        connectionId = args[0];
      }
      else if (typeof methodOrNode != "function") {
        connectionId = methodOrNode._connectionId;
      }
      if (connectionId && connectionId in this._networkRequests) {
        delete this._networkRequests[connectionId];
      }
    }
    else if (category == CATEGORY_WEBDEV &&
             methodOrNode == this.logConsoleAPIMessage) {
      let level = args[0].apiMessage.level;
      if (level == "dir") {
        this.jsterm.clearObjectCache(args[0].objectsCacheId);
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
    aMessageNode.propertyTreeView = null;
    if (tree.view) {
      tree.view.data = null;
    }
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
    if (aNode._evalCacheId && !aNode._panelOpen) {
      this.jsterm.clearObjectCache(aNode._evalCacheId);
    }

    if (aNode.classList.contains("webconsole-msg-cssparser")) {
      let desc = aNode.childNodes[2].textContent;
      let location = "";
      if (aNode.childNodes[4]) {
        location = aNode.childNodes[4].getAttribute("title");
      }
      delete this._cssNodes[desc + location];
    }
    else if (aNode.classList.contains("webconsole-msg-network")) {
      delete this._networkRequests[aNode._connectionId];
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
        str = aBody.resultString;
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
        rootCacheId: body.cacheId,
        panelCacheId: body.cacheId,
        remoteObject: Array.isArray(body.remoteObject) ? body.remoteObject : [],
        remoteObjectProvider: body.remoteObjectProvider,
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
   *        the call information that we need to display.
   */
  _makeConsoleLogMessageBody:
  function WCF__makeConsoleLogMessageBody(aMessage, aContainer, aBody)
  {
    aMessage._evalCacheId = aBody.cacheId;

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

    aBody.remoteObjects.forEach(function(aItem, aIndex) {
      if (aContainer.firstChild) {
        aContainer.appendChild(this.document.createTextNode(" "));
      }

      let text = aBody.argsToString[aIndex];
      if (!Array.isArray(aItem)) {
        aContainer.appendChild(this.document.createTextNode(text));
        return;
      }

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
    // plus an optional line number.
    let text = WebConsoleUtils.abbreviateSourceURL(aSourceURL);
    if (aSourceLine) {
      text += ":" + aSourceLine;
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
      if (aSourceURL == "Scratchpad") {
        let win = Services.wm.getMostRecentWindow("devtools:scratchpad");
        if (win) {
          win.focus();
        }
        return;
      }
      let viewSourceUtils = this.owner.gViewSourceUtils;
      viewSourceUtils.viewSource(aSourceURL, null, this.document, aSourceLine);
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
   */
  copySelectedItems: function WCF_copySelectedItems()
  {
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
        strings.push("[" + timestampString + "] " + item.clipboardText);
      }
    }

    clipboardHelper.copyString(strings.join("\n"), this.document);
  },

  /**
   * Destroy the HUD object. Call this method to avoid memory leaks when the Web
   * Console is closed.
   */
  destroy: function WCF_destroy()
  {
    if (this.jsterm) {
      this.jsterm.destroy();
    }
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
  this.autocompletePopup = new AutocompletePopup(this.hud.owner.chromeDocument);
  this.autocompletePopup.onSelect = this.onAutocompleteSelect.bind(this);
  this.autocompletePopup.onClick = this.acceptProposedCompletion.bind(this);
  this._keyPress = this.keyPress.bind(this);
  this._inputEventHandler = this.inputEventHandler.bind(this);
  this._initUI();
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

  /**
   * Getter for the element that holds the messages we display.
   * @type nsIDOMElement
   */
  get outputNode() this.hud.outputNode,

  COMPLETE_FORWARD: 0,
  COMPLETE_BACKWARD: 1,
  COMPLETE_HINT_ONLY: 2,

  /**
   * Initialize the JSTerminal UI.
   * @private
   */
  _initUI: function JST__initUI()
  {
    let doc = this.hud.document;
    this.completeNode = doc.querySelector(".jsterm-complete-node");
    this.inputNode = doc.querySelector(".jsterm-input-node");
    this.inputNode.addEventListener("keypress", this._keyPress, false);
    this.inputNode.addEventListener("input", this._inputEventHandler, false);
    this.inputNode.addEventListener("keyup", this._inputEventHandler, false);

    this.lastInputValue && this.setInputValue(this.lastInputValue);
  },

  /**
   * Asynchronously evaluate a string in the content process sandbox.
   *
   * @param string aString
   *        String to evaluate in the content process JavaScript sandbox.
   * @param function [aCallback]
   *        Optional function to be invoked when the evaluation result is
   *        received.
   */
  evalInContentSandbox: function JST_evalInContentSandbox(aString, aCallback)
  {
    let message = {
      str: aString,
      resultCacheId: "HUDEval-" + gSequenceId(),
    };

    this.hud.owner.sendMessageToContent("JSTerm:EvalRequest", message, aCallback);

    return message;
  },

  /**
   * The "JSTerm:EvalResult" message handler. This is the JSTerm execution
   * result callback which is invoked whenever JavaScript code evaluation
   * results come from the content process.
   *
   * @private
   * @param function [aCallback]
   *        Optional function to invoke when the evaluation result is added to
   *        the output.
   * @param object aResponse
   *        The JSTerm:EvalResult message received from the content process. See
   *        JSTerm.handleEvalRequest() in HUDService-content.js for further
   *        details.
   * @param object aRequest
   *        The JSTerm:EvalRequest message we sent to the content process.
   * @see JSTerm.handleEvalRequest() in HUDService-content.js
   */
  _executeResultCallback:
  function JST__executeResultCallback(aCallback, aResponse, aRequest)
  {
    let errorMessage = aResponse.errorMessage;
    let resultString = aResponse.resultString;

    // Hide undefined results coming from JSTerm helper functions.
    if (!errorMessage &&
        resultString == "undefined" &&
        aResponse.helperResult &&
        !aResponse.inspectable &&
        !aResponse.helperRawOutput) {
      return;
    }

    let afterNode = aRequest.outputNode;

    if (aCallback) {
      let oldFlushCallback = this.hud._flushCallback;
      this.hud._flushCallback = function() {
        aCallback();
        oldFlushCallback && oldFlushCallback();
        this.hud._flushCallback = oldFlushCallback;
      }.bind(this);
    }

    if (aResponse.errorMessage) {
      this.writeOutput(aResponse.errorMessage, CATEGORY_OUTPUT, SEVERITY_ERROR,
                       afterNode, aResponse.timestamp);
    }
    else if (aResponse.inspectable) {
      let node = this.writeOutputJS(aResponse.resultString,
                                    this._evalOutputClick.bind(this, aResponse),
                                    afterNode, aResponse.timestamp);
      node._evalCacheId = aResponse.childrenCacheId;
    }
    else {
      this.writeOutput(aResponse.resultString, CATEGORY_OUTPUT, SEVERITY_LOG,
                       afterNode, aResponse.timestamp);
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
      this.writeOutput("no value to execute", CATEGORY_OUTPUT, SEVERITY_LOG);
      return;
    }

    let node = this.writeOutput(aExecuteString, CATEGORY_INPUT, SEVERITY_LOG);

    let onResult = this._executeResultCallback.bind(this, aCallback);
    let messageToContent = this.evalInContentSandbox(aExecuteString, onResult);
    messageToContent.outputNode = node;

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
      hud.owner.sendMessageToContent("ConsoleAPI:ClearCache", {});
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
      switch (aEvent.charCode) {
        case 97:
          // control-a
          this.inputNode.setSelectionRange(0, 0);
          aEvent.preventDefault();
          break;
        case 101:
          // control-e
          this.inputNode.setSelectionRange(this.inputNode.value.length,
                                           this.inputNode.value.length);
          aEvent.preventDefault();
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

    let message = {
      id: "HUDComplete-" + gSequenceId(),
      input: this.inputNode.value,
    };

    this.lastCompletion = {
      requestId: message.id,
      completionType: aType,
      value: null,
    };
    let callback = this._receiveAutocompleteProperties.bind(this, aCallback);
    this.hud.owner.sendMessageToContent("JSTerm:Autocomplete", message, callback);
  },

  /**
   * Handler for the "JSTerm:AutocompleteProperties" message. This method takes
   * the completion result received from the content process and updates the UI
   * accordingly.
   *
   * @param function [aCallback=null]
   *        Optional, function to invoke when the completion result is received.
   * @param object aMessage
   *        The JSON message which holds the completion results received from
   *        the content process.
   */
  _receiveAutocompleteProperties:
  function JST__receiveAutocompleteProperties(aCallback, aMessage)
  {
    let inputNode = this.inputNode;
    let inputValue = inputNode.value;
    if (aMessage.input != inputValue ||
        this.lastCompletion.value == inputValue ||
        aMessage.id != this.lastCompletion.requestId) {
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
   * Clear the object cache from the Web Console content instance.
   *
   * @param string aCacheId
   *        The cache ID you want to clear. Multiple objects are cached into one
   *        group which is given an ID.
   */
  clearObjectCache: function JST_clearObjectCache(aCacheId)
  {
    if (this.hud) {
      this.hud.owner.sendMessageToContent("JSTerm:ClearObjectCache",
                                          { cacheId: aCacheId });
    }
  },

  /**
   * The remote object provider allows you to retrieve a given object from
   * a specific cache and have your callback invoked when the desired object is
   * received from the Web Console content instance.
   *
   * @param string aCacheId
   *        Retrieve the desired object from this cache ID.
   * @param string aObjectId
   *        The ID of the object you want.
   * @param string aResultCacheId
   *        The ID of the cache where you want any object references to be
   *        stored into.
   * @param function aCallback
   *        The function you want invoked when the desired object is retrieved.
   */
  remoteObjectProvider:
  function JST_remoteObjectProvider(aCacheId, aObjectId, aResultCacheId,
                                    aCallback) {
    let message = {
      cacheId: aCacheId,
      objectId: aObjectId,
      resultCacheId: aResultCacheId,
    };

    this.hud.owner.sendMessageToContent("JSTerm:GetEvalObject", message, aCallback);
  },

  /**
   * The "JSTerm:InspectObject" remote message handler. This allows the content
   * process to open the Property Panel for a given object.
   *
   * @param object aRequest
   *        The request message from the content process. This message includes
   *        the user input string that was evaluated to inspect an object and
   *        the result object which is to be inspected.
   */
  handleInspectObject: function JST_handleInspectObject(aRequest)
  {
    let options = {
      title: aRequest.input,

      data: {
        rootCacheId: aRequest.objectCacheId,
        panelCacheId: aRequest.objectCacheId,
        remoteObject: aRequest.resultObject,
        remoteObjectProvider: this.remoteObjectProvider.bind(this),
      },
    };

    let propPanel = this.openPropertyPanel(options);
    propPanel.panel.setAttribute("hudId", this.hudId);

    let onPopupHide = function JST__onPopupHide() {
      propPanel.panel.removeEventListener("popuphiding", onPopupHide, false);

      this.clearObjectCache(options.data.panelCacheId);
    }.bind(this);

    propPanel.panel.addEventListener("popuphiding", onPopupHide, false);
  },

  /**
   * The click event handler for evaluation results in the output.
   *
   * @private
   * @param object aResponse
   *        The JSTerm:EvalResult message received from the content process.
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
        // This is where the resultObject children are cached.
        rootCacheId: aResponse.childrenCacheId,
        remoteObject: aResponse.resultObject,
        // This is where all objects retrieved by the panel will be cached.
        panelCacheId: "HUDPanel-" + gSequenceId(),
        remoteObjectProvider: this.remoteObjectProvider.bind(this),
      },
    };

    options.updateButtonCallback = function JST__evalUpdateButton() {
      this.evalInContentSandbox(aResponse.input,
        this._evalOutputUpdatePanelCallback.bind(this, options, propPanel,
                                                 aResponse));
    }.bind(this);

    let propPanel = this.openPropertyPanel(options);
    propPanel.panel.setAttribute("hudId", this.hudId);

    let onPopupHide = function JST__evalInspectPopupHide() {
      propPanel.panel.removeEventListener("popuphiding", onPopupHide, false);

      this.clearObjectCache(options.data.panelCacheId);

      if (!aLinkNode.parentNode && aLinkNode._evalCacheId) {
        this.clearObjectCache(aLinkNode._evalCacheId);
      }
    }.bind(this);

    propPanel.panel.addEventListener("popuphiding", onPopupHide, false);
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

    if (!aNewResponse.inspectable) {
      this.writeOutput(l10n.getStr("JSTerm.updateNotInspectable"), CATEGORY_OUTPUT, SEVERITY_ERROR);
      return;
    }

    this.clearObjectCache(aOptions.data.panelCacheId);
    this.clearObjectCache(aOptions.data.rootCacheId);

    if (aOptions.anchor && aOptions.anchor._evalCacheId) {
      aOptions.anchor._evalCacheId = aNewResponse.childrenCacheId;
    }

    // Update the old response object such that when the panel is reopen, the
    // user sees the new response.
    aOldResponse.id = aNewResponse.id;
    aOldResponse.childrenCacheId = aNewResponse.childrenCacheId;
    aOldResponse.resultObject = aNewResponse.resultObject;
    aOldResponse.resultString = aNewResponse.resultString;

    aOptions.data.rootCacheId = aNewResponse.childrenCacheId;
    aOptions.data.remoteObject = aNewResponse.resultObject;

    // TODO: This updates the value of the tree.
    // However, the states of open nodes is not saved.
    // See bug 586246.
    aPropPanel.treeView.data = aOptions.data;
  },

  /**
   * Destroy the JSTerm object. Call this method to avoid memory leaks.
   */
  destroy: function JST_destroy()
  {
    this.clearCompletion();
    this.clearOutput();

    this.autocompletePopup.destroy();

    this.inputNode.removeEventListener("keypress", this._keyPress, false);
    this.inputNode.removeEventListener("input", this._inputEventHandler, false);
    this.inputNode.removeEventListener("keyup", this._inputEventHandler, false);
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
      case "cmd_selectAll":
        this.selectAll();
        break;
    }
  }
};

function gSequenceId()
{
  return gSequenceId.n++;
}
gSequenceId.n = 0;

