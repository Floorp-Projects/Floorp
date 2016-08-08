/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const MAX_ORDINAL = 99;
const SPLITCONSOLE_ENABLED_PREF = "devtools.toolbox.splitconsoleEnabled";
const SPLITCONSOLE_HEIGHT_PREF = "devtools.toolbox.splitconsoleHeight";
const OS_HISTOGRAM = "DEVTOOLS_OS_ENUMERATED_PER_USER";
const OS_IS_64_BITS = "DEVTOOLS_OS_IS_64_BITS_PER_USER";
const SCREENSIZE_HISTOGRAM = "DEVTOOLS_SCREEN_RESOLUTION_ENUMERATED_PER_USER";
const HTML_NS = "http://www.w3.org/1999/xhtml";
const { SourceMapService } = require("./source-map-service");

var {Cc, Ci, Cu} = require("chrome");
var promise = require("promise");
var defer = require("devtools/shared/defer");
var Services = require("Services");
var {Task} = require("devtools/shared/task");
var {gDevTools} = require("devtools/client/framework/devtools");
var EventEmitter = require("devtools/shared/event-emitter");
var Telemetry = require("devtools/client/shared/telemetry");
var HUDService = require("devtools/client/webconsole/hudservice");
var viewSource = require("devtools/client/shared/view-source");
var { attachThread, detachThread } = require("./attach-thread");
var Menu = require("devtools/client/framework/menu");
var MenuItem = require("devtools/client/framework/menu-item");
var { DOMHelpers } = require("resource://devtools/client/shared/DOMHelpers.jsm");
const { KeyCodes } = require("devtools/client/shared/keycodes");

const { BrowserLoader } =
  Cu.import("resource://devtools/client/shared/browser-loader.js", {});

loader.lazyGetter(this, "toolboxStrings", () => {
  const properties = "chrome://devtools/locale/toolbox.properties";
  const bundle = Services.strings.createBundle(properties);
  return (name, ...args) => {
    try {
      if (!args.length) {
        return bundle.GetStringFromName(name);
      }
      return bundle.formatStringFromName(name, args, args.length);
    } catch (ex) {
      console.log("Error reading '" + name + "'");
      return null;
    }
  };
});
loader.lazyRequireGetter(this, "CommandUtils",
  "devtools/client/shared/developer-toolbar", true);
loader.lazyRequireGetter(this, "getHighlighterUtils",
  "devtools/client/framework/toolbox-highlighter-utils", true);
loader.lazyRequireGetter(this, "Hosts",
  "devtools/client/framework/toolbox-hosts", true);
loader.lazyRequireGetter(this, "Selection",
  "devtools/client/framework/selection", true);
loader.lazyRequireGetter(this, "InspectorFront",
  "devtools/shared/fronts/inspector", true);
loader.lazyRequireGetter(this, "flags",
  "devtools/shared/flags");
loader.lazyRequireGetter(this, "showDoorhanger",
  "devtools/client/shared/doorhanger", true);
loader.lazyRequireGetter(this, "createPerformanceFront",
  "devtools/shared/fronts/performance", true);
loader.lazyRequireGetter(this, "system",
  "devtools/shared/system");
loader.lazyRequireGetter(this, "getPreferenceFront",
  "devtools/shared/fronts/preference", true);
loader.lazyRequireGetter(this, "KeyShortcuts",
  "devtools/client/shared/key-shortcuts", true);
loader.lazyRequireGetter(this, "ZoomKeys",
  "devtools/client/shared/zoom-keys");
loader.lazyRequireGetter(this, "settleAll",
  "devtools/shared/ThreadSafeDevToolsUtils", "settleAll");

loader.lazyGetter(this, "registerHarOverlay", () => {
  return require("devtools/client/netmonitor/har/toolbox-overlay").register;
});

// White-list buttons that can be toggled to prevent adding prefs for
// addons that have manually inserted toolbarbuttons into DOM.
// (By default, supported target is only local tab)
const ToolboxButtons = exports.ToolboxButtons = [
  { id: "command-button-pick",
    isTargetSupported: target =>
      target.getTrait("highlightable")
  },
  { id: "command-button-frames",
    isTargetSupported: target => {
      return target.activeTab && target.activeTab.traits.frames;
    }
  },
  { id: "command-button-splitconsole",
    isTargetSupported: target => !target.isAddon },
  { id: "command-button-responsive" },
  { id: "command-button-paintflashing" },
  { id: "command-button-scratchpad" },
  { id: "command-button-screenshot" },
  { id: "command-button-rulers" },
  { id: "command-button-measure" },
  { id: "command-button-noautohide",
    isTargetSupported: target => target.chrome },
];

/**
 * A "Toolbox" is the component that holds all the tools for one specific
 * target. Visually, it's a document that includes the tools tabs and all
 * the iframes where the tool panels will be living in.
 *
 * @param {object} target
 *        The object the toolbox is debugging.
 * @param {string} selectedTool
 *        Tool to select initially
 * @param {Toolbox.HostType} hostType
 *        Type of host that will host the toolbox (e.g. sidebar, window)
 * @param {object} hostOptions
 *        Options for host specifically
 */
function Toolbox(target, selectedTool, hostType, hostOptions) {
  this._target = target;
  this._toolPanels = new Map();
  this._telemetry = new Telemetry();
  if (Services.prefs.getBoolPref("devtools.sourcemap.locations.enabled")) {
    this._sourceMapService = new SourceMapService(this._target);
  }

  this._initInspector = null;
  this._inspector = null;

  // Map of frames (id => frame-info) and currently selected frame id.
  this.frameMap = new Map();
  this.selectedFrameId = null;

  this._toolRegistered = this._toolRegistered.bind(this);
  this._toolUnregistered = this._toolUnregistered.bind(this);
  this._refreshHostTitle = this._refreshHostTitle.bind(this);
  this._toggleAutohide = this._toggleAutohide.bind(this);
  this.showFramesMenu = this.showFramesMenu.bind(this);
  this._updateFrames = this._updateFrames.bind(this);
  this._splitConsoleOnKeypress = this._splitConsoleOnKeypress.bind(this);
  this.destroy = this.destroy.bind(this);
  this.highlighterUtils = getHighlighterUtils(this);
  this._highlighterReady = this._highlighterReady.bind(this);
  this._highlighterHidden = this._highlighterHidden.bind(this);
  this._prefChanged = this._prefChanged.bind(this);
  this._saveSplitConsoleHeight = this._saveSplitConsoleHeight.bind(this);
  this._onFocus = this._onFocus.bind(this);
  this._showDevEditionPromo = this._showDevEditionPromo.bind(this);
  this._updateTextboxMenuItems = this._updateTextboxMenuItems.bind(this);
  this._onBottomHostMinimized = this._onBottomHostMinimized.bind(this);
  this._onBottomHostMaximized = this._onBottomHostMaximized.bind(this);
  this._onToolSelectWhileMinimized = this._onToolSelectWhileMinimized.bind(this);
  this._onPerformanceFrontEvent = this._onPerformanceFrontEvent.bind(this);
  this._onBottomHostWillChange = this._onBottomHostWillChange.bind(this);
  this._toggleMinimizeMode = this._toggleMinimizeMode.bind(this);
  this._onTabbarFocus = this._onTabbarFocus.bind(this);
  this._onTabbarArrowKeypress = this._onTabbarArrowKeypress.bind(this);

  this._target.on("close", this.destroy);

  if (!hostType) {
    hostType = Services.prefs.getCharPref(this._prefs.LAST_HOST);
  }
  if (!selectedTool) {
    selectedTool = Services.prefs.getCharPref(this._prefs.LAST_TOOL);
  }
  this._defaultToolId = selectedTool;

  this._hostOptions = hostOptions;
  this._host = this._createHost(hostType, hostOptions);

  EventEmitter.decorate(this);

  this._target.on("navigate", this._refreshHostTitle);
  this._target.on("frame-update", this._updateFrames);

  this.on("host-changed", this._refreshHostTitle);
  this.on("select", this._refreshHostTitle);

  this.on("ready", this._showDevEditionPromo);

  gDevTools.on("tool-registered", this._toolRegistered);
  gDevTools.on("tool-unregistered", this._toolUnregistered);
}
exports.Toolbox = Toolbox;

/**
 * The toolbox can be 'hosted' either embedded in a browser window
 * or in a separate window.
 */
Toolbox.HostType = {
  BOTTOM: "bottom",
  SIDE: "side",
  WINDOW: "window",
  CUSTOM: "custom"
};

