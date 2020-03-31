/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Services = require("Services");
const ChromeUtils = require("ChromeUtils");
const { DevToolsLoader } = ChromeUtils.import(
  "resource://devtools/shared/Loader.jsm"
);

loader.lazyRequireGetter(this, "Tools", "devtools/client/definitions", true);
loader.lazyRequireGetter(
  this,
  "DevToolsClient",
  "devtools/client/devtools-client",
  true
);
loader.lazyRequireGetter(this, "l10n", "devtools/client/webconsole/utils/l10n");
loader.lazyRequireGetter(
  this,
  "BrowserConsole",
  "devtools/client/webconsole/browser-console"
);
loader.lazyRequireGetter(
  this,
  "PREFS",
  "devtools/client/webconsole/constants",
  true
);

const BC_WINDOW_FEATURES =
  "chrome,titlebar,toolbar,centerscreen,resizable,dialog=no";

class BrowserConsoleManager {
  constructor() {
    this._browserConsole = null;
    this._browserConsoleInitializing = null;
    this._browerConsoleSessionState = false;
    this._devToolsClient = null;
  }

  storeBrowserConsoleSessionState() {
    this._browerConsoleSessionState = !!this.getBrowserConsole();
  }

  getBrowserConsoleSessionState() {
    return this._browerConsoleSessionState;
  }

  /**
   * Open a Browser Console for the given target.
   *
   * @see devtools/framework/target.js for details about targets.
   *
   * @param object target
   *        The target that the browser console will connect to.
   * @param nsIDOMWindow iframeWindow
   *        The window where the browser console UI is already loaded.
   * @return object
   *         A promise object for the opening of the new BrowserConsole instance.
   */
  async openBrowserConsole(target, win) {
    const hud = new BrowserConsole(target, win, win);
    this._browserConsole = hud;
    await hud.init();
    return hud;
  }

  /**
   * Close the opened Browser Console
   */
  async closeBrowserConsole() {
    if (!this._browserConsole) {
      return;
    }

    await this._browserConsole.destroy();
    this._browserConsole = null;

    await this._devToolsClient.close();
    this._devToolsClient = null;
  }

  /**
   * Toggle the Browser Console.
   */
  async toggleBrowserConsole() {
    if (this._browserConsole) {
      return this.closeBrowserConsole();
    }

    if (this._browserConsoleInitializing) {
      return this._browserConsoleInitializing;
    }

    // Temporarily cache the async startup sequence so that if toggleBrowserConsole
    // gets called again we can return this console instead of opening another one.
    this._browserConsoleInitializing = (async () => {
      const target = await this.connect();
      await target.attach();
      const win = await this.openWindow();
      const browserConsole = await this.openBrowserConsole(target, win);
      return browserConsole;
    })();

    const browserConsole = await this._browserConsoleInitializing;
    this._browserConsoleInitializing = null;
    return browserConsole;
  }

  async connect() {
    // The Browser console ends up using the debugger in autocomplete.
    // Because the debugger can't be running in the same compartment than its debuggee,
    // we have to load the server in a dedicated Loader, flagged with
    // `freshCompartment`, which will force it to be loaded in another compartment.
    // We aren't using `invisibleToDebugger` in order to allow the Browser toolbox to
    // debug the Browser console. This is fine as they will spawn distinct Loaders and
    // so distinct `DevToolsServer` and actor modules.
    const loader = new DevToolsLoader({
      freshCompartment: true,
    });
    const { DevToolsServer } = loader.require(
      "devtools/server/devtools-server"
    );

    DevToolsServer.init();

    // Ensure that the root actor and the target-scoped actors have been registered on
    // the DevToolsServer, so that the Browser Console can retrieve the console actors.
    // (See Bug 1416105 for rationale).
    DevToolsServer.registerActors({ root: true, target: true });

    DevToolsServer.allowChromeProcess = true;

    this._devToolsClient = new DevToolsClient(DevToolsServer.connectPipe());
    await this._devToolsClient.connect();
    const descriptor = await this._devToolsClient.mainRoot.getMainProcess();
    return descriptor.getTarget();
  }

  async openWindow() {
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

    // It's important to declare the unload *after* the initial "DOMContentLoaded",
    // otherwise, since the window is navigated to Tools.webConsole.url, an unload event
    // is fired.
    win.addEventListener("unload", this.closeBrowserConsole.bind(this), {
      once: true,
    });

    const fissionSupport = Services.prefs.getBoolPref(
      PREFS.FEATURES.BROWSER_TOOLBOX_FISSION
    );
    const title = fissionSupport
      ? l10n.getStr("multiProcessBrowserConsole.title")
      : l10n.getStr("browserConsole.title");
    win.document.title = title;
    return win;
  }

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
  }

  /**
   * Get the Browser Console instance, if open.
   *
   * @return object|null
   *         A BrowserConsole instance or null if the Browser Console is not
   *         open.
   */
  getBrowserConsole() {
    return this._browserConsole;
  }
}

exports.BrowserConsoleManager = new BrowserConsoleManager();
