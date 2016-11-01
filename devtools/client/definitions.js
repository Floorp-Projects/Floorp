/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const osString = Services.appinfo.OS;

// Panels
loader.lazyGetter(this, "OptionsPanel", () => require("devtools/client/framework/toolbox-options").OptionsPanel);
loader.lazyGetter(this, "InspectorPanel", () => require("devtools/client/inspector/panel").InspectorPanel);
loader.lazyGetter(this, "WebConsolePanel", () => require("devtools/client/webconsole/panel").WebConsolePanel);
loader.lazyGetter(this, "DebuggerPanel", () => require("devtools/client/debugger/panel").DebuggerPanel);
loader.lazyGetter(this, "StyleEditorPanel", () => require("devtools/client/styleeditor/styleeditor-panel").StyleEditorPanel);
loader.lazyGetter(this, "ShaderEditorPanel", () => require("devtools/client/shadereditor/panel").ShaderEditorPanel);
loader.lazyGetter(this, "CanvasDebuggerPanel", () => require("devtools/client/canvasdebugger/panel").CanvasDebuggerPanel);
loader.lazyGetter(this, "WebAudioEditorPanel", () => require("devtools/client/webaudioeditor/panel").WebAudioEditorPanel);
loader.lazyGetter(this, "MemoryPanel", () => require("devtools/client/memory/panel").MemoryPanel);
loader.lazyGetter(this, "PerformancePanel", () => require("devtools/client/performance/panel").PerformancePanel);
loader.lazyGetter(this, "NetMonitorPanel", () => require("devtools/client/netmonitor/panel").NetMonitorPanel);
loader.lazyGetter(this, "StoragePanel", () => require("devtools/client/storage/panel").StoragePanel);
loader.lazyGetter(this, "ScratchpadPanel", () => require("devtools/client/scratchpad/scratchpad-panel").ScratchpadPanel);
loader.lazyGetter(this, "DomPanel", () => require("devtools/client/dom/dom-panel").DomPanel);

const {LocalizationHelper} = require("devtools/shared/l10n");
const L10N = new LocalizationHelper("devtools/locale/startup.properties");

var Tools = {};
exports.Tools = Tools;

// Definitions
Tools.options = {
  id: "options",
  ordinal: 0,
  url: "chrome://devtools/content/framework/toolbox-options.xhtml",
  icon: "chrome://devtools/skin/images/tool-options.svg",
  invertIconForDarkTheme: true,
  bgTheme: "theme-body",
  label: l10n("options.label"),
  iconOnly: true,
  panelLabel: l10n("options.panelLabel"),
  tooltip: l10n("optionsButton.tooltip"),
  inMenu: false,

  isTargetSupported: function () {
    return true;
  },

  build: function (iframeWindow, toolbox) {
    return new OptionsPanel(iframeWindow, toolbox);
  }
};

Tools.inspector = {
  id: "inspector",
  accesskey: l10n("inspector.accesskey"),
  key: l10n("inspector.commandkey"),
  ordinal: 1,
  modifiers: osString == "Darwin" ? "accel,alt" : "accel,shift",
  icon: "chrome://devtools/skin/images/tool-inspector.svg",
  invertIconForDarkTheme: true,
  url: "chrome://devtools/content/inspector/inspector.xhtml",
  label: l10n("inspector.label"),
  panelLabel: l10n("inspector.panelLabel"),
  get tooltip() {
    return l10n("inspector.tooltip2",
    (osString == "Darwin" ? "Cmd+Opt+" : "Ctrl+Shift+") + this.key);
  },
  inMenu: true,
  commands: [
    "devtools/client/responsivedesign/resize-commands",
    "devtools/client/inspector/inspector-commands"
  ],

  preventClosingOnKey: true,
  onkey: function (panel, toolbox) {
    toolbox.highlighterUtils.togglePicker();
  },

  isTargetSupported: function (target) {
    return target.hasActor("inspector");
  },

  build: function (iframeWindow, toolbox) {
    return new InspectorPanel(iframeWindow, toolbox);
  }
};

