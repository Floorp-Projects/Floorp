/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { TabUnloader } = ChromeUtils.importESModule(
  "resource:///modules/TabUnloader.sys.mjs"
);

async function refreshData() {
  const sortedTabs = await TabUnloader.getSortedTabs(null);
  const tabTable = document.querySelector(".tab-table > tbody");
  const getHost = uri => {
    try {
      return uri?.host;
    } catch (e) {
      return uri?.spec;
    }
  };
  const updateTimestamp = () => {
    document.l10n.setAttributes(
      document.getElementById("label-last-updated"),
      "about-unloads-last-updated",
      { date: Date.now() }
    );
  };

  // Reset the table
  // Don't delete the first row showing the "no unloadable tab" message
  while (tabTable.rows.length > 1) {
    tabTable.deleteRow(1);
  }
  if (!sortedTabs.length) {
    document.getElementById("button-unload").disabled = true;
    document.getElementById("no-unloadable-tab-message").hidden = false;
    updateTimestamp();
    return;
  }
  document.getElementById("button-unload").disabled =
    !TabUnloader.isDiscardable(sortedTabs[0]);
  document.getElementById("no-unloadable-tab-message").hidden = true;

  const fragmentRows = new DocumentFragment();
  const templateRow = document.querySelector("template[name=tab-table-row]");

  let ordinal = 0;
  for (const tabInfo of sortedTabs) {
    if (!("tab" in tabInfo)) {
      continue;
    }

    const fragment = templateRow.content.cloneNode(true);
    const row = fragment.querySelector("tr");

    row.children[0].textContent = TabUnloader.isDiscardable(tabInfo)
      ? ++ordinal
      : "-";
    row.children[1].textContent = getHost(
      tabInfo.tab?.linkedBrowser?.currentURI
    );
    if ("lastAccessed" in tabInfo.tab) {
      document.l10n.setAttributes(
        row.children[2],
        "about-unloads-last-accessed",
        { date: tabInfo.tab.lastAccessed }
      );
    }
    row.children[3].textContent = tabInfo.weight;
    row.children[4].textContent = tabInfo.sortWeight;
    if ("memory" in tabInfo) {
      document.l10n.setAttributes(
        row.children[5],
        "about-unloads-memory-in-mb",
        { mem: tabInfo.memory / 1024 / 1024 }
      );
    }

    if (tabInfo.processes) {
      for (const [pid, procEntry] of tabInfo.processes) {
        if (pid < 0) {
          // Tab is hosted by the main process
          continue;
        }

        const procLabel = document.createElement("span");
        const procInfo = procEntry.entryToProcessMap;

        if (procEntry.isTopLevel) {
          procLabel.classList.add("top-level-process");
        }
        if (procInfo.tabSet.size > 1) {
          procLabel.classList.add("shared-process");
        }
        procLabel.textContent = pid;
        document.l10n.setAttributes(
          procLabel,
          "about-unloads-memory-in-mb-tooltip",
          { mem: procInfo.memory / 1024 / 1024 }
        );
        row.children[6].appendChild(procLabel);
        row.children[6].appendChild(document.createTextNode(" "));
      }
    }

    fragmentRows.appendChild(fragment);
  }

  tabTable.appendChild(fragmentRows);
  updateTimestamp();
}

async function onLoad() {
  document
    .getElementById("button-unload")
    .addEventListener("click", async () => {
      await TabUnloader.unloadLeastRecentlyUsedTab(null);
      await refreshData();
    });
  await refreshData();
}

try {
  document.addEventListener("DOMContentLoaded", onLoad, { once: true });
} catch (ex) {
  console.error(ex);
}
