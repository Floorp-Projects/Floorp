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

const {Cc, Ci, Cu} = require("chrome");
const Services = require("Services");
const promise = require("promise");
const defer = require("devtools/shared/defer");
const Telemetry = require("devtools/client/shared/telemetry");
const { gDevTools } = require("./devtools");
const { when: unload } = require("sdk/system/unload");

// Load target and toolbox lazily as they need gDevTools to be fully initialized
loader.lazyRequireGetter(this, "TargetFactory", "devtools/client/framework/target", true);
loader.lazyRequireGetter(this, "Toolbox", "devtools/client/framework/toolbox", true);
loader.lazyRequireGetter(this, "DebuggerServer", "devtools/server/main", true);
loader.lazyRequireGetter(this, "DebuggerClient", "devtools/shared/client/main", true);
loader.lazyRequireGetter(this, "BrowserMenus", "devtools/client/framework/browser-menus");

loader.lazyImporter(this, "CustomizableUI", "resource:///modules/CustomizableUI.jsm");
loader.lazyImporter(this, "AppConstants", "resource://gre/modules/AppConstants.jsm");

const {LocalizationHelper} = require("devtools/shared/l10n");
const L10N = new LocalizationHelper("devtools/client/locales/toolbox.properties");

const TABS_OPEN_PEAK_HISTOGRAM = "DEVTOOLS_TABS_OPEN_PEAK_LINEAR";
const TABS_OPEN_AVG_HISTOGRAM = "DEVTOOLS_TABS_OPEN_AVERAGE_LINEAR";
const TABS_PINNED_PEAK_HISTOGRAM = "DEVTOOLS_TABS_PINNED_PEAK_LINEAR";
const TABS_PINNED_AVG_HISTOGRAM = "DEVTOOLS_TABS_PINNED_AVERAGE_LINEAR";

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

  _telemetry: new Telemetry(),

  _tabStats: {
    peakOpen: 0,
    peakPinned: 0,
    histOpen: [],
    histPinned: []
  },

  /**
   * This function is for the benefit of Tools:DevToolbox in
   * browser/base/content/browser-sets.inc and should not be used outside
   * of there
   */
  // used by browser-sets.inc, command
  toggleToolboxCommand: function (gBrowser) {
    let target = TargetFactory.forTab(gBrowser.selectedTab);
    let toolbox = gDevTools.getToolbox(target);

    // If a toolbox exists, using toggle from the Main window :
    // - should close a docked toolbox
    // - should focus a windowed toolbox
    let isDocked = toolbox && toolbox.hostType != Toolbox.HostType.WINDOW;
    isDocked ? toolbox.destroy() : gDevTools.showToolbox(target);
  },

  /**
   * This function ensures the right commands are enabled in a window,
   * depending on their relevant prefs. It gets run when a window is registered,
   * or when any of the devtools prefs change.
   */
  updateCommandAvailability: function (win) {
    let doc = win.document;

    function toggleMenuItem(id, isEnabled) {
      let cmd = doc.getElementById(id);
      if (isEnabled) {
        cmd.removeAttribute("disabled");
        cmd.removeAttribute("hidden");
      } else {
        cmd.setAttribute("disabled", "true");
        cmd.setAttribute("hidden", "true");
      }
    }

    // Enable developer toolbar?
    let devToolbarEnabled = Services.prefs.getBoolPref("devtools.toolbar.enabled");
    toggleMenuItem("menu_devToolbar", devToolbarEnabled);
    let focusEl = doc.getElementById("menu_devToolbar");
    if (devToolbarEnabled) {
      focusEl.removeAttribute("disabled");
    } else {
      focusEl.setAttribute("disabled", "true");
    }
    if (devToolbarEnabled && Services.prefs.getBoolPref("devtools.toolbar.visible")) {
      win.DeveloperToolbar.show(false).catch(console.error);
    }

    // Enable WebIDE?
    let webIDEEnabled = Services.prefs.getBoolPref("devtools.webide.enabled");
    toggleMenuItem("menu_webide", webIDEEnabled);

    let showWebIDEWidget = Services.prefs.getBoolPref("devtools.webide.widget.enabled");
    if (webIDEEnabled && showWebIDEWidget) {
      gDevToolsBrowser.installWebIDEWidget();
    } else {
      gDevToolsBrowser.uninstallWebIDEWidget();
    }

    // Enable Browser Toolbox?
    let chromeEnabled = Services.prefs.getBoolPref("devtools.chrome.enabled");
    let devtoolsRemoteEnabled = Services.prefs.getBoolPref("devtools.debugger.remote-enabled");
    let remoteEnabled = chromeEnabled && devtoolsRemoteEnabled;
    toggleMenuItem("menu_browserToolbox", remoteEnabled);
    toggleMenuItem("menu_browserContentToolbox", remoteEnabled && win.gMultiProcessBrowser);

    // Enable DevTools connection screen, if the preference allows this.
    toggleMenuItem("menu_devtools_connect", devtoolsRemoteEnabled);
  },

  observe: function (subject, topic, prefName) {
    switch (topic) {
      case "browser-delayed-startup-finished":
        this._registerBrowserWindow(subject);
        break;
      case "nsPref:changed":
        if (prefName.endsWith("enabled")) {
          for (let win of this._trackedBrowserWindows) {
            this.updateCommandAvailability(win);
          }
        }
        break;
    }
  },

  _prefObserverRegistered: false,

  ensurePrefObserver: function () {
    if (!this._prefObserverRegistered) {
      this._prefObserverRegistered = true;
      Services.prefs.addObserver("devtools.", this, false);
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
  selectToolCommand: function (gBrowser, toolId) {
    let target = TargetFactory.forTab(gBrowser.selectedTab);
    let toolbox = gDevTools.getToolbox(target);
    let toolDefinition = gDevTools.getToolDefinition(toolId);

    if (toolbox &&
        (toolbox.currentToolId == toolId ||
          (toolId == "webconsole" && toolbox.splitConsole)))
    {
      toolbox.fireCustomKey(toolId);

      if (toolDefinition.preventClosingOnKey || toolbox.hostType == Toolbox.HostType.WINDOW) {
        toolbox.raise();
      } else {
        toolbox.destroy();
      }
      gDevTools.emit("select-tool-command", toolId);
    } else {
      gDevTools.showToolbox(target, toolId).then(() => {
        let target = TargetFactory.forTab(gBrowser.selectedTab);
        let toolbox = gDevTools.getToolbox(target);

        toolbox.fireCustomKey(toolId);
        gDevTools.emit("select-tool-command", toolId);
      });
    }
  },

  /**
   * Open a tab on "about:debugging", optionally pre-select a given tab.
   */
   // Used by browser-sets.inc, command
  openAboutDebugging: function (gBrowser, hash) {
    let url = "about:debugging" + (hash ? "#" + hash : "");
    gBrowser.selectedTab = gBrowser.addTab(url);
  },

  /**
   * Open a tab to allow connects to a remote browser
   */
   // Used by browser-sets.inc, command
  openConnectScreen: function (gBrowser) {
    gBrowser.selectedTab = gBrowser.addTab("chrome://devtools/content/framework/connect/connect.xhtml");
  },

  /**
   * Open WebIDE
   */
   // Used by browser-sets.inc, command
   //         itself, webide widget
  openWebIDE: function () {
    let win = Services.wm.getMostRecentWindow("devtools:webide");
    if (win) {
      win.focus();
    } else {
      Services.ww.openWindow(null, "chrome://webide/content/", "webide", "chrome,centerscreen,resizable", null);
    }
  },

  _getContentProcessTarget: function (processId) {
    // Create a DebuggerServer in order to connect locally to it
    if (!DebuggerServer.initialized) {
      DebuggerServer.init();
      DebuggerServer.addBrowserActors();
    }
    DebuggerServer.allowChromeProcess = true;

    let transport = DebuggerServer.connectPipe();
    let client = new DebuggerClient(transport);

    let deferred = defer();
    client.connect().then(() => {
      client.getProcess(processId)
            .then(response => {
              let options = {
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

   // Used by menus.js
  openContentProcessToolbox: function (gBrowser) {
    let { childCount } = Services.ppmm;
    // Get the process message manager for the current tab
    let mm = gBrowser.selectedBrowser.messageManager.processMessageManager;
    let processId = null;
    for (let i = 1; i < childCount; i++) {
      let child = Services.ppmm.getChildAt(i);
      if (child == mm) {
        processId = i;
        break;
      }
    }
    if (processId) {
      this._getContentProcessTarget(processId)
          .then(target => {
            // Display a new toolbox, in a new window, with debugger by default
            return gDevTools.showToolbox(target, "jsdebugger",
                                         Toolbox.HostType.WINDOW);
          });
    } else {
      let msg = L10N.getStr("toolbox.noContentProcessForTab.message");
      Services.prompt.alert(null, "", msg);
    }
  },

  /**
   * Install Developer widget
   */
  installDeveloperWidget: function () {
    let id = "developer-button";
    let widget = CustomizableUI.getWidget(id);
    if (widget && widget.provider == CustomizableUI.PROVIDER_API) {
      return;
    }
    CustomizableUI.createWidget({
      id: id,
      type: "view",
      viewId: "PanelUI-developer",
      shortcutId: "key_devToolboxMenuItem",
      tooltiptext: "developer-button.tooltiptext2",
      defaultArea: AppConstants.MOZ_DEV_EDITION ?
                     CustomizableUI.AREA_NAVBAR :
                     CustomizableUI.AREA_PANEL,
      onViewShowing: function (aEvent) {
        // Populate the subview with whatever menuitems are in the developer
        // menu. We skip menu elements, because the menu panel has no way
        // of dealing with those right now.
        let doc = aEvent.target.ownerDocument;
        let win = doc.defaultView;

        let menu = doc.getElementById("menuWebDeveloperPopup");

        let itemsToDisplay = [...menu.children];
        // Hardcode the addition of the "work offline" menuitem at the bottom:
        itemsToDisplay.push({localName: "menuseparator", getAttribute: () => {}});
        itemsToDisplay.push(doc.getElementById("goOfflineMenuitem"));

        let developerItems = doc.getElementById("PanelUI-developerItems");
        // Import private helpers from CustomizableWidgets
        let { clearSubview, fillSubviewFromMenuItems } =
          Cu.import("resource:///modules/CustomizableWidgets.jsm", {});
        clearSubview(developerItems);
        fillSubviewFromMenuItems(itemsToDisplay, developerItems);
      },
      onBeforeCreated: function (doc) {
        // Bug 1223127, CUI should make this easier to do.
        if (doc.getElementById("PanelUI-developerItems")) {
          return;
        }
        let view = doc.createElement("panelview");
        view.id = "PanelUI-developerItems";
        let panel = doc.createElement("vbox");
        panel.setAttribute("class", "panel-subview-body");
        view.appendChild(panel);
        doc.getElementById("PanelUI-multiView").appendChild(view);
      }
    });
  },

  /**
   * Install WebIDE widget
   */
  // Used by itself
  installWebIDEWidget: function () {
    if (this.isWebIDEWidgetInstalled()) {
      return;
    }

    let defaultArea;
    if (Services.prefs.getBoolPref("devtools.webide.widget.inNavbarByDefault")) {
      defaultArea = CustomizableUI.AREA_NAVBAR;
    } else {
      defaultArea = CustomizableUI.AREA_PANEL;
    }

    CustomizableUI.createWidget({
      id: "webide-button",
      shortcutId: "key_webide",
      label: "devtools-webide-button2.label",
      tooltiptext: "devtools-webide-button2.tooltiptext",
      defaultArea: defaultArea,
      onCommand: function (aEvent) {
        gDevToolsBrowser.openWebIDE();
      }
    });
  },

  isWebIDEWidgetInstalled: function () {
    let widgetWrapper = CustomizableUI.getWidget("webide-button");
    return !!(widgetWrapper && widgetWrapper.provider == CustomizableUI.PROVIDER_API);
  },

  /**
   * The deferred promise will be resolved by WebIDE's UI.init()
   */
  isWebIDEInitialized: defer(),

  /**
   * Uninstall WebIDE widget
   */
  uninstallWebIDEWidget: function () {
    if (this.isWebIDEWidgetInstalled()) {
      CustomizableUI.removeWidgetFromArea("webide-button");
    }
    CustomizableUI.destroyWidget("webide-button");
  },

  /**
   * Move WebIDE widget to the navbar
   */
   // Used by webide.js
  moveWebIDEWidgetInNavbar: function () {
    CustomizableUI.addWidgetToArea("webide-button", CustomizableUI.AREA_NAVBAR);
  },

  /**
   * Add this DevTools's presence to a browser window's document
   *
   * @param {XULDocument} doc
   *        The document to which devtools should be hooked to.
   */
  _registerBrowserWindow: function (win) {
    if (gDevToolsBrowser._trackedBrowserWindows.has(win)) {
      return;
    }
    gDevToolsBrowser._trackedBrowserWindows.add(win);

    BrowserMenus.addMenus(win.document);

    // Register the Developer widget in the Hamburger menu or navbar
    // only once menus are registered as it depends on it.
    gDevToolsBrowser.installDeveloperWidget();

    // Inject lazily DeveloperToolbar on the chrome window
    loader.lazyGetter(win, "DeveloperToolbar", function () {
      let { DeveloperToolbar } = require("devtools/client/shared/developer-toolbar");
      return new DeveloperToolbar(win);
    });

    this.updateCommandAvailability(win);
    this.ensurePrefObserver();
    win.addEventListener("unload", this);

    let tabContainer = win.gBrowser.tabContainer;
    tabContainer.addEventListener("TabSelect", this, false);
    tabContainer.addEventListener("TabOpen", this, false);
    tabContainer.addEventListener("TabClose", this, false);
    tabContainer.addEventListener("TabPinned", this, false);
    tabContainer.addEventListener("TabUnpinned", this, false);
  },

  /**
   * Hook the JS debugger tool to the "Debug Script" button of the slow script
   * dialog.
   */
  setSlowScriptDebugHandler: function DT_setSlowScriptDebugHandler() {
    let debugService = Cc["@mozilla.org/dom/slow-script-debug;1"]
                         .getService(Ci.nsISlowScriptDebug);
    let tm = Cc["@mozilla.org/thread-manager;1"].getService(Ci.nsIThreadManager);

    function slowScriptDebugHandler(aTab, aCallback) {
      let target = TargetFactory.forTab(aTab);

      gDevTools.showToolbox(target, "jsdebugger").then(toolbox => {
        let threadClient = toolbox.getCurrentPanel().panelWin.gThreadClient;

        // Break in place, which means resuming the debuggee thread and pausing
        // right before the next step happens.
        switch (threadClient.state) {
          case "paused":
            // When the debugger is already paused.
            threadClient.resumeThenPause();
            aCallback();
            break;
          case "attached":
            // When the debugger is already open.
            threadClient.interrupt(() => {
              threadClient.resumeThenPause();
              aCallback();
            });
            break;
          case "resuming":
            // The debugger is newly opened.
            threadClient.addOneTimeListener("resumed", () => {
              threadClient.interrupt(() => {
                threadClient.resumeThenPause();
                aCallback();
              });
            });
            break;
          default:
            throw Error("invalid thread client state in slow script debug handler: " +
                        threadClient.state);
        }
      });
    }

    debugService.activationHandler = function (aWindow) {
      let chromeWindow = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIWebNavigation)
                                .QueryInterface(Ci.nsIDocShellTreeItem)
                                .rootTreeItem
                                .QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIDOMWindow)
                                .QueryInterface(Ci.nsIDOMChromeWindow);

      let setupFinished = false;
      slowScriptDebugHandler(chromeWindow.gBrowser.selectedTab,
                             () => { setupFinished = true; });

      // Don't return from the interrupt handler until the debugger is brought
      // up; no reason to continue executing the slow script.
      let utils = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDOMWindowUtils);
      utils.enterModalState();
      while (!setupFinished) {
        tm.currentThread.processNextEvent(true);
      }
      utils.leaveModalState();
    };

    debugService.remoteActivationHandler = function (aBrowser, aCallback) {
      let chromeWindow = aBrowser.ownerDocument.defaultView;
      let tab = chromeWindow.gBrowser.getTabForBrowser(aBrowser);
      chromeWindow.gBrowser.selected = tab;

      function callback() {
        aCallback.finishDebuggerStartup();
      }

      slowScriptDebugHandler(tab, callback);
    };
  },

  /**
   * Unset the slow script debug handler.
   */
  unsetSlowScriptDebugHandler: function DT_unsetSlowScriptDebugHandler() {
    let debugService = Cc["@mozilla.org/dom/slow-script-debug;1"]
                         .getService(Ci.nsISlowScriptDebug);
    debugService.activationHandler = undefined;
  },

  /**
   * Add the menuitem for a tool to all open browser windows.
   *
   * @param {object} toolDefinition
   *        properties of the tool to add
   */
  _addToolToWindows: function DT_addToolToWindows(toolDefinition) {
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
    } catch (e) {}

    // We need to insert the new tool in the right place, which means knowing
    // the tool that comes before the tool that we're trying to add
    let allDefs = gDevTools.getToolDefinitionArray();
    let prevDef;
    for (let def of allDefs) {
      if (!def.inMenu) {
        continue;
      }
      if (def === toolDefinition) {
        break;
      }
      prevDef = def;
    }

    for (let win of gDevToolsBrowser._trackedBrowserWindows) {
      BrowserMenus.insertToolMenuElements(win.document, toolDefinition, prevDef);
    }

    if (toolDefinition.id === "jsdebugger") {
      gDevToolsBrowser.setSlowScriptDebugHandler();
    }
  },

  hasToolboxOpened: function (win) {
    let tab = win.gBrowser.selectedTab;
    for (let [target, toolbox] of gDevTools._toolboxes) {
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
  _updateMenuCheckbox: function DT_updateMenuCheckbox() {
    for (let win of gDevToolsBrowser._trackedBrowserWindows) {

      let hasToolbox = gDevToolsBrowser.hasToolboxOpened(win);

      let menu = win.document.getElementById("menu_devToolbox");
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
  _removeToolFromWindows: function DT_removeToolFromWindows(toolId) {
    for (let win of gDevToolsBrowser._trackedBrowserWindows) {
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
  _forgetBrowserWindow: function (win) {
    if (!gDevToolsBrowser._trackedBrowserWindows.has(win)) {
      return;
    }
    gDevToolsBrowser._trackedBrowserWindows.delete(win);
    win.removeEventListener("unload", this);

    BrowserMenus.removeMenus(win.document);

    // Destroy toolboxes for closed window
    for (let [target, toolbox] of gDevTools._toolboxes) {
      if (toolbox.win.top == win) {
        toolbox.destroy();
      }
    }

    // Destroy the Developer toolbar if it has been accessed
    let desc = Object.getOwnPropertyDescriptor(win, "DeveloperToolbar");
    if (desc && !desc.get) {
      win.DeveloperToolbar.destroy();
    }

    let tabContainer = win.gBrowser.tabContainer;
    tabContainer.removeEventListener("TabSelect", this, false);
    tabContainer.removeEventListener("TabOpen", this, false);
    tabContainer.removeEventListener("TabClose", this, false);
    tabContainer.removeEventListener("TabPinned", this, false);
    tabContainer.removeEventListener("TabUnpinned", this, false);
  },

  handleEvent: function (event) {
    switch (event.type) {
      case "TabOpen":
      case "TabClose":
      case "TabPinned":
      case "TabUnpinned":
        let open = 0;
        let pinned = 0;

        for (let win of this._trackedBrowserWindows) {
          let tabContainer = win.gBrowser.tabContainer;
          let numPinnedTabs = win.gBrowser._numPinnedTabs || 0;
          let numTabs = tabContainer.itemCount - numPinnedTabs;

          open += numTabs;
          pinned += numPinnedTabs;
        }

        this._tabStats.histOpen.push(open);
        this._tabStats.histPinned.push(pinned);
        this._tabStats.peakOpen = Math.max(open, this._tabStats.peakOpen);
        this._tabStats.peakPinned = Math.max(pinned, this._tabStats.peakPinned);
        break;
      case "TabSelect":
        gDevToolsBrowser._updateMenuCheckbox();
        break;
      case "unload":
        // top-level browser window unload
        gDevToolsBrowser._forgetBrowserWindow(event.target.defaultView);
        break;
    }
  },

  _pingTelemetry: function () {
    let mean = function (arr) {
      if (arr.length === 0) {
        return 0;
      }

      let total = arr.reduce((a, b) => a + b);
      return Math.ceil(total / arr.length);
    };

    let tabStats = gDevToolsBrowser._tabStats;
    this._telemetry.log(TABS_OPEN_PEAK_HISTOGRAM, tabStats.peakOpen);
    this._telemetry.log(TABS_OPEN_AVG_HISTOGRAM, mean(tabStats.histOpen));
    this._telemetry.log(TABS_PINNED_PEAK_HISTOGRAM, tabStats.peakPinned);
    this._telemetry.log(TABS_PINNED_AVG_HISTOGRAM, mean(tabStats.histPinned));
  },

  /**
   * All browser windows have been closed, tidy up remaining objects.
   */
  destroy: function () {
    Services.prefs.removeObserver("devtools.", gDevToolsBrowser);
    Services.obs.removeObserver(gDevToolsBrowser, "browser-delayed-startup-finished");
    Services.obs.removeObserver(gDevToolsBrowser.destroy, "quit-application");

    gDevToolsBrowser._pingTelemetry();
    gDevToolsBrowser._telemetry = null;

    for (let win of gDevToolsBrowser._trackedBrowserWindows) {
      gDevToolsBrowser._forgetBrowserWindow(win);
    }
  },
};

// Handle all already registered tools,
gDevTools.getToolDefinitionArray()
         .forEach(def => gDevToolsBrowser._addToolToWindows(def));
// and the new ones.
gDevTools.on("tool-registered", function (ev, toolId) {
  let toolDefinition = gDevTools._tools.get(toolId);
  gDevToolsBrowser._addToolToWindows(toolDefinition);
});

gDevTools.on("tool-unregistered", function (ev, toolId) {
  gDevToolsBrowser._removeToolFromWindows(toolId);
});

gDevTools.on("toolbox-ready", gDevToolsBrowser._updateMenuCheckbox);
gDevTools.on("toolbox-destroyed", gDevToolsBrowser._updateMenuCheckbox);

Services.obs.addObserver(gDevToolsBrowser.destroy, "quit-application", false);
Services.obs.addObserver(gDevToolsBrowser, "browser-delayed-startup-finished", false);

// Fake end of browser window load event for all already opened windows
// that is already fully loaded.
let enumerator = Services.wm.getEnumerator(gDevTools.chromeWindowType);
while (enumerator.hasMoreElements()) {
  let win = enumerator.getNext();
  if (win.gBrowserInit && win.gBrowserInit.delayedStartupFinished) {
    gDevToolsBrowser._registerBrowserWindow(win);
  }
}

// Watch for module loader unload. Fires when the tools are reloaded.
unload(function () {
  gDevToolsBrowser.destroy();
});
