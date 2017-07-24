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
const defer = require("devtools/shared/defer");
const {gDevTools} = require("./devtools");

// Load target and toolbox lazily as they need gDevTools to be fully initialized
loader.lazyRequireGetter(this, "TargetFactory", "devtools/client/framework/target", true);
loader.lazyRequireGetter(this, "Toolbox", "devtools/client/framework/toolbox", true);
loader.lazyRequireGetter(this, "DebuggerServer", "devtools/server/main", true);
loader.lazyRequireGetter(this, "DebuggerClient", "devtools/shared/client/main", true);
loader.lazyRequireGetter(this, "BrowserMenus", "devtools/client/framework/browser-menus");
loader.lazyRequireGetter(this, "appendStyleSheet", "devtools/client/shared/stylesheet-utils", true);
loader.lazyRequireGetter(this, "DeveloperToolbar", "devtools/client/shared/developer-toolbar", true);
loader.lazyImporter(this, "BrowserToolboxProcess", "resource://devtools/client/framework/ToolboxProcess.jsm");
loader.lazyImporter(this, "ResponsiveUIManager", "resource://devtools/client/responsivedesign/responsivedesign.jsm");
loader.lazyImporter(this, "ScratchpadManager", "resource://devtools/client/scratchpad/scratchpad-manager.jsm");

loader.lazyImporter(this, "CustomizableUI", "resource:///modules/CustomizableUI.jsm");
loader.lazyImporter(this, "CustomizableWidgets", "resource:///modules/CustomizableWidgets.jsm");
loader.lazyImporter(this, "AppConstants", "resource://gre/modules/AppConstants.jsm");
loader.lazyImporter(this, "LightweightThemeManager", "resource://gre/modules/LightweightThemeManager.jsm");

const {LocalizationHelper} = require("devtools/shared/l10n");
const L10N = new LocalizationHelper("devtools/client/locales/toolbox.properties");

