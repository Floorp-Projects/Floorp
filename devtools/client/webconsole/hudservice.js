/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Services = require("Services");
loader.lazyRequireGetter(this, "TargetFactory", "devtools/client/framework/target", true);
loader.lazyRequireGetter(this, "Tools", "devtools/client/definitions", true);
loader.lazyRequireGetter(this, "gDevTools", "devtools/client/framework/devtools", true);
loader.lazyRequireGetter(this, "DebuggerServer", "devtools/server/main", true);
loader.lazyRequireGetter(this, "DebuggerClient", "devtools/shared/client/debugger-client", true);
loader.lazyRequireGetter(this, "l10n", "devtools/client/webconsole/webconsole-l10n");
loader.lazyRequireGetter(this, "WebConsole", "devtools/client/webconsole/webconsole");
loader.lazyRequireGetter(this, "BrowserConsole", "devtools/client/webconsole/browser-console");

const BC_WINDOW_FEATURES = "chrome,titlebar,toolbar,centerscreen,resizable,dialog=no";

function HUDService() {
  this.consoles = new Map();
  this.lastFinishedRequest = { callback: null };
}

HUDService.prototype = {
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
    const hud = new WebConsole(target, iframeWindow, chromeWindow, this);
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
    const hud = new BrowserConsole(target, iframeWindow, chromeWindow, this);
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
    for (const [, hud] of this.consoles) {
      const target = hud.target;
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
    const tab = this.currentContext().gBrowser.selectedTab;
    if (!tab || !TargetFactory.isKnownTab(tab)) {
      return null;
    }
    const target = TargetFactory.forTab(tab);
    const toolbox = gDevTools.getToolbox(target);
    const panel = toolbox ? toolbox.getPanel("webconsole") : null;
    return panel ? panel.hud : null;
  },

  /**
   * Toggle the Browser Console.
   */
  async toggleBrowserConsole() {
    if (this._browserConsoleID) {
      const hud = this.getHudReferenceById(this._browserConsoleID);
      return hud.destroy();
    }

    if (this._browserConsoleInitializing) {
      return this._browserConsoleInitializing;
    }

    async function connect() {
      // Ensure that the root actor and the target-scoped actors have been registered on
      // the DebuggerServer, so that the Browser Console can retrieve the console actors.
      // (See Bug 1416105 for rationale).
      DebuggerServer.init();
      DebuggerServer.registerActors({ root: true, target: true });

      DebuggerServer.allowChromeProcess = true;

      const client = new DebuggerClient(DebuggerServer.connectPipe());
      await client.connect();
      const response = await client.getProcess();
      return { form: response.form, client, chrome: true, isBrowsingContext: true };
    }

    async function openWindow(t) {
      const win = Services.ww.openWindow(null, Tools.webConsole.url,
                                       "_blank", BC_WINDOW_FEATURES, null);

      await new Promise(resolve => {
        win.addEventListener("DOMContentLoaded", resolve, {once: true});
      });

      win.document.title = l10n.getStr("browserConsole.title");

      return {iframeWindow: win, chromeWindow: win};
    }

    // Temporarily cache the async startup sequence so that if toggleBrowserConsole
    // gets called again we can return this console instead of opening another one.
    this._browserConsoleInitializing = (async () => {
      const connection = await connect();
      const target = await TargetFactory.forRemoteTab(connection);
      const {iframeWindow, chromeWindow} = await openWindow(target);
      const browserConsole =
        await this.openBrowserConsole(target, iframeWindow, chromeWindow);
      return browserConsole;
    })();

    const browserConsole = await this._browserConsoleInitializing;
    this._browserConsoleInitializing = null;
    return browserConsole;
  },

  /**
   * Opens or focuses the Browser Console.
   */
  openBrowserConsoleOrFocus() {
    const hud = this.getBrowserConsole();
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

exports.HUDService = new HUDService();
