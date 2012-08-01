/* -*- Mode: js2; js2-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const CONSOLEAPI_CLASS_ID = "{b49c18f8-3379-4fc0-8c90-d7772c1a9ff3}";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "WebConsoleUtils",
                                  "resource:///modules/WebConsoleUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "l10n", function() {
  return WebConsoleUtils.l10n;
});


var EXPORTED_SYMBOLS = ["HUDService"];

function LogFactory(aMessagePrefix)
{
  function log(aMessage) {
    var _msg = aMessagePrefix + " " + aMessage + "\n";
    dump(_msg);
  }
  return log;
}

let log = LogFactory("*** HUDService:");

// The HTML namespace.
const HTML_NS = "http://www.w3.org/1999/xhtml";

// The XUL namespace.
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";


// Possible directions that can be passed to HUDService.animate().
const ANIMATE_OUT = 0;
const ANIMATE_IN = 1;

// Minimum console height, in pixels.
const MINIMUM_CONSOLE_HEIGHT = 150;

// Minimum page height, in pixels. This prevents the Web Console from
// remembering a height that covers the whole page.
const MINIMUM_PAGE_HEIGHT = 50;

// The default console height, as a ratio from the content window inner height.
const DEFAULT_CONSOLE_HEIGHT = 0.33;

// This script is inserted into the content process.
const CONTENT_SCRIPT_URL = "chrome://browser/content/devtools/HUDService-content.js";

// points to the file to load in the Web Console iframe.
const UI_IFRAME_URL = "chrome://browser/content/devtools/webconsole.xul";

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

    gBrowser.tabContainer.addEventListener("TabClose", this.onTabClose, false);
    gBrowser.tabContainer.addEventListener("TabSelect", this.onTabSelect, false);
    window.addEventListener("unload", this.onWindowUnload, false);

    let hud = new WebConsole(aTab);
    this.hudReferences[hudId] = hud;

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

    let hud = this.getHudReferenceById(hudId);
    let document = hud.chromeDocument;

    hud.destroy();

    delete this.hudReferences[hudId];

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
   * Assign a function to this property to listen for every request that
   * completes. Used by unit tests. The callback takes one argument: the HTTP
   * activity object as received from the remote Web Console.
   *
   * @type function
   */
  lastFinishedRequestCallback: null,

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
    let hudBox = this.getHudReferenceById(aHUDId).iframe;
    if (!hudBox.hasAttribute("animated")) {
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
    let hudBox = HUDService.hudReferences[aHUDId].iframe;
    if (hudBox.hasAttribute("animated")) {
      hudBox.removeAttribute("animated");
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
    let innerHeight = HUD.tab.linkedBrowser.clientHeight;
    let chromeWindow = HUD.chromeWindow;
    if (!HUD.consolePanel) {
      let splitterStyle = chromeWindow.getComputedStyle(HUD.splitter, null);
      innerHeight += parseInt(splitterStyle.height) +
                     parseInt(splitterStyle.borderTopWidth) +
                     parseInt(splitterStyle.borderBottomWidth);
    }

    let boxStyle = chromeWindow.getComputedStyle(HUD.iframe, null);
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

    HUD.iframe.style.height = height + "px";
  },

  /**
   * Remember the height of the given Web Console, such that it can later be
   * reused when other Web Consoles are open.
   *
   * @param string aHUDId The ID of the Web Console.
   */
  storeHeight: function HS_storeHeight(aHUDId)
  {
    let hudBox = this.hudReferences[aHUDId].iframe;
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
};


/**
 * A WebConsole instance is an interactive console initialized *per tab*
 * that displays console log data as well as provides an interactive terminal to
 * manipulate the current tab's document content.
 *
 * This object only wraps the iframe that holds the Web Console UI.
 *
 * @param nsIDOMElement aTab
 *        The xul:tab for which you want the WebConsole object.
 */
function WebConsole(aTab)
{
  this.tab = aTab;
  this._onIframeLoad = this._onIframeLoad.bind(this);
  this._asyncRequests = {};
  this._init();
}

WebConsole.prototype = {
  /**
   * The xul:tab for which the current Web Console instance was created.
   * @type nsIDOMElement
   */
  tab: null,

  /**
   * Getter for HUDService.lastFinishedRequestCallback.
   *
   * @see HUDService.lastFinishedRequestCallback
   * @type function
   */
  get lastFinishedRequestCallback() HUDService.lastFinishedRequestCallback,

  /**
   * Track callback functions registered for specific async requests sent to
   * the content process.
   *
   * @private
   * @type object
   */
  _asyncRequests: null,

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

  /**
   * The xul:panel that holds the Web Console when it is positioned as a window.
   * @type nsIDOMElement
   */
  consolePanel: null,

  /**
   * The current tab location.
   * @type string
   */
  contentLocation: "",

  /**
   * Getter for the xul:popupset that holds any popups we open.
   * @type nsIDOMElement
   */
  get mainPopupSet()
  {
    return this.chromeDocument.getElementById("mainPopupSet");
  },

  /**
   * Getter for the output element that holds messages we display.
   * @type nsIDOMElement
   */
  get outputNode()
  {
    return this.ui ? this.ui.outputNode : null;
  },

  get gViewSourceUtils() this.chromeWindow.gViewSourceUtils,

  /**
   * Initialize the Web Console instance.
   * @private
   */
  _init: function WC__init()
  {
    this.chromeDocument = this.tab.ownerDocument;
    this.chromeWindow = this.chromeDocument.defaultView;
    this.messageManager = this.tab.linkedBrowser.messageManager;
    this.hudId = "hud_" + this.tab.linkedPanel;
    this.notificationBox = this.chromeDocument
                           .getElementById(this.tab.linkedPanel);

    this._initUI();
  },

  /**
   * Initialize the Web Console UI. This method sets up the iframe.
   * @private
   */
  _initUI: function WC__initUI()
  {
    this.splitter = this.chromeDocument.createElement("splitter");
    this.splitter.setAttribute("class", "web-console-splitter");

    this.iframe = this.chromeDocument.createElement("iframe");
    this.iframe.setAttribute("id", this.hudId);
    this.iframe.setAttribute("class", "web-console-frame");
    this.iframe.setAttribute("animated", "true");
    this.iframe.setAttribute("tooltip", "aHTMLTooltip");
    this.iframe.style.height = 0;
    this.iframe.addEventListener("load", this._onIframeLoad, true);
    this.iframe.setAttribute("src", UI_IFRAME_URL);

    let position = Services.prefs.getCharPref("devtools.webconsole.position");
    this.positionConsole(position);
  },

  /**
   * The "load" event handler for the Web Console iframe.
   * @private
   */
  _onIframeLoad: function WC__onIframeLoad()
  {
    this.iframe.removeEventListener("load", this._onIframeLoad, true);

    let position = Services.prefs.getCharPref("devtools.webconsole.position");

    this.iframeWindow = this.iframe.contentWindow.wrappedJSObject;
    this.ui = new this.iframeWindow.WebConsoleFrame(this, position);
    this._setupMessageManager();
  },

  /**
   * Create a panel to open the web console if it should float above
   * the content in its own window.
   * @private
   */
  _createOwnWindowPanel: function WC__createOwnWindowPanel()
  {
    if (this.consolePanel) {
      return;
    }

    let width = 0;
    try {
      width = Services.prefs.getIntPref("devtools.webconsole.width");
    }
    catch (ex) {}

    if (width < 1) {
      width = this.iframe.clientWidth || this.chromeWindow.innerWidth;
    }

    let height = this.iframe.clientHeight;

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

      this.iframe.style.height = "auto";
      this.iframe.flex = 1;

      panel.setAttribute("height", height);
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

    if (this.outputNode && this.outputNode.getIndexOfFirstVisibleRow) {
      lastIndex = this.outputNode.getIndexOfFirstVisibleRow() +
                  this.outputNode.getNumberOfVisibleRows() - 1;
    }

    if (this.splitter.parentNode) {
      this.splitter.parentNode.removeChild(this.splitter);
    }

    this._beforePositionConsole("window", lastIndex);

    panel.appendChild(this.iframe);

    let space = this.chromeDocument.createElement("spacer");
    space.flex = 1;

    let bottomBox = this.chromeDocument.createElement("hbox");

    let resizer = this.chromeDocument.createElement("resizer");
    resizer.setAttribute("dir", "bottomend");
    resizer.setAttribute("element", config.id);

    bottomBox.appendChild(space);
    bottomBox.appendChild(resizer);

    panel.appendChild(bottomBox);

    this.mainPopupSet.appendChild(panel);

    panel.openPopup(null, "overlay", left, top, false, false);

    this.consolePanel = panel;
    this.consoleWindowUnregisterOnHide = true;
  },

  /**
   * Retrieve the Web Console panel title.
   *
   * @return string
   *         The Web Console panel title.
   */
  getPanelTitle: function WC_getPanelTitle()
  {
    return l10n.getFormatStr("webConsoleWindowTitleAndURL",
                             [this.contentLocation]);
  },

  positions: {
    above: 0, // the childNode index
    below: 2,
    window: null
  },

  consoleWindowUnregisterOnHide: true,

  /**
   * Position the Web Console UI.
   *
   * @param string aPosition
   *        The desired Web Console UI location: above, below or window.
   */
  positionConsole: function WC_positionConsole(aPosition)
  {
    if (!(aPosition in this.positions)) {
      throw new Error("Incorrect argument: " + aPosition +
        ". Cannot position Web Console");
    }

    if (aPosition == "window") {
      this._createOwnWindowPanel();
      return;
    }

    let height = this.iframe.clientHeight;

    // get the node position index
    let nodeIdx = this.positions[aPosition];
    let nBox = this.notificationBox;
    let node = nBox.childNodes[nodeIdx];

    // check to see if console is already positioned in aPosition
    if (node == this.iframe) {
      return;
    }

    let lastIndex = -1;

    if (this.outputNode && this.outputNode.getIndexOfFirstVisibleRow) {
      lastIndex = this.outputNode.getIndexOfFirstVisibleRow() +
                  this.outputNode.getNumberOfVisibleRows() - 1;
    }

    // remove the console and splitter and reposition
    if (this.splitter.parentNode) {
      this.splitter.parentNode.removeChild(this.splitter);
    }

    this._beforePositionConsole(aPosition, lastIndex);

    if (aPosition == "below") {
      nBox.appendChild(this.splitter);
      nBox.appendChild(this.iframe);
    }
    else {
      nBox.insertBefore(this.splitter, node);
      nBox.insertBefore(this.iframe, this.splitter);
    }

    if (this.consolePanel) {
      // must destroy the consolePanel
      this.consoleWindowUnregisterOnHide = false;
      this.consolePanel.hidePopup();
      this.consolePanel.parentNode.removeChild(this.consolePanel);
      this.consolePanel = null;   // remove this as we're not in panel anymore
      this.iframe.removeAttribute("flex");
      this.iframe.removeAttribute("height");
      this.iframe.style.height = height + "px";
    }
  },

  /**
   * Common code that needs to execute before the Web Console is repositioned.
   * @private
   * @param string aPosition
   *        The new position: "above", "below" or "window".
   * @param number aLastIndex
   *        The last visible message in the console output before repositioning
   *        occurred.
   */
  _beforePositionConsole:
  function WC__beforePositionConsole(aPosition, aLastIndex)
  {
    if (!this.ui) {
      return;
    }

    let onLoad = function() {
      this.iframe.removeEventListener("load", onLoad, true);
      this.iframeWindow = this.iframe.contentWindow.wrappedJSObject;
      this.ui.positionConsole(aPosition, this.iframeWindow);

      if (aLastIndex > -1 && aLastIndex < this.outputNode.getRowCount()) {
        this.outputNode.ensureIndexIsVisible(aLastIndex);
      }

      this._currentUIPosition = aPosition;
      Services.prefs.setCharPref("devtools.webconsole.position", aPosition);
    }.bind(this);

    this.iframe.addEventListener("load", onLoad, true);
  },

  /**
   * The JSTerm object that manages the console's input.
   * @see webconsole.js::JSTerm
   * @type object
   */
  get jsterm()
  {
    return this.ui ? this.ui.jsterm : null;
  },

  /**
   * The close button handler.
   */
  onCloseButton: function WC_onCloseButton()
  {
    HUDService.animate(this.hudId, ANIMATE_OUT, function() {
      HUDService.deactivateHUDForContext(this.tab, true);
    }.bind(this));
  },

  /**
   * The clear output button handler.
   */
  onClearButton: function WC_onClearButton()
  {
    this.ui.jsterm.clearOutput(true);
    this.chromeWindow.DeveloperToolbar.resetErrorsCount(this.tab);
  },

  /**
   * Setup the message manager used to communicate with the Web Console content
   * script. This method loads the content script, adds the message listeners
   * and initializes the connection to the content script.
   *
   * @private
   */
  _setupMessageManager: function WC__setupMessageManager()
  {
    this.messageManager.loadFrameScript(CONTENT_SCRIPT_URL, true);

    this._messageListeners.forEach(function(aName) {
      this.messageManager.addMessageListener(aName, this.ui);
    }, this);

    let message = {
      features: ["ConsoleAPI", "JSTerm", "PageError", "NetworkMonitor",
                 "LocationChange"],
      cachedMessages: ["ConsoleAPI", "PageError"],
      NetworkMonitor: { monitorFileActivity: true },
      JSTerm: { notifyNonNativeConsoleAPI: true },
      preferences: {
        "NetworkMonitor.saveRequestAndResponseBodies":
          this.ui.saveRequestAndResponseBodies,
      },
    };

    this.sendMessageToContent("WebConsole:Init", message);
  },

  /**
   * Callback method for when the Web Console initialization is complete. For
   * now this method sends the web-console-created notification using the
   * nsIObserverService.
   *
   * @private
   */
  _onInitComplete: function WC__onInitComplete()
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
  function WC__receiveMessageWithCallback(aResponse)
  {
    if (aResponse.id in this._asyncRequests) {
      let request = this._asyncRequests[aResponse.id];
      request.callback(aResponse, request.message);
      delete this._asyncRequests[aResponse.id];
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
  function WC_sendMessageToContent(aName, aMessage, aCallback)
  {
    aMessage.hudId = this.hudId;
    if (!("id" in aMessage)) {
      aMessage.id = "HUDChrome-" + HUDService.sequenceId();
    }

    if (aCallback) {
      this._asyncRequests[aMessage.id] = {
        name: aName,
        message: aMessage,
        callback: aCallback,
      };
    }
    this.messageManager.sendAsyncMessage(aName, aMessage);
  },

  /**
   * Handler for the "WebConsole:LocationChange" message. If the Web Console is
   * opened in a panel the panel title is updated.
   *
   * @param object aMessage
   *        The message received from the content script. It needs to hold two
   *        properties: location and title.
   */
  onLocationChange: function WC_onLocationChange(aMessage)
  {
    this.contentLocation = aMessage.location;
    if (this.consolePanel) {
      this.consolePanel.label = this.getPanelTitle();
    }
  },

  /**
   * Alias for the WebConsoleFrame.setFilterState() method.
   * @see webconsole.js::WebConsoleFrame.setFilterState()
   */
  setFilterState: function WC_setFilterState()
  {
    this.ui && this.ui.setFilterState.apply(this.ui, arguments);
  },

  /**
   * Open a link in a new tab.
   *
   * @param string aLink
   *        The URL you want to open in a new tab.
   */
  openLink: function WC_openLink(aLink)
  {
    this.chromeWindow.openUILinkIn(aLink, "tab");
  },

  /**
   * Destroy the object. Call this method to avoid memory leaks when the Web
   * Console is closed.
   */
  destroy: function WC_destroy()
  {
    this.sendMessageToContent("WebConsole:Destroy", {});

    this._messageListeners.forEach(function(aName) {
      this.messageManager.removeMessageListener(aName, this.ui);
    }, this);

    // Make sure that the console panel does not try to call
    // deactivateHUDForContext() again.
    this.consoleWindowUnregisterOnHide = false;

    let popupset = this.mainPopupSet;
    let panels = popupset.querySelectorAll("panel[hudId=" + this.hudId + "]");
    for (let panel of panels) {
      if (panel != this.consolePanel) {
        panel.hidePopup();
      }
    }

    if (this.ui) {
      this.ui.destroy();
    }

    // Remove the iframe and the consolePanel if the Web Console is inside a
    // floating panel.
    if (this.consolePanel && this.consolePanel.parentNode) {
      this.consolePanel.hidePopup();
      this.consolePanel.parentNode.removeChild(this.consolePanel);
      this.consolePanel = null;
    }

    if (this.iframe.parentNode) {
      this.iframe.parentNode.removeChild(this.iframe);
    }

    if (this.splitter.parentNode) {
      this.splitter.parentNode.removeChild(this.splitter);
    }
  },
};

//////////////////////////////////////////////////////////////////////////
// HeadsUpDisplayUICommands
//////////////////////////////////////////////////////////////////////////

var HeadsUpDisplayUICommands = {
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
};

//////////////////////////////////////////////////////////////////////////
// WebConsoleObserver
//////////////////////////////////////////////////////////////////////////

var WebConsoleObserver = {
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


XPCOMUtils.defineLazyGetter(this, "HUDService", function () {
  return new HUD_SERVICE();
});

