/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const osString = Services.appinfo.OS;

// Panels
loader.lazyGetter(
  this,
  "OptionsPanel",
  () => require("devtools/client/framework/toolbox-options").OptionsPanel
);
loader.lazyGetter(
  this,
  "InspectorPanel",
  () => require("devtools/client/inspector/panel").InspectorPanel
);
loader.lazyGetter(
  this,
  "WebConsolePanel",
  () => require("devtools/client/webconsole/panel").WebConsolePanel
);
loader.lazyGetter(
  this,
  "DebuggerPanel",
  () => require("devtools/client/debugger/panel").DebuggerPanel
);
loader.lazyGetter(
  this,
  "StyleEditorPanel",
  () => require("devtools/client/styleeditor/panel").StyleEditorPanel
);
loader.lazyGetter(
  this,
  "MemoryPanel",
  () => require("devtools/client/memory/panel").MemoryPanel
);
loader.lazyGetter(
  this,
  "PerformancePanel",
  () => require("devtools/client/performance/panel").PerformancePanel
);
loader.lazyGetter(
  this,
  "NewPerformancePanel",
  () => require("devtools/client/performance-new/panel").PerformancePanel
);
loader.lazyGetter(
  this,
  "NetMonitorPanel",
  () => require("devtools/client/netmonitor/panel").NetMonitorPanel
);
loader.lazyGetter(
  this,
  "StoragePanel",
  () => require("devtools/client/storage/panel").StoragePanel
);
loader.lazyGetter(
  this,
  "ScratchpadPanel",
  () => require("devtools/client/scratchpad/panel").ScratchpadPanel
);
loader.lazyGetter(
  this,
  "DomPanel",
  () => require("devtools/client/dom/panel").DomPanel
);
loader.lazyGetter(
  this,
  "AccessibilityPanel",
  () => require("devtools/client/accessibility/panel").AccessibilityPanel
);
loader.lazyGetter(
  this,
  "ApplicationPanel",
  () => require("devtools/client/application/panel").ApplicationPanel
);
loader.lazyGetter(
  this,
  "reloadAndRecordTab",
  () => require("devtools/client/webreplay/menu.js").reloadAndRecordTab
);
loader.lazyGetter(
  this,
  "reloadAndStopRecordingTab",
  () => require("devtools/client/webreplay/menu.js").reloadAndStopRecordingTab
);

// Other dependencies
loader.lazyRequireGetter(
  this,
  "AccessibilityStartup",
  "devtools/client/accessibility/accessibility-startup",
  true
);
loader.lazyRequireGetter(
  this,
  "ResponsiveUIManager",
  "devtools/client/responsive/manager",
  true
);
loader.lazyImporter(
  this,
  "ScratchpadManager",
  "resource://devtools/client/scratchpad/scratchpad-manager.jsm"
);

const { MultiLocalizationHelper } = require("devtools/shared/l10n");
const L10N = new MultiLocalizationHelper(
  "devtools/client/locales/startup.properties",
  "devtools/startup/locales/key-shortcuts.properties"
);

var Tools = {};
exports.Tools = Tools;

// Definitions
Tools.options = {
  id: "options",
  ordinal: 0,
  url: "chrome://devtools/content/framework/toolbox-options.xhtml",
  icon: "chrome://devtools/skin/images/settings.svg",
  bgTheme: "theme-body",
  label: l10n("options.label"),
  iconOnly: true,
  panelLabel: l10n("options.panelLabel"),
  tooltip: l10n("optionsButton.tooltip"),
  inMenu: false,

  isTargetSupported: function() {
    return true;
  },

  build: function(iframeWindow, toolbox) {
    return new OptionsPanel(iframeWindow, toolbox);
  },
};

