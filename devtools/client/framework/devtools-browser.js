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

const {Cc, Ci} = require("chrome");
const Services = require("Services");
const defer = require("devtools/shared/defer");
const {gDevTools} = require("./devtools");

// Load target and toolbox lazily as they need gDevTools to be fully initialized
loader.lazyRequireGetter(this, "TargetFactory", "devtools/client/framework/target", true);
loader.lazyRequireGetter(this, "Toolbox", "devtools/client/framework/toolbox", true);
loader.lazyRequireGetter(this, "DebuggerServer", "devtools/server/main", true);
loader.lazyRequireGetter(this, "DebuggerClient", "devtools/shared/client/debugger-client", true);
loader.lazyRequireGetter(this, "BrowserMenus", "devtools/client/framework/browser-menus");
loader.lazyRequireGetter(this, "appendStyleSheet", "devtools/client/shared/stylesheet-utils", true);
loader.lazyRequireGetter(this, "ResponsiveUIManager", "devtools/client/responsive.html/manager", true);
loader.lazyImporter(this, "BrowserToolboxProcess", "resource://devtools/client/framework/ToolboxProcess.jsm");
loader.lazyImporter(this, "ScratchpadManager", "resource://devtools/client/scratchpad/scratchpad-manager.jsm");

loader.lazyImporter(this, "CustomizableUI", "resource:///modules/CustomizableUI.jsm");

const {LocalizationHelper} = require("devtools/shared/l10n");
const L10N = new LocalizationHelper("devtools/client/locales/toolbox.properties");

const BROWSER_STYLESHEET_URL = "chrome://devtools/skin/devtools-browser.css";

/**
 * gDevToolsBrowser exposes functions to connect the gDevTools instance with a
 * Firefox instance.
 */
