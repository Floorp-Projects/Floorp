/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;
const Ci = Components.interfaces;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/devtools/gDevTools.jsm");

const {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
const {require} = devtools;

const {ConnectionManager, Connection} = require("devtools/client/connection-manager");
const ConnectionStore = require("devtools/app-manager/connection-store");
const DeviceStore = require("devtools/app-manager/device-store");

let UI = {
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
    });

    let pre = document.querySelector("#logs > pre");
    pre.textContent = this.connection.logs;
    pre.scrollTop = pre.scrollTopMax;
    this.connection.on(Connection.Events.NEW_LOG, (event, str) => {
      pre.textContent += "\n" + str;
      pre.scrollTop = pre.scrollTopMax;
    });

    this.template = new Template(document.body, this.store, Utils.l10n);
    this.template.start();
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
}
