/* -*- Mode: js2; js2-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const CONSOLEAPI_CLASS_ID = "{b49c18f8-3379-4fc0-8c90-d7772c1a9ff3}";

const MIXED_CONTENT_LEARN_MORE = "https://developer.mozilla.org/en/Security/MixedContent";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

var EXPORTED_SYMBOLS = ["HUDService", "ConsoleUtils"];

XPCOMUtils.defineLazyServiceGetter(this, "clipboardHelper",
                                   "@mozilla.org/widget/clipboardhelper;1",
                                   "nsIClipboardHelper");

XPCOMUtils.defineLazyModuleGetter(this, "PropertyPanel",
                                  "resource:///modules/PropertyPanel.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PropertyTreeView",
                                  "resource:///modules/PropertyPanel.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AutocompletePopup",
                                  "resource:///modules/AutocompletePopup.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetworkPanel",
                                  "resource:///modules/NetworkPanel.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "WebConsoleUtils",
                                  "resource:///modules/WebConsoleUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "l10n", function() {
  return WebConsoleUtils.l10n;
});

function LogFactory(aMessagePrefix)
{
  function log(aMessage) {
    var _msg = aMessagePrefix + " " + aMessage + "\n";
    dump(_msg);
  }
  return log;
}

let log = LogFactory("*** HUDService:");

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

// The HTML namespace.
const HTML_NS = "http://www.w3.org/1999/xhtml";

// The XUL namespace.
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

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

// Possible directions that can be passed to HUDService.animate().
const ANIMATE_OUT = 0;
const ANIMATE_IN = 1;

// Constants used for defining the direction of JSTerm input history navigation.
const HISTORY_BACK = -1;
const HISTORY_FORWARD = 1;

// Minimum console height, in pixels.
const MINIMUM_CONSOLE_HEIGHT = 150;

// Minimum page height, in pixels. This prevents the Web Console from
// remembering a height that covers the whole page.
const MINIMUM_PAGE_HEIGHT = 50;

// The default console height, as a ratio from the content window inner height.
const DEFAULT_CONSOLE_HEIGHT = 0.33;

// This script is inserted into the content process.
const CONTENT_SCRIPT_URL = "chrome://browser/content/devtools/HUDService-content.js";

const ERRORS = { LOG_MESSAGE_MISSING_ARGS:
                 "Missing arguments: aMessage, aConsoleNode and aMessageNode are required.",
                 CANNOT_GET_HUD: "Cannot getHeads Up Display with provided ID",
                 MISSING_ARGS: "Missing arguments",
                 LOG_OUTPUT_FAILED: "Log Failure: Could not append messageNode to outputNode",
};

// The indent of a console group in pixels.
const GROUP_INDENT = 12;

// The pref prefix for webconsole filters
const PREFS_PREFIX = "devtools.webconsole.filter.";

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

///////////////////////////////////////////////////////////////////////////
//// Helper for creating the network panel.

/**
 * Creates a DOMNode and sets all the attributes of aAttributes on the created
 * element.
 *
 * @param nsIDOMDocument aDocument
 *        Document to create the new DOMNode.
 * @param string aTag
 *        Name of the tag for the DOMNode.
 * @param object aAttributes
 *        Attributes set on the created DOMNode.
 *
 * @returns nsIDOMNode
 */
function createElement(aDocument, aTag, aAttributes)
{
  let node = aDocument.createElement(aTag);
  if (aAttributes) {
    for (let attr in aAttributes) {
      node.setAttribute(attr, aAttributes[attr]);
    }
  }
  return node;
}

///////////////////////////////////////////////////////////////////////////
//// Private utility functions for the HUD service

/**
 * Ensures that the number of message nodes of type aCategory don't exceed that
 * category's line limit by removing old messages as needed.
 *
 * @param aHUDId aHUDId
 *        The HeadsUpDisplay ID.
 * @param integer aCategory
 *        The category of message nodes to limit.
 * @return number
 *         The number of removed nodes.
 */
function pruneConsoleOutputIfNecessary(aHUDId, aCategory)
{
  let hudRef = HUDService.getHudReferenceById(aHUDId);
  let outputNode = hudRef.outputNode;
  let logLimit = hudRef.logLimitForCategory(aCategory);

  let messageNodes = outputNode.getElementsByClassName("webconsole-msg-" +
      CATEGORY_CLASS_FRAGMENTS[aCategory]);
  let n = Math.max(0, messageNodes.length - logLimit);
  let toRemove = Array.prototype.slice.call(messageNodes, 0, n);
  toRemove.forEach(hudRef.removeOutputMessage, hudRef);

  return n;
}

///////////////////////////////////////////////////////////////////////////
//// The HUD service

function HUD_SERVICE()
{
  // These methods access the "this" object, but they're registered as
  // event listeners. So we hammer in the "this" binding.
  this.onTabClose = this.onTabClose.bind(this);
  this.onTabSelect = this.onTabSelect.bind(this);
  this.onWindowUnload = this.onWindowUnload.bind(this);

  // Remembers the last console height, in pixels.
  this.lastConsoleHeight = Services.prefs.getIntPref("devtools.hud.height");

  /**
   * Each HeadsUpDisplay has a set of filter preferences
   */
  this.filterPrefs = {};

  /**
   * Keeps a reference for each HeadsUpDisplay that is created
   */
  this.hudReferences = {};
};

