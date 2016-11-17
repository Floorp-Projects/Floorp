/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const {AppManager} = require("devtools/client/webide/modules/app-manager");
const EventEmitter = require("devtools/shared/event-emitter");
const {RuntimeScanners, WiFiScanner} = require("devtools/client/webide/modules/runtimes");
const {Devices} = require("resource://devtools/shared/apps/Devices.jsm");
const {Task} = require("devtools/shared/task");
const utils = require("devtools/client/webide/modules/utils");

const Strings = Services.strings.createBundle("chrome://devtools/locale/webide.properties");

var RuntimeList;

module.exports = RuntimeList = function (window, parentWindow) {
  EventEmitter.decorate(this);
  this._doc = window.document;
  this._UI = parentWindow.UI;
  this._Cmds = parentWindow.Cmds;
  this._parentWindow = parentWindow;
  this._panelNodeEl = "button";
  this._panelBoxEl = "div";

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

  appManagerUpdate: function (event, what, details) {
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
    }
  },

  onWebIDEUpdate: function (event, what, details) {
    if (what == "busy" || what == "unbusy") {
      this.updateCommands();
    }
  },

  takeScreenshot: function () {
    this._Cmds.takeScreenshot();
  },

  showRuntimeDetails: function () {
    this._Cmds.showRuntimeDetails();
  },

  showDevicePreferences: function () {
    this._Cmds.showDevicePrefs();
  },

  showSettings: function () {
    this._Cmds.showSettings();
  },

  showTroubleShooting: function () {
    this._Cmds.showTroubleShooting();
  },

  showAddons: function () {
    this._Cmds.showAddons();
  },

  refreshScanners: function () {
    RuntimeScanners.scan();
  },

  updateCommands: function () {
    let doc = this._doc;

    // Runtime commands
    let screenshotCmd = doc.querySelector("#runtime-screenshot");
    let detailsCmd = doc.querySelector("#runtime-details");
    let disconnectCmd = doc.querySelector("#runtime-disconnect");
    let devicePrefsCmd = doc.querySelector("#runtime-preferences");
    let settingsCmd = doc.querySelector("#runtime-settings");

    if (AppManager.connected) {
      if (AppManager.deviceFront) {
        detailsCmd.removeAttribute("disabled");
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
      screenshotCmd.setAttribute("disabled", "true");
      disconnectCmd.setAttribute("disabled", "true");
      devicePrefsCmd.setAttribute("disabled", "true");
      settingsCmd.setAttribute("disabled", "true");
    }
  },

  update: function () {
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
        connectButton.textContent = r.name;

        connectButton.addEventListener("click", () => {
          this._UI.dismissErrorNotification();
          this._UI.connectToRuntime(r);
        }, true);
        panelItemNode.appendChild(connectButton);

        if (r.configure) {
          let configButton = doc.createElement(this._panelNodeEl);
          configButton.className = "configure-button";
          configButton.addEventListener("click", r.configure.bind(r), true);
          panelItemNode.appendChild(configButton);
        }

        parent.appendChild(panelItemNode);
      }
    }
  },

  destroy: function () {
    this._doc = null;
    AppManager.off("app-manager-update", this.appManagerUpdate);
    this._UI.off("webide-update", this.onWebIDEUpdate);
    this._UI = null;
    this._Cmds = null;
    this._parentWindow = null;
    this._panelNodeEl = null;
  }
};