const COMPACT_LIGHT_ID = "firefox-compact-light@mozilla.org";
const COMPACT_DARK_ID = "firefox-compact-dark@mozilla.org";

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
   * WeakMap keeping track of DeveloperToolbar instances for each firefox window.
   */
  _toolbars: new WeakMap(),

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
  toggleToolboxCommand(gBrowser) {
    let target = TargetFactory.forTab(gBrowser.selectedTab);
    let toolbox = gDevTools.getToolbox(target);

    // If a toolbox exists, using toggle from the Main window :
    // - should close a docked toolbox
    // - should focus a windowed toolbox
    let isDocked = toolbox && toolbox.hostType != Toolbox.HostType.WINDOW;
    isDocked ? gDevTools.closeToolbox(target) : gDevTools.showToolbox(target);
  },

  /**
   * This function ensures the right commands are enabled in a window,
   * depending on their relevant prefs. It gets run when a window is registered,
   * or when any of the devtools prefs change.
   */
  updateCommandAvailability(win) {
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
      this.getDeveloperToolbar(win).show(false).catch(console.error);
    }

    // Enable WebIDE?
    let webIDEEnabled = Services.prefs.getBoolPref("devtools.webide.enabled");
    toggleMenuItem("menu_webide", webIDEEnabled);

    if (webIDEEnabled) {
      gDevToolsBrowser.installWebIDEWidget();
    } else {
      gDevToolsBrowser.uninstallWebIDEWidget();
    }

    // Enable Browser Toolbox?
    let chromeEnabled = Services.prefs.getBoolPref("devtools.chrome.enabled");
    let devtoolsRemoteEnabled = Services.prefs.getBoolPref(
      "devtools.debugger.remote-enabled");
    let remoteEnabled = chromeEnabled && devtoolsRemoteEnabled;
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
    win.document.getElementById("content")
       .setAttribute("devtoolstheme", devtoolsTheme);

    // If the toolbox color changes and we have the opposite compact theme applied,
    // change it to match.  For example:
    // 1) toolbox changes to dark, and the light compact theme was applied.
    //    Switch to the dark compact theme.
    // 2) toolbox changes to light or firebug, and the dark compact theme was applied.
    //    Switch to the light compact theme.
    // 3) No compact theme was applied. Do nothing.
    let currentTheme = LightweightThemeManager.currentTheme;
    let currentThemeID = currentTheme && currentTheme.id;
    if (currentThemeID == COMPACT_LIGHT_ID && devtoolsTheme == "dark") {
      LightweightThemeManager.currentTheme =
        LightweightThemeManager.getUsedTheme(COMPACT_DARK_ID);
    }
    if (currentThemeID == COMPACT_DARK_ID && devtoolsTheme == "light") {
      LightweightThemeManager.currentTheme =
        LightweightThemeManager.getUsedTheme(COMPACT_LIGHT_ID);
    }
  },

  observe(subject, topic, prefName) {
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
        if (prefName === "devtools.theme") {
          for (let win of this._trackedBrowserWindows) {
            this.updateDevtoolsThemeAttribute(win);
          }
        }
        break;
      case "quit-application":
        gDevToolsBrowser.destroy({ shuttingDown: true });
        break;
      case "sdk:loader:destroy":
        // This event is fired when the devtools loader unloads, which happens
        // only when the add-on workflow ask devtools to be reloaded.
        if (subject.wrappedJSObject == require("@loader/unload")) {
          gDevToolsBrowser.destroy({ shuttingDown: false });
        }
        break;
      case "lightweight-theme-changed":
        let currentTheme = LightweightThemeManager.currentTheme;
        let currentThemeID = currentTheme && currentTheme.id;
        let devtoolsTheme = Services.prefs.getCharPref("devtools.theme");

        // If the current lightweight theme changes to one of the compact themes, then
        // keep the devtools color in sync.
        if (currentThemeID == COMPACT_LIGHT_ID && devtoolsTheme == "dark") {
          Services.prefs.setCharPref("devtools.theme", "light");
        }
        if (currentThemeID == COMPACT_DARK_ID && devtoolsTheme == "light") {
          Services.prefs.setCharPref("devtools.theme", "dark");
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
  selectToolCommand(gBrowser, toolId) {
    let target = TargetFactory.forTab(gBrowser.selectedTab);
    let toolbox = gDevTools.getToolbox(target);
    let toolDefinition = gDevTools.getToolDefinition(toolId);

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
      gDevTools.showToolbox(target, toolId).then(newToolbox => {
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
   */
  onKeyShortcut(window, key) {
    // If this is a toolbox's panel key shortcut, delegate to selectToolCommand
    if (key.toolId) {
      gDevToolsBrowser.selectToolCommand(window.gBrowser, key.toolId);
      return;
    }
    // Otherwise implement all other key shortcuts individually here
    switch (key.id) {
      case "toggleToolbox":
      case "toggleToolboxF12":
        gDevToolsBrowser.toggleToolboxCommand(window.gBrowser);
        break;
      case "toggleToolbar":
        window.DeveloperToolbar.focusToggle();
        break;
      case "webide":
        gDevToolsBrowser.openWebIDE();
        break;
      case "browserToolbox":
        BrowserToolboxProcess.init();
        break;
      case "browserConsole":
        let HUDService = require("devtools/client/webconsole/hudservice");
        HUDService.openBrowserConsoleOrFocus();
        break;
      case "responsiveDesignMode":
        ResponsiveUIManager.toggle(window, window.gBrowser.selectedTab);
        break;
      case "scratchpad":
        ScratchpadManager.openScratchpad();
        break;
    }
  },

  /**
   * Open a tab on "about:debugging", optionally pre-select a given tab.
   */
   // Used by browser-sets.inc, command
  openAboutDebugging(gBrowser, hash) {
    let url = "about:debugging" + (hash ? "#" + hash : "");
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
    let win = Services.wm.getMostRecentWindow("devtools:webide");
    if (win) {
      win.focus();
    } else {
      Services.ww.openWindow(null, "chrome://webide/content/", "webide", "chrome,centerscreen,resizable", null);
    }
  },

  _getContentProcessTarget(processId) {
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
  openContentProcessToolbox(gBrowser) {
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
  installDeveloperWidget() {
    let id = "developer-button";
    let widget = CustomizableUI.getWidget(id);
    if (widget && widget.provider == CustomizableUI.PROVIDER_API) {
      return;
    }
    let item = {
      id: id,
      type: "view",
      viewId: "PanelUI-developer",
      shortcutId: "key_devToolboxMenuItem",
      tooltiptext: "developer-button.tooltiptext2",
      defaultArea: AppConstants.MOZ_DEV_EDITION ?
                     CustomizableUI.AREA_NAVBAR :
                     CustomizableUI.AREA_PANEL,
      onViewShowing(event) {
        // Populate the subview with whatever menuitems are in the developer
        // menu. We skip menu elements, because the menu panel has no way
        // of dealing with those right now.
        let doc = event.target.ownerDocument;

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
      onInit(anchor) {
        // Since onBeforeCreated already bails out when initialized, we can call
        // it right away.
        this.onBeforeCreated(anchor.ownerDocument);
      },
      onBeforeCreated(doc) {
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
    };
    CustomizableUI.createWidget(item);
    CustomizableWidgets.push(item);
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
    let widgetWrapper = CustomizableUI.getWidget("webide-button");
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
  loadBrowserStyleSheet: function (win) {
    if (this._browserStyleSheets.has(win)) {
      return Promise.resolve();
    }

    let doc = win.document;
    let {styleSheet, loadPromise} = appendStyleSheet(doc, BROWSER_STYLESHEET_URL);
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

    // Register the Developer widget in the Hamburger menu or navbar
    // only once menus are registered as it depends on it.
    gDevToolsBrowser.installDeveloperWidget();

    this.updateCommandAvailability(win);
    this.updateDevtoolsThemeAttribute(win);
    this.ensurePrefObserver();
    win.addEventListener("unload", this);

    let tabContainer = win.gBrowser.tabContainer;
    tabContainer.addEventListener("TabSelect", this);
    tabContainer.addEventListener("TabOpen", this);
    tabContainer.addEventListener("TabClose", this);
    tabContainer.addEventListener("TabPinned", this);
    tabContainer.addEventListener("TabUnpinned", this);
  },

  /**
   * Create singleton instance of the developer toolbar for a given top level window.
   *
   * @param {Window} win
   *        The window to which the toolbar should be created.
   */
  getDeveloperToolbar(win) {
    let toolbar = this._toolbars.get(win);
    if (toolbar) {
      return toolbar;
    }
    toolbar = new DeveloperToolbar(win);
    this._toolbars.set(win, toolbar);
    return toolbar;
  },

  /**
   * Hook the JS debugger tool to the "Debug Script" button of the slow script
   * dialog.
   */
  setSlowScriptDebugHandler() {
    let debugService = Cc["@mozilla.org/dom/slow-script-debug;1"]
                         .getService(Ci.nsISlowScriptDebug);
    let tm = Cc["@mozilla.org/thread-manager;1"].getService(Ci.nsIThreadManager);

    function slowScriptDebugHandler(tab, callback) {
      let target = TargetFactory.forTab(tab);

      gDevTools.showToolbox(target, "jsdebugger").then(toolbox => {
        let threadClient = toolbox.threadClient;

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

    debugService.activationHandler = function (window) {
      let chromeWindow = window.QueryInterface(Ci.nsIInterfaceRequestor)
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
      let utils = window.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDOMWindowUtils);
      utils.enterModalState();
      tm.spinEventLoopUntil(() => {
        return setupFinished;
      });
      utils.leaveModalState();
    };

    debugService.remoteActivationHandler = function (browser, callback) {
      let chromeWindow = browser.ownerDocument.defaultView;
      let tab = chromeWindow.gBrowser.getTabForBrowser(browser);
      chromeWindow.gBrowser.selected = tab;

      slowScriptDebugHandler(tab, function () {
        callback.finishDebuggerStartup();
      });
    };
  },

  /**
   * Unset the slow script debug handler.
   */
  unsetSlowScriptDebugHandler() {
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

  hasToolboxOpened(win) {
    let tab = win.gBrowser.selectedTab;
    for (let [target, ] of gDevTools._toolboxes) {
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
  _removeToolFromWindows(toolId) {
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
  _forgetBrowserWindow(win) {
    if (!gDevToolsBrowser._trackedBrowserWindows.has(win)) {
      return;
    }
    gDevToolsBrowser._trackedBrowserWindows.delete(win);
    win.removeEventListener("unload", this);

    BrowserMenus.removeMenus(win.document);

    // Destroy toolboxes for closed window
    for (let [target, toolbox] of gDevTools._toolboxes) {
      if (target.tab && target.tab.ownerDocument.defaultView == win) {
        toolbox.destroy();
      }
    }

    let styleSheet = this._browserStyleSheets.get(win);
    if (styleSheet) {
      styleSheet.remove();
      this._browserStyleSheets.delete(win);
    }

    this._toolbars.delete(win);

    let tabContainer = win.gBrowser.tabContainer;
    tabContainer.removeEventListener("TabSelect", this);
    tabContainer.removeEventListener("TabOpen", this);
    tabContainer.removeEventListener("TabClose", this);
    tabContainer.removeEventListener("TabPinned", this);
    tabContainer.removeEventListener("TabUnpinned", this);
  },

  handleEvent(event) {
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

  /**
   * Either the SDK Loader has been destroyed by the add-on contribution
   * workflow, or firefox is shutting down.

   * @param {boolean} shuttingDown
   *        True if firefox is currently shutting down. We may prevent doing
   *        some cleanups to speed it up. Otherwise everything need to be
   *        cleaned up in order to be able to load devtools again.
   */
  destroy({ shuttingDown }) {
    Services.prefs.removeObserver("devtools.", gDevToolsBrowser);
    Services.obs.removeObserver(gDevToolsBrowser, "lightweight-theme-changed");
    Services.obs.removeObserver(gDevToolsBrowser, "browser-delayed-startup-finished");
    Services.obs.removeObserver(gDevToolsBrowser, "quit-application");
    Services.obs.removeObserver(gDevToolsBrowser, "sdk:loader:destroy");

    for (let win of gDevToolsBrowser._trackedBrowserWindows) {
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
gDevTools.on("tool-registered", function (ev, toolId) {
  let toolDefinition = gDevTools._tools.get(toolId);
  // If the tool has been registered globally, add to all the
  // available windows.
  if (toolDefinition) {
    gDevToolsBrowser._addToolToWindows(toolDefinition);
  }
});

gDevTools.on("tool-unregistered", function (ev, toolId) {
  gDevToolsBrowser._removeToolFromWindows(toolId);
});

gDevTools.on("toolbox-ready", gDevToolsBrowser._updateMenuCheckbox);
gDevTools.on("toolbox-destroyed", gDevToolsBrowser._updateMenuCheckbox);

Services.obs.addObserver(gDevToolsBrowser, "quit-application");
Services.obs.addObserver(gDevToolsBrowser, "browser-delayed-startup-finished");
// Watch for module loader unload. Fires when the tools are reloaded.
Services.obs.addObserver(gDevToolsBrowser, "sdk:loader:destroy");
Services.obs.addObserver(gDevToolsBrowser, "lightweight-theme-changed");

// Fake end of browser window load event for all already opened windows
// that is already fully loaded.
let enumerator = Services.wm.getEnumerator(gDevTools.chromeWindowType);
while (enumerator.hasMoreElements()) {
  let win = enumerator.getNext();
  if (win.gBrowserInit && win.gBrowserInit.delayedStartupFinished) {
    gDevToolsBrowser._registerBrowserWindow(win);
  }
}
