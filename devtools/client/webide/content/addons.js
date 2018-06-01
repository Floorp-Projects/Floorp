/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {require} = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
const Services = require("Services");
const {gDevTools} = require("devtools/client/framework/devtools");
const {GetAvailableAddons, ForgetAddonsList} = require("devtools/client/webide/modules/addons");
const Strings = Services.strings.createBundle("chrome://devtools/locale/webide.properties");

window.addEventListener("load", function() {
  document.querySelector("#aboutaddons").onclick = function() {
    const browserWin = Services.wm.getMostRecentWindow(gDevTools.chromeWindowType);
    if (browserWin && browserWin.BrowserOpenAddonsMgr) {
      browserWin.BrowserOpenAddonsMgr("addons://list/extension");
    }
  };
  document.querySelector("#close").onclick = CloseUI;
  BuildUI(GetAvailableAddons());
}, {capture: true, once: true});

window.addEventListener("unload", function() {
  ForgetAddonsList();
}, {capture: true, once: true});

function CloseUI() {
  window.parent.UI.openProject();
}

function BuildUI(addons) {
  BuildItem(addons.adb, "adb");
}

function BuildItem(addon, type) {
  function onAddonUpdate(arg) {
    progress.removeAttribute("value");
    li.setAttribute("status", addon.status);
    status.textContent = Strings.GetStringFromName("addons_status_" + addon.status);
  }

  function onAddonFailure(arg) {
    window.parent.UI.reportError("error_operationFail", arg);
  }

  function onAddonProgress(arg) {
    if (arg == -1) {
      progress.removeAttribute("value");
    } else {
      progress.value = arg;
    }
  }

  addon.on("update", onAddonUpdate);
  addon.on("failure", onAddonFailure);
  addon.on("progress", onAddonProgress);

  window.addEventListener("unload", function() {
    addon.off("update", onAddonUpdate);
    addon.off("failure", onAddonFailure);
    addon.off("progress", onAddonProgress);
  }, {once: true});

  const li = document.createElement("li");
  li.setAttribute("status", addon.status);

  const name = document.createElement("span");
  name.className = "name";

  switch (type) {
    case "adb":
      li.setAttribute("addon", type);
      name.textContent = Strings.GetStringFromName("addons_adb_label");
      break;
  }

  li.appendChild(name);

  const status = document.createElement("span");
  status.className = "status";
  status.textContent = Strings.GetStringFromName("addons_status_" + addon.status);
  li.appendChild(status);

  const installButton = document.createElement("button");
  installButton.className = "install-button";
  installButton.onclick = () => addon.install();
  installButton.textContent = Strings.GetStringFromName("addons_install_button");
  li.appendChild(installButton);

  const uninstallButton = document.createElement("button");
  uninstallButton.className = "uninstall-button";
  uninstallButton.onclick = () => addon.uninstall();
  uninstallButton.textContent = Strings.GetStringFromName("addons_uninstall_button");
  li.appendChild(uninstallButton);

  const progress = document.createElement("progress");
  li.appendChild(progress);

  if (type == "adb") {
    const warning = document.createElement("p");
    warning.textContent = Strings.GetStringFromName("addons_adb_warning");
    warning.className = "warning";
    li.appendChild(warning);
  }

  document.querySelector("ul").appendChild(li);
}
