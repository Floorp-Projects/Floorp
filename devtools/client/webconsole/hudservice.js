/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft= javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cu} = require("chrome");

var WebConsoleUtils = require("devtools/shared/webconsole/utils").Utils;
var { extend } = require("sdk/core/heritage");
var {TargetFactory} = require("devtools/client/framework/target");
var {Tools} = require("devtools/client/definitions");
const { Task } = require("devtools/shared/task");
var promise = require("promise");
var Services = require("Services");

loader.lazyRequireGetter(this, "Telemetry", "devtools/client/shared/telemetry");
loader.lazyRequireGetter(this, "WebConsoleFrame", "devtools/client/webconsole/webconsole", true);
loader.lazyRequireGetter(this, "gDevTools", "devtools/client/framework/devtools", true);
loader.lazyRequireGetter(this, "DebuggerServer", "devtools/server/main", true);
loader.lazyRequireGetter(this, "DebuggerClient", "devtools/shared/client/main", true);
loader.lazyRequireGetter(this, "showDoorhanger", "devtools/client/shared/doorhanger", true);
loader.lazyRequireGetter(this, "viewSource", "devtools/client/shared/view-source");

const STRINGS_URI = "chrome://devtools/locale/webconsole.properties";
var l10n = new WebConsoleUtils.L10n(STRINGS_URI);

const BROWSER_CONSOLE_WINDOW_FEATURES = "chrome,titlebar,toolbar,centerscreen,resizable,dialog=no";

// The preference prefix for all of the Browser Console filters.
const BROWSER_CONSOLE_FILTER_PREFS_PREFIX = "devtools.browserconsole.filter.";

var gHudId = 0;

