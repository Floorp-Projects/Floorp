/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Services = require("Services");
loader.lazyRequireGetter(this, "Utils", "devtools/client/webconsole/utils", true);
loader.lazyRequireGetter(this, "extend", "devtools/shared/extend", true);
loader.lazyRequireGetter(this, "TargetFactory", "devtools/client/framework/target", true);
loader.lazyRequireGetter(this, "Tools", "devtools/client/definitions", true);
loader.lazyRequireGetter(this, "Telemetry", "devtools/client/shared/telemetry");
loader.lazyRequireGetter(this, "WebConsoleFrame", "devtools/client/webconsole/webconsole-frame", true);
loader.lazyRequireGetter(this, "gDevTools", "devtools/client/framework/devtools", true);
loader.lazyRequireGetter(this, "DebuggerServer", "devtools/server/main", true);
loader.lazyRequireGetter(this, "DebuggerClient", "devtools/shared/client/debugger-client", true);
loader.lazyRequireGetter(this, "viewSource", "devtools/client/shared/view-source");
loader.lazyRequireGetter(this, "l10n", "devtools/client/webconsole/webconsole-l10n");
const BC_WINDOW_FEATURES = "chrome,titlebar,toolbar,centerscreen,resizable,dialog=no";

// The preference prefix for all of the Browser Console filters.
const BC_FILTER_PREFS_PREFIX = "devtools.browserconsole.filter.";

var gHudId = 0;

function HUD_SERVICE() {
  this.consoles = new Map();
  this.lastFinishedRequest = { callback: null };
}

