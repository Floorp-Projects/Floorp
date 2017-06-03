/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const SOURCE_MAP_WORKER = "resource://devtools/client/shared/source-map/worker.js";

const MAX_ORDINAL = 99;
const SPLITCONSOLE_ENABLED_PREF = "devtools.toolbox.splitconsoleEnabled";
const SPLITCONSOLE_HEIGHT_PREF = "devtools.toolbox.splitconsoleHeight";
const DISABLE_AUTOHIDE_PREF = "ui.popup.disable_autohide";
const OS_HISTOGRAM = "DEVTOOLS_OS_ENUMERATED_PER_USER";
const OS_IS_64_BITS = "DEVTOOLS_OS_IS_64_BITS_PER_USER";
const HOST_HISTOGRAM = "DEVTOOLS_TOOLBOX_HOST";
const SCREENSIZE_HISTOGRAM = "DEVTOOLS_SCREEN_RESOLUTION_ENUMERATED_PER_USER";
const HTML_NS = "http://www.w3.org/1999/xhtml";

var {Ci, Cu} = require("chrome");
var promise = require("promise");
var defer = require("devtools/shared/defer");
var Services = require("Services");
var {Task} = require("devtools/shared/task");
var {gDevTools} = require("devtools/client/framework/devtools");
var EventEmitter = require("devtools/shared/event-emitter");
var Telemetry = require("devtools/client/shared/telemetry");
var { attachThread, detachThread } = require("./attach-thread");
var Menu = require("devtools/client/framework/menu");
var MenuItem = require("devtools/client/framework/menu-item");
var { DOMHelpers } = require("resource://devtools/client/shared/DOMHelpers.jsm");
const { KeyCodes } = require("devtools/client/shared/keycodes");

const { BrowserLoader } =
  Cu.import("resource://devtools/client/shared/browser-loader.js", {});

const {LocalizationHelper} = require("devtools/shared/l10n");
const L10N = new LocalizationHelper("devtools/client/locales/toolbox.properties");

loader.lazyRequireGetter(this, "getHighlighterUtils",
  "devtools/client/framework/toolbox-highlighter-utils", true);
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
  "devtools/client/shared/key-shortcuts");
loader.lazyRequireGetter(this, "ZoomKeys",
  "devtools/client/shared/zoom-keys");
loader.lazyRequireGetter(this, "settleAll",
  "devtools/shared/ThreadSafeDevToolsUtils", true);
loader.lazyRequireGetter(this, "ToolboxButtons",
  "devtools/client/definitions", true);
loader.lazyRequireGetter(this, "SourceMapURLService",
  "devtools/client/framework/source-map-url-service", true);
loader.lazyRequireGetter(this, "HUDService",
  "devtools/client/webconsole/hudservice");
loader.lazyRequireGetter(this, "viewSource",
  "devtools/client/shared/view-source");

loader.lazyGetter(this, "registerHarOverlay", () => {
  return require("devtools/client/netmonitor/src/har/toolbox-overlay").register;
});

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
 * @param {DOMWindow} contentWindow
 *        The window object of the toolbox document
 * @param {string} frameId
 *        A unique identifier to differentiate toolbox documents from the
 *        chrome codebase when passing DOM messages
 */
