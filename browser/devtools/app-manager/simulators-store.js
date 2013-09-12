/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {Cu} = require("chrome");
const ObservableObject = require("devtools/shared/observable-object");
const {Simulator} = Cu.import("resource://gre/modules/devtools/Simulator.jsm");

let store = new ObservableObject({versions:[]});

function feedStore() {
  store.object.versions = Simulator.availableVersions().map(v => {
    return {version:v}
  });
}

Simulator.on("register", feedStore);
Simulator.on("unregister", feedStore);
feedStore();

module.exports = store;
