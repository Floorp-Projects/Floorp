/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;
const {require} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
const {AppManager} = require("devtools/webide/app-manager");
const {Connection} = require("devtools/client/connection-manager");
const ConfigView = require("devtools/webide/config-view");

var configView = new ConfigView(window);

window.addEventListener("load", function onLoad() {
  window.removeEventListener("load", onLoad);
  AppManager.on("app-manager-update", OnAppManagerUpdate);
  document.getElementById("close").onclick = CloseUI;
  document.getElementById("device-fields").onchange = UpdateField;
  document.getElementById("device-fields").onclick = CheckReset;
  document.getElementById("search-bar").onkeyup = document.getElementById("search-bar").onclick = SearchField;
  document.getElementById("custom-value").onclick = UpdateNewField;
  document.getElementById("custom-value-type").onchange = ClearNewFields;
  document.getElementById("add-custom-field").onkeyup = CheckNewFieldSubmit;
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
  if (what == "connection" || what == "runtime-global-actors") {
    BuildUI();
  }
}

function CheckNewFieldSubmit(event) {
  configView.checkNewFieldSubmit(event);
}

function UpdateNewField() {
  configView.updateNewField();
}

function ClearNewFields() {
  configView.clearNewFields();
}

function CheckReset(event) {
  configView.checkReset(event);
}

function UpdateField(event) {
  configView.updateField(event);
}

function SearchField(event) {
  configView.search(event);
}

var getAllPrefs; // Used by tests
function BuildUI() {
  configView.resetTable();

  if (AppManager.connection &&
      AppManager.connection.status == Connection.Status.CONNECTED &&
      AppManager.preferenceFront) {
    configView.front = AppManager.preferenceFront;
    configView.kind = "Pref";
    configView.includeTypeName = true;

    getAllPrefs = AppManager.preferenceFront.getAllPrefs()
                  .then(json => configView.generateDisplay(json));
  } else {
    CloseUI();
  }
}
