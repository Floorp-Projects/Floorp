/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "WebConsoleDefinition" ];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "WebConsoleUtils",
                                  "resource://gre/modules/devtools/WebConsoleUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "HUDService",
                                  "resource:///modules/HUDService.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "EventEmitter",
                                  "resource:///modules/devtools/EventEmitter.jsm");

const STRINGS_URI = "chrome://browser/locale/devtools/webconsole.properties";
let l10n = new WebConsoleUtils.l10n(STRINGS_URI);

/**
 * The external API allowing us to be registered with DevTools.jsm
 */
this.WebConsoleDefinition = {
  id: "webconsole",
  key: l10n.getStr("cmd.commandkey"),
  accesskey: l10n.getStr("webConsoleCmd.accesskey"),
  modifiers: Services.appinfo.OS == "Darwin" ? "accel,alt" : "accel,shift",
  ordinal: 0,
  icon: "chrome://browser/skin/devtools/webconsole-tool-icon.png",
  url: "chrome://browser/content/devtools/webconsole.xul",
  label: l10n.getStr("ToolboxWebconsole.label"),
  isTargetSupported: function(target) {
    return true;
  },
  build: function(iframeWindow, toolbox) {
    return new WebConsolePanel(iframeWindow, toolbox);
  }
};

/**
 * A DevToolPanel that controls the Web Console.
 */
function WebConsolePanel(iframeWindow, toolbox) {
  this._frameWindow = iframeWindow;
  this._toolbox = toolbox;
  new EventEmitter(this);

  let tab = this._toolbox._getHostTab();
  let parentDoc = iframeWindow.document.defaultView.parent.document;
  let iframe = parentDoc.getElementById("toolbox-panel-iframe-webconsole");
  this.hud = HUDService.activateHUDForContext(tab, iframe, toolbox.target);

  let hudId = this.hud.hudId;
  let onOpen = function _onWebConsoleOpen(aSubject)
  {
    aSubject.QueryInterface(Ci.nsISupportsString);
    if (hudId == aSubject.data) {
      Services.obs.removeObserver(onOpen, "web-console-created");
      this.setReady();
    }
  }.bind(this);

  Services.obs.addObserver(onOpen, "web-console-created", false);
}

WebConsolePanel.prototype = {
  get target() this._toolbox.target,

  _isReady: false,
  get isReady() this._isReady,

  destroy: function WCP_destroy()
  {
    let hudId = this.hud.hudId;

    let onClose = function _onWebConsoleClose(aSubject)
    {
      aSubject.QueryInterface(Ci.nsISupportsString);
      if (hudId == aSubject.data) {
        Services.obs.removeObserver(onClose, "web-console-destroyed");
        this.emit("destroyed");
      }
    }.bind(this);

    Services.obs.addObserver(onClose, "web-console-destroyed", false);
    HUDService.deactivateHUDForContext(this.hud.tab, false);
  },

  setReady: function WCP_setReady()
  {
    this._isReady = true;
    this.emit("ready");
  },
}