Tools.inspector = {
  id: "inspector",
  accesskey: l10n("inspector.accesskey"),
  ordinal: 1,
  icon: "chrome://devtools/skin/images/tool-inspector.svg",
  url: "chrome://devtools/content/inspector/index.xhtml",
  label: l10n("inspector.label"),
  panelLabel: l10n("inspector.panelLabel"),
  get tooltip() {
    if (osString == "Darwin") {
      const cmdShiftC = "Cmd+Shift+" + l10n("inspector.commandkey");
      const cmdOptC = "Cmd+Opt+" + l10n("inspector.commandkey");
      return l10n("inspector.mac.tooltip", cmdShiftC, cmdOptC);
    }

    const ctrlShiftC = "Ctrl+Shift+" + l10n("inspector.commandkey");
    return l10n("inspector.tooltip2", ctrlShiftC);
  },
  inMenu: true,

  preventClosingOnKey: true,
  // preventRaisingOnKey is used to keep the focus on the content window for shortcuts
  // that trigger the element picker.
  preventRaisingOnKey: true,
  onkey: function(panel, toolbox) {
    toolbox.inspectorFront.nodePicker.togglePicker();
  },

  isTargetSupported: function(target) {
    return target.hasActor("inspector");
  },

  build: function(iframeWindow, toolbox) {
    return new InspectorPanel(iframeWindow, toolbox);
  },
};
Tools.webConsole = {
  id: "webconsole",
  accesskey: l10n("webConsoleCmd.accesskey"),
  ordinal: 2,
  url: "chrome://devtools/content/webconsole/index.html",
  icon: "chrome://devtools/skin/images/tool-webconsole.svg",
  label: l10n("ToolboxTabWebconsole.label"),
  menuLabel: l10n("MenuWebconsole.label"),
  panelLabel: l10n("ToolboxWebConsole.panelLabel"),
  get tooltip() {
    return l10n(
      "ToolboxWebconsole.tooltip2",
      (osString == "Darwin" ? "Cmd+Opt+" : "Ctrl+Shift+") +
        l10n("webconsole.commandkey")
    );
  },
  inMenu: true,

  preventClosingOnKey: true,
  onkey: function(panel, toolbox) {
    if (toolbox.splitConsole) {
      return toolbox.focusConsoleInput();
    }

    panel.focusInput();
    return undefined;
  },

  isTargetSupported: function() {
    return true;
  },
  build: function(iframeWindow, toolbox) {
    return new WebConsolePanel(iframeWindow, toolbox);
  },
};

Tools.jsdebugger = {
  id: "jsdebugger",
  accesskey: l10n("debuggerMenu.accesskey"),
  ordinal: 3,
  icon: "chrome://devtools/skin/images/tool-debugger.svg",
  url: "chrome://devtools/content/debugger/index.html",
  label: l10n("ToolboxDebugger.label"),
  panelLabel: l10n("ToolboxDebugger.panelLabel"),
  get tooltip() {
    return l10n("ToolboxDebugger.tooltip3");
  },
  inMenu: true,
  isTargetSupported: function() {
    return true;
  },
  build: function(iframeWindow, toolbox) {
    return new DebuggerPanel(iframeWindow, toolbox);
  },
};

Tools.styleEditor = {
  id: "styleeditor",
  ordinal: 5,
  visibilityswitch: "devtools.styleeditor.enabled",
  accesskey: l10n("open.accesskey"),
  icon: "chrome://devtools/skin/images/tool-styleeditor.svg",
  url: "chrome://devtools/content/styleeditor/index.xul",
  label: l10n("ToolboxStyleEditor.label"),
  panelLabel: l10n("ToolboxStyleEditor.panelLabel"),
  get tooltip() {
    return l10n(
      "ToolboxStyleEditor.tooltip3",
      "Shift+" + functionkey(l10n("styleeditor.commandkey"))
    );
  },
  inMenu: true,
  isTargetSupported: function(target) {
    return target.hasActor("styleSheets");
  },

  build: function(iframeWindow, toolbox) {
    return new StyleEditorPanel(iframeWindow, toolbox);
  },
};

Tools.performance = {
  id: "performance",
  ordinal: 6,
  icon: "chrome://devtools/skin/images/tool-profiler.svg",
  visibilityswitch: "devtools.performance.enabled",
  label: l10n("performance.label"),
  panelLabel: l10n("performance.panelLabel"),
  get tooltip() {
    return l10n(
      "performance.tooltip",
      "Shift+" + functionkey(l10n("performance.commandkey"))
    );
  },
  accesskey: l10n("performance.accesskey"),
  inMenu: true,
};

