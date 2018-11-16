/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {contentProcessTargetSpec} = require("devtools/shared/specs/targets/content-process");
const protocol = require("devtools/shared/protocol");

const ContentProcessTargetFront = protocol.FrontClassWithSpec(contentProcessTargetSpec, {
  initialize: function(client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);

    this.client = client;
    this.chromeDebugger = form.chromeDebugger;

    // Save the full form for Target class usage
    // Do not use `form` name to avoid colliding with protocol.js's `form` method
    this.targetForm = form;

    this.traits = {};
  },

  attachThread() {
    return this.client.attachThread(this.chromeDebugger);
  },

  reconfigure: function() {
    // Toolbox and options panel are calling this method but Worker Target can't be
    // reconfigured. So we ignore this call here.
    return Promise.resolve();
  },
});

exports.ContentProcessTargetFront = ContentProcessTargetFront;
