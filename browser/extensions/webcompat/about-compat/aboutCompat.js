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
  browser.runtime.sendMessage("getOverridesAndInterventions"),
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
  if ("interventionsChanged" in msg) {
    redrawTable($("#interventions"), msg.interventionsChanged);
  }

  if ("overridesChanged" in msg) {
    redrawTable($("#overrides"), msg.overridesChanged);
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
  const { overrides, interventions } = availablePatches;
  const showHidden = location.hash === "#all";
  redrawTable($("#overrides"), overrides, showHidden);
  redrawTable($("#interventions"), interventions, showHidden);
}

function redrawTable(table, data, showHidden = false) {
  const df = document.createDocumentFragment();
  table.querySelectorAll("tr").forEach(tr => {
    tr.remove();
  });

  let noEntriesMessage;
  if (data === false) {
    noEntriesMessage = "text-disabled-in-about-config";
  } else if (data.length === 0) {
    noEntriesMessage =
      table.id === "overrides" ? "text-no-overrides" : "text-no-interventions";
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
    if (row.hidden && !showHidden) {
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