function switchPerformancePanel() {
  if (
    Services.prefs.getBoolPref("devtools.performance.new-panel-enabled", false)
  ) {
    Tools.performance.url =
      "chrome://devtools/content/performance-new/index.xhtml";
    Tools.performance.build = function(frame, target) {
      return new NewPerformancePanel(frame, target);
    };
    Tools.performance.isTargetSupported = function(target) {
      // Root actors are lazily initialized, so we can't check if the target has
      // the perf actor yet. Also this function is not async, so we can't initialize
      // the actor yet.
      // We don't display the new performance panel for remote context in the
      // toolbox, because this has an overhead. Instead we should use WebIDE (or
      // the coming about:debugging).
      return target.isLocalTab;
    };
  } else {
    Tools.performance.url = "chrome://devtools/content/performance/index.xul";
    Tools.performance.build = function(frame, target) {
      return new PerformancePanel(frame, target);
    };
    Tools.performance.isTargetSupported = function(target) {
      return target.hasActor("performance");
    };
  }
}
switchPerformancePanel();

Services.prefs.addObserver("devtools.performance.new-panel-enabled", {
  observe: switchPerformancePanel,
});

Tools.memory = {
  id: "memory",
  ordinal: 7,
  icon: "chrome://devtools/skin/images/tool-memory.svg",
  url: "chrome://devtools/content/memory/index.xhtml",
  visibilityswitch: "devtools.memory.enabled",
  label: l10n("memory.label"),
  panelLabel: l10n("memory.panelLabel"),
  tooltip: l10n("memory.tooltip"),

  isTargetSupported: function(target) {
    return (
      target.getTrait("heapSnapshots") &&
      !target.isAddon &&
      !target.isWorkerTarget
    );
  },

  build: function(frame, target) {
    return new MemoryPanel(frame, target);
  },
};

Tools.netMonitor = {
  id: "netmonitor",
  accesskey: l10n("netmonitor.accesskey"),
  ordinal: 4,
  visibilityswitch: "devtools.netmonitor.enabled",
  icon: "chrome://devtools/skin/images/tool-network.svg",
  url: "chrome://devtools/content/netmonitor/index.html",
  label: l10n("netmonitor.label"),
  panelLabel: l10n("netmonitor.panelLabel"),
  get tooltip() {
    return l10n(
      "netmonitor.tooltip2",
      (osString == "Darwin" ? "Cmd+Opt+" : "Ctrl+Shift+") +
        l10n("netmonitor.commandkey")
    );
  },
  inMenu: true,

  isTargetSupported: function(target) {
    return target.getTrait("networkMonitor") && !target.isWorkerTarget;
  },

  build: function(iframeWindow, toolbox) {
    return new NetMonitorPanel(iframeWindow, toolbox);
  },
};

Tools.storage = {
  id: "storage",
  ordinal: 8,
  accesskey: l10n("storage.accesskey"),
  visibilityswitch: "devtools.storage.enabled",
  icon: "chrome://devtools/skin/images/tool-storage.svg",
  url: "chrome://devtools/content/storage/index.xul",
  label: l10n("storage.label"),
  menuLabel: l10n("storage.menuLabel"),
  panelLabel: l10n("storage.panelLabel"),
  get tooltip() {
    return l10n(
      "storage.tooltip3",
      "Shift+" + functionkey(l10n("storage.commandkey"))
    );
  },
  inMenu: true,

  isTargetSupported: function(target) {
    return (
      target.isLocalTab ||
      (target.hasActor("storage") && target.getTrait("storageInspector"))
    );
  },

  build: function(iframeWindow, toolbox) {
    return new StoragePanel(iframeWindow, toolbox);
  },
};

Tools.scratchpad = {
  id: "scratchpad",
  ordinal: 12,
  visibilityswitch: "devtools.scratchpad.enabled",
  icon: "chrome://devtools/skin/images/tool-scratchpad.svg",
  url: "chrome://devtools/content/scratchpad/index.xul",
  label: l10n("scratchpad.label"),
  panelLabel: l10n("scratchpad.panelLabel"),
  tooltip: l10n("scratchpad.tooltip"),
  inMenu: false,
  isTargetSupported: function(target) {
    return target.hasActor("console");
  },
  build: function(iframeWindow, toolbox) {
    return new ScratchpadPanel(iframeWindow, toolbox);
  },
};

