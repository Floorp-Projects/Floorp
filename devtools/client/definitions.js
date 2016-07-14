/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const osString = Services.appinfo.OS;

// Panels
loader.lazyGetter(this, "OptionsPanel", () => require("devtools/client/framework/toolbox-options").OptionsPanel);
loader.lazyGetter(this, "InspectorPanel", () => require("devtools/client/inspector/inspector-panel").InspectorPanel);
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

// Strings
const toolboxProps = "chrome://devtools/locale/toolbox.properties";
const inspectorProps = "chrome://devtools/locale/inspector.properties";
const webConsoleProps = "chrome://devtools/locale/webconsole.properties";
const debuggerProps = "chrome://devtools/locale/debugger.properties";
const styleEditorProps = "chrome://devtools/locale/styleeditor.properties";
const shaderEditorProps = "chrome://devtools/locale/shadereditor.properties";
const canvasDebuggerProps = "chrome://devtools/locale/canvasdebugger.properties";
const webAudioEditorProps = "chrome://devtools/locale/webaudioeditor.properties";
const performanceProps = "chrome://devtools/locale/performance.properties";
const netMonitorProps = "chrome://devtools/locale/netmonitor.properties";
const storageProps = "chrome://devtools/locale/storage.properties";
const scratchpadProps = "chrome://devtools/locale/scratchpad.properties";
const memoryProps = "chrome://devtools/locale/memory.properties";
const domProps = "chrome://devtools/locale/dom.properties";

loader.lazyGetter(this, "toolboxStrings", () => Services.strings.createBundle(toolboxProps));
loader.lazyGetter(this, "performanceStrings", () => Services.strings.createBundle(performanceProps));
loader.lazyGetter(this, "webConsoleStrings", () => Services.strings.createBundle(webConsoleProps));
loader.lazyGetter(this, "debuggerStrings", () => Services.strings.createBundle(debuggerProps));
loader.lazyGetter(this, "styleEditorStrings", () => Services.strings.createBundle(styleEditorProps));
loader.lazyGetter(this, "shaderEditorStrings", () => Services.strings.createBundle(shaderEditorProps));
loader.lazyGetter(this, "canvasDebuggerStrings", () => Services.strings.createBundle(canvasDebuggerProps));
loader.lazyGetter(this, "webAudioEditorStrings", () => Services.strings.createBundle(webAudioEditorProps));
loader.lazyGetter(this, "inspectorStrings", () => Services.strings.createBundle(inspectorProps));
loader.lazyGetter(this, "netMonitorStrings", () => Services.strings.createBundle(netMonitorProps));
loader.lazyGetter(this, "storageStrings", () => Services.strings.createBundle(storageProps));
loader.lazyGetter(this, "scratchpadStrings", () => Services.strings.createBundle(scratchpadProps));
loader.lazyGetter(this, "memoryStrings", () => Services.strings.createBundle(memoryProps));
loader.lazyGetter(this, "domStrings", () => Services.strings.createBundle(domProps));

var Tools = {};
exports.Tools = Tools;

