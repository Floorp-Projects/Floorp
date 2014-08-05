/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;
const {Services} = Cu.import("resource://gre/modules/Services.jsm");
const {require} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {}).devtools;
const {GetAvailableAddons, ForgetAddonsList} = require("devtools/webide/addons");
const Strings = Services.strings.createBundle("chrome://browser/locale/devtools/webide.properties");

window.addEventListener("load", function onLoad() {
  window.removeEventListener("load", onLoad);
  document.querySelector("#aboutaddons").onclick = function() {
    window.parent.UI.openInBrowser("about:addons");
  }
  document.querySelector("#close").onclick = CloseUI;
  GetAvailableAddons().then(BuildUI, (e) => {
    console.error(e);
    window.alert(Strings.formatStringFromName("error_cantFetchAddonsJSON", [e], 1));
  });
}, true);

window.addEventListener("unload", function onUnload() {
  window.removeEventListener("unload", onUnload);
  ForgetAddonsList();
}, true);

function CloseUI() {
  window.parent.UI.openProject();
}

function BuildUI(addons) {
  BuildItem(addons.adb, true /* is adb */);
  for (let addon of addons.simulators) {
    BuildItem(addon, false /* is adb */);
  }
}

function BuildItem(addon, isADB) {

  function onAddonUpdate(event, arg) {
    switch (event) {
      case "update":
        progress.removeAttribute("value");
        li.setAttribute("status", addon.status);
        status.textContent = Strings.GetStringFromName("addons_status_" + addon.status);
        break;
      case "failure":
        console.error(arg);
        window.alert(arg);
        break;
      case "progress":
        if (arg == -1) {
          progress.removeAttribute("value");
        } else {
          progress.value = arg;
        }
        break;
    }
  }

  let events = ["update", "failure", "progress"];
  for (let e of events) {
    addon.on(e, onAddonUpdate);
  }
  window.addEventListener("unload", function onUnload() {
    window.removeEventListener("unload", onUnload);
    for (let e of events) {
      addon.off(e, onAddonUpdate);
    }
  });

  let li = document.createElement("li");
  li.setAttribute("status", addon.status);

  // Used in tests
  if (isADB) {
    li.setAttribute("addon", "adb");
  } else {
    li.setAttribute("addon", "simulator-" + addon.version);
  }

  let name = document.createElement("span");
  name.className = "name";
  if (isADB) {
    name.textContent = Strings.GetStringFromName("addons_adb_label");
  } else {
    let stability = Strings.GetStringFromName("addons_" + addon.stability);
    name.textContent = Strings.formatStringFromName("addons_simulator_label", [addon.version, stability], 2);
  }

  li.appendChild(name);

  let status = document.createElement("span");
  status.className = "status";
  status.textContent = Strings.GetStringFromName("addons_status_" + addon.status);
  li.appendChild(status);

  let installButton = document.createElement("button");
  installButton.className = "install-button";
  installButton.onclick = () => addon.install();
  installButton.textContent = Strings.GetStringFromName("addons_install_button");
  li.appendChild(installButton);

  let uninstallButton = document.createElement("button");
  uninstallButton.className = "uninstall-button";
  uninstallButton.onclick = () => addon.uninstall();
  uninstallButton.textContent = Strings.GetStringFromName("addons_uninstall_button");
  li.appendChild(uninstallButton);

  let progress = document.createElement("progress");
  li.appendChild(progress);

  if (isADB) {
    let warning = document.createElement("p");
    warning.textContent = Strings.GetStringFromName("addons_adb_warning");
    warning.className = "warning";
    li.appendChild(warning);
  }

  document.querySelector("ul").appendChild(li);
}