Tools.dom = {
  id: "dom",
  accesskey: l10n("dom.accesskey"),
  ordinal: 11,
  visibilityswitch: "devtools.dom.enabled",
  icon: "chrome://devtools/skin/images/tool-dom.svg",
  url: "chrome://devtools/content/dom/index.html",
  label: l10n("dom.label"),
  panelLabel: l10n("dom.panelLabel"),
  get tooltip() {
    return l10n(
      "dom.tooltip",
      (osString == "Darwin" ? "Cmd+Opt+" : "Ctrl+Shift+") +
        l10n("dom.commandkey")
    );
  },
  inMenu: true,

  isTargetSupported: function(target) {
    return target.getTrait("webConsoleCommands");
  },

  build: function(iframeWindow, toolbox) {
    return new DomPanel(iframeWindow, toolbox);
  },
};

Tools.accessibility = {
  id: "accessibility",
  accesskey: l10n("accessibility.accesskey"),
  ordinal: 9,
  modifiers: osString == "Darwin" ? "accel,alt" : "accel,shift",
  visibilityswitch: "devtools.accessibility.enabled",
  icon: "chrome://devtools/skin/images/tool-accessibility.svg",
  url: "chrome://devtools/content/accessibility/index.html",
  label: l10n("accessibility.label"),
  panelLabel: l10n("accessibility.panelLabel"),
  get tooltip() {
    return l10n(
      "accessibility.tooltip3",
      "Shift+" + functionkey(l10n("accessibilityF12.commandkey"))
    );
  },
  inMenu: true,

  isTargetSupported(target) {
    return target.hasActor("accessibility");
  },

  build(iframeWindow, toolbox) {
    const startup = toolbox.getToolStartup("accessibility");
    return new AccessibilityPanel(iframeWindow, toolbox, startup);
  },

  buildToolStartup(toolbox) {
    return new AccessibilityStartup(toolbox);
  },
};

Tools.application = {
  id: "application",
  ordinal: 10,
  visibilityswitch: "devtools.application.enabled",
  icon: "chrome://devtools/skin/images/tool-application.svg",
  url: "chrome://devtools/content/application/index.html",
  label: l10n("application.label"),
  panelLabel: l10n("application.panellabel"),
  tooltip: l10n("application.tooltip"),
  inMenu: false,
  hiddenInOptions: true,

  isTargetSupported: function(target) {
    return target.isLocalTab;
  },

  build: function(iframeWindow, toolbox) {
    return new ApplicationPanel(iframeWindow, toolbox);
  },
};

var defaultTools = [
  Tools.options,
  Tools.webConsole,
  Tools.inspector,
  Tools.jsdebugger,
  Tools.styleEditor,
  Tools.performance,
  Tools.netMonitor,
  Tools.storage,
  Tools.scratchpad,
  Tools.memory,
  Tools.dom,
  Tools.accessibility,
  Tools.application,
];

exports.defaultTools = defaultTools;

Tools.darkTheme = {
  id: "dark",
  label: l10n("options.darkTheme.label2"),
  ordinal: 1,
  stylesheets: ["chrome://devtools/skin/dark-theme.css"],
  classList: ["theme-dark"],
};

Tools.lightTheme = {
  id: "light",
  label: l10n("options.lightTheme.label2"),
  ordinal: 2,
  stylesheets: ["chrome://devtools/skin/light-theme.css"],
  classList: ["theme-light"],
};

exports.defaultThemes = [Tools.darkTheme, Tools.lightTheme];

