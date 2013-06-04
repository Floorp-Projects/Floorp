/* -*- Mode: js2; js2-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "gDevTools",
    "resource:///modules/devtools/gDevTools.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "devtools",
    "resource://gre/modules/devtools/Loader.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
    "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DebuggerServer",
  "resource://gre/modules/devtools/dbg-server.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DebuggerClient",
  "resource://gre/modules/devtools/dbg-client.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "WebConsoleUtils",
    "resource://gre/modules/devtools/WebConsoleUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
    "resource://gre/modules/commonjs/sdk/core/promise.js");

XPCOMUtils.defineLazyModuleGetter(this, "ViewHelpers",
    "resource:///modules/devtools/ViewHelpers.jsm");

let Telemetry = devtools.require("devtools/shared/telemetry");

const STRINGS_URI = "chrome://browser/locale/devtools/webconsole.properties";
let l10n = new WebConsoleUtils.l10n(STRINGS_URI);

const BROWSER_CONSOLE_WINDOW_FEATURES = "chrome,titlebar,toolbar,centerscreen,resizable,dialog=no";

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
   * @see devtools/framework/target.js for details about targets.
   *
   * @param object aTarget
   *        The target that the web console will connect to.
   * @param nsIDOMWindow aIframeWindow
   *        The window where the web console UI is already loaded.
   * @param nsIDOMWindow aChromeWindow
   *        The window of the web console owner.
   * @return object
   *         A Promise object for the opening of the new WebConsole instance.
   */
  openWebConsole:
  function HS_openWebConsole(aTarget, aIframeWindow, aChromeWindow)
  {
    let hud = new WebConsole(aTarget, aIframeWindow, aChromeWindow);
    this.hudReferences[hud.hudId] = hud;
    return hud.init();
  },

  /**
   * Open a Browser Console for the given target.
   *
   * @see devtools/framework/Target.jsm for details about targets.
   *
   * @param object aTarget
   *        The target that the browser console will connect to.
   * @param nsIDOMWindow aIframeWindow
   *        The window where the browser console UI is already loaded.
   * @param nsIDOMWindow aChromeWindow
   *        The window of the browser console owner.
   * @return object
   *         A Promise object for the opening of the new BrowserConsole instance.
   */
  openBrowserConsole:
  function HS_openBrowserConsole(aTarget, aIframeWindow, aChromeWindow)
  {
    let hud = new BrowserConsole(aTarget, aIframeWindow, aChromeWindow);
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
 * This object only wraps the iframe that holds the Web Console UI. This is
 * meant to be an integration point between the Firefox UI and the Web Console
 * UI and features.
 *
 * @constructor
 * @param object aTarget
 *        The target that the web console will connect to.
 * @param nsIDOMWindow aIframeWindow
 *        The window where the web console UI is already loaded.
 * @param nsIDOMWindow aChromeWindow
 *        The window of the web console owner.
 */
function WebConsole(aTarget, aIframeWindow, aChromeWindow)
{
  this.iframeWindow = aIframeWindow;
  this.chromeWindow = aChromeWindow;
  this.hudId = "hud_" + Date.now();
  this.target = aTarget;

  this.browserWindow = this.chromeWindow.top;

  let element = this.browserWindow.document.documentElement;
  if (element.getAttribute("windowtype") != "navigator:browser") {
    this.browserWindow = HUDService.currentContext();
  }
}

WebConsole.prototype = {
  iframeWindow: null,
  chromeWindow: null,
  browserWindow: null,
  hudId: null,
  target: null,
  _browserConsole: false,
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
    this.ui = new this.iframeWindow.WebConsoleFrame(this);
    return this.ui.init().then(() => this);
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
    let toolbox = gDevTools.getToolbox(this.target);
    if (!toolbox) {
      this.viewSource(aSourceURL, aSourceLine);
      return;
    }

    gDevTools.showToolbox(this.target, "styleeditor").then(function(toolbox) {
      try {
        toolbox.getCurrentPanel().selectStyleSheet(aSourceURL, aSourceLine);
      } catch(e) {
        // Open view source if style editor fails.
        this.viewSource(aSourceURL, aSourceLine);
      }
    });
  },

  /**
   * Tries to open a JavaScript file related to the web page for the web console
   * instance in the Script Debugger. If the file is not found, it is opened in
   * source view instead.
   *
   * @param string aSourceURL
   *        The URL of the file.
   * @param integer aSourceLine
   *        The line number which you want to place the caret.
   */
  viewSourceInDebugger:
  function WC_viewSourceInDebugger(aSourceURL, aSourceLine)
  {
    let self = this;
    let panelWin = null;
    let debuggerWasOpen = true;
    let toolbox = gDevTools.getToolbox(this.target);
    if (!toolbox) {
      self.viewSource(aSourceURL, aSourceLine);
      return;
    }

    if (!toolbox.getPanel("jsdebugger")) {
      debuggerWasOpen = false;
      let toolboxWin = toolbox.doc.defaultView;
      toolboxWin.addEventListener("Debugger:AfterSourcesAdded",
                                  function afterSourcesAdded() {
        toolboxWin.removeEventListener("Debugger:AfterSourcesAdded",
                                       afterSourcesAdded);
        loadScript();
      });
    }

    toolbox.selectTool("jsdebugger").then(function onDebuggerOpen(dbg) {
      panelWin = dbg.panelWin;
      if (debuggerWasOpen) {
        loadScript();
      }
    });

    function loadScript() {
      let debuggerView = panelWin.DebuggerView;
      if (!debuggerView.Sources.containsValue(aSourceURL)) {
        toolbox.selectTool("webconsole");
        self.viewSource(aSourceURL, aSourceLine);
        return;
      }
      if (debuggerWasOpen && debuggerView.Sources.selectedValue == aSourceURL) {
        debuggerView.editor.setCaretPosition(aSourceLine - 1);
        return;
      }

      panelWin.addEventListener("Debugger:SourceShown", onSource, false);
      debuggerView.Sources.preferredSource = aSourceURL;
    }

    function onSource(aEvent) {
      if (aEvent.detail.url != aSourceURL) {
        return;
      }
      panelWin.removeEventListener("Debugger:SourceShown", onSource, false);
      panelWin.DebuggerView.editor.setCaretPosition(aSourceLine - 1);
    }
  },

  /**
   * Retrieve information about the JavaScript debugger's stackframes list. This
   * is used to allow the Web Console to evaluate code in the selected
   * stackframe.
   *
   * @return object|null
   *         An object which holds:
   *         - frames: the active ThreadClient.cachedFrames array.
   *         - selected: depth/index of the selected stackframe in the debugger
   *         UI.
   *         If the debugger is not open or if it's not paused, then |null| is
   *         returned.
   */
  getDebuggerFrames: function WC_getDebuggerFrames()
  {
    let toolbox = gDevTools.getToolbox(this.target);
    if (!toolbox) {
      return null;
    }
    let panel = toolbox.getPanel("jsdebugger");
    if (!panel) {
      return null;
    }
    let framesController = panel.panelWin.gStackFrames;
    let thread = framesController.activeThread;
    if (thread && thread.paused) {
      return {
        frames: thread.cachedFrames,
        selected: framesController.currentFrame,
      };
    }
    return null;
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


/**
 * A BrowserConsole instance is an interactive console initialized *per target*
 * that displays console log data as well as provides an interactive terminal to
 * manipulate the target's document content.
 *
 * This object only wraps the iframe that holds the Browser Console UI. This is
 * meant to be an integration point between the Firefox UI and the Browser Console
 * UI and features.
 *
 * @constructor
 * @param object aTarget
 *        The target that the browser console will connect to.
 * @param nsIDOMWindow aIframeWindow
 *        The window where the browser console UI is already loaded.
 * @param nsIDOMWindow aChromeWindow
 *        The window of the browser console owner.
 */
function BrowserConsole()
{
  WebConsole.apply(this, arguments);
  this._telemetry = new Telemetry();
}

ViewHelpers.create({ constructor: BrowserConsole, proto: WebConsole.prototype },
{
  _browserConsole: true,
  _bc_init: null,
  _bc_destroyer: null,

  $init: WebConsole.prototype.init,

  /**
   * Initialize the Browser Console instance.
   *
   * @return object
   *         A Promise for the initialization.
   */
  init: function BC_init()
  {
    if (this._bc_init) {
      return this._bc_init;
    }

    let window = this.iframeWindow;

    // Make sure that the closing of the Browser Console window destroys this
    // instance.
    let onClose = () => {
      window.removeEventListener("unload", onClose);
      this.destroy();
    };
    window.addEventListener("unload", onClose);

    // Make sure Ctrl-W closes the Browser Console window.
    window.document.getElementById("cmd_close").removeAttribute("disabled");

    this._telemetry.toolOpened("browserconsole");

    this._bc_init = this.$init();
    return this._bc_init;
  },

  $destroy: WebConsole.prototype.destroy,

  /**
   * Destroy the object.
   *
   * @return object
   *         A Promise object that is resolved once the Browser Console is closed.
   */
  destroy: function BC_destroy()
  {
    if (this._bc_destroyer) {
      return this._bc_destroyer.promise;
    }

    this._telemetry.toolClosed("browserconsole");

    this._bc_destroyer = Promise.defer();

    let chromeWindow = this.chromeWindow;
    this.$destroy().then(() =>
      this.target.client.close(() => {
        HeadsUpDisplayUICommands._browserConsoleID = null;
        chromeWindow.close();
        this._bc_destroyer.resolve(null);
      }));

    return this._bc_destroyer.promise;
  },
});


//////////////////////////////////////////////////////////////////////////
// HeadsUpDisplayUICommands
//////////////////////////////////////////////////////////////////////////

var HeadsUpDisplayUICommands = {
  _browserConsoleID: null,
  _browserConsoleDefer: null,

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
    let target = devtools.TargetFactory.forTab(window.gBrowser.selectedTab);
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
    if (!tab || !devtools.TargetFactory.isKnownTab(tab)) {
      return null;
    }
    let target = devtools.TargetFactory.forTab(tab);
    let toolbox = gDevTools.getToolbox(target);
    let panel = toolbox ? toolbox.getPanel("webconsole") : null;
    return panel ? panel.hud : null;
  },

  /**
   * Toggle the Browser Console.
   */
  toggleBrowserConsole: function UIC_toggleBrowserConsole()
  {
    if (this._browserConsoleID) {
      let hud = HUDService.getHudReferenceById(this._browserConsoleID);
      return hud.destroy();
    }

    if (this._browserConsoleDefer) {
      return this._browserConsoleDefer.promise;
    }

    this._browserConsoleDefer = Promise.defer();

    function connect()
    {
      let deferred = Promise.defer();

      if (!DebuggerServer.initialized) {
        DebuggerServer.init();
        DebuggerServer.addBrowserActors();
      }

      let client = new DebuggerClient(DebuggerServer.connectPipe());
      client.connect(() =>
        client.listTabs((aResponse) => {
          // Add Global Process debugging...
          let globals = JSON.parse(JSON.stringify(aResponse));
          delete globals.tabs;
          delete globals.selected;
          // ...only if there are appropriate actors (a 'from' property will
          // always be there).
          if (Object.keys(globals).length > 1) {
            deferred.resolve({ form: globals, client: client, chrome: true });
          } else {
            deferred.reject("Global console not found!");
          }
        }));

      return deferred.promise;
    }

    let target;
    function getTarget(aConnection)
    {
      let options = {
        form: aConnection.form,
        client: aConnection.client,
        chrome: true,
      };

      return devtools.TargetFactory.forRemoteTab(options);
    }

    function openWindow(aTarget)
    {
      target = aTarget;

      let deferred = Promise.defer();

      let win = Services.ww.openWindow(null, devtools.Tools.webConsole.url, "_blank",
                                       BROWSER_CONSOLE_WINDOW_FEATURES, null);
      win.addEventListener("DOMContentLoaded", function onLoad() {
        win.removeEventListener("DOMContentLoaded", onLoad);

        // Set the correct Browser Console title.
        let root = win.document.documentElement;
        root.setAttribute("title", root.getAttribute("browserConsoleTitle"));

        deferred.resolve(win);
      });

      return deferred.promise;
    }

    connect().then(getTarget).then(openWindow).then((aWindow) =>
      HUDService.openBrowserConsole(target, aWindow, aWindow)
        .then((aBrowserConsole) => {
          this._browserConsoleID = aBrowserConsole.hudId;
          this._browserConsoleDefer.resolve(aBrowserConsole);
          this._browserConsoleDefer = null;
        }));

    return this._browserConsoleDefer.promise;
  },

  get browserConsole() {
    return HUDService.getHudReferenceById(this._browserConsoleID);
  },
};

const HUDService = new HUD_SERVICE();

