/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const SOURCE_MAP_WORKER =
  "resource://devtools/client/shared/source-map/worker.js";
const SOURCE_MAP_WORKER_ASSETS =
  "resource://devtools/client/shared/source-map/assets/";

const MAX_ORDINAL = 99;
const SPLITCONSOLE_ENABLED_PREF = "devtools.toolbox.splitconsoleEnabled";
const SPLITCONSOLE_HEIGHT_PREF = "devtools.toolbox.splitconsoleHeight";
const DISABLE_AUTOHIDE_PREF = "ui.popup.disable_autohide";
const PSEUDO_LOCALE_PREF = "intl.l10n.pseudo";
const HOST_HISTOGRAM = "DEVTOOLS_TOOLBOX_HOST";
const CURRENT_THEME_SCALAR = "devtools.current_theme";
const HTML_NS = "http://www.w3.org/1999/xhtml";
const REGEX_4XX_5XX = /^[4,5]\d\d$/;

const BROWSERTOOLBOX_SCOPE_PREF = "devtools.browsertoolbox.scope";
const BROWSERTOOLBOX_SCOPE_EVERYTHING = "everything";
const BROWSERTOOLBOX_SCOPE_PARENTPROCESS = "parent-process";

var { Ci, Cc } = require("chrome");
const { debounce } = require("devtools/shared/debounce");
const { throttle } = require("devtools/shared/throttle");
const { safeAsyncMethod } = require("devtools/shared/async-utils");
var Services = require("Services");
var ChromeUtils = require("ChromeUtils");
var { gDevTools } = require("devtools/client/framework/devtools");
var EventEmitter = require("devtools/shared/event-emitter");
const Selection = require("devtools/client/framework/selection");
var Telemetry = require("devtools/client/shared/telemetry");
const { getUnicodeUrl } = require("devtools/client/shared/unicode-url");
var { DOMHelpers } = require("devtools/shared/dom-helpers");
const { KeyCodes } = require("devtools/client/shared/keycodes");
const {
  FluentL10n,
} = require("devtools/client/shared/fluent-l10n/fluent-l10n");

var Startup = Cc["@mozilla.org/devtools/startup-clh;1"].getService(
  Ci.nsISupports
).wrappedJSObject;

const { createCommandsDictionary } = require("devtools/shared/commands/index");

const { BrowserLoader } = ChromeUtils.import(
  "resource://devtools/shared/loader/browser-loader.js"
);

const { MultiLocalizationHelper } = require("devtools/shared/l10n");
const L10N = new MultiLocalizationHelper(
  "devtools/client/locales/toolbox.properties",
  "chrome://branding/locale/brand.properties"
);

loader.lazyRequireGetter(
  this,
  "registerStoreObserver",
  "devtools/client/shared/redux/subscriber",
  true
);
loader.lazyRequireGetter(
  this,
  "createToolboxStore",
  "devtools/client/framework/store",
  true
);
loader.lazyRequireGetter(
  this,
  [
    "refreshTargets",
    "registerTarget",
    "registerWalkerListeners",
    "selectTarget",
    "unregisterTarget",
  ],
  "devtools/client/framework/actions/index",
  true
);

loader.lazyRequireGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm",
  true
);
loader.lazyRequireGetter(this, "flags", "devtools/shared/flags");
loader.lazyRequireGetter(
  this,
  "KeyShortcuts",
  "devtools/client/shared/key-shortcuts"
);
loader.lazyRequireGetter(this, "ZoomKeys", "devtools/client/shared/zoom-keys");
loader.lazyRequireGetter(
  this,
  "ToolboxButtons",
  "devtools/client/definitions",
  true
);
loader.lazyRequireGetter(
  this,
  "SourceMapURLService",
  "devtools/client/framework/source-map-url-service",
  true
);
loader.lazyRequireGetter(
  this,
  "BrowserConsoleManager",
  "devtools/client/webconsole/browser-console-manager",
  true
);
loader.lazyRequireGetter(
  this,
  "viewSource",
  "devtools/client/shared/view-source"
);
loader.lazyRequireGetter(
  this,
  "buildHarLog",
  "devtools/client/netmonitor/src/har/har-builder-utils",
  true
);
loader.lazyRequireGetter(
  this,
  "NetMonitorAPI",
  "devtools/client/netmonitor/src/api",
  true
);
loader.lazyRequireGetter(
  this,
  "sortPanelDefinitions",
  "devtools/client/framework/toolbox-tabs-order-manager",
  true
);
loader.lazyRequireGetter(
  this,
  "createEditContextMenu",
  "devtools/client/framework/toolbox-context-menu",
  true
);
loader.lazyRequireGetter(
  this,
  "getSelectedTarget",
  "devtools/client/framework/reducers/targets",
  true
);
loader.lazyRequireGetter(
  this,
  "remoteClientManager",
  "devtools/client/shared/remote-debugging/remote-client-manager.js",
  true
);
loader.lazyRequireGetter(
  this,
  "ResponsiveUIManager",
  "devtools/client/responsive/manager"
);
loader.lazyRequireGetter(
  this,
  "DevToolsUtils",
  "devtools/shared/DevToolsUtils"
);
loader.lazyRequireGetter(
  this,
  "NodePicker",
  "devtools/client/inspector/node-picker"
);

loader.lazyGetter(this, "domNodeConstants", () => {
  return require("devtools/shared/dom-node-constants");
});

loader.lazyGetter(this, "DEBUG_TARGET_TYPES", () => {
  return require("devtools/client/shared/remote-debugging/constants")
    .DEBUG_TARGET_TYPES;
});

loader.lazyRequireGetter(
  this,
  "NodeFront",
  "devtools/client/fronts/node",
  true
);

loader.lazyRequireGetter(
  this,
  "PICKER_TYPES",
  "devtools/shared/picker-constants"
);

loader.lazyRequireGetter(
  this,
  "HarAutomation",
  "devtools/client/netmonitor/src/har/har-automation",
  true
);

loader.lazyRequireGetter(
  this,
  "getThreadOptions",
  "devtools/client/shared/thread-utils",
  true
);

const DEVTOOLS_F12_DISABLED_PREF = "devtools.experiment.f12.shortcut_disabled";

/**
 * A "Toolbox" is the component that holds all the tools for one specific
 * target. Visually, it's a document that includes the tools tabs and all
 * the iframes where the tool panels will be living in.
 *
 * @param {object} descriptorFront
 *        The context to inspect identified by this descriptor.
 * @param {string} selectedTool
 *        Tool to select initially
 * @param {Toolbox.HostType} hostType
 *        Type of host that will host the toolbox (e.g. sidebar, window)
 * @param {DOMWindow} contentWindow
 *        The window object of the toolbox document
 * @param {string} frameId
 *        A unique identifier to differentiate toolbox documents from the
 *        chrome codebase when passing DOM messages
 * @param {Number} msSinceProcessStart
 *        the number of milliseconds since process start using monotonic
 *        timestamps (unaffected by system clock changes).
 */
function Toolbox(
  descriptorFront,
  selectedTool,
  hostType,
  contentWindow,
  frameId,
  msSinceProcessStart
) {
  this._win = contentWindow;
  this.frameId = frameId;
  this.selection = new Selection();
  this.telemetry = new Telemetry();

  this.descriptorFront = descriptorFront;

  // The session ID is used to determine which telemetry events belong to which
  // toolbox session. Because we use Amplitude to analyse the telemetry data we
  // must use the time since the system wide epoch as the session ID.
  this.sessionId = msSinceProcessStart;

  // If the user opened the toolbox, we can now enable the F12 shortcut.
  if (Services.prefs.getBoolPref(DEVTOOLS_F12_DISABLED_PREF, false)) {
    // If the toolbox is opening while F12 was disabled, the user might have
    // pressed F12 and seen the "enable devtools" notification.
    // Flip the preference.
    Services.prefs.setBoolPref(DEVTOOLS_F12_DISABLED_PREF, false);
  }

  // Map of the available DevTools WebExtensions:
  //   Map<extensionUUID, extensionName>
  this._webExtensions = new Map();

  this._toolPanels = new Map();
  this._inspectorExtensionSidebars = new Map();

  this._netMonitorAPI = null;

  // Map of frames (id => frame-info) and currently selected frame id.
  this.frameMap = new Map();
  this.selectedFrameId = null;

  // Number of targets currently paused
  this._pausedTargets = 0;

  /**
   * KeyShortcuts instance specific to WINDOW host type.
   * This is the key shortcuts that are only register when the toolbox
   * is loaded in its own window. Otherwise, these shortcuts are typically
   * registered by devtools-startup.js module.
   */
  this._windowHostShortcuts = null;

  this._toolRegistered = this._toolRegistered.bind(this);
  this._toolUnregistered = this._toolUnregistered.bind(this);
  this._refreshHostTitle = this._refreshHostTitle.bind(this);
  this.toggleNoAutohide = this.toggleNoAutohide.bind(this);
  this.disablePseudoLocale = () => this.changePseudoLocale("none");
  this.enableAccentedPseudoLocale = () => this.changePseudoLocale("accented");
  this.enableBidiPseudoLocale = () => this.changePseudoLocale("bidi");
  this._updateFrames = this._updateFrames.bind(this);
  this._splitConsoleOnKeypress = this._splitConsoleOnKeypress.bind(this);
  this.closeToolbox = this.closeToolbox.bind(this);
  this.destroy = this.destroy.bind(this);
  this._applyCacheSettings = this._applyCacheSettings.bind(this);
  this._applyServiceWorkersTestingSettings = this._applyServiceWorkersTestingSettings.bind(
    this
  );
  this._saveSplitConsoleHeight = this._saveSplitConsoleHeight.bind(this);
  this._onFocus = this._onFocus.bind(this);
  this._onBrowserMessage = this._onBrowserMessage.bind(this);
  this._onTabsOrderUpdated = this._onTabsOrderUpdated.bind(this);
  this._onToolbarFocus = this._onToolbarFocus.bind(this);
  this._onToolbarArrowKeypress = this._onToolbarArrowKeypress.bind(this);
  this._onPickerClick = this._onPickerClick.bind(this);
  this._onPickerKeypress = this._onPickerKeypress.bind(this);
  this._onPickerStarting = this._onPickerStarting.bind(this);
  this._onPickerStarted = this._onPickerStarted.bind(this);
  this._onPickerStopped = this._onPickerStopped.bind(this);
  this._onPickerCanceled = this._onPickerCanceled.bind(this);
  this._onPickerPicked = this._onPickerPicked.bind(this);
  this._onPickerPreviewed = this._onPickerPreviewed.bind(this);
  this._onInspectObject = this._onInspectObject.bind(this);
  this._onNewSelectedNodeFront = this._onNewSelectedNodeFront.bind(this);
  this._onToolSelected = this._onToolSelected.bind(this);
  this._onContextMenu = this._onContextMenu.bind(this);
  this._onMouseDown = this._onMouseDown.bind(this);
  this.updateToolboxButtonsVisibility = this.updateToolboxButtonsVisibility.bind(
    this
  );
  this.updateToolboxButtons = this.updateToolboxButtons.bind(this);
  this.selectTool = this.selectTool.bind(this);
  this._pingTelemetrySelectTool = this._pingTelemetrySelectTool.bind(this);
  this.toggleSplitConsole = this.toggleSplitConsole.bind(this);
  this.toggleOptions = this.toggleOptions.bind(this);
  this._onTargetAvailable = this._onTargetAvailable.bind(this);
  this._onTargetDestroyed = this._onTargetDestroyed.bind(this);
  this._onTargetSelected = this._onTargetSelected.bind(this);
  this._onResourceAvailable = this._onResourceAvailable.bind(this);
  this._onResourceUpdated = this._onResourceUpdated.bind(this);
  this._onToolSelectedStopPicker = this._onToolSelectedStopPicker.bind(this);

  // `component` might be null if the toolbox was destroying during the throttling
  this._throttledSetToolboxButtons = throttle(
    () => this.component?.setToolboxButtons(this.toolbarButtons),
    500,
    this
  );

  if (!selectedTool) {
    selectedTool = Services.prefs.getCharPref(this._prefs.LAST_TOOL);
  }
  this._defaultToolId = selectedTool;

  this._hostType = hostType;

  this.isOpen = new Promise(
    function(resolve) {
      this._resolveIsOpen = resolve;
    }.bind(this)
  );

  EventEmitter.decorate(this);

  this.on("host-changed", this._refreshHostTitle);
  this.on("select", this._onToolSelected);

  this.selection.on("new-node-front", this._onNewSelectedNodeFront);

  gDevTools.on("tool-registered", this._toolRegistered);
  gDevTools.on("tool-unregistered", this._toolUnregistered);

  /**
   * Get text direction for the current locale direction.
   *
   * `getComputedStyle` forces a synchronous reflow, so use a lazy getter in order to
   * call it only once.
   */
  loader.lazyGetter(this, "direction", () => {
    const { documentElement } = this.doc;
    const isRtl =
      this.win.getComputedStyle(documentElement).direction === "rtl";
    return isRtl ? "rtl" : "ltr";
  });
}
exports.Toolbox = Toolbox;

/**
 * The toolbox can be 'hosted' either embedded in a browser window
 * or in a separate window.
 */
Toolbox.HostType = {
  BOTTOM: "bottom",
  RIGHT: "right",
  LEFT: "left",
  WINDOW: "window",
  BROWSERTOOLBOX: "browsertoolbox",
  // This is typically used by `about:debugging`, when opening toolbox in a new tab,
  // via `about:devtools-toolbox` URLs.
  PAGE: "page",
};