var gDevToolsBrowser = exports.gDevToolsBrowser = {
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
    const target = TargetFactory.forTab(gBrowser.selectedTab);
    const toolbox = gDevTools.getToolbox(target);

    // If a toolbox exists, using toggle from the Main window :
    // - should close a docked toolbox
    // - should focus a windowed toolbox
    const isDocked = toolbox && toolbox.hostType != Toolbox.HostType.WINDOW;
    if (isDocked) {
      gDevTools.closeToolbox(target);
    } else {
      gDevTools.showToolbox(target, null, null, null, startTime);
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
      if (isEnabled) {
        cmd.removeAttribute("disabled");
        cmd.removeAttribute("hidden");
      } else {
        cmd.setAttribute("disabled", "true");
        cmd.setAttribute("hidden", "true");
      }
    }

    // Enable WebIDE?
    const webIDEEnabled = Services.prefs.getBoolPref("devtools.webide.enabled");
    toggleMenuItem("menu_webide", webIDEEnabled);

    if (webIDEEnabled) {
      gDevToolsBrowser.installWebIDEWidget();
    } else {
      gDevToolsBrowser.uninstallWebIDEWidget();
    }

    // Enable Browser Toolbox?
    const chromeEnabled = Services.prefs.getBoolPref("devtools.chrome.enabled");
    const devtoolsRemoteEnabled = Services.prefs.getBoolPref(
      "devtools.debugger.remote-enabled");
    const remoteEnabled = chromeEnabled && devtoolsRemoteEnabled;
    toggleMenuItem("menu_browserToolbox", remoteEnabled);
    toggleMenuItem("menu_browserContentToolbox",
      remoteEnabled && win.gMultiProcessBrowser);

    // Enable DevTools connection screen, if the preference allows this.
    toggleMenuItem("menu_devtools_connect", devtoolsRemoteEnabled);
  },

  /**
   * This function makes sure that the "devtoolstheme" attribute is set on the browser
   * window to make it possible to change colors on elements in the browser (like gcli,
   * or the splitter between the toolbox and web content).
   */
  updateDevtoolsThemeAttribute(win) {
    // Set an attribute on root element of each window to make it possible
    // to change colors based on the selected devtools theme.
    let devtoolsTheme = Services.prefs.getCharPref("devtools.theme");
    if (devtoolsTheme != "dark") {
      devtoolsTheme = "light";
    }

    // Style gcli and the splitter between the toolbox and page content.  This used to
    // set the attribute on the browser's root node but that regressed tpaint:
    // bug 1331449.
    win.document.getElementById("browser-bottombox")
       .setAttribute("devtoolstheme", devtoolsTheme);
    win.document.getElementById("appcontent")
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
        if (prefName === "devtools.theme") {
          for (const win of this._trackedBrowserWindows) {
            this.updateDevtoolsThemeAttribute(win);
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

  _prefObserverRegistered: false,

  ensurePrefObserver() {
    if (!this._prefObserverRegistered) {
      this._prefObserverRegistered = true;
      Services.prefs.addObserver("devtools.", this);
    }
  },

  /**
   * This function is for the benefit of Tools:{toolId} commands,
   * triggered from the WebDeveloper menu and keyboard shortcuts.
   *
   * selectToolCommand's behavior:
   * - if the toolbox is closed,
   *   we open the toolbox and select the tool
   * - if the toolbox is open, and the targeted tool is not selected,
   *   we select it
   * - if the toolbox is open, and the targeted tool is selected,
   *   and the host is NOT a window, we close the toolbox
   * - if the toolbox is open, and the targeted tool is selected,
   *   and the host is a window, we raise the toolbox window
   */
  // Used when: - registering a new tool
  //            - new xul window, to add menu items
  selectToolCommand(gBrowser, toolId, startTime) {
    const target = TargetFactory.forTab(gBrowser.selectedTab);
    const toolbox = gDevTools.getToolbox(target);
    const toolDefinition = gDevTools.getToolDefinition(toolId);

    if (toolbox &&
        (toolbox.currentToolId == toolId ||
          (toolId == "webconsole" && toolbox.splitConsole))) {
      toolbox.fireCustomKey(toolId);

      if (toolDefinition.preventClosingOnKey ||
          toolbox.hostType == Toolbox.HostType.WINDOW) {
        toolbox.raise();
      } else {
        gDevTools.closeToolbox(target);
      }
      gDevTools.emit("select-tool-command", toolId);
    } else {
      gDevTools.showToolbox(target, toolId, null, null, startTime).then(newToolbox => {
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
   *         - `id` used to identify any other key shortcuts like scratchpad or
   *         about:debugging
   * @param {Number} startTime
   *        Optional, indicates the time at which the key event fired. This is a
   *        `Cu.now()` timing.
   */
  onKeyShortcut(window, key, startTime) {
    // If this is a toolbox's panel key shortcut, delegate to selectToolCommand
    if (key.toolId) {
      gDevToolsBrowser.selectToolCommand(window.gBrowser, key.toolId, startTime);
      return;
    }
    // Otherwise implement all other key shortcuts individually here
    switch (key.id) {
      case "toggleToolbox":
      case "toggleToolboxF12":
        gDevToolsBrowser.toggleToolboxCommand(window.gBrowser, startTime);
        break;
      case "webide":
        gDevToolsBrowser.openWebIDE();
        break;
      case "browserToolbox":
        BrowserToolboxProcess.init();
        break;
      case "browserConsole":
        const {HUDService} = require("devtools/client/webconsole/hudservice");
        HUDService.openBrowserConsoleOrFocus();
        break;
      case "responsiveDesignMode":
        ResponsiveUIManager.toggle(window, window.gBrowser.selectedTab, {
          trigger: "shortcut"
        });
        break;
      case "scratchpad":
        ScratchpadManager.openScratchpad();
        break;
      case "inspectorMac":
        gDevToolsBrowser.selectToolCommand(window.gBrowser, "inspector", startTime);
        break;
    }
  },

  /**
   * Open a tab on "about:debugging", optionally pre-select a given tab.
   */
   // Used by browser-sets.inc, command
  openAboutDebugging(gBrowser, hash) {
    const url = "about:debugging" + (hash ? "#" + hash : "");
    gBrowser.selectedTab = gBrowser.addTab(url);
  },

  /**
   * Open a tab to allow connects to a remote browser
   */
   // Used by browser-sets.inc, command
  openConnectScreen(gBrowser) {
    gBrowser.selectedTab = gBrowser.addTab("chrome://devtools/content/framework/connect/connect.xhtml");
  },

  /**
   * Open WebIDE
   */
   // Used by browser-sets.inc, command
   //         itself, webide widget
  openWebIDE() {
    const win = Services.wm.getMostRecentWindow("devtools:webide");
    if (win) {
      win.focus();
    } else {
      Services.ww.openWindow(null, "chrome://webide/content/", "webide", "chrome,centerscreen,resizable", null);
    }
  },

  _getContentProcessTarget(processId) {
    // Create a DebuggerServer in order to connect locally to it
    DebuggerServer.init();
    DebuggerServer.registerAllActors();
    DebuggerServer.allowChromeProcess = true;

    const transport = DebuggerServer.connectPipe();
    const client = new DebuggerClient(transport);

    const deferred = defer();
    client.connect().then(() => {
      client.getProcess(processId)
            .then(response => {
              const options = {
                form: response.form,
                client: client,
                chrome: true,
                isTabActor: false
              };
              return TargetFactory.forRemoteTab(options);
            })
            .then(target => {
              // Ensure closing the connection in order to cleanup
              // the debugger client and also the server created in the
              // content process
              target.on("close", () => {
                client.close();
              });
              deferred.resolve(target);
            });
    });

    return deferred.promise;
  },

  /**
   * Open the Browser Content Toolbox for the provided gBrowser instance.
   * Returns a promise that resolves with a toolbox instance. If no content process is
   * available, the promise will be rejected and a message will be displayed to the user.
   *
   * Used by menus.js
  */
  openContentProcessToolbox(gBrowser) {
    const { childCount } = Services.ppmm;
    // Get the process message manager for the current tab
    const mm = gBrowser.selectedBrowser.messageManager.processMessageManager;
    let processId = null;
    for (let i = 1; i < childCount; i++) {
      const child = Services.ppmm.getChildAt(i);
      if (child == mm) {
        processId = i;
        break;
      }
    }
    if (processId) {
      return this._getContentProcessTarget(processId)
          .then(target => {
            // Display a new toolbox in a new window
            return gDevTools.showToolbox(target, null, Toolbox.HostType.WINDOW);
          });
    }

    const msg = L10N.getStr("toolbox.noContentProcessForTab.message");
    Services.prompt.alert(null, "", msg);
    return Promise.reject(msg);
  },

  /**
   * Open a window-hosted toolbox to debug the worker associated to the provided
   * worker actor.
   *
   * @param  {DebuggerClient} client
   * @param  {Object} workerActor
   *         worker actor form to debug
   */
  openWorkerToolbox(client, workerActor) {
    client.attachWorker(workerActor, (response, workerClient) => {
      const workerTarget = TargetFactory.forWorker(workerClient);
      gDevTools.showToolbox(workerTarget, null, Toolbox.HostType.WINDOW)
        .then(toolbox => {
          toolbox.once("destroy", () => workerClient.detach());
        });
    });
  },

  /**
   * Install WebIDE widget
   */
  // Used by itself
  installWebIDEWidget() {
    if (this.isWebIDEWidgetInstalled()) {
      return;
    }

    CustomizableUI.createWidget({
      id: "webide-button",
      shortcutId: "key_webide",
      label: "devtools-webide-button2.label",
      tooltiptext: "devtools-webide-button2.tooltiptext",
      onCommand(event) {
        gDevToolsBrowser.openWebIDE();
      }
    });
  },

  isWebIDEWidgetInstalled() {
    const widgetWrapper = CustomizableUI.getWidget("webide-button");
    return !!(widgetWrapper && widgetWrapper.provider == CustomizableUI.PROVIDER_API);
  },

  /**
   * Add the devtools-browser stylesheet to browser window's document. Returns a promise.
   *
   * @param  {Window} win
   *         The window on which the stylesheet should be added.
   * @return {Promise} promise that resolves when the stylesheet is loaded (or rejects
   *         if it fails to load).
   */
  loadBrowserStyleSheet: function(win) {
    if (this._browserStyleSheets.has(win)) {
      return Promise.resolve();
    }

    const doc = win.document;
    const {styleSheet, loadPromise} = appendStyleSheet(doc, BROWSER_STYLESHEET_URL);
    this._browserStyleSheets.set(win, styleSheet);
    return loadPromise;
  },

  /**
   * The deferred promise will be resolved by WebIDE's UI.init()
   */
  isWebIDEInitialized: defer(),

  /**
   * Uninstall WebIDE widget
   */
  uninstallWebIDEWidget() {
    if (this.isWebIDEWidgetInstalled()) {
      CustomizableUI.removeWidgetFromArea("webide-button");
    }
    CustomizableUI.destroyWidget("webide-button");
  },

  /**
   * Add this DevTools's presence to a browser window's document
   *
   * @param {XULDocument} doc
   *        The document to which devtools should be hooked to.
   */
  _registerBrowserWindow(win) {
    if (gDevToolsBrowser._trackedBrowserWindows.has(win)) {
      return;
    }
    gDevToolsBrowser._trackedBrowserWindows.add(win);

    BrowserMenus.addMenus(win.document);

    this.updateCommandAvailability(win);
    this.updateDevtoolsThemeAttribute(win);
    this.ensurePrefObserver();
    win.addEventListener("unload", this);

    const tabContainer = win.gBrowser.tabContainer;
    tabContainer.addEventListener("TabSelect", this);
  },

  /**
   * Hook the JS debugger tool to the "Debug Script" button of the slow script
   * dialog.
   */
  setSlowScriptDebugHandler() {
    const debugService = Cc["@mozilla.org/dom/slow-script-debug;1"]
                         .getService(Ci.nsISlowScriptDebug);

    function slowScriptDebugHandler(tab, callback) {
      const target = TargetFactory.forTab(tab);

      gDevTools.showToolbox(target, "jsdebugger").then(toolbox => {
        const threadClient = toolbox.threadClient;

        // Break in place, which means resuming the debuggee thread and pausing
        // right before the next step happens.
        switch (threadClient.state) {
          case "paused":
            // When the debugger is already paused.
            threadClient.resumeThenPause();
            callback();
            break;
          case "attached":
            // When the debugger is already open.
            threadClient.interrupt(() => {
              threadClient.resumeThenPause();
              callback();
            });
            break;
          case "resuming":
            // The debugger is newly opened.
            threadClient.addOneTimeListener("resumed", () => {
              threadClient.interrupt(() => {
                threadClient.resumeThenPause();
                callback();
              });
            });
            break;
          default:
            throw Error("invalid thread client state in slow script debug handler: " +
                        threadClient.state);
        }
      });
    }

    debugService.activationHandler = function(window) {
      const chromeWindow = window.QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIWebNavigation)
                                .QueryInterface(Ci.nsIDocShellTreeItem)
                                .rootTreeItem
                                .QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIDOMWindow)
                                .QueryInterface(Ci.nsIDOMChromeWindow);

      let setupFinished = false;
      slowScriptDebugHandler(chromeWindow.gBrowser.selectedTab,
        () => {
          setupFinished = true;
        });

      // Don't return from the interrupt handler until the debugger is brought
      // up; no reason to continue executing the slow script.
      const utils = window.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDOMWindowUtils);
      utils.enterModalState();
      Services.tm.spinEventLoopUntil(() => {
        return setupFinished;
      });
      utils.leaveModalState();
    };

    debugService.remoteActivationHandler = function(browser, callback) {
      const chromeWindow = browser.ownerDocument.defaultView;
      const tab = chromeWindow.gBrowser.getTabForBrowser(browser);
      chromeWindow.gBrowser.selected = tab;

      slowScriptDebugHandler(tab, function() {
        callback.finishDebuggerStartup();
      });
    };
  },

  /**
   * Unset the slow script debug handler.
   */
  unsetSlowScriptDebugHandler() {
    const debugService = Cc["@mozilla.org/dom/slow-script-debug;1"]
                         .getService(Ci.nsISlowScriptDebug);
    debugService.activationHandler = undefined;
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
      if (toolDefinition.visibilityswitch &&
         !Services.prefs.getBoolPref(toolDefinition.visibilityswitch)) {
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
      BrowserMenus.insertToolMenuElements(win.document, toolDefinition, prevDef);
    }

    if (toolDefinition.id === "jsdebugger") {
      gDevToolsBrowser.setSlowScriptDebugHandler();
    }
  },

  hasToolboxOpened(win) {
    const tab = win.gBrowser.selectedTab;
    for (const [target, ] of gDevTools._toolboxes) {
      if (target.tab == tab) {
        return true;
      }
    }
    return false;
  },

  /**
   * Update the "Toggle Tools" checkbox in the developer tools menu. This is
   * called when a toolbox is created or destroyed.
   */
  _updateMenuCheckbox() {
    for (const win of gDevToolsBrowser._trackedBrowserWindows) {
      const hasToolbox = gDevToolsBrowser.hasToolboxOpened(win);

      const menu = win.document.getElementById("menu_devToolbox");
      if (hasToolbox) {
        menu.setAttribute("checked", "true");
      } else {
        menu.removeAttribute("checked");
      }
    }
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

    if (toolId === "jsdebugger") {
      gDevToolsBrowser.unsetSlowScriptDebugHandler();
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
    for (const [target, toolbox] of gDevTools._toolboxes) {
      if (target.tab && target.tab.ownerDocument.defaultView == win) {
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
        gDevToolsBrowser._updateMenuCheckbox();
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
    Services.obs.removeObserver(gDevToolsBrowser, "browser-delayed-startup-finished");
    Services.obs.removeObserver(gDevToolsBrowser, "quit-application");
    Services.obs.removeObserver(gDevToolsBrowser, "devtools:loader:destroy");

    for (const win of gDevToolsBrowser._trackedBrowserWindows) {
      gDevToolsBrowser._forgetBrowserWindow(win);
    }

    // Remove scripts loaded in content process to support the Browser Content Toolbox.
    DebuggerServer.removeContentServerScript();

    gDevTools.destroy({ shuttingDown });
  },
};

// Handle all already registered tools,
gDevTools.getToolDefinitionArray()
         .forEach(def => gDevToolsBrowser._addToolToWindows(def));
// and the new ones.
gDevTools.on("tool-registered", function(toolId) {
  const toolDefinition = gDevTools._tools.get(toolId);
  // If the tool has been registered globally, add to all the
  // available windows.
  if (toolDefinition) {
    gDevToolsBrowser._addToolToWindows(toolDefinition);
  }
});

gDevTools.on("tool-unregistered", function(toolId) {
  gDevToolsBrowser._removeToolFromWindows(toolId);
});

gDevTools.on("toolbox-ready", gDevToolsBrowser._updateMenuCheckbox);
gDevTools.on("toolbox-destroyed", gDevToolsBrowser._updateMenuCheckbox);

Services.obs.addObserver(gDevToolsBrowser, "quit-application");
Services.obs.addObserver(gDevToolsBrowser, "browser-delayed-startup-finished");
// Watch for module loader unload. Fires when the tools are reloaded.
Services.obs.addObserver(gDevToolsBrowser, "devtools:loader:destroy");

// Fake end of browser window load event for all already opened windows
// that is already fully loaded.
const enumerator = Services.wm.getEnumerator(gDevTools.chromeWindowType);
while (enumerator.hasMoreElements()) {
  const win = enumerator.getNext();
  if (win.gBrowserInit && win.gBrowserInit.delayedStartupFinished) {
    gDevToolsBrowser._registerBrowserWindow(win);
  }
}
