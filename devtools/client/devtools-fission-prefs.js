/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

loader.lazyRequireGetter(
  this,
  "HTMLTooltip",
  "devtools/client/shared/widgets/tooltip/HTMLTooltip",
  true
);

const PREFERENCES = [
  [
    "fission.autostart",
    "Enable fission in Firefox. When navigating between two domains, you " +
      "will switch between two distinct processes. And if an iframe is " +
      "hosted from another domain, it will run in another process",
  ],
  [
    "devtools.browsertoolbox.fission",
    "Enable the Multiprocess Browser Toolbox and Multiprocess Browser " +
      "Console, so that it can see and debug resources from the content " +
      "processes at the same time as resources from the parent process",
  ],
  [
    "devtools.target-switching.enabled",
    "If you navigate between two distinct process, the toolbox won’t close " +
      "and will instead switch to the new target. This impacts the regular " +
      "web toolbox, when switching from eg. about:sessionrestore to any http " +
      "url (parent process to content process). Or when navigating between " +
      "two distinct domains if `fission.autostart` is set to true",
  ],
  [
    "devtools.responsive.browserUI.enabled",
    "Enable the new version of RDM that doesn't rely on tunnelling and is " +
      "Fission compatible",
  ],
];

/**
 * Temporary module to show a Tooltip with the currently enabled preferences
 * relevant for DevTools Fission (& associated experiment efforts).
 *
 * This module should be deleted once the experimental Fission prefs are
 * preffed on in Nightly.
 */
function showTooltip(toolbox) {
  if (!toolbox._fissionPrefsTooltip) {
    toolbox._fissionPrefsTooltip = new HTMLTooltip(toolbox.doc, {
      type: "doorhanger",
      useXulWrapper: true,
    });
    toolbox.once("destroy", () => toolbox._fissionPrefsTooltip.destroy());
  }

  // Terrible hack to allow to toggle using the command button.
  if (toolbox._fissionPrefsTooltip.preventShow) {
    return;
  }

  const container = toolbox.doc.createElement("div");
  container.style.padding = "12px";
  container.style.fontSize = "11px";
  container.classList.add("theme-body");

  const header = toolbox.doc.createElement("h1");
  header.style.fontSize = "11px";
  header.style.margin = "0";
  header.style.padding = "0";
  header.textContent = "DevTools Fission preferences";
  container.appendChild(header);

  const prefList = toolbox.doc.createElement("ul");
  prefList.style.listStyle = "none";
  prefList.style.margin = "0";
  prefList.style.padding = "0";
  container.appendChild(prefList);

  for (const [name, desc] of PREFERENCES) {
    const isPrefEnabled = Services.prefs.getBoolPref(name, false);

    const prefEl = toolbox.doc.createElement("li");
    prefEl.classList.toggle("theme-comment", !isPrefEnabled);
    prefEl.style.margin = "8px 0 0";
    prefEl.style.lineHeight = "12px";
    prefEl.style.display = "grid";
    prefEl.style.gridTemplateColumns = "max-content auto max-content";
    prefEl.style.gridColumnGap = "8px";

    const prefInfo = toolbox.doc.createElement("div");
    prefInfo.title = desc;
    prefInfo.style.width = "12px";
    prefInfo.style.height = "12px";
    prefInfo.classList.add("fission-pref-icon");

    const prefTitle = toolbox.doc.createElement("span");
    prefTitle.textContent = name;
    prefTitle.style.userSelect = "text";
    prefTitle.style.fontWeight = isPrefEnabled ? "bold" : "normal";

    const prefValue = toolbox.doc.createElement("span");
    prefValue.textContent = isPrefEnabled;

    prefEl.appendChild(prefInfo);
    prefEl.appendChild(prefTitle);
    prefEl.appendChild(prefValue);
    prefList.appendChild(prefEl);
  }

  toolbox._fissionPrefsTooltip.panel.innerHTML = "";
  toolbox._fissionPrefsTooltip.panel.appendChild(container);

  const commandId = "command-button-fission-prefs";
  toolbox._fissionPrefsTooltip.show(toolbox.doc.getElementById(commandId));

  // Follows a hack to be able to close the tooltip when clicking on the
  // command button. Otherwise it will flicker and reopen.
  toolbox._fissionPrefsTooltip.preventShow = true;
  toolbox._fissionPrefsTooltip.once("hidden", () => {
    toolbox.win.setTimeout(
      () => (toolbox._fissionPrefsTooltip.preventShow = false),
      250
    );
  });
}
exports.showTooltip = showTooltip;

function isAnyPreferenceEnabled() {
  for (const [name] of PREFERENCES) {
    const isPrefEnabled = Services.prefs.getBoolPref(name, false);
    if (isPrefEnabled) {
      return true;
    }
  }
  return false;
}
exports.isAnyPreferenceEnabled = isAnyPreferenceEnabled;
