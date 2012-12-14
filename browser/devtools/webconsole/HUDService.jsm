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
Cu.import("resource:///modules/devtools/gDevTools.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "TargetFactory",
                                  "resource:///modules/devtools/Target.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "WebConsoleUtils",
                                  "resource://gre/modules/devtools/WebConsoleUtils.jsm");

const STRINGS_URI = "chrome://browser/locale/devtools/webconsole.properties";
let l10n = new WebConsoleUtils.l10n(STRINGS_URI);

this.EXPORTED_SYMBOLS = ["HUDService"];

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

///////////////////////////////////////////////////////////////////////////
//// The HUD service

function HUD_SERVICE()
{
  // These methods access the "this" object, but they're registered as
  // event listeners. So we hammer in the "this" binding.
  this.onWindowUnload = this.onWindowUnload.bind(this);

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
   * @param nsIDOMElement aIframe
   *        The iframe element into which to place the web console.
   * @param RemoteTarget aTarget
   *        The target that the web console will connect to.
   * @return object
   *         The new HeadsUpDisplay instance.
   */
  activateHUDForContext: function HS_activateHUDForContext(aTab, aIframe,
                                                           aTarget)
  {
    let hudId = "hud_" + aTab.linkedPanel;
    if (hudId in this.hudReferences) {
      return this.hudReferences[hudId];
    }

    this.wakeup();

    let window = aTab.ownerDocument.defaultView;
    let gBrowser = window.gBrowser;

    window.addEventListener("unload", this.onWindowUnload, false);

    let hud = new WebConsole(aTab, aIframe, aTarget);
    this.hudReferences[hudId] = hud;

    return hud;
  },

  /**
   * Deactivate a HeadsUpDisplay for the given tab context.
   *
   * @param nsIDOMElement aTab
   *        The xul:tab element you want to enable the Web Console for.
   * @return void
   */
  deactivateHUDForContext: function HS_deactivateHUDForContext(aTab)
  {
    let hudId = "hud_" + aTab.linkedPanel;
    if (!(hudId in this.hudReferences)) {
      return;
    }

    let hud = this.getHudReferenceById(hudId);
    let document = hud.chromeDocument;

    hud.destroy(function() {
      let id = WebConsoleUtils.supportsString(hudId);
      Services.obs.notifyObservers(id, "web-console-destroyed", null);
    });

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

      this.suspend();
    }

    let contentWindow = aTab.linkedBrowser.contentWindow;
    contentWindow.focus();
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
      this.deactivateHUDForContext(hud.tab);
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

    let tab = tabContainer.firstChild;
    while (tab != null) {
      this.deactivateHUDForContext(tab);
      tab = tab.nextSibling;
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
 * @param nsIDOMElement aIframe
 *        iframe into which we should create the WebConsole UI.
 * @param RemoteTarget aTarget
 *        The target that the web console will connect to.
 */
function WebConsole(aTab, aIframe, aTarget)
{
  this.tab = aTab;
  if (this.tab == null) {
    throw new Error('Missing tab');
  }

  this.iframe = aIframe;
  if (this.iframe == null) {
    console.trace();
    throw new Error('Missing iframe');
  }

  this.chromeDocument = this.tab.ownerDocument;
  this.chromeWindow = this.chromeDocument.defaultView;
  this.hudId = "hud_" + this.tab.linkedPanel;

  this.target = aTarget;

  this._onIframeLoad = this._onIframeLoad.bind(this);

  this.iframe.className = "web-console-frame";
  this.iframe.addEventListener("load", this._onIframeLoad, true);

  this.positionConsole();
}

WebConsole.prototype = {
  /**
   * The xul:tab for which the current Web Console instance was created.
   * @type nsIDOMElement
   */
  tab: null,

  chromeWindow: null,
  chromeDocument: null,

  /**
   * Getter for HUDService.lastFinishedRequestCallback.
   *
   * @see HUDService.lastFinishedRequestCallback
   * @type function
   */
  get lastFinishedRequestCallback() HUDService.lastFinishedRequestCallback,

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
   * The "load" event handler for the Web Console iframe.
   * @private
   */
  _onIframeLoad: function WC__onIframeLoad()
  {
    this.iframe.removeEventListener("load", this._onIframeLoad, true);

    this.iframeWindow = this.iframe.contentWindow.wrappedJSObject;
    this.ui = new this.iframeWindow.WebConsoleFrame(this);
  },

  /**
   * Retrieve the Web Console panel title.
   *
   * @return string
   *         The Web Console panel title.
   */
  getPanelTitle: function WC_getPanelTitle()
  {
    let url = this.ui ? this.ui.contentLocation : "";
    return l10n.getFormatStr("webConsoleWindowTitleAndURL", [url]);
  },

  consoleWindowUnregisterOnHide: true,

  /**
   * Position the Web Console UI.
   */
  positionConsole: function WC_positionConsole()
  {
    let lastIndex = -1;

    if (this.outputNode && this.outputNode.getIndexOfFirstVisibleRow) {
      lastIndex = this.outputNode.getIndexOfFirstVisibleRow() +
                  this.outputNode.getNumberOfVisibleRows() - 1;
    }

    this._beforePositionConsole(lastIndex);
  },

  /**
   * Common code that needs to execute before the Web Console is repositioned.
   * @private
   * @param number aLastIndex
   *        The last visible message in the console output before repositioning
   *        occurred.
   */
  _beforePositionConsole:
  function WC__beforePositionConsole(aLastIndex)
  {
    if (!this.ui) {
      return;
    }

    let onLoad = function() {
      this.iframe.removeEventListener("load", onLoad, true);
      this.iframeWindow = this.iframe.contentWindow.wrappedJSObject;
      this.ui.positionConsole(this.iframeWindow);

      if (aLastIndex > -1 && aLastIndex < this.outputNode.getRowCount()) {
        this.outputNode.ensureIndexIsVisible(aLastIndex);
      }
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
   * The clear output button handler.
   * @private
   */
  _onClearButton: function WC__onClearButton()
  {
    this.chromeWindow.DeveloperToolbar.resetErrorsCount(this.tab);
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
   * Open a link in Firefox's view source.
   *
   * @param string aSourceURL
   *        The URL of the file.
   * @param integer aSourceLine
   *        The line number which should be highlighted.
   */
  viewSource: function WC_viewSource(aSourceURL, aSourceLine)
  {
    this.gViewSourceUtils.viewSource(aSourceURL, null,
                                     this.iframeWindow.document, aSourceLine);
  },

  /**
   * Tries to open a Stylesheet file related to the web page for the web console
   * instance in the Style Editor. If the file is not found, it is opened in
   * source view instead.
   *
   * @param string aSourceURL
   *        The URL of the file.
   * @param integer aSourceLine
   *        The line number which you want to place the caret.
   * TODO: This function breaks the client-server boundaries.
   *       To be fixed in bug 793259.
   */
  viewSourceInStyleEditor:
  function WC_viewSourceInStyleEditor(aSourceURL, aSourceLine)
  {
    let styleSheets = this.tab.linkedBrowser.contentWindow.document.styleSheets;
    for each (let style in styleSheets) {
      if (style.href == aSourceURL) {
        let target = TargetFactory.forTab(this.tab);
        let gDevTools = this.chromeWindow.gDevTools;
        gDevTools.showToolbox(target, "styleeditor").then(function(toolbox) {
          toolbox.getCurrentPanel().selectStyleSheet(style, aSourceLine);
        });
        return;
      }
    }
    // Open view source if style editor fails.
    this.viewSource(aSourceURL, aSourceLine);
  },

  /**
   * Destroy the object. Call this method to avoid memory leaks when the Web
   * Console is closed.
   *
   * @param function [aOnDestroy]
   *        Optional function to invoke when the Web Console instance is
   *        destroyed.
   */
  destroy: function WC_destroy(aOnDestroy)
  {
    // Make sure that the console panel does not try to call
    // deactivateHUDForContext() again.
    this.consoleWindowUnregisterOnHide = false;

    let popupset = this.mainPopupSet;
    let panels = popupset.querySelectorAll("panel[hudId=" + this.hudId + "]");
    for (let panel of panels) {
      panel.hidePopup();
    }

    let onDestroy = function WC_onDestroyUI() {
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

      aOnDestroy && aOnDestroy();
    }.bind(this);

    if (this.ui) {
      this.ui.destroy(onDestroy);
    }
    else {
      onDestroy();
    }
  },
};

//////////////////////////////////////////////////////////////////////////
// HeadsUpDisplayUICommands
//////////////////////////////////////////////////////////////////////////

var HeadsUpDisplayUICommands = {
  toggleHUD: function UIC_toggleHUD(aOptions)
  {
    var window = HUDService.currentContext();
    let target = TargetFactory.forTab(window.gBrowser.selectedTab);
    let toolbox = gDevTools.getToolbox(target);

    return toolbox && toolbox.currentToolId == "webconsole" ?
        toolbox.destroy() :
        gDevTools.showToolbox(target, "webconsole");
  },

  toggleRemoteHUD: function UIC_toggleRemoteHUD()
  {
    if (this.getOpenHUD()) {
      this.toggleHUD();
      return;
    }

    let host = Services.prefs.getCharPref("devtools.debugger.remote-host");
    let port = Services.prefs.getIntPref("devtools.debugger.remote-port");

    let check = { value: false };
    let input = { value: host + ":" + port };

    let result = Services.prompt.prompt(null,
      l10n.getStr("remoteWebConsolePromptTitle"),
      l10n.getStr("remoteWebConsolePromptMessage"),
      input, null, check);

    if (!result) {
      return;
    }

    let parts = input.value.split(":");
    if (parts.length != 2) {
      return;
    }

    [host, port] = parts;
    if (!host.length || !port.length) {
      return;
    }

    Services.prefs.setCharPref("devtools.debugger.remote-host", host);
    Services.prefs.setIntPref("devtools.debugger.remote-port", port);

    this.toggleHUD({
      host: host,
      port: port,
    });
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

const HUDService = new HUD_SERVICE();