Tools.webConsole = {
  id: "webconsole",
  key: l10n("cmd.commandkey"),
  accesskey: l10n("webConsoleCmd.accesskey"),
  modifiers: Services.appinfo.OS == "Darwin" ? "accel,alt" : "accel,shift",
  ordinal: 2,
  icon: "chrome://devtools/skin/images/tool-webconsole.svg",
  invertIconForDarkTheme: true,
  url: "chrome://devtools/content/webconsole/webconsole.xul",
  label: l10n("ToolboxTabWebconsole.label"),
  menuLabel: l10n("MenuWebconsole.label"),
  panelLabel: l10n("ToolboxWebConsole.panelLabel"),
  get tooltip() {
    return l10n("ToolboxWebconsole.tooltip2",
    (osString == "Darwin" ? "Cmd+Opt+" : "Ctrl+Shift+") + this.key);
  },
  inMenu: true,
  commands: "devtools/client/webconsole/console-commands",

  preventClosingOnKey: true,
  onkey: function (panel, toolbox) {
    if (toolbox.splitConsole) {
      return toolbox.focusConsoleInput();
    }

    panel.focusInput();
    return undefined;
  },

  isTargetSupported: function () {
    return true;
  },

  build: function (iframeWindow, toolbox) {
    return new WebConsolePanel(iframeWindow, toolbox);
  }
};

Tools.jsdebugger = {
  id: "jsdebugger",
  key: l10n("debuggerMenu.commandkey"),
  accesskey: l10n("debuggerMenu.accesskey"),
  modifiers: osString == "Darwin" ? "accel,alt" : "accel,shift",
  ordinal: 3,
  icon: "chrome://devtools/skin/images/tool-debugger.svg",
  invertIconForDarkTheme: true,
  highlightedicon: "chrome://devtools/skin/images/tool-debugger-paused.svg",
  url: "chrome://devtools/content/debugger/debugger.xul",
  label: l10n("ToolboxDebugger.label"),
  panelLabel: l10n("ToolboxDebugger.panelLabel"),
  get tooltip() {
    return l10n("ToolboxDebugger.tooltip2",
    (osString == "Darwin" ? "Cmd+Opt+" : "Ctrl+Shift+") + this.key);
  },
  inMenu: true,
  commands: "devtools/client/debugger/debugger-commands",

  isTargetSupported: function () {
    return true;
  },

  build: function (iframeWindow, toolbox) {
    return new DebuggerPanel(iframeWindow, toolbox);
  }
};

function switchDebugger() {
  if (Services.prefs.getBoolPref("devtools.debugger.new-debugger-frontend")) {
    const NewDebuggerPanel = require("devtools/client/debugger/new/panel").DebuggerPanel;

    Tools.jsdebugger.url = "chrome://devtools/content/debugger/new/index.html";
    Tools.jsdebugger.build = function (iframeWindow, toolbox) {
      return new NewDebuggerPanel(iframeWindow, toolbox);
    };
  } else {
    Tools.jsdebugger.url = "chrome://devtools/content/debugger/debugger.xul";
    Tools.jsdebugger.build = function (iframeWindow, toolbox) {
      return new DebuggerPanel(iframeWindow, toolbox);
    };
  }
}
switchDebugger();

Services.prefs.addObserver(
  "devtools.debugger.new-debugger-frontend",
  { observe: switchDebugger },
  false
);

Tools.styleEditor = {
  id: "styleeditor",
  key: l10n("open.commandkey"),
  ordinal: 4,
  visibilityswitch: "devtools.styleeditor.enabled",
  accesskey: l10n("open.accesskey"),
  modifiers: "shift",
  icon: "chrome://devtools/skin/images/tool-styleeditor.svg",
  invertIconForDarkTheme: true,
  url: "chrome://devtools/content/styleeditor/styleeditor.xul",
  label: l10n("ToolboxStyleEditor.label"),
  panelLabel: l10n("ToolboxStyleEditor.panelLabel"),
  get tooltip() {
    return l10n("ToolboxStyleEditor.tooltip3",
    "Shift+" + functionkey(this.key));
  },
  inMenu: true,
  commands: "devtools/client/styleeditor/styleeditor-commands",

  isTargetSupported: function (target) {
    return target.hasActor("styleEditor") || target.hasActor("styleSheets");
  },

  build: function (iframeWindow, toolbox) {
    return new StyleEditorPanel(iframeWindow, toolbox);
  }
};

Tools.shaderEditor = {
  id: "shadereditor",
  ordinal: 5,
  visibilityswitch: "devtools.shadereditor.enabled",
  icon: "chrome://devtools/skin/images/tool-shadereditor.svg",
  invertIconForDarkTheme: true,
  url: "chrome://devtools/content/shadereditor/shadereditor.xul",
  label: l10n("ToolboxShaderEditor.label"),
  panelLabel: l10n("ToolboxShaderEditor.panelLabel"),
  tooltip: l10n("ToolboxShaderEditor.tooltip"),

  isTargetSupported: function (target) {
    return target.hasActor("webgl") && !target.chrome;
  },

  build: function (iframeWindow, toolbox) {
    return new ShaderEditorPanel(iframeWindow, toolbox);
  }
};

