/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cu} = require("chrome");

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/devtools/gDevTools.jsm");

Object.defineProperty(exports, "Toolbox", {
  get: () => require("devtools/framework/toolbox").Toolbox
});
Object.defineProperty(exports, "TargetFactory", {
  get: () => require("devtools/framework/target").TargetFactory
});

loader.lazyGetter(this, "osString", () => Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).OS);

let events = require("sdk/system/events");

// Panels
loader.lazyGetter(this, "OptionsPanel", () => require("devtools/framework/toolbox-options").OptionsPanel);
loader.lazyGetter(this, "InspectorPanel", () => require("devtools/inspector/inspector-panel").InspectorPanel);
loader.lazyGetter(this, "WebConsolePanel", () => require("devtools/webconsole/panel").WebConsolePanel);
loader.lazyGetter(this, "DebuggerPanel", () => require("devtools/debugger/panel").DebuggerPanel);
loader.lazyGetter(this, "StyleEditorPanel", () => require("devtools/styleeditor/styleeditor-panel").StyleEditorPanel);
loader.lazyGetter(this, "ShaderEditorPanel", () => require("devtools/shadereditor/panel").ShaderEditorPanel);
loader.lazyGetter(this, "CanvasDebuggerPanel", () => require("devtools/canvasdebugger/panel").CanvasDebuggerPanel);
loader.lazyGetter(this, "WebAudioEditorPanel", () => require("devtools/webaudioeditor/panel").WebAudioEditorPanel);
loader.lazyGetter(this, "ProfilerPanel", () => require("devtools/profiler/panel"));
loader.lazyGetter(this, "NetMonitorPanel", () => require("devtools/netmonitor/panel").NetMonitorPanel);
loader.lazyGetter(this, "ScratchpadPanel", () => require("devtools/scratchpad/scratchpad-panel").ScratchpadPanel);

// Strings
const toolboxProps = "chrome://browser/locale/devtools/toolbox.properties";
const inspectorProps = "chrome://browser/locale/devtools/inspector.properties";
const debuggerProps = "chrome://browser/locale/devtools/debugger.properties";
const styleEditorProps = "chrome://browser/locale/devtools/styleeditor.properties";
const shaderEditorProps = "chrome://browser/locale/devtools/shadereditor.properties";
const canvasDebuggerProps = "chrome://browser/locale/devtools/canvasdebugger.properties";
const webAudioEditorProps = "chrome://browser/locale/devtools/webaudioeditor.properties";

const webConsoleProps = "chrome://browser/locale/devtools/webconsole.properties";
const profilerProps = "chrome://browser/locale/devtools/profiler.properties";
const netMonitorProps = "chrome://browser/locale/devtools/netmonitor.properties";
const scratchpadProps = "chrome://browser/locale/devtools/scratchpad.properties";
loader.lazyGetter(this, "toolboxStrings", () => Services.strings.createBundle(toolboxProps));
loader.lazyGetter(this, "webConsoleStrings", () => Services.strings.createBundle(webConsoleProps));
loader.lazyGetter(this, "debuggerStrings", () => Services.strings.createBundle(debuggerProps));
loader.lazyGetter(this, "styleEditorStrings", () => Services.strings.createBundle(styleEditorProps));
loader.lazyGetter(this, "shaderEditorStrings", () => Services.strings.createBundle(shaderEditorProps));
loader.lazyGetter(this, "canvasDebuggerStrings", () => Services.strings.createBundle(canvasDebuggerProps));
loader.lazyGetter(this, "webAudioEditorStrings", () => Services.strings.createBundle(webAudioEditorProps));
loader.lazyGetter(this, "inspectorStrings", () => Services.strings.createBundle(inspectorProps));
loader.lazyGetter(this, "profilerStrings",() => Services.strings.createBundle(profilerProps));
loader.lazyGetter(this, "netMonitorStrings", () => Services.strings.createBundle(netMonitorProps));
loader.lazyGetter(this, "scratchpadStrings", () => Services.strings.createBundle(scratchpadProps));

let Tools = {};
exports.Tools = Tools;

// Definitions
Tools.options = {
  id: "options",
  ordinal: 0,
  url: "chrome://browser/content/devtools/framework/toolbox-options.xul",
  icon: "chrome://browser/skin/devtools/tool-options.svg",
  invertIconForLightTheme: true,
  bgTheme: "theme-body",
  tooltip: l10n("optionsButton.tooltip", toolboxStrings),
  inMenu: false,
  isTargetSupported: function(target) {
    return true;
  },
  build: function(iframeWindow, toolbox) {
    let panel = new OptionsPanel(iframeWindow, toolbox);
    return panel.open();
  }
}

