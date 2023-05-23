/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { SyncedTabs } = ChromeUtils.importESModule(
  "resource://services-sync/SyncedTabs.sys.mjs"
);
const { SyncedTabsDeckComponent } = ChromeUtils.importESModule(
  "resource:///modules/syncedtabs/SyncedTabsDeckComponent.sys.mjs"
);

var syncedTabsDeckComponent = new SyncedTabsDeckComponent({
  window,
  SyncedTabs,
});

let onLoaded = () => {
  window.top.MozXULElement.insertFTLIfNeeded("browser/syncedTabs.ftl");
  window.top.document
    .getElementById("SyncedTabsSidebarContext")
    .querySelectorAll("[data-lazy-l10n-id]")
    .forEach(el => {
      el.setAttribute("data-l10n-id", el.getAttribute("data-lazy-l10n-id"));
      el.removeAttribute("data-lazy-l10n-id");
    });
  syncedTabsDeckComponent.init();
  document
    .getElementById("template-container")
    .appendChild(syncedTabsDeckComponent.container);
};

let onUnloaded = () => {
  removeEventListener("DOMContentLoaded", onLoaded);
  removeEventListener("unload", onUnloaded);
  syncedTabsDeckComponent.uninit();
};

addEventListener("DOMContentLoaded", onLoaded);
addEventListener("unload", onUnloaded);