// The HUD service

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
    this._browserConsoleID = hud.hudId;
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
   * Find if there is a Web Console open for the current tab and return the
   * instance.
   * @return object|null
   *         The WebConsole object or null if the active tab has no open Web
   *         Console.
   */
  getOpenWebConsole: function HS_getOpenWebConsole()
  {
    let tab = this.currentContext().gBrowser.selectedTab;
    if (!tab || !TargetFactory.isKnownTab(tab)) {
      return null;
    }
    let target = TargetFactory.forTab(tab);
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
      DebuggerServer.allowChromeProcess = true;

      let client = new DebuggerClient(DebuggerServer.connectPipe());
      return client.connect()
        .then(() => client.getProcess())
        .then(aResponse => {
          // Set chrome:false in order to attach to the target
          // (i.e. send an `attach` request to the chrome actor)
          return { form: aResponse.form, client: client, chrome: false };
        });
    }

    let target;
    function getTarget(aConnection)
    {
      return TargetFactory.forRemoteTab(aConnection);
    }

    function openWindow(aTarget)
    {
      target = aTarget;

      let deferred = promise.defer();

      let win = Services.ww.openWindow(null, Tools.webConsole.url, "_blank",
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
      return this.openBrowserConsole(target, aWindow, aWindow)
        .then((aBrowserConsole) => {
          this._browserConsoleDefer.resolve(aBrowserConsole);
          this._browserConsoleDefer = null;
        });
    }, console.error.bind(console));

    return this._browserConsoleDefer.promise;
  },

  /**
   * Opens or focuses the Browser Console.
   */
  openBrowserConsoleOrFocus: function HS_openBrowserConsoleOrFocus()
  {
    let hud = this.getBrowserConsole();
    if (hud) {
      hud.iframeWindow.focus();
      return promise.resolve(hud);
    }
    else {
      return this.toggleBrowserConsole();
    }
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
  this.hudId = "hud_" + ++gHudId;
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
  get lastFinishedRequestCallback()
  {
    return HUDService.lastFinishedRequest.callback;
  },

  /**
   * Getter for the window that can provide various utilities that the web
   * console makes use of, like opening links, managing popups, etc.  In
   * most cases, this will be |this.browserWindow|, but in some uses (such as
   * the Browser Toolbox), there is no browser window, so an alternative window
   * hosts the utilities there.
   * @type nsIDOMWindow
   */
  get chromeUtilsWindow()
  {
    if (this.browserWindow) {
      return this.browserWindow;
    }
    return this.chromeWindow.top;
  },

  /**
   * Getter for the xul:popupset that holds any popups we open.
   * @type nsIDOMElement
   */
  get mainPopupSet()
  {
    return this.chromeUtilsWindow.document.getElementById("mainPopupSet");
  },

  /**
   * Getter for the output element that holds messages we display.
   * @type nsIDOMElement
   */
  get outputNode()
  {
    return this.ui ? this.ui.outputNode : null;
  },

  get gViewSourceUtils()
  {
    return this.chromeUtilsWindow.gViewSourceUtils;
  },

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
    this.chromeUtilsWindow.openUILinkIn(aLink, "tab");
  },

  /**
   * Open a link in Firefox's view source.
   *
   * @param string aSourceURL
   *        The URL of the file.
   * @param integer aSourceLine
   *        The line number which should be highlighted.
   */
  viewSource: function WC_viewSource(aSourceURL, aSourceLine) {
    // Attempt to access view source via a browser first, which may display it in
    // a tab, if enabled.
    let browserWin = Services.wm.getMostRecentWindow("navigator:browser");
    if (browserWin && browserWin.BrowserViewSourceOfDocument) {
      return browserWin.BrowserViewSourceOfDocument({
        URL: aSourceURL,
        lineNumber: aSourceLine
      });
    }
    this.gViewSourceUtils.viewSource(aSourceURL, null, this.iframeWindow.document, aSourceLine || 0);
  },

  /**
   * Tries to open a Stylesheet file related to the web page for the web console
   * instance in the Style Editor. If the file is not found, it is opened in
   * source view instead.
   *
   * Manually handle the case where toolbox does not exist (Browser Console).
   *
   * @param string aSourceURL
   *        The URL of the file.
   * @param integer aSourceLine
   *        The line number which you want to place the caret.
   */
  viewSourceInStyleEditor: function WC_viewSourceInStyleEditor(aSourceURL, aSourceLine) {
    let toolbox = gDevTools.getToolbox(this.target);
    if (!toolbox) {
      this.viewSource(aSourceURL, aSourceLine);
      return;
    }
    toolbox.viewSourceInStyleEditor(aSourceURL, aSourceLine);
  },

  /**
   * Tries to open a JavaScript file related to the web page for the web console
   * instance in the Script Debugger. If the file is not found, it is opened in
   * source view instead.
   *
   * Manually handle the case where toolbox does not exist (Browser Console).
   *
   * @param string aSourceURL
   *        The URL of the file.
   * @param integer aSourceLine
   *        The line number which you want to place the caret.
   */
  viewSourceInDebugger: function WC_viewSourceInDebugger(aSourceURL, aSourceLine) {
    let toolbox = gDevTools.getToolbox(this.target);
    if (!toolbox) {
      this.viewSource(aSourceURL, aSourceLine);
      return;
    }
    toolbox.viewSourceInDebugger(aSourceURL, aSourceLine).then(() => {
      this.ui.emit("source-in-debugger-opened");
    });
  },

  /**
   * Tries to open a JavaScript file related to the web page for the web console
   * instance in the corresponding Scratchpad.
   *
   * @param string aSourceURL
   *        The URL of the file which corresponds to a Scratchpad id.
   */
  viewSourceInScratchpad: function WC_viewSourceInScratchpad(aSourceURL, aSourceLine) {
    viewSource.viewSourceInScratchpad(aSourceURL, aSourceLine);
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
   * Retrieves the current selection from the Inspector, if such a selection
   * exists. This is used to pass the ID of the selected actor to the Web
   * Console server for the $0 helper.
   *
   * @return object|null
   *         A Selection referring to the currently selected node in the
   *         Inspector.
   *         If the inspector was never opened, or no node was ever selected,
   *         then |null| is returned.
   */
  getInspectorSelection: function WC_getInspectorSelection()
  {
    let toolbox = gDevTools.getToolbox(this.target);
    if (!toolbox) {
      return null;
    }
    let panel = toolbox.getPanel("inspector");
    if (!panel || !panel.selection) {
      return null;
    }
    return panel.selection;
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

    // The document may already be removed
    if (this.chromeUtilsWindow && this.mainPopupSet) {
      let popupset = this.mainPopupSet;
      let panels = popupset.querySelectorAll("panel[hudId=" + this.hudId + "]");
      for (let panel of panels) {
        panel.hidePopup();
      }
    }

    let onDestroy = Task.async(function* () {
      if (!this._browserConsole) {
        try {
          yield this.target.activeTab.focus();
        }
        catch (ex) {
          // Tab focus can fail if the tab or target is closed.
        }
      }

      let id = WebConsoleUtils.supportsString(this.hudId);
      Services.obs.notifyObservers(id, "web-console-destroyed", null);
      this._destroyer.resolve(null);
    }.bind(this));

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

BrowserConsole.prototype = extend(WebConsole.prototype, {
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
      window.removeEventListener("focus", onFocus);
      this.destroy();
    };
    window.addEventListener("unload", onClose);

    // Make sure Ctrl-W closes the Browser Console window.
    window.document.getElementById("cmd_close").removeAttribute("disabled");

    this._telemetry.toolOpened("browserconsole");

    // Create an onFocus handler just to display the dev edition promo.
    // This is to prevent race conditions in some environments.
    // Hook to display promotional Developer Edition doorhanger. Only displayed once.
    let onFocus = () => showDoorhanger({ window, type: "deveditionpromo" });
    window.addEventListener("focus", onFocus);

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
  let methods = ["openWebConsole", "openBrowserConsole",
                 "toggleBrowserConsole", "getOpenWebConsole",
                 "getBrowserConsole", "getHudByWindow",
                 "openBrowserConsoleOrFocus", "getHudReferenceById"];
  for (let method of methods) {
    exports[method] = HUDService[method].bind(HUDService);
  }

  exports.consoles = HUDService.consoles;
  exports.lastFinishedRequest = HUDService.lastFinishedRequest;
})();
