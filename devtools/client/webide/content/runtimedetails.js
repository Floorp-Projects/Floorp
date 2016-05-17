/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Cu = Components.utils;
const {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
const Services = require("Services");
const {AppManager} = require("devtools/client/webide/modules/app-manager");
const {Connection} = require("devtools/shared/client/connection-manager");
const {RuntimeTypes} = require("devtools/client/webide/modules/runtimes");
const Strings = Services.strings.createBundle("chrome://devtools/locale/webide.properties");

const UNRESTRICTED_HELP_URL = "https://developer.mozilla.org/docs/Tools/WebIDE/Running_and_debugging_apps#Unrestricted_app_debugging_%28including_certified_apps_main_process_etc.%29";

window.addEventListener("load", function onLoad() {
  window.removeEventListener("load", onLoad);
  document.querySelector("#close").onclick = CloseUI;
  document.querySelector("#devtools-check button").onclick = EnableCertApps;
  document.querySelector("#adb-check button").onclick = RootADB;
  document.querySelector("#unrestricted-privileges").onclick = function () {
    window.parent.UI.openInBrowser(UNRESTRICTED_HELP_URL);
  };
  AppManager.on("app-manager-update", OnAppManagerUpdate);
  BuildUI();
  CheckLockState();
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
    CheckLockState();
  }
}

function generateFields(json) {
  let table = document.querySelector("table");
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
}

var getDescriptionPromise; // Used by tests
function BuildUI() {
  let table = document.querySelector("table");
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

function CheckLockState() {
  let adbCheckResult = document.querySelector("#adb-check > .yesno");
  let devtoolsCheckResult = document.querySelector("#devtools-check > .yesno");
  let flipCertPerfButton = document.querySelector("#devtools-check button");
  let adbRootButton = document.querySelector("#adb-check button");
  let flipCertPerfAction = document.querySelector("#devtools-check > .action");
  let adbRootAction = document.querySelector("#adb-check > .action");

  let sYes = Strings.GetStringFromName("runtimedetails_checkyes");
  let sNo = Strings.GetStringFromName("runtimedetails_checkno");
  let sUnknown = Strings.GetStringFromName("runtimedetails_checkunknown");
  let sNotUSB = Strings.GetStringFromName("runtimedetails_notUSBDevice");

  flipCertPerfButton.setAttribute("disabled", "true");
  flipCertPerfAction.setAttribute("hidden", "true");
  adbRootAction.setAttribute("hidden", "true");

  adbCheckResult.textContent = sUnknown;
  devtoolsCheckResult.textContent = sUnknown;

  if (AppManager.connection &&
      AppManager.connection.status == Connection.Status.CONNECTED) {

    // ADB check
    if (AppManager.selectedRuntime.type === RuntimeTypes.USB) {
      let device = AppManager.selectedRuntime.device;
      if (device && device.summonRoot) {
        device.isRoot().then(isRoot => {
          if (isRoot) {
            adbCheckResult.textContent = sYes;
            flipCertPerfButton.removeAttribute("disabled");
          } else {
            adbCheckResult.textContent = sNo;
            adbRootAction.removeAttribute("hidden");
          }
        }, e => console.error(e));
      } else {
        adbCheckResult.textContent = sUnknown;
      }
    } else {
      adbCheckResult.textContent = sNotUSB;
    }

    // forbid-certified-apps check
    try {
      let prefFront = AppManager.preferenceFront;
      prefFront.getBoolPref("devtools.debugger.forbid-certified-apps").then(isForbidden => {
        if (isForbidden) {
          devtoolsCheckResult.textContent = sNo;
          flipCertPerfAction.removeAttribute("hidden");
        } else {
          devtoolsCheckResult.textContent = sYes;
        }
      }, e => console.error(e));
    } catch (e) {
      // Exception. pref actor is only accessible if forbird-certified-apps is false
      devtoolsCheckResult.textContent = sNo;
      flipCertPerfAction.removeAttribute("hidden");
    }

  }

}

function EnableCertApps() {
  let device = AppManager.selectedRuntime.device;
  // TODO: Remove `network.disable.ipc.security` once bug 1125916 is fixed.
  device.shell(
    "stop b2g && " +
    "cd /data/b2g/mozilla/*.default/ && " +
    "echo 'user_pref(\"devtools.debugger.forbid-certified-apps\", false);' >> prefs.js && " +
    "echo 'user_pref(\"dom.apps.developer_mode\", true);' >> prefs.js && " +
    "echo 'user_pref(\"network.disable.ipc.security\", true);' >> prefs.js && " +
    "echo 'user_pref(\"dom.webcomponents.enabled\", true);' >> prefs.js && " +
    "start b2g"
  );
}

function RootADB() {
  let device = AppManager.selectedRuntime.device;
  device.summonRoot().then(CheckLockState, (e) => console.error(e));
}