HUD_SERVICE.prototype =
{
  _browserConsoleID: null,
  _browserConsoleInitializing: null,

  /**
   * Keeps a reference for each Web Console / Browser Console that is created.
   * @type Map
   */
  consoles: null,

  _browerConsoleSessionState: false,
  storeBrowserConsoleSessionState() {
    this._browerConsoleSessionState = !!this.getBrowserConsole();
  },
  getBrowserConsoleSessionState() {
    return this._browerConsoleSessionState;
  },

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
   * Get the current context, which is the main application window.
   *
   * @returns nsIDOMWindow
   */
  currentContext() {
    return Services.wm.getMostRecentWindow(gDevTools.chromeWindowType);
  },

  /**
   * Open a Web Console for the given target.
   *
   * @see devtools/framework/target.js for details about targets.
   *
   * @param object target
   *        The target that the web console will connect to.
   * @param nsIDOMWindow iframeWindow
   *        The window where the web console UI is already loaded.
   * @param nsIDOMWindow chromeWindow
   *        The window of the web console owner.
   * @return object
   *         A promise object for the opening of the new WebConsole instance.
   */
  openWebConsole(target, iframeWindow, chromeWindow) {
    let hud = new WebConsole(target, iframeWindow, chromeWindow);
    this.consoles.set(hud.hudId, hud);
    return hud.init();
  },

  /**
   * Open a Browser Console for the given target.
   *
   * @see devtools/framework/target.js for details about targets.
   *
   * @param object target
   *        The target that the browser console will connect to.
   * @param nsIDOMWindow iframeWindow
   *        The window where the browser console UI is already loaded.
   * @param nsIDOMWindow chromeWindow
   *        The window of the browser console owner.
   * @return object
   *         A promise object for the opening of the new BrowserConsole instance.
   */
  openBrowserConsole(target, iframeWindow, chromeWindow) {
    let hud = new BrowserConsole(target, iframeWindow, chromeWindow);
    this._browserConsoleID = hud.hudId;
    this.consoles.set(hud.hudId, hud);
    return hud.init();
  },

  /**
   * Returns the Web Console object associated to a content window.
   *
   * @param nsIDOMWindow contentWindow
   * @returns object
   */
  getHudByWindow(contentWindow) {
    for (let [, hud] of this.consoles) {
      let target = hud.target;
      if (target && target.tab && target.window === contentWindow) {
        return hud;
      }
    }
    return null;
  },

  /**
   * Returns the console instance for a given id.
   *
   * @param string id
   * @returns Object
   */
  getHudReferenceById(id) {
    return this.consoles.get(id);
  },

  /**
   * Find if there is a Web Console open for the current tab and return the
   * instance.
   * @return object|null
   *         The WebConsole object or null if the active tab has no open Web
   *         Console.
   */
  getOpenWebConsole() {
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
  async toggleBrowserConsole() {
    if (this._browserConsoleID) {
      let hud = this.getHudReferenceById(this._browserConsoleID);
      return hud.destroy();
    }

    if (this._browserConsoleInitializing) {
      return this._browserConsoleInitializing;
    }

    async function connect() {
      // Ensure that the root actor and the tab actors have been registered on the
      // DebuggerServer, so that the Browser Console can retrieve the console actors.
      // (See Bug 1416105 for rationale).
      DebuggerServer.init();
      DebuggerServer.registerActors({ root: true, tab: true });

      DebuggerServer.allowChromeProcess = true;

      let client = new DebuggerClient(DebuggerServer.connectPipe());
      await client.connect();
      let response = await client.getProcess();
      return { form: response.form, client, chrome: true, isTabActor: true };
    }

    async function openWindow(t) {
      let win = Services.ww.openWindow(null, Tools.webConsole.browserConsoleURL,
                                       "_blank", BC_WINDOW_FEATURES, null);
      let iframeWindow = win;

      await new Promise(resolve => {
        win.addEventListener("DOMContentLoaded", resolve, {once: true});
      });

      win.document.title = l10n.getStr("browserConsole.title");

      // With a XUL wrapper doc, we host webconsole.html in an iframe.
      // Wait for that to be ready before resolving:
      if (!Tools.webConsole.browserConsoleUsesHTML) {
        let iframe = win.document.querySelector("iframe");
        await new Promise(resolve => {
          iframe.addEventListener("DOMContentLoaded", resolve, {once: true});
        });
        iframeWindow = iframe.contentWindow;
      }

      return {iframeWindow, chromeWindow: win};
    }

    // Temporarily cache the async startup sequence so that if toggleBrowserConsole
    // gets called again we can return this console instead of opening another one.
    this._browserConsoleInitializing = (async () => {
      let connection = await connect();
      let target = await TargetFactory.forRemoteTab(connection);
      let {iframeWindow, chromeWindow} = await openWindow(target);
      let browserConsole =
        await this.openBrowserConsole(target, iframeWindow, chromeWindow);
      return browserConsole;
    })();

    let browserConsole = await this._browserConsoleInitializing;
    this._browserConsoleInitializing = null;
    return browserConsole;
  },

  /**
   * Opens or focuses the Browser Console.
   */
  openBrowserConsoleOrFocus() {
    let hud = this.getBrowserConsole();
    if (hud) {
      hud.iframeWindow.focus();
      return Promise.resolve(hud);
    }

    return this.toggleBrowserConsole();
  },

  /**
   * Get the Browser Console instance, if open.
   *
   * @return object|null
   *         A BrowserConsole instance or null if the Browser Console is not
   *         open.
   */
  getBrowserConsole() {
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
 * @param object target
 *        The target that the web console will connect to.
 * @param nsIDOMWindow iframeWindow
 *        The window where the web console UI is already loaded.
 * @param nsIDOMWindow chromeWindow
 *        The window of the web console owner.
 */
function WebConsole(target, iframeWindow, chromeWindow) {
  this.iframeWindow = iframeWindow;
  this.chromeWindow = chromeWindow;
  this.hudId = "hud_" + ++gHudId;
  this.target = target;
  this.browserWindow = this.chromeWindow.top;
  let element = this.browserWindow.document.documentElement;
  if (element.getAttribute("windowtype") != gDevTools.chromeWindowType) {
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
  get lastFinishedRequestCallback() {
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
  get chromeUtilsWindow() {
    if (this.browserWindow) {
      return this.browserWindow;
    }
    return this.chromeWindow.top;
  },

  /**
   * Getter for the xul:popupset that holds any popups we open.
   * @type Element
   */
  get mainPopupSet() {
    return this.chromeUtilsWindow.document.getElementById("mainPopupSet");
  },

  /**
   * Getter for the output element that holds messages we display.
   * @type Element
   */
  get outputNode() {
    return this.ui ? this.ui.outputNode : null;
  },

  get gViewSourceUtils() {
    return this.chromeUtilsWindow.gViewSourceUtils;
  },

  /**
   * Initialize the Web Console instance.
   *
   * @return object
   *         A promise for the initialization.
   */
  init() {
    return this.ui.init().then(() => this);
  },

  /**
   * The JSTerm object that manages the console's input.
   * @see webconsole.js::JSTerm
   * @type object
   */
  get jsterm() {
    return this.ui ? this.ui.jsterm : null;
  },

  /**
   * Alias for the WebConsoleFrame.setFilterState() method.
   * @see webconsole.js::WebConsoleFrame.setFilterState()
   */
  setFilterState() {
    this.ui && this.ui.setFilterState.apply(this.ui, arguments);
  },

  /**
   * Open a link in a new tab.
   *
   * @param string link
   *        The URL you want to open in a new tab.
   */
  openLink(link, e) {
    let isOSX = Services.appinfo.OS == "Darwin";
    let where = "tab";
    if (e && (e.button === 1 || (e.button === 0 && (isOSX ? e.metaKey : e.ctrlKey)))) {
      where = "tabshifted";
    }
    this.chromeUtilsWindow.openWebLinkIn(link, where);
  },

  /**
   * Open a link in Firefox's view source.
   *
   * @param string sourceURL
   *        The URL of the file.
   * @param integer sourceLine
   *        The line number which should be highlighted.
   */
  viewSource(sourceURL, sourceLine) {
    this.gViewSourceUtils.viewSource({ URL: sourceURL, lineNumber: sourceLine || 0 });
  },

  /**
   * Tries to open a Stylesheet file related to the web page for the web console
   * instance in the Style Editor. If the file is not found, it is opened in
   * source view instead.
   *
   * Manually handle the case where toolbox does not exist (Browser Console).
   *
   * @param string sourceURL
   *        The URL of the file.
   * @param integer sourceLine
   *        The line number which you want to place the caret.
   */
  viewSourceInStyleEditor(sourceURL, sourceLine) {
    let toolbox = gDevTools.getToolbox(this.target);
    if (!toolbox) {
      this.viewSource(sourceURL, sourceLine);
      return;
    }
    toolbox.viewSourceInStyleEditor(sourceURL, sourceLine);
  },

  /**
   * Tries to open a JavaScript file related to the web page for the web console
   * instance in the Script Debugger. If the file is not found, it is opened in
   * source view instead.
   *
   * Manually handle the case where toolbox does not exist (Browser Console).
   *
   * @param string sourceURL
   *        The URL of the file.
   * @param integer sourceLine
   *        The line number which you want to place the caret.
   */
  viewSourceInDebugger(sourceURL, sourceLine) {
    let toolbox = gDevTools.getToolbox(this.target);
    if (!toolbox) {
      this.viewSource(sourceURL, sourceLine);
      return;
    }
    toolbox.viewSourceInDebugger(sourceURL, sourceLine).then(() => {
      this.ui.emit("source-in-debugger-opened");
    });
  },

  /**
   * Tries to open a JavaScript file related to the web page for the web console
   * instance in the corresponding Scratchpad.
   *
   * @param string sourceURL
   *        The URL of the file which corresponds to a Scratchpad id.
   */
  viewSourceInScratchpad(sourceURL, sourceLine) {
    viewSource.viewSourceInScratchpad(sourceURL, sourceLine);
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
  getDebuggerFrames() {
    let toolbox = gDevTools.getToolbox(this.target);
    if (!toolbox) {
      return null;
    }
    let panel = toolbox.getPanel("jsdebugger");

    if (!panel) {
      return null;
    }

    return panel.getFrames();
  },

  async getMappedExpression(expression) {
    let toolbox = gDevTools.getToolbox(this.target);
    if (!toolbox) {
      return expression;
    }
    let panel = toolbox.getPanel("jsdebugger");

    if (!panel) {
      return expression;
    }

    return panel.getMappedExpression(expression);
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
  getInspectorSelection() {
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
  async destroy() {
    if (this._destroyer) {
      return this._destroyer;
    }

    this._destroyer = (async () => {
      HUDService.consoles.delete(this.hudId);

      // The document may already be removed
      if (this.chromeUtilsWindow && this.mainPopupSet) {
        let popupset = this.mainPopupSet;
        let panels = popupset.querySelectorAll("panel[hudId=" + this.hudId + "]");
        for (let panel of panels) {
          panel.hidePopup();
        }
      }

      if (this.ui) {
        await this.ui.destroy();
      }

      if (!this._browserConsole) {
        try {
          await this.target.activeTab.focus();
        } catch (ex) {
          // Tab focus can fail if the tab or target is closed.
        }
      }

      let id = Utils.supportsString(this.hudId);
      Services.obs.notifyObservers(id, "web-console-destroyed");
    })();

    return this._destroyer;
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
 * @param object target
 *        The target that the browser console will connect to.
 * @param nsIDOMWindow iframeWindow
 *        The window where the browser console UI is already loaded.
 * @param nsIDOMWindow chromeWindow
 *        The window of the browser console owner.
 */
function BrowserConsole() {
  WebConsole.apply(this, arguments);
  this._telemetry = new Telemetry();
}

BrowserConsole.prototype = extend(WebConsole.prototype, {
  _browserConsole: true,
  _bcInit: null,
  _bcDestroyer: null,

  $init: WebConsole.prototype.init,

  /**
   * Initialize the Browser Console instance.
   *
   * @return object
   *         A promise for the initialization.
   */
  init() {
    if (this._bcInit) {
      return this._bcInit;
    }

    // Only add the shutdown observer if we've opened a Browser Console window.
    ShutdownObserver.init();

    this.ui._filterPrefsPrefix = BC_FILTER_PREFS_PREFIX;

    let window = this.iframeWindow;

    // Make sure that the closing of the Browser Console window destroys this
    // instance.
    window.addEventListener("unload", () => {
      this.destroy();
    }, {once: true});

    this._telemetry.toolOpened("browserconsole");

    this._bcInit = this.$init();
    return this._bcInit;
  },

  $destroy: WebConsole.prototype.destroy,

  /**
   * Destroy the object.
   *
   * @return object
   *         A promise object that is resolved once the Browser Console is closed.
   */
  destroy() {
    if (this._bcDestroyer) {
      return this._bcDestroyer;
    }

    this._bcDestroyer = (async () => {
      this._telemetry.toolClosed("browserconsole");
      await this.$destroy();
      await this.target.client.close();
      HUDService._browserConsoleID = null;
      this.chromeWindow.close();
    })();

    return this._bcDestroyer;
  },
});

const HUDService = new HUD_SERVICE();
exports.HUDService = HUDService;

/**
 * The ShutdownObserver listens for app shutdown and saves the current state
 * of the Browser Console for session restore.
 */
var ShutdownObserver = {
  _initialized: false,
  init() {
    if (this._initialized) {
      return;
    }

    Services.obs.addObserver(this, "quit-application-granted");

    this._initialized = true;
  },

  observe(message, topic) {
    if (topic == "quit-application-granted") {
      HUDService.storeBrowserConsoleSessionState();
      this.uninit();
    }
  },

  uninit() {
    Services.obs.removeObserver(this, "quit-application-granted");
  }
};
