/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-sync/SyncedTabs.jsm");
Cu.import("resource:///modules/syncedtabs/SyncedTabsDeckComponent.js");

XPCOMUtils.defineLazyModuleGetter(this, "fxAccounts",
                                  "resource://gre/modules/FxAccounts.jsm");

this.syncedTabsDeckComponent = new SyncedTabsDeckComponent({window, SyncedTabs, fxAccounts});

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