function Toolbox(target, selectedTool, hostType, contentWindow, frameId) {
  this._target = target;
  this._win = contentWindow;
  this.frameId = frameId;

  this._toolPanels = new Map();
  this._telemetry = new Telemetry();

  this._initInspector = null;
  this._inspector = null;

  // Map of frames (id => frame-info) and currently selected frame id.
  this.frameMap = new Map();
  this.selectedFrameId = null;

  this._toolRegistered = this._toolRegistered.bind(this);
  this._toolUnregistered = this._toolUnregistered.bind(this);
  this._refreshHostTitle = this._refreshHostTitle.bind(this);
  this._toggleNoAutohide = this._toggleNoAutohide.bind(this);
  this.showFramesMenu = this.showFramesMenu.bind(this);
  this._updateFrames = this._updateFrames.bind(this);
  this._splitConsoleOnKeypress = this._splitConsoleOnKeypress.bind(this);
  this.destroy = this.destroy.bind(this);
  this.highlighterUtils = getHighlighterUtils(this);
  this._highlighterReady = this._highlighterReady.bind(this);
  this._highlighterHidden = this._highlighterHidden.bind(this);
  this._applyCacheSettings = this._applyCacheSettings.bind(this);
  this._applyServiceWorkersTestingSettings =
    this._applyServiceWorkersTestingSettings.bind(this);
  this._saveSplitConsoleHeight = this._saveSplitConsoleHeight.bind(this);
  this._onFocus = this._onFocus.bind(this);
  this._onBrowserMessage = this._onBrowserMessage.bind(this);
  this._showDevEditionPromo = this._showDevEditionPromo.bind(this);
  this._updateTextBoxMenuItems = this._updateTextBoxMenuItems.bind(this);
  this._onBottomHostMinimized = this._onBottomHostMinimized.bind(this);
  this._onBottomHostMaximized = this._onBottomHostMaximized.bind(this);
  this._onToolSelectWhileMinimized = this._onToolSelectWhileMinimized.bind(this);
  this._onPerformanceFrontEvent = this._onPerformanceFrontEvent.bind(this);
  this._onBottomHostWillChange = this._onBottomHostWillChange.bind(this);
  this._toggleMinimizeMode = this._toggleMinimizeMode.bind(this);
  this._onToolbarFocus = this._onToolbarFocus.bind(this);
  this._onToolbarArrowKeypress = this._onToolbarArrowKeypress.bind(this);
  this._onPickerClick = this._onPickerClick.bind(this);
  this._onPickerKeypress = this._onPickerKeypress.bind(this);
  this._onPickerStarted = this._onPickerStarted.bind(this);
  this._onPickerStopped = this._onPickerStopped.bind(this);
  this.selectTool = this.selectTool.bind(this);

  this._target.on("close", this.destroy);

  if (!selectedTool) {
    selectedTool = Services.prefs.getCharPref(this._prefs.LAST_TOOL);
  }
  this._defaultToolId = selectedTool;

  this._hostType = hostType;

  this._isOpenDeferred = defer();
  this.isOpen = this._isOpenDeferred.promise;

  EventEmitter.decorate(this);

  this._target.on("navigate", this._refreshHostTitle);
  this._target.on("frame-update", this._updateFrames);

  this.on("host-changed", this._refreshHostTitle);
  this.on("select", this._refreshHostTitle);

  this.on("ready", this._showDevEditionPromo);

  gDevTools.on("tool-registered", this._toolRegistered);
  gDevTools.on("tool-unregistered", this._toolUnregistered);

  this.on("picker-started", this._onPickerStarted);
  this.on("picker-stopped", this._onPickerStopped);
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
    LAST_TOOL: "devtools.toolbox.selectedTool",
    SIDE_ENABLED: "devtools.toolbox.sideEnabled",
  },

  get currentToolId() {
    return this._currentToolId;
  },

  set currentToolId(id) {
    this._currentToolId = id;
    this.component.setCurrentToolId(id);
  },

  get panelDefinitions() {
    return this._panelDefinitions;
  },

  set panelDefinitions(definitions) {
    this._panelDefinitions = definitions;
    this._combineAndSortPanelDefinitions();
  },

  get visibleAdditionalTools() {
    if (!this._visibleAdditionalTools) {
      this._visibleAdditionalTools = [];
    }

    return this._visibleAdditionalTools;
  },

  set visibleAdditionalTools(tools) {
    this._visibleAdditionalTools = tools;
    this._combineAndSortPanelDefinitions();
  },

  /**
   * Combines the built-in panel definitions and the additional tool definitions that
   * can be set by add-ons.
   */
  _combineAndSortPanelDefinitions() {
    const definitions = [...this._panelDefinitions, ...this.getVisibleAdditionalTools()];
    definitions.sort(definition => {
      return -1 * (definition.ordinal == undefined || definition.ordinal < 0
        ? MAX_ORDINAL
        : definition.ordinal
      );
    });
    this.component.setPanelDefinitions(definitions);
  },

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
      this.on(id + "-ready", (e, initializedPanel) => {
        deferred.resolve(initializedPanel);
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
    return this._hostType;
  },

  /**
   * Shortcut to the window containing the toolbox UI
   */
  get win() {
    return this._win;
  },

  /**
   * Shortcut to the document containing the toolbox UI
   */
  get doc() {
    return this.win.document;
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
      this.browserRequire = BrowserLoader({
        window: this.doc.defaultView,
        useOnlyShared: true
      }).require;

      if (this.win.location.href.startsWith(this._URL)) {
        // Update the URL so that onceDOMReady watch for the right url.
        this._URL = this.win.location.href;
      }

      let domReady = defer();
      let domHelper = new DOMHelpers(this.win);
      domHelper.onceDOMReady(() => {
        domReady.resolve();
      }, this._URL);

      // Optimization: fire up a few other things before waiting on
      // the iframe being ready (makes startup faster)

      // Load the toolbox-level actor fronts and utilities now
      yield this._target.makeRemote();

      // Start tracking network activity on toolbox open for targets such as tabs.
      // (Workers and potentially others don't manage the console client in the target.)
      if (this._target.activeConsole) {
        yield this._target.activeConsole.startListeners([
          "NetworkActivity",
        ]);
      }

      // Attach the thread
      this._threadClient = yield attachThread(this);
      yield domReady.promise;

      this.isReady = true;
      let framesPromise = this._listFrames();

      Services.prefs.addObserver("devtools.cache.disabled", this._applyCacheSettings);
      Services.prefs.addObserver("devtools.serviceWorkers.testing.enabled",
                                 this._applyServiceWorkersTestingSettings);

      this.textBoxContextMenuPopup =
        this.doc.getElementById("toolbox-textbox-context-popup");
      this.textBoxContextMenuPopup.addEventListener("popupshowing",
        this._updateTextBoxMenuItems, true);

      this.shortcuts = new KeyShortcuts({
        window: this.doc.defaultView
      });
      // Get the DOM element to mount the ToolboxController to.
      this._componentMount = this.doc.getElementById("toolbox-toolbar-mount");

      this._mountReactComponent();
      this._buildDockButtons();
      this._buildOptions();
      this._buildTabs();
      this._applyCacheSettings();
      this._applyServiceWorkersTestingSettings();
      this._addKeysToWindow();
      this._addReloadKeys();
      this._addHostListeners();
      this._registerOverlays();
      if (!this._hostOptions || this._hostOptions.zoom === true) {
        ZoomKeys.register(this.win);
      }

      this._componentMount.addEventListener("keypress", this._onToolbarArrowKeypress);

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

      // Start rendering the toolbox toolbar before selecting the tool, as the tools
      // can take a few hundred milliseconds seconds to start up.
      this.component.setCanRender();

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
      this._isOpenDeferred.resolve();
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

  get ReactRedux() {
    return this.browserRequire("devtools/client/shared/vendor/react-redux");
  },

  get ToolboxController() {
    return this.browserRequire("devtools/client/framework/components/toolbox-controller");
  },

  /**
   * A common access point for the client-side mapping service for source maps that
   * any panel can use.  This is a "low-level" API that connects to
   * the source map worker.
   */
  get sourceMapService() {
    if (!Services.prefs.getBoolPref("devtools.source-map.client-service.enabled")) {
      return null;
    }
    if (this._sourceMapService) {
      return this._sourceMapService;
    }
    // Uses browser loader to access the `Worker` global.
    this._sourceMapService =
      this.browserRequire("devtools/client/shared/source-map/index");
    this._sourceMapService.startSourceMapWorker(SOURCE_MAP_WORKER);
    return this._sourceMapService;
  },

  /**
   * Clients wishing to use source maps but that want the toolbox to
   * track the source actor mapping can use this source map service.
   * This is a higher-level service than the one returned by
   * |sourceMapService|, in that it automatically tracks source actor
   * IDs.
   */
  get sourceMapURLService() {
    if (this._sourceMapURLService) {
      return this._sourceMapURLService;
    }
    let sourceMaps = this.sourceMapService;
    if (!sourceMaps) {
      return null;
    }
    this._sourceMapURLService = new SourceMapURLService(this._target, this.threadClient,
                                                        sourceMaps);
    return this._sourceMapURLService;
  },

  // Return HostType id for telemetry
  _getTelemetryHostId: function () {
    switch (this.hostType) {
      case Toolbox.HostType.BOTTOM: return 0;
      case Toolbox.HostType.SIDE: return 1;
      case Toolbox.HostType.WINDOW: return 2;
      case Toolbox.HostType.CUSTOM: return 3;
      default: return 9;
    }
  },

  _pingTelemetry: function () {
    this._telemetry.toolOpened("toolbox");

    this._telemetry.logOncePerBrowserVersion(OS_HISTOGRAM, system.getOSCPU());
    this._telemetry.logOncePerBrowserVersion(OS_IS_64_BITS,
                                             Services.appinfo.is64Bit ? 1 : 0);
    this._telemetry.logOncePerBrowserVersion(SCREENSIZE_HISTOGRAM,
                                             system.getScreenDimensions());
    this._telemetry.log(HOST_HISTOGRAM, this._getTelemetryHostId());
  },

  /**
   * Create a simple object to store the state of a toolbox button. The checked state of
   * a button can be updated arbitrarily outside of the scope of the toolbar and its
   * controllers. In order to simplify this interaction this object emits an
   * "updatechecked" event any time the isChecked value is updated, allowing any consuming
   * components to listen and respond to updates.
   *
   * @param {Object} options:
   *
   * @property {String} id - The id of the button or command.
   * @property {String} className - An optional additional className for the button.
   * @property {String} description - The value that will display as a tooltip and in
   *                    the options panel for enabling/disabling.
   * @property {Function} onClick - The function to run when the button is activated by
   *                      click or keyboard shortcut. First argument will be the 'click'
   *                      event, and second argument is the toolbox instance.
   * @property {Boolean} isInStartContainer - Buttons can either be placed at the start
   *                     of the toolbar, or at the end.
   * @property {Function} setup - Function run immediately to listen for events changing
   *                      whenever the button is checked or unchecked. The toolbox object
   *                      is passed as first argument and a callback is passed as second
   *                       argument, to be called whenever the checked state changes.
   * @property {Function} teardown - Function run on toolbox close to let a chance to
   *                      unregister listeners set when `setup` was called and avoid
   *                      memory leaks. The same arguments than `setup` function are
   *                      passed to `teardown`.
   * @property {Function} isTargetSupported - Function to automatically enable/disable
   *                      the button based on the target. If the target don't support
   *                      the button feature, this method should return false.
   * @property {Function} isChecked - Optional function called to known if the button
   *                      is toggled or not. The function should return true when
   *                      the button should be displayed as toggled on.
   */
  _createButtonState: function (options) {
    let isCheckedValue = false;
    const { id, className, description, onClick, isInStartContainer, setup, teardown,
            isTargetSupported, isChecked } = options;
    const toolbox = this;
    const button = {
      id,
      className,
      description,
      onClick(event) {
        if (typeof onClick == "function") {
          onClick(event, toolbox);
        }
      },
      isTargetSupported,
      get isChecked() {
        if (typeof isChecked == "function") {
          return isChecked(toolbox);
        }
        return isCheckedValue;
      },
      set isChecked(value) {
        // Note that if options.isChecked is given, this is ignored
        isCheckedValue = value;
        this.emit("updatechecked");
      },
      // The preference for having this button visible.
      visibilityswitch: `devtools.${id}.enabled`,
      // The toolbar has a container at the start and end of the toolbar for
      // holding buttons. By default the buttons are placed in the end container.
      isInStartContainer: !!isInStartContainer
    };
    if (typeof setup == "function") {
      let onChange = () => {
        button.emit("updatechecked");
      };
      setup(this, onChange);
      // Save a reference to the cleanup method that will unregister the onChange
      // callback. Immediately bind the function argument so that we don't have to
      // also save a reference to them.
      button.teardown = teardown.bind(options, this, onChange);
    }
    button.isVisible = this._commandIsVisible(button);

    EventEmitter.decorate(button);

    return button;
  },

  _buildOptions: function () {
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
    this.shortcuts.on(L10N.getStr("toolbox.options.key"), selectOptions);
    this.shortcuts.on(L10N.getStr("toolbox.help.key"), selectOptions);
  },

  _splitConsoleOnKeypress: function (e) {
    if (e.keyCode === KeyCodes.DOM_VK_ESCAPE) {
      this.toggleSplitConsole();
      // If the debugger is paused, don't let the ESC key stop any pending
      // navigation.
      if (this._threadClient.state == "paused") {
        e.preventDefault();
      }
    }
  },

  /**
   * Add a shortcut key that should work when a split console
   * has focus to the toolbox.
   *
   * @param {String} key
   *        The electron key shortcut.
   * @param {Function} handler
   *        The callback that should be called when the provided key shortcut is pressed.
   * @param {String} whichTool
   *        The tool the key belongs to. The corresponding handler will only be triggered
   *        if this tool is active.
   */
  useKeyWithSplitConsole: function (key, handler, whichTool) {
    this.shortcuts.on(key, (name, event) => {
      if (this.currentToolId === whichTool && this.isSplitConsoleFocused()) {
        handler();
        event.preventDefault();
      }
    });
  },

  _addReloadKeys: function () {
    [
      ["reload", false],
      ["reload2", false],
      ["forceReload", true],
      ["forceReload2", true]
    ].forEach(([id, force]) => {
      let key = L10N.getStr("toolbox." + id + ".key");
      this.shortcuts.on(key, (name, event) => {
        this.reloadTarget(force);

        // Prevent Firefox shortcuts from reloading the page
        event.preventDefault();
      });
    });
  },

  _addHostListeners: function () {
    this.shortcuts.on(L10N.getStr("toolbox.nextTool.key"),
                 (name, event) => {
                   this.selectNextTool();
                   event.preventDefault();
                 });
    this.shortcuts.on(L10N.getStr("toolbox.previousTool.key"),
                 (name, event) => {
                   this.selectPreviousTool();
                   event.preventDefault();
                 });
    this.shortcuts.on(L10N.getStr("toolbox.minimize.key"),
                 (name, event) => {
                   this._toggleMinimizeMode();
                   event.preventDefault();
                 });
    this.shortcuts.on(L10N.getStr("toolbox.toggleHost.key"),
                 (name, event) => {
                   this.switchToPreviousHost();
                   event.preventDefault();
                 });

    this.doc.addEventListener("keypress", this._splitConsoleOnKeypress);
    this.doc.addEventListener("focus", this._onFocus, true);
    this.win.addEventListener("unload", this.destroy);
    this.win.addEventListener("message", this._onBrowserMessage, true);
  },

  _removeHostListeners: function () {
    // The host iframe's contentDocument may already be gone.
    if (this.doc) {
      this.doc.removeEventListener("keypress", this._splitConsoleOnKeypress);
      this.doc.removeEventListener("focus", this._onFocus, true);
      this.win.removeEventListener("unload", this.destroy);
      this.win.removeEventListener("message", this._onBrowserMessage, true);
    }
  },

  // Called whenever the chrome send a message
  _onBrowserMessage: function (event) {
    if (!event.data) {
      return;
    }
    switch (event.data.name) {
      case "switched-host":
        this._onSwitchedHost(event.data);
        break;
      case "host-minimized":
        if (this.hostType == Toolbox.HostType.BOTTOM) {
          this._onBottomHostMinimized();
        }
        break;
      case "host-maximized":
        if (this.hostType == Toolbox.HostType.BOTTOM) {
          this._onBottomHostMaximized();
        }
        break;
    }
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

      key.setAttribute("key", L10N.getStr("browserConsoleCmd.commandkey"));
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
    if (!this._target.isLocalTab) {
      this.component.setDockButtonsEnabled(false);
      return;
    }

    // Bottom-type host can be minimized, add a button for this.
    if (this.hostType == Toolbox.HostType.BOTTOM) {
      this.component.setCanMinimize(true);

      // Maximize again when a tool gets selected.
      this.on("before-select", this._onToolSelectWhileMinimized);
      // Maximize and stop listening before the host type changes.
      this.once("host-will-change", this._onBottomHostWillChange);
    }

    this.component.setDockButtonsEnabled(true);
    this.component.setCanCloseToolbox(this.hostType !== Toolbox.HostType.WINDOW);

    let sideEnabled = Services.prefs.getBoolPref(this._prefs.SIDE_ENABLED);

    let hostTypes = [];
    for (let type in Toolbox.HostType) {
      let position = Toolbox.HostType[type];
      if (position == this.hostType ||
          position == Toolbox.HostType.CUSTOM ||
          (!sideEnabled && position == Toolbox.HostType.SIDE)) {
        continue;
      }

      hostTypes.push({
        position,
        switchHost: this.switchHost.bind(this, position)
      });
    }

    this.component.setHostTypes(hostTypes);
  },

  _onBottomHostMinimized: function () {
    this.component.setMinimizeState("minimized");
  },

  _onBottomHostMaximized: function () {
    this.component.setMinimizeState("maximized");
  },

  _onToolSelectWhileMinimized: function () {
    this.postMessage({
      name: "maximize-host"
    });
  },

  postMessage: function (msg) {
    // We sometime try to send messages in middle of destroy(), where the
    // toolbox iframe may already be detached and no longer have a parent.
    if (this.win.parent) {
      // Toolbox document is still chrome and disallow identifying message
      // origin via event.source as it is null. So use a custom id.
      msg.frameId = this.frameId;
      this.win.parent.postMessage(msg, "*");
    }
  },

  _onBottomHostWillChange: function () {
    this.postMessage({
      name: "maximize-host"
    });

    this.off("before-select", this._onToolSelectWhileMinimized);
  },

  _toggleMinimizeMode: function () {
    if (this.hostType !== Toolbox.HostType.BOTTOM) {
      return;
    }

    // Calculate the height to which the host should be minimized so the
    // tabbar is still visible.
    let toolbarHeight = this._componentMount.getBoxQuads({box: "content"})[0].bounds
                                                                             .height;
    this.postMessage({
      name: "toggle-minimize-mode",
      toolbarHeight
    });
  },

  /**
   * Initiate ToolboxTabs React component and all it's properties. Do the initial render.
   */
  _buildTabs: function () {
    // Get the initial list of tab definitions. This list can be amended at a later time
    // by tools registering themselves.
    const definitions = gDevTools.getToolDefinitionArray();
    definitions.forEach(definition => this._buildPanelForTool(definition));

    // Get the definitions that will only affect the main tab area.
    this.panelDefinitions = definitions.filter(definition =>
      definition.isTargetSupported(this._target) && definition.id !== "options");

    this.optionsDefinition = definitions.find(({id}) => id === "options");
    // The options tool is treated slightly differently, and is in a different area.
    this.component.setOptionsPanel(definitions.find(({id}) => id === "options"));
  },

  _mountReactComponent: function () {
    // Ensure the toolbar doesn't try to render until the tool is ready.
    const element = this.React.createElement(this.ToolboxController, {
      L10N,
      currentToolId: this.currentToolId,
      selectTool: this.selectTool,
      closeToolbox: this.destroy,
      focusButton: this._onToolbarFocus,
      toggleMinimizeMode: this._toggleMinimizeMode,
      toolbox: this
    });

    this.component = this.ReactDOM.render(element, this._componentMount);
  },

  /**
   * Reset tabindex attributes across all focusable elements inside the toolbar.
   * Only have one element with tabindex=0 at a time to make sure that tabbing
   * results in navigating away from the toolbar container.
   * @param  {FocusEvent} event
   */
  _onToolbarFocus: function (id) {
    this.component.setFocusedButton(id);
  },

  /**
   * On left/right arrow press, attempt to move the focus inside the toolbar to
   * the previous/next focusable element. This is not in the React component
   * as it is difficult to coordinate between different component elements.
   * The components are responsible for setting the correct tabindex value
   * for if they are the focused element.
   * @param  {KeyboardEvent} event
   */
  _onToolbarArrowKeypress: function (event) {
    let { key, target, ctrlKey, shiftKey, altKey, metaKey } = event;

    // If any of the modifier keys are pressed do not attempt navigation as it
    // might conflict with global shortcuts (Bug 1327972).
    if (ctrlKey || shiftKey || altKey || metaKey) {
      return;
    }

    let buttons = [...this._componentMount.querySelectorAll("button")];
    let curIndex = buttons.indexOf(target);

    if (curIndex === -1) {
      console.warn(target + " is not found among Developer Tools tab bar " +
        "focusable elements.");
      return;
    }

    let newTarget;

    if (key === "ArrowLeft") {
      // Do nothing if already at the beginning.
      if (curIndex === 0) {
        return;
      }
      newTarget = buttons[curIndex - 1];
    } else if (key === "ArrowRight") {
      // Do nothing if already at the end.
      if (curIndex === buttons.length - 1) {
        return;
      }
      newTarget = buttons[curIndex + 1];
    } else {
      return;
    }

    newTarget.focus();

    event.preventDefault();
    event.stopPropagation();
  },

  /**
   * Add buttons to the UI as specified in devtools/client/definitions.js
   */
  _buildButtons: Task.async(function* () {
    // Beyond the normal preference filtering
    this.toolbarButtons = [
      this._buildPickerButton(),
      this._buildFrameButton(),
      yield this._buildNoAutoHideButton()
    ];

    ToolboxButtons.forEach(definition => {
      let button = this._createButtonState(definition);
      this.toolbarButtons.push(button);
    });

    this.component.setToolboxButtons(this.toolbarButtons);
  }),

  /**
   * Button to select a frame for the inspector to target.
   */
  _buildFrameButton() {
    this.frameButton = this._createButtonState({
      id: "command-button-frames",
      description: L10N.getStr("toolbox.frames.tooltip"),
      onClick: this.showFramesMenu,
      isTargetSupported: target => {
        return target.activeTab && target.activeTab.traits.frames;
      }
    });

    return this.frameButton;
  },

  /**
   * Button that disables/enables auto-hiding XUL pop-ups. When enabled, XUL
   * pop-ups will not automatically close when they lose focus.
   */
  _buildNoAutoHideButton: Task.async(function* () {
    this.autohideButton = this._createButtonState({
      id: "command-button-noautohide",
      description: L10N.getStr("toolbox.noautohide.tooltip"),
      onClick: this._toggleNoAutohide,
      isTargetSupported: target => target.chrome
    });

    this._isDisableAutohideEnabled().then(enabled => {
      this.autohideButton.isChecked = enabled;
    });

    return this.autohideButton;
  }),

  /**
   * Toggle the picker, but also decide whether or not the highlighter should
   * focus the window. This is only desirable when the toolbox is mounted to the
   * window. When devtools is free floating, then the target window should not
   * pop in front of the viewer when the picker is clicked.
   */
  _onPickerClick: function () {
    let focus = this.hostType === Toolbox.HostType.BOTTOM ||
                this.hostType === Toolbox.HostType.SIDE;
    this.highlighterUtils.togglePicker(focus);
  },

  /**
   * If the picker is activated, then allow the Escape key to deactivate the
   * functionality instead of the default behavior of toggling the console.
   */
  _onPickerKeypress: function (event) {
    if (event.keyCode === KeyCodes.DOM_VK_ESCAPE) {
      this.highlighterUtils.cancelPicker();
      // Stop the console from toggling.
      event.stopImmediatePropagation();
    }
  },

  _onPickerStarted: function () {
    this.doc.addEventListener("keypress", this._onPickerKeypress, true);
  },

  _onPickerStopped: function () {
    this.doc.removeEventListener("keypress", this._onPickerKeypress, true);
  },

  /**
   * The element picker button enables the ability to select a DOM node by clicking
   * it on the page.
   */
  _buildPickerButton() {
    this.pickerButton = this._createButtonState({
      id: "command-button-pick",
      description: L10N.getStr("pickButton.tooltip"),
      onClick: this._onPickerClick,
      isInStartContainer: true,
      isTargetSupported: target => {
        return target.activeTab && target.activeTab.traits.frames;
      }
    });

    return this.pickerButton;
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
  * Return all toolbox buttons (command buttons, plus any others that were
  * added manually).

  /**
   * Update the visibility of the buttons.
   */
  updateToolboxButtonsVisibility() {
    this.toolbarButtons.forEach(button => {
      button.isVisible = this._commandIsVisible(button);
    });
    this.component.setToolboxButtons(this.toolbarButtons);
  },

  /**
   * Ensure the visibility of each toolbox button matches the preference value.
   */
  _commandIsVisible: function (button) {
    const {
      isTargetSupported,
      visibilityswitch
    } = button;

    let visible = Services.prefs.getBoolPref(visibilityswitch, true);

    if (isTargetSupported) {
      return visible && isTargetSupported(this.target);
    }

    return visible;
  },

  /**
   * Build a panel for a tool definition.
   *
   * @param {string} toolDefinition
   *        Tool definition of the tool to build a tab for.
   */
  _buildPanelForTool: function (toolDefinition) {
    if (!toolDefinition.isTargetSupported(this._target)) {
      return;
    }

    let deck = this.doc.getElementById("toolbox-deck");
    let id = toolDefinition.id;

    if (toolDefinition.ordinal == undefined || toolDefinition.ordinal < 0) {
      toolDefinition.ordinal = MAX_ORDINAL;
    }

    if (!toolDefinition.bgTheme) {
      toolDefinition.bgTheme = "theme-toolbar";
    }
    let panel = this.doc.createElement("vbox");
    panel.className = "toolbox-panel " + toolDefinition.bgTheme;

    // There is already a container for the webconsole frame.
    if (!this.doc.getElementById("toolbox-panel-" + id)) {
      panel.id = "toolbox-panel-" + id;
    }

    deck.appendChild(panel);

    this._addKeysToWindow();
  },

  /**
   * Lazily created map of the additional tools registered to this toolbox.
   *
   * @returns {Map<string, object>}
   *          a map of the tools definitions registered to this
   *          particular toolbox (the key is the toolId string, the value
   *          is the tool definition plain javascript object).
   */
  get additionalToolDefinitions() {
    if (!this._additionalToolDefinitions) {
      this._additionalToolDefinitions = new Map();
    }

    return this._additionalToolDefinitions;
  },

  /**
   * Retrieve the array of the additional tools registered to this toolbox.
   *
   * @return {Array<object>}
   *         the array of additional tool definitions registered on this toolbox.
   */
  getAdditionalTools() {
    if (this._additionalToolDefinitions) {
      return Array.from(this.additionalToolDefinitions.values());
    }
    return [];
  },

  /**
   * Get the additional tools that have been registered and are visible.
   *
   * @return {Array<object>}
   *         the array of additional tool definitions registered on this toolbox.
   */
  getVisibleAdditionalTools() {
    return this.visibleAdditionalTools
               .map(toolId => this.additionalToolDefinitions.get(toolId));
  },

  /**
   * Test the existence of a additional tools registered to this toolbox by tool id.
   *
   * @param {string} toolId
   *        the id of the tool to test for existence.
   *
   * @return {boolean}
   *
   */
  hasAdditionalTool(toolId) {
    return this.additionalToolDefinitions.has(toolId);
  },

  /**
   * Register and load an additional tool on this particular toolbox.
   *
   * @param {object} definition
   *        the additional tool definition to register and add to this toolbox.
   */
  addAdditionalTool(definition) {
    if (!definition.id) {
      throw new Error("Tool definition id is missing");
    }

    if (this.isToolRegistered(definition.id)) {
      throw new Error("Tool definition already registered: " +
                      definition.id);
    }
    this.additionalToolDefinitions.set(definition.id, definition);
    this.visibleAdditionalTools = [...this.visibleAdditionalTools, definition.id];

    this._combineAndSortPanelDefinitions();
    this._buildPanelForTool(definition);
  },

  /**
   * Unregister and unload an additional tool from this particular toolbox.
   *
   * @param {string} toolId
   *        the id of the additional tool to unregister and remove.
   */
  removeAdditionalTool(toolId) {
    if (!this.hasAdditionalTool(toolId)) {
      throw new Error("Tool definition not registered to this toolbox: " +
                      toolId);
    }

    this.additionalToolDefinitions.delete(toolId);
    this.visibleAdditionalTools = this.visibleAdditionalTools
                                      .filter(id => id !== toolId);
    this.unloadTool(toolId);
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
        this.once(id + "-ready", initializedPanel => {
          deferred.resolve(initializedPanel);
        });
      }
      return deferred.promise;
    }

    // Retrieve the tool definition (from the global or the per-toolbox tool maps)
    let definition = this.getToolDefinition(id);

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
      vbox.visibility = "visible";
    }

    let onLoad = () => {
      // Prevent flicker while loading by waiting to make visible until now.
      iframe.style.visibility = "visible";

      // Try to set the dir attribute as early as possible.
      this.setIframeDocumentDir(iframe);

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
   * Set the dir attribute on the content document element of the provided iframe.
   *
   * @param {IFrameElement} iframe
   */
  setIframeDocumentDir: function (iframe) {
    let docEl = iframe.contentWindow && iframe.contentWindow.document.documentElement;
    if (!docEl || docEl.namespaceURI !== HTML_NS) {
      // Bail out if the content window or document is not ready or if the document is not
      // HTML.
      return;
    }

    if (docEl.hasAttribute("dir")) {
      // Set the dir attribute value only if dir is already present on the document.
      let top = this.win.top;
      let topDocEl = top.document.documentElement;
      let isRtl = top.getComputedStyle(topDocEl).direction === "rtl";
      docEl.setAttribute("dir", isRtl ? "rtl" : "ltr");
    }
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

    if (this.currentToolId == id) {
      let panel = this._toolPanels.get(id);
      if (panel) {
        // We have a panel instance, so the tool is already fully loaded.

        // re-focus tool to get key events again
        this.focusTool(id);

        // Return the existing panel in order to have a consistent return value.
        return promise.resolve(panel);
      }
      // Otherwise, if there is no panel instance, it is still loading,
      // so we are racing another call to selectTool with the same id.
      return this.once("select").then(() => promise.resolve(this._toolPanels.get(id)));
    }

    if (!this.isReady) {
      throw new Error("Can't select tool, wait for toolbox 'ready' event");
    }

    // Check if the tool exists.
    if (this.panelDefinitions.find((definition) => definition.id === id) ||
        id === "options" ||
        this.additionalToolDefinitions.get(id)) {
      if (this.currentToolId) {
        this._telemetry.toolClosed(this.currentToolId);
      }
      this._telemetry.toolOpened(id);
    } else {
      throw new Error("No tool found");
    }

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

    return this.loadTool("webconsole").then(() => {
      this.emit("split-console");
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
    const index = this.panelDefinitions.findIndex(({id}) => id === this.currentToolId);
    let definition = this.panelDefinitions[index + 1];
    if (!definition) {
      definition = index === -1
        ? this.panelDefinitions[0]
        : this.optionsDefinition;
    }
    return this.selectTool(definition.id);
  },

  /**
   * Loads the tool just left to the currently selected tool.
   */
  selectPreviousTool: function () {
    const index = this.panelDefinitions.findIndex(({id}) => id === this.currentToolId);
    let definition = this.panelDefinitions[index - 1];
    if (!definition) {
      definition = index === -1
        ? this.panelDefinitions[this.panelDefinitions.length - 1]
        : this.optionsDefinition;
    }
    return this.selectTool(definition.id);
  },

  /**
   * Highlights the tool's tab if it is not the currently selected tool.
   *
   * @param {string} id
   *        The id of the tool to highlight
   */
  highlightTool: Task.async(function* (id) {
    if (!this.component) {
      yield this.isOpen;
    }
    this.component.highlightTool(id);
  }),

  /**
   * De-highlights the tool's tab.
   *
   * @param {string} id
   *        The id of the tool to unhighlight
   */
  unhighlightTool: Task.async(function* (id) {
    if (!this.component) {
      yield this.isOpen;
    }
    this.component.unhighlightTool(id);
  }),

  /**
   * Raise the toolbox host.
   */
  raise: function () {
    this.postMessage({
      name: "raise-host"
    });
  },

  /**
   * Refresh the host's title.
   */
  _refreshHostTitle: function () {
    let title;
    if (this.target.name && this.target.name != this.target.url) {
      const url = this.target.isWebExtension ?
                  this.target.getExtensionPathName(this.target.url) : this.target.url;
      title = L10N.getFormatStr("toolbox.titleTemplate2", this.target.name,
                                                          url);
    } else {
      title = L10N.getFormatStr("toolbox.titleTemplate1", this.target.url);
    }
    this.postMessage({
      name: "set-host-title",
      title
    });
  },

  // Returns an instance of the preference actor
  get preferenceFront() {
    if (this._preferenceFront) {
      return Promise.resolve(this._preferenceFront);
    }
    return this.isOpen.then(() => {
      return this.target.root.then(rootForm => {
        let front = getPreferenceFront(this.target.client, rootForm);
        this._preferenceFront = front;
        return front;
      });
    });
  },

  _toggleNoAutohide: Task.async(function* () {
    let front = yield this.preferenceFront;
    let toggledValue = !(yield this._isDisableAutohideEnabled());

    front.setBoolPref(DISABLE_AUTOHIDE_PREF, toggledValue);

    this.autohideButton.isChecked = toggledValue;
  }),

  _isDisableAutohideEnabled: Task.async(function* () {
    // Ensure that the tools are open, and the button is visible.
    yield this.isOpen;
    if (!this.autohideButton.isVisible) {
      return false;
    }

    let prefFront = yield this.preferenceFront;
    return yield prefFront.getBoolPref(DISABLE_AUTOHIDE_PREF);
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

      let label = frame.url;

      if (this.target.isWebExtension) {
        // Show a shorter url for extensions page.
        label = this.target.getExtensionPathName(frame.url);
      }

      // Create menu item.
      menu.append(new MenuItem({
        label,
        type: "radio",
        checked,
        click: () => {
          this.onSelectFrame(frame.id);
        }
      }));
    });

    menu.once("open").then(() => {
      this.frameButton.isChecked = true;
    });

    menu.once("close").then(() => {
      this.frameButton.isChecked = false;
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
    this._framesButtonChecked = false;

    // If non-top level frame is selected the toolbar button is
    // marked as 'checked' indicating that a child frame is active.
    if (!topFrameSelected && this.selectedFrameId) {
      this._framesButtonChecked = false;
    }
  },

  /**
   * Switch to the last used host for the toolbox UI.
   */
  switchToPreviousHost: function () {
    return this.switchHost("previous");
  },

  /**
   * Switch to a new host for the toolbox UI. E.g. bottom, sidebar, window,
   * and focus the window when done.
   *
   * @param {string} hostType
   *        The host type of the new host object
   */
  switchHost: function (hostType) {
    if (hostType == this.hostType || !this._target.isLocalTab) {
      return null;
    }

    this.emit("host-will-change", hostType);

    // ToolboxHostManager is going to call swapFrameLoaders which mess up with
    // focus. We have to blur before calling it in order to be able to restore
    // the focus after, in _onSwitchedHost.
    this.focusTool(this.currentToolId, false);

    // Host code on the chrome side will send back a message once the host
    // switched
    this.postMessage({
      name: "switch-host",
      hostType
    });

    return this.once("host-changed");
  },

  _onSwitchedHost: function ({ hostType }) {
    this._hostType = hostType;

    this._buildDockButtons();
    this._addKeysToWindow();

    // We blurred the tools at start of switchHost, but also when clicking on
    // host switching button. We now have to restore the focus.
    this.focusTool(this.currentToolId, true);

    this.emit("host-changed");
    this._telemetry.log(HOST_HISTOGRAM, this._getTelemetryHostId());
  },

  /**
   * Test the availability of a tool (both globally registered tools and
   * additional tools registered to this toolbox) by tool id.
   *
   * @param  {string} toolId
   *         Id of the tool definition to search in the per-toolbox or globally
   *         registered tools.
   *
   * @returns {bool}
   *         Returns true if the tool is registered globally or on this toolbox.
   */
  isToolRegistered: function (toolId) {
    return !!this.getToolDefinition(toolId);
  },

  /**
   * Return the tool definition registered globally or additional tools registered
   * to this toolbox.
   *
   * @param  {string} toolId
   *         Id of the tool definition to retrieve for the per-toolbox and globally
   *         registered tools.
   *
   * @returns {object}
   *         The plain javascript object that represents the requested tool definition.
   */
  getToolDefinition: function (toolId) {
    return gDevTools.getToolDefinition(toolId) ||
      this.additionalToolDefinitions.get(toolId);
  },

  /**
   * Internal helper that removes a loaded tool from the toolbox,
   * it removes a loaded tool panel and tab from the toolbox without removing
   * its definition, so that it can still be listed in options and re-added later.
   *
   * @param  {string} toolId
   *         Id of the tool to be removed.
   */
  unloadTool: function (toolId) {
    if (typeof toolId != "string") {
      throw new Error("Unexpected non-string toolId received.");
    }

    if (this._toolPanels.has(toolId)) {
      let instance = this._toolPanels.get(toolId);
      instance.destroy();
      this._toolPanels.delete(toolId);
    }

    let panel = this.doc.getElementById("toolbox-panel-" + toolId);

    // Select another tool.
    if (this.currentToolId == toolId) {
      let index = this.panelDefinitions.findIndex(({id}) => id === toolId);
      let nextTool = this.panelDefinitions[index + 1];
      let previousTool = this.panelDefinitions[index - 1];
      let toolNameToSelect;

      if (nextTool) {
        toolNameToSelect = nextTool.id;
      }
      if (previousTool) {
        toolNameToSelect = previousTool.id;
      }
      if (toolNameToSelect) {
        this.selectTool(toolNameToSelect);
      }
    }

    // Remove this tool from the current panel definitions.
    this.panelDefinitions = this.panelDefinitions.filter(({id}) => id !== toolId);
    this.visibleAdditionalTools = this.visibleAdditionalTools
                                      .filter(id => id !== toolId);
    this._combineAndSortPanelDefinitions();

    if (panel) {
      panel.remove();
    }

    if (this.hostType == Toolbox.HostType.WINDOW) {
      let doc = this.win.parent.document;
      let key = doc.getElementById("key_" + toolId);
      if (key) {
        key.remove();
      }
    }
  },

  /**
   * Handler for the tool-registered event.
   * @param  {string} event
   *         Name of the event ("tool-registered")
   * @param  {string} toolId
   *         Id of the tool that was registered
   */
  _toolRegistered: function (event, toolId) {
    // Tools can either be in the global devtools, or added to this specific toolbox
    // as an additional tool.
    let definition = gDevTools.getToolDefinition(toolId);
    let isAdditionalTool = false;
    if (!definition) {
      definition = this.additionalToolDefinitions.get(toolId);
      isAdditionalTool = true;
    }

    if (definition.isTargetSupported(this._target)) {
      if (isAdditionalTool) {
        this.visibleAdditionalTools = [...this.visibleAdditionalTools, toolId];
        this._combineAndSortPanelDefinitions();
      } else {
        this.panelDefinitions = this.panelDefinitions.concat(definition);
      }
      this._buildPanelForTool(definition);

      // Emit the event so tools can listen to it from the toolbox level
      // instead of gDevTools.
      this.emit("tool-registered", toolId);
    }
  },

  /**
   * Handler for the tool-unregistered event.
   * @param  {string} event
   *         Name of the event ("tool-unregistered")
   * @param  {string} toolId
   *         id of the tool that was unregistered
   */
  _toolUnregistered: function (event, toolId) {
    this.unloadTool(toolId);
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
        let pref = "devtools.inspector.showAllAnonymousContent";
        let showAllAnonymousContent = Services.prefs.getBoolPref(pref);
        this._walker = yield this._inspector.getWalker({ showAllAnonymousContent });
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

    this._destroyingInspector = Task.spawn(function* () {
      if (!this._inspector) {
        return;
      }

      // Ensure that the inspector isn't still being initiated, otherwise race conditions
      // in the initialization process can throw errors.
      yield this._initInspector;

      // Releasing the walker (if it has been created)
      // This can fail, but in any case, we want to continue destroying the
      // inspector/highlighter/selection
      // FF42+: Inspector actor starts managing Walker actor and auto destroy it.
      if (this._walker && !this.walker.traits.autoReleased) {
        try {
          yield this._walker.release();
        } catch (e) {
          // Do nothing;
        }
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
    return this._destroyingInspector;
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
   * Remove all UI elements, detach from target and clear up
   */
  destroy: function () {
    // If several things call destroy then we give them all the same
    // destruction promise so we're sure to destroy only once
    if (this._destroyer) {
      return this._destroyer;
    }
    let deferred = defer();
    this._destroyer = deferred.promise;

    this.emit("destroy");

    this._target.off("navigate", this._refreshHostTitle);
    this._target.off("frame-update", this._updateFrames);
    this.off("select", this._refreshHostTitle);
    this.off("host-changed", this._refreshHostTitle);
    this.off("ready", this._showDevEditionPromo);

    gDevTools.off("tool-registered", this._toolRegistered);
    gDevTools.off("tool-unregistered", this._toolUnregistered);

    Services.prefs.removeObserver("devtools.cache.disabled", this._applyCacheSettings);
    Services.prefs.removeObserver("devtools.serviceWorkers.testing.enabled",
                                  this._applyServiceWorkersTestingSettings);

    this._lastFocusedElement = null;

    if (this._sourceMapURLService) {
      this._sourceMapURLService.destroy();
      this._sourceMapURLService = null;
    }

    if (this._sourceMapService) {
      this._sourceMapService.stopSourceMapWorker();
      this._sourceMapService = null;
    }

    if (this.webconsolePanel) {
      this._saveSplitConsoleHeight();
      this.webconsolePanel.removeEventListener("resize",
        this._saveSplitConsoleHeight);
      this.webconsolePanel = null;
    }
    if (this.textBoxContextMenuPopup) {
      this.textBoxContextMenuPopup.removeEventListener("popupshowing",
        this._updateTextBoxMenuItems, true);
      this.textBoxContextMenuPopup = null;
    }
    if (this._componentMount) {
      this._componentMount.removeEventListener("keypress", this._onToolbarArrowKeypress);
      this.ReactDOM.unmountComponentAtNode(this._componentMount);
      this._componentMount = null;
    }

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
    outstanding.push(this.destroyInspector());

    // Destroy the profiler connection
    outstanding.push(this.destroyPerformance());

    // Destroy the preference front
    outstanding.push(this.destroyPreference());

    // Detach the thread
    detachThread(this._threadClient);
    this._threadClient = null;

    // Unregister buttons listeners
    this.toolbarButtons.forEach(button => {
      if (typeof button.teardown == "function") {
        // teardown arguments have already been bound in _createButtonState
        button.teardown();
      }
    });

    // We need to grab a reference to win before this._host is destroyed.
    let win = this.win;

    this._telemetry.toolClosed("toolbox");
    this._telemetry.destroy();

    // Finish all outstanding tasks (which means finish destroying panels and
    // then destroying the host, successfully or not) before destroying the
    // target.
    deferred.resolve(settleAll(outstanding)
        .catch(console.error)
        .then(() => {
          this._removeHostListeners();

          // `location` may already be 'invalid' if the toolbox document is
          // already in process of destruction. Otherwise if it is still
          // around, ensure releasing toolbox document and triggering cleanup
          // thanks to unload event. We do that precisely here, before
          // nullifying the target as various cleanup code depends on the
          // target attribute to be still
          // defined.
          try {
            win.location.replace("about:blank");
          } catch (e) {
            // Do nothing;
          }

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
          this._win = null;
          this._toolPanels.clear();

          // Force GC to prevent long GC pauses when running tests and to free up
          // memory in general when the toolbox is closed.
          if (flags.testing) {
            win.QueryInterface(Ci.nsIInterfaceRequestor)
              .getInterface(Ci.nsIDOMWindowUtils)
              .garbageCollect();
          }
        }).then(null, console.error));

    let leakCheckObserver = ({wrappedJSObject: barrier}) => {
      // Make the leak detector wait until this toolbox is properly destroyed.
      barrier.client.addBlocker("DevTools: Wait until toolbox is destroyed",
                                this._destroyer);
    };

    let topic = "shutdown-leaks-before-check";
    Services.obs.addObserver(leakCheckObserver, topic);
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
  _updateTextBoxMenuItems: function () {
    let window = this.win;
    ["cmd_undo", "cmd_delete", "cmd_cut",
     "cmd_copy", "cmd_paste", "cmd_selectAll"].forEach(window.goUpdateCommand);
  },

  /**
   * Open the textbox context menu at given coordinates.
   * Panels in the toolbox can call this on contextmenu events with event.screenX/Y
   * instead of having to implement their own copy/paste/selectAll menu.
   * @param {Number} x
   * @param {Number} y
   */
  openTextBoxContextMenu: function (x, y) {
    this.textBoxContextMenuPopup.openPopupAtScreen(x, y, true);
  },

  /**
   * Connects to the Gecko Profiler when the developer tools are open. This is
   * necessary because of the WebConsole's `profile` and `profileEnd` methods.
   */
  initPerformance: Task.async(function* () {
    // If target does not have profiler actor (addons), do not
    // even register the shared performance connection.
    if (!this.target.hasActor("profiler")) {
      return promise.resolve();
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
   * Destroy the preferences actor when the toolbox is unloaded.
   */
  destroyPreference: Task.async(function* () {
    if (!this._preferenceFront) {
      return;
    }
    this._preferenceFront.destroy();
    this._preferenceFront = null;
  }),

  /**
   * Called when any event comes from the PerformanceFront. If the performance tool is
   * already loaded when the first event comes in, immediately unbind this handler, as
   * this is only used to queue up observed recordings before the performance tool can
   * handle them, which will only occur when `console.profile()` recordings are started
   * before the tool loads.
   */
  _onPerformanceFrontEvent: Task.async(function* (eventName, recording) {
    if (this.getPanel("performance")) {
      this.performance.off("*", this._onPerformanceFrontEvent);
      return;
    }

    this._performanceQueuedRecordings = this._performanceQueuedRecordings || [];
    let recordings = this._performanceQueuedRecordings;

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
