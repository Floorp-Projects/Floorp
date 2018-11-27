/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {addonTargetSpec} = require("devtools/shared/specs/targets/addon");
const protocol = require("devtools/shared/protocol");
const {custom} = protocol;

const AddonTargetFront = protocol.FrontClassWithSpec(addonTargetSpec, {
  initialize: function(client) {
    protocol.Front.prototype.initialize.call(this, client);

    this.client = client;

    this.traits = {};
  },

  form(json) {
    this.actorID = json.actor;

    // Save the full form for Target class usage.
    // Do not use `form` name to avoid colliding with protocol.js's `form` method
    this.targetForm = json;

    // We used to manipulate the form rather than the front itself.
    // Expose all form attributes to ease accessing them.
    for (const name in json) {
      if (name == "actor") {
        continue;
      }
      this[name] = json[name];
    }
  },

  isLegacyTemporaryExtension() {
    if (!this.type) {
      // If about:debugging is connected to an older then 59 remote Firefox, and type is
      // not available on the addon/webextension actors, return false to avoid showing
      // irrelevant warning messages.
      return false;
    }
    return this.type == "extension" &&
           this.temporarilyInstalled &&
           !this.isWebExtension &&
           !this.isAPIExtension;
  },

  attach: custom(async function() {
    const response = await this._attach();

    this.threadActor = response.threadActor;

    return response;
  }, {
    impl: "_attach",
  }),

  reconfigure: function() {
    // Toolbox and options panel are calling this method but Addon Target can't be
    // reconfigured. So we ignore this call here.
    return Promise.resolve();
  },

  attachThread: function() {
    return this.client.attachThread(this.threadActor);
  },
});

exports.AddonTargetFront = AddonTargetFront;
