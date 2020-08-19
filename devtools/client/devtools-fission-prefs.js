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
    "devtools.contenttoolbox.fission",
    "Enable fission support in the regular Toolbox. Allows to see and debug " +
      "resources from remote frames. Should only be used when " +
      "`fission.autostart` is enabled",
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
    "devtools.testing.enableServerWatcherSupport",
    "Enable experimental server-side resources (see watcher actor to get the " +
      "list of impacted resources",
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

  updateTooltipContent(toolbox);

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
function updateTooltipContent(toolbox) {
  const container = toolbox.doc.createElement("div");

  /*
   *  This is the grid we want to have:
   *  +--------------------------------------------+---------------+
   *  | Header text                                | Reset button  |
   *  +------+-----------------------------+-------+---------------+
   *  | Icon | Preference name             | Value | Toggle button |
   *  +------+-----------------------------+-------+---------------+
   *  | Icon | Preference name             | Value | Toggle button |
   *  +------+-----------------------------+-------+---------------+
   */
  container.style.display = "grid";
  container.style.gridTemplateColumns =
    "max-content minmax(300px, auto) max-content max-content";
  container.style.gridColumnGap = "8px";
  container.style.gridTemplateRows = `repeat(${PREFERENCES.length + 1}, auto)`;
  container.style.gridRowGap = "8px";
  container.style.padding = "12px";
  container.style.fontSize = "11px";
  container.classList.add("theme-body");

  const headerContainer = toolbox.doc.createElement("header");
  /**
   * The grid layout of the header container is as follows:
   *
   *  +-------------------------+--------------+
   *  | Header text             | Reset button |
   *  +-------------------------+--------------+
   */
  headerContainer.style.display = "grid";
  headerContainer.style.gridTemplateColumns = "subgrid";
  headerContainer.style.gridColumn = "1 / -1";

  const header = toolbox.doc.createElement("h1");
  header.style.gridColumn = "1 / -2";
  header.style.fontSize = "11px";
  header.style.margin = "0";
  header.style.padding = "0";
  header.textContent = "DevTools Fission preferences";

  const resetButton = toolbox.doc.createElement("button");
  resetButton.addEventListener("click", () => {
    for (const [name] of PREFERENCES) {
      Services.prefs.clearUserPref(name);
    }
    updateTooltipContent(toolbox);
  });
  resetButton.textContent = "reset all";

  headerContainer.append(header, resetButton);

  const prefList = toolbox.doc.createElement("ul");
  prefList.style.display = "grid";
  prefList.style.gridTemplateColumns = "subgrid";
  prefList.style.gridTemplateRows = "subgrid";

  // Subgrid should span all grid columns
  prefList.style.gridColumn = "1 / -1";

  prefList.style.gridRow = "2 / -1";
  prefList.style.listStyle = "none";
  prefList.style.margin = "0";
  prefList.style.padding = "0";

  for (const [name, desc] of PREFERENCES) {
    const prefEl = createPreferenceListItem(toolbox, name, desc);
    prefList.appendChild(prefEl);
  }

  container.append(headerContainer, prefList);

  toolbox._fissionPrefsTooltip.panel.innerHTML = "";
  // There is a hardcoded 320px max width for doorhanger tooltips,
  // see Bug 1654020.
  toolbox._fissionPrefsTooltip.panel.style.maxWidth = "unset";
  toolbox._fissionPrefsTooltip.panel.appendChild(container);
}

function createPreferenceListItem(toolbox, name, desc) {
  const isPrefEnabled = Services.prefs.getBoolPref(name, false);

  const prefEl = toolbox.doc.createElement("li");
  prefEl.classList.toggle("theme-comment", !isPrefEnabled);
  prefEl.style.margin = "0";
  prefEl.style.lineHeight = "12px";

  /**
   * The grid layout of a preference line is as follows:
   *
   *  +------+-----------------------------+-------+---------------+
   *  | Icon | Preference name             | Value | Toggle button |
   *  +------+-----------------------------+-------+---------------+
   */
  prefEl.style.display = "grid";
  prefEl.style.alignItems = "center";
  prefEl.style.gridTemplateColumns = "subgrid";
  prefEl.style.gridColumn = "1 / -1";

  // Icon
  const prefInfo = toolbox.doc.createElement("div");
  prefInfo.title = desc;
  prefInfo.style.width = "12px";
  prefInfo.style.height = "12px";
  prefInfo.classList.add("fission-pref-icon");

  // Preference name
  const prefTitle = toolbox.doc.createElement("span");
  prefTitle.textContent = name;
  prefTitle.style.userSelect = "text";
  prefTitle.style.fontWeight = isPrefEnabled ? "bold" : "normal";

  // Value
  const prefValue = toolbox.doc.createElement("span");
  prefValue.textContent = isPrefEnabled;

  // Toggle Button
  const toggleButton = toolbox.doc.createElement("button");
  toggleButton.addEventListener("click", () => {
    Services.prefs.setBoolPref(name, !isPrefEnabled);
    updateTooltipContent(toolbox);
  });
  toggleButton.textContent = "toggle";

  prefEl.append(prefInfo, prefTitle, prefValue, toggleButton);
  return prefEl;
}

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
