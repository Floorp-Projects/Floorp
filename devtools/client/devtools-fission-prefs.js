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
    "devtools.target-switching.server.enabled",
    "Enable experimental server side target switching",
  ],
  [
    "devtools.testing.enableServerWatcherSupport",
    "Enable experimental server-side resources (see watcher actor to get the " +
      "list of impacted resources",
  ],
  [
    "fission.bfcacheInParent",
    "Enable bfcache navigation in parent process (requires Fission and involve " +
      "more top level target switching",
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

  Object.assign(container.style, {
    display: "grid",
    gridTemplateColumns:
      "max-content minmax(300px, auto) max-content max-content",
    gridColumnGap: "8px",
    gridTemplateRows: `repeat(${PREFERENCES.length + 1}, auto)`,
    gridRowGap: "8px",
    padding: "12px",
    fontSize: "11px",
  });

  container.classList.add("theme-body");

  const headerContainer = toolbox.doc.createElement("header");
  /**
   * The grid layout of the header container is as follows:
   *
   *  +-------------------------+--------------+
   *  | Header text             | Reset button |
   *  +-------------------------+--------------+
   */

  Object.assign(headerContainer.style, {
    display: "grid",
    gridTemplateColumns: "subgrid",
    gridColumn: "1 / -1",
  });

  const header = toolbox.doc.createElement("h1");

  Object.assign(header.style, {
    gridColumn: "1 / -2",
    fontSize: "11px",
    margin: "0",
    padding: "0",
  });

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
  Object.assign(prefList.style, {
    display: "grid",
    gridTemplateColumns: "subgrid",
    gridTemplateRows: "subgrid",
    // Subgrid should span all grid columns
    gridColumn: "1 / -1",
    gridRow: "2 / -1",
    listStyle: "none",
    margin: "0",
    padding: "0",
  });

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

  /**
   * The grid layout of a preference line is as follows:
   *
   *  +------+-----------------------------+-------+---------------+
   *  | Icon | Preference name             | Value | Toggle button |
   *  +------+-----------------------------+-------+---------------+
   */

  Object.assign(prefEl.style, {
    margin: "0",
    lineHeight: "12px",
    display: "grid",
    alignItems: "center",
    gridTemplateColumns: "subgrid",
    gridColumn: "1 / -1",
  });

  prefEl.classList.toggle("theme-comment", !isPrefEnabled);

  // Icon
  const prefInfo = toolbox.doc.createElement("div");
  prefInfo.title = desc;

  Object.assign(prefInfo.style, {
    width: "12px",
    height: "12px",
  });

  prefInfo.classList.add("fission-pref-icon");

  // Preference name
  const prefTitle = toolbox.doc.createElement("span");

  Object.assign(prefTitle.style, {
    userSelect: "text",
    fontWeight: isPrefEnabled ? "bold" : "normal",
  });

  prefTitle.textContent = name;

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
