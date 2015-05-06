/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const MAX_ORDINAL = 99;
const ZOOM_PREF = "devtools.toolbox.zoomValue";
const SPLITCONSOLE_ENABLED_PREF = "devtools.toolbox.splitconsoleEnabled";
const SPLITCONSOLE_HEIGHT_PREF = "devtools.toolbox.splitconsoleHeight";
const MIN_ZOOM = 0.5;
const MAX_ZOOM = 2;
const OS_HISTOGRAM = "DEVTOOLS_OS_ENUMERATED_PER_USER";
const OS_IS_64_BITS = "DEVTOOLS_OS_IS_64_BITS_PER_USER";
const SCREENSIZE_HISTOGRAM = "DEVTOOLS_SCREEN_RESOLUTION_ENUMERATED_PER_USER";

let {Cc, Ci, Cu} = require("chrome");
let {Promise: promise} = require("resource://gre/modules/Promise.jsm");
let EventEmitter = require("devtools/toolkit/event-emitter");
let Telemetry = require("devtools/shared/telemetry");
let {getHighlighterUtils} = require("devtools/framework/toolbox-highlighter-utils");
let HUDService = require("devtools/webconsole/hudservice");
let {showDoorhanger} = require("devtools/shared/doorhanger");
let sourceUtils = require("devtools/shared/source-utils");

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/devtools/gDevTools.jsm");
Cu.import("resource:///modules/devtools/scratchpad-manager.jsm");
Cu.import("resource:///modules/devtools/DOMHelpers.jsm");
Cu.import("resource://gre/modules/Task.jsm");

loader.lazyGetter(this, "Hosts", () => require("devtools/framework/toolbox-hosts").Hosts);

loader.lazyImporter(this, "CommandUtils", "resource:///modules/devtools/DeveloperToolbar.jsm");

loader.lazyGetter(this, "toolboxStrings", () => {
  let bundle = Services.strings.createBundle("chrome://browser/locale/devtools/toolbox.properties");
  return (name, ...args) => {
    try {
      if (!args.length) {
        return bundle.GetStringFromName(name);
      }
      return bundle.formatStringFromName(name, args, args.length);
    } catch (ex) {
      Services.console.logStringMessage("Error reading '" + name + "'");
      return null;
    }
  };
});

loader.lazyGetter(this, "Selection", () => require("devtools/framework/selection").Selection);
loader.lazyGetter(this, "InspectorFront", () => require("devtools/server/actors/inspector").InspectorFront);
loader.lazyRequireGetter(this, "DevToolsUtils", "devtools/toolkit/DevToolsUtils");
loader.lazyRequireGetter(this, "getPerformanceActorsConnection", "devtools/performance/front", true);

XPCOMUtils.defineLazyGetter(this, "screenManager", () => {
  return Cc["@mozilla.org/gfx/screenmanager;1"].getService(Ci.nsIScreenManager);
});

XPCOMUtils.defineLazyGetter(this, "oscpu", () => {
  return Cc["@mozilla.org/network/protocol;1?name=http"]
           .getService(Ci.nsIHttpProtocolHandler).oscpu;
});