HUD_SERVICE.prototype =
{
  /**
   * getter for UI commands to be used by the frontend
   *
   * @returns object
   */
  get consoleUI() {
    return HeadsUpDisplayUICommands;
  },

  /**
   * The sequencer is a generator (after initialization) that returns unique
   * integers
   */
  sequencer: null,

  /**
   * Firefox-specific current tab getter
   *
   * @returns nsIDOMWindow
   */
  currentContext: function HS_currentContext() {
    return Services.wm.getMostRecentWindow("navigator:browser");
  },

  /**
   * Activate a HeadsUpDisplay for the given tab context.
   *
   * @param nsIDOMElement aTab
   *        The xul:tab element.
   * @param boolean aAnimated
   *        True if you want to animate the opening of the Web console.
   * @return object
   *         The new HeadsUpDisplay instance.
   */
  activateHUDForContext: function HS_activateHUDForContext(aTab, aAnimated)
  {
    let hudId = "hud_" + aTab.linkedPanel;
    if (hudId in this.hudReferences) {
      return this.hudReferences[hudId];
    }

    this.wakeup();

    let window = aTab.ownerDocument.defaultView;
    let gBrowser = window.gBrowser;

    // TODO: check that this works as intended
    gBrowser.tabContainer.addEventListener("TabClose", this.onTabClose, false);
    gBrowser.tabContainer.addEventListener("TabSelect", this.onTabSelect, false);
    window.addEventListener("unload", this.onWindowUnload, false);

    this.registerDisplay(hudId);

    let hud = new HeadsUpDisplay(aTab);
    this.hudReferences[hudId] = hud;

    // register the controller to handle "select all" properly
    this.createController(window);

    if (!aAnimated || hud.consolePanel) {
      this.disableAnimation(hudId);
    }

    HeadsUpDisplayUICommands.refreshCommand();

    return hud;
  },

  /**
   * Deactivate a HeadsUpDisplay for the given tab context.
   *
   * @param nsIDOMElement aTab
   *        The xul:tab element you want to enable the Web Console for.
   * @param boolean aAnimated
   *        True if you want to animate the closing of the Web console.
   * @return void
   */
  deactivateHUDForContext: function HS_deactivateHUDForContext(aTab, aAnimated)
  {
    let hudId = "hud_" + aTab.linkedPanel;
    if (!(hudId in this.hudReferences)) {
      return;
    }

    if (!aAnimated) {
      this.storeHeight(hudId);
    }

    this.unregisterDisplay(hudId);

    let contentWindow = aTab.linkedBrowser.contentWindow;
    contentWindow.focus();

    HeadsUpDisplayUICommands.refreshCommand();

    let id = WebConsoleUtils.supportsString(hudId);
    Services.obs.notifyObservers(id, "web-console-destroyed", null);
  },

  /**
   * get a unique ID from the sequence generator
   *
   * @returns integer
   */
  sequenceId: function HS_sequencerId()
  {
    if (!this.sequencer) {
      this.sequencer = this.createSequencer(-1);
    }
    return this.sequencer.next();
  },

  /**
   * get the default filter prefs
   *
   * @param string aHUDId
   * @returns JS Object
   */
  getDefaultFilterPrefs: function HS_getDefaultFilterPrefs(aHUDId) {
    return this.filterPrefs[aHUDId];
  },

  /**
   * get the current filter prefs
   *
   * @param string aHUDId
   * @returns JS Object
   */
  getFilterPrefs: function HS_getFilterPrefs(aHUDId) {
    return this.filterPrefs[aHUDId];
  },

  /**
   * get the filter state for a specific toggle button on a heads up display
   *
   * @param string aHUDId
   * @param string aToggleType
   * @returns boolean
   */
  getFilterState: function HS_getFilterState(aHUDId, aToggleType)
  {
    if (!aHUDId) {
      return false;
    }
    try {
      var bool = this.filterPrefs[aHUDId][aToggleType];
      return bool;
    }
    catch (ex) {
      return false;
    }
  },

  /**
   * set the filter state for a specific toggle button on a heads up display
   *
   * @param string aHUDId
   * @param string aToggleType
   * @param boolean aState
   * @returns void
   */
  setFilterState: function HS_setFilterState(aHUDId, aToggleType, aState)
  {
    this.filterPrefs[aHUDId][aToggleType] = aState;
    this.adjustVisibilityForMessageType(aHUDId, aToggleType, aState);
    Services.prefs.setBoolPref(PREFS_PREFIX + aToggleType, aState);
  },

  /**
   * Splits the given console messages into groups based on their timestamps.
   *
   * @param nsIDOMNode aOutputNode
   *        The output node to alter.
   * @returns void
   */
  regroupOutput: function HS_regroupOutput(aOutputNode)
  {
    // Go through the nodes and adjust the placement of "webconsole-new-group"
    // classes.
    let nodes = aOutputNode.querySelectorAll(".hud-msg-node" +
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
   * Turns the display of log nodes on and off appropriately to reflect the
   * adjustment of the message type filter named by @aPrefKey.
   *
   * @param string aHUDId
   *        The ID of the HUD to alter.
   * @param string aPrefKey
   *        The preference key for the message type being filtered: one of the
   *        values in the MESSAGE_PREFERENCE_KEYS table.
   * @param boolean aState
   *        True if the filter named by @aMessageType is being turned on; false
   *        otherwise.
   * @returns void
   */
  adjustVisibilityForMessageType:
  function HS_adjustVisibilityForMessageType(aHUDId, aPrefKey, aState)
  {
    let outputNode = this.getHudReferenceById(aHUDId).outputNode;
    let doc = outputNode.ownerDocument;

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

    this.regroupOutput(outputNode);
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
  stringMatchesFilters: function stringMatchesFilters(aString, aFilter)
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
   * adjustment of the search string.
   *
   * @param string aHUDId
   *        The ID of the HUD to alter.
   * @param string aSearchString
   *        The new search string.
   * @returns void
   */
  adjustVisibilityOnSearchStringChange:
  function HS_adjustVisibilityOnSearchStringChange(aHUDId, aSearchString)
  {
    let outputNode = this.getHudReferenceById(aHUDId).outputNode;

    let nodes = outputNode.getElementsByClassName("hud-msg-node");

    for (let i = 0, n = nodes.length; i < n; ++i) {
      let node = nodes[i];

      // hide nodes that match the strings
      let text = node.clipboardText;

      // if the text matches the words in aSearchString...
      if (this.stringMatchesFilters(text, aSearchString)) {
        node.classList.remove("hud-filtered-by-string");
      }
      else {
        node.classList.add("hud-filtered-by-string");
      }
    }

    this.regroupOutput(outputNode);
  },

  /**
   * Register a new Heads Up Display
   *
   * @returns void
   */
  registerDisplay: function HS_registerDisplay(aHUDId)
  {
    // register a display DOM node Id with the service.
    if (!aHUDId){
      throw new Error(ERRORS.MISSING_ARGS);
    }
    this.filterPrefs[aHUDId] = {
      network: Services.prefs.getBoolPref(PREFS_PREFIX + "network"),
      networkinfo: Services.prefs.getBoolPref(PREFS_PREFIX + "networkinfo"),
      csserror: Services.prefs.getBoolPref(PREFS_PREFIX + "csserror"),
      cssparser: Services.prefs.getBoolPref(PREFS_PREFIX + "cssparser"),
      exception: Services.prefs.getBoolPref(PREFS_PREFIX + "exception"),
      jswarn: Services.prefs.getBoolPref(PREFS_PREFIX + "jswarn"),
      error: Services.prefs.getBoolPref(PREFS_PREFIX + "error"),
      info: Services.prefs.getBoolPref(PREFS_PREFIX + "info"),
      warn: Services.prefs.getBoolPref(PREFS_PREFIX + "warn"),
      log: Services.prefs.getBoolPref(PREFS_PREFIX + "log"),
    };
  },

  /**
   * When a display is being destroyed, unregister it first
   *
   * @param string aHUDId
   *        The ID of a HUD.
   * @returns void
   */
  unregisterDisplay: function HS_unregisterDisplay(aHUDId)
  {
    let hud = this.getHudReferenceById(aHUDId);
    let document = hud.chromeDocument;

    hud.destroy();

    delete this.hudReferences[aHUDId];

    if (Object.keys(this.hudReferences).length == 0) {
      let autocompletePopup = document.
                              getElementById("webConsole_autocompletePopup");
      if (autocompletePopup) {
        autocompletePopup.parentNode.removeChild(autocompletePopup);
      }

      let window = document.defaultView;

      window.removeEventListener("unload", this.onWindowUnload, false);

      let gBrowser = window.gBrowser;
      let tabContainer = gBrowser.tabContainer;
      tabContainer.removeEventListener("TabClose", this.onTabClose, false);
      tabContainer.removeEventListener("TabSelect", this.onTabSelect, false);

      this.suspend();
    }
  },

  /**
   * "Wake up" the Web Console activity. This is called when the first Web
   * Console is open. This method initializes the various observers we have.
   *
   * @returns void
   */
  wakeup: function HS_wakeup()
  {
    if (Object.keys(this.hudReferences).length > 0) {
      return;
    }

    WebConsoleObserver.init();
  },

  /**
   * Suspend Web Console activity. This is called when all Web Consoles are
   * closed.
   *
   * @returns void
   */
  suspend: function HS_suspend()
  {
    delete this.defaultFilterPrefs;
    delete this.lastFinishedRequestCallback;

    WebConsoleObserver.uninit();
  },

  /**
   * Shutdown all HeadsUpDisplays on quit-application-granted.
   *
   * @returns void
   */
  shutdown: function HS_shutdown()
  {
    for (let hud of this.hudReferences) {
      this.deactivateHUDForContext(hud.tab, false);
    }
  },

  /**
   * Returns the HeadsUpDisplay object associated to a content window.
   *
   * @param nsIDOMWindow aContentWindow
   * @returns object
   */
  getHudByWindow: function HS_getHudByWindow(aContentWindow)
  {
    let hudId = this.getHudIdByWindow(aContentWindow);
    return hudId ? this.hudReferences[hudId] : null;
  },

  /**
   * Returns the hudId that is corresponding to the hud activated for the
   * passed aContentWindow. If there is no matching hudId null is returned.
   *
   * @param nsIDOMWindow aContentWindow
   * @returns string or null
   */
  getHudIdByWindow: function HS_getHudIdByWindow(aContentWindow)
  {
    let window = this.currentContext();
    let index =
      window.gBrowser.getBrowserIndexForDocument(aContentWindow.document);
    if (index == -1) {
      return null;
    }

    let tab = window.gBrowser.tabs[index];
    let hudId = "hud_" + tab.linkedPanel;
    return hudId in this.hudReferences ? hudId : null;
  },

  /**
   * Returns the hudReference for a given id.
   *
   * @param string aId
   * @returns Object
   */
  getHudReferenceById: function HS_getHudReferenceById(aId)
  {
    return aId in this.hudReferences ? this.hudReferences[aId] : null;
  },

  /**
   * Get the current filter string for the HeadsUpDisplay
   *
   * @param string aHUDId
   * @returns string
   */
  getFilterStringByHUDId: function HS_getFilterStringbyHUDId(aHUDId) {
    return this.getHudReferenceById(aHUDId).filterBox.value;
  },

  /**
   * Update the filter text in the internal tracking object for all
   * filter strings
   *
   * @param nsIDOMNode aTextBoxNode
   * @returns void
   */
  updateFilterText: function HS_updateFiltertext(aTextBoxNode)
  {
    var hudId = aTextBoxNode.getAttribute("hudId");
    this.adjustVisibilityOnSearchStringChange(hudId, aTextBoxNode.value);
  },

  /**
   * Assign a function to this property to listen for every request that
   * completes. Used by unit tests. The callback takes one argument: the HTTP
   * activity object as received from the remote Web Console.
   *
   * @type function
   */
  lastFinishedRequestCallback: null,

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
  openNetworkPanel: function HS_openNetworkPanel(aNode, aHttpActivity)
  {
    let doc = aNode.ownerDocument;
    let parent = doc.getElementById("mainPopupSet");
    let netPanel = new NetworkPanel(parent, aHttpActivity);
    netPanel.linkNode = aNode;
    aNode._netPanel = netPanel;

    let panel = netPanel.panel;
    panel.openPopup(aNode, "after_pointer", 0, 0, false, false);
    panel.sizeTo(450, 500);
    panel.setAttribute("hudId", aHttpActivity.hudId);

    panel.addEventListener("popuphiding", function HS_netPanel_onHide() {
      panel.removeEventListener("popuphiding", HS_netPanel_onHide);

      aNode._panelOpen = false;
      aNode._netPanel = null;
    });

    aNode._panelOpen = true;

    return netPanel;
  },

  /**
   * Creates a generator that always returns a unique number for use in the
   * indexes
   *
   * @returns Generator
   */
  createSequencer: function HS_createSequencer(aInt)
  {
    function sequencer(aInt)
    {
      while(1) {
        aInt++;
        yield aInt;
      }
    }
    return sequencer(aInt);
  },

  /**
   * onTabClose event handler function
   *
   * @param aEvent
   * @returns void
   */
  onTabClose: function HS_onTabClose(aEvent)
  {
    this.deactivateHUDForContext(aEvent.target, false);
  },

  /**
   * onTabSelect event handler function
   *
   * @param aEvent
   * @returns void
   */
  onTabSelect: function HS_onTabSelect(aEvent)
  {
    HeadsUpDisplayUICommands.refreshCommand();
  },

  /**
   * Called whenever a browser window closes. Cleans up any consoles still
   * around.
   *
   * @param nsIDOMEvent aEvent
   *        The dispatched event.
   * @returns void
   */
  onWindowUnload: function HS_onWindowUnload(aEvent)
  {
    let window = aEvent.target.defaultView;

    window.removeEventListener("unload", this.onWindowUnload, false);

    let gBrowser = window.gBrowser;
    let tabContainer = gBrowser.tabContainer;

    tabContainer.removeEventListener("TabClose", this.onTabClose, false);
    tabContainer.removeEventListener("TabSelect", this.onTabSelect, false);

    let tab = tabContainer.firstChild;
    while (tab != null) {
      this.deactivateHUDForContext(tab, false);
      tab = tab.nextSibling;
    }

    if (window.webConsoleCommandController) {
      window.controllers.removeController(window.webConsoleCommandController);
      window.webConsoleCommandController = null;
    }
  },

  /**
   * Adds the command controller to the XUL window if it's not already present.
   *
   * @param nsIDOMWindow aWindow
   *        The browser XUL window.
   * @returns void
   */
  createController: function HS_createController(aWindow)
  {
    if (!aWindow.webConsoleCommandController) {
      aWindow.webConsoleCommandController = new CommandController(aWindow);
      aWindow.controllers.insertControllerAt(0,
        aWindow.webConsoleCommandController);
    }
  },

  /**
   * Animates the Console appropriately.
   *
   * @param string aHUDId The ID of the console.
   * @param string aDirection Whether to animate the console appearing
   *        (ANIMATE_IN) or disappearing (ANIMATE_OUT).
   * @param function aCallback An optional callback, which will be called with
   *        the "transitionend" event passed as a parameter once the animation
   *        finishes.
   */
  animate: function HS_animate(aHUDId, aDirection, aCallback)
  {
    let hudBox = this.getHudReferenceById(aHUDId).HUDBox;
    if (!hudBox.classList.contains("animated")) {
      if (aCallback) {
        aCallback();
      }
      return;
    }

    switch (aDirection) {
      case ANIMATE_OUT:
        hudBox.style.height = 0;
        break;
      case ANIMATE_IN:
        this.resetHeight(aHUDId);
        break;
    }

    if (aCallback) {
      hudBox.addEventListener("transitionend", aCallback, false);
    }
  },

  /**
   * Disables all animation for a console, for unit testing. After this call,
   * the console will instantly take on a reasonable height, and the close
   * animation will not occur.
   *
   * @param string aHUDId The ID of the console.
   */
  disableAnimation: function HS_disableAnimation(aHUDId)
  {
    let hudBox = HUDService.hudReferences[aHUDId].HUDBox;
    if (hudBox.classList.contains("animated")) {
      hudBox.classList.remove("animated");
      this.resetHeight(aHUDId);
    }
  },

  /**
   * Reset the height of the Web Console.
   *
   * @param string aHUDId The ID of the Web Console.
   */
  resetHeight: function HS_resetHeight(aHUDId)
  {
    let HUD = this.hudReferences[aHUDId];
    let innerHeight = HUD.browser.clientHeight;
    let chromeWindow = HUD.chromeWindow;
    if (!HUD.consolePanel) {
      let splitterStyle = chromeWindow.getComputedStyle(HUD.splitter, null);
      innerHeight += parseInt(splitterStyle.height) +
                     parseInt(splitterStyle.borderTopWidth) +
                     parseInt(splitterStyle.borderBottomWidth);
    }

    let boxStyle = chromeWindow.getComputedStyle(HUD.HUDBox, null);
    innerHeight += parseInt(boxStyle.height) +
                   parseInt(boxStyle.borderTopWidth) +
                   parseInt(boxStyle.borderBottomWidth);

    let height = this.lastConsoleHeight > 0 ? this.lastConsoleHeight :
      Math.ceil(innerHeight * DEFAULT_CONSOLE_HEIGHT);

    if ((innerHeight - height) < MINIMUM_PAGE_HEIGHT) {
      height = innerHeight - MINIMUM_PAGE_HEIGHT;
    }

    if (isNaN(height) || height < MINIMUM_CONSOLE_HEIGHT) {
      height = MINIMUM_CONSOLE_HEIGHT;
    }

    HUD.HUDBox.style.height = height + "px";
  },

  /**
   * Remember the height of the given Web Console, such that it can later be
   * reused when other Web Consoles are open.
   *
   * @param string aHUDId The ID of the Web Console.
   */
  storeHeight: function HS_storeHeight(aHUDId)
  {
    let hudBox = this.hudReferences[aHUDId].HUDBox;
    let window = hudBox.ownerDocument.defaultView;
    let style = window.getComputedStyle(hudBox, null);
    let height = parseInt(style.height);
    height += parseInt(style.borderTopWidth);
    height += parseInt(style.borderBottomWidth);
    this.lastConsoleHeight = height;

    let pref = Services.prefs.getIntPref("devtools.hud.height");
    if (pref > -1) {
      Services.prefs.setIntPref("devtools.hud.height", height);
    }
  },

  /**
   * Copies the selected items to the system clipboard.
   *
   * @param nsIDOMNode aOutputNode
   *        The output node.
   * @returns void
   */
  copySelectedItems: function HS_copySelectedItems(aOutputNode)
  {
    // Gather up the selected items and concatenate their clipboard text.

    let strings = [];
    let newGroup = false;

    let children = aOutputNode.children;

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
    clipboardHelper.copyString(strings.join("\n"), aOutputNode.ownerDocument);
  }
};

//////////////////////////////////////////////////////////////////////////
// HeadsUpDisplay
//////////////////////////////////////////////////////////////////////////

/**
 * HeadsUpDisplay is an interactive console initialized *per tab*  that
 * displays console log data as well as provides an interactive terminal to
 * manipulate the current tab's document content.
 *
 * @param nsIDOMElement aTab
 *        The xul:tab for which you want the HeadsUpDisplay object.
 */
function HeadsUpDisplay(aTab)
{
  this.tab = aTab;
  this.hudId = "hud_" + this.tab.linkedPanel;
  this.chromeDocument = this.tab.ownerDocument;
  this.chromeWindow = this.chromeDocument.defaultView;
  this.notificationBox = this.chromeDocument.getElementById(this.tab.linkedPanel);
  this.browser = this.tab.linkedBrowser;
  this.messageManager = this.browser.messageManager;

  // Track callback functions registered for specific async requests sent to the
  // content process.
  this.asyncRequests = {};

  // create a panel dynamically and attach to the parentNode
  this.createHUD();

  this._outputQueue = [];
  this._pruneCategoriesQueue = {};

  // create the JSTerm input element
  this.jsterm = new JSTerm(this);
  this.jsterm.inputNode.focus();

  // A cache for tracking repeated CSS Nodes.
  this.cssNodes = {};

  this._networkRequests = {};

  this._setupMessageManager();
}

HeadsUpDisplay.prototype = {
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
   * Message names that the HUD listens for. These messages come from the remote
   * Web Console content script.
   *
   * @private
   * @type array
   */
  _messageListeners: ["JSTerm:EvalObject", "WebConsole:ConsoleAPI",
    "WebConsole:CachedMessages", "WebConsole:PageError", "JSTerm:EvalResult",
    "JSTerm:AutocompleteProperties", "JSTerm:ClearOutput",
    "JSTerm:InspectObject", "WebConsole:NetworkActivity",
    "WebConsole:FileActivity", "WebConsole:LocationChange",
    "JSTerm:NonNativeConsoleAPI"],

  consolePanel: null,

  contentLocation: "",

  /**
   * The nesting depth of the currently active console group.
   */
  groupDepth: 0,

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

    this.sendMessageToContent("WebConsole:SetPreferences", message);
  },

  get mainPopupSet()
  {
    return this.chromeDocument.getElementById("mainPopupSet");
  },

  /**
   * Create a panel to open the web console if it should float above
   * the content in its own window.
   */
  createOwnWindowPanel: function HUD_createOwnWindowPanel()
  {
    if (this.uiInOwnWindow) {
      return this.consolePanel;
    }

    let width = 0;
    try {
      width = Services.prefs.getIntPref("devtools.webconsole.width");
    }
    catch (ex) {}

    if (width < 1) {
      width = this.HUDBox.clientWidth || this.chromeWindow.innerWidth;
    }

    let height = this.HUDBox.clientHeight;

    let top = 0;
    try {
      top = Services.prefs.getIntPref("devtools.webconsole.top");
    }
    catch (ex) {}

    let left = 0;
    try {
      left = Services.prefs.getIntPref("devtools.webconsole.left");
    }
    catch (ex) {}

    let panel = this.chromeDocument.createElementNS(XUL_NS, "panel");

    let config = { id: "console_window_" + this.hudId,
                   label: this.getPanelTitle(),
                   titlebar: "normal",
                   noautohide: "true",
                   norestorefocus: "true",
                   close: "true",
                   flex: "1",
                   hudId: this.hudId,
                   width: width,
                   position: "overlap",
                   top: top,
                   left: left,
                 };

    for (let attr in config) {
      panel.setAttribute(attr, config[attr]);
    }

    panel.classList.add("web-console-panel");

    let onPopupShown = (function HUD_onPopupShown() {
      panel.removeEventListener("popupshown", onPopupShown, false);

      // Make sure that the HUDBox size updates when the panel is resized.

      let height = panel.clientHeight;

      this.HUDBox.style.height = "auto";
      this.HUDBox.setAttribute("flex", "1");

      panel.setAttribute("height", height);

      // Scroll the outputNode back to the last location.
      if (lastIndex > -1 && lastIndex < this.outputNode.getRowCount()) {
        this.outputNode.ensureIndexIsVisible(lastIndex);
      }

      this.jsterm.inputNode.focus();
    }).bind(this);

    panel.addEventListener("popupshown", onPopupShown,false);

    let onPopupHidden = (function HUD_onPopupHidden(aEvent) {
      if (aEvent.target != panel) {
        return;
      }

      panel.removeEventListener("popuphidden", onPopupHidden, false);

      let width = 0;
      try {
        width = Services.prefs.getIntPref("devtools.webconsole.width");
      }
      catch (ex) { }

      if (width > 0) {
        Services.prefs.setIntPref("devtools.webconsole.width", panel.clientWidth);
      }

      // Are we destroying the HUD or repositioning it?
      if (this.consoleWindowUnregisterOnHide) {
        HUDService.deactivateHUDForContext(this.tab, false);
      }
    }).bind(this);

    panel.addEventListener("popuphidden", onPopupHidden, false);

    let lastIndex = -1;

    if (this.outputNode.getIndexOfFirstVisibleRow) {
      lastIndex = this.outputNode.getIndexOfFirstVisibleRow() +
                  this.outputNode.getNumberOfVisibleRows() - 1;
    }

    if (this.splitter.parentNode) {
      this.splitter.parentNode.removeChild(this.splitter);
    }
    panel.appendChild(this.HUDBox);

    let space = this.chromeDocument.createElement("spacer");
    space.setAttribute("flex", "1");

    let bottomBox = this.chromeDocument.createElement("hbox");
    space.setAttribute("flex", "1");

    let resizer = this.chromeDocument.createElement("resizer");
    resizer.setAttribute("dir", "bottomend");
    resizer.setAttribute("element", config.id);

    bottomBox.appendChild(space);
    bottomBox.appendChild(resizer);

    panel.appendChild(bottomBox);

    this.mainPopupSet.appendChild(panel);

    Services.prefs.setCharPref("devtools.webconsole.position", "window");

    panel.openPopup(null, "overlay", left, top, false, false);

    this.consolePanel = panel;
    this.consoleWindowUnregisterOnHide = true;

    return panel;
  },

  /**
   * Retrieve the Web Console panel title.
   *
   * @return string
   *         The Web Console panel title.
   */
  getPanelTitle: function HUD_getPanelTitle()
  {
    return l10n.getFormatStr("webConsoleWindowTitleAndURL", [this.contentLocation]);
  },

  positions: {
    above: 0, // the childNode index
    below: 2,
    window: null
  },

  consoleWindowUnregisterOnHide: true,

  /**
   * Re-position the console
   */
  positionConsole: function HUD_positionConsole(aPosition)
  {
    if (!(aPosition in this.positions)) {
      throw new Error("Incorrect argument: " + aPosition +
        ". Cannot position Web Console");
    }

    if (aPosition == "window") {
      let closeButton = this.consoleFilterToolbar.
        querySelector(".webconsole-close-button");
      closeButton.setAttribute("hidden", "true");
      this.createOwnWindowPanel();
      this.positionMenuitems.window.setAttribute("checked", true);
      if (this.positionMenuitems.last) {
        this.positionMenuitems.last.setAttribute("checked", false);
      }
      this.positionMenuitems.last = this.positionMenuitems[aPosition];
      this.uiInOwnWindow = true;
      return;
    }

    let height = this.HUDBox.clientHeight;

    // get the node position index
    let nodeIdx = this.positions[aPosition];
    let nBox = this.notificationBox;
    let node = nBox.childNodes[nodeIdx];

    // check to see if console is already positioned in aPosition
    if (node == this.HUDBox) {
      return;
    }

    let lastIndex = -1;

    if (this.outputNode.getIndexOfFirstVisibleRow) {
      lastIndex = this.outputNode.getIndexOfFirstVisibleRow() +
                  this.outputNode.getNumberOfVisibleRows() - 1;
    }

    // remove the console and splitter and reposition
    if (this.splitter.parentNode) {
      this.splitter.parentNode.removeChild(this.splitter);
    }

    if (aPosition == "below") {
      nBox.appendChild(this.splitter);
      nBox.appendChild(this.HUDBox);
    }
    else {
      nBox.insertBefore(this.splitter, node);
      nBox.insertBefore(this.HUDBox, this.splitter);
    }

    this.positionMenuitems[aPosition].setAttribute("checked", true);
    if (this.positionMenuitems.last) {
      this.positionMenuitems.last.setAttribute("checked", false);
    }
    this.positionMenuitems.last = this.positionMenuitems[aPosition];

    Services.prefs.setCharPref("devtools.webconsole.position", aPosition);

    if (lastIndex > -1 && lastIndex < this.outputNode.getRowCount()) {
      this.outputNode.ensureIndexIsVisible(lastIndex);
    }

    let closeButton = this.consoleFilterToolbar.
      getElementsByClassName("webconsole-close-button")[0];
    closeButton.removeAttribute("hidden");

    this.uiInOwnWindow = false;
    if (this.consolePanel) {
      // must destroy the consolePanel
      this.consoleWindowUnregisterOnHide = false;
      this.consolePanel.hidePopup();
      this.consolePanel.parentNode.removeChild(this.consolePanel);
      this.consolePanel = null;   // remove this as we're not in panel anymore
      this.HUDBox.removeAttribute("flex");
      this.HUDBox.removeAttribute("height");
      this.HUDBox.style.height = height + "px";
    }

    if (this.jsterm) {
      this.jsterm.inputNode.focus();
    }
  },

  /**
   * The JSTerm object that contains the console's inputNode
   *
   */
  jsterm: null,

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
  function HUD__displayCachedConsoleMessages(aRemoteMessages)
  {
    if (!aRemoteMessages.length) {
      return;
    }

    aRemoteMessages.forEach(function(aMessage) {
      switch (aMessage._type) {
        case "PageError": {
          let category = this.categoryForScriptError(aMessage);
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
   * Shortcut to make XUL nodes
   *
   * @param string aTag
   * @returns nsIDOMNode
   */
  makeXULNode:
  function HUD_makeXULNode(aTag)
  {
    return this.chromeDocument.createElement(aTag);
  },

  /**
   * Build the UI of each HeadsUpDisplay
   *
   * @returns nsIDOMNode
   */
  makeHUDNodes: function HUD_makeHUDNodes()
  {
    this.splitter = this.makeXULNode("splitter");
    this.splitter.setAttribute("class", "hud-splitter");

    this.HUDBox = this.makeXULNode("vbox");
    this.HUDBox.setAttribute("id", this.hudId);
    this.HUDBox.setAttribute("class", "hud-box animated");
    this.HUDBox.style.height = 0;
    this.HUDBox.lastTimestamp = 0;

    let outerWrap = this.makeXULNode("vbox");
    outerWrap.setAttribute("class", "hud-outer-wrapper");
    outerWrap.setAttribute("flex", "1");

    let consoleCommandSet = this.makeXULNode("commandset");
    outerWrap.appendChild(consoleCommandSet);

    let consoleWrap = this.makeXULNode("vbox");
    this.consoleWrap = consoleWrap;
    consoleWrap.setAttribute("class", "hud-console-wrapper");
    consoleWrap.setAttribute("flex", "1");

    this.filterSpacer = this.makeXULNode("spacer");
    this.filterSpacer.setAttribute("flex", "1");

    this.filterBox = this.makeXULNode("textbox");
    this.filterBox.setAttribute("class", "compact hud-filter-box");
    this.filterBox.setAttribute("hudId", this.hudId);
    this.filterBox.setAttribute("placeholder", l10n.getStr("stringFilter"));
    this.filterBox.setAttribute("type", "search");

    this.setFilterTextBoxEvents();

    this.createConsoleMenu(this.consoleWrap);

    this.filterPrefs = HUDService.getDefaultFilterPrefs(this.hudId);

    let consoleFilterToolbar = this.makeFilterToolbar();
    consoleFilterToolbar.setAttribute("id", "viewGroup");
    this.consoleFilterToolbar = consoleFilterToolbar;

    this.hintNode = this.makeXULNode("vbox");
    this.hintNode.setAttribute("class", "gcliterm-hint-node");

    let hintParentNode = this.makeXULNode("vbox");
    hintParentNode.setAttribute("flex", "0");
    hintParentNode.setAttribute("class", "gcliterm-hint-parent");
    hintParentNode.setAttribute("pack", "end");
    hintParentNode.appendChild(this.hintNode);
    hintParentNode.hidden = true;

    let hbox = this.makeXULNode("hbox");
    hbox.setAttribute("flex", "1");
    hbox.setAttribute("class", "gcliterm-display");

    this.outputNode = this.makeXULNode("richlistbox");
    this.outputNode.setAttribute("class", "hud-output-node");
    this.outputNode.setAttribute("flex", "1");
    this.outputNode.setAttribute("orient", "vertical");
    this.outputNode.setAttribute("context", this.hudId + "-output-contextmenu");
    this.outputNode.setAttribute("style", "direction: ltr;");
    this.outputNode.setAttribute("seltype", "multiple");

    hbox.appendChild(hintParentNode);
    hbox.appendChild(this.outputNode);

    consoleWrap.appendChild(consoleFilterToolbar);
    consoleWrap.appendChild(hbox);

    outerWrap.appendChild(consoleWrap);

    this.HUDBox.lastTimestamp = 0;

    this.jsTermParentNode = this.consoleWrap;
    this.HUDBox.appendChild(outerWrap);

    return this.HUDBox;
  },

  /**
   * sets the click events for all binary toggle filter buttons
   *
   * @returns void
   */
  setFilterTextBoxEvents: function HUD_setFilterTextBoxEvents()
  {
    var filterBox = this.filterBox;
    function onChange()
    {
      // To improve responsiveness, we let the user finish typing before we
      // perform the search.

      if (this.timer == null) {
        let timerClass = Cc["@mozilla.org/timer;1"];
        this.timer = timerClass.createInstance(Ci.nsITimer);
      } else {
        this.timer.cancel();
      }

      let timerEvent = {
        notify: function setFilterTextBoxEvents_timerEvent_notify() {
          HUDService.updateFilterText(filterBox);
        }
      };

      this.timer.initWithCallback(timerEvent, SEARCH_DELAY,
        Ci.nsITimer.TYPE_ONE_SHOT);
    }

    filterBox.addEventListener("command", onChange, false);
    filterBox.addEventListener("input", onChange, false);
  },

  /**
   * Make the filter toolbar where we can toggle logging filters
   *
   * @returns nsIDOMNode
   */
  makeFilterToolbar: function HUD_makeFilterToolbar()
  {
    const BUTTONS = [
      {
        name: "PageNet",
        category: "net",
        severities: [
          { name: "ConsoleErrors", prefKey: "network" },
          { name: "ConsoleLog", prefKey: "networkinfo" }
        ]
      },
      {
        name: "PageCSS",
        category: "css",
        severities: [
          { name: "ConsoleErrors", prefKey: "csserror" },
          { name: "ConsoleWarnings", prefKey: "cssparser" }
        ]
      },
      {
        name: "PageJS",
        category: "js",
        severities: [
          { name: "ConsoleErrors", prefKey: "exception" },
          { name: "ConsoleWarnings", prefKey: "jswarn" }
        ]
      },
      {
        name: "PageLogging",
        category: "logging",
        severities: [
          { name: "ConsoleErrors", prefKey: "error" },
          { name: "ConsoleWarnings", prefKey: "warn" },
          { name: "ConsoleInfo", prefKey: "info" },
          { name: "ConsoleLog", prefKey: "log" }
        ]
      }
    ];

    let toolbar = this.makeXULNode("toolbar");
    toolbar.setAttribute("class", "hud-console-filter-toolbar");
    toolbar.setAttribute("mode", "full");

#ifdef XP_MACOSX
    this.makeCloseButton(toolbar);
#endif

    for (let i = 0; i < BUTTONS.length; i++) {
      this.makeFilterButton(toolbar, BUTTONS[i]);
    }

    toolbar.appendChild(this.filterSpacer);

    let positionUI = this.createPositionUI();
    toolbar.appendChild(positionUI);

    toolbar.appendChild(this.filterBox);
    this.makeClearConsoleButton(toolbar);

#ifndef XP_MACOSX
    this.makeCloseButton(toolbar);
#endif

    return toolbar;
  },

  /**
   * Creates the UI for re-positioning the console
   *
   * @return nsIDOMNode
   *         The toolbarbutton which holds the menu that allows the user to
   *         change the console position.
   */
  createPositionUI: function HUD_createPositionUI()
  {
    this._positionConsoleAbove = (function HUD_positionAbove() {
      this.positionConsole("above");
    }).bind(this);

    this._positionConsoleBelow = (function HUD_positionBelow() {
      this.positionConsole("below");
    }).bind(this);
    this._positionConsoleWindow = (function HUD_positionWindow() {
      this.positionConsole("window");
    }).bind(this);

    let button = this.makeXULNode("toolbarbutton");
    button.setAttribute("type", "menu");
    button.setAttribute("label", l10n.getStr("webConsolePosition"));
    button.setAttribute("tooltip", l10n.getStr("webConsolePositionTooltip"));

    let menuPopup = this.makeXULNode("menupopup");
    button.appendChild(menuPopup);

    let itemAbove = this.makeXULNode("menuitem");
    itemAbove.setAttribute("label", l10n.getStr("webConsolePositionAbove"));
    itemAbove.setAttribute("type", "checkbox");
    itemAbove.setAttribute("autocheck", "false");
    itemAbove.addEventListener("command", this._positionConsoleAbove, false);
    menuPopup.appendChild(itemAbove);

    let itemBelow = this.makeXULNode("menuitem");
    itemBelow.setAttribute("label", l10n.getStr("webConsolePositionBelow"));
    itemBelow.setAttribute("type", "checkbox");
    itemBelow.setAttribute("autocheck", "false");
    itemBelow.addEventListener("command", this._positionConsoleBelow, false);
    menuPopup.appendChild(itemBelow);

    let itemWindow = this.makeXULNode("menuitem");
    itemWindow.setAttribute("label", l10n.getStr("webConsolePositionWindow"));
    itemWindow.setAttribute("type", "checkbox");
    itemWindow.setAttribute("autocheck", "false");
    itemWindow.addEventListener("command", this._positionConsoleWindow, false);
    menuPopup.appendChild(itemWindow);

    this.positionMenuitems = {
      last: null,
      above: itemAbove,
      below: itemBelow,
      window: itemWindow,
    };

    return button;
  },

  /**
   * Creates the context menu on the console.
   *
   * @param nsIDOMNode aOutputNode
   *        The console output DOM node.
   * @returns void
   */
  createConsoleMenu: function HUD_createConsoleMenu(aConsoleWrapper) {
    let menuPopup = this.makeXULNode("menupopup");
    let id = this.hudId + "-output-contextmenu";
    menuPopup.setAttribute("id", id);
    menuPopup.addEventListener("popupshowing", function() {
      saveBodiesItem.setAttribute("checked", this.saveRequestAndResponseBodies);
    }.bind(this), true);

    let saveBodiesItem = this.makeXULNode("menuitem");
    saveBodiesItem.setAttribute("label", l10n.getStr("saveBodies.label"));
    saveBodiesItem.setAttribute("accesskey",
                                 l10n.getStr("saveBodies.accesskey"));
    saveBodiesItem.setAttribute("type", "checkbox");
    saveBodiesItem.setAttribute("buttonType", "saveBodies");
    saveBodiesItem.setAttribute("oncommand", "HUDConsoleUI.command(this);");
    saveBodiesItem.setAttribute("hudId", this.hudId);
    menuPopup.appendChild(saveBodiesItem);

    menuPopup.appendChild(this.makeXULNode("menuseparator"));

    let copyItem = this.makeXULNode("menuitem");
    copyItem.setAttribute("label", l10n.getStr("copyCmd.label"));
    copyItem.setAttribute("accesskey", l10n.getStr("copyCmd.accesskey"));
    copyItem.setAttribute("key", "key_copy");
    copyItem.setAttribute("command", "cmd_copy");
    copyItem.setAttribute("buttonType", "copy");
    menuPopup.appendChild(copyItem);

    let selectAllItem = this.makeXULNode("menuitem");
    selectAllItem.setAttribute("label", l10n.getStr("selectAllCmd.label"));
    selectAllItem.setAttribute("accesskey",
                               l10n.getStr("selectAllCmd.accesskey"));
    selectAllItem.setAttribute("hudId", this.hudId);
    selectAllItem.setAttribute("buttonType", "selectAll");
    selectAllItem.setAttribute("oncommand", "HUDConsoleUI.command(this);");
    menuPopup.appendChild(selectAllItem);

    aConsoleWrapper.appendChild(menuPopup);
    aConsoleWrapper.setAttribute("context", id);
  },

  /**
   * Creates one of the filter buttons on the toolbar.
   *
   * @param nsIDOMNode aParent
   *        The node to which the filter button should be appended.
   * @param object aDescriptor
   *        A descriptor that contains info about the button. Contains "name",
   *        "category", and "prefKey" properties, and optionally a "severities"
   *        property.
   * @return void
   */
  makeFilterButton: function HUD_makeFilterButton(aParent, aDescriptor)
  {
    let toolbarButton = this.makeXULNode("toolbarbutton");
    aParent.appendChild(toolbarButton);

    let toggleFilter = HeadsUpDisplayUICommands.toggleFilter;
    toolbarButton.addEventListener("click", toggleFilter, false);

    let name = aDescriptor.name;
    toolbarButton.setAttribute("type", "menu-button");
    toolbarButton.setAttribute("label", l10n.getStr("btn" + name));
    toolbarButton.setAttribute("tooltip", l10n.getStr("tip" + name));
    toolbarButton.setAttribute("category", aDescriptor.category);
    toolbarButton.setAttribute("hudId", this.hudId);
    toolbarButton.classList.add("webconsole-filter-button");

    let menuPopup = this.makeXULNode("menupopup");
    toolbarButton.appendChild(menuPopup);

    let someChecked = false;
    for (let i = 0; i < aDescriptor.severities.length; i++) {
      let severity = aDescriptor.severities[i];
      let menuItem = this.makeXULNode("menuitem");
      menuItem.setAttribute("label", l10n.getStr("btn" + severity.name));
      menuItem.setAttribute("type", "checkbox");
      menuItem.setAttribute("autocheck", "false");
      menuItem.setAttribute("hudId", this.hudId);

      let prefKey = severity.prefKey;
      menuItem.setAttribute("prefKey", prefKey);

      let checked = this.filterPrefs[prefKey];
      menuItem.setAttribute("checked", checked);
      if (checked) {
        someChecked = true;
      }

      menuItem.addEventListener("command", toggleFilter, false);

      menuPopup.appendChild(menuItem);
    }

    toolbarButton.setAttribute("checked", someChecked);
  },

  /**
   * Creates the close button on the toolbar.
   *
   * @param nsIDOMNode aParent
   *        The toolbar to attach the close button to.
   * @return void
   */
  makeCloseButton: function HUD_makeCloseButton(aToolbar)
  {
    this.closeButtonOnCommand = (function HUD_closeButton_onCommand() {
      HUDService.animate(this.hudId, ANIMATE_OUT, (function() {
        HUDService.deactivateHUDForContext(this.tab, true);
      }).bind(this));
    }).bind(this);

    this.closeButton = this.makeXULNode("toolbarbutton");
    this.closeButton.classList.add("webconsole-close-button");
    this.closeButton.addEventListener("command",
      this.closeButtonOnCommand, false);
    aToolbar.appendChild(this.closeButton);
  },

  /**
   * Creates the "Clear Console" button.
   *
   * @param nsIDOMNode aParent
   *        The toolbar to attach the "Clear Console" button to.
   * @param string aHUDId
   *        The ID of the console.
   * @return void
   */
  makeClearConsoleButton: function HUD_makeClearConsoleButton(aToolbar)
  {
    let hudId = this.hudId;
    function HUD_clearButton_onCommand() {
      let hud = HUDService.getHudReferenceById(hudId);
      hud.jsterm.clearOutput(true);
      hud.chromeWindow.DeveloperToolbar.resetErrorsCount(hud.tab);
    }

    let clearButton = this.makeXULNode("toolbarbutton");
    clearButton.setAttribute("label", l10n.getStr("btnClear"));
    clearButton.classList.add("webconsole-clear-console-button");
    clearButton.addEventListener("command", HUD_clearButton_onCommand, false);

    aToolbar.appendChild(clearButton);
  },

  /**
   * Destroy the property inspector message node. This performs the necessary
   * cleanup for the tree widget and removes it from the DOM.
   *
   * @param nsIDOMNode aMessageNode
   *        The message node that contains the property inspector from a
   *        console.dir call.
   */
  pruneConsoleDirNode: function HUD_pruneConsoleDirNode(aMessageNode)
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
   * Create the Web Console UI.
   *
   * @return nsIDOMNode
   *         The Web Console container element (HUDBox).
   */
  createHUD: function HUD_createHUD()
  {
    if (!this.HUDBox) {
      this.makeHUDNodes();
      let positionPref = Services.prefs.getCharPref("devtools.webconsole.position");
      this.positionConsole(positionPref);
    }
    return this.HUDBox;
  },

  uiInOwnWindow: false,

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
  logConsoleAPIMessage: function HUD_logConsoleAPIMessage(aMessage)
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
        body = argsToString.join(" ");
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

    let node = ConsoleUtils.createMessageNode(this.chromeDocument,
                                              CATEGORY_WEBDEV,
                                              LEVELS[level],
                                              body,
                                              this.hudId,
                                              sourceURL,
                                              sourceLine,
                                              clipboardText,
                                              level,
                                              aMessage.timeStamp);

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
   * Reports an error in the page source, either JavaScript or CSS.
   *
   * @param nsIScriptError aScriptError
   *        The error message to report.
   * @return nsIDOMElement|undefined
   *         The message element to display in the Web Console output.
   */
  reportPageError: function HUD_reportPageError(aCategory, aScriptError)
  {
    // Warnings and legacy strict errors become warnings; other types become
    // errors.
    let severity = SEVERITY_ERROR;
    if ((aScriptError.flags & aScriptError.warningFlag) ||
        (aScriptError.flags & aScriptError.strictFlag)) {
      severity = SEVERITY_WARNING;
    }

    let node = ConsoleUtils.createMessageNode(this.chromeDocument,
                                              aCategory,
                                              severity,
                                              aScriptError.errorMessage,
                                              this.hudId,
                                              aScriptError.sourceName,
                                              aScriptError.lineNumber,
                                              null,
                                              null,
                                              aScriptError.timeStamp);
    return node;
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
  categoryForScriptError: function HUD_categoryForScriptError(aScriptError)
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
   * Log network activity.
   *
   * @param object aHttpActivity
   *        The HTTP activity to log.
   * @return nsIDOMElement|undefined
   *         The message element to display in the Web Console output.
   */
  logNetActivity: function HUD_logNetActivity(aConnectionId)
  {
    let networkInfo = this._networkRequests[aConnectionId];
    if (!networkInfo) {
      return;
    }

    let entry = networkInfo.httpActivity.log.entries[0];
    let request = entry.request;

    let msgNode = this.chromeDocument.createElementNS(XUL_NS, "hbox");

    let methodNode = this.chromeDocument.createElementNS(XUL_NS, "label");
    methodNode.setAttribute("value", request.method);
    methodNode.classList.add("webconsole-msg-body-piece");
    msgNode.appendChild(methodNode);

    let linkNode = this.chromeDocument.createElementNS(XUL_NS, "hbox");
    linkNode.setAttribute("flex", "1");
    linkNode.classList.add("webconsole-msg-body-piece");
    linkNode.classList.add("webconsole-msg-link");
    msgNode.appendChild(linkNode);

    let urlNode = this.chromeDocument.createElementNS(XUL_NS, "label");
    urlNode.setAttribute("crop", "center");
    urlNode.setAttribute("flex", "1");
    urlNode.setAttribute("title", request.url);
    urlNode.setAttribute("value", request.url);
    urlNode.classList.add("hud-clickable");
    urlNode.classList.add("webconsole-msg-body-piece");
    urlNode.classList.add("webconsole-msg-url");
    linkNode.appendChild(urlNode);

    let severity = SEVERITY_LOG;
    if(this.isMixedContentLoad(request.url, this.contentLocation)) {
      urlNode.classList.add("webconsole-mixed-content");
      this.makeMixedContentNode(linkNode);
      //If we define a SEVERITY_SECURITY in the future, switch this to SEVERITY_SECURITY
      severity = SEVERITY_WARNING;
    }

    let statusNode = this.chromeDocument.createElementNS(XUL_NS, "label");
    statusNode.setAttribute("value", "");
    statusNode.classList.add("hud-clickable");
    statusNode.classList.add("webconsole-msg-body-piece");
    statusNode.classList.add("webconsole-msg-status");
    linkNode.appendChild(statusNode);

    let clipboardText = request.method + " " + request.url;

    let messageNode = ConsoleUtils.createMessageNode(this.chromeDocument,
                                                     CATEGORY_NETWORK,
                                                     severity,
                                                     msgNode,
                                                     this.hudId,
                                                     null,
                                                     null,
                                                     clipboardText);

    messageNode._connectionId = entry.connection;

    this.makeOutputMessageLink(messageNode, function HUD_net_message_link() {
      if (!messageNode._panelOpen) {
        HUDService.openNetworkPanel(messageNode, networkInfo.httpActivity);
      }
    }.bind(this));

    networkInfo.node = messageNode;

    this._updateNetMessage(entry.connection);

    return messageNode;
  },

  /**
   * Log file activity.
   *
   * @param string aFileURI
   *        The file URI that was loaded.
   * @return nsIDOMElement|undefined
   *         The message element to display in the Web Console output.
   */
  logFileActivity: function HUD_logFileActivity(aFileURI)
  {
    let chromeDocument = this.chromeDocument;

    let urlNode = chromeDocument.createElementNS(XUL_NS, "label");
    urlNode.setAttribute("crop", "center");
    urlNode.setAttribute("flex", "1");
    urlNode.setAttribute("title", aFileURI);
    urlNode.setAttribute("value", aFileURI);
    urlNode.classList.add("hud-clickable");
    urlNode.classList.add("webconsole-msg-url");

    let outputNode = ConsoleUtils.createMessageNode(chromeDocument,
                                                    CATEGORY_NETWORK,
                                                    SEVERITY_LOG,
                                                    urlNode,
                                                    this.hudId,
                                                    null,
                                                    null,
                                                    aFileURI);

    this.makeOutputMessageLink(outputNode, function HUD__onFileClick() {
      let viewSourceUtils = chromeDocument.defaultView.gViewSourceUtils;
      viewSourceUtils.viewSource(aFileURI, null, chromeDocument);
    });

    return outputNode;
  },

  /**
   * Inform user that the Web Console API has been replaced by a script
   * in a content page.
   *
   * @return nsIDOMElement|undefined
   *         The message element to display in the Web Console output.
   */
  logWarningAboutReplacedAPI: function HUD_logWarningAboutReplacedAPI()
  {
    let message = l10n.getStr("ConsoleAPIDisabled");
    let node = ConsoleUtils.createMessageNode(this.chromeDocument, CATEGORY_JS,
                                              SEVERITY_WARNING, message,
                                              this.hudId);
    return node;
  },

  ERRORS: {
    HUD_BOX_DOES_NOT_EXIST: "Heads Up Display does not exist",
    TAB_ID_REQUIRED: "Tab DOM ID is required",
    PARENTNODE_NOT_FOUND: "parentNode element not found"
  },

  /**
   * Setup the message manager used to communicate with the Web Console content
   * script. This method loads the content script, adds the message listeners
   * and initializes the connection to the content script.
   *
   * @private
   */
  _setupMessageManager: function HUD__setupMessageManager()
  {
    this.messageManager.loadFrameScript(CONTENT_SCRIPT_URL, true);

    this._messageListeners.forEach(function(aName) {
      this.messageManager.addMessageListener(aName, this);
    }, this);

    let message = {
      features: ["ConsoleAPI", "JSTerm", "PageError", "NetworkMonitor",
                 "LocationChange"],
      cachedMessages: ["ConsoleAPI", "PageError"],
      NetworkMonitor: { monitorFileActivity: true },
      JSTerm: { notifyNonNativeConsoleAPI: true },
      preferences: {
        "NetworkMonitor.saveRequestAndResponseBodies":
          this.saveRequestAndResponseBodies,
      },
    };
    this.sendMessageToContent("WebConsole:Init", message);
  },

  /**
   * Handler for all of the messages coming from the Web Console content script.
   *
   * @private
   * @param object aMessage
   *        A MessageManager object that holds the remote message.
   */
  receiveMessage: function HUD_receiveMessage(aMessage)
  {
    if (!aMessage.json || aMessage.json.hudId != this.hudId) {
      return;
    }

    switch (aMessage.name) {
      case "JSTerm:EvalResult":
      case "JSTerm:EvalObject":
      case "JSTerm:AutocompleteProperties":
        this._receiveMessageWithCallback(aMessage.json);
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
        let category = this.categoryForScriptError(pageError);
        this.outputMessage(category, this.reportPageError,
                           [category, pageError]);
        break;
      }
      case "WebConsole:CachedMessages":
        this._displayCachedConsoleMessages(aMessage.json.messages);
        this._onInitComplete();
        break;
      case "WebConsole:NetworkActivity":
        this.handleNetworkActivity(aMessage.json);
        break;
      case "WebConsole:FileActivity":
        this.outputMessage(CATEGORY_NETWORK, this.logFileActivity,
                           [aMessage.json.uri]);
        break;
      case "WebConsole:LocationChange":
        this.onLocationChange(aMessage.json);
        break;
      case "JSTerm:NonNativeConsoleAPI":
        this.outputMessage(CATEGORY_JS, this.logWarningAboutReplacedAPI);
        break;
    }
  },

  /**
   * Callback method for when the Web Console initialization is complete. For
   * now this method sends the web-console-created notification using the
   * nsIObserverService.
   *
   * @private
   */
  _onInitComplete: function HUD__onInitComplete()
  {
    let id = WebConsoleUtils.supportsString(this.hudId);
    Services.obs.notifyObservers(id, "web-console-created", null);
  },

  /**
   * Handler for messages that have an associated callback function. The
   * this.sendMessageToContent() allows one to provide a function to be invoked
   * when the content script replies to the message previously sent. This is the
   * method that invokes the callback.
   *
   * @see this.sendMessageToContent
   * @private
   * @param object aResponse
   *        Message object received from the content script.
   */
  _receiveMessageWithCallback:
  function HUD__receiveMessageWithCallback(aResponse)
  {
    if (aResponse.id in this.asyncRequests) {
      let request = this.asyncRequests[aResponse.id];
      request.callback(aResponse, request.message);
      delete this.asyncRequests[aResponse.id];
    }
    else {
      Cu.reportError("receiveMessageWithCallback response for stale request " +
                     "ID " + aResponse.id);
    }
  },

  /**
   * Send a message to the content script.
   *
   * @param string aName
   *        The name of the message you want to send.
   *
   * @param object aMessage
   *        The message object you want to send. This object needs to have no
   *        cyclic references and it needs to be JSON-stringifiable.
   *
   * @param function [aCallback]
   *        Optional function you want to have called when the content script
   *        replies to your message. Your callback receives two arguments:
   *        (1) the response object from the content script and (2) the message
   *        you sent to the content script (which is aMessage here).
   */
  sendMessageToContent:
  function HUD_sendMessageToContent(aName, aMessage, aCallback)
  {
    aMessage.hudId = this.hudId;
    if (!("id" in aMessage)) {
      aMessage.id = "HUDChrome-" + HUDService.sequenceId();
    }

    if (aCallback) {
      this.asyncRequests[aMessage.id] = {
        name: aName,
        message: aMessage,
        callback: aCallback,
      };
    }
    this.messageManager.sendAsyncMessage(aName, aMessage);
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
  handleNetworkActivity: function HUD_handleNetworkActivity(aMessage)
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
    if (HUDService.lastFinishedRequestCallback &&
        aMessage.meta.stages.indexOf("REQUEST_STOP") > -1 &&
        aMessage.meta.stages.indexOf("TRANSACTION_CLOSE") > -1) {
      HUDService.lastFinishedRequestCallback(aMessage);
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
  _updateNetMessage: function HUD__updateNetMessage(aConnectionId)
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
        ConsoleUtils.setMessageType(messageNode, CATEGORY_NETWORK,
                                    SEVERITY_ERROR);
      }
    }

    if (messageNode._netPanel) {
      messageNode._netPanel.update();
    }
  },

  /**
   * Handler for the "WebConsole:LocationChange" message. If the Web Console is
   * opened in a panel the panel title is updated.
   *
   * @param object aMessage
   *        The message received from the content script. It needs to hold two
   *        properties: location and title.
   */
  onLocationChange: function HUD_onLocationChange(aMessage)
  {
    this.contentLocation = aMessage.location;
    if (this.consolePanel) {
      this.consolePanel.label = this.getPanelTitle();
    }
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
  makeOutputMessageLink: function HUD_makeOutputMessageLink(aNode, aCallback)
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
  outputMessage: function HUD_outputMessage(aCategory, aMethodOrNode, aArguments)
  {
    if (!this._outputQueue.length) {
      // If the queue is empty we consider that now was the last output flush.
      // This avoid an immediate output flush when the timer executes.
      this._lastOutputFlush = Date.now();
    }

    this._outputQueue.push([aCategory, aMethodOrNode, aArguments]);

    if (!this._outputTimeout) {
      this._outputTimeout =
        this.chromeWindow.setTimeout(this._flushMessageQueue.bind(this),
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
  _flushMessageQueue: function HUD__flushMessageQueue()
  {
    let timeSinceFlush = Date.now() - this._lastOutputFlush;
    if (this._outputQueue.length > MESSAGES_IN_INTERVAL &&
        timeSinceFlush < THROTTLE_UPDATES) {
      this._outputTimeout =
        this.chromeWindow.setTimeout(this._flushMessageQueue.bind(this),
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
    let scrolledToBottom = ConsoleUtils.isOutputScrolledToBottom(outputNode);
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
        removedNodes += pruneConsoleOutputIfNecessary(this.hudId, aCategory);
      }, this);
      this._pruneCategoriesQueue = {};
    }

    // Regroup messages at the end of the queue.
    if (!this._outputQueue.length) {
      HUDService.regroupOutput(outputNode);
    }

    let isInputOutput = lastVisibleNode &&
      (lastVisibleNode.classList.contains("webconsole-msg-input") ||
       lastVisibleNode.classList.contains("webconsole-msg-output"));

    // Scroll to the new node if it is not filtered, and if the output node is
    // scrolled at the bottom or if the new node is a jsterm input/output
    // message.
    if (lastVisibleNode && (scrolledToBottom || isInputOutput)) {
      ConsoleUtils.scrollToVisible(lastVisibleNode);
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
        this.chromeWindow.setTimeout(this._flushMessageQueue.bind(this),
                                     OUTPUT_INTERVAL);
    }
    else {
      this._outputTimeout = null;
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
  function HUD__outputMessageFromQueue(aHudIdSupportsString, aItem)
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

    let isFiltered = ConsoleUtils.filterMessageNode(node, this.hudId);

    let isRepeated = false;
    if (node.classList.contains("webconsole-msg-cssparser")) {
      isRepeated = ConsoleUtils.filterRepeatedCSS(node, this.outputNode,
                                                  this.hudId);
    }

    if (!isRepeated &&
        !node.classList.contains("webconsole-msg-network") &&
        (node.classList.contains("webconsole-msg-console") ||
         node.classList.contains("webconsole-msg-exception") ||
         node.classList.contains("webconsole-msg-error"))) {
      isRepeated = ConsoleUtils.filterRepeatedConsole(node, this.outputNode);
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
  _pruneOutputQueue: function HUD__pruneOutputQueue()
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
      let limit = this.logLimitForCategory(category);
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
  _pruneItemFromQueue: function HUD__pruneItemFromQueue(aItem)
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
   * Retrieve the limit of messages for a specific category.
   *
   * @param number aCategory
   *        The category of messages you want to retrieve the limit for. See the
   *        CATEGORY_* constants.
   * @return number
   *         The number of messages allowed for the specific category.
   */
  logLimitForCategory: function HUD_logLimitForCategory(aCategory)
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

  /**
   * Remove a given message from the output.
   *
   * @param nsIDOMNode aNode
   *        The message node you want to remove.
   */
  removeOutputMessage: function HUD_removeOutputMessage(aNode)
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
      delete this.cssNodes[desc + location];
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
   * Destroy the HUD object. Call this method to avoid memory leaks when the Web
   * Console is closed.
   */
  destroy: function HUD_destroy()
  {
    this.sendMessageToContent("WebConsole:Destroy", {});

    this._messageListeners.forEach(function(aName) {
      this.messageManager.removeMessageListener(aName, this);
    }, this);

    if (this.jsterm) {
      this.jsterm.destroy();
    }

    // Make sure that the console panel does not try to call
    // deactivateHUDForContext() again.
    this.consoleWindowUnregisterOnHide = false;

    let popupset = this.chromeDocument.getElementById("mainPopupSet");
    let panels = popupset.querySelectorAll("panel[hudId=" + this.hudId + "]");
    for (let panel of panels) {
      if (panel != this.consolePanel) {
        panel.hidePopup();
      }
    }

    // Remove the HUDBox and the consolePanel if the Web Console is inside a
    // floating panel.
    if (this.consolePanel && this.consolePanel.parentNode) {
      this.consolePanel.hidePopup();
      this.consolePanel.parentNode.removeChild(this.consolePanel);
      this.consolePanel = null;
    }

    if (this.HUDBox.parentNode) {
      this.HUDBox.parentNode.removeChild(this.HUDBox);
    }

    if (this.splitter.parentNode) {
      this.splitter.parentNode.removeChild(this.splitter);
    }

    delete this.asyncRequests;
    delete this.messageManager;
    delete this.browser;
    delete this.chromeDocument;
    delete this.chromeWindow;
    delete this.outputNode;

    this.positionMenuitems.above.removeEventListener("command",
      this._positionConsoleAbove, false);
    this.positionMenuitems.below.removeEventListener("command",
      this._positionConsoleBelow, false);
    this.positionMenuitems.window.removeEventListener("command",
      this._positionConsoleWindow, false);

    this.closeButton.removeEventListener("command",
      this.closeButtonOnCommand, false);
  },

  /**
   * Determine whether the request should display a Mixed Content warning
   *
   * @param string request
   *        location of the requested content
   * @param string contentLocation
   *        location of the current page
   * @return bool
   *         True if the content is mixed, false if not
   */
  isMixedContentLoad: function HUD_isMixedContentLoad(request, contentLocation) {
    try {
      let requestURIObject = Services.io.newURI(request, null, null);
      let contentWindowURI = Services.io.newURI(contentLocation, null, null);
      return (contentWindowURI.scheme == "https" && requestURIObject.scheme != "https");
    } catch(e) {
      return false;
    }
  },

  /**
   * Create a mixedContentWarningNode and add it the webconsole output.
   *
   * @param linkNode
   *        parent to the requested urlNode
   */
  makeMixedContentNode: function HUD_makeMixedContentNode(linkNode) {
    let mixedContentWarning = "[" + l10n.getStr("webConsoleMixedContentWarning") + "]";

    //mixedContentWarning message links to a Learn More page
    let mixedContentWarningNode = this.chromeDocument.createElement("label");
    mixedContentWarningNode.setAttribute("value", mixedContentWarning);
    mixedContentWarningNode.setAttribute("title", mixedContentWarning);

    //UI for mixed content warning.
    mixedContentWarningNode.classList.add("hud-clickable");
    mixedContentWarningNode.classList.add("webconsole-mixed-content-link");

    linkNode.appendChild(mixedContentWarningNode);

    mixedContentWarningNode.addEventListener("click", function(aEvent) {
      this.chromeWindow.openUILinkIn(MIXED_CONTENT_LEARN_MORE, "tab");
      aEvent.preventDefault();
      aEvent.stopPropagation();
    }.bind(this));
  },
};

/**
 * Creates a DOM Node factory for XUL nodes - as well as textNodes
 * @param aFactoryType "xul" or "text"
 * @param ignored This parameter is currently ignored, and will be removed
 * See bug 594304
 * @param aDocument The document, the factory is to generate nodes from
 * @return DOM Node Factory function
 */
function NodeFactory(aFactoryType, ignored, aDocument)
{
  // aDocument is presumed to be a XULDocument
  if (aFactoryType == "text") {
    return function factory(aText)
    {
      return aDocument.createTextNode(aText);
    }
  }
  else if (aFactoryType == "xul") {
    return function factory(aTag)
    {
      return aDocument.createElement(aTag);
    }
  }
  else {
    throw new Error('NodeFactory: Unknown factory type: ' + aFactoryType);
  }
}


/**
 * Create a JSTerminal (a JavaScript command line). This is attached to an
 * existing HeadsUpDisplay (a Web Console instance). This code is responsible
 * with handling command line input, code evaluation and result output.
 *
 * @constructor
 * @param object aHud
 *        The HeadsUpDisplay object that owns this JSTerm instance.
 */
function JSTerm(aHud)
{
  this.hud = aHud;
  this.document = this.hud.chromeDocument;
  this.parentNode = this.hud.jsTermParentNode;

  this.hudId = this.hud.hudId;

  this.lastCompletion = { value: null };
  this.history = [];
  this.historyIndex = 0;
  this.historyPlaceHolder = 0;  // this.history.length;
  this.autocompletePopup = new AutocompletePopup(this.document);
  this.autocompletePopup.onSelect = this.onAutocompleteSelect.bind(this);
  this.autocompletePopup.onClick = this.acceptProposedCompletion.bind(this);
  this.init();
}

JSTerm.prototype = {
  lastInputValue: "",
  COMPLETE_FORWARD: 0,
  COMPLETE_BACKWARD: 1,
  COMPLETE_HINT_ONLY: 2,

  /**
   * Initialize the JSTerminal instance.
   */
  init: function JST_init()
  {
    this._generateUI();

    this._keyPress = this.keyPress.bind(this);
    this._inputEventHandler = this.inputEventHandler.bind(this);

    this.inputNode.addEventListener("keypress", this._keyPress, false);
    this.inputNode.addEventListener("input", this._inputEventHandler, false);
    this.inputNode.addEventListener("keyup", this._inputEventHandler, false);
  },

  /**
   * Generate the JSTerminal UI.
   * @private
   */
  _generateUI: function JST__generateUI()
  {
    this.completeNode = this.hud.makeXULNode("textbox");
    this.completeNode.setAttribute("class", "jsterm-complete-node");
    this.completeNode.setAttribute("multiline", "true");
    this.completeNode.setAttribute("rows", "1");
    this.completeNode.setAttribute("tabindex", "-1");

    this.inputNode = this.hud.makeXULNode("textbox");
    this.inputNode.setAttribute("class", "jsterm-input-node");
    this.inputNode.setAttribute("multiline", "true");
    this.inputNode.setAttribute("rows", "1");

    let inputStack = this.hud.makeXULNode("stack");
    inputStack.setAttribute("class", "jsterm-stack-node");
    inputStack.setAttribute("flex", "1");
    inputStack.appendChild(this.completeNode);
    inputStack.appendChild(this.inputNode);

    let term = this.hud.makeXULNode("hbox");
    term.setAttribute("class", "jsterm-input-container");
    term.setAttribute("style", "direction: ltr;");
    term.appendChild(inputStack);

    this.parentNode.appendChild(term);
  },

  get outputNode() this.hud.outputNode,

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
      resultCacheId: "HUDEval-" + HUDService.sequenceId(),
    };

    this.hud.sendMessageToContent("JSTerm:EvalRequest", message, aCallback);

    return message;
  },

  /**
   * The "JSTerm:EvalResult" message handler. This is the JSTerm execution
   * result callback which is invoked whenever JavaScript code evaluation
   * results come from the content process.
   *
   * @private
   * @param object aResponse
   *        The JSTerm:EvalResult message received from the content process. See
   *        JSTerm.handleEvalRequest() in HUDService-content.js for further
   *        details.
   * @param object aRequest
   *        The JSTerm:EvalRequest message we sent to the content process.
   * @see JSTerm.handleEvalRequest() in HUDService-content.js
   */
  _executeResultCallback:
  function JST__executeResultCallback(aResponse, aRequest)
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
   */
  execute: function JST_execute(aExecuteString)
  {
    // attempt to execute the content of the inputNode
    aExecuteString = aExecuteString || this.inputNode.value;
    if (!aExecuteString) {
      this.writeOutput("no value to execute", CATEGORY_OUTPUT, SEVERITY_LOG);
      return;
    }

    let node = this.writeOutput(aExecuteString, CATEGORY_INPUT, SEVERITY_LOG);

    let messageToContent =
      this.evalInContentSandbox(aExecuteString,
                                this._executeResultCallback.bind(this));
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

    let parent = this.document.getElementById("mainPopupSet");
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
    let node = ConsoleUtils.createMessageNode(this.document, aCategory,
                                              aSeverity, aOutputMessage,
                                              this.hudId, null, null, null,
                                              null, aTimestamp);
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

    hud.HUDBox.lastTimestamp = 0;
    hud.groupDepth = 0;
    hud._outputQueue.forEach(hud._pruneItemFromQueue, hud);
    hud._outputQueue = [];
    hud._networkRequests = {};
    hud.cssNodes = {};

    if (aClearStorage) {
      hud.sendMessageToContent("ConsoleAPI:ClearCache", {});
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

  history: null,

  // Stores the data for the last completion.
  lastCompletion: null,

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
      id: "HUDComplete-" + HUDService.sequenceId(),
      input: this.inputNode.value,
    };

    this.lastCompletion = {
      requestId: message.id,
      completionType: aType,
      value: null,
    };
    let callback = this._receiveAutocompleteProperties.bind(this, aCallback);
    this.hud.sendMessageToContent("JSTerm:Autocomplete", message, callback);
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
      this.hud.sendMessageToContent("JSTerm:ClearObjectCache",
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

    this.hud.sendMessageToContent("JSTerm:GetEvalObject", message, aCallback);
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
        panelCacheId: "HUDPanel-" + HUDService.sequenceId(),
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

    delete this.history;
    delete this.hud;
    delete this.autocompletePopup;
    delete this.document;
  },
};

//////////////////////////////////////////////////////////////////////////////
// Utility functions used by multiple callers
//////////////////////////////////////////////////////////////////////////////

/**
 * ConsoleUtils: a collection of globally used functions
 *
 */
ConsoleUtils = {
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
  scrollToVisible: function ConsoleUtils_scrollToVisible(aNode) {
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
   * Given a category and message body, creates a DOM node to represent an
   * incoming message. The timestamp is automatically added.
   *
   * @param nsIDOMDocument aDocument
   *        The document in which to create the node.
   * @param number aCategory
   *        The category of the message: one of the CATEGORY_* constants.
   * @param number aSeverity
   *        The severity of the message: one of the SEVERITY_* constants;
   * @param string|nsIDOMNode aBody
   *        The body of the message, either a simple string or a DOM node.
   * @param number aHUDId
   *        The HeadsUpDisplay ID.
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
  function ConsoleUtils_createMessageNode(aDocument, aCategory, aSeverity,
                                          aBody, aHUDId, aSourceURL,
                                          aSourceLine, aClipboardText, aLevel,
                                          aTimeStamp) {
    if (typeof aBody != "string" && aClipboardText == null && aBody.innerText) {
      aClipboardText = aBody.innerText;
    }

    // Make the icon container, which is a vertical box. Its purpose is to
    // ensure that the icon stays anchored at the top of the message even for
    // long multi-line messages.
    let iconContainer = aDocument.createElementNS(XUL_NS, "vbox");
    iconContainer.classList.add("webconsole-msg-icon-container");
    // Apply the curent group by indenting appropriately.
    let hud = HUDService.getHudReferenceById(aHUDId);
    iconContainer.style.marginLeft = hud.groupDepth * GROUP_INDENT + "px";

    // Make the icon node. It's sprited and the actual region of the image is
    // determined by CSS rules.
    let iconNode = aDocument.createElementNS(XUL_NS, "image");
    iconNode.classList.add("webconsole-msg-icon");
    iconContainer.appendChild(iconNode);

    // Make the spacer that positions the icon.
    let spacer = aDocument.createElementNS(XUL_NS, "spacer");
    spacer.setAttribute("flex", "1");
    iconContainer.appendChild(spacer);

    // Create the message body, which contains the actual text of the message.
    let bodyNode = aDocument.createElementNS(XUL_NS, "description");
    bodyNode.setAttribute("flex", "1");
    bodyNode.classList.add("webconsole-msg-body");

    // Store the body text, since it is needed later for the property tree
    // case.
    let body = aBody;
    // If a string was supplied for the body, turn it into a DOM node and an
    // associated clipboard string now.
    aClipboardText = aClipboardText ||
                     (aBody + (aSourceURL ? " @ " + aSourceURL : "") +
                              (aSourceLine ? ":" + aSourceLine : ""));
    if (!(aBody instanceof Ci.nsIDOMNode)) {
      aBody = aDocument.createTextNode(aLevel == "dir" ?
                                       aBody.resultString : aBody);
    }

    if (!aBody.nodeType) {
      aBody = aDocument.createTextNode(aBody.toString());
    }
    if (typeof aBody == "string") {
      aBody = aDocument.createTextNode(aBody);
    }

    bodyNode.appendChild(aBody);

    let repeatContainer = aDocument.createElementNS(XUL_NS, "hbox");
    repeatContainer.setAttribute("align", "start");
    let repeatNode = aDocument.createElementNS(XUL_NS, "label");
    repeatNode.setAttribute("value", "1");
    repeatNode.classList.add("webconsole-msg-repeat");
    repeatContainer.appendChild(repeatNode);

    // Create the timestamp.
    let timestampNode = aDocument.createElementNS(XUL_NS, "label");
    timestampNode.classList.add("webconsole-timestamp");
    let timestamp = aTimeStamp || Date.now();
    let timestampString = l10n.timestampString(timestamp);
    timestampNode.setAttribute("value", timestampString);

    // Create the source location (e.g. www.example.com:6) that sits on the
    // right side of the message, if applicable.
    let locationNode;
    if (aSourceURL) {
      locationNode = this.createLocationNode(aDocument, aSourceURL,
                                             aSourceLine);
    }

    // Create the containing node and append all its elements to it.
    let node = aDocument.createElementNS(XUL_NS, "richlistitem");
    node.clipboardText = aClipboardText;
    node.classList.add("hud-msg-node");

    node.timestamp = timestamp;
    ConsoleUtils.setMessageType(node, aCategory, aSeverity);

    node.appendChild(timestampNode);
    node.appendChild(iconContainer);
    // Display the object tree after the message node.
    if (aLevel == "dir") {
      // Make the body container, which is a vertical box, for grouping the text
      // and tree widgets.
      let bodyContainer = aDocument.createElement("vbox");
      bodyContainer.setAttribute("flex", "1");
      bodyContainer.appendChild(bodyNode);
      // Create the tree.
      let tree = createElement(aDocument, "tree", {
        flex: 1,
        hidecolumnpicker: "true"
      });

      let treecols = aDocument.createElement("treecols");
      let treecol = createElement(aDocument, "treecol", {
        primary: "true",
        flex: 1,
        hideheader: "true",
        ignoreincolumnpicker: "true"
      });
      treecols.appendChild(treecol);
      tree.appendChild(treecols);

      tree.appendChild(aDocument.createElement("treechildren"));

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

    node.setAttribute("id", "console-msg-" + HUDService.sequenceId());

    return node;
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
  function ConsoleUtils_setMessageType(aMessageNode, aNewCategory,
                                       aNewSeverity) {
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
   * Creates the XUL label that displays the textual location of an incoming
   * message.
   *
   * @param nsIDOMDocument aDocument
   *        The document in which to create the node.
   * @param string aSourceURL
   *        The URL of the source file responsible for the error.
   * @param number aSourceLine [optional]
   *        The line number on which the error occurred. If zero or omitted,
   *        there is no line number associated with this message.
   * @return nsIDOMNode
   *         The new XUL label node, ready to be added to the message node.
   */
  createLocationNode:
  function ConsoleUtils_createLocationNode(aDocument, aSourceURL,
                                           aSourceLine) {
    let locationNode = aDocument.createElementNS(XUL_NS, "label");

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
      let viewSourceUtils = aDocument.defaultView.gViewSourceUtils;
      viewSourceUtils.viewSource(aSourceURL, null, aDocument, aSourceLine);
    }, true);

    return locationNode;
  },

  /**
   * Applies the user's filters to a newly-created message node via CSS
   * classes.
   *
   * @param nsIDOMNode aNode
   *        The newly-created message node.
   * @param string aHUDId
   *        The ID of the HUD which this node is to be inserted into.
   * @return boolean
   *         True if the message was filtered or false otherwise.
   */
  filterMessageNode: function ConsoleUtils_filterMessageNode(aNode, aHUDId) {
    let isFiltered = false;

    // Filter by the message type.
    let prefKey = MESSAGE_PREFERENCE_KEYS[aNode.category][aNode.severity];
    if (prefKey && !HUDService.getFilterState(aHUDId, prefKey)) {
      // The node is filtered by type.
      aNode.classList.add("hud-filtered-by-type");
      isFiltered = true;
    }

    // Filter on the search string.
    let search = HUDService.getFilterStringByHUDId(aHUDId);
    let text = aNode.clipboardText;

    // if string matches the filter text
    if (!HUDService.stringMatchesFilters(text, search)) {
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
  function ConsoleUtils_mergeFilteredMessageNode(aOriginal, aFiltered) {
    // childNodes[3].firstChild is the node containing the number of repetitions
    // of a node.
    let repeatNode = aOriginal.childNodes[3].firstChild;
    if (!repeatNode) {
      return aOriginal; // no repeat node, return early.
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
   * @param nsIDOMNode aOutput
   *        The outputNode of the HUD.
   * @returns boolean
   *         true if the message is filtered, false otherwise.
   */
  filterRepeatedCSS:
  function ConsoleUtils_filterRepeatedCSS(aNode, aOutput, aHUDId) {
    let hud = HUDService.getHudReferenceById(aHUDId);

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

    let dupe = hud.cssNodes[description + location];
    if (!dupe) {
      // no matching nodes
      hud.cssNodes[description + location] = aNode;
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
   * @param nsIDOMNode aOutput
   *        The outputNode of the HUD.
   * @return boolean
   *         true if the message is filtered, false otherwise.
   */
  filterRepeatedConsole:
  function ConsoleUtils_filterRepeatedConsole(aNode, aOutput) {
    let lastMessage = aOutput.lastChild;

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
   * Check if the given output node is scrolled to the bottom.
   *
   * @param nsIDOMNode aOutputNode
   * @return boolean
   *         True if the output node is scrolled to the bottom, or false
   *         otherwise.
   */
  isOutputScrolledToBottom:
  function ConsoleUtils_isOutputScrolledToBottom(aOutputNode)
  {
    let lastNodeHeight = aOutputNode.lastChild ?
                         aOutputNode.lastChild.clientHeight : 0;
    let scrollBox = aOutputNode.scrollBoxObject.element;

    return scrollBox.scrollTop + scrollBox.clientHeight >=
           scrollBox.scrollHeight - lastNodeHeight / 2;
  },
};

//////////////////////////////////////////////////////////////////////////
// HeadsUpDisplayUICommands
//////////////////////////////////////////////////////////////////////////

HeadsUpDisplayUICommands = {
  refreshCommand: function UIC_refreshCommand() {
    var window = HUDService.currentContext();
    if (!window) {
      return;
    }

    let command = window.document.getElementById("Tools:WebConsole");
    if (this.getOpenHUD() != null) {
      command.setAttribute("checked", true);
    } else {
      command.setAttribute("checked", false);
    }
  },

  toggleHUD: function UIC_toggleHUD() {
    var window = HUDService.currentContext();
    var gBrowser = window.gBrowser;
    var linkedBrowser = gBrowser.selectedTab.linkedBrowser;
    var tabId = gBrowser.getNotificationBox(linkedBrowser).getAttribute("id");
    var hudId = "hud_" + tabId;
    var ownerDocument = gBrowser.selectedTab.ownerDocument;
    var hud = ownerDocument.getElementById(hudId);
    var hudRef = HUDService.hudReferences[hudId];

    if (hudRef && hud) {
      if (hudRef.consolePanel) {
        hudRef.consolePanel.hidePopup();
      }
      else {
        HUDService.storeHeight(hudId);

        HUDService.animate(hudId, ANIMATE_OUT, function() {
          // If the user closes the console while the console is animating away,
          // then these callbacks will queue up, but all the callbacks after the
          // first will have no console to operate on. This test handles this
          // case gracefully.
          if (ownerDocument.getElementById(hudId)) {
            HUDService.deactivateHUDForContext(gBrowser.selectedTab, true);
          }
        });
      }
    }
    else {
      HUDService.activateHUDForContext(gBrowser.selectedTab, true);
      HUDService.animate(hudId, ANIMATE_IN);
    }
  },

  /**
   * Find the hudId for the active chrome window.
   * @return string|null
   *         The hudId or null if the active chrome window has no open Web
   *         Console.
   */
  getOpenHUD: function UIC_getOpenHUD() {
    let chromeWindow = HUDService.currentContext();
    let hudId = "hud_" + chromeWindow.gBrowser.selectedTab.linkedPanel;
    return hudId in HUDService.hudReferences ? hudId : null;
  },

  /**
   * The event handler that is called whenever a user switches a filter on or
   * off.
   *
   * @param nsIDOMEvent aEvent
   *        The event that triggered the filter change.
   * @return boolean
   */
  toggleFilter: function UIC_toggleFilter(aEvent) {
    let hudId = this.getAttribute("hudId");
    switch (this.tagName) {
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

        let state = this.getAttribute("checked") !== "true";
        this.setAttribute("checked", state);

        // This is a filter button with a drop-down, and the user clicked the
        // main part of the button. Go through all the severities and toggle
        // their associated filters.
        let menuItems = this.querySelectorAll("menuitem");
        for (let i = 0; i < menuItems.length; i++) {
          menuItems[i].setAttribute("checked", state);
          let prefKey = menuItems[i].getAttribute("prefKey");
          HUDService.setFilterState(hudId, prefKey, state);
        }
        break;
      }

      case "menuitem": {
        let state = this.getAttribute("checked") !== "true";
        this.setAttribute("checked", state);

        let prefKey = this.getAttribute("prefKey");
        HUDService.setFilterState(hudId, prefKey, state);

        // Adjust the state of the button appropriately.
        let menuPopup = this.parentNode;

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

    return true;
  },

  command: function UIC_command(aButton) {
    var filter = aButton.getAttribute("buttonType");
    var hudId = aButton.getAttribute("hudId");
    switch (filter) {
      case "copy": {
        let outputNode = HUDService.hudReferences[hudId].outputNode;
        HUDService.copySelectedItems(outputNode);
        break;
      }
      case "selectAll": {
        HUDService.hudReferences[hudId].outputNode.selectAll();
        break;
      }
      case "saveBodies": {
        let checked = aButton.getAttribute("checked") === "true";
        HUDService.hudReferences[hudId].saveRequestAndResponseBodies = checked;
        break;
      }
    }
  },
};

//////////////////////////////////////////////////////////////////////////
// WebConsoleObserver
//////////////////////////////////////////////////////////////////////////

let WebConsoleObserver = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  init: function WCO_init()
  {
    Services.obs.addObserver(this, "quit-application-granted", false);
  },

  observe: function WCO_observe(aSubject, aTopic)
  {
    if (aTopic == "quit-application-granted") {
      HUDService.shutdown();
    }
  },

  uninit: function WCO_uninit()
  {
    Services.obs.removeObserver(this, "quit-application-granted");
  },
};

///////////////////////////////////////////////////////////////////////////////
// CommandController
///////////////////////////////////////////////////////////////////////////////

/**
 * A controller (an instance of nsIController) that makes editing actions
 * behave appropriately in the context of the Web Console.
 */
function CommandController(aWindow) {
  this.window = aWindow;
}

CommandController.prototype = {
  /**
   * Returns the HUD output node that currently has the focus, or null if the
   * currently-focused element isn't inside the output node.
   *
   * @returns nsIDOMNode
   *          The currently-focused output node.
   */
  _getFocusedOutputNode: function CommandController_getFocusedOutputNode()
  {
    let element = this.window.document.commandDispatcher.focusedElement;
    if (element && element.classList.contains("hud-output-node")) {
      return element;
    }
    return null;
  },

  /**
   * Copies the currently-selected entries in the Web Console output to the
   * clipboard.
   *
   * @param nsIDOMNode aOutputNode
   *        The Web Console output node.
   * @returns void
   */
  copy: function CommandController_copy(aOutputNode)
  {
    HUDService.copySelectedItems(aOutputNode);
  },

  /**
   * Selects all the text in the HUD output.
   *
   * @param nsIDOMNode aOutputNode
   *        The HUD output node.
   * @returns void
   */
  selectAll: function CommandController_selectAll(aOutputNode)
  {
    aOutputNode.selectAll();
  },

  supportsCommand: function CommandController_supportsCommand(aCommand)
  {
    return this.isCommandEnabled(aCommand);
  },

  isCommandEnabled: function CommandController_isCommandEnabled(aCommand)
  {
    let outputNode = this._getFocusedOutputNode();
    if (!outputNode) {
      return false;
    }

    switch (aCommand) {
      case "cmd_copy":
        // Only enable "copy" if nodes are selected.
        return outputNode.selectedCount > 0;
      case "cmd_selectAll":
        // "Select All" is always enabled.
        return true;
    }
  },

  doCommand: function CommandController_doCommand(aCommand)
  {
    let outputNode = this._getFocusedOutputNode();
    switch (aCommand) {
      case "cmd_copy":
        this.copy(outputNode);
        break;
      case "cmd_selectAll":
        this.selectAll(outputNode);
        break;
    }
  }
};

XPCOMUtils.defineLazyGetter(this, "HUDService", function () {
  return new HUD_SERVICE();
});

