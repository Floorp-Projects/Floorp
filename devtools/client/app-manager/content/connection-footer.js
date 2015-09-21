/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;
const Ci = Components.interfaces;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/devtools/client/framework/gDevTools.jsm");

const {Simulator} = Cu.import("resource://gre/modules/devtools/shared/apps/Simulator.jsm")
const {Devices} = Cu.import("resource://gre/modules/devtools/shared/apps/Devices.jsm");
const {require} = Cu.import("resource://gre/modules/devtools/shared/Loader.jsm", {});

const {ConnectionManager, Connection} = require("devtools/shared/client/connection-manager");
const {getDeviceFront} = require("devtools/server/actors/device");
const ConnectionStore = require("devtools/client/app-manager/connection-store");
const DeviceStore = require("devtools/client/app-manager/device-store");
const simulatorsStore = require("devtools/client/app-manager/simulators-store");
const adbStore = require("devtools/client/app-manager/builtin-adb-store");

window.addEventListener("unload", function onUnload() {
  window.removeEventListener("unload", onUnload);
  UI.destroy();
});

var UI = {
  init: function() {
    this.useFloatingScrollbarsIfNeeded();
    let connections = ConnectionManager.connections;
    if (connections.length > 0) {
      let hash = window.location.hash;
      if (hash) {
        let res = (/cid=([^&]+)/).exec(hash)
        if (res) {
          let [,cid] = res;
          this.connection = connections.filter((({uid}) => uid == cid))[0];
        }
      }
      if (!this.connection) {
        // We take the first connection available.
        this.connection = connections[0];
      }
    } else {
      let host = Services.prefs.getCharPref("devtools.debugger.remote-host");
      let port = Services.prefs.getIntPref("devtools.debugger.remote-port");
      this.connection = ConnectionManager.createConnection(host, port);
    }

    window.location.hash = "cid=" + this.connection.uid;
    window.parent.postMessage(JSON.stringify({name:"connection",cid:this.connection.uid}), "*");

    this.store = Utils.mergeStores({
      "device": new DeviceStore(this.connection),
      "connection": new ConnectionStore(this.connection),
      "simulators": simulatorsStore,
      "adb": adbStore
    });

    let pre = document.querySelector("#logs > pre");
    pre.textContent = this.connection.logs;
    pre.scrollTop = pre.scrollTopMax;
    this.connection.on(Connection.Events.NEW_LOG, this._onNewLog);

    this.template = new Template(document.body, this.store, Utils.l10n);
    this.template.start();

    this._onSimulatorConnected = this._onSimulatorConnected.bind(this);
    this._onSimulatorDisconnected = this._onSimulatorDisconnected.bind(this);
  },

  destroy: function() {
    this.store.destroy();
    this.connection.off(Connection.Events.NEW_LOG, this._onNewLog);
    this.template.destroy();
  },

  _onNewLog: function(event, str) {
    let pre = document.querySelector("#logs > pre");
    pre.textContent += "\n" + str;
    pre.scrollTop = pre.scrollTopMax;
  },

  useFloatingScrollbarsIfNeeded: function() {
    if (Services.appinfo.OS == "Darwin") {
      return;
    }
    let scrollbarsUrl = Services.io.newURI("chrome://browser/skin/devtools/floating-scrollbars-light.css", null, null);
    let winUtils = window.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    winUtils.loadSheet(scrollbarsUrl, winUtils.AGENT_SHEET);
    let computedStyle = window.getComputedStyle(document.documentElement);
    if (computedStyle) { // Force a reflow to take the new css into account
      let display = computedStyle.display; // Save display value
      document.documentElement.style.display = "none";
      window.getComputedStyle(document.documentElement).display; // Flush
      document.documentElement.style.display = display; // Restore
    }
  },

  disconnect: function() {
    this.connection.disconnect();
  },

  connect: function() {
    this.connection.connect();
  },

  editConnectionParameters: function() {
    document.body.classList.add("edit-connection");
    document.querySelector("input.host").focus();
  },

  saveConnectionInfo: function() {
    document.body.classList.remove("edit-connection");
    document.querySelector("#connect-button").focus();
    let host = document.querySelector("input.host").value;
    let port = document.querySelector("input.port").value;
    this.connection.port = port;
    this.connection.host = host;
    Services.prefs.setCharPref("devtools.debugger.remote-host", host);
    Services.prefs.setIntPref("devtools.debugger.remote-port", port);
  },

  showSimulatorList: function() {
    document.body.classList.add("show-simulators");
  },

  cancelShowSimulatorList: function() {
    document.body.classList.remove("show-simulators");
  },

  installSimulator: function() {
    let url = "https://developer.mozilla.org/docs/Mozilla/Firefox_OS/Using_the_App_Manager#Simulator";
    window.open(url);
  },

  startSimulator: function(version) {
    this._portBeforeSimulatorStarted = this.connection.port;
    let port = ConnectionManager.getFreeTCPPort();
    let simulator = Simulator.getByName(version);
    if (!simulator) {
      this.connection.log("Error: can't find simulator: " + version);
      return;
    }
    if (!simulator.launch) {
      this.connection.log("Error: invalid simulator: " + version);
      return;
    }
    this.connection.log("Found simulator: " + version);
    this.connection.log("Starting simulator...");

    this.simulator = simulator;
    this.simulator.launch({ port: port })
      .then(() => {
        this.connection.log("Simulator ready. Connecting.");
        this.connection.port = port;
        this.connection.host = "localhost";
        this.connection.once("connected",
                             this._onSimulatorConnected);
        this.connection.once("disconnected",
                             this._onSimulatorDisconnected);
        this.connection.keepConnecting = true;
        this.connection.connect();
      });
    document.body.classList.remove("show-simulators");
  },

  _onSimulatorConnected: function() {
    this.connection.log("Connected to simulator.");
    this.connection.keepConnecting = false;

    // This doesn't change the current (successful) connection,
    // but makes sure that when the simulator is disconnected, the
    // connection doesn't end up with a random port number (from
    // getFreeTCPPort).
    this.connection.port = this._portBeforeSimulatorStarted;
  },

  _onSimulatorDisconnected: function() {
    this.connection.off("connected", this._onSimulatorConnected);
  },

  connectToAdbDevice: function(name) {
    let device = Devices.getByName(name);
    device.connect().then((port) => {
      this.connection.host = "localhost";
      this.connection.port = port;
      this.connect();
    });
  },
  
  screenshot: function() {
    this.connection.client.listTabs(
      response => {
        let front = getDeviceFront(this.connection.client, response);
        front.screenshotToBlob().then(blob => {
          let topWindow = Services.wm.getMostRecentWindow("navigator:browser");
          let gBrowser = topWindow.gBrowser;
          let url = topWindow.URL.createObjectURL(blob);
          let tab = gBrowser.selectedTab = gBrowser.addTab(url);
          tab.addEventListener("TabClose", function onTabClose() {
            tab.removeEventListener("TabClose", onTabClose, false);
            topWindow.URL.revokeObjectURL(url);
          }, false);
        }).then(null, console.error);
      }
    );
  },
}
