/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { loader, require } = ChromeUtils.import(
  "resource://devtools/shared/Loader.jsm"
);

const Services = require("Services");
const Strings = Services.strings.createBundle(
  "chrome://devtools/locale/webide.properties"
);

const { gDevTools } = require("devtools/client/framework/devtools");

loader.lazyRequireGetter(
  this,
  "adbAddon",
  "devtools/shared/adb/adb-addon",
  true
);

window.addEventListener(
  "load",
  function() {
    document.querySelector("#aboutaddons").onclick = function() {
      const browserWin = Services.wm.getMostRecentWindow(
        gDevTools.chromeWindowType
      );
      if (browserWin && browserWin.BrowserOpenAddonsMgr) {
        browserWin.BrowserOpenAddonsMgr("addons://list/extension");
      }
    };
    document.querySelector("#close").onclick = CloseUI;
    BuildUI();
  },
  { capture: true, once: true }
);

function CloseUI() {
  window.parent.UI.openProject();
}

function BuildUI() {
  function onAddonUpdate(arg) {
    progress.removeAttribute("value");
    li.setAttribute("status", adbAddon.status);
    status.textContent = Strings.GetStringFromName(
      "addons_status_" + adbAddon.status
    );
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

  adbAddon.on("update", onAddonUpdate);
  adbAddon.on("failure", onAddonFailure);
  adbAddon.on("progress", onAddonProgress);

  window.addEventListener(
    "unload",
    function() {
      adbAddon.off("update", onAddonUpdate);
      adbAddon.off("failure", onAddonFailure);
      adbAddon.off("progress", onAddonProgress);
    },
    { once: true }
  );

  const li = document.createElement("li");
  li.setAttribute("status", adbAddon.status);

  const name = document.createElement("span");
  name.className = "name";

  li.setAttribute("addon", "adb");
  name.textContent = "ADB Extension";

  li.appendChild(name);

  const status = document.createElement("span");
  status.className = "status";
  status.textContent = Strings.GetStringFromName(
    "addons_status_" + adbAddon.status
  );
  li.appendChild(status);

  const installButton = document.createElement("button");
  installButton.className = "install-button";
  installButton.onclick = () => adbAddon.install("webide");
  installButton.textContent = Strings.GetStringFromName(
    "addons_install_button"
  );
  li.appendChild(installButton);

  const uninstallButton = document.createElement("button");
  uninstallButton.className = "uninstall-button";
  uninstallButton.onclick = () => adbAddon.uninstall();
  uninstallButton.textContent = Strings.GetStringFromName(
    "addons_uninstall_button"
  );
  li.appendChild(uninstallButton);

  const progress = document.createElement("progress");
  li.appendChild(progress);

  const warning = document.createElement("p");
  warning.textContent = Strings.GetStringFromName("addons_adb_warning");
  warning.className = "warning";
  li.appendChild(warning);

  document.querySelector("ul").appendChild(li);
}
