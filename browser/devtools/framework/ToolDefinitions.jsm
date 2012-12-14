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

// Strings
XPCOMUtils.defineLazyGetter(this, "webConsoleStrings",
  function() Services.strings.createBundle(webConsoleProps));

XPCOMUtils.defineLazyGetter(this, "debuggerStrings",
  function() Services.strings.createBundle(debuggerProps));

XPCOMUtils.defineLazyGetter(this, "styleEditorStrings",
  function() Services.strings.createBundle(styleEditorProps));

XPCOMUtils.defineLazyGetter(this, "inspectorStrings",
  function() Services.strings.createBundle(inspectorProps));

// Definitions
let webConsoleDefinition = {
  id: "webconsole",
  key: l10n("cmd.commandkey", webConsoleStrings),
  accesskey: l10n("webConsoleCmd.accesskey", webConsoleStrings),
  modifiers: Services.appinfo.OS == "Darwin" ? "accel,alt" : "accel,shift",
  ordinal: 0,
  icon: "chrome://browser/skin/devtools/webconsole-tool-icon.png",
  url: "chrome://browser/content/devtools/webconsole.xul",
  label: l10n("ToolboxWebconsole.label", webConsoleStrings),
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
  ordinal: 1,
  killswitch: "devtools.debugger.enabled",
  icon: "chrome://browser/skin/devtools/tools-icons-small.png",
  url: "chrome://browser/content/debugger.xul",
  label: l10n("ToolboxDebugger.label", debuggerStrings),

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
  ordinal: 2,
  modifiers: osString == "Darwin" ? "accel,alt" : "accel,shift",
  icon: "chrome://browser/skin/devtools/tools-icons-small.png",
  url: "chrome://browser/content/devtools/inspector/inspector.xul",
  label: l10n("inspector.label", inspectorStrings),

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
  url: "chrome://browser/content/styleeditor.xul",

  isTargetSupported: function(target) {
    return !target.isRemote && !target.isChrome;
  },

  build: function(iframeWindow, toolbox) {
    let panel = new StyleEditorPanel(iframeWindow, toolbox);
    return panel.open();
  }
};

this.defaultTools = [
  styleEditorDefinition,
  webConsoleDefinition,
  debuggerDefinition,
  inspectorDefinition,
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
function l10n(name, bundle)
{
  try {
    return bundle.GetStringFromName(name);
  } catch (ex) {
    Services.console.logStringMessage("Error reading '" + name + "'");
    throw new Error("l10n error with " + name);
  }
}
