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

XPCOMUtils.defineLazyModuleGetter(this, "gDevTools",
    "resource:///modules/devtools/gDevTools.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "TargetFactory",
    "resource:///modules/devtools/Target.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
    "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "WebConsoleUtils",
    "resource://gre/modules/devtools/WebConsoleUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
    "resource://gre/modules/commonjs/sdk/core/promise.js");

const STRINGS_URI = "chrome://browser/locale/devtools/webconsole.properties";
let l10n = new WebConsoleUtils.l10n(STRINGS_URI);

this.EXPORTED_SYMBOLS = ["HUDService"];

///////////////////////////////////////////////////////////////////////////
//// The HUD service

function HUD_SERVICE()
{
  this.hudReferences = {};
}

HUD_SERVICE.prototype =
{
  /**
   * Keeps a reference for each HeadsUpDisplay that is created
   * @type object
   */
  hudReferences: null,

  /**
   * getter for UI commands to be used by the frontend
   *
   * @returns object
   */
  get consoleUI() {
    return HeadsUpDisplayUICommands;
  },

  /**
   * Firefox-specific current tab getter
   *
   * @returns nsIDOMWindow
   */
  currentContext: function HS_currentContext() {
    return Services.wm.getMostRecentWindow("navigator:browser");
  },

  /**
   * Open a Web Console for the given target.
   *
   * @see devtools/framework/Target.jsm for details about targets.
   *
   * @param object aTarget
   *        The target that the web console will connect to.
   * @param nsIDOMElement aIframe
   *        The iframe element into which to place the web console.
   * @return object
   *         A Promise object for the opening of the new WebConsole instance.
   */
  openWebConsole: function HS_openWebConsole(aTarget, aIframe)
  {
    let hud = new WebConsole(aTarget, aIframe);
    this.hudReferences[hud.hudId] = hud;
    return hud.init();
  },

  /**
   * Returns the HeadsUpDisplay object associated to a content window.
   *
   * @param nsIDOMWindow aContentWindow
   * @returns object
   */
  getHudByWindow: function HS_getHudByWindow(aContentWindow)
  {
    for each (let hud in this.hudReferences) {
      let target = hud.target;
      if (target && target.tab && target.window === aContentWindow) {
        return hud;
      }
    }
    return null;
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
    let hud = this.getHudByWindow(aContentWindow);
    return hud ? hud.hudId : null;
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
};


/**
 * A WebConsole instance is an interactive console initialized *per target*
 * that displays console log data as well as provides an interactive terminal to
 * manipulate the target's document content.
 *
 * This object only wraps the iframe that holds the Web Console UI.
 *
 * @constructor
 * @param object aTarget
 *        The target that the web console will connect to.
 * @param nsIDOMElement aIframe
 *        iframe into which we should create the WebConsole UI.
 */
function WebConsole(aTarget, aIframe)
{
  this.iframe = aIframe;
  this.iframe.className = "web-console-frame";
  this.chromeDocument = this.iframe.ownerDocument;
  this.chromeWindow = this.chromeDocument.defaultView;
  this.hudId = "hud_" + Date.now();
  this.target = aTarget;

  this.browserWindow = this.chromeWindow.top;
  let element = this.browserWindow.document.documentElement;
  if (element.getAttribute("windowtype") != "navigator:browser") {
    this.browserWindow = HUDService.currentContext();
  }
}

WebConsole.prototype = {
  chromeWindow: null,
  chromeDocument: null,
  hudId: null,
  target: null,
  iframe: null,
  _destroyer: null,

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
    return this.browserWindow.document.getElementById("mainPopupSet");
  },

  /**
   * Getter for the output element that holds messages we display.
   * @type nsIDOMElement
   */
  get outputNode()
  {
    return this.ui ? this.ui.outputNode : null;
  },

  get gViewSourceUtils() this.browserWindow.gViewSourceUtils,

  /**
   * Initialize the Web Console instance.
   *
   * @return object
   *         A Promise for the initialization.
   */
  init: function WC_init()
  {
    let deferred = Promise.defer();

    let onIframeLoad = function() {
      this.iframe.removeEventListener("load", onIframeLoad, true);
      initUI();
    }.bind(this);

    let initUI = function() {
      this.iframeWindow = this.iframe.contentWindow.wrappedJSObject;
      this.ui = new this.iframeWindow.WebConsoleFrame(this);
      this.ui.init().then(onSuccess, onFailure);
    }.bind(this);

    let onSuccess = function() {
      deferred.resolve(this);
    }.bind(this);

    let onFailure = function(aReason) {
      deferred.reject(aReason);
    };

    let win, doc;
    if ((win = this.iframe.contentWindow) &&
        (doc = win.document) &&
        doc.readyState == "complete") {
      initUI();
    }
    else {
      this.iframe.addEventListener("load", onIframeLoad, true);
    }
    return deferred.promise;
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
    if (this.target.isLocalTab) {
      this.browserWindow.DeveloperToolbar.resetErrorsCount(this.target.tab);
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
    this.browserWindow.openUILinkIn(aLink, "tab");
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
    let styleSheets = {};
    if (this.target.isLocalTab) {
      styleSheets = this.target.window.document.styleSheets;
    }
    for each (let style in styleSheets) {
      if (style.href == aSourceURL) {
        gDevTools.showToolbox(this.target, "styleeditor").then(function(toolbox) {
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
   * @return object
   *         A Promise object that is resolved once the Web Console is closed.
   */
  destroy: function WC_destroy()
  {
    if (this._destroyer) {
      return this._destroyer.promise;
    }

    delete HUDService.hudReferences[this.hudId];

    this._destroyer = Promise.defer();

    let popupset = this.mainPopupSet;
    let panels = popupset.querySelectorAll("panel[hudId=" + this.hudId + "]");
    for (let panel of panels) {
      panel.hidePopup();
    }

    let onDestroy = function WC_onDestroyUI() {
      try {
        let tabWindow = this.target.isLocalTab ? this.target.window : null;
        tabWindow && tabWindow.focus();
      }
      catch (ex) {
        // Tab focus can fail if the tab or target is closed.
      }

      let id = WebConsoleUtils.supportsString(this.hudId);
      Services.obs.notifyObservers(id, "web-console-destroyed", null);
      this._destroyer.resolve(null);
    }.bind(this);

    if (this.ui) {
      this.ui.destroy().then(onDestroy);
    }
    else {
      onDestroy();
    }

    return this._destroyer.promise;
  },
};

//////////////////////////////////////////////////////////////////////////
// HeadsUpDisplayUICommands
//////////////////////////////////////////////////////////////////////////

var HeadsUpDisplayUICommands = {
  /**
   * Toggle the Web Console for the current tab.
   *
   * @return object
   *         A Promise for either the opening of the toolbox that holds the Web
   *         Console, or a Promise for the closing of the toolbox.
   */
  toggleHUD: function UIC_toggleHUD()
  {
    let window = HUDService.currentContext();
    let target = TargetFactory.forTab(window.gBrowser.selectedTab);
    let toolbox = gDevTools.getToolbox(target);

    return toolbox && toolbox.currentToolId == "webconsole" ?
        toolbox.destroy() :
        gDevTools.showToolbox(target, "webconsole");
  },

  /**
   * Find if there is a Web Console open for the current tab and return the
   * instance.
   * @return object|null
   *         The WebConsole object or null if the active tab has no open Web
   *         Console.
   */
  getOpenHUD: function UIC_getOpenHUD()
  {
    let tab = HUDService.currentContext().gBrowser.selectedTab;
    if (!tab || !TargetFactory.isKnownTab(tab)) {
      return null;
    }
    let target = TargetFactory.forTab(tab);
    let toolbox = gDevTools.getToolbox(target);
    let panel = toolbox ? toolbox.getPanel("webconsole") : null;
    return panel ? panel.hud : null;
  },
};

const HUDService = new HUD_SERVICE();
