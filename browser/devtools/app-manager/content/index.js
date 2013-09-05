/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;
Cu.import("resource:///modules/devtools/gDevTools.jsm");
const {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
const {require} = devtools;
const {ConnectionManager, Connection} = require("devtools/client/connection-manager");

let connection;

window.addEventListener("message", function(event) {
  try {
    let json = JSON.parse(event.data);
    if (json.name == "connection") {
      let cid = +json.cid;
      for (let c of ConnectionManager.connections) {
        if (c.uid == cid) {
          connection = c;
          onNewConnection();
          break;
        }
      }
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
    if (!newSelection) continue;
    if (oldSelection) oldSelection.removeAttribute("selected");
    newSelection.setAttribute("selected", "true");
  }
}
selectTab("projects");

