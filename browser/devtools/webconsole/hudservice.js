/* -*- js2-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cu} = require("chrome");

let WebConsoleUtils = require("devtools/toolkit/webconsole/utils").Utils;
let Heritage = require("sdk/core/heritage");

loader.lazyGetter(this, "promise", () => require("sdk/core/promise"));
loader.lazyGetter(this, "Telemetry", () => require("devtools/shared/telemetry"));
loader.lazyGetter(this, "WebConsoleFrame", () => require("devtools/webconsole/webconsole").WebConsoleFrame);
loader.lazyImporter(this, "gDevTools", "resource:///modules/devtools/gDevTools.jsm");
loader.lazyImporter(this, "devtools", "resource://gre/modules/devtools/Loader.jsm");
loader.lazyImporter(this, "Services", "resource://gre/modules/Services.jsm");
loader.lazyImporter(this, "DebuggerServer", "resource://gre/modules/devtools/dbg-server.jsm");
loader.lazyImporter(this, "DebuggerClient", "resource://gre/modules/devtools/dbg-client.jsm");

const STRINGS_URI = "chrome://browser/locale/devtools/webconsole.properties";
let l10n = new WebConsoleUtils.l10n(STRINGS_URI);

const BROWSER_CONSOLE_WINDOW_FEATURES = "chrome,titlebar,toolbar,centerscreen,resizable,dialog=no";

// The preference prefix for all of the Browser Console filters.
const BROWSER_CONSOLE_FILTER_PREFS_PREFIX = "devtools.browserconsole.filter.";

///////////////////////////////////////////////////////////////////////////
//// The HUD service

function HUD_SERVICE()
{
  this.consoles = new Map();
  this.lastFinishedRequest = { callback: null };
}

HUD_SERVICE.prototype =
{
  _browserConsoleID: null,
  _browserConsoleDefer: null,

  /**
   * Keeps a reference for each Web Console / Browser Console that is created.
   * @type Map
   */
  consoles: null,

  /**
   * Assign a function to this property to listen for every request that
   * completes. Used by unit tests. The callback takes one argument: the HTTP
   * activity object as received from the remote Web Console.
   *
   * @type object
   *       Includes a property named |callback|. Assign the function to the
   *       |callback| property of this object.
   */
  lastFinishedRequest: null,

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
   *         A promise object for the opening of the new WebConsole instance.
   */
  openWebConsole:
  function HS_openWebConsole(aTarget, aIframeWindow, aChromeWindow)
  {
    let hud = new WebConsole(aTarget, aIframeWindow, aChromeWindow);
    this.consoles.set(hud.hudId, hud);
    return hud.init();
  },

  /**
   * Open a Browser Console for the given target.
   *
   * @see devtools/framework/target.js for details about targets.
   *
   * @param object aTarget
   *        The target that the browser console will connect to.
   * @param nsIDOMWindow aIframeWindow
   *        The window where the browser console UI is already loaded.
   * @param nsIDOMWindow aChromeWindow
   *        The window of the browser console owner.
   * @return object
   *         A promise object for the opening of the new BrowserConsole instance.
   */
  openBrowserConsole:
  function HS_openBrowserConsole(aTarget, aIframeWindow, aChromeWindow)
  {
    let hud = new BrowserConsole(aTarget, aIframeWindow, aChromeWindow);
    this.consoles.set(hud.hudId, hud);
    return hud.init();
  },

  /**
   * Returns the Web Console object associated to a content window.
   *
   * @param nsIDOMWindow aContentWindow
   * @returns object
   */
  getHudByWindow: function HS_getHudByWindow(aContentWindow)
  {
    for (let [hudId, hud] of this.consoles) {
      let target = hud.target;
      if (target && target.tab && target.window === aContentWindow) {
        return hud;
      }
    }
    return null;
  },

  /**
   * Returns the console instance for a given id.
   *
   * @param string aId
   * @returns Object
   */
  getHudReferenceById: function HS_getHudReferenceById(aId)
  {
    return this.consoles.get(aId);
  },

  /**
   * Toggle the Web Console for the current tab.
   *
   * @return object
   *         A promise for either the opening of the toolbox that holds the Web
   *         Console, or a Promise for the closing of the toolbox.
   */
  toggleWebConsole: function HS_toggleWebConsole()
  {
    let window = this.currentContext();
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
  getOpenWebConsole: function HS_getOpenWebConsole()
  {
    let tab = this.currentContext().gBrowser.selectedTab;
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
  toggleBrowserConsole: function HS_toggleBrowserConsole()
  {
    if (this._browserConsoleID) {
      let hud = this.getHudReferenceById(this._browserConsoleID);
      return hud.destroy();
    }

    if (this._browserConsoleDefer) {
      return this._browserConsoleDefer.promise;
    }

    this._browserConsoleDefer = promise.defer();

    function connect()
    {
      let deferred = promise.defer();

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

      let deferred = promise.defer();

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

    connect().then(getTarget).then(openWindow).then((aWindow) => {
      this.openBrowserConsole(target, aWindow, aWindow)
        .then((aBrowserConsole) => {
          this._browserConsoleID = aBrowserConsole.hudId;
          this._browserConsoleDefer.resolve(aBrowserConsole);
          this._browserConsoleDefer = null;
        })
    }, console.error);

    return this._browserConsoleDefer.promise;
  },

  /**
   * Get the Browser Console instance, if open.
   *
   * @return object|null
   *         A BrowserConsole instance or null if the Browser Console is not
   *         open.
   */
  getBrowserConsole: function HS_getBrowserConsole()
  {
    return this.getHudReferenceById(this._browserConsoleID);
  },
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

  this.ui = new WebConsoleFrame(this);
}

WebConsole.prototype = {
  iframeWindow: null,
  chromeWindow: null,
  browserWindow: null,
  hudId: null,
  target: null,
  ui: null,
  _browserConsole: false,
  _destroyer: null,

  /**
   * Getter for a function to to listen for every request that completes. Used
   * by unit tests. The callback takes one argument: the HTTP activity object as
   * received from the remote Web Console.
   *
   * @type function
   */
  get lastFinishedRequestCallback() HUDService.lastFinishedRequest.callback,

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
   *         A promise for the initialization.
   */
  init: function WC_init()
  {
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
    let toolbox = gDevTools.getToolbox(this.target);
    if (!toolbox) {
      this.viewSource(aSourceURL, aSourceLine);
      return;
    }

    let showSource = ({ DebuggerView }) => {
      if (DebuggerView.Sources.containsValue(aSourceURL)) {
        DebuggerView.setEditorLocation(aSourceURL, aSourceLine, { noDebug: true });
        return;
      }
      toolbox.selectTool("webconsole");
      this.viewSource(aSourceURL, aSourceLine);
    }

    // If the Debugger was already open, switch to it and try to show the
    // source immediately. Otherwise, initialize it and wait for the sources
    // to be added first.
    let debuggerAlreadyOpen = toolbox.getPanel("jsdebugger");
    toolbox.selectTool("jsdebugger").then(({ panelWin: dbg }) => {
      if (debuggerAlreadyOpen) {
        showSource(dbg);
      } else {
        dbg.once(dbg.EVENTS.SOURCES_ADDED, () => showSource(dbg));
      }
    });
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
    let framesController = panel.panelWin.DebuggerController.StackFrames;
    let thread = framesController.activeThread;
    if (thread && thread.paused) {
      return {
        frames: thread.cachedFrames,
        selected: framesController.currentFrameDepth,
      };
    }
    return null;
  },

  /**
   * Destroy the object. Call this method to avoid memory leaks when the Web
   * Console is closed.
   *
   * @return object
   *         A promise object that is resolved once the Web Console is closed.
   */
  destroy: function WC_destroy()
  {
    if (this._destroyer) {
      return this._destroyer.promise;
    }

    HUDService.consoles.delete(this.hudId);

    this._destroyer = promise.defer();

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

BrowserConsole.prototype = Heritage.extend(WebConsole.prototype,
{
  _browserConsole: true,
  _bc_init: null,
  _bc_destroyer: null,

  $init: WebConsole.prototype.init,

  /**
   * Initialize the Browser Console instance.
   *
   * @return object
   *         A promise for the initialization.
   */
  init: function BC_init()
  {
    if (this._bc_init) {
      return this._bc_init;
    }

    this.ui._filterPrefsPrefix = BROWSER_CONSOLE_FILTER_PREFS_PREFIX;

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
   *         A promise object that is resolved once the Browser Console is closed.
   */
  destroy: function BC_destroy()
  {
    if (this._bc_destroyer) {
      return this._bc_destroyer.promise;
    }

    this._telemetry.toolClosed("browserconsole");

    this._bc_destroyer = promise.defer();

    let chromeWindow = this.chromeWindow;
    this.$destroy().then(() =>
      this.target.client.close(() => {
        HUDService._browserConsoleID = null;
        chromeWindow.close();
        this._bc_destroyer.resolve(null);
      }));

    return this._bc_destroyer.promise;
  },
});

const HUDService = new HUD_SERVICE();

(() => {
  let methods = ["openWebConsole", "openBrowserConsole", "toggleWebConsole",
                 "toggleBrowserConsole", "getOpenWebConsole",
                 "getBrowserConsole", "getHudByWindow", "getHudReferenceById"];
  for (let method of methods) {
    exports[method] = HUDService[method].bind(HUDService);
  }

  exports.consoles = HUDService.consoles;
  exports.lastFinishedRequest = HUDService.lastFinishedRequest;
})();
