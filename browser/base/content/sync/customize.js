/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");

let service = Cc["@mozilla.org/weave/service;1"]
                .getService(Ci.nsISupports)
                .wrappedJSObject;

if (!service.allowPasswordsEngine) {
  let checkbox = document.getElementById("fxa-pweng-chk");
  checkbox.checked = false;
  checkbox.disabled = true;
}

addEventListener("dialogaccept", function () {
  let pane = document.getElementById("sync-customize-pane");
  pane.writePreferences(true);
  window.arguments[0].accepted = true;
});
