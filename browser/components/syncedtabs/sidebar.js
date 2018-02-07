/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://services-sync/SyncedTabs.jsm");
ChromeUtils.import("resource:///modules/syncedtabs/SyncedTabsDeckComponent.js");

ChromeUtils.defineModuleGetter(this, "fxAccounts",
                               "resource://gre/modules/FxAccounts.jsm");

var syncedTabsDeckComponent = new SyncedTabsDeckComponent({window, SyncedTabs, fxAccounts});

let onLoaded = () => {
  syncedTabsDeckComponent.init();
  document.getElementById("template-container").appendChild(syncedTabsDeckComponent.container);
};

let onUnloaded = () => {
  removeEventListener("DOMContentLoaded", onLoaded);
  removeEventListener("unload", onUnloaded);
  syncedTabsDeckComponent.uninit();
};

addEventListener("DOMContentLoaded", onLoaded);
addEventListener("unload", onUnloaded);
