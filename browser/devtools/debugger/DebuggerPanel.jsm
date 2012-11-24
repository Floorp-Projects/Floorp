/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

this.EXPORTED_SYMBOLS = ["DebuggerDefinition"];

const STRINGS_URI = "chrome://browser/locale/devtools/debugger.properties";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "EventEmitter",
                                  "resource:///modules/devtools/EventEmitter.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DebuggerServer",
                                  "resource://gre/modules/devtools/dbg-server.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "gDevTools",
                                  "resource:///modules/devtools/gDevTools.jsm");
XPCOMUtils.defineLazyGetter(this, "_strings",
  function() Services.strings.createBundle(STRINGS_URI));

XPCOMUtils.defineLazyGetter(this, "osString", function() {
  return Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).OS;
});

this.DebuggerDefinition = {
  id: "jsdebugger",
  key: l10n("open.commandkey"),
  accesskey: l10n("debuggerMenu.accesskey"),
  modifiers: osString == "Darwin" ? "accel,alt" : "accel,shift",
  ordinal: 1,
  killswitch: "devtools.debugger.enabled",
  icon: "chrome://browser/skin/devtools/tools-icons-small.png",
  url: "chrome://browser/content/debugger.xul",
  label: l10n("ToolboxDebugger.label"),

  isTargetSupported: function(target) {
    return true;
  },

  build: function(iframeWindow, toolbox) {
    return new DebuggerPanel(iframeWindow, toolbox);
  }
};


function DebuggerPanel(iframeWindow, toolbox) {
  this._toolbox = toolbox;
  this._controller = iframeWindow.DebuggerController;
  this._view = iframeWindow.DebuggerView;
  this._controller._target = this.target;
  this._bkp = this._controller.Breakpoints;
  this.panelWin = iframeWindow;

  this._ensureOnlyOneRunningDebugger();
  if (!this.target.isRemote) {
    if (!DebuggerServer.initialized) {
      DebuggerServer.init();
      DebuggerServer.addBrowserActors();
    }
  }

  let onDebuggerLoaded = function () {
    iframeWindow.removeEventListener("Debugger:Loaded", onDebuggerLoaded, true);
    this.setReady();
  }.bind(this);

  let onDebuggerConnected = function () {
    iframeWindow.removeEventListener("Debugger:Connected",
      onDebuggerConnected, true);
    this.emit("connected");
  }.bind(this);

  iframeWindow.addEventListener("Debugger:Loaded", onDebuggerLoaded, true);
  iframeWindow.addEventListener("Debugger:Connected",
    onDebuggerConnected, true);

  new EventEmitter(this);
}

DebuggerPanel.prototype = {
  // DevToolPanel API
  get target() this._toolbox.target,

  get isReady() this._isReady,

  setReady: function() {
    this._isReady = true;
    this.emit("ready");
  },

  destroy: function() {
    delete this._toolbox;
    delete this._target;
    delete this._controller;
    delete this._view;
    delete this._bkp;
    delete this.panelWin;
  },

  // DebuggerPanel API

  addBreakpoint: function() {
    this._bkp.addBreakpoint.apply(this._bkp, arguments);
  },

  removeBreakpoint: function() {
    this._bkp.removeBreakpoint.apply(this._bkp, arguments);
  },

  getBreakpoint: function() {
    return this._bkp.getBreakpoint.apply(this._bkp, arguments);
  },

  getAllBreakpoints: function() {
    return this._bkp.store;
  },

  // Private

  _ensureOnlyOneRunningDebugger: function() {
    // FIXME
  },
};

/**
 * Lookup l10n string from a string bundle.
 * @param {string} aName The key to lookup.
 * @returns A localized version of the given key.
 */
function l10n(aName)
{
  try {
    return _strings.GetStringFromName(aName);
  } catch (ex) {
    Services.console.logStringMessage("Error reading '" + aName + "'");
    throw new Error("l10n error with " + aName);
  }
}
