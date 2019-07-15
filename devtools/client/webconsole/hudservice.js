/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Services = require("Services");
loader.lazyRequireGetter(this, "Tools", "devtools/client/definitions", true);
loader.lazyRequireGetter(
  this,
  "gDevTools",
  "devtools/client/framework/devtools",
  true
);
loader.lazyRequireGetter(
  this,
  "DebuggerClient",
  "devtools/shared/client/debugger-client",
  true
);
loader.lazyRequireGetter(
  this,
  "l10n",
  "devtools/client/webconsole/webconsole-l10n"
);
loader.lazyRequireGetter(
  this,
  "WebConsole",
  "devtools/client/webconsole/webconsole"
);
loader.lazyRequireGetter(
  this,
  "BrowserConsole",
  "devtools/client/webconsole/browser-console"
);

const BC_WINDOW_FEATURES =
  "chrome,titlebar,toolbar,centerscreen,resizable,dialog=no";

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
   * Returns the console instance for a given id.
   *
   * @param string id
   * @returns Object
   */
  getHudReferenceById(id) {
    return this.consoles.get(id);
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
      // The Browser console ends up using the debugger in autocomplete.
      // Because the debugger can't be running in the same compartment than its debuggee,
      // we have to load the server in a dedicated Loader, flagged with
      // `freshCompartment`, which will force it to be loaded in another compartment.
      // We aren't using `invisibleToDebugger` in order to allow the Browser toolbox to
      // debug the Browser console. This is fine as they will spawn distinct Loaders and
      // so distinct `DebuggerServer` and actor modules.
      const ChromeUtils = require("ChromeUtils");
      const { DevToolsLoader } = ChromeUtils.import(
        "resource://devtools/shared/Loader.jsm"
      );
      const loader = new DevToolsLoader();
      loader.freshCompartment = true;
      const { DebuggerServer } = loader.require("devtools/server/main");

      DebuggerServer.init();

      // Ensure that the root actor and the target-scoped actors have been registered on
      // the DebuggerServer, so that the Browser Console can retrieve the console actors.
      // (See Bug 1416105 for rationale).
      DebuggerServer.registerActors({ root: true, target: true });

      DebuggerServer.allowChromeProcess = true;

      const client = new DebuggerClient(DebuggerServer.connectPipe());
      await client.connect();
      return client.mainRoot.getMainProcess();
    }

    async function openWindow(t) {
      const win = Services.ww.openWindow(
        null,
        Tools.webConsole.url,
        "_blank",
        BC_WINDOW_FEATURES,
        null
      );

      await new Promise(resolve => {
        win.addEventListener("DOMContentLoaded", resolve, { once: true });
      });

      win.document.title = l10n.getStr("browserConsole.title");

      return { iframeWindow: win, chromeWindow: win };
    }

    // Temporarily cache the async startup sequence so that if toggleBrowserConsole
    // gets called again we can return this console instead of opening another one.
    this._browserConsoleInitializing = (async () => {
      const target = await connect();
      await target.attach();
      const { iframeWindow, chromeWindow } = await openWindow(target);
      const browserConsole = await this.openBrowserConsole(
        target,
        iframeWindow,
        chromeWindow
      );
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