Tools.canvasDebugger = {
  id: "canvasdebugger",
  ordinal: 6,
  visibilityswitch: "devtools.canvasdebugger.enabled",
  icon: "chrome://devtools/skin/images/tool-canvas.svg",
  invertIconForDarkTheme: true,
  url: "chrome://devtools/content/canvasdebugger/canvasdebugger.xul",
  label: l10n("ToolboxCanvasDebugger.label"),
  panelLabel: l10n("ToolboxCanvasDebugger.panelLabel"),
  tooltip: l10n("ToolboxCanvasDebugger.tooltip"),

  // Hide the Canvas Debugger in the Add-on Debugger and Browser Toolbox
  // (bug 1047520).
  isTargetSupported: function (target) {
    return target.hasActor("canvas") && !target.chrome;
  },

  build: function (iframeWindow, toolbox) {
    return new CanvasDebuggerPanel(iframeWindow, toolbox);
  }
};

Tools.performance = {
  id: "performance",
  ordinal: 7,
  icon: "chrome://devtools/skin/images/tool-profiler.svg",
  invertIconForDarkTheme: true,
  highlightedicon: "chrome://devtools/skin/images/tool-profiler-active.svg",
  url: "chrome://devtools/content/performance/performance.xul",
  visibilityswitch: "devtools.performance.enabled",
  label: l10n("performance.label"),
  panelLabel: l10n("performance.panelLabel"),
  get tooltip() {
    return l10n("performance.tooltip", "Shift+" + functionkey(this.key));
  },
  accesskey: l10n("performance.accesskey"),
  key: l10n("performance.commandkey"),
  modifiers: "shift",
  inMenu: true,

  isTargetSupported: function (target) {
    return target.hasActor("profiler");
  },

  build: function (frame, target) {
    return new PerformancePanel(frame, target);
  }
};

Tools.memory = {
  id: "memory",
  ordinal: 8,
  icon: "chrome://devtools/skin/images/tool-memory.svg",
  invertIconForDarkTheme: true,
  highlightedicon: "chrome://devtools/skin/images/tool-memory-active.svg",
  url: "chrome://devtools/content/memory/memory.xhtml",
  visibilityswitch: "devtools.memory.enabled",
  label: l10n("memory.label"),
  panelLabel: l10n("memory.panelLabel"),
  tooltip: l10n("memory.tooltip"),

  isTargetSupported: function (target) {
    return target.getTrait("heapSnapshots") && !target.isAddon;
  },

  build: function (frame, target) {
    return new MemoryPanel(frame, target);
  }
};

Tools.netMonitor = {
  id: "netmonitor",
  accesskey: l10n("netmonitor.accesskey"),
  key: l10n("netmonitor.commandkey"),
  ordinal: 9,
  modifiers: osString == "Darwin" ? "accel,alt" : "accel,shift",
  visibilityswitch: "devtools.netmonitor.enabled",
  icon: "chrome://devtools/skin/images/tool-network.svg",
  invertIconForDarkTheme: true,
  url: "chrome://devtools/content/netmonitor/netmonitor.xul",
  label: l10n("netmonitor.label"),
  panelLabel: l10n("netmonitor.panelLabel"),
  get tooltip() {
    return l10n("netmonitor.tooltip2",
    (osString == "Darwin" ? "Cmd+Opt+" : "Ctrl+Shift+") + this.key);
  },
  inMenu: true,

  isTargetSupported: function (target) {
    return target.getTrait("networkMonitor");
  },

  build: function (iframeWindow, toolbox) {
    return new NetMonitorPanel(iframeWindow, toolbox);
  }
};

Tools.storage = {
  id: "storage",
  key: l10n("storage.commandkey"),
  ordinal: 10,
  accesskey: l10n("storage.accesskey"),
  modifiers: "shift",
  visibilityswitch: "devtools.storage.enabled",
  icon: "chrome://devtools/skin/images/tool-storage.svg",
  invertIconForDarkTheme: true,
  url: "chrome://devtools/content/storage/storage.xul",
  label: l10n("storage.label"),
  menuLabel: l10n("storage.menuLabel"),
  panelLabel: l10n("storage.panelLabel"),
  get tooltip() {
    return l10n("storage.tooltip3", "Shift+" + functionkey(this.key));
  },
  inMenu: true,

  isTargetSupported: function (target) {
    return target.isLocalTab ||
           (target.hasActor("storage") && target.getTrait("storageInspector"));
  },

  build: function (iframeWindow, toolbox) {
    return new StoragePanel(iframeWindow, toolbox);
  }
};

