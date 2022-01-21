/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals browser */

let availablePatches;

const portToAddon = (function() {
  let port;

  function connect() {
    port = browser.runtime.connect({ name: "AboutCompatTab" });
    port.onMessage.addListener(onMessageFromAddon);
    port.onDisconnect.addListener(e => {
      port = undefined;
    });
  }

  connect();

  async function send(message) {
    if (port) {
      return port.postMessage(message);
    }
    return Promise.reject("background script port disconnected");
  }

  return { send };
})();

const $ = function(sel) {
  return document.querySelector(sel);
};

const DOMContentLoadedPromise = new Promise(resolve => {
  document.addEventListener(
    "DOMContentLoaded",
    () => {
      resolve();
    },
    { once: true }
  );
});

Promise.all([
  browser.runtime.sendMessage("getAllInterventions"),
  DOMContentLoadedPromise,
]).then(([info]) => {
  document.body.addEventListener("click", async evt => {
    const ele = evt.target;
    if (ele.nodeName === "BUTTON") {
      const row = ele.closest("[data-id]");
      if (row) {
        evt.preventDefault();
        ele.disabled = true;
        const id = row.getAttribute("data-id");
        try {
          await browser.runtime.sendMessage({ command: "toggle", id });
        } catch (_) {
          ele.disabled = false;
        }
      }
    } else if (ele.classList.contains("tab")) {
      document.querySelectorAll(".tab").forEach(tab => {
        tab.classList.remove("active");
      });
      ele.classList.add("active");
    }
  });

  availablePatches = info;
  redraw();
});

function onMessageFromAddon(msg) {
  const alsoShowHidden = location.hash === "#all";

  if ("interventionsChanged" in msg) {
    redrawTable($("#interventions"), msg.interventionsChanged, alsoShowHidden);
  }

  if ("overridesChanged" in msg) {
    redrawTable($("#overrides"), msg.overridesChanged, alsoShowHidden);
  }

  if ("shimsChanged" in msg) {
    updateShimTables(msg.shimsChanged, alsoShowHidden);
  }

  const id = msg.toggling || msg.toggled;
  const button = $(`[data-id="${id}"] button`);
  if (!button) {
    return;
  }
  const active = msg.active;
  document.l10n.setAttributes(
    button,
    active ? "label-disable" : "label-enable"
  );
  button.disabled = !!msg.toggling;
}

function redraw() {
  if (!availablePatches) {
    return;
  }
  const { overrides, interventions, shims } = availablePatches;
  const alsoShowHidden = location.hash === "#all";
  redrawTable($("#overrides"), overrides, alsoShowHidden);
  redrawTable($("#interventions"), interventions, alsoShowHidden);
  updateShimTables(shims, alsoShowHidden);
}

function clearTableAndAddMessage(table, msgId) {
  table.querySelectorAll("tr").forEach(tr => {
    tr.remove();
  });

  const tr = document.createElement("tr");
  tr.className = "message";
  tr.id = msgId;

  const td = document.createElement("td");
  td.setAttribute("colspan", "3");
  document.l10n.setAttributes(td, msgId);
  tr.appendChild(td);

  table.appendChild(tr);
}

function hideMessagesOnTable(table) {
  table.querySelectorAll("tr.message").forEach(tr => {
    tr.remove();
  });
}

function updateShimTables(shimsChanged, alsoShowHidden) {
  const tables = document.querySelectorAll("table.shims");
  if (!tables.length) {
    return;
  }

  for (const { bug, disabledReason, hidden, id, name, type } of shimsChanged) {
    // if any shim is disabled by global pref, all of them are. just show the
    // "disabled in about:config" message on each shim table in that case.
    if (disabledReason === "globalPref") {
      for (const table of tables) {
        clearTableAndAddMessage(table, "text-disabled-in-about-config");
      }
      return;
    }

    // otherwise, find which table the shim belongs in. if there is none,
    // ignore the shim (we're not showing it on the UI for whatever reason).
    const table = document.querySelector(`table.shims#${type}`);
    if (!table) {
      continue;
    }

    // similarly, skip shims hidden from the UI (only for testing, etc).
    if (!alsoShowHidden && hidden) {
      continue;
    }

    // also, hide the shim if it is disabled because it is not meant for this
    // platform, release (etc) rather than being disabled by pref/about:compat
    const notApplicable =
      disabledReason &&
      disabledReason !== "pref" &&
      disabledReason !== "session";
    if (!alsoShowHidden && notApplicable) {
      continue;
    }

    // create an updated table-row for the shim
    const tr = document.createElement("tr");
    tr.setAttribute("data-id", id);

    let td = document.createElement("td");
    td.innerText = name;
    tr.appendChild(td);

    td = document.createElement("td");
    const a = document.createElement("a");
    a.href = `https://bugzilla.mozilla.org/show_bug.cgi?id=${bug}`;
    document.l10n.setAttributes(a, "label-more-information", { bug });
    a.target = "_blank";
    td.appendChild(a);
    tr.appendChild(td);

    td = document.createElement("td");
    tr.appendChild(td);
    const button = document.createElement("button");
    document.l10n.setAttributes(
      button,
      disabledReason ? "label-enable" : "label-disable"
    );
    td.appendChild(button);

    // is it already in the table?
    const row = table.querySelector(`tr[data-id="${id}"]`);
    if (row) {
      row.replaceWith(tr);
    } else {
      table.appendChild(tr);
    }
  }

  for (const table of tables) {
    if (!table.querySelector("tr:not(.message)")) {
      // no shims? then add a message that none are available for this platform/config
      clearTableAndAddMessage(table, `text-no-${table.id}`);
    } else {
      // otherwise hide any such message, since we have shims on the list
      hideMessagesOnTable(table);
    }
  }
}

function redrawTable(table, data, alsoShowHidden) {
  const df = document.createDocumentFragment();
  table.querySelectorAll("tr").forEach(tr => {
    tr.remove();
  });

  let noEntriesMessage;
  if (data === false) {
    noEntriesMessage = "text-disabled-in-about-config";
  } else if (data.length === 0) {
    noEntriesMessage = `text-no-${table.id}`;
  }

  if (noEntriesMessage) {
    const tr = document.createElement("tr");
    df.appendChild(tr);

    const td = document.createElement("td");
    td.setAttribute("colspan", "3");
    document.l10n.setAttributes(td, noEntriesMessage);
    tr.appendChild(td);

    table.appendChild(df);
    return;
  }

  for (const row of data) {
    if (row.hidden && !alsoShowHidden) {
      continue;
    }

    const tr = document.createElement("tr");
    tr.setAttribute("data-id", row.id);
    df.appendChild(tr);

    let td = document.createElement("td");
    td.innerText = row.domain;
    tr.appendChild(td);

    td = document.createElement("td");
    const a = document.createElement("a");
    const bug = row.bug;
    a.href = `https://bugzilla.mozilla.org/show_bug.cgi?id=${bug}`;
    document.l10n.setAttributes(a, "label-more-information", { bug });
    a.target = "_blank";
    td.appendChild(a);
    tr.appendChild(td);

    td = document.createElement("td");
    tr.appendChild(td);
    const button = document.createElement("button");
    document.l10n.setAttributes(
      button,
      row.active ? "label-disable" : "label-enable"
    );
    td.appendChild(button);
  }
  table.appendChild(df);
}

window.onhashchange = redraw;
