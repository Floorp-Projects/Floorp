/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;
const {Services} = Cu.import("resource://gre/modules/Services.jsm");
const {require} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {}).devtools;
const {AppManager} = require("devtools/webide/app-manager");
const {Connection} = require("devtools/client/connection-manager");

window.addEventListener("load", function onLoad() {
  window.removeEventListener("load", onLoad);
  document.querySelector("#close").onclick = CloseUI;
  AppManager.on("app-manager-update", OnAppManagerUpdate);
  BuildUI();
}, true);

window.addEventListener("unload", function onUnload() {
  window.removeEventListener("unload", onUnload);
  AppManager.off("app-manager-update", OnAppManagerUpdate);
});

function CloseUI() {
  window.parent.UI.openProject();
}

function OnAppManagerUpdate(event, what) {
  if (what == "connection" || what == "list-tabs-response") {
    BuildUI();
  }
}

let getDescriptionPromise; // Used by tests
function BuildUI() {
  let table = document.querySelector("table");
  table.innerHTML = "";
  if (AppManager.connection &&
      AppManager.connection.status == Connection.Status.CONNECTED &&
      AppManager.deviceFront) {
    getDescriptionPromise = AppManager.deviceFront.getDescription();
    getDescriptionPromise.then(json => {
      for (let name in json) {
        let tr = document.createElement("tr");
        let td = document.createElement("td");
        td.textContent = name;
        tr.appendChild(td);
        td = document.createElement("td");
        td.textContent = json[name];
        tr.appendChild(td);
        table.appendChild(tr);
      }
    });
  } else {
    CloseUI();
  }
}
