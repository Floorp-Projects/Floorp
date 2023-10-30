/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This is the main module loaded in Firefox desktop that handles browser
 * windows and coordinates devtools around each window.
 *
 * This module is loaded lazily by devtools-clhandler.js, once the first
 * browser window is ready (i.e. fired browser-delayed-startup-finished event)
 **/

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  BrowserToolboxLauncher:
    "resource://devtools/client/framework/browser-toolbox/Launcher.sys.mjs",
});

const {
  gDevTools,
} = require("resource://devtools/client/framework/devtools.js");
const {
  getTheme,
  addThemeObserver,
  removeThemeObserver,
} = require("resource://devtools/client/shared/theme.js");

// Load toolbox lazily as it needs gDevTools to be fully initialized
loader.lazyRequireGetter(
  this,
  "Toolbox",
  "resource://devtools/client/framework/toolbox.js",
  true
);
loader.lazyRequireGetter(
  this,
  "DevToolsServer",
  "resource://devtools/server/devtools-server.js",
  true
);
loader.lazyRequireGetter(
  this,
  "BrowserMenus",
  "resource://devtools/client/framework/browser-menus.js"
);
loader.lazyRequireGetter(
  this,
  "appendStyleSheet",
  "resource://devtools/client/shared/stylesheet-utils.js",
  true
);
loader.lazyRequireGetter(
  this,
  "ResponsiveUIManager",
  "resource://devtools/client/responsive/manager.js"
);

const BROWSER_STYLESHEET_URL = "chrome://devtools/skin/devtools-browser.css";

const DEVTOOLS_F12_ENABLED_PREF = "devtools.f12_enabled";

/**
 * gDevToolsBrowser exposes functions to connect the gDevTools instance with a
 * Firefox instance.
 */
