/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
                          "defaultTools",
                          "webConsoleDefinition",
                          "debuggerDefinition",
                          "inspectorDefinition",
                          "styleEditorDefinition"
                        ];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

const inspectorProps = "chrome://browser/locale/devtools/inspector.properties";
const debuggerProps = "chrome://browser/locale/devtools/debugger.properties";
const styleEditorProps = "chrome://browser/locale/devtools/styleeditor.properties";
const webConsoleProps = "chrome://browser/locale/devtools/webconsole.properties";
const profilerProps = "chrome://browser/locale/devtools/profiler.properties";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/devtools/EventEmitter.jsm");

XPCOMUtils.defineLazyGetter(this, "osString",
  function() Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).OS);

// Panels
XPCOMUtils.defineLazyModuleGetter(this, "WebConsolePanel",
  "resource:///modules/WebConsolePanel.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DebuggerPanel",
  "resource:///modules/devtools/DebuggerPanel.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "StyleEditorPanel",
  "resource:///modules/devtools/StyleEditorPanel.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "InspectorPanel",
  "resource:///modules/devtools/InspectorPanel.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ProfilerPanel",
  "resource:///modules/devtools/ProfilerPanel.jsm");

// Strings
XPCOMUtils.defineLazyGetter(this, "webConsoleStrings",
  function() Services.strings.createBundle(webConsoleProps));

XPCOMUtils.defineLazyGetter(this, "debuggerStrings",
  function() Services.strings.createBundle(debuggerProps));

XPCOMUtils.defineLazyGetter(this, "styleEditorStrings",
  function() Services.strings.createBundle(styleEditorProps));

XPCOMUtils.defineLazyGetter(this, "inspectorStrings",
  function() Services.strings.createBundle(inspectorProps));

XPCOMUtils.defineLazyGetter(this, "profilerStrings",
  function() Services.strings.createBundle(profilerProps));

// Definitions
let webConsoleDefinition = {
  id: "webconsole",
  key: l10n("cmd.commandkey", webConsoleStrings),
  accesskey: l10n("webConsoleCmd.accesskey", webConsoleStrings),
  modifiers: Services.appinfo.OS == "Darwin" ? "accel,alt" : "accel,shift",
  ordinal: 0,
  icon: "chrome://browser/skin/devtools/tool-webconsole.png",
  url: "chrome://browser/content/devtools/webconsole.xul",
  label: l10n("ToolboxWebconsole.label", webConsoleStrings),
  tooltip: l10n("ToolboxWebconsole.tooltip", webConsoleStrings),

  isTargetSupported: function(target) {
    return true;
  },
  build: function(iframeWindow, toolbox) {
    let panel = new WebConsolePanel(iframeWindow, toolbox);
    return panel.open();
  }
};

let debuggerDefinition = {
  id: "jsdebugger",
  key: l10n("open.commandkey", debuggerStrings),
  accesskey: l10n("debuggerMenu.accesskey", debuggerStrings),
  modifiers: osString == "Darwin" ? "accel,alt" : "accel,shift",
  ordinal: 2,
  killswitch: "devtools.debugger.enabled",
  icon: "chrome://browser/skin/devtools/tool-debugger.png",
  url: "chrome://browser/content/debugger.xul",
  label: l10n("ToolboxDebugger.label", debuggerStrings),
  tooltip: l10n("ToolboxDebugger.tooltip", debuggerStrings),

  isTargetSupported: function(target) {
    return true;
  },

  build: function(iframeWindow, toolbox) {
    let panel = new DebuggerPanel(iframeWindow, toolbox);
    return panel.open();
  }
};

let inspectorDefinition = {
  id: "inspector",
  accesskey: l10n("inspector.accesskey", inspectorStrings),
  key: l10n("inspector.commandkey", inspectorStrings),
  ordinal: 1,
  modifiers: osString == "Darwin" ? "accel,alt" : "accel,shift",
  icon: "chrome://browser/skin/devtools/tool-inspector.png",
  url: "chrome://browser/content/devtools/inspector/inspector.xul",
  label: l10n("inspector.label", inspectorStrings),
  tooltip: l10n("inspector.tooltip", inspectorStrings),

  isTargetSupported: function(target) {
    return !target.isRemote;
  },

  build: function(iframeWindow, toolbox) {
    let panel = new InspectorPanel(iframeWindow, toolbox);
    return panel.open();
  }
};

let styleEditorDefinition = {
  id: "styleeditor",
  key: l10n("open.commandkey", styleEditorStrings),
  ordinal: 3,
  accesskey: l10n("open.accesskey", styleEditorStrings),
  modifiers: "shift",
  label: l10n("ToolboxStyleEditor.label", styleEditorStrings),
  icon: "chrome://browser/skin/devtools/tool-styleeditor.png",
  url: "chrome://browser/content/styleeditor.xul",
  tooltip: l10n("ToolboxStyleEditor.tooltip", styleEditorStrings),

  isTargetSupported: function(target) {
    return !target.isRemote;
  },

  build: function(iframeWindow, toolbox) {
    let panel = new StyleEditorPanel(iframeWindow, toolbox);
    return panel.open();
  }
};

let profilerDefinition = {
  id: "jsprofiler",
  accesskey: l10n("profiler.accesskey", profilerStrings),
  key: l10n("profiler.commandkey", profilerStrings),
  ordinal: 4,
  modifiers: "shift",
  killswitch: "devtools.profiler.enabled",
  url: "chrome://browser/content/profiler.xul",
  label: l10n("profiler.label", profilerStrings),
  icon: "chrome://browser/skin/devtools/tool-profiler.png",
  tooltip: l10n("profiler.tooltip", profilerStrings),

  isTargetSupported: function (target) {
    return true;
  },

  build: function (frame, target) {
    let panel = new ProfilerPanel(frame, target);
    return panel.open();
  }
};


this.defaultTools = [
  styleEditorDefinition,
  webConsoleDefinition,
  debuggerDefinition,
  inspectorDefinition,
];

if (Services.prefs.getBoolPref("devtools.profiler.enabled")) {
  defaultTools.push(profilerDefinition);
}

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