// White-list buttons that can be toggled to prevent adding prefs for
// addons that have manually inserted toolbarbuttons into DOM.
// (By default, supported target is only local tab)
exports.ToolboxButtons = [
  {
    id: "command-button-paintflashing",
    description: l10n("toolbox.buttons.paintflashing"),
    isTargetSupported: target => target.isLocalTab,
    onClick(event, toolbox) {
      toolbox.togglePaintFlashing();
    },
    isChecked(toolbox) {
      return toolbox.isPaintFlashing;
    },
  },
  {
    id: "command-button-scratchpad",
    description: l10n("toolbox.buttons.scratchpad"),
    isTargetSupported: target => target.isLocalTab,
    onClick(event, toolbox) {
      ScratchpadManager.openScratchpad();
    },
  },
  {
    id: "command-button-replay",
    description: l10n("toolbox.buttons.replay"),
    isTargetSupported: target =>
      Services.prefs.getBoolPref("devtools.recordreplay.mvp.enabled") &&
      !target.canRewind &&
      target.isLocalTab,
    onClick: () => reloadAndRecordTab(),
    isChecked: () => false,
  },
  {
    id: "command-button-stop-replay",
    description: l10n("toolbox.buttons.stopReplay"),
    isTargetSupported: target =>
      Services.prefs.getBoolPref("devtools.recordreplay.mvp.enabled") &&
      target.canRewind &&
      target.isLocalTab,
    onClick: () => reloadAndStopRecordingTab(),
    isChecked: () => true,
  },
  {
    id: "command-button-responsive",
    description: l10n(
      "toolbox.buttons.responsive",
      osString == "Darwin" ? "Cmd+Opt+M" : "Ctrl+Shift+M"
    ),
    isTargetSupported: target => target.isLocalTab,
    onClick(event, toolbox) {
      const tab = toolbox.target.tab;
      const browserWindow = tab.ownerDocument.defaultView;
      ResponsiveUIManager.toggle(browserWindow, tab, { trigger: "toolbox" });
    },
    isChecked(toolbox) {
      if (!toolbox.target.tab) {
        return false;
      }
      return ResponsiveUIManager.isActiveForTab(toolbox.target.tab);
    },
    setup(toolbox, onChange) {
      ResponsiveUIManager.on("on", onChange);
      ResponsiveUIManager.on("off", onChange);
    },
    teardown(toolbox, onChange) {
      ResponsiveUIManager.off("on", onChange);
      ResponsiveUIManager.off("off", onChange);
    },
  },
  {
    id: "command-button-screenshot",
    description: l10n("toolbox.buttons.screenshot"),
    isTargetSupported: target =>
      !target.chrome && target.hasActor("screenshot"),
    async onClick(event, toolbox) {
      // Special case for screenshot button to check for clipboard preference
      const clipboardEnabled = Services.prefs.getBoolPref(
        "devtools.screenshot.clipboard.enabled"
      );
      const args = { fullpage: true, file: true };
      if (clipboardEnabled) {
        args.clipboard = true;
      }
      const screenshotFront = await toolbox.target.getFront("screenshot");
      await screenshotFront.captureAndSave(toolbox.win, args);
    },
  },
  createHighlightButton("RulersHighlighter", "rulers"),
  createHighlightButton("MeasuringToolHighlighter", "measure"),
];

function createHighlightButton(highlighterName, id) {
  return {
    id: `command-button-${id}`,
    description: l10n(`toolbox.buttons.${id}`),
    isTargetSupported: target => !target.chrome,
    async onClick(event, toolbox) {
      await toolbox.initInspector();
      const highlighter = await toolbox.inspectorFront.getOrCreateHighlighterByType(
        highlighterName
      );
      if (highlighter.isShown()) {
        return highlighter.hide();
      }
      // Starting with FF63, higlighter's spec accept a null first argument.
      // Still pass an empty object to fake a domnode front in order to support old
      // servers.
      return highlighter.show({});
    },
    isChecked(toolbox) {
      // if the inspector doesn't exist, then the highlighter has not yet been connected
      // to the front end.
      const inspectorFront = toolbox.target.getCachedFront("inspector");
      if (!inspectorFront) {
        // initialize the inspector front asyncronously. There is a potential for buggy
        // behavior here, but we need to change how the buttons get data (have them
        // consume data from reducers rather than writing our own version) in order to
        // fix this properly.
        return false;
      }
      const highlighter = inspectorFront.getKnownHighlighter(highlighterName);
      return highlighter && highlighter.isShown();
    },
  };
}

/**
 * Lookup l10n string from a string bundle.
 *
 * @param {string} name
 *        The key to lookup.
 * @param {...string} args
 *        Optional format argument.
 * @returns A localized version of the given key.
 */
function l10n(name, ...args) {
  try {
    return args ? L10N.getFormatStr(name, ...args) : L10N.getStr(name);
  } catch (ex) {
    console.log("Error reading '" + name + "'");
    throw new Error("l10n error with " + name);
  }
}

function functionkey(shortkey) {
  return shortkey.split("_")[1];
}
