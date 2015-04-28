/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {Cu} = require("chrome");

const {Services} = Cu.import("resource://gre/modules/Services.jsm");
const {AppProjects} = require("devtools/app-manager/app-projects");
const {AppManager} = require("devtools/webide/app-manager");
const {Promise: promise} = Cu.import("resource://gre/modules/Promise.jsm", {});
const EventEmitter = require("devtools/toolkit/event-emitter");
const {RuntimeScanners, WiFiScanner} = require("devtools/webide/runtimes");
const {Devices} = Cu.import("resource://gre/modules/devtools/Devices.jsm");
const {Task} = Cu.import("resource://gre/modules/Task.jsm", {});
const utils = require("devtools/webide/utils");

const Strings = Services.strings.createBundle("chrome://browser/locale/devtools/webide.properties");

let RuntimeList;

module.exports = RuntimeList = function(window, parentWindow) {
  EventEmitter.decorate(this);
  this._doc = window.document;
  this._UI = parentWindow.UI;
  this._Cmds = parentWindow.Cmds;
  this._parentWindow = parentWindow;
  this._panelNodeEl = "toolbarbutton";
  this._panelBoxEl = "hbox";
  this._sidebarsEnabled = Services.prefs.getBoolPref("devtools.webide.sidebars");

  if (this._sidebarsEnabled) {
    this._panelNodeEl = "button";
    this._panelBoxEl = "div";
  }

  this.onWebIDEUpdate = this.onWebIDEUpdate.bind(this);
  this._UI.on("webide-update", this.onWebIDEUpdate);

  AppManager.init();
  this.appManagerUpdate = this.appManagerUpdate.bind(this);
  AppManager.on("app-manager-update", this.appManagerUpdate);
};

RuntimeList.prototype = {
  get doc() {
    return this._doc;
  },

  get sidebarsEnabled() {
    return this._sidebarsEnabled;
  },

  appManagerUpdate: function(event, what, details) {
    // Got a message from app-manager.js
    // See AppManager.update() for descriptions of what these events mean.
    switch (what) {
      case "runtime-list":
        this.update();
        break;
      case "connection":
      case "runtime-global-actors":
        this.updateCommands();
        break;
    };
  },

  onWebIDEUpdate: function(event, what, details) {
    if (what == "busy" || what == "unbusy") {
      this.updateCommands();
    }
  },

  takeScreenshot: function() {
    return this._UI.busyUntil(AppManager.deviceFront.screenshotToDataURL().then(longstr => {
       return longstr.string().then(dataURL => {
         longstr.release().then(null, console.error);
         this._UI.openInBrowser(dataURL);
       });
    }), "taking screenshot");
  },

  showRuntimeDetails: function() {
    this._Cmds.showRuntimeDetails();
  },

  showPermissionsTable: function() {
    this._Cmds.showPermissionsTable();
  },

  showDevicePreferences: function() {
    this._Cmds.showDevicePrefs();
  },

  showSettings: function() {
    this._Cmds.showSettings();
  },

  showTroubleShooting: function() {
    this._Cmds.showTroubleShooting();
  },

  showAddons: function() {
    this._Cmds.showAddons();
  },

  updateCommands: function() {
    let doc = this._doc;

    // Runtime commands
    let screenshotCmd = doc.querySelector("#runtime-screenshot");
    let permissionsCmd = doc.querySelector("#runtime-permissions");
    let detailsCmd = doc.querySelector("#runtime-details");
    let disconnectCmd = doc.querySelector("#runtime-disconnect");
    let devicePrefsCmd = doc.querySelector("#runtime-preferences");
    let settingsCmd = doc.querySelector("#runtime-settings");

    if (AppManager.connected) {
      if (AppManager.deviceFront) {
        detailsCmd.removeAttribute("disabled");
        permissionsCmd.removeAttribute("disabled");
        screenshotCmd.removeAttribute("disabled");
      }
      if (AppManager.preferenceFront) {
        devicePrefsCmd.removeAttribute("disabled");
      }
      if (AppManager.settingsFront) {
        settingsCmd.removeAttribute("disabled");
      }
      disconnectCmd.removeAttribute("disabled");
    } else {
      detailsCmd.setAttribute("disabled", "true");
      permissionsCmd.setAttribute("disabled", "true");
      screenshotCmd.setAttribute("disabled", "true");
      disconnectCmd.setAttribute("disabled", "true");
      devicePrefsCmd.setAttribute("disabled", "true");
      settingsCmd.setAttribute("disabled", "true");
    }
  },

  update: function() {
    let doc = this._doc;
    let wifiHeaderNode = doc.querySelector("#runtime-header-wifi");

    if (WiFiScanner.allowed) {
      wifiHeaderNode.removeAttribute("hidden");
    } else {
      wifiHeaderNode.setAttribute("hidden", "true");
    }

    let usbListNode = doc.querySelector("#runtime-panel-usb");
    let wifiListNode = doc.querySelector("#runtime-panel-wifi");
    let simulatorListNode = doc.querySelector("#runtime-panel-simulator");
    let otherListNode = doc.querySelector("#runtime-panel-other");
    let noHelperNode = doc.querySelector("#runtime-panel-noadbhelper");
    let noUSBNode = doc.querySelector("#runtime-panel-nousbdevice");

    if (Devices.helperAddonInstalled) {
      noHelperNode.setAttribute("hidden", "true");
    } else {
      noHelperNode.removeAttribute("hidden");
    }

    let runtimeList = AppManager.runtimeList;

    if (!runtimeList) {
      return;
    }

    if (runtimeList.usb.length === 0 && Devices.helperAddonInstalled) {
      noUSBNode.removeAttribute("hidden");
    } else {
      noUSBNode.setAttribute("hidden", "true");
    }

    for (let [type, parent] of [
      ["usb", usbListNode],
      ["wifi", wifiListNode],
      ["simulator", simulatorListNode],
      ["other", otherListNode],
    ]) {
      while (parent.hasChildNodes()) {
        parent.firstChild.remove();
      }
      for (let runtime of runtimeList[type]) {
        let r = runtime;
        let panelItemNode = doc.createElement(this._panelBoxEl);
        panelItemNode.className = "panel-item-complex";

        let connectButton = doc.createElement(this._panelNodeEl);
        connectButton.className = "panel-item runtime-panel-item-" + type;

        if (this._sidebarsEnabled) {
          connectButton.textContent = r.name;
        } else {
          connectButton.setAttribute("label", r.name);
          connectButton.setAttribute("flex", "1");
        }

        connectButton.addEventListener("click", () => {
          if (!this._sidebarsEnabled) {
            this._UI.hidePanels();
          }
          this._UI.dismissErrorNotification();
          this._UI.connectToRuntime(r);
        }, true);
        panelItemNode.appendChild(connectButton);

        if (r.configure && this._UI.isRuntimeConfigurationEnabled()) {
          let configButton = doc.createElement(this._panelNodeEl);
          configButton.className = "configure-button";
          configButton.addEventListener("click", r.configure.bind(r), true);
          panelItemNode.appendChild(configButton);
        }

        parent.appendChild(panelItemNode);
      }
    }
  },

  destroy: function() {
    this._doc = null;
    AppManager.off("app-manager-update", this.appManagerUpdate);
    if (this.sidebarsEnabled) {
      this._UI.off("webide-update", this.onWebIDEUpdate);
    }
    this._UI = null;
    this._Cmds = null;
    this._parentWindow = null;
    this._panelNodeEl = null;
    this._sidebarsEnabled = null;
  }
};