XPCOMUtils.defineLazyGetter(this, "is64Bit", () => {
  return Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULAppInfo).is64Bit;
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
    isTargetSupported: target =>
      ( target.activeTab && target.activeTab.traits.frames )
  },
  { id: "command-button-splitconsole",
    isTargetSupported: target => !target.isAddon },
  { id: "command-button-responsive" },
  { id: "command-button-paintflashing" },
  { id: "command-button-tilt",
    commands: "devtools/tilt/tilt-commands" },
  { id: "command-button-scratchpad" },
  { id: "command-button-eyedropper" },
  { id: "command-button-screenshot" },
  { id: "command-button-rulers"}
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

  this._toolRegistered = this._toolRegistered.bind(this);
  this._toolUnregistered = this._toolUnregistered.bind(this);
  this._refreshHostTitle = this._refreshHostTitle.bind(this);
  this.selectFrame = this.selectFrame.bind(this);
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

  this._target.on("close", this.destroy);

  if (!hostType) {
    hostType = Services.prefs.getCharPref(this._prefs.LAST_HOST);
  }
  if (!selectedTool) {
    selectedTool = Services.prefs.getCharPref(this._prefs.LAST_TOOL);
  }
  if (!gDevTools.getToolDefinition(selectedTool)) {
    selectedTool = "webconsole";
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
  _URL: "chrome://browser/content/devtools/framework/toolbox.xul",

  _prefs: {
    LAST_HOST: "devtools.toolbox.host",
    LAST_TOOL: "devtools.toolbox.selectedTool",
    SIDE_ENABLED: "devtools.toolbox.sideEnabled"
  },

  currentToolId: null,

  /**
   * Returns a *copy* of the _toolPanels collection.
   *
   * @return {Map} panels
   *         All the running panels in the toolbox
   */
  getToolPanels: function() {
    return new Map(this._toolPanels);
  },

  /**
   * Access the panel for a given tool
   */
  getPanel: function(id) {
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
  getPanelWhenReady: function(id) {
    let deferred = promise.defer();
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
  getCurrentPanel: function() {
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
   * Shortcut to the document containing the toolbox UI
   */
  get doc() {
    return this.frame.contentDocument;
  },

  /**
   * Get current zoom level of toolbox
   */
  get zoomValue() {
    return parseFloat(Services.prefs.getCharPref(ZOOM_PREF));
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
   * Open the toolbox
   */
  open: function () {
    return Task.spawn(function*() {
      let iframe = yield this._host.create();
      let domReady = promise.defer();

      // Load the toolbox-level actor fronts and utilities now
      yield this._target.makeRemote();
      iframe.setAttribute("src", this._URL);
      iframe.setAttribute("aria-label", toolboxStrings("toolbox.label"));
      let domHelper = new DOMHelpers(iframe.contentWindow);
      domHelper.onceDOMReady(() => domReady.resolve());

      yield domReady.promise;

      this.isReady = true;
      let framesPromise = this._listFrames();

      this.closeButton = this.doc.getElementById("toolbox-close");
      this.closeButton.addEventListener("command", this.destroy, true);

      gDevTools.on("pref-changed", this._prefChanged);

      let framesMenu = this.doc.getElementById("command-button-frames");
      framesMenu.addEventListener("command", this.selectFrame, true);

      this.textboxContextMenuPopup =
        this.doc.getElementById("toolbox-textbox-context-popup");
      this.textboxContextMenuPopup.addEventListener("popupshowing",
        this._updateTextboxMenuItems, true);

      this._buildDockButtons();
      this._buildOptions();
      this._buildTabs();
      this._applyCacheSettings();
      this._applyServiceWorkersTestingSettings();
      this._addKeysToWindow();
      this._addReloadKeys();
      this._addHostListeners();
      if (this._hostOptions && this._hostOptions.zoom === false) {
        this._disableZoomKeys();
      } else {
        this._addZoomKeys();
        this._loadInitialZoom();
      }

      this.webconsolePanel = this.doc.querySelector("#toolbox-panel-webconsole");
      this.webconsolePanel.height = Services.prefs.getIntPref(SPLITCONSOLE_HEIGHT_PREF);
      this.webconsolePanel.addEventListener("resize", this._saveSplitConsoleHeight);

      let buttonsPromise = this._buildButtons();

      this._pingTelemetry();

      let panel = yield this.selectTool(this._defaultToolId);

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
      let profilerReady = this._connectProfiler();

      // However, while testing, we must wait for the performance connection to finish,
      // as most tests shut down without waiting for a toolbox destruction event,
      // resulting in the shared profiler connection being opened and closed
      // outside of the test that originally opened the toolbox.
      if (gDevTools.testing) {
        yield profilerReady;
      }

      this.emit("ready");
    }.bind(this)).then(null, console.error.bind(console));
  },

  _pingTelemetry: function() {
    this._telemetry.toolOpened("toolbox");

    this._telemetry.logOncePerBrowserVersion(OS_HISTOGRAM,
                                             this._getOsCpu());
    this._telemetry.logOncePerBrowserVersion(OS_IS_64_BITS, is64Bit ? 1 : 0);
    this._telemetry.logOncePerBrowserVersion(SCREENSIZE_HISTOGRAM,
                                             this._getScreenDimensions());
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
  _prefChanged: function(event, data) {
    switch(data.pref) {
    case "devtools.cache.disabled":
      this._applyCacheSettings();
      break;
    case "devtools.serviceWorkers.testing.enabled":
      this._applyServiceWorkersTestingSettings();
      break;
    }
  },

  _buildOptions: function() {
    let key = this.doc.getElementById("toolbox-options-key");
    key.addEventListener("command", () => {
      this.selectTool("options");
    }, true);
  },

  _splitConsoleOnKeypress: function(e) {
    if (e.keyCode === e.DOM_VK_ESCAPE) {
      this.toggleSplitConsole();
      // If the debugger is paused, don't let the ESC key stop any pending
      // navigation.
      let jsdebugger = this.getPanel("jsdebugger");
      if (jsdebugger && jsdebugger.panelWin.gThreadClient.state == "paused") {
        e.preventDefault();
      }
    }
  },

  _addReloadKeys: function() {
    [
      ["toolbox-reload-key", false],
      ["toolbox-reload-key2", false],
      ["toolbox-force-reload-key", true],
      ["toolbox-force-reload-key2", true]
    ].forEach(([id, force]) => {
      this.doc.getElementById(id).addEventListener("command", (event) => {
        this.reloadTarget(force);
      }, true);
    });
  },

  _addHostListeners: function() {
    let nextKey = this.doc.getElementById("toolbox-next-tool-key");
    nextKey.addEventListener("command", this.selectNextTool.bind(this), true);
    let prevKey = this.doc.getElementById("toolbox-previous-tool-key");
    prevKey.addEventListener("command", this.selectPreviousTool.bind(this), true);

    // Split console uses keypress instead of command so the event can be
    // cancelled with stopPropagation on the keypress, and not preventDefault.
    this.doc.addEventListener("keypress", this._splitConsoleOnKeypress, false);

    this.doc.addEventListener("focus", this._onFocus, true);
  },

  _saveSplitConsoleHeight: function() {
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
  _refreshConsoleDisplay: function() {
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
   * Wire up the listeners for the zoom keys.
   */
  _addZoomKeys: function() {
    let inKey = this.doc.getElementById("toolbox-zoom-in-key");
    inKey.addEventListener("command", this.zoomIn.bind(this), true);

    let inKey2 = this.doc.getElementById("toolbox-zoom-in-key2");
    inKey2.addEventListener("command", this.zoomIn.bind(this), true);

    let outKey = this.doc.getElementById("toolbox-zoom-out-key");
    outKey.addEventListener("command", this.zoomOut.bind(this), true);

    let resetKey = this.doc.getElementById("toolbox-zoom-reset-key");
    resetKey.addEventListener("command", this.zoomReset.bind(this), true);
  },

  _disableZoomKeys: function() {
    let inKey = this.doc.getElementById("toolbox-zoom-in-key");
    inKey.setAttribute("disabled", "true");

    let inKey2 = this.doc.getElementById("toolbox-zoom-in-key2");
    inKey2.setAttribute("disabled", "true");

    let outKey = this.doc.getElementById("toolbox-zoom-out-key");
    outKey.setAttribute("disabled", "true");

    let resetKey = this.doc.getElementById("toolbox-zoom-reset-key");
    resetKey.setAttribute("disabled", "true");
  },

  /**
   * Set zoom on toolbox to whatever the last setting was.
   */
  _loadInitialZoom: function() {
    this.setZoom(this.zoomValue);
  },

  /**
   * Increase zoom level of toolbox window - make things bigger.
   */
  zoomIn: function() {
    this.setZoom(this.zoomValue + 0.1);
  },

  /**
   * Decrease zoom level of toolbox window - make things smaller.
   */
  zoomOut: function() {
    this.setZoom(this.zoomValue - 0.1);
  },

  /**
   * Reset zoom level of the toolbox window.
   */
  zoomReset: function() {
    this.setZoom(1);
  },

  /**
   * Set zoom level of the toolbox window.
   *
   * @param {number} zoomValue
   *        Zoom level e.g. 1.2
   */
  setZoom: function(zoomValue) {
    // cap zoom value
    zoomValue = Math.max(zoomValue, MIN_ZOOM);
    zoomValue = Math.min(zoomValue, MAX_ZOOM);

    let contViewer = this.frame.docShell.contentViewer;

    contViewer.fullZoom = zoomValue;

    Services.prefs.setCharPref(ZOOM_PREF, zoomValue);
  },

  /**
   * Adds the keys and commands to the Toolbox Window in window mode.
   */
  _addKeysToWindow: function() {
    if (this.hostType != Toolbox.HostType.WINDOW) {
      return;
    }

    let doc = this.doc.defaultView.parent.document;

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
      key.setAttribute("oncommand", "void(0);"); // needed. See bug 371900
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
      key.setAttribute("oncommand", "void(0)"); // needed. See bug 371900
      key.addEventListener("command", () => {
        HUDService.toggleBrowserConsole();
      }, true);
      doc.getElementById("toolbox-keyset").appendChild(key);
    }
  },

  /**
   * Handle any custom key events.  Returns true if there was a custom key binding run
   * @param {string} toolId
   *        Which tool to run the command on (skip if not current)
   */
  fireCustomKey: function(toolId) {
    let toolDefinition = gDevTools.getToolDefinition(toolId);

    if (toolDefinition.onkey &&
        ((this.currentToolId === toolId) ||
          (toolId == "webconsole" && this.splitConsole))) {
      toolDefinition.onkey(this.getCurrentPanel(), this);
    }
  },

  /**
   * Build the buttons for changing hosts. Called every time
   * the host changes.
   */
  _buildDockButtons: function() {
    let dockBox = this.doc.getElementById("toolbox-dock-buttons");

    while (dockBox.firstChild) {
      dockBox.removeChild(dockBox.firstChild);
    }

    if (!this._target.isLocalTab) {
      return;
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

      let button = this.doc.createElement("toolbarbutton");
      button.id = "toolbox-dock-" + position;
      button.className = "toolbox-dock-button";
      button.setAttribute("tooltiptext", toolboxStrings("toolboxDockButtons." +
                                                        position + ".tooltip"));
      button.addEventListener("command", () => {
        this.switchHost(position);
      });

      dockBox.appendChild(button);
    }
  },

  /**
   * Add tabs to the toolbox UI for registered tools
   */
  _buildTabs: function() {
    for (let definition of gDevTools.getToolDefinitionArray()) {
      this._buildTabForTool(definition);
    }
  },

  /**
   * Add buttons to the UI as specified in the devtools.toolbox.toolbarSpec pref
   */
  _buildButtons: function() {
    if (!this.target.isAddon) {
      this._buildPickerButton();
    }

    this.setToolboxButtonsVisibility();

    // Old servers don't have a GCLI Actor, so just return
    if (!this.target.hasActor("gcli")) {
      return promise.resolve();
    }

    const options = {
      environment: CommandUtils.createEnvironment(this, '_target')
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
  _buildPickerButton: function() {
    this._pickerButton = this.doc.createElement("toolbarbutton");
    this._pickerButton.id = "command-button-pick";
    this._pickerButton.className = "command-button command-button-invertable";
    this._pickerButton.setAttribute("tooltiptext", toolboxStrings("pickButton.tooltip"));
    this._pickerButton.setAttribute("hidden", "true");

    let container = this.doc.querySelector("#toolbox-picker-container");
    container.appendChild(this._pickerButton);

    this._togglePicker = this.highlighterUtils.togglePicker.bind(this.highlighterUtils);
    this._pickerButton.addEventListener("command", this._togglePicker, false);
  },

  /**
   * Apply the current cache setting from devtools.cache.disabled to this
   * toolbox's tab.
   */
  _applyCacheSettings: function() {
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
  _applyServiceWorkersTestingSettings: function() {
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

      // Disable tilt in E10S mode. Removing it from the list of toolbox buttons
      // allows a bunch of tests to pass without modification.
      if (this.target.isMultiProcess && options.id === "command-button-tilt") {
        return false;
      }

      return {
        id: options.id,
        button: button,
        label: button.getAttribute("tooltiptext"),
        visibilityswitch: "devtools." + options.id + ".enabled",
        isTargetSupported: options.isTargetSupported ? options.isTargetSupported
                                                     : target => target.isLocalTab
      };
    }).filter(button=>button);
  },

  /**
   * Ensure the visibility of each toolbox button matches the
   * preference value.  Simply hide buttons that are preffed off.
   */
  setToolboxButtonsVisibility: function() {
    this.toolboxButtons.forEach(buttonSpec => {
      let { visibilityswitch, id, button, isTargetSupported } = buttonSpec;
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

    // Tilt is handled separately because it is disabled in E10S mode. Because
    // we have removed tilt from toolboxButtons we have to deal with it here.
    let tiltEnabled = !this.target.isMultiProcess &&
                      Services.prefs.getBoolPref("devtools.command-button-tilt.enabled");
    let tiltButton = this.doc.getElementById("command-button-tilt");
    // Remote toolboxes don't add the button to the DOM at all
    if (!tiltButton) {
      return;
    }

    if (tiltEnabled) {
      tiltButton.removeAttribute("hidden");
    } else {
      tiltButton.setAttribute("hidden", "true");
    }
  },

  /**
   * Build a tab for one tool definition and add to the toolbox
   *
   * @param {string} toolDefinition
   *        Tool definition of the tool to build a tab for.
   */
  _buildTabForTool: function(toolDefinition) {
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
    radio.setAttribute("ordinal", toolDefinition.ordinal);
    radio.setAttribute("tooltiptext", toolDefinition.tooltip);
    if (toolDefinition.invertIconForLightTheme) {
      radio.setAttribute("icon-invertable", "true");
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
      label.setAttribute("value", toolDefinition.label)
      label.setAttribute("crop", "end");
      label.setAttribute("flex", "1");
      radio.appendChild(label);
      radio.setAttribute("flex", "1");
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
  loadTool: function(id) {
    if (id === "inspector" && !this._inspector) {
      return this.initInspector().then(() => {
        return this.loadTool(id);
      });
    }

    let deferred = promise.defer();
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
      deferred.reject(new Error("no such tool id "+id));
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
      if (!(built instanceof Promise)) {
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
          let deferred = promise.defer();
          deferred.resolve(panel);
          built = deferred.promise;
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
      }
      iframe.addEventListener("DOMContentLoaded", callback);
    }

    return deferred.promise;
  },

  /**
   * Switch to the tool with the given id
   *
   * @param {string} id
   *        The id of the tool to switch to
   */
  selectTool: function(id) {
    let selected = this.doc.querySelector(".devtools-tab[selected]");
    if (selected) {
      selected.removeAttribute("selected");
      selected.setAttribute("aria-selected", "false");
    }

    let tab = this.doc.getElementById("toolbox-tab-" + id);
    tab.setAttribute("selected", "true");
    tab.setAttribute("aria-selected", "true");

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

    tab = this.doc.getElementById("toolbox-tab-" + id);

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
    let deck = this.doc.getElementById("toolbox-deck");
    let panel = this.doc.getElementById("toolbox-panel-" + id);
    deck.selectedPanel = panel;

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
  focusTool: function(id) {
    let iframe = this.doc.getElementById("toolbox-panel-iframe-" + id);
    iframe.focus();
  },

  /**
   * Focus split console's input line
   */
  focusConsoleInput: function() {
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
  _onFocus: function({originalTarget}) {
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
  openSplitConsole: function() {
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
  closeSplitConsole: function() {
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
  toggleSplitConsole: function() {
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
  reloadTarget: function(force) {
    this.target.activeTab.reload({ force: force });
  },

  /**
   * Loads the tool next to the currently selected tool.
   */
  selectNextTool: function() {
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
  selectPreviousTool: function() {
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
  highlightTool: function(id) {
    let tab = this.doc.getElementById("toolbox-tab-" + id);
    tab && tab.setAttribute("highlighted", "true");
  },

  /**
   * De-highlights the tool's tab.
   *
   * @param {string} id
   *        The id of the tool to unhighlight
   */
  unhighlightTool: function(id) {
    let tab = this.doc.getElementById("toolbox-tab-" + id);
    tab && tab.removeAttribute("highlighted");
  },

  /**
   * Raise the toolbox host.
   */
  raise: function() {
    this._host.raise();
  },

  /**
   * Refresh the host's title.
   */
  _refreshHostTitle: function() {
    let toolName;
    let toolDef = gDevTools.getToolDefinition(this.currentToolId);
    if (toolDef) {
      toolName = toolDef.label;
    } else {
      // no tool is selected
      toolName = toolboxStrings("toolbox.defaultTitle");
    }
    let title = toolboxStrings("toolbox.titleTemplate",
                               toolName,
                               this.target.isAddon ?
                               this.target.name :
                               this.target.url || this.target.name);
    this._host.setTitle(title);
  },

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

  selectFrame: function (event) {
    let windowId = event.target.getAttribute("data-window-id");
    let packet = {
      to: this._target.form.actor,
      type: "switchToFrame",
      windowId: windowId
    };
    this._target.client.request(packet);
    // Wait for frameUpdate event to update the UI
  },

  _updateFrames: function (event, data) {
    if (!Services.prefs.getBoolPref("devtools.command-button-frames.enabled")) {
      return;
    }

    // We may receive this event before the toolbox is ready.
    if (!this.isReady) {
      return;
    }

    let menu = this.doc.getElementById("command-button-frames");

    if (data.destroyAll) {
      let menupopup = menu.firstChild;
      while (menupopup.firstChild) {
        menupopup.firstChild.remove();
      }
      return;
    } else if (data.selected) {
      let item = menu.querySelector("menuitem[data-window-id=\"" + data.selected + "\"]");
      if (!item) {
        return;
      }
      // Toggle the toolbarbutton if we selected a non top-level frame
      if (item.hasAttribute("data-parent-id")) {
        menu.setAttribute("checked", "true");
      } else {
        menu.removeAttribute("checked");
      }
      // Uncheck the previously selected frame
      let selected = menu.querySelector("menuitem[checked=true]")
      if (selected) {
        selected.removeAttribute("checked");
      }
      // Check the new one
      item.setAttribute("checked", "true");
    } else if (data.frames) {
      data.frames.forEach(win => {
        let item = menu.querySelector("menuitem[data-window-id=\"" + win.id + "\"]");
        if (win.destroy) {
          if (item) {
            item.remove();
          }
          return;
        }
        if (!item) {
          item = this.doc.createElement("menuitem");
          item.setAttribute("data-window-id", win.id);
          if (win.parentID) {
            item.setAttribute("data-parent-id", win.parentID);
          }
          // If we register a root docshell and we don't have any selected,
          // consider it as the currently targeted one.
          if (!win.parentID && !menu.querySelector("menuitem[checked=true]")) {
            item.setAttribute("checked", "true");
            menu.removeAttribute("checked");
          }
          menu.firstChild.appendChild(item);
        }
        item.setAttribute("label", win.url);
      });
    }
  },

  /**
   * Create a host object based on the given host type.
   *
   * Warning: some hosts require that the toolbox target provides a reference to
   * the attached tab. Not all Targets have a tab property - make sure you correctly
   * mix and match hosts and targets.
   *
   * @param {string} hostType
   *        The host type of the new host object
   *
   * @return {Host} host
   *        The created host object
   */
  _createHost: function(hostType, options) {
    if (!Hosts[hostType]) {
      throw new Error("Unknown hostType: " + hostType);
    }

    // clean up the toolbox if its window is closed
    let newHost = new Hosts[hostType](this.target.tab, options);
    newHost.on("window-closed", this.destroy);
    return newHost;
  },

  /**
   * Switch to a new host for the toolbox UI. E.g.
   * bottom, sidebar, separate window.
   *
   * @param {string} hostType
   *        The host type of the new host object
   */
  switchHost: function(hostType) {
    if (hostType == this._host.type || !this._target.isLocalTab) {
      return null;
    }

    let newHost = this._createHost(hostType);
    return newHost.create().then(iframe => {
      // change toolbox document's parent to the new host
      iframe.QueryInterface(Ci.nsIFrameLoaderOwner);
      iframe.swapFrameLoaders(this.frame);

      this._host.off("window-closed", this.destroy);
      this.destroyHost();

      this._host = newHost;

      if (this.hostType != Toolbox.HostType.CUSTOM) {
        Services.prefs.setCharPref(this._prefs.LAST_HOST, this._host.type);
      }

      this._buildDockButtons();
      this._addKeysToWindow();

      this.emit("host-changed");
    });
  },

  /**
   * Handler for the tool-registered event.
   * @param  {string} event
   *         Name of the event ("tool-registered")
   * @param  {string} toolId
   *         Id of the tool that was registered
   */
  _toolRegistered: function(event, toolId) {
    let tool = gDevTools.getToolDefinition(toolId);
    this._buildTabForTool(tool);
  },

  /**
   * Handler for the tool-unregistered event.
   * @param  {string} event
   *         Name of the event ("tool-unregistered")
   * @param  {string|object} toolId
   *         Definition or id of the tool that was unregistered. Passing the
   *         tool id should be avoided as it is a temporary measure.
   */
  _toolUnregistered: function(event, toolId) {
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
      let doc = this.doc.defaultView.parent.document;
      let key = doc.getElementById("key_" + toolId);
      if (key) {
        key.parentNode.removeChild(key);
      }
    }
  },

  /**
   * Initialize the inspector/walker/selection/highlighter fronts.
   * Returns a promise that resolves when the fronts are initialized
   */
  initInspector: function() {
    if (!this._initInspector) {
      this._initInspector = Task.spawn(function*() {
        this._inspector = InspectorFront(this._target.client, this._target.form);
        this._walker = yield this._inspector.getWalker(
          {showAllAnonymousContent: Services.prefs.getBoolPref("devtools.inspector.showAllAnonymousContent")}
        );
        this._selection = new Selection(this._walker);

        if (this.highlighterUtils.isRemoteHighlightable()) {
          this.walker.on("highlighter-ready", this._highlighterReady);
          this.walker.on("highlighter-hide", this._highlighterHidden);

          let autohide = !gDevTools.testing;
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
  destroyInspector: function() {
    if (this._destroying) {
      return this._destroying;
    }

    if (!this._inspector) {
      return promise.resolve();
    }

    let outstanding = () => {
      return Task.spawn(function*() {
        yield this.highlighterUtils.stopPicker();
        yield this._inspector.destroy();
        if (this._highlighter) {
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
    };

    // Releasing the walker (if it has been created)
    // This can fail, but in any case, we want to continue destroying the
    // inspector/highlighter/selection
    let walker = (this._destroying = this._walker) ?
                 this._walker.release() :
                 promise.resolve();
    return walker.then(outstanding, outstanding);
  },

  /**
   * Get the toolbox's notification box
   *
   * @return The notification box element.
   */
  getNotificationBox: function() {
    return this.doc.getElementById("toolbox-notificationbox");
  },

  _getScreenDimensions: function() {
    let width = {};
    let height = {};

    screenManager.primaryScreen.GetRect({}, {}, width, height);
    let dims = width.value + "x" + height.value;

    if (width.value < 800 || height.value < 600) return 0;
    if (dims === "800x600")   return 1;
    if (dims === "1024x768")  return 2;
    if (dims === "1280x800")  return 3;
    if (dims === "1280x1024") return 4;
    if (dims === "1366x768")  return 5;
    if (dims === "1440x900")  return 6;
    if (dims === "1920x1080") return 7;
    if (dims === "2560×1440") return 8;
    if (dims === "2560×1600") return 9;
    if (dims === "2880x1800") return 10;
    if (width.value > 2880 || height.value > 1800) return 12;

    return 11; // Other dimension such as a VM.
  },

  _getOsCpu: function() {
    if (oscpu.includes("NT 5.1") || oscpu.includes("NT 5.2")) return 0;
    if (oscpu.includes("NT 6.0")) return 1;
    if (oscpu.includes("NT 6.1")) return 2;
    if (oscpu.includes("NT 6.2")) return 3;
    if (oscpu.includes("NT 6.3")) return 4;
    if (oscpu.includes("OS X"))   return 5;
    if (oscpu.includes("Linux"))  return 6;

    return 12; // Other OS.
  },

  /**
   * Destroy the current host, and remove event listeners from its frame.
   *
   * @return {promise} to be resolved when the host is destroyed.
   */
  destroyHost: function() {
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
  destroy: function() {
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

    if (this.webconsolePanel) {
      this._saveSplitConsoleHeight();
      this.webconsolePanel.removeEventListener("resize",
        this._saveSplitConsoleHeight);
    }
    this.closeButton.removeEventListener("command", this.destroy, true);
    this.textboxContextMenuPopup.removeEventListener("popupshowing",
      this._updateTextboxMenuItems, true);

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

    // Now that we are closing the toolbox we can re-enable the cache settings
    // and disable the service workers testing settings for the current tab.
    if (this.target.activeTab) {
      this.target.activeTab.reconfigure({
        "cacheDisabled": false,
        "serviceWorkersTestingEnabled": false
      });
    }

    // Destroying the walker and inspector fronts
    outstanding.push(this.destroyInspector().then(() => {
      // Removing buttons
      if (this._pickerButton) {
        this._pickerButton.removeEventListener("command", this._togglePicker, false);
        this._pickerButton = null;
      }
    }));

    // Destroy the profiler connection
    outstanding.push(this._disconnectProfiler());

    // We need to grab a reference to win before this._host is destroyed.
    let win = this.frame.ownerGlobal;

    if (this._requisition) {
      CommandUtils.destroyRequisition(this._requisition, this.target);
    }
    this._telemetry.toolClosed("toolbox");
    this._telemetry.destroy();

    // Finish all outstanding tasks (which means finish destroying panels and
    // then destroying the host, successfully or not) before destroying the
    // target.
    this._destroyer = DevToolsUtils.settleAll(outstanding)
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
      if (gDevTools.testing) {
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

  _highlighterReady: function() {
    this.emit("highlighter-ready");
  },

  _highlighterHidden: function() {
    this.emit("highlighter-hide");
  },

  /**
   * For displaying the promotional Doorhanger on first opening of
   * the developer tools, promoting the Developer Edition.
   */
  _showDevEditionPromo: function() {
    // Do not display in browser toolbox
    if (this.target.chrome) {
      return;
    }
    let window = this.frame.contentWindow;
    showDoorhanger({ window, type: "deveditionpromo" });
  },

  /**
   * Enable / disable necessary textbox menu items using globalOverlay.js.
   */
  _updateTextboxMenuItems: function() {
    let window = this.doc.defaultView;
    ['cmd_undo', 'cmd_delete', 'cmd_cut',
     'cmd_copy', 'cmd_paste','cmd_selectAll'].forEach(window.goUpdateCommand);
  },

  getPerformanceActorsConnection: function() {
    if (!this._performanceConnection) {
      this._performanceConnection = getPerformanceActorsConnection(this.target);
    }
    return this._performanceConnection;
  },

  /**
   * Connects to the SPS profiler when the developer tools are open. This is
   * necessary because of the WebConsole's `profile` and `profileEnd` methods.
   */
  _connectProfiler: Task.async(function*() {
    // If target does not have profiler actor (addons), do not
    // even register the shared performance connection.
    if (!this.target.hasActor("profiler")) {
      return;
    }

    yield this.getPerformanceActorsConnection().open();
    // Emit an event when connected, but don't wait on startup for this.
    this.emit("profiler-connected");
  }),

  /**
   * Disconnects the underlying Performance Actor Connection. If the connection
   * has not finished initializing, as opening a toolbox does not wait,
   * the performance connection destroy method will wait for it on its own.
   */
  _disconnectProfiler: Task.async(function*() {
    if (!this._performanceConnection) {
      return;
    }
    yield this._performanceConnection.destroy();
    this._performanceConnection = null;
  }),

  /**
   * Returns gViewSourceUtils for viewing source.
   */
  get gViewSourceUtils() {
    return this.frame.contentWindow.gViewSourceUtils;
  },

  /**
   * Opens source in style editor. Falls back to plain "view-source:".
   * @see browser/devtools/shared/source-utils.js
   */
  viewSourceInStyleEditor: function (sourceURL, sourceLine) {
    return sourceUtils.viewSourceInStyleEditor(this, sourceURL, sourceLine);
  },

  /**
   * Opens source in debugger. Falls back to plain "view-source:".
   * @see browser/devtools/shared/source-utils.js
   */
  viewSourceInDebugger: function (sourceURL, sourceLine) {
    return sourceUtils.viewSourceInDebugger(this, sourceURL, sourceLine);
  },

  /**
   * Opens source in scratchpad. Falls back to plain "view-source:".
   * TODO The `sourceURL` for scratchpad instances are like `Scratchpad/1`.
   * If instances are scoped one-per-browser-window, then we should be able
   * to infer the URL from this toolbox, or use the built in scratchpad IN
   * the toolbox.
   *
   * @see browser/devtools/shared/source-utils.js
   */
  viewSourceInScratchpad: function (sourceURL, sourceLine) {
    return sourceUtils.viewSourceInScratchpad(sourceURL, sourceLine);
  },

  /**
   * Opens source in plain "view-source:".
   * @see browser/devtools/shared/source-utils.js
   */
  viewSource: function (sourceURL, sourceLine) {
    return sourceUtils.viewSource(this, sourceURL, sourceLine);
  },
};
