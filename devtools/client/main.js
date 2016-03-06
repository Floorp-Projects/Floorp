/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { gDevTools } = require("devtools/client/framework/devtools");

// This is important step in initialization codepath where we are going to
// start registering all default tools and themes: create menuitems, keys, emit
// related events.
gDevTools.registerDefaults();

Object.defineProperty(exports, "Toolbox", {
  get: () => require("devtools/client/framework/toolbox").Toolbox
});
Object.defineProperty(exports, "TargetFactory", {
  get: () => require("devtools/client/framework/target").TargetFactory
});

const unloadObserver = {
  observe: function(subject) {
    if (subject.wrappedJSObject === require("@loader/unload")) {
      Services.obs.removeObserver(unloadObserver, "sdk:loader:destroy");
      gDevTools.unregisterDefaults();
    }
  }
};
Services.obs.addObserver(unloadObserver, "sdk:loader:destroy", false);
