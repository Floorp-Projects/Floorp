/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {Cu} = require("chrome");
const ObservableObject = require("devtools/client/shared/observable-object");
const {Devices} = Cu.import("resource://gre/modules/devtools/shared/apps/Devices.jsm");

var store = new ObservableObject({versions:[]});

function feedStore() {
  store.object.available = Devices.helperAddonInstalled;
  store.object.devices = Devices.available().map(n => {
    return {name:n}
  });
}

Devices.on("register", feedStore);
Devices.on("unregister", feedStore);
Devices.on("addon-status-updated", feedStore);

feedStore();

module.exports = store;