Toolbox.prototype = {
  _URL: "about:devtools-toolbox",

  _prefs: {
    LAST_TOOL: "devtools.toolbox.selectedTool",
    SIDE_ENABLED: "devtools.toolbox.sideEnabled",
  },

  get nodePicker() {
    if (!this._nodePicker) {
      this._nodePicker = new NodePicker(
        this.commands.targetCommand,
        this.selection
      );
      this._nodePicker.on("picker-starting", this._onPickerStarting);
      this._nodePicker.on("picker-started", this._onPickerStarted);
      this._nodePicker.on("picker-stopped", this._onPickerStopped);
      this._nodePicker.on("picker-node-canceled", this._onPickerCanceled);
      this._nodePicker.on("picker-node-picked", this._onPickerPicked);
      this._nodePicker.on("picker-node-previewed", this._onPickerPreviewed);
    }

    return this._nodePicker;
  },

  get store() {
    if (!this._store) {
      this._store = createToolboxStore();
      registerStoreObserver(this._store, this._onToolboxStateChange.bind(this));
    }
    return this._store;
  },

  get currentToolId() {
    return this._currentToolId;
  },

  set currentToolId(id) {
    this._currentToolId = id;
    this.component.setCurrentToolId(id);
  },

  get defaultToolId() {
    return this._defaultToolId;
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
    if (this.isReady) {
      this._combineAndSortPanelDefinitions();
    }
  },

  /**
   * Combines the built-in panel definitions and the additional tool definitions that
   * can be set by add-ons.
   */
  _combineAndSortPanelDefinitions() {
    let definitions = [
      ...this._panelDefinitions,
      ...this.getVisibleAdditionalTools(),
    ];
    definitions = sortPanelDefinitions(definitions);
    this.component.setPanelDefinitions(definitions);
  },

  lastUsedToolId: null,

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
    const panel = this.getPanel(id);
    return new Promise(resolve => {
      if (panel) {
        resolve(panel);
      } else {
        this.on(id + "-ready", initializedPanel => {
          resolve(initializedPanel);
        });
      }
    });
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
   * Get the current top level target the toolbox is debugging.
   *
   * This will only be defined *after* calling Toolbox.open(),
   * after it has called `targetCommands.startListening`.
   */
  get target() {
    return this.commands.targetCommand.targetFront;
  },

  get threadFront() {
    return this.commands.targetCommand.targetFront.threadFront;
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
   * When the toolbox is loaded in a frame with type="content", win.parent will not return
   * the parent Chrome window. This getter should return the parent Chrome window
   * regardless of the frame type. See Bug 1539979.
   */
  get topWindow() {
    return DevToolsUtils.getTopWindow(this.win);
  },

  get topDoc() {
    return this.topWindow.document;
  },

  /**
   * Shortcut to the document containing the toolbox UI
   */
  get doc() {
    return this.win.document;
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
  isSplitConsoleFocused: function() {
    if (!this._splitConsole) {
      return false;
    }
    const focusedWin = Services.focus.focusedWindow;
    return (
      focusedWin &&
      focusedWin ===
        this.doc.querySelector("#toolbox-panel-iframe-webconsole").contentWindow
    );
  },

  get isBrowserToolbox() {
    return this.hostType === Toolbox.HostType.BROWSERTOOLBOX;
  },

  /**
   * Set a given target as selected (which may impact the console evaluation context selector).
   *
   * @param {String} targetActorID: The actorID of the target we want to select.
   */
  selectTarget(targetActorID) {
    if (this.getSelectedTargetFront()?.actorID !== targetActorID) {
      this.store.dispatch(selectTarget(targetActorID));
    }
  },

  /**
   * @returns {ThreadFront|null} The selected thread front, or null if there is none.
   */
  getSelectedTargetFront: function() {
    const selectedTarget = getSelectedTarget(this.store.getState());
    if (!selectedTarget) {
      return null;
    }

    return this.commands.client.getFrontByID(selectedTarget.actorID);
  },

  _onToolboxStateChange(state, oldState) {
    if (getSelectedTarget(state) !== getSelectedTarget(oldState)) {
      const dbg = this.getPanel("jsdebugger");
      if (!dbg) {
        return;
      }

      const threadActorID = getSelectedTarget(state)?.threadFront?.actorID;
      if (!threadActorID) {
        return;
      }

      dbg.selectThread(threadActorID);
    }
  },

  /**
   * Called on each new THREAD_STATE resource
   *
   * @param {Object} resource The THREAD_STATE resource
   */
  _onThreadStateChanged(resource) {
    if (resource.state == "paused") {
      this._pauseToolbox(resource.why.type);
    } else if (resource.state == "resumed") {
      this._resumeToolbox();
    }
  },

  _pauseToolbox(reason) {
    // Suppress interrupted events by default because the thread is
    // paused/resumed a lot for various actions.
    if (reason === "interrupted") {
      return;
    }

    this.highlightTool("jsdebugger");

    if (
      reason === "debuggerStatement" ||
      reason === "mutationBreakpoint" ||
      reason === "eventBreakpoint" ||
      reason === "breakpoint" ||
      reason === "exception" ||
      reason === "resumeLimit" ||
      reason === "XHR"
    ) {
      this.raise();
      this.selectTool("jsdebugger", reason);
      // Each Target/Thread can be paused only once at a time,
      // so, for each pause, we should have a related resumed event.
      // But we may have multiple targets paused at the same time
      this._pausedTargets++;
      this.emit("toolbox-paused");
    }
  },

  _resumeToolbox() {
    if (this.isHighlighted("jsdebugger")) {
      this._pausedTargets--;
      if (this._pausedTargets == 0) {
        this.emit("toolbox-resumed");
        this.unhighlightTool("jsdebugger");
      }
    }
  },

  /**
   * This method will be called for the top-level target, as well as any potential
   * additional targets we may care about.
   */
  async _onTargetAvailable({ targetFront, isTargetSwitching }) {
    if (targetFront.isTopLevel) {
      // Attach to a new top-level target.
      // For now, register these event listeners only on the top level target
      if (!targetFront.targetForm.ignoreSubFrames) {
        targetFront.on("frame-update", this._updateFrames);
      }
      const consoleFront = await targetFront.getFront("console");
      consoleFront.on("inspectObject", this._onInspectObject);
    }

    // Walker listeners allow to monitor DOM Mutation breakpoint updates.
    // All targets should be monitored.
    targetFront.watchFronts("inspector", async inspectorFront => {
      registerWalkerListeners(this.store, inspectorFront.walker);
    });

    if (this.hostType !== Toolbox.HostType.PAGE) {
      await this.store.dispatch(registerTarget(targetFront));
    }

    if (targetFront.isTopLevel && isTargetSwitching) {
      // These methods expect the target to be attached, which is guaranteed by the time
      // _onTargetAvailable is called by the targetCommand.
      await this._listFrames();
      // The target may have been destroyed while calling _listFrames if we navigate quickly
      if (targetFront.isDestroyed()) {
        return;
      }
    }

    if (targetFront.targetForm.ignoreSubFrames) {
      this._updateFrames({
        frames: [
          {
            id: targetFront.actorID,
            targetFront,
            url: targetFront.url,
            title: targetFront.title,
            isTopLevel: targetFront.isTopLevel,
          },
        ],
      });
    }

    // If a new popup is debugged, automagically switch the toolbox to become
    // an independant window so that we can easily keep debugging the new tab.
    // Only do that if that's not the current top level, otherwise it means
    // we opened a toolbox dedicated to the popup.
    if (
      targetFront.targetForm.isPopup &&
      !targetFront.isTopLevel &&
      this.descriptorFront.isLocalTab
    ) {
      await this.switchHostToTab(targetFront.targetForm.browsingContextID);
    }
  },

  async _onTargetSelected({ targetFront }) {
    this._updateFrames({ selected: targetFront.actorID });
    this.selectTarget(targetFront.actorID);
  },

  _onTargetDestroyed({ targetFront }) {
    if (targetFront.isTopLevel) {
      const consoleFront = targetFront.getCachedFront("console");
      // If the target has already been destroyed, its console front will
      // also already be destroyed and so we won't be able to retrieve it.
      // Nor is it important to clear its listener as fronts automatically clears
      // all their listeners on destroy.
      if (consoleFront) {
        consoleFront.off("inspectObject", this._onInspectObject);
      }
      targetFront.off("frame-update", this._updateFrames);
      // When navigating the old target can get destroyed before the thread state changed
      // event for the target is received, so it gets lost. This currently happens with bf-cache
      // navigations when paused, so lets make sure we resumed if not.
      this._resumeToolbox();
    } else if (this.selection) {
      this.selection.onTargetDestroyed(targetFront);
    }

    if (this.hostType !== Toolbox.HostType.PAGE) {
      this.store.dispatch(unregisterTarget(targetFront));
    }

    if (targetFront.targetForm.ignoreSubFrames) {
      this._updateFrames({
        frames: [
          {
            id: targetFront.actorID,
            destroy: true,
          },
        ],
      });
    }
  },

  _onTargetThreadFrontResumeWrongOrder() {
    const box = this.getNotificationBox();
    box.appendNotification(
      L10N.getStr("toolbox.resumeOrderWarning"),
      "wrong-resume-order",
      "",
      box.PRIORITY_WARNING_HIGH
    );
  },

  /**
   * Open the toolbox
   */
  open: function() {
    return async function() {
      // Kick off async loading the Fluent bundles.
      const fluentL10n = new FluentL10n();
      const fluentInitPromise = fluentL10n.init([
        "devtools/client/toolbox.ftl",
      ]);

      const isToolboxURL = this.win.location.href.startsWith(this._URL);
      if (isToolboxURL) {
        // Update the URL so that onceDOMReady watch for the right url.
        this._URL = this.win.location.href;
      }

      const domReady = new Promise(resolve => {
        DOMHelpers.onceDOMReady(
          this.win,
          () => {
            resolve();
          },
          this._URL
        );
      });

      // This attribute is meant to be a public attribute on the Toolbox object
      // It exposes commands modules listed in devtools/shared/commands/index.js
      // which are an abstraction on top of RDP methods.
      // See devtools/shared/commands/README.md
      // Bug 1700909 will make the commands be instantiated by gDevTools instead of the Toolbox.
      this.commands = await createCommandsDictionary(this.descriptorFront);

      this.commands.targetCommand.on(
        "target-thread-wrong-order-on-resume",
        this._onTargetThreadFrontResumeWrongOrder.bind(this)
      );

      // Bug 1709063: Use commands.resourceCommand instead of toolbox.resourceCommand
      this.resourceCommand = this.commands.resourceCommand;

      // Optimization: fire up a few other things before waiting on
      // the iframe being ready (makes startup faster)
      await this.commands.targetCommand.startListening();

      // Lets get the current thread settings from the prefs and
      // update the threadConfigurationActor which should manage
      // updating the current threads.
      const options = await getThreadOptions();
      await this.commands.threadConfigurationCommand.updateConfiguration(
        options
      );

      // The targetCommand is created right before this code.
      // It means that this call to watchTargets is the first,
      // and we are registering the first target listener, which means
      // Toolbox._onTargetAvailable will be called first, before any other
      // onTargetAvailable listener that might be registered on targetCommand.
      await this.commands.targetCommand.watchTargets({
        types: this.commands.targetCommand.ALL_TYPES,
        onAvailable: this._onTargetAvailable,
        onSelected: this._onTargetSelected,
        onDestroyed: this._onTargetDestroyed,
      });

      const onResourcesWatched = this.resourceCommand.watchResources(
        [
          // Watch for console API messages, errors and network events in order to populate
          // the error count icon in the toolbox.
          this.resourceCommand.TYPES.CONSOLE_MESSAGE,
          this.resourceCommand.TYPES.ERROR_MESSAGE,
          // Independently of watching network event resources for the error count icon,
          // we need to start tracking network activity on toolbox open for targets such
          // as tabs, in order to ensure there is always at least one listener existing
          // for network events across the lifetime of the various panels, so stopping
          // the resource command from clearing out its cache of network event resources.
          this.resourceCommand.TYPES.NETWORK_EVENT,
          this.resourceCommand.TYPES.DOCUMENT_EVENT,
          this.resourceCommand.TYPES.THREAD_STATE,
        ],
        {
          onAvailable: this._onResourceAvailable,
          onUpdated: this._onResourceUpdated,
        }
      );

      await domReady;

      this.browserRequire = BrowserLoader({
        window: this.win,
        useOnlyShared: true,
      }).require;

      this.isReady = true;

      const framesPromise = this._listFrames();

      Services.prefs.addObserver(
        "devtools.cache.disabled",
        this._applyCacheSettings
      );
      Services.prefs.addObserver(
        "devtools.serviceWorkers.testing.enabled",
        this._applyServiceWorkersTestingSettings
      );
      Services.prefs.addObserver(
        BROWSERTOOLBOX_SCOPE_PREF,
        this._refreshHostTitle
      );

      // Get the DOM element to mount the ToolboxController to.
      this._componentMount = this.doc.getElementById("toolbox-toolbar-mount");

      await fluentInitPromise;

      this._mountReactComponent(fluentL10n.getBundles());
      this._buildDockOptions();
      this._buildTabs();

      // Forward configuration flags to the DevTools server.
      this._applyCacheSettings();
      this._applyServiceWorkersTestingSettings();

      this._addWindowListeners();
      this._addChromeEventHandlerEvents();

      // Get the tab bar of the ToolboxController to attach the "keypress" event listener to.
      this._tabBar = this.doc.querySelector(".devtools-tabbar");
      this._tabBar.addEventListener("keypress", this._onToolbarArrowKeypress);

      this._componentMount.setAttribute(
        "aria-label",
        L10N.getStr("toolbox.label")
      );

      // Set debug target data on the ToolboxController component.
      this._setDebugTargetData();

      this.webconsolePanel = this.doc.querySelector(
        "#toolbox-panel-webconsole"
      );
      this.webconsolePanel.height = Services.prefs.getIntPref(
        SPLITCONSOLE_HEIGHT_PREF
      );
      this.webconsolePanel.addEventListener(
        "resize",
        this._saveSplitConsoleHeight
      );

      this._buildButtons();

      this._pingTelemetry();

      // The isToolSupported check needs to happen after the target is
      // remoted, otherwise we could have done it in the toolbox constructor
      // (bug 1072764).
      const toolDef = gDevTools.getToolDefinition(this._defaultToolId);
      if (!toolDef || !toolDef.isToolSupported(this)) {
        this._defaultToolId = "webconsole";
      }

      // Start rendering the toolbox toolbar before selecting the tool, as the tools
      // can take a few hundred milliseconds seconds to start up.
      //
      // Delay React rendering as Toolbox.open is synchronous.
      // Even if this involve promises, it is synchronous. Toolbox.open already loads
      // react modules and freeze the event loop for a significant time.
      // requestIdleCallback allows releasing it to allow user events to be processed.
      // Use 16ms maximum delay to allow one frame to be rendered at 60FPS
      // (1000ms/60FPS=16ms)
      this.win.requestIdleCallback(
        () => {
          this.component.setCanRender();
        },
        { timeout: 16 }
      );

      await this.selectTool(this._defaultToolId, "initial_panel");

      // Wait until the original tool is selected so that the split
      // console input will receive focus.
      let splitConsolePromise = Promise.resolve();
      if (Services.prefs.getBoolPref(SPLITCONSOLE_ENABLED_PREF)) {
        splitConsolePromise = this.openSplitConsole();
        this.telemetry.addEventProperty(
          this.topWindow,
          "open",
          "tools",
          null,
          "splitconsole",
          true
        );
      } else {
        this.telemetry.addEventProperty(
          this.topWindow,
          "open",
          "tools",
          null,
          "splitconsole",
          false
        );
      }

      await Promise.all([
        splitConsolePromise,
        framesPromise,
        onResourcesWatched,
      ]);

      // We do not expect the focus to be restored when using about:debugging toolboxes
      // Otherwise, when reloading the toolbox, the debugged tab will be focused.
      if (this.hostType !== Toolbox.HostType.PAGE) {
        // Request the actor to restore the focus to the content page once the
        // target is detached. This typically happens when the console closes.
        // We restore the focus as it may have been stolen by the console input.
        await this.commands.targetConfigurationCommand.updateConfiguration({
          restoreFocus: true,
        });
      }

      await this.initHarAutomation();

      this.emit("ready");
      this._resolveIsOpen();
    }
      .bind(this)()
      .catch(e => {
        console.error("Exception while opening the toolbox", String(e), e);
        // While the exception stack is correctly printed in the Browser console when
        // passing `e` to console.error, it is not on the stdout, so print it via dump.
        dump(e.stack + "\n");
      });
  },

  /**
   * Retrieve the ChromeEventHandler associated to the toolbox frame.
   * When DevTools are loaded in a content frame, this will return the containing chrome
   * frame. Events from nested frames will bubble up to this chrome frame, which allows to
   * listen to events from nested frames.
   */
  getChromeEventHandler() {
    if (!this.win || !this.win.docShell) {
      return null;
    }
    return this.win.docShell.chromeEventHandler;
  },

  /**
   * Attach events on the chromeEventHandler for the current window. When loaded in a
   * frame with type set to "content", events will not bubble across frames. The
   * chromeEventHandler does not have this limitation and will catch all events triggered
   * on any of the frames under the devtools document.
   *
   * Events relying on the chromeEventHandler need to be added and removed at specific
   * moments in the lifecycle of the toolbox, so all the events relying on it should be
   * grouped here.
   */
  _addChromeEventHandlerEvents: function() {
    // win.docShell.chromeEventHandler might not be accessible anymore when removing the
    // events, so we can't rely on a dynamic getter here.
    // Keep a reference on the chromeEventHandler used to addEventListener to be sure we
    // can remove the listeners afterwards.
    this._chromeEventHandler = this.getChromeEventHandler();
    if (!this._chromeEventHandler) {
      return;
    }

    // Add shortcuts and window-host-shortcuts that use the ChromeEventHandler as target.
    this._addShortcuts();
    this._addWindowHostShortcuts();

    this._chromeEventHandler.addEventListener(
      "keypress",
      this._splitConsoleOnKeypress
    );
    this._chromeEventHandler.addEventListener("focus", this._onFocus, true);
    this._chromeEventHandler.addEventListener(
      "contextmenu",
      this._onContextMenu
    );
    this._chromeEventHandler.addEventListener("mousedown", this._onMouseDown);
  },

  _removeChromeEventHandlerEvents: function() {
    if (!this._chromeEventHandler) {
      return;
    }

    // Remove shortcuts and window-host-shortcuts that use the ChromeEventHandler as
    // target.
    this._removeShortcuts();
    this._removeWindowHostShortcuts();

    this._chromeEventHandler.removeEventListener(
      "keypress",
      this._splitConsoleOnKeypress
    );
    this._chromeEventHandler.removeEventListener("focus", this._onFocus, true);
    this._chromeEventHandler.removeEventListener(
      "contextmenu",
      this._onContextMenu
    );
    this._chromeEventHandler.removeEventListener(
      "mousedown",
      this._onMouseDown
    );

    this._chromeEventHandler = null;
  },

  _addShortcuts: function() {
    // Create shortcuts instance for the toolbox
    if (!this.shortcuts) {
      this.shortcuts = new KeyShortcuts({
        window: this.doc.defaultView,
        // The toolbox key shortcuts should be triggered from any frame in DevTools.
        // Use the chromeEventHandler as the target to catch events from all frames.
        target: this.getChromeEventHandler(),
      });
    }

    // Listen for the shortcut key to show the frame list
    this.shortcuts.on(L10N.getStr("toolbox.showFrames.key"), event => {
      if (event.target.id === "command-button-frames") {
        event.target.click();
      }
    });

    // Listen for tool navigation shortcuts.
    this.shortcuts.on(L10N.getStr("toolbox.nextTool.key"), event => {
      this.selectNextTool();
      event.preventDefault();
    });
    this.shortcuts.on(L10N.getStr("toolbox.previousTool.key"), event => {
      this.selectPreviousTool();
      event.preventDefault();
    });
    this.shortcuts.on(L10N.getStr("toolbox.toggleHost.key"), event => {
      this.switchToPreviousHost();
      event.preventDefault();
    });

    // List for Help/Settings key.
    this.shortcuts.on(L10N.getStr("toolbox.help.key"), this.toggleOptions);

    // Listen for Reload shortcuts
    [
      ["reload", false],
      ["reload2", false],
      ["forceReload", true],
      ["forceReload2", true],
    ].forEach(([id, force]) => {
      const key = L10N.getStr("toolbox." + id + ".key");
      this.shortcuts.on(key, event => {
        this.commands.targetCommand.reloadTopLevelTarget(force);

        // Prevent Firefox shortcuts from reloading the page
        event.preventDefault();
      });
    });

    // Add zoom-related shortcuts.
    if (!this._hostOptions || this._hostOptions.zoom === true) {
      ZoomKeys.register(this.win, this.shortcuts);
    }
  },

  _removeShortcuts: function() {
    if (this.shortcuts) {
      this.shortcuts.destroy();
      this.shortcuts = null;
    }
  },

  /**
   * Adds the keys and commands to the Toolbox Window in window mode.
   */
  _addWindowHostShortcuts: function() {
    if (this.hostType != Toolbox.HostType.WINDOW) {
      // Those shortcuts are only valid for host type WINDOW.
      return;
    }

    if (!this._windowHostShortcuts) {
      this._windowHostShortcuts = new KeyShortcuts({
        window: this.win,
        // The window host key shortcuts should be triggered from any frame in DevTools.
        // Use the chromeEventHandler as the target to catch events from all frames.
        target: this.getChromeEventHandler(),
      });
    }

    const shortcuts = this._windowHostShortcuts;

    for (const item of Startup.KeyShortcuts) {
      const { id, toolId, shortcut, modifiers } = item;
      const electronKey = KeyShortcuts.parseXulKey(modifiers, shortcut);

      if (id == "browserConsole") {
        // Add key for toggling the browser console from the detached window
        shortcuts.on(electronKey, () => {
          BrowserConsoleManager.toggleBrowserConsole();
        });
      } else if (toolId) {
        // KeyShortcuts contain tool-specific and global key shortcuts,
        // here we only need to copy shortcut specific to each tool.
        shortcuts.on(electronKey, () => {
          this.selectTool(toolId, "key_shortcut").then(() =>
            this.fireCustomKey(toolId)
          );
        });
      }
    }

    // CmdOrCtrl+W is registered only when the toolbox is running in
    // detached window. In the other case the entire browser tab
    // is closed when the user uses this shortcut.
    shortcuts.on(L10N.getStr("toolbox.closeToolbox.key"), this.closeToolbox);

    // The others are only registered in window host type as for other hosts,
    // these keys are already registered by devtools-startup.js
    shortcuts.on(
      L10N.getStr("toolbox.toggleToolboxF12.key"),
      this.closeToolbox
    );
    if (AppConstants.platform == "macosx") {
      shortcuts.on(
        L10N.getStr("toolbox.toggleToolboxOSX.key"),
        this.closeToolbox
      );
    } else {
      shortcuts.on(L10N.getStr("toolbox.toggleToolbox.key"), this.closeToolbox);
    }
  },

  _removeWindowHostShortcuts: function() {
    if (this._windowHostShortcuts) {
      this._windowHostShortcuts.destroy();
      this._windowHostShortcuts = null;
    }
  },

  _onContextMenu: function(e) {
    // Handle context menu events in standard input elements: <input> and <textarea>.
    // Also support for custom input elements using .devtools-input class
    // (e.g. CodeMirror instances).
    const isInInput =
      e.originalTarget.closest("input[type=text]") ||
      e.originalTarget.closest("input[type=search]") ||
      e.originalTarget.closest("input:not([type])") ||
      e.originalTarget.closest(".devtools-input") ||
      e.originalTarget.closest("textarea");

    const doc = e.originalTarget.ownerDocument;
    const isHTMLPanel = doc.documentElement.namespaceURI === HTML_NS;

    if (
      // Context-menu events on input elements will use a custom context menu.
      isInInput ||
      // Context-menu events from HTML panels should not trigger the default
      // browser context menu for HTML documents.
      isHTMLPanel
    ) {
      e.stopPropagation();
      e.preventDefault();
    }

    if (isInInput) {
      this.openTextBoxContextMenu(e.screenX, e.screenY);
    }
  },

  _onMouseDown: function(e) {
    const isMiddleClick = e.button === 1;
    if (isMiddleClick) {
      // Middle clicks will trigger the scroll lock feature to turn on.
      // When the DevTools toolbox was running in an <iframe>, this behavior was
      // disabled by default. When running in a <browser> element, we now need
      // to catch and preventDefault() on those events.
      e.preventDefault();
    }
  },

  _getDebugTargetData: function() {
    const url = new URL(this.win.location);
    const searchParams = new this.win.URLSearchParams(url.search);

    const targetType = searchParams.get("type") || DEBUG_TARGET_TYPES.TAB;

    const remoteId = searchParams.get("remoteId");
    const runtimeInfo = remoteClientManager.getRuntimeInfoByRemoteId(remoteId);
    const connectionType = remoteClientManager.getConnectionTypeByRemoteId(
      remoteId
    );

    return {
      connectionType,
      runtimeInfo,
      targetType,
    };
  },

  isDebugTargetFenix() {
    return this._getDebugTargetData()?.runtimeInfo?.isFenix;
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
    return this.browserRequire(
      "devtools/client/framework/components/ToolboxController"
    );
  },

  /**
   * Unconditionally create and get the source map service.
   */
  _createSourceMapService: function() {
    if (this._sourceMapService) {
      return this._sourceMapService;
    }
    // Uses browser loader to access the `Worker` global.
    const service = this.browserRequire(
      "devtools/client/shared/source-map/index"
    );

    // Provide a wrapper for the service that reports errors more nicely.
    this._sourceMapService = new Proxy(service, {
      get: (target, name) => {
        switch (name) {
          case "getOriginalURLs":
            return urlInfo => {
              return target.getOriginalURLs(urlInfo).catch(text => {
                const message = L10N.getFormatStr(
                  "toolbox.sourceMapFailure",
                  text,
                  urlInfo.url,
                  urlInfo.sourceMapURL
                );
                this.target.logWarningInPage(message, "source map");
                // It's ok to swallow errors here, because a null
                // result just means that no source map was found.
                return null;
              });
            };

          case "getOriginalSourceText":
            return originalSourceId => {
              return target
                .getOriginalSourceText(originalSourceId)
                .catch(error => {
                  const message = L10N.getFormatStr(
                    "toolbox.sourceMapSourceFailure",
                    error.message,
                    error.metadata ? error.metadata.url : "<unknown>"
                  );
                  this.target.logWarningInPage(message, "source map");
                  // Also replace the result with the error text.
                  // Note that this result has to have the same form
                  // as whatever the upstream getOriginalSourceText
                  // returns.
                  return {
                    text: message,
                    contentType: "text/plain",
                  };
                });
            };

          case "applySourceMap":
            return (generatedId, url, code, mappings) => {
              return target
                .applySourceMap(generatedId, url, code, mappings)
                .then(async result => {
                  // If a tool has changed or introduced a source map
                  // (e.g, by pretty-printing a source), tell the
                  // source map URL service about the change, so that
                  // subscribers to that service can be updated as
                  // well.
                  if (this._sourceMapURLService) {
                    await this._sourceMapURLService.newSourceMapCreated(
                      generatedId
                    );
                  }
                  return result;
                });
            };

          default:
            return target[name];
        }
      },
    });

    this._sourceMapService.startSourceMapWorker(
      SOURCE_MAP_WORKER,
      SOURCE_MAP_WORKER_ASSETS
    );
    return this._sourceMapService;
  },

  /**
   * A common access point for the client-side mapping service for source maps that
   * any panel can use.  This is a "low-level" API that connects to
   * the source map worker.
   */
  get sourceMapService() {
    return this._createSourceMapService();
  },

  /**
   * A common access point for the client-side parser service that any panel can use.
   */
  get parserService() {
    if (this._parserService) {
      return this._parserService;
    }

    const {
      ParserDispatcher,
    } = require("devtools/client/debugger/src/workers/parser/index");

    this._parserService = new ParserDispatcher();
    this._parserService.start(
      "resource://devtools/client/debugger/dist/parser-worker.js",
      this.win
    );
    return this._parserService;
  },

  /**
   * Clients wishing to use source maps but that want the toolbox to
   * track the source and style sheet actor mapping can use this
   * source map service.  This is a higher-level service than the one
   * returned by |sourceMapService|, in that it automatically tracks
   * source and style sheet actor IDs.
   */
  get sourceMapURLService() {
    if (this._sourceMapURLService) {
      return this._sourceMapURLService;
    }
    const sourceMaps = this._createSourceMapService();
    this._sourceMapURLService = new SourceMapURLService(
      this.commands,
      sourceMaps
    );
    return this._sourceMapURLService;
  },

  // Return HostType id for telemetry
  _getTelemetryHostId: function() {
    switch (this.hostType) {
      case Toolbox.HostType.BOTTOM:
        return 0;
      case Toolbox.HostType.RIGHT:
        return 1;
      case Toolbox.HostType.WINDOW:
        return 2;
      case Toolbox.HostType.BROWSERTOOLBOX:
        return 3;
      case Toolbox.HostType.LEFT:
        return 4;
      case Toolbox.HostType.PAGE:
        return 5;
      default:
        return 9;
    }
  },

  // Return HostType string for telemetry
  _getTelemetryHostString: function() {
    switch (this.hostType) {
      case Toolbox.HostType.BOTTOM:
        return "bottom";
      case Toolbox.HostType.LEFT:
        return "left";
      case Toolbox.HostType.RIGHT:
        return "right";
      case Toolbox.HostType.WINDOW:
        return "window";
      case Toolbox.HostType.PAGE:
        return "page";
      case Toolbox.HostType.BROWSERTOOLBOX:
        return "other";
      default:
        return "bottom";
    }
  },

  _pingTelemetry: function() {
    Services.prefs.setBoolPref("devtools.everOpened", true);
    this.telemetry.toolOpened("toolbox", this.sessionId, this);

    this.telemetry
      .getHistogramById(HOST_HISTOGRAM)
      .add(this._getTelemetryHostId());

    // Log current theme. The question we want to answer is:
    // "What proportion of users use which themes?"
    const currentTheme = Services.prefs.getCharPref("devtools.theme");
    this.telemetry.keyedScalarAdd(CURRENT_THEME_SCALAR, currentTheme, 1);

    const browserWin = this.topWindow;
    this.telemetry.preparePendingEvent(browserWin, "open", "tools", null, [
      "entrypoint",
      "first_panel",
      "host",
      "shortcut",
      "splitconsole",
      "width",
      "session_id",
    ]);
    this.telemetry.addEventProperty(
      browserWin,
      "open",
      "tools",
      null,
      "host",
      this._getTelemetryHostString()
    );
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
   * @property {Boolean} disabled - An optional disabled state for the button.
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
   * @property {Function} isToolSupported - Function to automatically enable/disable
   *                      the button based on the toolbox. If the toolbox don't support
   *                      the button feature, this method should return false.
   * @property {Function} isCurrentlyVisible - Function to automatically
   *                      hide/show the button based on current state.
   * @property {Function} isChecked - Optional function called to known if the button
   *                      is toggled or not. The function should return true when
   *                      the button should be displayed as toggled on.
   */
  _createButtonState: function(options) {
    let isCheckedValue = false;
    const {
      id,
      className,
      description,
      disabled,
      onClick,
      isInStartContainer,
      setup,
      teardown,
      isToolSupported,
      isCurrentlyVisible,
      isChecked,
      onKeyDown,
      experimentalURL,
    } = options;
    const toolbox = this;
    const button = {
      id,
      className,
      description,
      disabled,
      async onClick(event) {
        if (typeof onClick == "function") {
          await onClick(event, toolbox);
          button.emit("updatechecked");
        }
      },
      onKeyDown(event) {
        if (typeof onKeyDown == "function") {
          onKeyDown(event, toolbox);
        }
      },
      isToolSupported,
      isCurrentlyVisible,
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
      isInStartContainer: !!isInStartContainer,
      experimentalURL,
    };
    if (typeof setup == "function") {
      const onChange = () => {
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

  _splitConsoleOnKeypress: function(e) {
    if (e.keyCode === KeyCodes.DOM_VK_ESCAPE) {
      this.toggleSplitConsole();
      // If the debugger is paused, don't let the ESC key stop any pending navigation.
      // If the host is page, don't let the ESC stop the load of the webconsole frame.
      if (
        this.threadFront.state == "paused" ||
        this.hostType === Toolbox.HostType.PAGE
      ) {
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
  useKeyWithSplitConsole: function(key, handler, whichTool) {
    this.shortcuts.on(key, event => {
      if (this.currentToolId === whichTool && this.isSplitConsoleFocused()) {
        handler();
        event.preventDefault();
      }
    });
  },

  _addWindowListeners: function() {
    this.win.addEventListener("unload", this.destroy);
    this.win.addEventListener("message", this._onBrowserMessage, true);
  },

  _removeWindowListeners: function() {
    // The host iframe's contentDocument may already be gone.
    if (this.win) {
      this.win.removeEventListener("unload", this.destroy);
      this.win.removeEventListener("message", this._onBrowserMessage, true);
    }
  },

  // Called whenever the chrome send a message
  _onBrowserMessage: function(event) {
    if (event.data?.name === "switched-host") {
      this._onSwitchedHost(event.data);
    }
    if (event.data?.name === "switched-host-to-tab") {
      this._onSwitchedHostToTab(event.data.browsingContextID);
    }
  },

  _saveSplitConsoleHeight: function() {
    Services.prefs.setIntPref(
      SPLITCONSOLE_HEIGHT_PREF,
      this.webconsolePanel.height
    );
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
    const deck = this.doc.getElementById("toolbox-deck");
    const webconsolePanel = this.webconsolePanel;
    const splitter = this.doc.getElementById("toolbox-console-splitter");
    const openedConsolePanel = this.currentToolId === "webconsole";

    if (openedConsolePanel) {
      deck.collapsed = true;
      splitter.hidden = true;
      webconsolePanel.collapsed = false;
    } else {
      deck.collapsed = false;
      splitter.hidden = !this.splitConsole;
      webconsolePanel.collapsed = !this.splitConsole;
    }
  },

  /**
   * Handle any custom key events.  Returns true if there was a custom key
   * binding run.
   * @param {string} toolId Which tool to run the command on (skip if not
   * current)
   */
  fireCustomKey: function(toolId) {
    const toolDefinition = gDevTools.getToolDefinition(toolId);

    if (
      toolDefinition.onkey &&
      (this.currentToolId === toolId ||
        (toolId == "webconsole" && this.splitConsole))
    ) {
      toolDefinition.onkey(this.getCurrentPanel(), this);
    }
  },

  /**
   * Build the notification box as soon as needed.
   */
  get notificationBox() {
    if (!this._notificationBox) {
      let { NotificationBox, PriorityLevels } = this.browserRequire(
        "devtools/client/shared/components/NotificationBox"
      );

      NotificationBox = this.React.createFactory(NotificationBox);

      // Render NotificationBox and assign priority levels to it.
      const box = this.doc.getElementById("toolbox-notificationbox");
      this._notificationBox = Object.assign(
        this.ReactDOM.render(NotificationBox({}), box),
        PriorityLevels
      );
    }
    return this._notificationBox;
  },

  /**
   * Build the options for changing hosts. Called every time
   * the host changes.
   */
  _buildDockOptions: function() {
    if (!this.descriptorFront.isLocalTab) {
      this.component.setDockOptionsEnabled(false);
      this.component.setCanCloseToolbox(false);
      return;
    }

    this.component.setDockOptionsEnabled(true);
    this.component.setCanCloseToolbox(
      this.hostType !== Toolbox.HostType.WINDOW
    );

    const sideEnabled = Services.prefs.getBoolPref(this._prefs.SIDE_ENABLED);

    const hostTypes = [];
    for (const type in Toolbox.HostType) {
      const position = Toolbox.HostType[type];
      if (
        position == Toolbox.HostType.BROWSERTOOLBOX ||
        position == Toolbox.HostType.PAGE ||
        (!sideEnabled &&
          (position == Toolbox.HostType.LEFT ||
            position == Toolbox.HostType.RIGHT))
      ) {
        continue;
      }

      hostTypes.push({
        position,
        switchHost: this.switchHost.bind(this, position),
      });
    }

    this.component.setCurrentHostType(this.hostType);
    this.component.setHostTypes(hostTypes);
  },

  postMessage: function(msg) {
    // We sometime try to send messages in middle of destroy(), where the
    // toolbox iframe may already be detached.
    if (!this._destroyer) {
      // Toolbox document is still chrome and disallow identifying message
      // origin via event.source as it is null. So use a custom id.
      msg.frameId = this.frameId;
      this.topWindow.postMessage(msg, "*");
    }
  },

  /**
   * Initiate ToolboxTabs React component and all it's properties. Do the initial render.
   */
  _buildTabs: async function() {
    // Get the initial list of tab definitions. This list can be amended at a later time
    // by tools registering themselves.
    const definitions = gDevTools.getToolDefinitionArray();
    definitions.forEach(definition => this._buildPanelForTool(definition));

    // Get the definitions that will only affect the main tab area.
    this.panelDefinitions = definitions.filter(
      definition =>
        definition.isToolSupported(this) && definition.id !== "options"
    );

    // Do async lookups for the target browser's state.
    if (this.isBrowserChromeTarget) {
      // Parallelize the asynchronous calls, so that the DOM is only updated once when
      // updating the React components.
      const [disableAutohide, pseudoLocale] = await Promise.all([
        this._isDisableAutohideEnabled(),
        this.getPseudoLocale(),
      ]);
      this.component.setDisableAutohide(disableAutohide);
      this.component.setPseudoLocale(pseudoLocale);
    }
  },

  _mountReactComponent(fluentBundles) {
    // Ensure the toolbar doesn't try to render until the tool is ready.
    const element = this.React.createElement(this.ToolboxController, {
      L10N,
      fluentBundles,
      currentToolId: this.currentToolId,
      selectTool: this.selectTool,
      toggleOptions: this.toggleOptions,
      toggleSplitConsole: this.toggleSplitConsole,
      toggleNoAutohide: this.toggleNoAutohide,
      disablePseudoLocale: this.disablePseudoLocale,
      enableAccentedPseudoLocale: this.enableAccentedPseudoLocale,
      enableBidiPseudoLocale: this.enableBidiPseudoLocale,
      closeToolbox: this.closeToolbox,
      focusButton: this._onToolbarFocus,
      toolbox: this,
      onTabsOrderUpdated: this._onTabsOrderUpdated,
    });

    this.component = this.ReactDOM.render(element, this._componentMount);
  },

  /**
   * Reset tabindex attributes across all focusable elements inside the toolbar.
   * Only have one element with tabindex=0 at a time to make sure that tabbing
   * results in navigating away from the toolbar container.
   * @param  {FocusEvent} event
   */
  _onToolbarFocus: function(id) {
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
  _onToolbarArrowKeypress: function(event) {
    const { key, target, ctrlKey, shiftKey, altKey, metaKey } = event;

    // If any of the modifier keys are pressed do not attempt navigation as it
    // might conflict with global shortcuts (Bug 1327972).
    if (ctrlKey || shiftKey || altKey || metaKey) {
      return;
    }

    const buttons = [...this._tabBar.querySelectorAll("button")];
    const curIndex = buttons.indexOf(target);

    if (curIndex === -1) {
      console.warn(
        target +
          " is not found among Developer Tools tab bar " +
          "focusable elements."
      );
      return;
    }

    let newTarget;
    const firstTabIndex = 0;
    const lastTabIndex = buttons.length - 1;
    const nextOrLastTabIndex = Math.min(lastTabIndex, curIndex + 1);
    const previousOrFirstTabIndex = Math.max(firstTabIndex, curIndex - 1);
    const ltr = this.direction === "ltr";

    if (key === "ArrowLeft") {
      // Do nothing if already at the beginning.
      if (
        (ltr && curIndex === firstTabIndex) ||
        (!ltr && curIndex === lastTabIndex)
      ) {
        return;
      }
      newTarget = buttons[ltr ? previousOrFirstTabIndex : nextOrLastTabIndex];
    } else if (key === "ArrowRight") {
      // Do nothing if already at the end.
      if (
        (ltr && curIndex === lastTabIndex) ||
        (!ltr && curIndex === firstTabIndex)
      ) {
        return;
      }
      newTarget = buttons[ltr ? nextOrLastTabIndex : previousOrFirstTabIndex];
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
  _buildButtons() {
    // Beyond the normal preference filtering
    this.toolbarButtons = [
      this._buildErrorCountButton(),
      this._buildPickerButton(),
      this._buildFrameButton(),
    ];

    ToolboxButtons.forEach(definition => {
      const button = this._createButtonState(definition);
      this.toolbarButtons.push(button);
    });

    this.component.setToolboxButtons(this.toolbarButtons);
  },

  /**
   * Button to select a frame for the inspector to target.
   */
  _buildFrameButton() {
    this.frameButton = this._createButtonState({
      id: "command-button-frames",
      description: L10N.getStr("toolbox.frames.tooltip"),
      isToolSupported: toolbox => {
        return toolbox.target.getTrait("frames");
      },
      isCurrentlyVisible: () => {
        const hasFrames = this.frameMap.size > 1;
        const isOnOptionsPanel = this.currentToolId === "options";
        return hasFrames || isOnOptionsPanel;
      },
    });

    return this.frameButton;
  },

  /**
   * Button to display the number of errors.
   */
  _buildErrorCountButton() {
    this.errorCountButton = this._createButtonState({
      id: "command-button-errorcount",
      isInStartContainer: false,
      isToolSupported: toolbox => true,
      description: L10N.getStr("toolbox.errorCountButton.description"),
    });
    // Use updateErrorCountButton to set some properties so we don't have to repeat
    // the logic here.
    this.updateErrorCountButton();

    return this.errorCountButton;
  },

  /**
   * Toggle the picker, but also decide whether or not the highlighter should
   * focus the window. This is only desirable when the toolbox is mounted to the
   * window. When devtools is free floating, then the target window should not
   * pop in front of the viewer when the picker is clicked.
   *
   * Note: Toggle picker can be overwritten by panel other than the inspector to
   * allow for custom picker behaviour.
   */
  _onPickerClick: async function() {
    const focus =
      this.hostType === Toolbox.HostType.BOTTOM ||
      this.hostType === Toolbox.HostType.LEFT ||
      this.hostType === Toolbox.HostType.RIGHT;
    const currentPanel = this.getCurrentPanel();
    if (currentPanel.togglePicker) {
      currentPanel.togglePicker(focus);
    } else {
      this.nodePicker.togglePicker(focus);
    }
  },

  /**
   * If the picker is activated, then allow the Escape key to deactivate the
   * functionality instead of the default behavior of toggling the console.
   */
  _onPickerKeypress: function(event) {
    if (event.keyCode === KeyCodes.DOM_VK_ESCAPE) {
      const currentPanel = this.getCurrentPanel();
      if (currentPanel.cancelPicker) {
        currentPanel.cancelPicker();
      } else {
        this.nodePicker.stop({ canceled: true });
      }
      // Stop the console from toggling.
      event.stopImmediatePropagation();
    }
  },

  _onPickerStarting: async function() {
    if (this.isDestroying()) {
      return;
    }
    this.tellRDMAboutPickerState(true, PICKER_TYPES.ELEMENT);
    this.pickerButton.isChecked = true;
    await this.selectTool("inspector", "inspect_dom");
    // turn off color picker when node picker is starting
    this.getPanel("inspector").hideEyeDropper();
    this.on("select", this._onToolSelectedStopPicker);
  },

  _onPickerStarted: async function() {
    this.doc.addEventListener("keypress", this._onPickerKeypress, true);
  },

  _onPickerStopped: function() {
    if (this.isDestroying()) {
      return;
    }
    this.tellRDMAboutPickerState(false, PICKER_TYPES.ELEMENT);
    this.off("select", this._onToolSelectedStopPicker);
    this.doc.removeEventListener("keypress", this._onPickerKeypress, true);
    this.pickerButton.isChecked = false;
  },

  _onToolSelectedStopPicker: function() {
    this.nodePicker.stop({ canceled: true });
  },

  /**
   * When the picker is canceled, make sure the toolbox
   * gets the focus.
   */
  _onPickerCanceled: function() {
    if (this.hostType !== Toolbox.HostType.WINDOW) {
      this.win.focus();
    }
  },

  _onPickerPicked: function(nodeFront) {
    this.selection.setNodeFront(nodeFront, { reason: "picker-node-picked" });
  },

  _onPickerPreviewed: function(nodeFront) {
    this.selection.setNodeFront(nodeFront, { reason: "picker-node-previewed" });
  },

  /**
   * RDM sometimes simulates touch events. For this to work correctly at all times, it
   * needs to know when the picker is active or not.
   * This method communicates with the RDM Manager if it exists.
   *
   * @param {Boolean} state
   * @param {String} pickerType
   *        One of devtools/shared/picker-constants
   */
  tellRDMAboutPickerState: async function(state, pickerType) {
    const { localTab } = this.target;

    if (!ResponsiveUIManager.isActiveForTab(localTab)) {
      return;
    }

    const ui = ResponsiveUIManager.getResponsiveUIForTab(localTab);
    await ui.responsiveFront.setElementPickerState(state, pickerType);
  },

  /**
   * The element picker button enables the ability to select a DOM node by clicking
   * it on the page.
   */
  _buildPickerButton() {
    this.pickerButton = this._createButtonState({
      id: "command-button-pick",
      className: this._getPickerAdditionalClassName(),
      description: this._getPickerTooltip(),
      onClick: this._onPickerClick,
      isInStartContainer: true,
      isToolSupported: toolbox => {
        return toolbox.target.getTrait("frames");
      },
    });

    return this.pickerButton;
  },

  _getPickerAdditionalClassName() {
    if (this.isDebugTargetFenix()) {
      return "remote-fenix";
    }
    return null;
  },

  /**
   * Get the tooltip for the element picker button.
   * It has multiple possible keyboard shortcuts for macOS.
   *
   * @return {String}
   */
  _getPickerTooltip() {
    let shortcut = L10N.getStr("toolbox.elementPicker.key");
    shortcut = KeyShortcuts.parseElectronKey(this.win, shortcut);
    shortcut = KeyShortcuts.stringify(shortcut);
    const shortcutMac = L10N.getStr("toolbox.elementPicker.mac.key");
    const isMac = Services.appinfo.OS === "Darwin";

    let label;
    if (this.isDebugTargetFenix()) {
      label = isMac
        ? "toolbox.androidElementPicker.mac.tooltip"
        : "toolbox.androidElementPicker.tooltip";
    } else {
      label = isMac
        ? "toolbox.elementPicker.mac.tooltip"
        : "toolbox.elementPicker.tooltip";
    }

    return isMac
      ? L10N.getFormatStr(label, shortcut, shortcutMac)
      : L10N.getFormatStr(label, shortcut);
  },

  /**
   * Apply the current cache setting from devtools.cache.disabled to this
   * toolbox's tab.
   */
  _applyCacheSettings: async function() {
    const pref = "devtools.cache.disabled";
    const cacheDisabled = Services.prefs.getBoolPref(pref);

    await this.commands.targetConfigurationCommand.updateConfiguration({
      cacheDisabled,
    });

    // This event is only emitted for tests in order to know when to reload
    if (flags.testing) {
      this.emit("cache-reconfigured");
    }
  },

  /**
   * Apply the current service workers testing setting from
   * devtools.serviceWorkers.testing.enabled to this toolbox's tab.
   */
  _applyServiceWorkersTestingSettings: function() {
    const pref = "devtools.serviceWorkers.testing.enabled";
    const serviceWorkersTestingEnabled = Services.prefs.getBoolPref(pref);
    this.commands.targetConfigurationCommand.updateConfiguration({
      serviceWorkersTestingEnabled,
    });
  },

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
   * Update the buttons.
   */
  updateToolboxButtons() {
    const inspectorFront = this.target.getCachedFront("inspector");
    // two of the buttons have highlighters that need to be cleared
    // on will-navigate, otherwise we hold on to the stale highlighter
    const hasHighlighters =
      inspectorFront &&
      (inspectorFront.hasHighlighter("RulersHighlighter") ||
        inspectorFront.hasHighlighter("MeasuringToolHighlighter"));
    if (hasHighlighters) {
      inspectorFront.destroyHighlighters();
      this.component.setToolboxButtons(this.toolbarButtons);
    }
  },

  /**
   * Visually update picker button.
   * This function is called on every "select" event. Newly selected panel can
   * update the visual state of the picker button such as disabled state,
   * additional CSS classes (className), and tooltip (description).
   */
  updatePickerButton() {
    const button = this.pickerButton;
    const currentPanel = this.getCurrentPanel();

    if (currentPanel?.updatePickerButton) {
      currentPanel.updatePickerButton();
    } else {
      // If the current panel doesn't define a custom updatePickerButton,
      // revert the button to its default state
      button.description = this._getPickerTooltip();
      button.className = this._getPickerAdditionalClassName();
      button.disabled = null;
    }
  },

  /**
   * Update the visual state of the Frame picker button.
   */
  updateFrameButton() {
    if (this.isDestroying()) {
      return;
    }

    if (this.currentToolId === "options" && this.frameMap.size <= 1) {
      // If the button is only visible because the user is on the Options panel, disable
      // the button and set an appropriate description.
      this.frameButton.disabled = true;
      this.frameButton.description = L10N.getStr(
        "toolbox.frames.disabled.tooltip"
      );
    } else {
      // Otherwise, enable the button and update the description.
      this.frameButton.disabled = false;
      this.frameButton.description = L10N.getStr("toolbox.frames.tooltip");
    }

    // Highlight the button when a child frame is selected and visible.
    const selectedFrame = this.frameMap.get(this.selectedFrameId) || {};

    // We need to do something a bit different to avoid some test failures. This function
    // can be called from onWillNavigate, and the current target might have this `traits`
    // property nullifed, which is unfortunate as that's what isToolSupported is checking,
    // so it will throw.
    // So here, we check first if the button isn't going to be visible anyway (it only checks
    // for this.frameMap size) so we don't call _commandIsVisible.
    const isVisible = !this.frameButton.isCurrentlyVisible()
      ? false
      : this._commandIsVisible(this.frameButton);

    this.frameButton.isVisible = isVisible;

    if (isVisible) {
      this.frameButton.isChecked = !selectedFrame.isTopLevel;
    }
  },

  updateErrorCountButton() {
    this.errorCountButton.isVisible =
      this._commandIsVisible(this.errorCountButton) && this._errorCount > 0;
    this.errorCountButton.errorCount = this._errorCount;
  },

  /**
   * Ensure the visibility of each toolbox button matches the preference value.
   */
  _commandIsVisible: function(button) {
    const { isToolSupported, isCurrentlyVisible, visibilityswitch } = button;

    if (!Services.prefs.getBoolPref(visibilityswitch, true)) {
      return false;
    }

    if (isToolSupported && !isToolSupported(this)) {
      return false;
    }

    if (isCurrentlyVisible && !isCurrentlyVisible()) {
      return false;
    }

    return true;
  },

  /**
   * Build a panel for a tool definition.
   *
   * @param {string} toolDefinition
   *        Tool definition of the tool to build a tab for.
   */
  _buildPanelForTool: function(toolDefinition) {
    if (!toolDefinition.isToolSupported(this)) {
      return;
    }

    const deck = this.doc.getElementById("toolbox-deck");
    const id = toolDefinition.id;

    if (toolDefinition.ordinal == undefined || toolDefinition.ordinal < 0) {
      toolDefinition.ordinal = MAX_ORDINAL;
    }

    if (!toolDefinition.bgTheme) {
      toolDefinition.bgTheme = "theme-toolbar";
    }
    const panel = this.doc.createXULElement("vbox");
    panel.className = "toolbox-panel " + toolDefinition.bgTheme;

    // There is already a container for the webconsole frame.
    if (!this.doc.getElementById("toolbox-panel-" + id)) {
      panel.id = "toolbox-panel-" + id;
    }

    deck.appendChild(panel);
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
    return this.visibleAdditionalTools.map(toolId =>
      this.additionalToolDefinitions.get(toolId)
    );
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
      throw new Error("Tool definition already registered: " + definition.id);
    }

    this.additionalToolDefinitions.set(definition.id, definition);
    this.visibleAdditionalTools = [
      ...this.visibleAdditionalTools,
      definition.id,
    ];

    const buildPanel = () => this._buildPanelForTool(definition);

    if (this.isReady) {
      buildPanel();
    } else {
      this.once("ready", buildPanel);
    }
  },

  /**
   * Retrieve the registered inspector extension sidebars
   * (used by the inspector panel during its deferred initialization).
   */
  get inspectorExtensionSidebars() {
    return this._inspectorExtensionSidebars;
  },

  /**
   * Register an extension sidebar for the inspector panel.
   *
   * @param {String} id
   *        An unique sidebar id
   * @param {Object} options
   * @param {String} options.title
   *        A title for the sidebar
   */
  async registerInspectorExtensionSidebar(id, options) {
    this._inspectorExtensionSidebars.set(id, options);

    // Defer the extension sidebar creation if the inspector
    // has not been created yet (and do not create the inspector
    // only to register an extension sidebar).
    if (!this.target.getCachedFront("inspector")) {
      return;
    }

    const inspector = this.getPanel("inspector");
    if (!inspector) {
      return;
    }

    inspector.addExtensionSidebar(id, options);
  },

  /**
   * Unregister an extension sidebar for the inspector panel.
   *
   * @param {String} id
   *        An unique sidebar id
   */
  unregisterInspectorExtensionSidebar(id) {
    // Unregister the sidebar from the toolbox if the toolbox is not already
    // being destroyed (otherwise we would trigger a re-rendering of the
    // inspector sidebar tabs while the toolbox is going away).
    if (this._destroyer) {
      return;
    }

    const sidebarDef = this._inspectorExtensionSidebars.get(id);
    if (!sidebarDef) {
      return;
    }

    this._inspectorExtensionSidebars.delete(id);

    // Remove the created sidebar instance if the inspector panel
    // has been already created.
    if (!this.target.getCachedFront("inspector")) {
      return;
    }

    const inspector = this.getPanel("inspector");
    inspector.removeExtensionSidebar(id);
  },

  /**
   * Unregister and unload an additional tool from this particular toolbox.
   *
   * @param {string} toolId
   *        the id of the additional tool to unregister and remove.
   */
  removeAdditionalTool(toolId) {
    // Early exit if the toolbox is already destroying itself.
    if (this._destroyer) {
      return;
    }

    if (!this.hasAdditionalTool(toolId)) {
      throw new Error(
        "Tool definition not registered to this toolbox: " + toolId
      );
    }

    this.additionalToolDefinitions.delete(toolId);
    this.visibleAdditionalTools = this.visibleAdditionalTools.filter(
      id => id !== toolId
    );
    this.unloadTool(toolId);
  },

  /**
   * Ensure the tool with the given id is loaded.
   *
   * @param {string} id
   *        The id of the tool to load.
   * @param {Object} options
   *        Object that will be passed to the panel `open` method.
   */
  loadTool: function(id, options) {
    let iframe = this.doc.getElementById("toolbox-panel-iframe-" + id);
    if (iframe) {
      const panel = this._toolPanels.get(id);
      return new Promise(resolve => {
        if (panel) {
          resolve(panel);
        } else {
          this.once(id + "-ready", initializedPanel => {
            resolve(initializedPanel);
          });
        }
      });
    }

    return new Promise((resolve, reject) => {
      // Retrieve the tool definition (from the global or the per-toolbox tool maps)
      const definition = this.getToolDefinition(id);

      if (!definition) {
        reject(new Error("no such tool id " + id));
        return;
      }

      iframe = this.doc.createXULElement("iframe");
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
        const vbox = this.doc.getElementById("toolbox-panel-" + id);
        vbox.appendChild(iframe);
        vbox.visibility = "visible";
      }

      const onLoad = async () => {
        // Prevent flicker while loading by waiting to make visible until now.
        iframe.style.visibility = "visible";

        // Try to set the dir attribute as early as possible.
        this.setIframeDocumentDir(iframe);

        // The build method should return a panel instance, so events can
        // be fired with the panel as an argument. However, in order to keep
        // backward compatibility with existing extensions do a check
        // for a promise return value.
        let built = definition.build(iframe.contentWindow, this, this.commands);

        if (!(typeof built.then == "function")) {
          const panel = built;
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
            built = panel.open(options);
          } else {
            built = new Promise(resolve => {
              resolve(panel);
            });
          }
        }

        // Wait till the panel is fully ready and fire 'ready' events.
        Promise.resolve(built).then(panel => {
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

          resolve(panel);
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
        DOMHelpers.onceDOMReady(iframe.contentWindow, onLoad);
      } else {
        const callback = () => {
          iframe.removeEventListener("DOMContentLoaded", callback);
          onLoad();
        };

        iframe.addEventListener("DOMContentLoaded", callback);
      }
    });
  },

  /**
   * Set the dir attribute on the content document element of the provided iframe.
   *
   * @param {IFrameElement} iframe
   */
  setIframeDocumentDir: function(iframe) {
    const docEl = iframe.contentWindow?.document.documentElement;
    if (!docEl || docEl.namespaceURI !== HTML_NS) {
      // Bail out if the content window or document is not ready or if the document is not
      // HTML.
      return;
    }

    if (docEl.hasAttribute("dir")) {
      // Set the dir attribute value only if dir is already present on the document.
      docEl.setAttribute("dir", this.direction);
    }
  },

  /**
   * Mark all in collection as unselected; and id as selected
   * @param {string} collection
   *        DOM collection of items
   * @param {string} id
   *        The Id of the item within the collection to select
   */
  selectSingleNode: function(collection, id) {
    [...collection].forEach(node => {
      if (node.id === id) {
        node.setAttribute("selected", "true");
        node.setAttribute("aria-selected", "true");
      } else {
        node.removeAttribute("selected");
        node.removeAttribute("aria-selected");
      }
      // The webconsole panel is in a special location due to split console
      if (!node.id) {
        node = this.webconsolePanel;
      }

      const iframe = node.querySelector(".toolbox-panel-iframe");
      if (iframe) {
        let visible = node.id == id;
        // Prevents hiding the split-console if it is currently enabled
        if (node == this.webconsolePanel && this.splitConsole) {
          visible = true;
        }
        this.setIframeVisible(iframe, visible);
      }
    });
  },

  /**
   * Make a privileged iframe visible/hidden.
   *
   * For now, XUL Iframes loading chrome documents (i.e. <iframe type!="content" />)
   * can't be hidden at platform level. And so don't support 'visibilitychange' event.
   *
   * This helper workarounds that by at least being able to send these kind of events.
   * It will help panel react differently depending on them being displayed or in
   * background.
   */
  setIframeVisible: function(iframe, visible) {
    const state = visible ? "visible" : "hidden";
    const win = iframe.contentWindow;
    const doc = win.document;
    if (doc.visibilityState != state) {
      // 1) Overload document's `visibilityState` attribute
      // Use defineProperty, as by default `document.visbilityState` is read only.
      Object.defineProperty(doc, "visibilityState", {
        value: state,
        configurable: true,
      });

      // 2) Fake the 'visibilitychange' event
      doc.dispatchEvent(new win.Event("visibilitychange"));
    }
  },

  /**
   * Switch to the tool with the given id
   *
   * @param {string} id
   *        The id of the tool to switch to
   * @param {string} reason
   *        Reason the tool was opened
   * @param {Object} options
   *        Object that will be passed to the panel
   */
  selectTool: function(id, reason = "unknown", options) {
    this.emit("panel-changed");

    if (this.currentToolId == id) {
      const panel = this._toolPanels.get(id);
      if (panel) {
        // We have a panel instance, so the tool is already fully loaded.

        // re-focus tool to get key events again
        this.focusTool(id);

        // Return the existing panel in order to have a consistent return value.
        return Promise.resolve(panel);
      }
      // Otherwise, if there is no panel instance, it is still loading,
      // so we are racing another call to selectTool with the same id.
      return this.once("select").then(() =>
        Promise.resolve(this._toolPanels.get(id))
      );
    }

    if (!this.isReady) {
      throw new Error("Can't select tool, wait for toolbox 'ready' event");
    }

    // Check if the tool exists.
    if (
      this.panelDefinitions.find(definition => definition.id === id) ||
      id === "options" ||
      this.additionalToolDefinitions.get(id)
    ) {
      if (this.currentToolId) {
        this.telemetry.toolClosed(this.currentToolId, this.sessionId, this);
      }

      this._pingTelemetrySelectTool(id, reason);
    } else {
      throw new Error("No tool found");
    }

    // and select the right iframe
    const toolboxPanels = this.doc.querySelectorAll(".toolbox-panel");
    this.selectSingleNode(toolboxPanels, "toolbox-panel-" + id);

    this.lastUsedToolId = this.currentToolId;
    this.currentToolId = id;
    this._refreshConsoleDisplay();
    if (id != "options") {
      Services.prefs.setCharPref(this._prefs.LAST_TOOL, id);
    }

    return this.loadTool(id, options).then(panel => {
      // focus the tool's frame to start receiving key events
      this.focusTool(id);

      this.emit("select", id);
      this.emit(id + "-selected", panel);
      return panel;
    });
  },

  _pingTelemetrySelectTool(id, reason) {
    const width = Math.ceil(this.win.outerWidth / 50) * 50;
    const panelName = this.getTelemetryPanelNameOrOther(id);
    const prevPanelName = this.getTelemetryPanelNameOrOther(this.currentToolId);
    const cold = !this.getPanel(id);
    const pending = [
      "host",
      "width",
      "start_state",
      "panel_name",
      "cold",
      "session_id",
    ];

    // On first load this.currentToolId === undefined so we need to skip sending
    // a devtools.main.exit telemetry event.
    if (this.currentToolId) {
      this.telemetry.recordEvent("exit", prevPanelName, null, {
        host: this._hostType,
        width: width,
        panel_name: prevPanelName,
        next_panel: panelName,
        reason: reason,
        session_id: this.sessionId,
      });
    }

    this.telemetry.addEventProperties(this.topWindow, "open", "tools", null, {
      width: width,
      session_id: this.sessionId,
    });

    if (id === "webconsole") {
      pending.push("message_count");
    }

    this.telemetry.preparePendingEvent(this, "enter", panelName, null, pending);

    this.telemetry.addEventProperties(this, "enter", panelName, null, {
      host: this._hostType,
      start_state: reason,
      panel_name: panelName,
      cold: cold,
      session_id: this.sessionId,
    });

    if (reason !== "initial_panel") {
      const width = Math.ceil(this.win.outerWidth / 50) * 50;
      this.telemetry.addEventProperty(
        this,
        "enter",
        panelName,
        null,
        "width",
        width
      );
    }

    // Cold webconsole event message_count is handled in
    // devtools/client/webconsole/webconsole-wrapper.js
    if (!cold && id === "webconsole") {
      this.telemetry.addEventProperty(
        this,
        "enter",
        "webconsole",
        null,
        "message_count",
        0
      );
    }

    this.telemetry.toolOpened(id, this.sessionId, this);
  },

  /**
   * Focus a tool's panel by id
   * @param  {string} id
   *         The id of tool to focus
   */
  focusTool: function(id, state = true) {
    const iframe = this.doc.getElementById("toolbox-panel-iframe-" + id);

    if (state) {
      iframe.focus();
    } else {
      iframe.blur();
    }
  },

  /**
   * Focus split console's input line
   */
  focusConsoleInput: function() {
    const consolePanel = this.getPanel("webconsole");
    if (consolePanel) {
      consolePanel.focusInput();
    }
  },

  /**
   * If the console is split and we are focusing an element outside
   * of the console, then store the newly focused element, so that
   * it can be restored once the split console closes.
   */
  _onFocus: function({ originalTarget }) {
    // Ignore any non element nodes, or any elements contained
    // within the webconsole frame.
    const webconsoleURL = gDevTools.getToolDefinition("webconsole").url;
    if (
      originalTarget.nodeType !== 1 ||
      originalTarget.baseURI === webconsoleURL
    ) {
      return;
    }

    this._lastFocusedElement = originalTarget;
  },

  _onTabsOrderUpdated: function() {
    this._combineAndSortPanelDefinitions();
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

    // Ensure split console is visible if console was already loaded in background
    const iframe = this.webconsolePanel.querySelector(".toolbox-panel-iframe");
    if (iframe) {
      this.setIframeVisible(iframe, true);
    }

    return this.loadTool("webconsole").then(() => {
      this.component.setIsSplitConsoleActive(true);
      this.telemetry.recordEvent("activate", "split_console", null, {
        host: this._getTelemetryHostString(),
        width: Math.ceil(this.win.outerWidth / 50) * 50,
        session_id: this.sessionId,
      });
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
  closeSplitConsole: function() {
    this._splitConsole = false;
    Services.prefs.setBoolPref(SPLITCONSOLE_ENABLED_PREF, false);
    this._refreshConsoleDisplay();
    this.component.setIsSplitConsoleActive(false);

    this.telemetry.recordEvent("deactivate", "split_console", null, {
      host: this._getTelemetryHostString(),
      width: Math.ceil(this.win.outerWidth / 50) * 50,
      session_id: this.sessionId,
    });

    this.emit("split-console");

    if (this._lastFocusedElement) {
      this._lastFocusedElement.focus();
    }
    return Promise.resolve();
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
      return this.splitConsole
        ? this.closeSplitConsole()
        : this.openSplitConsole();
    }

    return Promise.resolve();
  },

  /**
   * Toggles the options panel.
   * If the option panel is already selected then select the last selected panel.
   */
  toggleOptions: function(event) {
    // Flip back to the last used panel if we are already
    // on the options panel.
    if (
      this.currentToolId === "options" &&
      gDevTools.getToolDefinition(this.lastUsedToolId)
    ) {
      this.selectTool(this.lastUsedToolId, "toggle_settings_off");
    } else {
      this.selectTool("options", "toggle_settings_on");
    }

    // preventDefault will avoid a Linux only bug when the focus is on a text input
    // See Bug 1519087.
    event.preventDefault();
  },

  /**
   * Loads the tool next to the currently selected tool.
   */
  selectNextTool: function() {
    const definitions = this.component.panelDefinitions;
    const index = definitions.findIndex(({ id }) => id === this.currentToolId);
    const definition =
      index === -1 || index >= definitions.length - 1
        ? definitions[0]
        : definitions[index + 1];
    return this.selectTool(definition.id, "select_next_key");
  },

  /**
   * Loads the tool just left to the currently selected tool.
   */
  selectPreviousTool: function() {
    const definitions = this.component.panelDefinitions;
    const index = definitions.findIndex(({ id }) => id === this.currentToolId);
    const definition =
      index === -1 || index < 1
        ? definitions[definitions.length - 1]
        : definitions[index - 1];
    return this.selectTool(definition.id, "select_prev_key");
  },

  /**
   * Tells if the given tool is currently highlighted.
   * (doesn't mean selected, its tab header will be green)
   *
   * @param {string} id
   *        The id of the tool to check.
   */
  isHighlighted(id) {
    return this.component.state.highlightedTools.has(id);
  },

  /**
   * Highlights the tool's tab if it is not the currently selected tool.
   *
   * @param {string} id
   *        The id of the tool to highlight
   */
  async highlightTool(id) {
    if (!this.component) {
      await this.isOpen;
    }
    this.component.highlightTool(id);
  },

  /**
   * De-highlights the tool's tab.
   *
   * @param {string} id
   *        The id of the tool to unhighlight
   */
  async unhighlightTool(id) {
    if (!this.component) {
      await this.isOpen;
    }
    this.component.unhighlightTool(id);
  },

  /**
   * Raise the toolbox host.
   */
  raise: function() {
    this.postMessage({ name: "raise-host" });
  },

  /**
   * Fired when user just started navigating away to another web page.
   */
  async _onWillNavigate({ isFrameSwitching } = {}) {
    // On navigate, the server will resume all paused threads, but due to an
    // issue which can cause loosing outgoing messages/RDP packets, the THREAD_STATE
    // resources for the resumed state might not get received. So let assume it happens
    // make use the UI is the appropriate state.
    if (this._pausedTargets > 0) {
      this.emit("toolbox-resumed");
      this._pausedTargets = 0;
      if (this.isHighlighted("jsdebugger")) {
        this.unhighlightTool("jsdebugger");
      }
    }

    // Clearing the error count and the iframe list as soon as we navigate
    this.setErrorCount(0);
    if (!isFrameSwitching) {
      this._updateFrames({ destroyAll: true });
    }
    this.updateToolboxButtons();
    const toolId = this.currentToolId;
    // For now, only inspector, webconsole, netmonitor and accessibility fire "reloaded" event
    if (
      toolId != "inspector" &&
      toolId != "webconsole" &&
      toolId != "netmonitor" &&
      toolId != "accessibility"
    ) {
      return;
    }

    const start = this.win.performance.now();
    const panel = this.getPanel(toolId);
    // Ignore the timing if the panel is still loading
    if (!panel) {
      return;
    }

    await panel.once("reloaded");
    // The toolbox may have been destroyed while the panel was reloading
    if (this.isDestroying()) {
      return;
    }
    const delay = this.win.performance.now() - start;

    const telemetryKey = "DEVTOOLS_TOOLBOX_PAGE_RELOAD_DELAY_MS";
    this.telemetry.getKeyedHistogramById(telemetryKey).add(toolId, delay);
  },

  /**
   * Refresh the host's title.
   */
  _refreshHostTitle: function() {
    let title;

    const isMultiProcessBrowserToolbox =
      this.isBrowserToolbox &&
      Services.prefs.getBoolPref("devtools.browsertoolbox.fission", false);

    if (this.target.isXpcShellTarget) {
      // This will only be displayed for local development and can remain
      // hardcoded in english.
      title = "XPCShell Toolbox";
    } else if (isMultiProcessBrowserToolbox) {
      const scope = Services.prefs.getCharPref(BROWSERTOOLBOX_SCOPE_PREF);
      if (scope == BROWSERTOOLBOX_SCOPE_EVERYTHING) {
        title = L10N.getStr("toolbox.multiProcessBrowserToolboxTitle");
      } else if (scope == BROWSERTOOLBOX_SCOPE_PARENTPROCESS) {
        title = L10N.getStr("toolbox.parentProcessBrowserToolboxTitle");
      } else {
        throw new Error("Unsupported scope: " + scope);
      }
    } else if (this.target.name && this.target.name != this.target.url) {
      const url = this.target.isWebExtension
        ? this.target.getExtensionPathName(this.target.url)
        : getUnicodeUrl(this.target.url);
      title = L10N.getFormatStr(
        "toolbox.titleTemplate2",
        this.target.name,
        url
      );
    } else {
      title = L10N.getFormatStr(
        "toolbox.titleTemplate1",
        getUnicodeUrl(this.target.url)
      );
    }
    this.postMessage({
      name: "set-host-title",
      title,
    });
  },

  /**
   * Returns an instance of the preference actor. This is a lazily initialized root
   * actor that persists preferences to the debuggee, instead of just to the DevTools
   * client. See the definition of the preference actor for more information.
   */
  get preferenceFront() {
    if (!this._preferenceFrontRequest) {
      // Set the _preferenceFrontRequest property to allow the resetPreference toolbox
      // method to cleanup the preference set when the toolbox is closed.
      this._preferenceFrontRequest = this.commands.client.mainRoot.getFront(
        "preference"
      );
    }
    return this._preferenceFrontRequest;
  },

  // The auto-hide of pop-ups feature and pseudo-localization require targeting
  // browser chrome.
  get isBrowserChromeTarget() {
    return this.target.chrome;
  },

  /**
   * See: https://firefox-source-docs.mozilla.org/l10n/fluent/tutorial.html#manually-testing-ui-with-pseudolocalization
   *
   * @param {"bidi" | "accented" | "none"} pseudoLocale
   */
  async changePseudoLocale(pseudoLocale) {
    await this.isOpen;
    const prefFront = await this.preferenceFront;
    if (pseudoLocale === "none") {
      await prefFront.clearUserPref(PSEUDO_LOCALE_PREF);
    } else {
      await prefFront.setCharPref(PSEUDO_LOCALE_PREF, pseudoLocale);
    }
    this.component.setPseudoLocale(pseudoLocale);
    this._pseudoLocaleChanged = true;
  },

  /**
   * Returns the pseudo-locale when the target is browser chrome, otherwise undefined.
   *
   * @returns {"bidi" | "accented" | "none" | undefined}
   */
  async getPseudoLocale() {
    // Ensure that the tools are open and the feature is available in this
    // context.
    await this.isOpen;
    if (!this.isBrowserChromeTarget) {
      return undefined;
    }

    const prefFront = await this.preferenceFront;
    const locale = await prefFront.getCharPref(PSEUDO_LOCALE_PREF);

    switch (locale) {
      case "bidi":
      case "accented":
        return locale;
      default:
        return "none";
    }
  },

  async toggleNoAutohide() {
    const front = await this.preferenceFront;

    const toggledValue = !(await this._isDisableAutohideEnabled());

    front.setBoolPref(DISABLE_AUTOHIDE_PREF, toggledValue);

    if (this.isBrowserChromeTarget) {
      this.component.setDisableAutohide(toggledValue);
    }
    this._autohideHasBeenToggled = true;
  },

  async _isDisableAutohideEnabled() {
    // Ensure that the tools are open and the feature is available in this
    // context.
    await this.isOpen;
    if (!this.isBrowserChromeTarget) {
      return false;
    }

    const prefFront = await this.preferenceFront;
    return prefFront.getBoolPref(DISABLE_AUTOHIDE_PREF);
  },

  _listFrames: async function(event) {
    if (
      !this.target.getTrait("frames") ||
      this.target.targetForm.ignoreSubFrames
    ) {
      // We are not targetting a regular WindowGlobalTargetActor (it can be either an
      // addon or browser toolbox actor), or EFT is enabled.
      return Promise.resolve();
    }

    try {
      const { frames } = await this.target.listFrames();
      this._updateFrames({ frames });
    } catch (e) {
      console.error("Error while listing frames", e);
    }
  },

  /**
   * Called by the iframe picker when the user selected a frame.
   *
   * @param {String} frameIdOrTargetActorId
   */
  onIframePickerFrameSelected: function(frameIdOrTargetActorId) {
    if (!this.frameMap.has(frameIdOrTargetActorId)) {
      console.error(
        `Can't focus on frame "${frameIdOrTargetActorId}", it is not a known frame`
      );
      return;
    }

    const frameInfo = this.frameMap.get(frameIdOrTargetActorId);
    // If there is no targetFront in the frameData, this means EFT is not enabled.
    // Send packet to the backend to select specified frame and  wait for 'frameUpdate'
    // event packet to update the UI.
    if (!frameInfo.targetFront) {
      this.target.switchToFrame({ windowId: frameIdOrTargetActorId });
      return;
    }

    // Here, EFT is enabled, so we want to focus the toolbox on the specific targetFront
    // that was selected by the user. This will trigger this._onTargetSelected which will
    // take care of updating the iframe picker state.
    this.commands.targetCommand.selectTarget(frameInfo.targetFront);
  },

  /**
   * Highlight a frame in the page
   *
   * @param {String} frameIdOrTargetActorId
   */
  onHighlightFrame: async function(frameIdOrTargetActorId) {
    // Only enable frame highlighting when the top level document is targeted
    if (!this.rootFrameSelected) {
      return;
    }

    const frameInfo = this.frameMap.get(frameIdOrTargetActorId);
    if (!frameInfo) {
      return;
    }

    let nodeFront;
    if (frameInfo.targetFront) {
      const inspectorFront = await frameInfo.targetFront.getFront("inspector");
      nodeFront = await inspectorFront.walker.documentElement();
    } else {
      const inspectorFront = await this.target.getFront("inspector");
      nodeFront = await inspectorFront.walker.getNodeActorFromWindowID(
        frameIdOrTargetActorId
      );
    }
    const highlighter = this.getHighlighter();
    return highlighter.highlight(nodeFront);
  },

  /**
   * Handles changes in document frames.
   *
   * @param {Object} data
   * @param {Boolean} data.destroyAll: All frames have been destroyed.
   * @param {Number} data.selected: A frame has been selected
   * @param {Object} data.frameData: Some frame data were updated
   * @param {String} data.frameData.url: new frame URL (it might have been blank or about:blank)
   * @param {String} data.frameData.title: new frame title
   * @param {Number|String} data.frameData.id: frame ID / targetFront actorID when EFT is enabled.
   * @param {Array<Object>} data.frames: List of frames. Every frame can have:
   * @param {Number|String} data.frames[].id: frame ID / targetFront actorID when EFT is enabled.
   * @param {String} data.frames[].url: frame URL
   * @param {String} data.frames[].title: frame title
   * @param {Boolean} data.frames[].destroy: Set to true if destroyed
   * @param {Boolean} data.frames[].isTopLevel: true for top level window
   */
  _updateFrames: function(data) {
    // At the moment, frames `id` can either be outerWindowID (a Number),
    // or a targetActorID (a String).
    // In order to have the same type of data as a key of `frameMap`, we transform any
    // outerWindowID into a string.
    // This can be removed once EFT is enabled by default
    if (data.selected) {
      data.selected = data.selected.toString();
    } else if (data.frameData) {
      data.frameData.id = data.frameData.id.toString();
    } else if (data.frames) {
      data.frames.forEach(frame => {
        if (frame.id) {
          frame.id = frame.id.toString();
        }
      });
    }

    // Store (synchronize) data about all existing frames on the backend
    if (data.destroyAll) {
      this.frameMap.clear();
      this.selectedFrameId = null;
    } else if (data.selected) {
      // If we select the top level target, default back to no particular selected document.
      if (data.selected == this.target.actorID) {
        this.selectedFrameId = null;
      } else {
        this.selectedFrameId = data.selected;
      }
    } else if (data.frameData && this.frameMap.has(data.frameData.id)) {
      const existingFrameData = this.frameMap.get(data.frameData.id);
      if (
        existingFrameData.title == data.frameData.title &&
        existingFrameData.url == data.frameData.url
      ) {
        return;
      }

      this.frameMap.set(data.frameData.id, {
        ...existingFrameData,
        url: data.frameData.url,
        title: data.frameData.title,
      });
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
      const frames = [...this.frameMap.values()];
      const topFrames = frames.filter(frame => frame.isTopLevel);
      this.selectedFrameId = topFrames.length ? topFrames[0].id : null;
    }

    // Debounce the update to avoid unnecessary flickering/rendering.
    if (!this.debouncedToolbarUpdate) {
      this.debouncedToolbarUpdate = debounce(
        () => {
          // Toolbox may have been destroyed in the meantime
          if (this.component) {
            this.component.setToolboxButtons(this.toolbarButtons);
          }
          this.debouncedToolbarUpdate = null;
        },
        200,
        this
      );
    }

    const updateUiElements = () => {
      // We may need to hide/show the frames button now.
      this.updateFrameButton();

      if (this.debouncedToolbarUpdate) {
        this.debouncedToolbarUpdate();
      }
    };

    // This may have been called before the toolbox is ready (= the dom elements for
    // the iframe picker don't exist yet).
    if (!this.isReady) {
      this.once("ready").then(() => updateUiElements);
    } else {
      updateUiElements();
    }
  },

  /**
   * Returns whether a root frame (with no parent frame) is selected.
   */
  get rootFrameSelected() {
    // If the frame switcher is disabled, we won't have a selected frame ID.
    // In this case, we're always showing the root frame.
    if (!this.selectedFrameId) {
      return true;
    }

    return this.frameMap.get(this.selectedFrameId).isTopLevel;
  },

  /**
   * Switch to the last used host for the toolbox UI.
   */
  switchToPreviousHost: function() {
    return this.switchHost("previous");
  },

  /**
   * Switch to a new host for the toolbox UI. E.g. bottom, sidebar, window,
   * and focus the window when done.
   *
   * @param {string} hostType
   *        The host type of the new host object
   */
  switchHost: function(hostType) {
    if (hostType == this.hostType || !this.descriptorFront.isLocalTab) {
      return null;
    }

    // chromeEventHandler will change after swapping hosts, remove events relying on it.
    this._removeChromeEventHandlerEvents();

    this.emit("host-will-change", hostType);

    // ToolboxHostManager is going to call swapFrameLoaders which mess up with
    // focus. We have to blur before calling it in order to be able to restore
    // the focus after, in _onSwitchedHost.
    this.focusTool(this.currentToolId, false);

    // Host code on the chrome side will send back a message once the host
    // switched
    this.postMessage({
      name: "switch-host",
      hostType,
    });

    return this.once("host-changed");
  },

  /**
   * Request to Firefox UI to move the toolbox to another tab.
   * This is used when we move a toolbox to a new popup opened by the tab we were currently debugging.
   * We also move the toolbox back to the original tab we were debugging if we select it via Firefox tabs.
   *
   * @param {String} tabBrowsingContextID
   *        The BrowsingContext ID of the tab we want to move to.
   * @returns {Promise<undefined>}
   *        This will resolve only once we moved to the new tab.
   */
  switchHostToTab(tabBrowsingContextID) {
    this.postMessage({
      name: "switch-host-to-tab",
      tabBrowsingContextID,
    });

    return this.once("switched-host-to-tab");
  },

  _onSwitchedHost: function({ hostType }) {
    this._hostType = hostType;

    this._buildDockOptions();

    // chromeEventHandler changed after swapping hosts, add again events relying on it.
    this._addChromeEventHandlerEvents();

    // We blurred the tools at start of switchHost, but also when clicking on
    // host switching button. We now have to restore the focus.
    this.focusTool(this.currentToolId, true);

    this.emit("host-changed");
    this.telemetry
      .getHistogramById(HOST_HISTOGRAM)
      .add(this._getTelemetryHostId());

    this.component.setCurrentHostType(hostType);
  },

  /**
   * Event handler fired when the toolbox was moved to another tab.
   * This fires when the toolbox itself requests to be moved to another tab,
   * but also when we select the original tab where the toolbox originally was.
   *
   * @param {String} browsingContextID
   *        The BrowsingContext ID of the tab the toolbox has been moved to.
   */
  _onSwitchedHostToTab(browsingContextID) {
    const targets = this.commands.targetCommand.getAllTargets([
      this.commands.targetCommand.TYPES.FRAME,
    ]);
    const target = targets.find(
      target => target.browsingContextID == browsingContextID
    );

    this.commands.targetCommand.selectTarget(target);

    this.emit("switched-host-to-tab");
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
  isToolRegistered: function(toolId) {
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
  getToolDefinition: function(toolId) {
    return (
      gDevTools.getToolDefinition(toolId) ||
      this.additionalToolDefinitions.get(toolId)
    );
  },

  /**
   * Internal helper that removes a loaded tool from the toolbox,
   * it removes a loaded tool panel and tab from the toolbox without removing
   * its definition, so that it can still be listed in options and re-added later.
   *
   * @param  {string} toolId
   *         Id of the tool to be removed.
   */
  unloadTool: function(toolId) {
    if (typeof toolId != "string") {
      throw new Error("Unexpected non-string toolId received.");
    }

    if (this._toolPanels.has(toolId)) {
      const instance = this._toolPanels.get(toolId);
      instance.destroy();
      this._toolPanels.delete(toolId);
    }

    const panel = this.doc.getElementById("toolbox-panel-" + toolId);

    // Select another tool.
    if (this.currentToolId == toolId) {
      const index = this.panelDefinitions.findIndex(({ id }) => id === toolId);
      const nextTool = this.panelDefinitions[index + 1];
      const previousTool = this.panelDefinitions[index - 1];
      let toolNameToSelect;

      if (nextTool) {
        toolNameToSelect = nextTool.id;
      }
      if (previousTool) {
        toolNameToSelect = previousTool.id;
      }
      if (toolNameToSelect) {
        this.selectTool(toolNameToSelect, "tool_unloaded");
      }
    }

    // Remove this tool from the current panel definitions.
    this.panelDefinitions = this.panelDefinitions.filter(
      ({ id }) => id !== toolId
    );
    this.visibleAdditionalTools = this.visibleAdditionalTools.filter(
      id => id !== toolId
    );
    this._combineAndSortPanelDefinitions();

    if (panel) {
      panel.remove();
    }

    if (this.hostType == Toolbox.HostType.WINDOW) {
      const doc = this.win.parent.document;
      const key = doc.getElementById("key_" + toolId);
      if (key) {
        key.remove();
      }
    }
  },

  /**
   * Handler for the tool-registered event.
   * @param  {string} toolId
   *         Id of the tool that was registered
   */
  _toolRegistered: function(toolId) {
    // Tools can either be in the global devtools, or added to this specific toolbox
    // as an additional tool.
    let definition = gDevTools.getToolDefinition(toolId);
    let isAdditionalTool = false;
    if (!definition) {
      definition = this.additionalToolDefinitions.get(toolId);
      isAdditionalTool = true;
    }

    if (definition.isToolSupported(this)) {
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
   * @param  {string} toolId
   *         id of the tool that was unregistered
   */
  _toolUnregistered: function(toolId) {
    this.unloadTool(toolId);

    // Emit the event so tools can listen to it from the toolbox level
    // instead of gDevTools
    this.emit("tool-unregistered", toolId);
  },

  /**
   * A helper function that returns an object containing methods to show and hide the
   * Box Model Highlighter on a given NodeFront or node grip (object with metadata which
   * can be used to obtain a NodeFront for a node), as well as helpers to listen to the
   * higligher show and hide events. The event helpers are used in tests where it is
   * cumbersome to load the Inspector panel in order to listen to highlighter events.
   *
   * @returns {Object} an object of the following shape:
   *   - {AsyncFunction} highlight: A function that will show a Box Model Highlighter
   *                     for the provided NodeFront or node grip.
   *   - {AsyncFunction} unhighlight: A function that will hide any Box Model Highlighter
   *                     that is visible. If the `highlight` promise isn't settled yet,
   *                     it will wait until it's done and then unhighlight to prevent
   *                     zombie highlighters.
   *   - {AsyncFunction} waitForHighlighterShown: Returns a promise which resolves with
   *                     the "highlighter-shown" event data once the highlighter is shown.
   *   - {AsyncFunction} waitForHighlighterHidden: Returns a promise which resolves with
   *                     the "highlighter-hidden" event data once the highlighter is
   *                     hidden.
   *
   */
  getHighlighter() {
    let pendingHighlight;

    /**
     * Return a promise wich resolves with a reference to the Inspector panel.
     */
    const _getInspector = async () => {
      const inspector = this.getPanel("inspector");
      if (inspector) {
        return inspector;
      }

      return this.loadTool("inspector");
    };

    /**
     * Returns a promise which resolves when a Box Model Highlighter emits the given event
     *
     * @param  {String} eventName
     *         Name of the event to listen to.
     * @return {Promise}
     *         Promise which resolves when the highlighter event occurs.
     *         Resolves with the data payload attached to the event.
     */
    async function _waitForHighlighterEvent(eventName) {
      const inspector = await _getInspector();
      return new Promise(resolve => {
        function _handler(data) {
          if (data.type === inspector.highlighters.TYPES.BOXMODEL) {
            inspector.highlighters.off(eventName, _handler);
            resolve(data);
          }
        }

        inspector.highlighters.on(eventName, _handler);
      });
    }

    return {
      // highlight might be triggered right before a test finishes. Wrap it
      // with safeAsyncMethod to avoid intermittents.
      highlight: this._safeAsyncAfterDestroy(async (object, options) => {
        pendingHighlight = (async () => {
          let nodeFront = object;

          if (!(nodeFront instanceof NodeFront)) {
            const inspectorFront = await this.target.getFront("inspector");
            nodeFront = await inspectorFront.getNodeFrontFromNodeGrip(object);
          }

          if (!nodeFront) {
            return null;
          }

          const inspector = await _getInspector();
          return inspector.highlighters.showHighlighterTypeForNode(
            inspector.highlighters.TYPES.BOXMODEL,
            nodeFront,
            options
          );
        })();
        return pendingHighlight;
      }),
      unhighlight: this._safeAsyncAfterDestroy(async () => {
        if (pendingHighlight) {
          await pendingHighlight;
          pendingHighlight = null;
        }

        const inspector = await _getInspector();
        return inspector.highlighters.hideHighlighterType(
          inspector.highlighters.TYPES.BOXMODEL
        );
      }),

      waitForHighlighterShown: this._safeAsyncAfterDestroy(async () => {
        return _waitForHighlighterEvent("highlighter-shown");
      }),

      waitForHighlighterHidden: this._safeAsyncAfterDestroy(async () => {
        return _waitForHighlighterEvent("highlighter-hidden");
      }),
    };
  },

  /**
   * Shortcut to avoid throwing errors when an async method fails after toolbox
   * destroy. Should be used with methods that might be triggered by a user
   * input, regardless of the toolbox lifecycle.
   */
  _safeAsyncAfterDestroy(fn) {
    return safeAsyncMethod(fn, () => !!this._destroyer);
  },

  _onNewSelectedNodeFront: async function() {
    // Emit a "selection-changed" event when the toolbox.selection has been set
    // to a new node (or cleared). Currently used in the WebExtensions APIs (to
    // provide the `devtools.panels.elements.onSelectionChanged` event).
    this.emit("selection-changed");

    const targetFrontActorID = this.selection?.nodeFront?.targetFront?.actorID;
    if (targetFrontActorID) {
      this.selectTarget(targetFrontActorID);
    }
  },

  _onToolSelected: function() {
    this._refreshHostTitle();

    this.updatePickerButton();
    this.updateFrameButton();
    this.updateErrorCountButton();

    // Calling setToolboxButtons in case the visibility of a button changed.
    this.component.setToolboxButtons(this.toolbarButtons);
  },

  /**
   * Listener for "inspectObject" event on console top level target actor.
   */
  _onInspectObject(packet) {
    this.inspectObjectActor(packet.objectActor, packet.inspectFromAnnotation);
  },

  inspectObjectActor: async function(objectActor, inspectFromAnnotation) {
    const objectGrip = objectActor?.getGrip
      ? objectActor.getGrip()
      : objectActor;

    if (
      objectGrip.preview &&
      objectGrip.preview.nodeType === domNodeConstants.ELEMENT_NODE
    ) {
      return this.viewElementInInspector(objectGrip, inspectFromAnnotation);
    }

    if (objectGrip.class == "Function") {
      if (!objectGrip.location) {
        console.error("Missing location in Function objectGrip", objectGrip);
        return;
      }

      const { url, line, column } = objectGrip.location;
      return this.viewSourceInDebugger(url, line, column);
    }

    if (objectGrip.type !== "null" && objectGrip.type !== "undefined") {
      // Open then split console and inspect the object in the variables view,
      // when the objectActor doesn't represent an undefined or null value.
      if (this.currentToolId != "webconsole") {
        await this.openSplitConsole();
      }

      const panel = this.getPanel("webconsole");
      panel.hud.ui.inspectObjectActor(objectActor);
    }
  },

  /**
   * Get the toolbox's notification component
   *
   * @return The notification box component.
   */
  getNotificationBox: function() {
    return this.notificationBox;
  },

  closeToolbox: async function() {
    await this.destroy();
  },

  /**
   * Public API to check is the current toolbox is currently being destroyed.
   */
  isDestroying: function() {
    return this._destroyer;
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

    // This pattern allows to immediately return the destroyer promise.
    // See Bug 1602727 for more details.
    let destroyerResolve;
    this._destroyer = new Promise(r => (destroyerResolve = r));
    this._destroyToolbox().then(destroyerResolve);

    return this._destroyer;
  },

  _destroyToolbox: async function() {
    this.emit("destroy");

    // This flag will be checked by Fronts in order to decide if they should
    // skip their destroy.
    this.commands.client.isToolboxDestroy = true;

    this.off("select", this._onToolSelected);
    this.off("host-changed", this._refreshHostTitle);

    gDevTools.off("tool-registered", this._toolRegistered);
    gDevTools.off("tool-unregistered", this._toolUnregistered);

    Services.prefs.removeObserver(
      "devtools.cache.disabled",
      this._applyCacheSettings
    );
    Services.prefs.removeObserver(
      "devtools.serviceWorkers.testing.enabled",
      this._applyServiceWorkersTestingSettings
    );
    Services.prefs.removeObserver(
      BROWSERTOOLBOX_SCOPE_PREF,
      this._refreshHostTitle
    );

    // We normally handle toolClosed from selectTool() but in the event of the
    // toolbox closing we need to handle it here instead.
    this.telemetry.toolClosed(this.currentToolId, this.sessionId, this);

    this._lastFocusedElement = null;
    this._pausedTargets = null;

    if (this._sourceMapService) {
      this._sourceMapService.stopSourceMapWorker();
      this._sourceMapService = null;
    }

    if (this._parserService) {
      this._parserService.stop();
      this._parserService = null;
    }

    if (this.webconsolePanel) {
      this._saveSplitConsoleHeight();
      this.webconsolePanel.removeEventListener(
        "resize",
        this._saveSplitConsoleHeight
      );
      this.webconsolePanel = null;
    }
    if (this._componentMount) {
      this._tabBar.removeEventListener(
        "keypress",
        this._onToolbarArrowKeypress
      );
      this.ReactDOM.unmountComponentAtNode(this._componentMount);
      this.component = null;
      this._componentMount = null;
      this._tabBar = null;
    }
    this.destroyHarAutomation();

    for (const [id, panel] of this._toolPanels) {
      try {
        gDevTools.emit(id + "-destroy", this, panel);
        this.emit(id + "-destroy", panel);

        const rv = panel.destroy();
        if (rv) {
          console.error(
            `Panel ${id}'s destroy method returned something whereas it shouldn't (and should be synchronous).`
          );
        }
      } catch (e) {
        // We don't want to stop here if any panel fail to close.
        console.error("Panel " + id + ":", e);
      }
    }

    this.browserRequire = null;
    this._toolNames = null;

    // Reset preferences set by the toolbox, then remove the preference front.
    const onResetPreference = this.resetPreference().then(() => {
      this._preferenceFrontRequest = null;
    });

    this.commands.targetCommand.unwatchTargets({
      types: this.commands.targetCommand.ALL_TYPES,
      onAvailable: this._onTargetAvailable,
      onSelected: this._onTargetSelected,
      onDestroyed: this._onTargetDestroyed,
    });
    this.resourceCommand.unwatchResources(
      [
        this.resourceCommand.TYPES.CONSOLE_MESSAGE,
        this.resourceCommand.TYPES.ERROR_MESSAGE,
        this.resourceCommand.TYPES.NETWORK_EVENT,
        this.resourceCommand.TYPES.DOCUMENT_EVENT,
        this.resourceCommand.TYPES.THREAD_STATE,
      ],
      {
        onAvailable: this._onResourceAvailable,
      }
    );

    // Unregister buttons listeners
    this.toolbarButtons.forEach(button => {
      if (typeof button.teardown == "function") {
        // teardown arguments have already been bound in _createButtonState
        button.teardown();
      }
    });

    // We need to grab a reference to win before this._host is destroyed.
    const win = this.win;
    const host = this._getTelemetryHostString();
    const width = Math.ceil(win.outerWidth / 50) * 50;
    const prevPanelName = this.getTelemetryPanelNameOrOther(this.currentToolId);

    this.telemetry.toolClosed("toolbox", this.sessionId, this);
    this.telemetry.recordEvent("exit", prevPanelName, null, {
      host: host,
      width: width,
      panel_name: this.getTelemetryPanelNameOrOther(this.currentToolId),
      next_panel: "none",
      reason: "toolbox_close",
      session_id: this.sessionId,
    });
    this.telemetry.recordEvent("close", "tools", null, {
      host: host,
      width: width,
      session_id: this.sessionId,
    });

    // Wait for the preferences to be reset before destroying the target descriptor (which will destroy the preference front)
    const onceDestroyed = new Promise(resolve => {
      resolve(
        onResetPreference
          .catch(console.error)
          .then(async () => {
            // Destroy the node picker *after* destroying the panel,
            // which may still try to access it. (And might spawn a new one)
            if (this._nodePicker) {
              this._nodePicker.destroy();
              this._nodePicker = null;
            }
            this.selection.destroy();
            this.selection = null;

            if (this._netMonitorAPI) {
              this._netMonitorAPI.destroy();
              this._netMonitorAPI = null;
            }

            if (this._sourceMapURLService) {
              await this._sourceMapURLService.waitForSourcesLoading();
              this._sourceMapURLService.destroy();
              this._sourceMapURLService = null;
            }

            this._removeWindowListeners();
            this._removeChromeEventHandlerEvents();

            this._store = null;

            // Notify toolbox-host-manager that the host can be destroyed.
            this.emit("toolbox-unload");

            // targetCommand need to be notified that the toolbox is being torn down.
            // This is done after other destruction tasks since it may tear down
            // fronts and the debugger transport which earlier destroy methods may
            // require to complete.
            //
            // For similar reasons, only destroy the target-list after every
            // other outstanding cleanup is done. Destroying the target list
            // will lead to destroy frame targets which can temporarily make
            // some fronts unresponsive and block the cleanup.
            this.commands.targetCommand.destroy();
            return this.descriptorFront.destroy();
          }, console.error)
          .then(() => {
            this.emit("destroyed");

            // Free _host after the call to destroyed in order to let a chance
            // to destroyed listeners to still query toolbox attributes
            this._host = null;
            this._win = null;
            this._toolPanels.clear();
            this.descriptorFront = null;
            this.resourceCommand = null;
            this.commands = null;

            // Force GC to prevent long GC pauses when running tests and to free up
            // memory in general when the toolbox is closed.
            if (flags.testing) {
              win.windowUtils.garbageCollect();
            }
          })
          .catch(console.error)
      );
    });

    const leakCheckObserver = ({ wrappedJSObject: barrier }) => {
      // Make the leak detector wait until this toolbox is properly destroyed.
      barrier.client.addBlocker(
        "DevTools: Wait until toolbox is destroyed",
        onceDestroyed
      );
    };

    const topic = "shutdown-leaks-before-check";
    Services.obs.addObserver(leakCheckObserver, topic);

    await onceDestroyed;

    Services.obs.removeObserver(leakCheckObserver, topic);
  },

  /**
   * Open the textbox context menu at given coordinates.
   * Panels in the toolbox can call this on contextmenu events with event.screenX/Y
   * instead of having to implement their own copy/paste/selectAll menu.
   * @param {Number} x
   * @param {Number} y
   */
  openTextBoxContextMenu: function(x, y) {
    const menu = createEditContextMenu(this.topWindow, "toolbox-menu");

    // Fire event for tests
    menu.once("open", () => this.emit("menu-open"));
    menu.once("close", () => this.emit("menu-close"));

    menu.popup(x, y, this.doc);
  },

  /**
   *  Retrieve the current textbox context menu, if available.
   */
  getTextBoxContextMenu: function() {
    return this.topDoc.getElementById("toolbox-menu");
  },

  /**
   * Reset preferences set by the toolbox.
   */
  async resetPreference() {
    if (
      // No preferences have been changed, so there is nothing to reset.
      !this._preferenceFrontRequest ||
      // Did any pertinent prefs actually change? For autohide and the pseudo-locale,
      // only reset prefs in the Browser Toolbox if it's been toggled in the UI
      // (don't reset the pref if it was already set before opening)
      (!this._autohideHasBeenToggled && !this._pseudoLocaleChanged)
    ) {
      return;
    }

    const preferenceFront = await this.preferenceFront;

    if (this._autohideHasBeenToggled) {
      await preferenceFront.clearUserPref(DISABLE_AUTOHIDE_PREF);
    }
    if (this._pseudoLocaleChanged) {
      await preferenceFront.clearUserPref(PSEUDO_LOCALE_PREF);
    }
  },

  // HAR Automation

  async initHarAutomation() {
    const autoExport = Services.prefs.getBoolPref(
      "devtools.netmonitor.har.enableAutoExportToFile"
    );
    if (autoExport) {
      this.harAutomation = new HarAutomation();
      await this.harAutomation.initialize(this);
    }
  },
  destroyHarAutomation() {
    if (this.harAutomation) {
      this.harAutomation.destroy();
    }
  },

  /**
   * Returns gViewSourceUtils for viewing source.
   */
  get gViewSourceUtils() {
    return this.win.gViewSourceUtils;
  },

  /**
   * Open a CSS file when there is no line or column information available.
   *
   * @param {string} url The URL of the CSS file to open.
   */
  viewGeneratedSourceInStyleEditor: async function(url) {
    if (typeof url !== "string") {
      console.warn("Failed to open generated source, no url given");
      return;
    }

    // The style editor hides the generated file if the file has original
    // sources, so we have no choice but to open whichever original file
    // corresponds to the first line of the generated file.
    return viewSource.viewSourceInStyleEditor(this, url, 1);
  },

  /**
   * Given a URL for a stylesheet (generated or original), open in the style
   * editor if possible. Falls back to plain "view-source:".
   * If the stylesheet has a sourcemap, we will attempt to open the original
   * version of the file instead of the generated version.
   */
  viewSourceInStyleEditorByURL: async function(url, line, column) {
    if (typeof url !== "string") {
      console.warn("Failed to open source, no url given");
      return;
    }
    if (typeof line !== "number") {
      console.warn(
        "No line given when navigating to source. If you're seeing this, there is a bug."
      );

      // This is a fallback in case of programming errors, but in a perfect
      // world, viewSourceInStyleEditorByURL would always get a line/colum.
      line = 1;
      column = null;
    }

    return viewSource.viewSourceInStyleEditor(this, url, line, column);
  },

  /**
   * Opens source in style editor. Falls back to plain "view-source:".
   * If the stylesheet has a sourcemap, we will attempt to open the original
   * version of the file instead of the generated version.
   */
  viewSourceInStyleEditorByFront: async function(
    stylesheetFront,
    line,
    column
  ) {
    if (!stylesheetFront || typeof stylesheetFront !== "object") {
      console.warn("Failed to open source, no stylesheet given");
      return;
    }
    if (typeof line !== "number") {
      console.warn(
        "No line given when navigating to source. If you're seeing this, there is a bug."
      );

      // This is a fallback in case of programming errors, but in a perfect
      // world, viewSourceInStyleEditorByFront would always get a line/colum.
      line = 1;
      column = null;
    }

    return viewSource.viewSourceInStyleEditor(
      this,
      stylesheetFront,
      line,
      column
    );
  },

  viewElementInInspector: async function(objectGrip, reason) {
    // Open the inspector and select the DOM Element.
    await this.loadTool("inspector");
    const inspector = this.getPanel("inspector");
    const nodeFound = await inspector.inspectNodeActor(objectGrip, reason);
    if (nodeFound) {
      await this.selectTool("inspector", reason);
    }
  },

  /**
   * Open a JS file when there is no line or column information available.
   *
   * @param {string} url The URL of the JS file to open.
   */
  viewGeneratedSourceInDebugger: async function(url) {
    if (typeof url !== "string") {
      console.warn("Failed to open generated source, no url given");
      return;
    }

    return viewSource.viewSourceInDebugger(this, url, null, null, null, null);
  },

  /**
   * Opens source in debugger, the sourcemapped location will be selected in
   * the debugger panel, if the given location resolves to a know sourcemapped one.
   *
   * Falls back to plain "view-source:".
   *
   * @see devtools/client/shared/source-utils.js
   */
  viewSourceInDebugger: async function(
    sourceURL,
    sourceLine,
    sourceColumn,
    sourceId,
    reason
  ) {
    if (typeof sourceURL !== "string" && typeof sourceId !== "string") {
      console.warn("Failed to open generated source, no url/id given");
      return;
    }
    if (typeof sourceLine !== "number") {
      console.warn(
        "No line given when navigating to source. If you're seeing this, there is a bug."
      );

      // This is a fallback in case of programming errors, but in a perfect
      // world, viewSourceInDebugger would always get a line/colum.
      sourceLine = 1;
      sourceColumn = null;
    }

    return viewSource.viewSourceInDebugger(
      this,
      sourceURL,
      sourceLine,
      sourceColumn,
      sourceId,
      reason
    );
  },

  /**
   * Opens source in plain "view-source:".
   * @see devtools/client/shared/source-utils.js
   */
  viewSource: function(sourceURL, sourceLine) {
    return viewSource.viewSource(this, sourceURL, sourceLine);
  },

  // Support for WebExtensions API (`devtools.network.*`)

  /**
   * Return Netmonitor API object. This object offers Network monitor
   * public API that can be consumed by other panels or WE API.
   */
  getNetMonitorAPI: async function() {
    const netPanel = this.getPanel("netmonitor");

    // Return Net panel if it exists.
    if (netPanel) {
      return netPanel.panelWin.Netmonitor.api;
    }

    if (this._netMonitorAPI) {
      return this._netMonitorAPI;
    }

    // Create and initialize Network monitor API object.
    // This object is only connected to the backend - not to the UI.
    this._netMonitorAPI = new NetMonitorAPI();
    await this._netMonitorAPI.connect(this);

    return this._netMonitorAPI;
  },

  /**
   * Returns data (HAR) collected by the Network panel.
   */
  getHARFromNetMonitor: async function() {
    const netMonitor = await this.getNetMonitorAPI();
    let har = await netMonitor.getHar();

    // Return default empty HAR file if needed.
    har = har || buildHarLog(Services.appinfo);

    // Return the log directly to be compatible with
    // Chrome WebExtension API.
    return har.log;
  },

  /**
   * Add listener for `onRequestFinished` events.
   *
   * @param {Object} listener
   *        The listener to be called it's expected to be
   *        a function that takes ({harEntry, requestId})
   *        as first argument.
   */
  addRequestFinishedListener: async function(listener) {
    const netMonitor = await this.getNetMonitorAPI();
    netMonitor.addRequestFinishedListener(listener);
  },

  removeRequestFinishedListener: async function(listener) {
    const netMonitor = await this.getNetMonitorAPI();
    netMonitor.removeRequestFinishedListener(listener);

    // Destroy Network monitor API object if the following is true:
    // 1) there is no listener
    // 2) the Net panel doesn't exist/use the API object (if the panel
    //    exists it's also responsible for destroying it,
    //    see `NetMonitorPanel.open` for more details)
    const netPanel = this.getPanel("netmonitor");
    const hasListeners = netMonitor.hasRequestFinishedListeners();
    if (this._netMonitorAPI && !hasListeners && !netPanel) {
      this._netMonitorAPI.destroy();
      this._netMonitorAPI = null;
    }
  },

  /**
   * Used to lazily fetch HTTP response content within
   * `onRequestFinished` event listener.
   *
   * @param {String} requestId
   *        Id of the request for which the response content
   *        should be fetched.
   */
  fetchResponseContent: async function(requestId) {
    const netMonitor = await this.getNetMonitorAPI();
    return netMonitor.fetchResponseContent(requestId);
  },

  // Support management of installed WebExtensions that provide a devtools_page.

  /**
   * List the subset of the active WebExtensions which have a devtools_page (used by
   * toolbox-options.js to create the list of the tools provided by the enabled
   * WebExtensions).
   * @see devtools/client/framework/toolbox-options.js
   */
  listWebExtensions: function() {
    // Return the array of the enabled webextensions (we can't use the prefs list here,
    // because some of them may be disabled by the Addon Manager and still have a devtools
    // preference).
    return Array.from(this._webExtensions).map(([uuid, { name, pref }]) => {
      return { uuid, name, pref };
    });
  },

  /**
   * Add a WebExtension to the list of the active extensions (given the extension UUID,
   * a unique id assigned to an extension when it is installed, and its name),
   * and emit a "webextension-registered" event to allow toolbox-options.js
   * to refresh the listed tools accordingly.
   * @see browser/components/extensions/ext-devtools.js
   */
  registerWebExtension: function(extensionUUID, { name, pref }) {
    // Ensure that an installed extension (active in the AddonManager) which
    // provides a devtools page is going to be listed in the toolbox options
    // (and refresh its name if it was already listed).
    this._webExtensions.set(extensionUUID, { name, pref });
    this.emit("webextension-registered", extensionUUID);
  },

  /**
   * Remove an active WebExtension from the list of the active extensions (given the
   * extension UUID, a unique id assigned to an extension when it is installed, and its
   * name), and emit a "webextension-unregistered" event to allow toolbox-options.js
   * to refresh the listed tools accordingly.
   * @see browser/components/extensions/ext-devtools.js
   */
  unregisterWebExtension: function(extensionUUID) {
    // Ensure that an extension that has been disabled/uninstalled from the AddonManager
    // is going to be removed from the toolbox options.
    this._webExtensions.delete(extensionUUID);
    this.emit("webextension-unregistered", extensionUUID);
  },

  /**
   * A helper function which returns true if the extension with the given UUID is listed
   * as active for the toolbox and has its related devtools about:config preference set
   * to true.
   * @see browser/components/extensions/ext-devtools.js
   */
  isWebExtensionEnabled: function(extensionUUID) {
    const extInfo = this._webExtensions.get(extensionUUID);
    return extInfo && Services.prefs.getBoolPref(extInfo.pref, false);
  },

  /**
   * Returns a panel id in the case of built in panels or "other" in the case of
   * third party panels. This is necessary due to limitations in addon id strings,
   * the permitted length of event telemetry property values and what we actually
   * want to see in our telemetry.
   *
   * @param {String} id
   *        The panel id we would like to process.
   */
  getTelemetryPanelNameOrOther: function(id) {
    if (!this._toolNames) {
      const definitions = gDevTools.getToolDefinitionArray();
      const definitionIds = definitions.map(definition => definition.id);

      this._toolNames = new Set(definitionIds);
    }

    if (!this._toolNames.has(id)) {
      return "other";
    }

    return id;
  },

  /**
   * Sets basic information on the DebugTargetInfo component
   */
  _setDebugTargetData() {
    if (this.hostType === Toolbox.HostType.PAGE) {
      // Displays DebugTargetInfo which shows the basic information of debug target,
      // if `about:devtools-toolbox` URL opens directly.
      // DebugTargetInfo requires this._debugTargetData to be populated
      this.component.setDebugTargetData(this._getDebugTargetData());
    }
  },

  _onResourceAvailable(resources) {
    let errors = this._errorCount || 0;

    const { TYPES } = this.resourceCommand;
    for (const resource of resources) {
      const { resourceType } = resource;
      if (
        resourceType === TYPES.ERROR_MESSAGE &&
        // ERROR_MESSAGE resources can be warnings/info, but here we only want to count errors
        resource.pageError.error
      ) {
        errors++;
        continue;
      }

      if (resourceType === TYPES.CONSOLE_MESSAGE) {
        const { level } = resource.message;
        if (level === "error" || level === "exception" || level === "assert") {
          errors++;
        }

        // Reset the count on console.clear
        if (level === "clear") {
          errors = 0;
        }
      }

      // Only consider top level document, and ignore remote iframes top document
      if (
        resourceType === TYPES.DOCUMENT_EVENT &&
        resource.name === "will-navigate" &&
        resource.targetFront.isTopLevel
      ) {
        this._onWillNavigate({
          isFrameSwitching: resource.isFrameSwitching,
        });
        // While we will call `setErrorCount(0)` from onWillNavigate, we also need to reset
        // `errors` local variable in order to clear previous errors processed in the same
        // throttling bucket as this will-navigate resource.
        errors = 0;
      }

      if (
        resourceType === TYPES.DOCUMENT_EVENT &&
        !resource.isFrameSwitching &&
        // `url` is set on the targetFront when we receive dom-loading, and `title` when
        // `dom-interactive` is received. Here we're only updating the window title in
        // the "newer" event.
        resource.name === "dom-interactive"
      ) {
        // the targetFront title and url are updated on dom-interactive, so delay refreshing
        // the host title a bit in order for the event listener in targetCommand to be
        // executed.
        setTimeout(() => {
          // Update the EvaluationContext selector so url/title of targets can be updated
          this.store.dispatch(refreshTargets());

          this._updateFrames({
            frameData: {
              id: resource.targetFront.actorID,
              url: resource.targetFront.url,
              title: resource.targetFront.title,
            },
          });

          if (resource.targetFront.isTopLevel) {
            this._refreshHostTitle();
            this._setDebugTargetData();
          }
        }, 0);
      }

      if (resourceType == TYPES.THREAD_STATE) {
        this._onThreadStateChanged(resource);
      }
    }

    this.setErrorCount(errors);
  },

  _onResourceUpdated(resources) {
    let errors = this._errorCount || 0;

    for (const { update } of resources) {
      // In order to match webconsole behaviour, we treat 4xx and 5xx network calls as errors.
      if (
        update.resourceType === this.resourceCommand.TYPES.NETWORK_EVENT &&
        update.resourceUpdates.status &&
        update.resourceUpdates.status.toString().match(REGEX_4XX_5XX)
      ) {
        errors++;
      }
    }

    this.setErrorCount(errors);
  },

  /**
   * Set the number of errors in the toolbar icon.
   *
   * @param {Number} count
   */
  setErrorCount(count) {
    // Don't re-render if the number of errors changed
    if (!this.component || this._errorCount === count) {
      return;
    }

    this._errorCount = count;

    // Update button properties and trigger a render of the toolbox
    this.updateErrorCountButton();
    this._throttledSetToolboxButtons();
  },
};