// Definitions
Tools.options = {
  id: "options",
  ordinal: 0,
  url: "chrome://devtools/content/framework/toolbox-options.xhtml",
  icon: "chrome://devtools/skin/images/tool-options.svg",
  invertIconForLightTheme: true,
  bgTheme: "theme-body",
  label: l10n("options.label", toolboxStrings),
  iconOnly: true,
  panelLabel: l10n("options.panelLabel", toolboxStrings),
  tooltip: l10n("optionsButton.tooltip", toolboxStrings),
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
  accesskey: l10n("inspector.accesskey", inspectorStrings),
  key: l10n("inspector.commandkey", inspectorStrings),
  ordinal: 1,
  modifiers: osString == "Darwin" ? "accel,alt" : "accel,shift",
  icon: "chrome://devtools/skin/images/tool-inspector.svg",
  invertIconForLightTheme: true,
  url: "chrome://devtools/content/inspector/inspector.xul",
  label: l10n("inspector.label", inspectorStrings),
  panelLabel: l10n("inspector.panelLabel", inspectorStrings),
  get tooltip() {
    return l10n("inspector.tooltip2", inspectorStrings,
    (osString == "Darwin" ? "Cmd+Opt+" : "Ctrl+Shift+") + this.key);
  },
  inMenu: true,
  commands: [
    "devtools/client/responsivedesign/resize-commands",
    "devtools/client/inspector/inspector-commands",
    "devtools/client/eyedropper/commands.js"
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
  key: l10n("cmd.commandkey", webConsoleStrings),
  accesskey: l10n("webConsoleCmd.accesskey", webConsoleStrings),
  modifiers: Services.appinfo.OS == "Darwin" ? "accel,alt" : "accel,shift",
  ordinal: 2,
  icon: "chrome://devtools/skin/images/tool-webconsole.svg",
  invertIconForLightTheme: true,
  url: "chrome://devtools/content/webconsole/webconsole.xul",
  label: l10n("ToolboxTabWebconsole.label", webConsoleStrings),
  menuLabel: l10n("MenuWebconsole.label", webConsoleStrings),
  panelLabel: l10n("ToolboxWebConsole.panelLabel", webConsoleStrings),
  get tooltip() {
    return l10n("ToolboxWebconsole.tooltip2", webConsoleStrings,
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
  key: l10n("debuggerMenu.commandkey", debuggerStrings),
  accesskey: l10n("debuggerMenu.accesskey", debuggerStrings),
  modifiers: osString == "Darwin" ? "accel,alt" : "accel,shift",
  ordinal: 3,
  icon: "chrome://devtools/skin/images/tool-debugger.svg",
  invertIconForLightTheme: true,
  highlightedicon: "chrome://devtools/skin/images/tool-debugger-paused.svg",
  url: "chrome://devtools/content/debugger/debugger.xul",
  label: l10n("ToolboxDebugger.label", debuggerStrings),
  panelLabel: l10n("ToolboxDebugger.panelLabel", debuggerStrings),
  get tooltip() {
    return l10n("ToolboxDebugger.tooltip2", debuggerStrings,
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

Tools.styleEditor = {
  id: "styleeditor",
  key: l10n("open.commandkey", styleEditorStrings),
  ordinal: 4,
  visibilityswitch: "devtools.styleeditor.enabled",
  accesskey: l10n("open.accesskey", styleEditorStrings),
  modifiers: "shift",
  icon: "chrome://devtools/skin/images/tool-styleeditor.svg",
  invertIconForLightTheme: true,
  url: "chrome://devtools/content/styleeditor/styleeditor.xul",
  label: l10n("ToolboxStyleEditor.label", styleEditorStrings),
  panelLabel: l10n("ToolboxStyleEditor.panelLabel", styleEditorStrings),
  get tooltip() {
    return l10n("ToolboxStyleEditor.tooltip3", styleEditorStrings,
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
  invertIconForLightTheme: true,
  url: "chrome://devtools/content/shadereditor/shadereditor.xul",
  label: l10n("ToolboxShaderEditor.label", shaderEditorStrings),
  panelLabel: l10n("ToolboxShaderEditor.panelLabel", shaderEditorStrings),
  tooltip: l10n("ToolboxShaderEditor.tooltip", shaderEditorStrings),

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
  invertIconForLightTheme: true,
  url: "chrome://devtools/content/canvasdebugger/canvasdebugger.xul",
  label: l10n("ToolboxCanvasDebugger.label", canvasDebuggerStrings),
  panelLabel: l10n("ToolboxCanvasDebugger.panelLabel", canvasDebuggerStrings),
  tooltip: l10n("ToolboxCanvasDebugger.tooltip", canvasDebuggerStrings),

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
  invertIconForLightTheme: true,
  highlightedicon: "chrome://devtools/skin/images/tool-profiler-active.svg",
  url: "chrome://devtools/content/performance/performance.xul",
  visibilityswitch: "devtools.performance.enabled",
  label: l10n("performance.label", performanceStrings),
  panelLabel: l10n("performance.panelLabel", performanceStrings),
  get tooltip() {
    return l10n("performance.tooltip", performanceStrings,
    "Shift+" + functionkey(this.key));
  },
  accesskey: l10n("performance.accesskey", performanceStrings),
  key: l10n("performance.commandkey", performanceStrings),
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
  invertIconForLightTheme: true,
  highlightedicon: "chrome://devtools/skin/images/tool-memory-active.svg",
  url: "chrome://devtools/content/memory/memory.xhtml",
  visibilityswitch: "devtools.memory.enabled",
  label: l10n("memory.label", memoryStrings),
  panelLabel: l10n("memory.panelLabel", memoryStrings),
  tooltip: l10n("memory.tooltip", memoryStrings),

  isTargetSupported: function (target) {
    return target.getTrait("heapSnapshots");
  },

  build: function (frame, target) {
    return new MemoryPanel(frame, target);
  }
};

Tools.netMonitor = {
  id: "netmonitor",
  accesskey: l10n("netmonitor.accesskey", netMonitorStrings),
  key: l10n("netmonitor.commandkey", netMonitorStrings),
  ordinal: 9,
  modifiers: osString == "Darwin" ? "accel,alt" : "accel,shift",
  visibilityswitch: "devtools.netmonitor.enabled",
  icon: "chrome://devtools/skin/images/tool-network.svg",
  invertIconForLightTheme: true,
  url: "chrome://devtools/content/netmonitor/netmonitor.xul",
  label: l10n("netmonitor.label", netMonitorStrings),
  panelLabel: l10n("netmonitor.panelLabel", netMonitorStrings),
  get tooltip() {
    return l10n("netmonitor.tooltip2", netMonitorStrings,
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
  key: l10n("storage.commandkey", storageStrings),
  ordinal: 10,
  accesskey: l10n("storage.accesskey", storageStrings),
  modifiers: "shift",
  visibilityswitch: "devtools.storage.enabled",
  icon: "chrome://devtools/skin/images/tool-storage.svg",
  invertIconForLightTheme: true,
  url: "chrome://devtools/content/storage/storage.xul",
  label: l10n("storage.label", storageStrings),
  menuLabel: l10n("storage.menuLabel", storageStrings),
  panelLabel: l10n("storage.panelLabel", storageStrings),
  get tooltip() {
    return l10n("storage.tooltip3", storageStrings,
    "Shift+" + functionkey(this.key));
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
  invertIconForLightTheme: true,
  url: "chrome://devtools/content/webaudioeditor/webaudioeditor.xul",
  label: l10n("ToolboxWebAudioEditor1.label", webAudioEditorStrings),
  panelLabel: l10n("ToolboxWebAudioEditor1.panelLabel", webAudioEditorStrings),
  tooltip: l10n("ToolboxWebAudioEditor1.tooltip", webAudioEditorStrings),

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
  invertIconForLightTheme: true,
  url: "chrome://devtools/content/scratchpad/scratchpad.xul",
  label: l10n("scratchpad.label", scratchpadStrings),
  panelLabel: l10n("scratchpad.panelLabel", scratchpadStrings),
  tooltip: l10n("scratchpad.tooltip", scratchpadStrings),
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
  accesskey: l10n("dom.accesskey", domStrings),
  key: l10n("dom.commandkey", domStrings),
  ordinal: 13,
  modifiers: osString == "Darwin" ? "accel,alt" : "accel,shift",
  visibilityswitch: "devtools.dom.enabled",
  icon: "chrome://devtools/skin/images/tool-dom.svg",
  invertIconForLightTheme: true,
  url: "chrome://devtools/content/dom/dom.html",
  label: l10n("dom.label", domStrings),
  panelLabel: l10n("dom.panelLabel", domStrings),
  get tooltip() {
    return l10n("dom.tooltip", domStrings,
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
  label: l10n("options.darkTheme.label2", toolboxStrings),
  ordinal: 1,
  stylesheets: ["chrome://devtools/skin/dark-theme.css"],
  classList: ["theme-dark"],
};

Tools.lightTheme = {
  id: "light",
  label: l10n("options.lightTheme.label2", toolboxStrings),
  ordinal: 2,
  stylesheets: ["chrome://devtools/skin/light-theme.css"],
  classList: ["theme-light"],
};

Tools.firebugTheme = {
  id: "firebug",
  label: l10n("options.firebugTheme.label2", toolboxStrings),
  ordinal: 3,
  stylesheets: ["chrome://devtools/skin/firebug-theme.css"],
  classList: ["theme-light", "theme-firebug"],
};

exports.defaultThemes = [
  Tools.darkTheme,
  Tools.lightTheme,
  Tools.firebugTheme,
];

/**
 * Lookup l10n string from a string bundle.
 *
 * @param {string} name
 *        The key to lookup.
 * @param {StringBundle} bundle
 *        The key to lookup.
 * @returns A localized version of the given key.
 */
function l10n(name, bundle, arg) {
  try {
    return arg ? bundle.formatStringFromName(name, [arg], 1)
    : bundle.GetStringFromName(name);
  } catch (ex) {
    console.log("Error reading '" + name + "'");
    throw new Error("l10n error with " + name);
  }
}

function functionkey(shortkey) {
  return shortkey.split("_")[1];
}
