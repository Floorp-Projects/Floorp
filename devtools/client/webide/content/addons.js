/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Cu = Components.utils;
const {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
const Services = require("Services");
const {GetAvailableAddons, ForgetAddonsList} = require("devtools/client/webide/modules/addons");
const Strings = Services.strings.createBundle("chrome://devtools/locale/webide.properties");

window.addEventListener("load", function onLoad() {
  window.removeEventListener("load", onLoad);
  document.querySelector("#aboutaddons").onclick = function () {
    let browserWin = Services.wm.getMostRecentWindow("navigator:browser");
    browserWin.BrowserOpenAddonsMgr("addons://list/extension");
  };
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
  BuildItem(addons.adb, "adb");
  BuildItem(addons.adapters, "adapters");
  for (let addon of addons.simulators) {
    BuildItem(addon, "simulator");
  }
}

function BuildItem(addon, type) {

  function onAddonUpdate(event, arg) {
    switch (event) {
      case "update":
        progress.removeAttribute("value");
        li.setAttribute("status", addon.status);
        status.textContent = Strings.GetStringFromName("addons_status_" + addon.status);
        break;
      case "failure":
        window.parent.UI.reportError("error_operationFail", arg);
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

  let name = document.createElement("span");
  name.className = "name";

  switch (type) {
    case "adb":
      li.setAttribute("addon", type);
      name.textContent = Strings.GetStringFromName("addons_adb_label");
      break;
    case "adapters":
      li.setAttribute("addon", type);
      try {
        name.textContent = Strings.GetStringFromName("addons_adapters_label");
      } catch (e) {
        // This code (bug 1081093) will be backported to Aurora, which doesn't
        // contain this string.
        name.textContent = "Tools Adapters Add-on";
      }
      break;
    case "simulator":
      li.setAttribute("addon", "simulator-" + addon.version);
      let stability = Strings.GetStringFromName("addons_" + addon.stability);
      name.textContent = Strings.formatStringFromName("addons_simulator_label", [addon.version, stability], 2);
      break;
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

  if (type == "adb") {
    let warning = document.createElement("p");
    warning.textContent = Strings.GetStringFromName("addons_adb_warning");
    warning.className = "warning";
    li.appendChild(warning);
  }

  document.querySelector("ul").appendChild(li);
}