Toolbox.prototype = {
  _URL: "about:devtools-toolbox",

  _prefs: {
    LAST_HOST: "devtools.toolbox.host",
    LAST_TOOL: "devtools.toolbox.selectedTool",
    SIDE_ENABLED: "devtools.toolbox.sideEnabled",
    PREVIOUS_HOST: "devtools.toolbox.previousHost"
  },

  currentToolId: null,
  lastUsedToolId: null,

  /**
   * Returns a *copy* of the _toolPanels collection.
   *
   * @return {Map} panels
   *         All the running panels in the toolbox
   */
  getToolPanels: function () {
    return new Map(this._toolPanels);
  },

  /**
   * Access the panel for a given tool
   */
  getPanel: function (id) {
    return this._toolPanels.get(id);
  },

  /**
   * Get the panel instance for a given tool once it is ready.
   * If the tool is already opened, the promise will resolve immediately,
   * otherwise it will wait until the tool has been opened before resolving.
   *
   * Note that this does not open the tool, use selectTool if you'd
   * like to select the tool right away.
   *
   * @param  {String} id
   *         The id of the panel, for example "jsdebugger".
   * @returns Promise
   *          A promise that resolves once the panel is ready.
   */
  getPanelWhenReady: function (id) {
    let deferred = defer();
    let panel = this.getPanel(id);
    if (panel) {
      deferred.resolve(panel);
    } else {
      this.on(id + "-ready", (e, panel) => {
        deferred.resolve(panel);
      });
    }

    return deferred.promise;
  },

  /**
   * This is a shortcut for getPanel(currentToolId) because it is much more
   * likely that we're going to want to get the panel that we've just made
   * visible
   */
  getCurrentPanel: function () {
    return this._toolPanels.get(this.currentToolId);
  },

  /**
   * Get/alter the target of a Toolbox so we're debugging something different.
   * See Target.jsm for more details.
   * TODO: Do we allow |toolbox.target = null;| ?
   */
  get target() {
    return this._target;
  },

  get threadClient() {
    return this._threadClient;
  },

  /**
   * Get/alter the host of a Toolbox, i.e. is it in browser or in a separate
   * tab. See HostType for more details.
   */
  get hostType() {
    return this._host.type;
  },

  /**
   * Get the iframe containing the toolbox UI.
   */
  get frame() {
    return this._host.frame;
  },

  /**
   * Shortcut to the window containing the toolbox UI
   */
  get win() {
    return this.frame.contentWindow;
  },

  /**
   * Shortcut to the document containing the toolbox UI
   */
  get doc() {
    return this.frame.contentDocument;
  },

  /**
   * Get the toolbox highlighter front. Note that it may not always have been
   * initialized first. Use `initInspector()` if needed.
   * Consider using highlighterUtils instead, it exposes the highlighter API in
   * a useful way for the toolbox panels
   */
  get highlighter() {
    return this._highlighter;
  },

  /**
   * Get the toolbox's performance front. Note that it may not always have been
   * initialized first. Use `initPerformance()` if needed.
   */
  get performance() {
    return this._performance;
  },

  /**
   * Get the toolbox's inspector front. Note that it may not always have been
   * initialized first. Use `initInspector()` if needed.
   */
  get inspector() {
    return this._inspector;
  },

  /**
   * Get the toolbox's walker front. Note that it may not always have been
   * initialized first. Use `initInspector()` if needed.
   */
  get walker() {
    return this._walker;
  },

  /**
   * Get the toolbox's node selection. Note that it may not always have been
   * initialized first. Use `initInspector()` if needed.
   */
  get selection() {
    return this._selection;
  },

  /**
   * Get the toggled state of the split console
   */
  get splitConsole() {
    return this._splitConsole;
  },
  /**
   * Get the focused state of the split console
   */
  isSplitConsoleFocused: function () {
    if (!this._splitConsole) {
      return false;
    }
    let focusedWin = Services.focus.focusedWindow;
    return focusedWin && focusedWin ===
      this.doc.querySelector("#toolbox-panel-iframe-webconsole").contentWindow;
  },

  /**
   * Open the toolbox
   */
  open: function () {
    return Task.spawn(function* () {
      let iframe = yield this._host.create();
      let domReady = defer();

      // Prevent reloading the document when the toolbox is opened in a tab
      let location = iframe.contentWindow.location.href;
      if (!location.startsWith(this._URL)) {
        iframe.setAttribute("src", this._URL);
      } else {
        // Update the URL so that onceDOMReady watch for the right url.
        this._URL = location;
      }

      this.browserRequire = BrowserLoader({
        window: this.doc.defaultView,
        useOnlyShared: true
      }).require;

      iframe.setAttribute("aria-label", toolboxStrings("toolbox.label"));
      let domHelper = new DOMHelpers(iframe.contentWindow);
      domHelper.onceDOMReady(() => {
        domReady.resolve();
      }, this._URL);

      // Optimization: fire up a few other things before waiting on
      // the iframe being ready (makes startup faster)

      // Load the toolbox-level actor fronts and utilities now
      yield this._target.makeRemote();

      // Attach the thread
      this._threadClient = yield attachThread(this);
      yield domReady.promise;

      this.isReady = true;
      let framesPromise = this._listFrames();

      this.closeButton = this.doc.getElementById("toolbox-close");
      this.closeButton.addEventListener("click", this.destroy, true);

      gDevTools.on("pref-changed", this._prefChanged);

      let framesMenu = this.doc.getElementById("command-button-frames");
      framesMenu.addEventListener("click", this.showFramesMenu, false);

      let noautohideMenu = this.doc.getElementById("command-button-noautohide");
      noautohideMenu.addEventListener("click", this._toggleAutohide, true);

      this.textboxContextMenuPopup =
        this.doc.getElementById("toolbox-textbox-context-popup");
      this.textboxContextMenuPopup.addEventListener("popupshowing",
        this._updateTextboxMenuItems, true);

      var shortcuts = new KeyShortcuts({
        window: this.doc.defaultView
      });
      this._buildDockButtons();
      this._buildOptions(shortcuts);
      this._buildTabs();
      this._applyCacheSettings();
      this._applyServiceWorkersTestingSettings();
      this._addKeysToWindow();
      this._addReloadKeys(shortcuts);
      this._addHostListeners(shortcuts);
      this._registerOverlays();
      if (!this._hostOptions || this._hostOptions.zoom === true) {
        ZoomKeys.register(this.win);
      }

      this.tabbar = this.doc.querySelector(".devtools-tabbar");
      this.tabbar.addEventListener("focus", this._onTabbarFocus, true);
      this.tabbar.addEventListener("click", this._onTabbarFocus, true);
      this.tabbar.addEventListener("keypress", this._onTabbarArrowKeypress);

      this.webconsolePanel = this.doc.querySelector("#toolbox-panel-webconsole");
      this.webconsolePanel.height = Services.prefs.getIntPref(SPLITCONSOLE_HEIGHT_PREF);
      this.webconsolePanel.addEventListener("resize", this._saveSplitConsoleHeight);

      let buttonsPromise = this._buildButtons();

      this._pingTelemetry();

      // The isTargetSupported check needs to happen after the target is
      // remoted, otherwise we could have done it in the toolbox constructor
      // (bug 1072764).
      let toolDef = gDevTools.getToolDefinition(this._defaultToolId);
      if (!toolDef || !toolDef.isTargetSupported(this._target)) {
        this._defaultToolId = "webconsole";
      }

      yield this.selectTool(this._defaultToolId);

      // Wait until the original tool is selected so that the split
      // console input will receive focus.
      let splitConsolePromise = promise.resolve();
      if (Services.prefs.getBoolPref(SPLITCONSOLE_ENABLED_PREF)) {
        splitConsolePromise = this.openSplitConsole();
      }

      yield promise.all([
        splitConsolePromise,
        buttonsPromise,
        framesPromise
      ]);

      // Lazily connect to the profiler here and don't wait for it to complete,
      // used to intercept console.profile calls before the performance tools are open.
      let performanceFrontConnection = this.initPerformance();

      // If in testing environment, wait for performance connection to finish,
      // so we don't have to explicitly wait for this in tests; ideally, all tests
      // will handle this on their own, but each have their own tear down function.
      if (flags.testing) {
        yield performanceFrontConnection;
      }

      this.emit("ready");
    }.bind(this)).then(null, console.error.bind(console));
  },

  /**
   * loading React modules when needed (to avoid performance penalties
   * during Firefox start up time).
   */
  get React() {
    return this.browserRequire("devtools/client/shared/vendor/react");
  },

  get ReactDOM() {
    return this.browserRequire("devtools/client/shared/vendor/react-dom");
  },

  _pingTelemetry: function () {
    this._telemetry.toolOpened("toolbox");

    this._telemetry.logOncePerBrowserVersion(OS_HISTOGRAM, system.getOSCPU());
    this._telemetry.logOncePerBrowserVersion(OS_IS_64_BITS,
                                             Services.appinfo.is64Bit ? 1 : 0);
    this._telemetry.logOncePerBrowserVersion(SCREENSIZE_HISTOGRAM, system.getScreenDimensions());
  },

  /**
   * Because our panels are lazy loaded this is a good place to watch for
   * "pref-changed" events.
   * @param  {String} event
   *         The event type, "pref-changed".
   * @param  {Object} data
   *         {
   *           newValue: The new value
   *           oldValue:  The old value
   *           pref: The name of the preference that has changed
   *         }
   */
  _prefChanged: function (event, data) {
    switch (data.pref) {
      case "devtools.cache.disabled":
        this._applyCacheSettings();
        break;
      case "devtools.serviceWorkers.testing.enabled":
        this._applyServiceWorkersTestingSettings();
        break;
    }
  },

  _buildOptions: function (shortcuts) {
    let selectOptions = (name, event) => {
      // Flip back to the last used panel if we are already
      // on the options panel.
      if (this.currentToolId === "options" &&
          gDevTools.getToolDefinition(this.lastUsedToolId)) {
        this.selectTool(this.lastUsedToolId);
      } else {
        this.selectTool("options");
      }
      // Prevent the opening of bookmarks window on toolbox.options.key
      event.preventDefault();
    };
    shortcuts.on(toolboxStrings("toolbox.options.key"), selectOptions);
    shortcuts.on(toolboxStrings("toolbox.help.key"), selectOptions);
  },

  _splitConsoleOnKeypress: function (e) {
    if (e.keyCode === KeyCodes.DOM_VK_ESCAPE) {
      this.toggleSplitConsole();
      // If the debugger is paused, don't let the ESC key stop any pending
      // navigation.
      let jsdebugger = this.getPanel("jsdebugger");
      if (jsdebugger && jsdebugger.panelWin.gThreadClient.state == "paused") {
        e.preventDefault();
      }
    }
  },

  /**
   * Add a shortcut key that should work when a split console
   * has focus to the toolbox.
   *
   * @param {element} keyElement
   *        They <key> XUL element describing the shortcut key
   * @param {string} whichTool
   *        The tool the key belongs to. The corresponding command
   *        will only trigger if this tool is active.
   */
  useKeyWithSplitConsole: function (keyElement, whichTool) {
    let cloned = keyElement.cloneNode();
    cloned.setAttribute("oncommand", "void(0)");
    cloned.removeAttribute("command");
    cloned.addEventListener("command", (e) => {
      // Only forward the command if the tool is active
      if (this.currentToolId === whichTool && this.isSplitConsoleFocused()) {
        keyElement.doCommand();
      }
    }, true);
    this.doc.getElementById("toolbox-keyset").appendChild(cloned);
  },

  _addReloadKeys: function (shortcuts) {
    [
      ["reload", false],
      ["reload2", false],
      ["forceReload", true],
      ["forceReload2", true]
    ].forEach(([id, force]) => {
      let key = toolboxStrings("toolbox." + id + ".key");
      shortcuts.on(key, (name, event) => {
        this.reloadTarget(force);

        // Prevent Firefox shortcuts from reloading the page
        event.preventDefault();
      });
    });
  },

  _addHostListeners: function (shortcuts) {
    shortcuts.on(toolboxStrings("toolbox.nextTool.key"),
                 this.selectNextTool.bind(this));
    shortcuts.on(toolboxStrings("toolbox.previousTool.key"),
                 this.selectPreviousTool.bind(this));
    shortcuts.on(toolboxStrings("toolbox.minimize.key"),
                 this._toggleMinimizeMode.bind(this));
    shortcuts.on(toolboxStrings("toolbox.toggleHost.key"),
                 (name, event) => {
                   this.switchToPreviousHost();
                   event.preventDefault();
                 });

    this.doc.addEventListener("keypress", this._splitConsoleOnKeypress, false);

    this.doc.addEventListener("focus", this._onFocus, true);
  },

  _registerOverlays: function () {
    registerHarOverlay(this);
  },

  _saveSplitConsoleHeight: function () {
    Services.prefs.setIntPref(SPLITCONSOLE_HEIGHT_PREF,
      this.webconsolePanel.height);
  },

  /**
   * Make sure that the console is showing up properly based on all the
   * possible conditions.
   *   1) If the console tab is selected, then regardless of split state
   *      it should take up the full height of the deck, and we should
   *      hide the deck and splitter.
   *   2) If the console tab is not selected and it is split, then we should
   *      show the splitter, deck, and console.
   *   3) If the console tab is not selected and it is *not* split,
   *      then we should hide the console and splitter, and show the deck
   *      at full height.
   */
  _refreshConsoleDisplay: function () {
    let deck = this.doc.getElementById("toolbox-deck");
    let webconsolePanel = this.webconsolePanel;
    let splitter = this.doc.getElementById("toolbox-console-splitter");
    let openedConsolePanel = this.currentToolId === "webconsole";

    if (openedConsolePanel) {
      deck.setAttribute("collapsed", "true");
      splitter.setAttribute("hidden", "true");
      webconsolePanel.removeAttribute("collapsed");
    } else {
      deck.removeAttribute("collapsed");
      if (this.splitConsole) {
        webconsolePanel.removeAttribute("collapsed");
        splitter.removeAttribute("hidden");
      } else {
        webconsolePanel.setAttribute("collapsed", "true");
        splitter.setAttribute("hidden", "true");
      }
    }
  },

  /**
   * Adds the keys and commands to the Toolbox Window in window mode.
   */
  _addKeysToWindow: function () {
    if (this.hostType != Toolbox.HostType.WINDOW) {
      return;
    }

    let doc = this.win.parent.document;

    for (let [id, toolDefinition] of gDevTools.getToolDefinitionMap()) {
      // Prevent multiple entries for the same tool.
      if (!toolDefinition.key || doc.getElementById("key_" + id)) {
        continue;
      }

      let toolId = id;
      let key = doc.createElement("key");

      key.id = "key_" + toolId;

      if (toolDefinition.key.startsWith("VK_")) {
        key.setAttribute("keycode", toolDefinition.key);
      } else {
        key.setAttribute("key", toolDefinition.key);
      }

      key.setAttribute("modifiers", toolDefinition.modifiers);
      // needed. See bug 371900
      key.setAttribute("oncommand", "void(0);");
      key.addEventListener("command", () => {
        this.selectTool(toolId).then(() => this.fireCustomKey(toolId));
      }, true);
      doc.getElementById("toolbox-keyset").appendChild(key);
    }

    // Add key for toggling the browser console from the detached window
    if (!doc.getElementById("key_browserconsole")) {
      let key = doc.createElement("key");
      key.id = "key_browserconsole";

      key.setAttribute("key", toolboxStrings("browserConsoleCmd.commandkey"));
      key.setAttribute("modifiers", "accel,shift");
      // needed. See bug 371900
      key.setAttribute("oncommand", "void(0)");
      key.addEventListener("command", () => {
        HUDService.toggleBrowserConsole();
      }, true);
      doc.getElementById("toolbox-keyset").appendChild(key);
    }
  },

  /**
   * Handle any custom key events.  Returns true if there was a custom key
   * binding run.
   * @param {string} toolId Which tool to run the command on (skip if not
   * current)
   */
  fireCustomKey: function (toolId) {
    let toolDefinition = gDevTools.getToolDefinition(toolId);

    if (toolDefinition.onkey &&
        ((this.currentToolId === toolId) ||
          (toolId == "webconsole" && this.splitConsole))) {
      toolDefinition.onkey(this.getCurrentPanel(), this);
    }
  },

  /**
   * Build the notification box as soon as needed.
   */
  get notificationBox() {
    if (!this._notificationBox) {
      let { NotificationBox, PriorityLevels } =
        this.browserRequire(
          "devtools/client/shared/components/notification-box");

      NotificationBox = this.React.createFactory(NotificationBox);

      // Render NotificationBox and assign priority levels to it.
      let box = this.doc.getElementById("toolbox-notificationbox");
      this._notificationBox = Object.assign(
        this.ReactDOM.render(NotificationBox({}), box),
        PriorityLevels);
    }
    return this._notificationBox;
  },

  /**
   * Build the buttons for changing hosts. Called every time
   * the host changes.
   */
  _buildDockButtons: function () {
    let dockBox = this.doc.getElementById("toolbox-dock-buttons");

    while (dockBox.firstChild) {
      dockBox.removeChild(dockBox.firstChild);
    }

    if (!this._target.isLocalTab) {
      return;
    }

    // Bottom-type host can be minimized, add a button for this.
    if (this.hostType == Toolbox.HostType.BOTTOM) {
      let minimizeBtn = this.doc.createElementNS(HTML_NS, "button");
      minimizeBtn.id = "toolbox-dock-bottom-minimize";
      minimizeBtn.className = "devtools-button";
      /* Bug 1177463 - The minimize button is currently hidden until we agree on
         the UI for it, and until bug 1173849 is fixed too. */
      minimizeBtn.setAttribute("hidden", "true");

      minimizeBtn.addEventListener("click", this._toggleMinimizeMode);
      dockBox.appendChild(minimizeBtn);
      // Show the button in its maximized state.
      this._onBottomHostMaximized();

      // Update the label and icon when the state changes.
      this._host.on("minimized", this._onBottomHostMinimized);
      this._host.on("maximized", this._onBottomHostMaximized);
      // Maximize again when a tool gets selected.
      this.on("before-select", this._onToolSelectWhileMinimized);
      // Maximize and stop listening before the host type changes.
      this.once("host-will-change", this._onBottomHostWillChange);
    }

    if (this.hostType == Toolbox.HostType.WINDOW) {
      this.closeButton.setAttribute("hidden", "true");
    } else {
      this.closeButton.removeAttribute("hidden");
    }

    let sideEnabled = Services.prefs.getBoolPref(this._prefs.SIDE_ENABLED);

    for (let type in Toolbox.HostType) {
      let position = Toolbox.HostType[type];
      if (position == this.hostType ||
          position == Toolbox.HostType.CUSTOM ||
          (!sideEnabled && position == Toolbox.HostType.SIDE)) {
        continue;
      }

      let button = this.doc.createElementNS(HTML_NS, "button");
      button.id = "toolbox-dock-" + position;
      button.className = "toolbox-dock-button devtools-button";
      button.setAttribute("title", toolboxStrings("toolboxDockButtons." +
                                                  position + ".tooltip"));
      button.addEventListener("click", () => {
        this.switchHost(position);
      });

      dockBox.appendChild(button);
    }
  },

  _getMinimizeButtonShortcutTooltip: function () {
    let str = toolboxStrings("toolbox.minimize.key");
    let key = KeyShortcuts.parseElectronKey(this.win, str);
    return "(" + KeyShortcuts.stringify(key) + ")";
  },

  _onBottomHostMinimized: function () {
    let btn = this.doc.querySelector("#toolbox-dock-bottom-minimize");
    btn.className = "minimized";

    btn.setAttribute("title",
      toolboxStrings("toolboxDockButtons.bottom.maximize") + " " +
      this._getMinimizeButtonShortcutTooltip());
  },

  _onBottomHostMaximized: function () {
    let btn = this.doc.querySelector("#toolbox-dock-bottom-minimize");
    btn.className = "maximized";

    btn.setAttribute("title",
      toolboxStrings("toolboxDockButtons.bottom.minimize") + " " +
      this._getMinimizeButtonShortcutTooltip());
  },

  _onToolSelectWhileMinimized: function () {
    this._host.maximize();
  },

  _onBottomHostWillChange: function () {
    this._host.maximize();

    this._host.off("minimized", this._onBottomHostMinimized);
    this._host.off("maximized", this._onBottomHostMaximized);
    this.off("before-select", this._onToolSelectWhileMinimized);
  },

  _toggleMinimizeMode: function () {
    if (this.hostType !== Toolbox.HostType.BOTTOM) {
      return;
    }

    // Calculate the height to which the host should be minimized so the
    // tabbar is still visible.
    let toolbarHeight = this.tabbar.getBoxQuads({box: "content"})[0].bounds
                                                                    .height;
    this._host.toggleMinimizeMode(toolbarHeight);
  },

  /**
   * Add tabs to the toolbox UI for registered tools
   */
  _buildTabs: function () {
    for (let definition of gDevTools.getToolDefinitionArray()) {
      this._buildTabForTool(definition);
    }
  },

  /**
   * Get all dev tools tab bar focusable elements. These are visible elements
   * such as buttons or elements with tabindex.
   */
  get tabbarFocusableElms() {
    return [...this.tabbar.querySelectorAll(
      "[tabindex]:not([hidden]), button:not([hidden])")];
  },

  /**
   * Reset tabindex attributes across all focusable elements inside the tabbar.
   * Only have one element with tabindex=0 at a time to make sure that tabbing
   * results in navigating away from the tabbar container.
   * @param  {FocusEvent} event
   */
  _onTabbarFocus: function (event) {
    this.tabbarFocusableElms.forEach(elm =>
      elm.setAttribute("tabindex", event.target === elm ? "0" : "-1"));
  },

  /**
   * On left/right arrow press, attempt to move the focus inside the tabbar to
   * the previous/next focusable element.
   * @param  {KeyboardEvent} event
   */
  _onTabbarArrowKeypress: function (event) {
    let { key, target } = event;
    let focusableElms = this.tabbarFocusableElms;
    let curIndex = focusableElms.indexOf(target);

    if (curIndex === -1) {
      console.warn(target + " is not found among Developer Tools tab bar " +
        "focusable elements. It needs to either be a button or have " +
        "tabindex. If it is intended to be hidden, 'hidden' attribute must " +
        "be used.");
      return;
    }

    let newTarget;

    if (key === "ArrowLeft") {
      // Do nothing if already at the beginning.
      if (curIndex === 0) {
        return;
      }
      newTarget = focusableElms[curIndex - 1];
    } else if (key === "ArrowRight") {
      // Do nothing if already at the end.
      if (curIndex === focusableElms.length - 1) {
        return;
      }
      newTarget = focusableElms[curIndex + 1];
    } else {
      return;
    }

    focusableElms.forEach(elm =>
      elm.setAttribute("tabindex", newTarget === elm ? "0" : "-1"));
    newTarget.focus();

    event.preventDefault();
    event.stopPropagation();
  },

  /**
   * Add buttons to the UI as specified in the devtools.toolbox.toolbarSpec pref
   */
  _buildButtons: function () {
    if (!this.target.isAddon || this.target.isWebExtension) {
      this._buildPickerButton();
    }

    this.setToolboxButtonsVisibility();

    // Old servers don't have a GCLI Actor, so just return
    if (!this.target.hasActor("gcli")) {
      return promise.resolve();
    }
    // Disable gcli in browser toolbox until there is usages of it
    if (this.target.chrome) {
      return promise.resolve();
    }

    const options = {
      environment: CommandUtils.createEnvironment(this, "_target")
    };
    return CommandUtils.createRequisition(this.target, options).then(requisition => {
      this._requisition = requisition;

      const spec = CommandUtils.getCommandbarSpec("devtools.toolbox.toolbarSpec");
      return CommandUtils.createButtons(spec, this.target, this.doc,
                                        requisition).then(buttons => {
                                          let container = this.doc.getElementById("toolbox-buttons");
                                          buttons.forEach(button=> {
                                            if (button) {
                                              container.appendChild(button);
                                            }
                                          });
                                          this.setToolboxButtonsVisibility();
                                        });
    });
  },

  /**
   * Adding the element picker button is done here unlike the other buttons
   * since we want it to work for remote targets too
   */
  _buildPickerButton: function () {
    this._pickerButton = this.doc.createElementNS(HTML_NS, "button");
    this._pickerButton.id = "command-button-pick";
    this._pickerButton.className = "command-button command-button-invertable devtools-button";
    this._pickerButton.setAttribute("title", toolboxStrings("pickButton.tooltip"));
    this._pickerButton.setAttribute("hidden", "true");

    let container = this.doc.querySelector("#toolbox-picker-container");
    container.appendChild(this._pickerButton);

    this._togglePicker = this.highlighterUtils.togglePicker.bind(this.highlighterUtils);
    this._pickerButton.addEventListener("click", this._togglePicker, false);
  },

  /**
   * Apply the current cache setting from devtools.cache.disabled to this
   * toolbox's tab.
   */
  _applyCacheSettings: function () {
    let pref = "devtools.cache.disabled";
    let cacheDisabled = Services.prefs.getBoolPref(pref);

    if (this.target.activeTab) {
      this.target.activeTab.reconfigure({"cacheDisabled": cacheDisabled});
    }
  },

  /**
   * Apply the current service workers testing setting from
   * devtools.serviceWorkers.testing.enabled to this toolbox's tab.
   */
  _applyServiceWorkersTestingSettings: function () {
    let pref = "devtools.serviceWorkers.testing.enabled";
    let serviceWorkersTestingEnabled =
      Services.prefs.getBoolPref(pref) || false;

    if (this.target.activeTab) {
      this.target.activeTab.reconfigure({
        "serviceWorkersTestingEnabled": serviceWorkersTestingEnabled
      });
    }
  },

  /**
   * Setter for the checked state of the picker button in the toolbar
   * @param {Boolean} isChecked
   */
  set pickerButtonChecked(isChecked) {
    if (isChecked) {
      this._pickerButton.setAttribute("checked", "true");
    } else {
      this._pickerButton.removeAttribute("checked");
    }
  },

  /**
   * Return all toolbox buttons (command buttons, plus any others that were
   * added manually).
   */
  get toolboxButtons() {
    return ToolboxButtons.map(options => {
      let button = this.doc.getElementById(options.id);
      // Some buttons may not exist inside of Browser Toolbox
      if (!button) {
        return false;
      }

      return {
        id: options.id,
        button: button,
        label: button.getAttribute("title"),
        visibilityswitch: "devtools." + options.id + ".enabled",
        isTargetSupported: options.isTargetSupported
                           ? options.isTargetSupported
                           : target => target.isLocalTab,
      };
    }).filter(button=>button);
  },

  /**
   * Ensure the visibility of each toolbox button matches the
   * preference value.  Simply hide buttons that are preffed off.
   */
  setToolboxButtonsVisibility: function () {
    this.toolboxButtons.forEach(buttonSpec => {
      let { visibilityswitch, button, isTargetSupported } = buttonSpec;
      let on = true;
      try {
        on = Services.prefs.getBoolPref(visibilityswitch);
      } catch (ex) { }

      on = on && isTargetSupported(this.target);

      if (button) {
        if (on) {
          button.removeAttribute("hidden");
        } else {
          button.setAttribute("hidden", "true");
        }
      }
    });

    this._updateNoautohideButton();
  },

  /**
   * Build a tab for one tool definition and add to the toolbox
   *
   * @param {string} toolDefinition
   *        Tool definition of the tool to build a tab for.
   */
  _buildTabForTool: function (toolDefinition) {
    if (!toolDefinition.isTargetSupported(this._target)) {
      return;
    }

    let tabs = this.doc.getElementById("toolbox-tabs");
    let deck = this.doc.getElementById("toolbox-deck");

    let id = toolDefinition.id;

    if (toolDefinition.ordinal == undefined || toolDefinition.ordinal < 0) {
      toolDefinition.ordinal = MAX_ORDINAL;
    }

    let radio = this.doc.createElement("radio");
    // The radio element is not being used in the conventional way, thus
    // the devtools-tab class replaces the radio XBL binding with its base
    // binding (the control-item binding).
    radio.className = "devtools-tab";
    radio.id = "toolbox-tab-" + id;
    radio.setAttribute("toolid", id);
    radio.setAttribute("tabindex", "0");
    radio.setAttribute("ordinal", toolDefinition.ordinal);
    radio.setAttribute("tooltiptext", toolDefinition.tooltip);
    if (toolDefinition.invertIconForLightTheme) {
      radio.setAttribute("icon-invertable", "light-theme");
    } else if (toolDefinition.invertIconForDarkTheme) {
      radio.setAttribute("icon-invertable", "dark-theme");
    }

    radio.addEventListener("command", () => {
      this.selectTool(id);
    });

    // spacer lets us center the image and label, while allowing cropping
    let spacer = this.doc.createElement("spacer");
    spacer.setAttribute("flex", "1");
    radio.appendChild(spacer);

    if (toolDefinition.icon) {
      let image = this.doc.createElement("image");
      image.className = "default-icon";
      image.setAttribute("src",
                         toolDefinition.icon || toolDefinition.highlightedicon);
      radio.appendChild(image);
      // Adding the highlighted icon image
      image = this.doc.createElement("image");
      image.className = "highlighted-icon";
      image.setAttribute("src",
                         toolDefinition.highlightedicon || toolDefinition.icon);
      radio.appendChild(image);
    }

    if (toolDefinition.label && !toolDefinition.iconOnly) {
      let label = this.doc.createElement("label");
      label.setAttribute("value", toolDefinition.label);
      label.setAttribute("crop", "end");
      label.setAttribute("flex", "1");
      radio.appendChild(label);
    }

    if (!toolDefinition.bgTheme) {
      toolDefinition.bgTheme = "theme-toolbar";
    }
    let vbox = this.doc.createElement("vbox");
    vbox.className = "toolbox-panel " + toolDefinition.bgTheme;

    // There is already a container for the webconsole frame.
    if (!this.doc.getElementById("toolbox-panel-" + id)) {
      vbox.id = "toolbox-panel-" + id;
    }

    if (id === "options") {
      // Options panel is special.  It doesn't belong in the same container as
      // the other tabs.
      radio.setAttribute("role", "button");
      let optionTabContainer = this.doc.getElementById("toolbox-option-container");
      optionTabContainer.appendChild(radio);
      deck.appendChild(vbox);
    } else {
      radio.setAttribute("role", "tab");

      // If there is no tab yet, or the ordinal to be added is the largest one.
      if (tabs.childNodes.length == 0 ||
          tabs.lastChild.getAttribute("ordinal") <= toolDefinition.ordinal) {
        tabs.appendChild(radio);
        deck.appendChild(vbox);
      } else {
        // else, iterate over all the tabs to get the correct location.
        Array.some(tabs.childNodes, (node, i) => {
          if (+node.getAttribute("ordinal") > toolDefinition.ordinal) {
            tabs.insertBefore(radio, node);
            deck.insertBefore(vbox, deck.childNodes[i]);
            return true;
          }
          return false;
        });
      }
    }

    this._addKeysToWindow();
  },

  /**
   * Ensure the tool with the given id is loaded.
   *
   * @param {string} id
   *        The id of the tool to load.
   */
  loadTool: function (id) {
    if (id === "inspector" && !this._inspector) {
      return this.initInspector().then(() => {
        return this.loadTool(id);
      });
    }

    let deferred = defer();
    let iframe = this.doc.getElementById("toolbox-panel-iframe-" + id);

    if (iframe) {
      let panel = this._toolPanels.get(id);
      if (panel) {
        deferred.resolve(panel);
      } else {
        this.once(id + "-ready", panel => {
          deferred.resolve(panel);
        });
      }
      return deferred.promise;
    }

    let definition = gDevTools.getToolDefinition(id);
    if (!definition) {
      deferred.reject(new Error("no such tool id " + id));
      return deferred.promise;
    }

    iframe = this.doc.createElement("iframe");
    iframe.className = "toolbox-panel-iframe";
    iframe.id = "toolbox-panel-iframe-" + id;
    iframe.setAttribute("flex", 1);
    iframe.setAttribute("forceOwnRefreshDriver", "");
    iframe.tooltip = "aHTMLTooltip";
    iframe.style.visibility = "hidden";

    gDevTools.emit(id + "-init", this, iframe);
    this.emit(id + "-init", iframe);

    // If no parent yet, append the frame into default location.
    if (!iframe.parentNode) {
      let vbox = this.doc.getElementById("toolbox-panel-" + id);
      vbox.appendChild(iframe);
    }

    let onLoad = () => {
      // Prevent flicker while loading by waiting to make visible until now.
      iframe.style.visibility = "visible";

      // The build method should return a panel instance, so events can
      // be fired with the panel as an argument. However, in order to keep
      // backward compatibility with existing extensions do a check
      // for a promise return value.
      let built = definition.build(iframe.contentWindow, this);
      if (!(typeof built.then == "function")) {
        let panel = built;
        iframe.panel = panel;

        // The panel instance is expected to fire (and listen to) various
        // framework events, so make sure it's properly decorated with
        // appropriate API (on, off, once, emit).
        // In this case we decorate panel instances directly returned by
        // the tool definition 'build' method.
        if (typeof panel.emit == "undefined") {
          EventEmitter.decorate(panel);
        }

        gDevTools.emit(id + "-build", this, panel);
        this.emit(id + "-build", panel);

        // The panel can implement an 'open' method for asynchronous
        // initialization sequence.
        if (typeof panel.open == "function") {
          built = panel.open();
        } else {
          let buildDeferred = defer();
          buildDeferred.resolve(panel);
          built = buildDeferred.promise;
        }
      }

      // Wait till the panel is fully ready and fire 'ready' events.
      promise.resolve(built).then((panel) => {
        this._toolPanels.set(id, panel);

        // Make sure to decorate panel object with event API also in case
        // where the tool definition 'build' method returns only a promise
        // and the actual panel instance is available as soon as the
        // promise is resolved.
        if (typeof panel.emit == "undefined") {
          EventEmitter.decorate(panel);
        }

        gDevTools.emit(id + "-ready", this, panel);
        this.emit(id + "-ready", panel);

        deferred.resolve(panel);
      }, console.error);
    };

    iframe.setAttribute("src", definition.url);
    if (definition.panelLabel) {
      iframe.setAttribute("aria-label", definition.panelLabel);
    }

    // Depending on the host, iframe.contentWindow is not always
    // defined at this moment. If it is not defined, we use an
    // event listener on the iframe DOM node. If it's defined,
    // we use the chromeEventHandler. We can't use a listener
    // on the DOM node every time because this won't work
    // if the (xul chrome) iframe is loaded in a content docshell.
    if (iframe.contentWindow) {
      let domHelper = new DOMHelpers(iframe.contentWindow);
      domHelper.onceDOMReady(onLoad);
    } else {
      let callback = () => {
        iframe.removeEventListener("DOMContentLoaded", callback);
        onLoad();
      };

      iframe.addEventListener("DOMContentLoaded", callback);
    }

    return deferred.promise;
  },

  /**
   * Mark all in collection as unselected; and id as selected
   * @param {string} collection
   *        DOM collection of items
   * @param {string} id
   *        The Id of the item within the collection to select
   */
  selectSingleNode: function (collection, id) {
    [...collection].forEach(node => {
      if (node.id === id) {
        node.setAttribute("selected", "true");
        node.setAttribute("aria-selected", "true");
      } else {
        node.removeAttribute("selected");
        node.removeAttribute("aria-selected");
      }
    });
  },

  /**
   * Switch to the tool with the given id
   *
   * @param {string} id
   *        The id of the tool to switch to
   */
  selectTool: function (id) {
    this.emit("before-select", id);

    let tabs = this.doc.querySelectorAll(".devtools-tab");
    this.selectSingleNode(tabs, "toolbox-tab-" + id);

    // If options is selected, the separator between it and the
    // command buttons should be hidden.
    let sep = this.doc.getElementById("toolbox-controls-separator");
    if (id === "options") {
      sep.setAttribute("invisible", "true");
    } else {
      sep.removeAttribute("invisible");
    }

    if (this.currentToolId == id) {
      // re-focus tool to get key events again
      this.focusTool(id);

      // Return the existing panel in order to have a consistent return value.
      return promise.resolve(this._toolPanels.get(id));
    }

    if (!this.isReady) {
      throw new Error("Can't select tool, wait for toolbox 'ready' event");
    }

    let tab = this.doc.getElementById("toolbox-tab-" + id);

    if (tab) {
      if (this.currentToolId) {
        this._telemetry.toolClosed(this.currentToolId);
      }
      this._telemetry.toolOpened(id);
    } else {
      throw new Error("No tool found");
    }

    let tabstrip = this.doc.getElementById("toolbox-tabs");

    // select the right tab, making 0th index the default tab if right tab not
    // found.
    tabstrip.selectedItem = tab || tabstrip.childNodes[0];

    // and select the right iframe
    let toolboxPanels = this.doc.querySelectorAll(".toolbox-panel");
    this.selectSingleNode(toolboxPanels, "toolbox-panel-" + id);

    this.lastUsedToolId = this.currentToolId;
    this.currentToolId = id;
    this._refreshConsoleDisplay();
    if (id != "options") {
      Services.prefs.setCharPref(this._prefs.LAST_TOOL, id);
    }

    return this.loadTool(id).then(panel => {
      // focus the tool's frame to start receiving key events
      this.focusTool(id);

      this.emit("select", id);
      this.emit(id + "-selected", panel);
      return panel;
    });
  },

  /**
   * Focus a tool's panel by id
   * @param  {string} id
   *         The id of tool to focus
   */
  focusTool: function (id, state = true) {
    let iframe = this.doc.getElementById("toolbox-panel-iframe-" + id);

    if (state) {
      iframe.focus();
    } else {
      iframe.blur();
    }
  },

  /**
   * Focus split console's input line
   */
  focusConsoleInput: function () {
    let consolePanel = this.getPanel("webconsole");
    if (consolePanel) {
      consolePanel.focusInput();
    }
  },

  /**
   * If the console is split and we are focusing an element outside
   * of the console, then store the newly focused element, so that
   * it can be restored once the split console closes.
   */
  _onFocus: function ({originalTarget}) {
    // Ignore any non element nodes, or any elements contained
    // within the webconsole frame.
    let webconsoleURL = gDevTools.getToolDefinition("webconsole").url;
    if (originalTarget.nodeType !== 1 ||
        originalTarget.baseURI === webconsoleURL) {
      return;
    }

    this._lastFocusedElement = originalTarget;
  },

  /**
   * Opens the split console.
   *
   * @returns {Promise} a promise that resolves once the tool has been
   *          loaded and focused.
   */
  openSplitConsole: function () {
    this._splitConsole = true;
    Services.prefs.setBoolPref(SPLITCONSOLE_ENABLED_PREF, true);
    this._refreshConsoleDisplay();
    this.emit("split-console");

    return this.loadTool("webconsole").then(() => {
      this.focusConsoleInput();
    });
  },

  /**
   * Closes the split console.
   *
   * @returns {Promise} a promise that resolves once the tool has been
   *          closed.
   */
  closeSplitConsole: function () {
    this._splitConsole = false;
    Services.prefs.setBoolPref(SPLITCONSOLE_ENABLED_PREF, false);
    this._refreshConsoleDisplay();
    this.emit("split-console");

    if (this._lastFocusedElement) {
      this._lastFocusedElement.focus();
    }
    return promise.resolve();
  },

  /**
   * Toggles the split state of the webconsole.  If the webconsole panel
   * is already selected then this command is ignored.
   *
   * @returns {Promise} a promise that resolves once the tool has been
   *          opened or closed.
   */
  toggleSplitConsole: function () {
    if (this.currentToolId !== "webconsole") {
      return this.splitConsole ?
             this.closeSplitConsole() :
             this.openSplitConsole();
    }

    return promise.resolve();
  },

  /**
   * Tells the target tab to reload.
   */
  reloadTarget: function (force) {
    this.target.activeTab.reload({ force: force });
  },

  /**
   * Loads the tool next to the currently selected tool.
   */
  selectNextTool: function () {
    let tools = this.doc.querySelectorAll(".devtools-tab");
    let selected = this.doc.querySelector(".devtools-tab[selected]");
    let nextIndex = [...tools].indexOf(selected) + 1;
    let next = tools[nextIndex] || tools[0];
    let tool = next.getAttribute("toolid");
    return this.selectTool(tool);
  },

  /**
   * Loads the tool just left to the currently selected tool.
   */
  selectPreviousTool: function () {
    let tools = this.doc.querySelectorAll(".devtools-tab");
    let selected = this.doc.querySelector(".devtools-tab[selected]");
    let prevIndex = [...tools].indexOf(selected) - 1;
    let prev = tools[prevIndex] || tools[tools.length - 1];
    let tool = prev.getAttribute("toolid");
    return this.selectTool(tool);
  },

  /**
   * Highlights the tool's tab if it is not the currently selected tool.
   *
   * @param {string} id
   *        The id of the tool to highlight
   */
  highlightTool: function (id) {
    let tab = this.doc.getElementById("toolbox-tab-" + id);
    tab && tab.setAttribute("highlighted", "true");
  },

  /**
   * De-highlights the tool's tab.
   *
   * @param {string} id
   *        The id of the tool to unhighlight
   */
  unhighlightTool: function (id) {
    let tab = this.doc.getElementById("toolbox-tab-" + id);
    tab && tab.removeAttribute("highlighted");
  },

  /**
   * Raise the toolbox host.
   */
  raise: function () {
    this._host.raise();
  },

  /**
   * Refresh the host's title.
   */
  _refreshHostTitle: function () {
    let title;
    if (this.target.name && this.target.name != this.target.url) {
      title = toolboxStrings("toolbox.titleTemplate2",
                             this.target.name, this.target.url);
    } else {
      title = toolboxStrings("toolbox.titleTemplate1", this.target.url);
    }
    this._host.setTitle(title);
  },

  // Returns an instance of the preference actor
  get _preferenceFront() {
    return this.target.root.then(rootForm => {
      return new getPreferenceFront(this.target.client, rootForm);
    });
  },

  _toggleAutohide: Task.async(function* () {
    let prefName = "ui.popup.disable_autohide";
    let front = yield this._preferenceFront;
    let current = yield front.getBoolPref(prefName);
    yield front.setBoolPref(prefName, !current);

    this._updateNoautohideButton();
  }),

  _updateNoautohideButton: Task.async(function* () {
    let menu = this.doc.getElementById("command-button-noautohide");
    if (menu.getAttribute("hidden") === "true") {
      return;
    }
    if (!this.target.root) {
      return;
    }
    let prefName = "ui.popup.disable_autohide";
    let front = yield this._preferenceFront;
    let current = yield front.getBoolPref(prefName);
    if (current) {
      menu.setAttribute("checked", "true");
    } else {
      menu.removeAttribute("checked");
    }
  }),

  _listFrames: function (event) {
    if (!this._target.activeTab || !this._target.activeTab.traits.frames) {
      // We are not targetting a regular TabActor
      // it can be either an addon or browser toolbox actor
      return promise.resolve();
    }
    let packet = {
      to: this._target.form.actor,
      type: "listFrames"
    };
    return this._target.client.request(packet, resp => {
      this._updateFrames(null, { frames: resp.frames });
    });
  },

  /**
   * Show a drop down menu that allows the user to switch frames.
   */
  showFramesMenu: function (event) {
    let menu = new Menu();
    let target = event.target;

    // Generate list of menu items from the list of frames.
    this.frameMap.forEach(frame => {
      // A frame is checked if it's the selected one.
      let checked = frame.id == this.selectedFrameId;

      // Create menu item.
      menu.append(new MenuItem({
        label: frame.url,
        type: "radio",
        checked,
        click: () => {
          this.onSelectFrame(frame.id);
        }
      }));
    });

    menu.once("open").then(() => {
      target.setAttribute("open", "true");
    });

    menu.once("close").then(() => {
      target.removeAttribute("open");
    });

    // Show a drop down menu with frames.
    // XXX Missing menu API for specifying target (anchor)
    // and relative position to it. See also:
    // https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XUL/Method/openPopup
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1274551
    let rect = target.getBoundingClientRect();
    let screenX = target.ownerDocument.defaultView.mozInnerScreenX;
    let screenY = target.ownerDocument.defaultView.mozInnerScreenY;
    menu.popup(rect.left + screenX, rect.bottom + screenY, this);

    return menu;
  },

  /**
   * Select a frame by sending 'switchToFrame' packet to the backend.
   */
  onSelectFrame: function (frameId) {
    // Send packet to the backend to select specified frame and
    // wait for 'frameUpdate' event packet to update the UI.
    let packet = {
      to: this._target.form.actor,
      type: "switchToFrame",
      windowId: frameId
    };
    this._target.client.request(packet);
  },

  /**
   * A handler for 'frameUpdate' packets received from the backend.
   * Following properties might be set on the packet:
   *
   * destroyAll {Boolean}: All frames have been destroyed.
   * selected {Number}: A frame has been selected
   * frames {Array}: list of frames. Every frame can have:
   *                 id {Number}: frame ID
   *                 url {String}: frame URL
   *                 title {String}: frame title
   *                 destroy {Boolean}: Set to true if destroyed
   *                 parentID {Number}: ID of the parent frame (not set
   *                                    for top level window)
   */
  _updateFrames: function (event, data) {
    if (!Services.prefs.getBoolPref("devtools.command-button-frames.enabled")) {
      return;
    }

    // We may receive this event before the toolbox is ready.
    if (!this.isReady) {
      return;
    }

    // Store (synchronize) data about all existing frames on the backend
    if (data.destroyAll) {
      this.frameMap.clear();
      this.selectedFrameId = null;
    } else if (data.selected) {
      this.selectedFrameId = data.selected;
    } else if (data.frames) {
      data.frames.forEach(frame => {
        if (frame.destroy) {
          this.frameMap.delete(frame.id);

          // Reset the currently selected frame if it's destroyed.
          if (this.selectedFrameId == frame.id) {
            this.selectedFrameId = null;
          }
        } else {
          this.frameMap.set(frame.id, frame);
        }
      });
    }

    // If there is no selected frame select the first top level
    // frame by default. Note that there might be more top level
    // frames in case of the BrowserToolbox.
    if (!this.selectedFrameId) {
      let frames = [...this.frameMap.values()];
      let topFrames = frames.filter(frame => !frame.parentID);
      this.selectedFrameId = topFrames.length ? topFrames[0].id : null;
    }

    // Check out whether top frame is currently selected.
    // Note that only child frame has parentID.
    let frame = this.frameMap.get(this.selectedFrameId);
    let topFrameSelected = frame ? !frame.parentID : false;
    let button = this.doc.getElementById("command-button-frames");
    button.removeAttribute("checked");

    // If non-top level frame is selected the toolbar button is
    // marked as 'checked' indicating that a child frame is active.
    if (!topFrameSelected && this.selectedFrameId) {
      button.setAttribute("checked", "true");
    }
  },

  /**
   * Create a host object based on the given host type.
   *
   * Warning: some hosts require that the toolbox target provides a reference to
   * the attached tab. Not all Targets have a tab property - make sure you
   * correctly mix and match hosts and targets.
   *
   * @param {string} hostType
   *        The host type of the new host object
   *
   * @return {Host} host
   *        The created host object
   */
  _createHost: function (hostType, options) {
    if (!Hosts[hostType]) {
      throw new Error("Unknown hostType: " + hostType);
    }

    // clean up the toolbox if its window is closed
    let newHost = new Hosts[hostType](this.target.tab, options);
    newHost.on("window-closed", this.destroy);
    return newHost;
  },

  /**
   * Switch to the last used host for the toolbox UI.
   * This is determined by the devtools.toolbox.previousHost pref.
   */
  switchToPreviousHost: function () {
    let hostType = Services.prefs.getCharPref(this._prefs.PREVIOUS_HOST);

    // Handle the case where the previous host happens to match the current
    // host. If so, switch to bottom if it's not already used, and side if not.
    if (hostType === this._host.type) {
      if (hostType === Toolbox.HostType.BOTTOM) {
        hostType = Toolbox.HostType.SIDE;
      } else {
        hostType = Toolbox.HostType.BOTTOM;
      }
    }

    return this.switchHost(hostType);
  },

  /**
   * Switch to a new host for the toolbox UI. E.g. bottom, sidebar, window,
   * and focus the window when done.
   *
   * @param {string} hostType
   *        The host type of the new host object
   */
  switchHost: function (hostType) {
    if (hostType == this._host.type || !this._target.isLocalTab) {
      return null;
    }

    this.emit("host-will-change", hostType);

    // If we call swapFrameLoaders() when a tool if focused it leaves the
    // browser in a state where it thinks that the tool is focused but in
    // reality the content area is focused. Blurring the tool before calling
    // swapFrameLoaders() works around this issue.
    this.focusTool(this.currentToolId, false);

    let newHost = this._createHost(hostType);
    return newHost.create().then(iframe => {
      // change toolbox document's parent to the new host
      iframe.QueryInterface(Ci.nsIFrameLoaderOwner);
      iframe.swapFrameLoaders(this.frame);

      this._host.off("window-closed", this.destroy);
      this.destroyHost();

      let prevHostType = this._host.type;
      this._host = newHost;

      if (this.hostType != Toolbox.HostType.CUSTOM) {
        Services.prefs.setCharPref(this._prefs.LAST_HOST, this._host.type);
        Services.prefs.setCharPref(this._prefs.PREVIOUS_HOST, prevHostType);
      }

      this._buildDockButtons();
      this._addKeysToWindow();

      // Focus the tool to make sure keyboard shortcuts work straight away.
      this.focusTool(this.currentToolId, true);

      this.emit("host-changed");
    });
  },

  /**
   * Return if the tool is available as a tab (i.e. if it's checked
   * in the options panel). This is different from Toolbox.getPanel -
   * a tool could be registered but not yet opened in which case
   * isToolRegistered would return true but getPanel would return false.
   */
  isToolRegistered: function (toolId) {
    return gDevTools.getToolDefinitionMap().has(toolId);
  },

  /**
   * Handler for the tool-registered event.
   * @param  {string} event
   *         Name of the event ("tool-registered")
   * @param  {string} toolId
   *         Id of the tool that was registered
   */
  _toolRegistered: function (event, toolId) {
    let tool = gDevTools.getToolDefinition(toolId);
    this._buildTabForTool(tool);
    // Emit the event so tools can listen to it from the toolbox level
    // instead of gDevTools
    this.emit("tool-registered", toolId);
  },

  /**
   * Handler for the tool-unregistered event.
   * @param  {string} event
   *         Name of the event ("tool-unregistered")
   * @param  {string|object} toolId
   *         Definition or id of the tool that was unregistered. Passing the
   *         tool id should be avoided as it is a temporary measure.
   */
  _toolUnregistered: function (event, toolId) {
    if (typeof toolId != "string") {
      toolId = toolId.id;
    }

    if (this._toolPanels.has(toolId)) {
      let instance = this._toolPanels.get(toolId);
      instance.destroy();
      this._toolPanels.delete(toolId);
    }

    let radio = this.doc.getElementById("toolbox-tab-" + toolId);
    let panel = this.doc.getElementById("toolbox-panel-" + toolId);

    if (radio) {
      if (this.currentToolId == toolId) {
        let nextToolName = null;
        if (radio.nextSibling) {
          nextToolName = radio.nextSibling.getAttribute("toolid");
        }
        if (radio.previousSibling) {
          nextToolName = radio.previousSibling.getAttribute("toolid");
        }
        if (nextToolName) {
          this.selectTool(nextToolName);
        }
      }
      radio.parentNode.removeChild(radio);
    }

    if (panel) {
      panel.parentNode.removeChild(panel);
    }

    if (this.hostType == Toolbox.HostType.WINDOW) {
      let doc = this.win.parent.document;
      let key = doc.getElementById("key_" + toolId);
      if (key) {
        key.parentNode.removeChild(key);
      }
    }
    // Emit the event so tools can listen to it from the toolbox level
    // instead of gDevTools
    this.emit("tool-unregistered", toolId);
  },

  /**
   * Initialize the inspector/walker/selection/highlighter fronts.
   * Returns a promise that resolves when the fronts are initialized
   */
  initInspector: function () {
    if (!this._initInspector) {
      this._initInspector = Task.spawn(function* () {
        this._inspector = InspectorFront(this._target.client, this._target.form);
        this._walker = yield this._inspector.getWalker(
          {showAllAnonymousContent: Services.prefs.getBoolPref("devtools.inspector.showAllAnonymousContent")}
        );
        this._selection = new Selection(this._walker);

        if (this.highlighterUtils.isRemoteHighlightable()) {
          this.walker.on("highlighter-ready", this._highlighterReady);
          this.walker.on("highlighter-hide", this._highlighterHidden);

          let autohide = !flags.testing;
          this._highlighter = yield this._inspector.getHighlighter(autohide);
        }
      }.bind(this));
    }
    return this._initInspector;
  },

  /**
   * Destroy the inspector/walker/selection fronts
   * Returns a promise that resolves when the fronts are destroyed
   */
  destroyInspector: function () {
    if (this._destroyingInspector) {
      return this._destroyingInspector;
    }

    return this._destroyingInspector = Task.spawn(function* () {
      if (!this._inspector) {
        return;
      }

      // Releasing the walker (if it has been created)
      // This can fail, but in any case, we want to continue destroying the
      // inspector/highlighter/selection
      // FF42+: Inspector actor starts managing Walker actor and auto destroy it.
      if (this._walker && !this.walker.traits.autoReleased) {
        try {
          yield this._walker.release();
        } catch (e) {}
      }

      yield this.highlighterUtils.stopPicker();
      yield this._inspector.destroy();
      if (this._highlighter) {
        // Note that if the toolbox is closed, this will work fine, but will fail
        // in case the browser is closed and will trigger a noSuchActor message.
        // We ignore the promise that |_hideBoxModel| returns, since we should still
        // proceed with the rest of destruction if it fails.
        // FF42+ now does the cleanup from the actor.
        if (!this.highlighter.traits.autoHideOnDestroy) {
          this.highlighterUtils.unhighlight();
        }
        yield this._highlighter.destroy();
      }
      if (this._selection) {
        this._selection.destroy();
      }

      if (this.walker) {
        this.walker.off("highlighter-ready", this._highlighterReady);
        this.walker.off("highlighter-hide", this._highlighterHidden);
      }

      this._inspector = null;
      this._highlighter = null;
      this._selection = null;
      this._walker = null;
    }.bind(this));
  },

  /**
   * Get the toolbox's notification component
   *
   * @return The notification box component.
   */
  getNotificationBox: function () {
    return this.notificationBox;
  },

  /**
   * Destroy the current host, and remove event listeners from its frame.
   *
   * @return {promise} to be resolved when the host is destroyed.
   */
  destroyHost: function () {
    // The host iframe's contentDocument may already be gone.
    if (this.doc) {
      this.doc.removeEventListener("keypress",
        this._splitConsoleOnKeypress, false);
      this.doc.removeEventListener("focus", this._onFocus, true);
    }
    return this._host.destroy();
  },

  /**
   * Remove all UI elements, detach from target and clear up
   */
  destroy: function () {
    // If several things call destroy then we give them all the same
    // destruction promise so we're sure to destroy only once
    if (this._destroyer) {
      return this._destroyer;
    }

    this.emit("destroy");

    this._target.off("navigate", this._refreshHostTitle);
    this._target.off("frame-update", this._updateFrames);
    this.off("select", this._refreshHostTitle);
    this.off("host-changed", this._refreshHostTitle);
    this.off("ready", this._showDevEditionPromo);

    gDevTools.off("tool-registered", this._toolRegistered);
    gDevTools.off("tool-unregistered", this._toolUnregistered);

    gDevTools.off("pref-changed", this._prefChanged);

    this._lastFocusedElement = null;
    if (this._sourceMapService) {
      this._sourceMapService.destroy();
      this._sourceMapService = null;
    }

    if (this.webconsolePanel) {
      this._saveSplitConsoleHeight();
      this.webconsolePanel.removeEventListener("resize",
        this._saveSplitConsoleHeight);
    }
    this.closeButton.removeEventListener("click", this.destroy, true);
    this.textboxContextMenuPopup.removeEventListener("popupshowing",
      this._updateTextboxMenuItems, true);
    this.tabbar.removeEventListener("focus", this._onTabbarFocus, true);
    this.tabbar.removeEventListener("click", this._onTabbarFocus, true);
    this.tabbar.removeEventListener("keypress", this._onTabbarArrowKeypress);

    let outstanding = [];
    for (let [id, panel] of this._toolPanels) {
      try {
        gDevTools.emit(id + "-destroy", this, panel);
        this.emit(id + "-destroy", panel);

        outstanding.push(panel.destroy());
      } catch (e) {
        // We don't want to stop here if any panel fail to close.
        console.error("Panel " + id + ":", e);
      }
    }

    this.browserRequire = null;

    // Now that we are closing the toolbox we can re-enable the cache settings
    // and disable the service workers testing settings for the current tab.
    // FF41+ automatically cleans up state in actor on disconnect.
    if (this.target.activeTab && !this.target.activeTab.traits.noTabReconfigureOnClose) {
      this.target.activeTab.reconfigure({
        "cacheDisabled": false,
        "serviceWorkersTestingEnabled": false
      });
    }

    // Destroying the walker and inspector fronts
    outstanding.push(this.destroyInspector().then(() => {
      // Removing buttons
      if (this._pickerButton) {
        this._pickerButton.removeEventListener("click", this._togglePicker, false);
        this._pickerButton = null;
      }
    }));

    // Destroy the profiler connection
    outstanding.push(this.destroyPerformance());

    // Detach the thread
    detachThread(this._threadClient);
    this._threadClient = null;

    // We need to grab a reference to win before this._host is destroyed.
    let win = this.frame.ownerDocument.defaultView;

    if (this._requisition) {
      CommandUtils.destroyRequisition(this._requisition, this.target);
    }
    this._telemetry.toolClosed("toolbox");
    this._telemetry.destroy();

    // Finish all outstanding tasks (which means finish destroying panels and
    // then destroying the host, successfully or not) before destroying the
    // target.
    this._destroyer = settleAll(outstanding)
        .catch(console.error)
        .then(() => this.destroyHost())
        .catch(console.error)
        .then(() => {
          // Targets need to be notified that the toolbox is being torn down.
          // This is done after other destruction tasks since it may tear down
          // fronts and the debugger transport which earlier destroy methods may
          // require to complete.
          if (!this._target) {
            return null;
          }
          let target = this._target;
          this._target = null;
          this.highlighterUtils.release();
          target.off("close", this.destroy);
          return target.destroy();
        }, console.error).then(() => {
          this.emit("destroyed");

          // Free _host after the call to destroyed in order to let a chance
          // to destroyed listeners to still query toolbox attributes
          this._host = null;
          this._toolPanels.clear();

          // Force GC to prevent long GC pauses when running tests and to free up
          // memory in general when the toolbox is closed.
          if (flags.testing) {
            win.QueryInterface(Ci.nsIInterfaceRequestor)
              .getInterface(Ci.nsIDOMWindowUtils)
              .garbageCollect();
          }
        }).then(null, console.error);

    let leakCheckObserver = ({wrappedJSObject: barrier}) => {
      // Make the leak detector wait until this toolbox is properly destroyed.
      barrier.client.addBlocker("DevTools: Wait until toolbox is destroyed",
                                this._destroyer);
    };

    let topic = "shutdown-leaks-before-check";
    Services.obs.addObserver(leakCheckObserver, topic, false);
    this._destroyer.then(() => {
      Services.obs.removeObserver(leakCheckObserver, topic);
    });

    return this._destroyer;
  },

  _highlighterReady: function () {
    this.emit("highlighter-ready");
  },

  _highlighterHidden: function () {
    this.emit("highlighter-hide");
  },

  /**
   * For displaying the promotional Doorhanger on first opening of
   * the developer tools, promoting the Developer Edition.
   */
  _showDevEditionPromo: function () {
    // Do not display in browser toolbox
    if (this.target.chrome) {
      return;
    }
    showDoorhanger({ window: this.win, type: "deveditionpromo" });
  },

  /**
   * Enable / disable necessary textbox menu items using globalOverlay.js.
   */
  _updateTextboxMenuItems: function () {
    let window = this.win;
    ["cmd_undo", "cmd_delete", "cmd_cut",
     "cmd_copy", "cmd_paste", "cmd_selectAll"].forEach(window.goUpdateCommand);
  },

  /**
   * Connects to the SPS profiler when the developer tools are open. This is
   * necessary because of the WebConsole's `profile` and `profileEnd` methods.
   */
  initPerformance: Task.async(function* () {
    // If target does not have profiler actor (addons), do not
    // even register the shared performance connection.
    if (!this.target.hasActor("profiler")) {
      return;
    }

    if (this._performanceFrontConnection) {
      return this._performanceFrontConnection.promise;
    }

    this._performanceFrontConnection = defer();
    this._performance = createPerformanceFront(this._target);
    yield this.performance.connect();

    // Emit an event when connected, but don't wait on startup for this.
    this.emit("profiler-connected");

    this.performance.on("*", this._onPerformanceFrontEvent);
    this._performanceFrontConnection.resolve(this.performance);
    return this._performanceFrontConnection.promise;
  }),

  /**
   * Disconnects the underlying Performance actor. If the connection
   * has not finished initializing, as opening a toolbox does not wait,
   * the performance connection destroy method will wait for it on its own.
   */
  destroyPerformance: Task.async(function* () {
    if (!this.performance) {
      return;
    }
    // If still connecting to performance actor, allow the
    // actor to resolve its connection before attempting to destroy.
    if (this._performanceFrontConnection) {
      yield this._performanceFrontConnection.promise;
    }
    this.performance.off("*", this._onPerformanceFrontEvent);
    yield this.performance.destroy();
    this._performance = null;
  }),

  /**
   * Called when any event comes from the PerformanceFront. If the performance tool is already
   * loaded when the first event comes in, immediately unbind this handler, as this is
   * only used to queue up observed recordings before the performance tool can handle them,
   * which will only occur when `console.profile()` recordings are started before the tool loads.
   */
  _onPerformanceFrontEvent: Task.async(function* (eventName, recording) {
    if (this.getPanel("performance")) {
      this.performance.off("*", this._onPerformanceFrontEvent);
      return;
    }

    let recordings = this._performanceQueuedRecordings = this._performanceQueuedRecordings || [];

    // Before any console recordings, we'll get a `console-profile-start` event
    // warning us that a recording will come later (via `recording-started`), so
    // start to boot up the tool and populate the tool with any other recordings
    // observed during that time.
    if (eventName === "console-profile-start" && !this._performanceToolOpenedViaConsole) {
      this._performanceToolOpenedViaConsole = this.loadTool("performance");
      let panel = yield this._performanceToolOpenedViaConsole;
      yield panel.open();

      panel.panelWin.PerformanceController.populateWithRecordings(recordings);
      this.performance.off("*", this._onPerformanceFrontEvent);
    }

    // Otherwise, if it's a recording-started event, we've already started loading
    // the tool, so just store this recording in our array to be later populated
    // once the tool loads.
    if (eventName === "recording-started") {
      recordings.push(recording);
    }
  }),

  /**
   * Returns gViewSourceUtils for viewing source.
   */
  get gViewSourceUtils() {
    return this.win.gViewSourceUtils;
  },

  /**
   * Opens source in style editor. Falls back to plain "view-source:".
   * @see devtools/client/shared/source-utils.js
   */
  viewSourceInStyleEditor: function (sourceURL, sourceLine) {
    return viewSource.viewSourceInStyleEditor(this, sourceURL, sourceLine);
  },

  /**
   * Opens source in debugger. Falls back to plain "view-source:".
   * @see devtools/client/shared/source-utils.js
   */
  viewSourceInDebugger: function (sourceURL, sourceLine) {
    return viewSource.viewSourceInDebugger(this, sourceURL, sourceLine);
  },

  /**
   * Opens source in scratchpad. Falls back to plain "view-source:".
   * TODO The `sourceURL` for scratchpad instances are like `Scratchpad/1`.
   * If instances are scoped one-per-browser-window, then we should be able
   * to infer the URL from this toolbox, or use the built in scratchpad IN
   * the toolbox.
   *
   * @see devtools/client/shared/source-utils.js
   */
  viewSourceInScratchpad: function (sourceURL, sourceLine) {
    return viewSource.viewSourceInScratchpad(sourceURL, sourceLine);
  },

  /**
   * Opens source in plain "view-source:".
   * @see devtools/client/shared/source-utils.js
   */
  viewSource: function (sourceURL, sourceLine) {
    return viewSource.viewSource(this, sourceURL, sourceLine);
  },
};
