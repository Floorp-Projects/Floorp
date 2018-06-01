/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {require} = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
const Services = require("Services");
const {AppManager} = require("devtools/client/webide/modules/app-manager");
const {Connection} = require("devtools/shared/client/connection-manager");
const {RuntimeTypes} = require("devtools/client/webide/modules/runtimes");
const Strings = Services.strings.createBundle("chrome://devtools/locale/webide.properties");

const UNRESTRICTED_HELP_URL = "https://developer.mozilla.org/docs/Tools/WebIDE/Running_and_debugging_apps#Unrestricted_app_debugging_%28including_certified_apps_main_process_etc.%29";

window.addEventListener("load", function() {
  document.querySelector("#close").onclick = CloseUI;
  document.querySelector("#devtools-check button").onclick = EnableCertApps;
  document.querySelector("#adb-check button").onclick = RootADB;
  document.querySelector("#unrestricted-privileges").onclick = function() {
    window.parent.UI.openInBrowser(UNRESTRICTED_HELP_URL);
  };
  AppManager.on("app-manager-update", OnAppManagerUpdate);
  BuildUI();
  CheckLockState();
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
    CheckLockState();
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

function CheckLockState() {
  const adbCheckResult = document.querySelector("#adb-check > .yesno");
  const devtoolsCheckResult = document.querySelector("#devtools-check > .yesno");
  const flipCertPerfButton = document.querySelector("#devtools-check button");
  const flipCertPerfAction = document.querySelector("#devtools-check > .action");
  const adbRootAction = document.querySelector("#adb-check > .action");

  const sYes = Strings.GetStringFromName("runtimedetails_checkyes");
  const sNo = Strings.GetStringFromName("runtimedetails_checkno");
  const sUnknown = Strings.GetStringFromName("runtimedetails_checkunknown");
  const sNotUSB = Strings.GetStringFromName("runtimedetails_notUSBDevice");

  flipCertPerfButton.setAttribute("disabled", "true");
  flipCertPerfAction.setAttribute("hidden", "true");
  adbRootAction.setAttribute("hidden", "true");

  adbCheckResult.textContent = sUnknown;
  devtoolsCheckResult.textContent = sUnknown;

  if (AppManager.connection &&
      AppManager.connection.status == Connection.Status.CONNECTED) {
    // ADB check
    if (AppManager.selectedRuntime.type === RuntimeTypes.USB) {
      const device = AppManager.selectedRuntime.device;
      if (device && device.summonRoot) {
        device.isRoot().then(isRoot => {
          if (isRoot) {
            adbCheckResult.textContent = sYes;
            flipCertPerfButton.removeAttribute("disabled");
          } else {
            adbCheckResult.textContent = sNo;
            adbRootAction.removeAttribute("hidden");
          }
        }, console.error);
      } else {
        adbCheckResult.textContent = sUnknown;
      }
    } else {
      adbCheckResult.textContent = sNotUSB;
    }

    // forbid-certified-apps check
    try {
      const prefFront = AppManager.preferenceFront;
      prefFront.getBoolPref("devtools.debugger.forbid-certified-apps").then(isForbidden => {
        if (isForbidden) {
          devtoolsCheckResult.textContent = sNo;
          flipCertPerfAction.removeAttribute("hidden");
        } else {
          devtoolsCheckResult.textContent = sYes;
        }
      }, console.error);
    } catch (e) {
      // Exception. pref actor is only accessible if forbird-certified-apps is false
      devtoolsCheckResult.textContent = sNo;
      flipCertPerfAction.removeAttribute("hidden");
    }
  }
}

function EnableCertApps() {
  const device = AppManager.selectedRuntime.device;
  // TODO: Remove `network.disable.ipc.security` once bug 1125916 is fixed.
  device.shell(
    "stop b2g && " +
    "cd /data/b2g/mozilla/*.default/ && " +
    "echo 'user_pref(\"devtools.debugger.forbid-certified-apps\", false);' >> prefs.js && " +
    "echo 'user_pref(\"dom.apps.developer_mode\", true);' >> prefs.js && " +
    "echo 'user_pref(\"network.disable.ipc.security\", true);' >> prefs.js && " +
    "echo 'user_pref(\"dom.webcomponents.shadowdom.enabled\", true);' >> prefs.js && " +
    "start b2g"
  );
}

function RootADB() {
  const device = AppManager.selectedRuntime.device;
  device.summonRoot().then(CheckLockState, console.error);
}