Tools.webAudioEditor = {
  id: "webaudioeditor",
  ordinal: 11,
  visibilityswitch: "devtools.webaudioeditor.enabled",
  icon: "chrome://devtools/skin/images/tool-webaudio.svg",
  invertIconForDarkTheme: true,
  url: "chrome://devtools/content/webaudioeditor/webaudioeditor.xul",
  label: l10n("ToolboxWebAudioEditor1.label"),
  panelLabel: l10n("ToolboxWebAudioEditor1.panelLabel"),
  tooltip: l10n("ToolboxWebAudioEditor1.tooltip"),

  isTargetSupported: function (target) {
    return !target.chrome && target.hasActor("webaudio");
  },

  build: function (iframeWindow, toolbox) {
    return new WebAudioEditorPanel(iframeWindow, toolbox);
  }
};

Tools.scratchpad = {
  id: "scratchpad",
  ordinal: 12,
  visibilityswitch: "devtools.scratchpad.enabled",
  icon: "chrome://devtools/skin/images/tool-scratchpad.svg",
  invertIconForDarkTheme: true,
  url: "chrome://devtools/content/scratchpad/scratchpad.xul",
  label: l10n("scratchpad.label"),
  panelLabel: l10n("scratchpad.panelLabel"),
  tooltip: l10n("scratchpad.tooltip"),
  inMenu: false,
  commands: "devtools/client/scratchpad/scratchpad-commands",

  isTargetSupported: function (target) {
    return target.hasActor("console");
  },

  build: function (iframeWindow, toolbox) {
    return new ScratchpadPanel(iframeWindow, toolbox);
  }
};

Tools.dom = {
  id: "dom",
  accesskey: l10n("dom.accesskey"),
  key: l10n("dom.commandkey"),
  ordinal: 13,
  modifiers: osString == "Darwin" ? "accel,alt" : "accel,shift",
  visibilityswitch: "devtools.dom.enabled",
  icon: "chrome://devtools/skin/images/tool-dom.svg",
  invertIconForDarkTheme: true,
  url: "chrome://devtools/content/dom/dom.html",
  label: l10n("dom.label"),
  panelLabel: l10n("dom.panelLabel"),
  get tooltip() {
    return l10n("dom.tooltip",
      (osString == "Darwin" ? "Cmd+Opt+" : "Ctrl+Shift+") + this.key);
  },
  inMenu: true,

  isTargetSupported: function (target) {
    return target.getTrait("webConsoleCommands");
  },

  build: function (iframeWindow, toolbox) {
    return new DomPanel(iframeWindow, toolbox);
  }
};

var defaultTools = [
  Tools.options,
  Tools.webConsole,
  Tools.inspector,
  Tools.jsdebugger,
  Tools.styleEditor,
  Tools.shaderEditor,
  Tools.canvasDebugger,
  Tools.webAudioEditor,
  Tools.performance,
  Tools.netMonitor,
  Tools.storage,
  Tools.scratchpad,
  Tools.memory,
  Tools.dom,
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

Tools.firebugTheme = {
  id: "firebug",
  label: l10n("options.firebugTheme.label2"),
  ordinal: 3,
  stylesheets: ["chrome://devtools/skin/firebug-theme.css"],
  classList: ["theme-light", "theme-firebug"],
};

exports.defaultThemes = [
  Tools.darkTheme,
  Tools.lightTheme,
  Tools.firebugTheme,
];

// White-list buttons that can be toggled to prevent adding prefs for
// addons that have manually inserted toolbarbuttons into DOM.
// (By default, supported target is only local tab)
exports.ToolboxButtons = [
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
 * Lookup l10n string from a string bundle.
 *
 * @param {string} name
 *        The key to lookup.
 * @param {string} arg
 *        Optional format argument.
 * @returns A localized version of the given key.
 */
function l10n(name, arg) {
  try {
    return arg ? L10N.getFormatStr(name, arg) : L10N.getStr(name);
  } catch (ex) {
    console.log("Error reading '" + name + "'");
    throw new Error("l10n error with " + name);
  }
}

function functionkey(shortkey) {
  return shortkey.split("_")[1];
}
