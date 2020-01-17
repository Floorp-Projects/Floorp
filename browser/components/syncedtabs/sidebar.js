/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { SyncedTabs } = ChromeUtils.import(
  "resource://services-sync/SyncedTabs.jsm"
);
const { SyncedTabsDeckComponent } = ChromeUtils.import(
  "resource:///modules/syncedtabs/SyncedTabsDeckComponent.js"
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

  // Needed due to Bug 1596852.
  // Should be removed once this bug is resolved.
  window.addEventListener(
    "pageshow",
    e => {
      window
        .getWindowGlobalChild()
        .getActor("LightweightTheme")
        .handleEvent(e);
    },
    { once: true }
  );
};

let onUnloaded = () => {
  removeEventListener("DOMContentLoaded", onLoaded);
  removeEventListener("unload", onUnloaded);
  syncedTabsDeckComponent.uninit();
};

addEventListener("DOMContentLoaded", onLoaded);
addEventListener("unload", onUnloaded);
