/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "gDevTools", "DevTools", "gDevToolsBrowser" ];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/devtools/Loader.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "promise",
                                  "resource://gre/modules/Promise.jsm", "Promise");
XPCOMUtils.defineLazyModuleGetter(this, "console",
                                  "resource://gre/modules/devtools/Console.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CustomizableUI",
                                  "resource:///modules/CustomizableUI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DebuggerServer",
                                  "resource://gre/modules/devtools/dbg-server.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DebuggerClient",
                                  "resource://gre/modules/devtools/dbg-client.jsm");

const EventEmitter = devtools.require("devtools/toolkit/event-emitter");
const Telemetry = devtools.require("devtools/shared/telemetry");

const TABS_OPEN_PEAK_HISTOGRAM = "DEVTOOLS_TABS_OPEN_PEAK_LINEAR";
const TABS_OPEN_AVG_HISTOGRAM = "DEVTOOLS_TABS_OPEN_AVERAGE_LINEAR";
const TABS_PINNED_PEAK_HISTOGRAM = "DEVTOOLS_TABS_PINNED_PEAK_LINEAR";
const TABS_PINNED_AVG_HISTOGRAM = "DEVTOOLS_TABS_PINNED_AVERAGE_LINEAR";

const FORBIDDEN_IDS = new Set(["toolbox", ""]);
const MAX_ORDINAL = 99;

const bundle = Services.strings.createBundle("chrome://browser/locale/devtools/toolbox.properties");

/**
 * DevTools is a class that represents a set of developer tools, it holds a
 * set of tools and keeps track of open toolboxes in the browser.
 */
this.DevTools = function DevTools() {
  this._tools = new Map();     // Map<toolId, tool>
  this._themes = new Map();    // Map<themeId, theme>
  this._toolboxes = new Map(); // Map<target, toolbox>
  this._telemetry = new Telemetry();

  // destroy() is an observer's handler so we need to preserve context.
  this.destroy = this.destroy.bind(this);
  this._teardown = this._teardown.bind(this);

  this._testing = false;

  EventEmitter.decorate(this);

  Services.obs.addObserver(this._teardown, "devtools-unloaded", false);
  Services.obs.addObserver(this.destroy, "quit-application", false);
};

