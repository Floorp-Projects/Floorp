/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {require} = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
const {AppManager} = require("devtools/client/webide/modules/app-manager");
const {Connection} = require("devtools/shared/client/connection-manager");

window.addEventListener("load", function() {
  document.querySelector("#close").onclick = CloseUI;
  AppManager.on("app-manager-update", OnAppManagerUpdate);
  BuildUI();
}, {capture: true, once: true});

window.addEventListener("unload", function() {
  AppManager.off("app-manager-update", OnAppManagerUpdate);
}, {once: true});

function CloseUI() {
  window.parent.UI.openProject();
}

function OnAppManagerUpdate(what) {
  if (what == "connection" || what == "runtime-global-actors") {
    BuildUI();
  }
}

function generateFields(json) {
  const table = document.querySelector("table");
  for (const name in json) {
    const tr = document.createElement("tr");
    let td = document.createElement("td");
    td.textContent = name;
    tr.appendChild(td);
    td = document.createElement("td");
    td.textContent = json[name];
    tr.appendChild(td);
    table.appendChild(tr);
  }
}

// Used by tests
/* exported getDescriptionPromise */
var getDescriptionPromise;
function BuildUI() {
  const table = document.querySelector("table");
  table.innerHTML = "";
  if (AppManager.connection &&
      AppManager.connection.status == Connection.Status.CONNECTED &&
      AppManager.deviceFront) {
    getDescriptionPromise = AppManager.deviceFront.getDescription()
                            .then(json => generateFields(json));
  } else {
    CloseUI();
  }
}