Tools.webConsole = {
  id: "webconsole",
  key: l10n("cmd.commandkey", webConsoleStrings),
  accesskey: l10n("webConsoleCmd.accesskey", webConsoleStrings),
  modifiers: Services.appinfo.OS == "Darwin" ? "accel,alt" : "accel,shift",
  ordinal: 1,
  icon: "chrome://browser/skin/devtools/tool-webconsole.svg",
  invertIconForLightTheme: true,
  url: "chrome://browser/content/devtools/webconsole.xul",
  label: l10n("ToolboxTabWebconsole.label", webConsoleStrings),
  menuLabel: l10n("MenuWebconsole.label", webConsoleStrings),
  tooltip: l10n("ToolboxWebconsole.tooltip", webConsoleStrings),
  inMenu: true,
  commands: "devtools/webconsole/console-commands",

  preventClosingOnKey: true,
  onkey: function(panel, toolbox) {
    if (toolbox.splitConsole)
      return toolbox.focusConsoleInput();

    panel.focusInput();
  },

  isTargetSupported: function(target) {
    return !target.isAddon;
  },
  build: function(iframeWindow, toolbox) {
    let panel = new WebConsolePanel(iframeWindow, toolbox);
    return panel.open();
  }
};

Tools.inspector = {
  id: "inspector",
  accesskey: l10n("inspector.accesskey", inspectorStrings),
  key: l10n("inspector.commandkey", inspectorStrings),
  ordinal: 2,
  modifiers: osString == "Darwin" ? "accel,alt" : "accel,shift",
  icon: "chrome://browser/skin/devtools/tool-inspector.svg",
  invertIconForLightTheme: true,
  url: "chrome://browser/content/devtools/inspector/inspector.xul",
  label: l10n("inspector.label", inspectorStrings),
  tooltip: l10n("inspector.tooltip", inspectorStrings),
  inMenu: true,
  commands: [
    "devtools/resize-commands",
    "devtools/inspector/inspector-commands",
  ],

  preventClosingOnKey: true,
  onkey: function(panel) {
    panel.toolbox.highlighterUtils.togglePicker();
  },

  isTargetSupported: function(target) {
    return !target.isAddon;
  },

  build: function(iframeWindow, toolbox) {
    let panel = new InspectorPanel(iframeWindow, toolbox);
    return panel.open();
  }
};

Tools.jsdebugger = {
  id: "jsdebugger",
  key: l10n("debuggerMenu.commandkey", debuggerStrings),
  accesskey: l10n("debuggerMenu.accesskey", debuggerStrings),
  modifiers: osString == "Darwin" ? "accel,alt" : "accel,shift",
  ordinal: 3,
  icon: "chrome://browser/skin/devtools/tool-debugger.svg",
  invertIconForLightTheme: true,
  highlightedicon: "chrome://browser/skin/devtools/tool-debugger-paused.svg",
  url: "chrome://browser/content/devtools/debugger.xul",
  label: l10n("ToolboxDebugger.label", debuggerStrings),
  tooltip: l10n("ToolboxDebugger.tooltip", debuggerStrings),
  inMenu: true,
  commands: "devtools/debugger/debugger-commands",

  isTargetSupported: function(target) {
    return true;
  },

  build: function(iframeWindow, toolbox) {
    let panel = new DebuggerPanel(iframeWindow, toolbox);
    return panel.open();
  }
};

Tools.styleEditor = {
  id: "styleeditor",
  key: l10n("open.commandkey", styleEditorStrings),
  ordinal: 4,
  accesskey: l10n("open.accesskey", styleEditorStrings),
  modifiers: "shift",
  icon: "chrome://browser/skin/devtools/tool-styleeditor.svg",
  invertIconForLightTheme: true,
  url: "chrome://browser/content/devtools/styleeditor.xul",
  label: l10n("ToolboxStyleEditor.label", styleEditorStrings),
  tooltip: l10n("ToolboxStyleEditor.tooltip2", styleEditorStrings),
  inMenu: true,
  commands: "devtools/styleeditor/styleeditor-commands",

  isTargetSupported: function(target) {
    return !target.isAddon;
  },

  build: function(iframeWindow, toolbox) {
    let panel = new StyleEditorPanel(iframeWindow, toolbox);
    return panel.open();
  }
};

Tools.shaderEditor = {
  id: "shadereditor",
  ordinal: 5,
  visibilityswitch: "devtools.shadereditor.enabled",
  icon: "chrome://browser/skin/devtools/tool-styleeditor.svg",
  invertIconForLightTheme: true,
  url: "chrome://browser/content/devtools/shadereditor.xul",
  label: l10n("ToolboxShaderEditor.label", shaderEditorStrings),
  tooltip: l10n("ToolboxShaderEditor.tooltip", shaderEditorStrings),

  isTargetSupported: function(target) {
    return !target.isAddon;
  },

  build: function(iframeWindow, toolbox) {
    let panel = new ShaderEditorPanel(iframeWindow, toolbox);
    return panel.open();
  }
};

Tools.canvasDebugger = {
  id: "canvasdebugger",
  ordinal: 6,
  visibilityswitch: "devtools.canvasdebugger.enabled",
  icon: "chrome://browser/skin/devtools/tool-styleeditor.svg",
  invertIconForLightTheme: true,
  url: "chrome://browser/content/devtools/canvasdebugger.xul",
  label: l10n("ToolboxCanvasDebugger.label", canvasDebuggerStrings),
  tooltip: l10n("ToolboxCanvasDebugger.tooltip", canvasDebuggerStrings),
  isTargetSupported: function(target) {
    return !target.isAddon;
  },
  build: function (iframeWindow, toolbox) {
    let panel = new CanvasDebuggerPanel(iframeWindow, toolbox);
    return panel.open();
  }
};