DevTools.prototype = {
  /**
   * When the testing flag is set we take appropriate action to prevent race
   * conditions in our testing environment. This means setting
   * dom.send_after_paint_to_content to false to prevent infinite MozAfterPaint
   * loops and not autohiding the highlighter.
   */
  get testing() {
    return this._testing;
  },

  set testing(state) {
    let oldState = this._testing;
    this._testing = state;

    if (state !== oldState) {
      if (state) {
        this._savedSendAfterPaintToContentPref =
          Services.prefs.getBoolPref("dom.send_after_paint_to_content");

        // dom.send_after_paint_to_content is set to true (non-default) in
        // testing/profiles/prefs_general.js so lets set it to the same as it is
        // in a default browser profile for the duration of the test.
        Services.prefs.setBoolPref("dom.send_after_paint_to_content", false);
      } else {
        Services.prefs.setBoolPref("dom.send_after_paint_to_content",
                                   this._savedSendAfterPaintToContentPref);
      }
    }
  },

  /**
   * Register a new developer tool.
   *
   * A definition is a light object that holds different information about a
   * developer tool. This object is not supposed to have any operational code.
   * See it as a "manifest".
   * The only actual code lives in the build() function, which will be used to
   * start an instance of this tool.
   *
   * Each toolDefinition has the following properties:
   * - id: Unique identifier for this tool (string|required)
   * - visibilityswitch: Property name to allow us to hide this tool from the
   *                     DevTools Toolbox.
   *                     A falsy value indicates that it cannot be hidden.
   * - icon: URL pointing to a graphic which will be used as the src for an
   *         16x16 img tag (string|required)
   * - invertIconForLightTheme: The icon can automatically have an inversion
   *         filter applied (default is false).  All builtin tools are true, but
   *         addons may omit this to prevent unwanted changes to the `icon`
   *         image. See browser/themes/shared/devtools/filters.svg#invert for
   *         the filter being applied to the images (boolean|optional)
   * - url: URL pointing to a XUL/XHTML document containing the user interface
   *        (string|required)
   * - label: Localized name for the tool to be displayed to the user
   *          (string|required)
   * - build: Function that takes an iframe, which has been populated with the
   *          markup from |url|, and also the toolbox containing the panel.
   *          And returns an instance of ToolPanel (function|required)
   */
  registerTool: function DT_registerTool(toolDefinition) {
    let toolId = toolDefinition.id;

    if (!toolId || FORBIDDEN_IDS.has(toolId)) {
      throw new Error("Invalid definition.id");
    }

    // Make sure that additional tools will always be able to be hidden.
    // When being called from main.js, defaultTools has not yet been exported.
    // But, we can assume that in this case, it is a default tool.
    if (devtools.defaultTools && devtools.defaultTools.indexOf(toolDefinition) == -1) {
      toolDefinition.visibilityswitch = "devtools." + toolId + ".enabled";
    }

    this._tools.set(toolId, toolDefinition);

    this.emit("tool-registered", toolId);
  },

  /**
   * Removes all tools that match the given |toolId|
   * Needed so that add-ons can remove themselves when they are deactivated
   *
   * @param {string|object} tool
   *        Definition or the id of the tool to unregister. Passing the
   *        tool id should be avoided as it is a temporary measure.
   * @param {boolean} isQuitApplication
   *        true to indicate that the call is due to app quit, so we should not
   *        cause a cascade of costly events
   */
  unregisterTool: function DT_unregisterTool(tool, isQuitApplication) {
    let toolId = null;
    if (typeof tool == "string") {
      toolId = tool;
      tool = this._tools.get(tool);
    }
    else {
      toolId = tool.id;
    }
    this._tools.delete(toolId);

    if (!isQuitApplication) {
      this.emit("tool-unregistered", tool);
    }
  },

  /**
   * Sorting function used for sorting tools based on their ordinals.
   */
  ordinalSort: function DT_ordinalSort(d1, d2) {
    let o1 = (typeof d1.ordinal == "number") ? d1.ordinal : MAX_ORDINAL;
    let o2 = (typeof d2.ordinal == "number") ? d2.ordinal : MAX_ORDINAL;
    return o1 - o2;
  },

  getDefaultTools: function DT_getDefaultTools() {
    return devtools.defaultTools.sort(this.ordinalSort);
  },

  getAdditionalTools: function DT_getAdditionalTools() {
    let tools = [];
    for (let [key, value] of this._tools) {
      if (devtools.defaultTools.indexOf(value) == -1) {
        tools.push(value);
      }
    }
    return tools.sort(this.ordinalSort);
  },

  /**
   * Get a tool definition if it exists and is enabled.
   *
   * @param {string} toolId
   *        The id of the tool to show
   *
   * @return {ToolDefinition|null} tool
   *         The ToolDefinition for the id or null.
   */
  getToolDefinition: function DT_getToolDefinition(toolId) {
    let tool = this._tools.get(toolId);
    if (!tool) {
      return null;
    } else if (!tool.visibilityswitch) {
      return tool;
    }

    let enabled;
    try {
      enabled = Services.prefs.getBoolPref(tool.visibilityswitch);
    } catch (e) {
      enabled = true;
    }

    return enabled ? tool : null;
  },

  /**
   * Allow ToolBoxes to get at the list of tools that they should populate
   * themselves with.
   *
   * @return {Map} tools
   *         A map of the the tool definitions registered in this instance
   */
  getToolDefinitionMap: function DT_getToolDefinitionMap() {
    let tools = new Map();

    for (let [id, definition] of this._tools) {
      if (this.getToolDefinition(id)) {
        tools.set(id, definition);
      }
    }

    return tools;
  },

  /**
   * Tools have an inherent ordering that can't be represented in a Map so
   * getToolDefinitionArray provides an alternative representation of the
   * definitions sorted by ordinal value.
   *
   * @return {Array} tools
   *         A sorted array of the tool definitions registered in this instance
   */
  getToolDefinitionArray: function DT_getToolDefinitionArray() {
    let definitions = [];

    for (let [id, definition] of this._tools) {
      if (this.getToolDefinition(id)) {
        definitions.push(definition);
      }
    }

    return definitions.sort(this.ordinalSort);
  },

  /**
   * Register a new theme for developer tools toolbox.
   *
   * A definition is a light object that holds various information about a
   * theme.
   *
   * Each themeDefinition has the following properties:
   * - id: Unique identifier for this theme (string|required)
   * - label: Localized name for the theme to be displayed to the user
   *          (string|required)
   * - stylesheets: Array of URLs pointing to a CSS document(s) containing
   *                the theme style rules (array|required)
   * - classList: Array of class names identifying the theme within a document.
   *              These names are set to document element when applying
   *              the theme (array|required)
   * - onApply: Function that is executed by the framework when the theme
   *            is applied. The function takes the current iframe window
   *            and the previous theme id as arguments (function)
   * - onUnapply: Function that is executed by the framework when the theme
   *            is unapplied. The function takes the current iframe window
   *            and the new theme id as arguments (function)
   */
  registerTheme: function DT_registerTheme(themeDefinition) {
    let themeId = themeDefinition.id;

    if (!themeId) {
      throw new Error("Invalid theme id");
    }

    if (this._themes.get(themeId)) {
      throw new Error("Theme with the same id is already registered");
    }

    this._themes.set(themeId, themeDefinition);

    this.emit("theme-registered", themeId);
  },

  /**
   * Removes an existing theme from the list of registered themes.
   * Needed so that add-ons can remove themselves when they are deactivated
   *
   * @param {string|object} theme
   *        Definition or the id of the theme to unregister.
   */
  unregisterTheme: function DT_unregisterTheme(theme) {
    let themeId = null;
    if (typeof theme == "string") {
      themeId = theme;
      theme = this._themes.get(theme);
    }
    else {
      themeId = theme.id;
    }

    let currTheme = Services.prefs.getCharPref("devtools.theme");

    // Change the current theme if it's being dynamically removed together
    // with the owner (bootstrapped) extension.
    // But, do not change it if the application is just shutting down.
    if (!Services.startup.shuttingDown && theme.id == currTheme) {
      Services.prefs.setCharPref("devtools.theme", "light");

      let data = {
        pref: "devtools.theme",
        newValue: "light",
        oldValue: currTheme
      };

      gDevTools.emit("pref-changed", data);

      this.emit("theme-unregistered", theme);
    }

    this._themes.delete(themeId);
  },

  /**
   * Get a theme definition if it exists.
   *
   * @param {string} themeId
   *        The id of the theme
   *
   * @return {ThemeDefinition|null} theme
   *         The ThemeDefinition for the id or null.
   */
  getThemeDefinition: function DT_getThemeDefinition(themeId) {
    let theme = this._themes.get(themeId);
    if (!theme) {
      return null;
    }
    return theme;
  },

  /**
   * Get map of registered themes.
   *
   * @return {Map} themes
   *         A map of the the theme definitions registered in this instance
   */
  getThemeDefinitionMap: function DT_getThemeDefinitionMap() {
    let themes = new Map();

    for (let [id, definition] of this._themes) {
      if (this.getThemeDefinition(id)) {
        themes.set(id, definition);
      }
    }

    return themes;
  },

  /**
   * Get registered themes definitions sorted by ordinal value.
   *
   * @return {Array} themes
   *         A sorted array of the theme definitions registered in this instance
   */
  getThemeDefinitionArray: function DT_getThemeDefinitionArray() {
    let definitions = [];

    for (let [id, definition] of this._themes) {
      if (this.getThemeDefinition(id)) {
        definitions.push(definition);
      }
    }

    return definitions.sort(this.ordinalSort);
  },

  /**
   * Show a Toolbox for a target (either by creating a new one, or if a toolbox
   * already exists for the target, by bring to the front the existing one)
   * If |toolId| is specified then the displayed toolbox will have the
   * specified tool selected.
   * If |hostType| is specified then the toolbox will be displayed using the
   * specified HostType.
   *
   * @param {Target} target
   *         The target the toolbox will debug
   * @param {string} toolId
   *        The id of the tool to show
   * @param {Toolbox.HostType} hostType
   *        The type of host (bottom, window, side)
   * @param {object} hostOptions
   *        Options for host specifically
   *
   * @return {Toolbox} toolbox
   *        The toolbox that was opened
   */
  showToolbox: function(target, toolId, hostType, hostOptions) {
    let deferred = promise.defer();

    let toolbox = this._toolboxes.get(target);
    if (toolbox) {

      let hostPromise = (hostType != null && toolbox.hostType != hostType) ?
          toolbox.switchHost(hostType) :
          promise.resolve(null);

      if (toolId != null && toolbox.currentToolId != toolId) {
        hostPromise = hostPromise.then(function() {
          return toolbox.selectTool(toolId);
        });
      }

      return hostPromise.then(function() {
        toolbox.raise();
        return toolbox;
      });
    }
    else {
      // No toolbox for target, create one
      toolbox = new devtools.Toolbox(target, toolId, hostType, hostOptions);

      this.emit("toolbox-created", toolbox);

      this._toolboxes.set(target, toolbox);

      toolbox.once("destroy", () => {
        this.emit("toolbox-destroy", target);
      });

      toolbox.once("destroyed", () => {
        this._toolboxes.delete(target);
        this.emit("toolbox-destroyed", target);
      });

      // If toolId was passed in, it will already be selected before the
      // open promise resolves.
      toolbox.open().then(() => {
        deferred.resolve(toolbox);
        this.emit("toolbox-ready", toolbox);
      });
    }

    return deferred.promise;
  },

  /**
   * Return the toolbox for a given target.
   *
   * @param  {object} target
   *         Target value e.g. the target that owns this toolbox
   *
   * @return {Toolbox} toolbox
   *         The toolbox that is debugging the given target
   */
  getToolbox: function DT_getToolbox(target) {
    return this._toolboxes.get(target);
  },

  /**
   * Close the toolbox for a given target
   *
   * @return promise
   *         This promise will resolve to false if no toolbox was found
   *         associated to the target. true, if the toolbox was successfully
   *         closed.
   */
  closeToolbox: function DT_closeToolbox(target) {
    let toolbox = this._toolboxes.get(target);
    if (toolbox == null) {
      return promise.resolve(false);
    }
    return toolbox.destroy().then(() => true);
  },

  _pingTelemetry: function() {
    let mean = function(arr) {
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
   * Called to tear down a tools provider.
   */
  _teardown: function DT_teardown() {
    for (let [target, toolbox] of this._toolboxes) {
      toolbox.destroy();
    }
  },

  /**
   * All browser windows have been closed, tidy up remaining objects.
   */
  destroy: function() {
    Services.obs.removeObserver(this.destroy, "quit-application");
    Services.obs.removeObserver(this._teardown, "devtools-unloaded");

    for (let [key, tool] of this.getToolDefinitionMap()) {
      this.unregisterTool(key, true);
    }

    this._pingTelemetry();
    this._telemetry = null;

    // Cleaning down the toolboxes: i.e.
    //   for (let [target, toolbox] of this._toolboxes) toolbox.destroy();
    // Is taken care of by the gDevToolsBrowser.forgetBrowserWindow
  },

  /**
   * Iterator that yields each of the toolboxes.
   */
  *[Symbol.iterator]() {
    for (let toolbox of this._toolboxes) {
      yield toolbox;
    }
  }
};

/**
 * gDevTools is a singleton that controls the Firefox Developer Tools.
 *
 * It is an instance of a DevTools class that holds a set of tools. It has the
 * same lifetime as the browser.
 */
let gDevTools = new DevTools();
this.gDevTools = gDevTools;

/**
 * gDevToolsBrowser exposes functions to connect the gDevTools instance with a
 * Firefox instance.
 */
let gDevToolsBrowser = {
  /**
   * A record of the windows whose menus we altered, so we can undo the changes
   * as the window is closed
   */
  _trackedBrowserWindows: new Set(),

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
  toggleToolboxCommand: function(gBrowser) {
    let target = devtools.TargetFactory.forTab(gBrowser.selectedTab);
    let toolbox = gDevTools.getToolbox(target);

    toolbox ? toolbox.destroy() : gDevTools.showToolbox(target);
  },

  toggleBrowserToolboxCommand: function(gBrowser) {
    let target = devtools.TargetFactory.forWindow(gBrowser.ownerDocument.defaultView);
    let toolbox = gDevTools.getToolbox(target);

    toolbox ? toolbox.destroy()
     : gDevTools.showToolbox(target, "inspector", Toolbox.HostType.WINDOW);
  },

  /**
   * This function ensures the right commands are enabled in a window,
   * depending on their relevant prefs. It gets run when a window is registered,
   * or when any of the devtools prefs change.
   */
  updateCommandAvailability: function(win) {
    let doc = win.document;

    function toggleCmd(id, isEnabled) {
      let cmd = doc.getElementById(id);
      if (isEnabled) {
        cmd.removeAttribute("disabled");
        cmd.removeAttribute("hidden");
      } else {
        cmd.setAttribute("disabled", "true");
        cmd.setAttribute("hidden", "true");
      }
    };

    // Enable developer toolbar?
    let devToolbarEnabled = Services.prefs.getBoolPref("devtools.toolbar.enabled");
    toggleCmd("Tools:DevToolbar", devToolbarEnabled);
    let focusEl = doc.getElementById("Tools:DevToolbarFocus");
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
    toggleCmd("Tools:WebIDE", webIDEEnabled);

    let showWebIDEWidget = Services.prefs.getBoolPref("devtools.webide.widget.enabled");
    if (webIDEEnabled && showWebIDEWidget) {
      gDevToolsBrowser.installWebIDEWidget();
    } else {
      gDevToolsBrowser.uninstallWebIDEWidget();
    }

    // Enable App Manager?
    let appMgrEnabled = Services.prefs.getBoolPref("devtools.appmanager.enabled");
    toggleCmd("Tools:DevAppMgr", !webIDEEnabled && appMgrEnabled);

    // Enable Browser Toolbox?
    let chromeEnabled = Services.prefs.getBoolPref("devtools.chrome.enabled");
    let devtoolsRemoteEnabled = Services.prefs.getBoolPref("devtools.debugger.remote-enabled");
    let remoteEnabled = chromeEnabled && devtoolsRemoteEnabled;
    toggleCmd("Tools:BrowserToolbox", remoteEnabled);
    toggleCmd("Tools:BrowserContentToolbox", remoteEnabled && win.gMultiProcessBrowser);

    // Enable Error Console?
    let consoleEnabled = Services.prefs.getBoolPref("devtools.errorconsole.enabled");
    toggleCmd("Tools:ErrorConsole", consoleEnabled);

    // Enable DevTools connection screen, if the preference allows this.
    toggleCmd("Tools:DevToolsConnect", devtoolsRemoteEnabled);
  },

  observe: function(subject, topic, prefName) {
    if (prefName.endsWith("enabled")) {
      for (let win of this._trackedBrowserWindows) {
        this.updateCommandAvailability(win);
      }
    }
  },

  _prefObserverRegistered: false,

  ensurePrefObserver: function() {
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
  selectToolCommand: function(gBrowser, toolId) {
    let target = devtools.TargetFactory.forTab(gBrowser.selectedTab);
    let toolbox = gDevTools.getToolbox(target);
    let toolDefinition = gDevTools.getToolDefinition(toolId);

    if (toolbox &&
        (toolbox.currentToolId == toolId ||
          (toolId == "webconsole" && toolbox.splitConsole)))
    {
      toolbox.fireCustomKey(toolId);

      if (toolDefinition.preventClosingOnKey || toolbox.hostType == devtools.Toolbox.HostType.WINDOW) {
        toolbox.raise();
      } else {
        toolbox.destroy();
      }
      gDevTools.emit("select-tool-command", toolId);
    } else {
      gDevTools.showToolbox(target, toolId).then(() => {
        let target = devtools.TargetFactory.forTab(gBrowser.selectedTab);
        let toolbox = gDevTools.getToolbox(target);

        toolbox.fireCustomKey(toolId);
        gDevTools.emit("select-tool-command", toolId);
      });
    }
  },

  /**
   * Open a tab to allow connects to a remote browser
   */
  openConnectScreen: function(gBrowser) {
    gBrowser.selectedTab = gBrowser.addTab("chrome://browser/content/devtools/connect.xhtml");
  },

  /**
   * Open the App Manager
   */
  openAppManager: function(gBrowser) {
    gBrowser.selectedTab = gBrowser.addTab("about:app-manager");
  },

  /**
   * Open WebIDE
   */
  openWebIDE: function() {
    let win = Services.wm.getMostRecentWindow("devtools:webide");
    if (win) {
      win.focus();
    } else {
      Services.ww.openWindow(null, "chrome://webide/content/", "webide", "chrome,centerscreen,resizable", null);
    }
  },

  _getContentProcessTarget: function () {
    // Create a DebuggerServer in order to connect locally to it
    if (!DebuggerServer.initialized) {
      DebuggerServer.init();
      DebuggerServer.addBrowserActors();
    }
    DebuggerServer.allowChromeProcess = true;

    let transport = DebuggerServer.connectPipe();
    let client = new DebuggerClient(transport);

    let deferred = promise.defer();
    client.connect(() => {
      client.mainRoot.listProcesses(response => {
        // Do nothing if there is only one process, the parent process.
        let contentProcesses = response.processes.filter(p => (!p.parent));
        if (contentProcesses.length < 1) {
          let msg = bundle.GetStringFromName("toolbox.noContentProcess.message");
          Services.prompt.alert(null, "", msg);
          deferred.reject("No content processes available.");
          return;
        }
        // Otherwise, arbitrary connect to the unique content process.
        client.getProcess(contentProcesses[0].id)
              .then(response => {
                let options = {
                  form: response.form,
                  client: client,
                  chrome: true,
                  isTabActor: false
                };
                return devtools.TargetFactory.forRemoteTab(options);
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
    });

    return deferred.promise;
  },

  openContentProcessToolbox: function () {
    this._getContentProcessTarget()
        .then(target => {
          // Display a new toolbox, in a new window, with debugger by default
          return gDevTools.showToolbox(target, "jsdebugger",
                                       devtools.Toolbox.HostType.WINDOW);
        });
  },

  /**
   * Install WebIDE widget
   */
  installWebIDEWidget: function() {
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
      onCommand: function(aEvent) {
        gDevToolsBrowser.openWebIDE();
      }
    });
  },

  isWebIDEWidgetInstalled: function() {
    let widgetWrapper = CustomizableUI.getWidget("webide-button");
    return !!(widgetWrapper && widgetWrapper.provider == CustomizableUI.PROVIDER_API);
  },

  /**
   * The deferred promise will be resolved by WebIDE's UI.init()
   */
  isWebIDEInitialized: promise.defer(),

  /**
   * Uninstall WebIDE widget
   */
  uninstallWebIDEWidget: function() {
    if (this.isWebIDEWidgetInstalled()) {
      CustomizableUI.removeWidgetFromArea("webide-button");
    }
    CustomizableUI.destroyWidget("webide-button");
  },

  /**
   * Move WebIDE widget to the navbar
   */
  moveWebIDEWidgetInNavbar: function() {
    CustomizableUI.addWidgetToArea("webide-button", CustomizableUI.AREA_NAVBAR);
  },

  /**
   * Add this DevTools's presence to a browser window's document
   *
   * @param {XULDocument} doc
   *        The document to which menuitems and handlers are to be added
   */
  registerBrowserWindow: function DT_registerBrowserWindow(win) {
    this.updateCommandAvailability(win);
    this.ensurePrefObserver();
    gDevToolsBrowser._trackedBrowserWindows.add(win);
    gDevToolsBrowser._addAllToolsToMenu(win.document);

    if (this._isFirebugInstalled()) {
      let broadcaster = win.document.getElementById("devtoolsMenuBroadcaster_DevToolbox");
      broadcaster.removeAttribute("key");
    }

    let tabContainer = win.document.getElementById("tabbrowser-tabs");
    tabContainer.addEventListener("TabSelect", this, false);
    tabContainer.addEventListener("TabOpen", this, false);
    tabContainer.addEventListener("TabClose", this, false);
    tabContainer.addEventListener("TabPinned", this, false);
    tabContainer.addEventListener("TabUnpinned", this, false);
  },

  /**
   * Add a <key> to <keyset id="devtoolsKeyset">.
   * Appending a <key> element is not always enough. The <keyset> needs
   * to be detached and reattached to make sure the <key> is taken into
   * account (see bug 832984).
   *
   * @param {XULDocument} doc
   *        The document to which keys are to be added
   * @param {XULElement} or {DocumentFragment} keys
   *        Keys to add
   */
  attachKeybindingsToBrowser: function DT_attachKeybindingsToBrowser(doc, keys) {
    let devtoolsKeyset = doc.getElementById("devtoolsKeyset");

    if (!devtoolsKeyset) {
      devtoolsKeyset = doc.createElement("keyset");
      devtoolsKeyset.setAttribute("id", "devtoolsKeyset");
    }
    devtoolsKeyset.appendChild(keys);
    let mainKeyset = doc.getElementById("mainKeyset");
    mainKeyset.parentNode.insertBefore(devtoolsKeyset, mainKeyset);
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
      let target = devtools.TargetFactory.forTab(aTab);

      gDevTools.showToolbox(target, "jsdebugger").then(toolbox => {
        let threadClient = toolbox.getCurrentPanel().panelWin.gThreadClient;

        // Break in place, which means resuming the debuggee thread and pausing
        // right before the next step happens.
        switch (threadClient.state) {
          case "paused":
            // When the debugger is already paused.
            threadClient.breakOnNext();
            aCallback();
            break;
          case "attached":
            // When the debugger is already open.
            threadClient.interrupt(() => {
              threadClient.breakOnNext();
              aCallback();
            });
            break;
          case "resuming":
            // The debugger is newly opened.
            threadClient.addOneTimeListener("resumed", () => {
              threadClient.interrupt(() => {
                threadClient.breakOnNext();
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

    debugService.activationHandler = function(aWindow) {
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

    debugService.remoteActivationHandler = function(aBrowser, aCallback) {
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
   * Detect the presence of a Firebug.
   *
   * @return promise
   */
  _isFirebugInstalled: function DT_isFirebugInstalled() {
    let bootstrappedAddons = Services.prefs.getCharPref("extensions.bootstrappedAddons");
    return bootstrappedAddons.indexOf("firebug@software.joehewitt.com") != -1;
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
    } catch(e) {}

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
      let doc = win.document;
      let elements = gDevToolsBrowser._createToolMenuElements(toolDefinition, doc);

      doc.getElementById("mainCommandSet").appendChild(elements.cmd);

      if (elements.key) {
        this.attachKeybindingsToBrowser(doc, elements.key);
      }

      doc.getElementById("mainBroadcasterSet").appendChild(elements.bc);

      let amp = doc.getElementById("appmenu_webDeveloper_popup");
      if (amp) {
        let ref;

        if (prevDef != null) {
          let menuitem = doc.getElementById("appmenuitem_" + prevDef.id);
          ref = menuitem && menuitem.nextSibling ? menuitem.nextSibling : null;
        } else {
          ref = doc.getElementById("appmenu_devtools_separator");
        }

        if (ref) {
          amp.insertBefore(elements.appmenuitem, ref);
        }
      }

      let mp = doc.getElementById("menuWebDeveloperPopup");
      if (mp) {
        let ref;

        if (prevDef != null) {
          let menuitem = doc.getElementById("menuitem_" + prevDef.id);
          ref = menuitem && menuitem.nextSibling ? menuitem.nextSibling : null;
        } else {
          ref = doc.getElementById("menu_devtools_separator");
        }

        if (ref) {
          mp.insertBefore(elements.menuitem, ref);
        }
      }
    }

    if (toolDefinition.id === "jsdebugger") {
      gDevToolsBrowser.setSlowScriptDebugHandler();
    }
  },

  /**
   * Add all tools to the developer tools menu of a window.
   *
   * @param {XULDocument} doc
   *        The document to which the tool items are to be added.
   */
  _addAllToolsToMenu: function DT_addAllToolsToMenu(doc) {
    let fragCommands = doc.createDocumentFragment();
    let fragKeys = doc.createDocumentFragment();
    let fragBroadcasters = doc.createDocumentFragment();
    let fragAppMenuItems = doc.createDocumentFragment();
    let fragMenuItems = doc.createDocumentFragment();

    for (let toolDefinition of gDevTools.getToolDefinitionArray()) {
      if (!toolDefinition.inMenu) {
        continue;
      }

      let elements = gDevToolsBrowser._createToolMenuElements(toolDefinition, doc);

      if (!elements) {
        return;
      }

      fragCommands.appendChild(elements.cmd);
      if (elements.key) {
        fragKeys.appendChild(elements.key);
      }
      fragBroadcasters.appendChild(elements.bc);
      fragAppMenuItems.appendChild(elements.appmenuitem);
      fragMenuItems.appendChild(elements.menuitem);
    }

    let mcs = doc.getElementById("mainCommandSet");
    mcs.appendChild(fragCommands);

    this.attachKeybindingsToBrowser(doc, fragKeys);

    let mbs = doc.getElementById("mainBroadcasterSet");
    mbs.appendChild(fragBroadcasters);

    let amp = doc.getElementById("appmenu_webDeveloper_popup");
    if (amp) {
      let amps = doc.getElementById("appmenu_devtools_separator");
      amp.insertBefore(fragAppMenuItems, amps);
    }

    let mp = doc.getElementById("menuWebDeveloperPopup");
    let mps = doc.getElementById("menu_devtools_separator");
    mp.insertBefore(fragMenuItems, mps);
  },

  /**
   * Add a menu entry for a tool definition
   *
   * @param {string} toolDefinition
   *        Tool definition of the tool to add a menu entry.
   * @param {XULDocument} doc
   *        The document to which the tool menu item is to be added.
   */
  _createToolMenuElements: function DT_createToolMenuElements(toolDefinition, doc) {
    let id = toolDefinition.id;

    // Prevent multiple entries for the same tool.
    if (doc.getElementById("Tools:" + id)) {
      return;
    }

    let cmd = doc.createElement("command");
    cmd.id = "Tools:" + id;
    cmd.setAttribute("oncommand",
        'gDevToolsBrowser.selectToolCommand(gBrowser, "' + id + '");');

    let key = null;
    if (toolDefinition.key) {
      key = doc.createElement("key");
      key.id = "key_" + id;

      if (toolDefinition.key.startsWith("VK_")) {
        key.setAttribute("keycode", toolDefinition.key);
      } else {
        key.setAttribute("key", toolDefinition.key);
      }

      key.setAttribute("command", cmd.id);
      key.setAttribute("modifiers", toolDefinition.modifiers);
    }

    let bc = doc.createElement("broadcaster");
    bc.id = "devtoolsMenuBroadcaster_" + id;
    bc.setAttribute("label", toolDefinition.menuLabel || toolDefinition.label);
    bc.setAttribute("command", cmd.id);

    if (key) {
      bc.setAttribute("key", "key_" + id);
    }

    let appmenuitem = doc.createElement("menuitem");
    appmenuitem.id = "appmenuitem_" + id;
    appmenuitem.setAttribute("observes", "devtoolsMenuBroadcaster_" + id);

    let menuitem = doc.createElement("menuitem");
    menuitem.id = "menuitem_" + id;
    menuitem.setAttribute("observes", "devtoolsMenuBroadcaster_" + id);

    if (toolDefinition.accesskey) {
      menuitem.setAttribute("accesskey", toolDefinition.accesskey);
    }

    return {
      cmd: cmd,
      key: key,
      bc: bc,
      appmenuitem: appmenuitem,
      menuitem: menuitem
    };
  },

  /**
   * Update the "Toggle Tools" checkbox in the developer tools menu. This is
   * called when a toolbox is created or destroyed.
   */
  _updateMenuCheckbox: function DT_updateMenuCheckbox() {
    for (let win of gDevToolsBrowser._trackedBrowserWindows) {

      let hasToolbox = false;
      if (devtools.TargetFactory.isKnownTab(win.gBrowser.selectedTab)) {
        let target = devtools.TargetFactory.forTab(win.gBrowser.selectedTab);
        if (gDevTools._toolboxes.has(target)) {
          hasToolbox = true;
        }
      }

      let broadcaster = win.document.getElementById("devtoolsMenuBroadcaster_DevToolbox");
      if (hasToolbox) {
        broadcaster.setAttribute("checked", "true");
      } else {
        broadcaster.removeAttribute("checked");
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
      gDevToolsBrowser._removeToolFromMenu(toolId, win.document);
    }

    if (toolId === "jsdebugger") {
      gDevToolsBrowser.unsetSlowScriptDebugHandler();
    }
  },

  /**
   * Remove a tool's menuitem from a window
   *
   * @param {string} toolId
   *        Id of the tool to add a menu entry for
   * @param {XULDocument} doc
   *        The document to which the tool menu item is to be removed from
   */
  _removeToolFromMenu: function DT_removeToolFromMenu(toolId, doc) {
    let command = doc.getElementById("Tools:" + toolId);
    if (command) {
      command.parentNode.removeChild(command);
    }

    let key = doc.getElementById("key_" + toolId);
    if (key) {
      key.parentNode.removeChild(key);
    }

    let bc = doc.getElementById("devtoolsMenuBroadcaster_" + toolId);
    if (bc) {
      bc.parentNode.removeChild(bc);
    }

    let appmenuitem = doc.getElementById("appmenuitem_" + toolId);
    if (appmenuitem) {
      appmenuitem.parentNode.removeChild(appmenuitem);
    }

    let menuitem = doc.getElementById("menuitem_" + toolId);
    if (menuitem) {
      menuitem.parentNode.removeChild(menuitem);
    }
  },

  /**
   * Called on browser unload to remove menu entries, toolboxes and event
   * listeners from the closed browser window.
   *
   * @param  {XULWindow} win
   *         The window containing the menu entry
   */
  forgetBrowserWindow: function DT_forgetBrowserWindow(win) {
    gDevToolsBrowser._trackedBrowserWindows.delete(win);

    // Destroy toolboxes for closed window
    for (let [target, toolbox] of gDevTools._toolboxes) {
      if (toolbox.frame && toolbox.frame.ownerDocument.defaultView == win) {
        toolbox.destroy();
      }
    }

    let tabContainer = win.document.getElementById("tabbrowser-tabs");
    tabContainer.removeEventListener("TabSelect", this, false);
    tabContainer.removeEventListener("TabOpen", this, false);
    tabContainer.removeEventListener("TabClose", this, false);
    tabContainer.removeEventListener("TabPinned", this, false);
    tabContainer.removeEventListener("TabUnpinned", this, false);
  },

  handleEvent: function(event) {
    switch (event.type) {
      case "TabOpen":
      case "TabClose":
      case "TabPinned":
      case "TabUnpinned":
        let open = 0;
        let pinned = 0;

        for (let win of this._trackedBrowserWindows) {
          let tabContainer = win.gBrowser.tabContainer;
          let numPinnedTabs = tabContainer.tabbrowser._numPinnedTabs;
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
    }
  },

  /**
   * All browser windows have been closed, tidy up remaining objects.
   */
  destroy: function() {
    Services.prefs.removeObserver("devtools.", gDevToolsBrowser);
    Services.obs.removeObserver(gDevToolsBrowser.destroy, "quit-application");
  },
}

this.gDevToolsBrowser = gDevToolsBrowser;

gDevTools.on("tool-registered", function(ev, toolId) {
  let toolDefinition = gDevTools._tools.get(toolId);
  gDevToolsBrowser._addToolToWindows(toolDefinition);
});

gDevTools.on("tool-unregistered", function(ev, toolId) {
  if (typeof toolId != "string") {
    toolId = toolId.id;
  }
  gDevToolsBrowser._removeToolFromWindows(toolId);
});

gDevTools.on("toolbox-ready", gDevToolsBrowser._updateMenuCheckbox);
gDevTools.on("toolbox-destroyed", gDevToolsBrowser._updateMenuCheckbox);

Services.obs.addObserver(gDevToolsBrowser.destroy, "quit-application", false);

// Load the browser devtools main module as the loader's main module.
devtools.main("main");
