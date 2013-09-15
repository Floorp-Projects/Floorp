/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;
Cu.import("resource:///modules/devtools/gDevTools.jsm");
const {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
const {require} = devtools;
const {ConnectionManager, Connection} = require("devtools/client/connection-manager");
const prefs = require('sdk/preferences/service');

let connection;

window.addEventListener("message", function(event) {
  try {
    let json = JSON.parse(event.data);
    switch (json.name) {
      case "connection":
        let cid = +json.cid;
        for (let c of ConnectionManager.connections) {
          if (c.uid == cid) {
            connection = c;
            onNewConnection();
            break;
          }
        }
        break;
      case "closeHelp":
        selectTab("projects");
        break;
      default:
        Cu.reportError("Unknown message: " + json.name);
    }
  } catch(e) { Cu.reportError(e); }

  // Forward message
  let panels = document.querySelectorAll(".panel");
  for (let frame of panels) {
    frame.contentWindow.postMessage(event.data, "*");
  }
}, false);

function onNewConnection() {
  connection.on(Connection.Status.CONNECTED, () => {
    document.querySelector("#content").classList.add("connected");
  });
  connection.on(Connection.Status.DISCONNECTED, () => {
    document.querySelector("#content").classList.remove("connected");
  });
}

function selectTab(id) {
  for (let type of ["button", "panel"]) {
    let oldSelection = document.querySelector("." + type + "[selected]");
    let newSelection = document.querySelector("." + id + "-" + type);
    if (oldSelection) oldSelection.removeAttribute("selected");
    if (newSelection) newSelection.setAttribute("selected", "true");
  }
  if (id != "help") {
    // Might be the first time the user is accessing the actual app manager
    prefs.set("devtools.appmanager.firstrun", false);
  }
}

let firstRun = prefs.get("devtools.appmanager.firstrun");
if (firstRun) {
  selectTab("help");
} else {
  selectTab("projects");
}
