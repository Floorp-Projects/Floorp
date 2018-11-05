/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {addonTargetSpec} = require("devtools/shared/specs/targets/addon");
const protocol = require("devtools/shared/protocol");
const {custom} = protocol;

const AddonTargetFront = protocol.FrontClassWithSpec(addonTargetSpec, {
  initialize: function(client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);

    this.client = client;

    this.traits = {};
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