Tools.webAudioEditor = {
  id: "webaudioeditor",
  ordinal: 10,
  visibilityswitch: "devtools.webaudioeditor.enabled",
  icon: "chrome://browser/skin/devtools/tool-styleeditor.svg",
  invertIconForLightTheme: true,
  url: "chrome://browser/content/devtools/webaudioeditor.xul",
  label: l10n("ToolboxWebAudioEditor.label", webAudioEditorStrings),
  tooltip: l10n("ToolboxWebAudioEditor.tooltip", webAudioEditorStrings),
  isTargetSupported: function(target) {
    return !target.isAddon;
  },
  build: function(iframeWindow, toolbox) {
    let panel = new WebAudioEditorPanel(iframeWindow, toolbox);
    return panel.open();
  }
};

Tools.jsprofiler = {
  id: "jsprofiler",
  accesskey: l10n("profiler.accesskey", profilerStrings),
  key: l10n("profiler2.commandkey", profilerStrings),
  ordinal: 7,
  modifiers: "shift",
  visibilityswitch: "devtools.profiler.enabled",
  icon: "chrome://browser/skin/devtools/tool-profiler.svg",
  invertIconForLightTheme: true,
  url: "chrome://browser/content/devtools/profiler.xul",
  label: l10n("profiler.label", profilerStrings),
  tooltip: l10n("profiler.tooltip2", profilerStrings),
  inMenu: true,
  commands: "devtools/profiler/commands",

  isTargetSupported: function (target) {
    return !target.isAddon;
  },

  build: function (frame, target) {
    let panel = new ProfilerPanel(frame, target);
    return panel.open();
  }
};

Tools.netMonitor = {
  id: "netmonitor",
  accesskey: l10n("netmonitor.accesskey", netMonitorStrings),
  key: l10n("netmonitor.commandkey", netMonitorStrings),
  ordinal: 8,
  modifiers: osString == "Darwin" ? "accel,alt" : "accel,shift",
  visibilityswitch: "devtools.netmonitor.enabled",
  icon: "chrome://browser/skin/devtools/tool-network.svg",
  invertIconForLightTheme: true,
  url: "chrome://browser/content/devtools/netmonitor.xul",
  label: l10n("netmonitor.label", netMonitorStrings),
  tooltip: l10n("netmonitor.tooltip", netMonitorStrings),
  inMenu: true,

  isTargetSupported: function(target) {
    let root = target.client.mainRoot;
    return !target.isAddon && (root.traits.networkMonitor || !target.isApp);
  },

  build: function(iframeWindow, toolbox) {
    let panel = new NetMonitorPanel(iframeWindow, toolbox);
    return panel.open();
  }
};

Tools.scratchpad = {
  id: "scratchpad",
  ordinal: 9,
  visibilityswitch: "devtools.scratchpad.enabled",
  icon: "chrome://browser/skin/devtools/tool-scratchpad.svg",
  invertIconForLightTheme: true,
  url: "chrome://browser/content/devtools/scratchpad.xul",
  label: l10n("scratchpad.label", scratchpadStrings),
  tooltip: l10n("scratchpad.tooltip", scratchpadStrings),
  inMenu: false,
  commands: "devtools/scratchpad/scratchpad-commands",

  isTargetSupported: function(target) {
    return !target.isAddon && target.isRemote;
  },

  build: function(iframeWindow, toolbox) {
    let panel = new ScratchpadPanel(iframeWindow, toolbox);
    return panel.open();
  }
};

let defaultTools = [
  Tools.options,
  Tools.webConsole,
  Tools.inspector,
  Tools.jsdebugger,
  Tools.styleEditor,
  Tools.shaderEditor,
  Tools.canvasDebugger,
  Tools.webAudioEditor,
  Tools.jsprofiler,
  Tools.netMonitor,
  Tools.scratchpad
];

exports.defaultTools = defaultTools;

for (let definition of defaultTools) {
  gDevTools.registerTool(definition);
}

var unloadObserver = {
  observe: function(subject, topic, data) {
    if (subject.wrappedJSObject === require("@loader/unload")) {
      Services.obs.removeObserver(unloadObserver, "sdk:loader:destroy");
      for (let definition of gDevTools.getToolDefinitionArray()) {
        gDevTools.unregisterTool(definition.id);
      }
    }
  }
};
Services.obs.addObserver(unloadObserver, "sdk:loader:destroy", false);

events.emit("devtools-loaded", {});

/**
 * Lookup l10n string from a string bundle.
 *
 * @param {string} name
 *        The key to lookup.
 * @param {StringBundle} bundle
 *        The key to lookup.
 * @returns A localized version of the given key.
 */
function l10n(name, bundle)
{
  try {
    return bundle.GetStringFromName(name);
  } catch (ex) {
    Services.console.logStringMessage("Error reading '" + name + "'");
    throw new Error("l10n error with " + name);
  }
}
