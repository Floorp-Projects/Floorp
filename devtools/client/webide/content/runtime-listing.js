/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Cu = Components.utils;
const {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
const RuntimeList = require("devtools/client/webide/modules/runtime-list");

var runtimeList = new RuntimeList(window, window.parent);

window.addEventListener("load", function onLoad() {
  window.removeEventListener("load", onLoad, true);
  document.getElementById("runtime-screenshot").onclick = TakeScreenshot;
  document.getElementById("runtime-permissions").onclick = ShowPermissionsTable;
  document.getElementById("runtime-details").onclick = ShowRuntimeDetails;
  document.getElementById("runtime-disconnect").onclick = DisconnectRuntime;
  document.getElementById("runtime-preferences").onclick = ShowDevicePreferences;
  document.getElementById("runtime-settings").onclick = ShowSettings;
  document.getElementById("runtime-panel-installsimulator").onclick = ShowAddons;
  document.getElementById("runtime-panel-noadbhelper").onclick = ShowAddons;
  document.getElementById("runtime-panel-nousbdevice").onclick = ShowTroubleShooting;
  runtimeList.update();
  runtimeList.updateCommands();
}, true);

window.addEventListener("unload", function onUnload() {
  window.removeEventListener("unload", onUnload);
  runtimeList.destroy();
});

function TakeScreenshot() {
  runtimeList.takeScreenshot();
}

function ShowRuntimeDetails() {
  runtimeList.showRuntimeDetails();
}

function ShowPermissionsTable() {
  runtimeList.showPermissionsTable();
}

function ShowDevicePreferences() {
  runtimeList.showDevicePreferences();
}

function ShowSettings() {
  runtimeList.showSettings();
}

function DisconnectRuntime() {
  window.parent.Cmds.disconnectRuntime();
}

function ShowAddons() {
  runtimeList.showAddons();
}

function ShowTroubleShooting() {
  runtimeList.showTroubleShooting();
}