var gDevToolsBrowser = (exports.gDevToolsBrowser = {
  /**
   * A record of the windows whose menus we altered, so we can undo the changes
   * as the window is closed
   */
  _trackedBrowserWindows: new Set(),

  /**
   * WeakMap keeping track of the devtools-browser stylesheets loaded in the various
   * tracked windows.
   */
  _browserStyleSheets: new WeakMap(),

  /**
   * This function is for the benefit of Tools:DevToolbox in
   * browser/base/content/browser-sets.inc and should not be used outside
   * of there
   */
  // used by browser-sets.inc, command
  toggleToolboxCommand(gBrowser, startTime) {
    const toolbox = gDevTools.getToolboxForTab(gBrowser.selectedTab);

    // If a toolbox exists, using toggle from the Main window :
    // - should close a docked toolbox
    // - should focus a windowed toolbox
    const isDocked = toolbox && toolbox.hostType != Toolbox.HostType.WINDOW;
    if (isDocked) {
      gDevTools.closeToolboxForTab(gBrowser.selectedTab);
    } else {
      gDevTools.showToolboxForTab(gBrowser.selectedTab, { startTime });
    }
  },

  /**
   * This function ensures the right commands are enabled in a window,
   * depending on their relevant prefs. It gets run when a window is registered,
   * or when any of the devtools prefs change.
   */
  updateCommandAvailability(win) {
    const doc = win.document;

    function toggleMenuItem(id, isEnabled) {
      const cmd = doc.getElementById(id);
      cmd.hidden = !isEnabled;
      if (isEnabled) {
        cmd.removeAttribute("disabled");
      } else {
        cmd.setAttribute("disabled", "true");
      }
    }

    // Enable Browser Toolbox?
    const chromeEnabled = Services.prefs.getBoolPref("devtools.chrome.enabled");
    const devtoolsRemoteEnabled = Services.prefs.getBoolPref(
      "devtools.debugger.remote-enabled"
    );
    const remoteEnabled = chromeEnabled && devtoolsRemoteEnabled;
    toggleMenuItem("menu_browserToolbox", remoteEnabled);

    if (Services.prefs.getBoolPref("devtools.policy.disabled", false)) {
      toggleMenuItem("menu_devToolbox", false);
      toggleMenuItem("menu_devtools_remotedebugging", false);
      toggleMenuItem("menu_browserToolbox", false);
      toggleMenuItem("menu_browserConsole", false);
      toggleMenuItem("menu_responsiveUI", false);
      toggleMenuItem("menu_eyedropper", false);
      toggleMenuItem("extensionsForDevelopers", false);
    }
  },

  /**
   * This function makes sure that the "devtoolstheme" attribute is set on the browser
   * window to make it possible to change colors on elements in the browser (like the
   * splitter between the toolbox and web content).
   */
  updateDevtoolsThemeAttribute(win) {
    // Set an attribute on root element of each window to make it possible
    // to change colors based on the selected devtools theme.
    let devtoolsTheme = getTheme();
    if (devtoolsTheme != "dark") {
      devtoolsTheme = "light";
    }

    // Style the splitter between the toolbox and page content.  This used to
    // set the attribute on the browser's root node but that regressed tpaint:
    // bug 1331449.
    win.document
      .getElementById("appcontent")
      .setAttribute("devtoolstheme", devtoolsTheme);
  },

  observe(subject, topic, prefName) {
    switch (topic) {
      case "browser-delayed-startup-finished":
        this._registerBrowserWindow(subject);
        break;
      case "nsPref:changed":
        if (prefName.endsWith("enabled")) {
          for (const win of this._trackedBrowserWindows) {
            this.updateCommandAvailability(win);
          }
        }
        break;
      case "quit-application":
        gDevToolsBrowser.destroy({ shuttingDown: true });
        break;
      case "devtools:loader:destroy":
        // This event is fired when the devtools loader unloads, which happens
        // only when the add-on workflow ask devtools to be reloaded.
        if (subject.wrappedJSObject == require("@loader/unload")) {
          gDevToolsBrowser.destroy({ shuttingDown: false });
        }
        break;
    }
  },

  _observersRegistered: false,

  /**
   * This function is for the benefit of Tools:{toolId} commands,
   * triggered from the WebDeveloper menu and keyboard shortcuts.
   *
   * selectToolCommand's behavior:
   * - if the current page is about:devtools-toolbox
   *   we select the targeted tool
   * - if the toolbox is closed,
   *   we open the toolbox and select the tool
   * - if the toolbox is open, and the targeted tool is not selected,
   *   we select it
   * - if the toolbox is open, and the targeted tool is selected,
   *   and the host is NOT a window, we close the toolbox
   * - if the toolbox is open, and the targeted tool is selected,
   *   and the host is a window, we raise the toolbox window
   *
   * Used when: - registering a new tool
   *            - new xul window, to add menu items
   */
  async selectToolCommand(win, toolId, startTime) {
    if (gDevToolsBrowser._isAboutDevtoolsToolbox(win)) {
      const toolbox = gDevToolsBrowser._getAboutDevtoolsToolbox(win);
      await toolbox.selectTool(toolId, "key_shortcut");
      return;
    }

    const tab = win.gBrowser.selectedTab;
    const toolbox = gDevTools.getToolboxForTab(tab);
    const toolDefinition = gDevTools.getToolDefinition(toolId);

    if (
      toolbox &&
      (toolbox.currentToolId == toolId ||
        (toolId == "webconsole" && toolbox.splitConsole))
    ) {
      toolbox.fireCustomKey(toolId);

      if (
        toolDefinition.preventClosingOnKey ||
        toolbox.hostType == Toolbox.HostType.WINDOW
      ) {
        if (!toolDefinition.preventRaisingOnKey) {
          await toolbox.raise();
        }
      } else {
        await toolbox.destroy();
      }
      gDevTools.emit("select-tool-command", toolId);
    } else {
      await gDevTools
        .showToolboxForTab(tab, {
          raise: !toolDefinition.preventRaisingOnKey,
          startTime,
          toolId,
        })
        .then(newToolbox => {
          newToolbox.fireCustomKey(toolId);
          gDevTools.emit("select-tool-command", toolId);
        });
    }
  },

  /**
   * Called by devtools/client/devtools-startup.js when a key shortcut is pressed
   *
   * @param  {Window} window
   *         The top level browser window from which the key shortcut is pressed.
   * @param  {Object} key
   *         Key object describing the key shortcut being pressed. It comes
   *         from devtools-startup.js's KeyShortcuts array. The useful fields here
   *         are:
   *         - `toolId` used to identify a toolbox's panel like inspector or webconsole,
   *         - `id` used to identify any other key shortcuts like about:debugging
   * @param {Number} startTime
   *        Optional, indicates the time at which the key event fired. This is a
   *        `Cu.now()` timing.
   */
  async onKeyShortcut(window, key, startTime) {
    // Avoid to open devtools when the about:devtools-toolbox page is showing
    // on the window now.
    if (
      gDevToolsBrowser._isAboutDevtoolsToolbox(window) &&
      (key.id === "toggleToolbox" || key.id === "toggleToolboxF12")
    ) {
      return;
    }

    // If this is a toolbox's panel key shortcut, delegate to selectToolCommand
    if (key.toolId) {
      await gDevToolsBrowser.selectToolCommand(window, key.toolId, startTime);
      return;
    }
    // Otherwise implement all other key shortcuts individually here
    switch (key.id) {
      case "toggleToolbox":
        gDevToolsBrowser.toggleToolboxCommand(window.gBrowser, startTime);
        break;
      case "toggleToolboxF12":
        if (Services.prefs.getBoolPref(DEVTOOLS_F12_ENABLED_PREF, true)) {
          gDevToolsBrowser.toggleToolboxCommand(window.gBrowser, startTime);
        }
        break;
      case "browserToolbox":
        lazy.BrowserToolboxLauncher.init();
        break;
      case "browserConsole":
        const {
          BrowserConsoleManager,
        } = require("resource://devtools/client/webconsole/browser-console-manager.js");
        BrowserConsoleManager.openBrowserConsoleOrFocus();
        break;
      case "responsiveDesignMode":
        ResponsiveUIManager.toggle(window, window.gBrowser.selectedTab, {
          trigger: "shortcut",
        });
        break;
      case "javascriptTracingToggle":
        const toolbox = gDevTools.getToolboxForTab(window.gBrowser.selectedTab);
        if (!toolbox) {
          break;
        }
        const dbg = await toolbox.getPanel("jsdebugger");
        if (!dbg) {
          break;
        }
        dbg.toggleJavascriptTracing();
        break;
    }
  },

  /**
   * Open a tab on "about:debugging", optionally pre-select a given tab.
   */
  // Used by browser-sets.inc, command
  openAboutDebugging(gBrowser, hash) {
    const url = "about:debugging" + (hash ? "#" + hash : "");
    gBrowser.selectedTab = gBrowser.addTrustedTab(url);
  },

  /**
   * Add the devtools-browser stylesheet to browser window's document. Returns a promise.
   *
   * @param  {Window} win
   *         The window on which the stylesheet should be added.
   * @return {Promise} promise that resolves when the stylesheet is loaded (or rejects
   *         if it fails to load).
   */
  loadBrowserStyleSheet(win) {
    if (this._browserStyleSheets.has(win)) {
      return Promise.resolve();
    }

    const doc = win.document;
    const { styleSheet, loadPromise } = appendStyleSheet(
      doc,
      BROWSER_STYLESHEET_URL
    );
    this._browserStyleSheets.set(win, styleSheet);
    return loadPromise;
  },

  /**
   * Add this DevTools's presence to a browser window's document
   *
   * @param {HTMLDocument} doc
   *        The document to which devtools should be hooked to.
   */
  _registerBrowserWindow(win) {
    if (gDevToolsBrowser._trackedBrowserWindows.has(win)) {
      return;
    }
    if (!win.document.getElementById("menuWebDeveloperPopup")) {
      // Menus etc. set up here are browser specific.
      return;
    }
    gDevToolsBrowser._trackedBrowserWindows.add(win);
    BrowserMenus.addMenus(win.document);

    this.updateCommandAvailability(win);
    this.updateDevtoolsThemeAttribute(win);
    if (!this._observersRegistered) {
      this._observersRegistered = true;
      Services.prefs.addObserver("devtools.", this);
      this._onThemeChanged = this._onThemeChanged.bind(this);
      addThemeObserver(this._onThemeChanged);
    }

    win.addEventListener("unload", this);

    const tabContainer = win.gBrowser.tabContainer;
    tabContainer.addEventListener("TabSelect", this);
  },

  _onThemeChanged() {
    for (const win of this._trackedBrowserWindows) {
      this.updateDevtoolsThemeAttribute(win);
    }
  },

  /**
   * Add the menuitem for a tool to all open browser windows.
   *
   * @param {object} toolDefinition
   *        properties of the tool to add
   */
  _addToolToWindows(toolDefinition) {
    // No menu item or global shortcut is required for options panel.
    if (!toolDefinition.inMenu) {
      return;
    }

    // Skip if the tool is disabled.
    try {
      if (
        toolDefinition.visibilityswitch &&
        !Services.prefs.getBoolPref(toolDefinition.visibilityswitch)
      ) {
        return;
      }
    } catch (e) {
      // Prevent breaking everything if the pref doesn't exists.
    }

    // We need to insert the new tool in the right place, which means knowing
    // the tool that comes before the tool that we're trying to add
    const allDefs = gDevTools.getToolDefinitionArray();
    let prevDef;
    for (const def of allDefs) {
      if (!def.inMenu) {
        continue;
      }
      if (def === toolDefinition) {
        break;
      }
      prevDef = def;
    }

    for (const win of gDevToolsBrowser._trackedBrowserWindows) {
      BrowserMenus.insertToolMenuElements(
        win.document,
        toolDefinition,
        prevDef
      );
      // If we are on a page where devtools menu items are hidden such as
      // about:devtools-toolbox, we need to call _updateMenuItems to update the
      // visibility of the newly created menu item.
      gDevToolsBrowser._updateMenuItems(win);
    }
  },

  hasToolboxOpened(win) {
    const tab = win.gBrowser.selectedTab;
    for (const commands of gDevTools._toolboxesPerCommands.keys()) {
      if (commands.descriptorFront.localTab == tab) {
        return true;
      }
    }
    return false;
  },

  /**
   * Update developer tools menu items and the "Toggle Tools" checkbox. This is
   * called when a toolbox is created or destroyed.
   */
  _updateMenu() {
    for (const win of gDevToolsBrowser._trackedBrowserWindows) {
      gDevToolsBrowser._updateMenuItems(win);
    }
  },

  /**
   * Update developer tools menu items and the "Toggle Tools" checkbox of XULWindow.
   *
   * @param {XULWindow} win
   */
  _updateMenuItems(win) {
    const menu = win.document.getElementById("menu_devToolbox");

    // Hide the "Toggle Tools" menu item if we are on about:devtools-toolbox.
    menu.hidden =
      gDevToolsBrowser._isAboutDevtoolsToolbox(win) ||
      Services.prefs.getBoolPref("devtools.policy.disabled", false);

    // Add a checkmark for the "Toggle Tools" menu item if a toolbox is already opened.
    const hasToolbox = gDevToolsBrowser.hasToolboxOpened(win);
    if (hasToolbox) {
      menu.setAttribute("checked", "true");
    } else {
      menu.removeAttribute("checked");
    }
  },

  /**
   * Check whether the window is showing about:devtools-toolbox page or not.
   *
   * @param {XULWindow} win
   * @return {boolean} true: about:devtools-toolbox is showing
   *                   false: otherwise
   */
  _isAboutDevtoolsToolbox(win) {
    const currentURI = win.gBrowser.currentURI;
    return (
      currentURI.scheme === "about" &&
      currentURI.filePath === "devtools-toolbox"
    );
  },

  /**
   * Retrieve the Toolbox instance loaded in the current page if the page is
   * about:devtools-toolbox, null otherwise.
   *
   * @param {XULWindow} win
   *        The chrome window containing about:devtools-toolbox. Will match
   *        toolbox.topWindow.
   * @return {Toolbox} The toolbox instance loaded in about:devtools-toolbox
   *
   */
  _getAboutDevtoolsToolbox(win) {
    if (!gDevToolsBrowser._isAboutDevtoolsToolbox(win)) {
      return null;
    }
    return gDevTools.getToolboxes().find(toolbox => toolbox.topWindow === win);
  },

  /**
   * Remove the menuitem for a tool to all open browser windows.
   *
   * @param {string} toolId
   *        id of the tool to remove
   */
  _removeToolFromWindows(toolId) {
    for (const win of gDevToolsBrowser._trackedBrowserWindows) {
      BrowserMenus.removeToolFromMenu(toolId, win.document);
    }
  },

  /**
   * Called on browser unload to remove menu entries, toolboxes and event
   * listeners from the closed browser window.
   *
   * @param  {XULWindow} win
   *         The window containing the menu entry
   */
  _forgetBrowserWindow(win) {
    if (!gDevToolsBrowser._trackedBrowserWindows.has(win)) {
      return;
    }
    gDevToolsBrowser._trackedBrowserWindows.delete(win);
    win.removeEventListener("unload", this);

    BrowserMenus.removeMenus(win.document);

    // Destroy toolboxes for closed window
    for (const [commands, toolbox] of gDevTools._toolboxesPerCommands) {
      if (
        commands.descriptorFront.localTab?.ownerDocument?.defaultView == win
      ) {
        toolbox.destroy();
      }
    }

    const styleSheet = this._browserStyleSheets.get(win);
    if (styleSheet) {
      styleSheet.remove();
      this._browserStyleSheets.delete(win);
    }

    const tabContainer = win.gBrowser.tabContainer;
    tabContainer.removeEventListener("TabSelect", this);
  },

  handleEvent(event) {
    switch (event.type) {
      case "TabSelect":
        gDevToolsBrowser._updateMenu();
        break;
      case "unload":
        // top-level browser window unload
        gDevToolsBrowser._forgetBrowserWindow(event.target.defaultView);
        break;
    }
  },

  /**
   * Either the DevTools Loader has been destroyed by the add-on contribution
   * workflow, or firefox is shutting down.

   * @param {boolean} shuttingDown
   *        True if firefox is currently shutting down. We may prevent doing
   *        some cleanups to speed it up. Otherwise everything need to be
   *        cleaned up in order to be able to load devtools again.
   */
  destroy({ shuttingDown }) {
    Services.prefs.removeObserver("devtools.", gDevToolsBrowser);
    removeThemeObserver(this._onThemeChanged);
    Services.obs.removeObserver(
      gDevToolsBrowser,
      "browser-delayed-startup-finished"
    );
    Services.obs.removeObserver(gDevToolsBrowser, "quit-application");
    Services.obs.removeObserver(gDevToolsBrowser, "devtools:loader:destroy");

    for (const win of gDevToolsBrowser._trackedBrowserWindows) {
      gDevToolsBrowser._forgetBrowserWindow(win);
    }

    // Remove scripts loaded in content process to support the Browser Content Toolbox.
    DevToolsServer.removeContentServerScript();

    gDevTools.destroy({ shuttingDown });
  },
});

// Handle all already registered tools,
gDevTools
  .getToolDefinitionArray()
  .forEach(def => gDevToolsBrowser._addToolToWindows(def));
// and the new ones.
gDevTools.on("tool-registered", function (toolId) {
  const toolDefinition = gDevTools._tools.get(toolId);
  // If the tool has been registered globally, add to all the
  // available windows.
  if (toolDefinition) {
    gDevToolsBrowser._addToolToWindows(toolDefinition);
  }
});

gDevTools.on("tool-unregistered", function (toolId) {
  gDevToolsBrowser._removeToolFromWindows(toolId);
});

gDevTools.on("toolbox-ready", gDevToolsBrowser._updateMenu);
gDevTools.on("toolbox-destroyed", gDevToolsBrowser._updateMenu);

Services.obs.addObserver(gDevToolsBrowser, "quit-application");
Services.obs.addObserver(gDevToolsBrowser, "browser-delayed-startup-finished");
// Watch for module loader unload. Fires when the tools are reloaded.
Services.obs.addObserver(gDevToolsBrowser, "devtools:loader:destroy");

// Fake end of browser window load event for all already opened windows
// that is already fully loaded.
for (const win of Services.wm.getEnumerator(gDevTools.chromeWindowType)) {
  if (win.gBrowserInit?.delayedStartupFinished) {
    gDevToolsBrowser._registerBrowserWindow(win);
  }
}
