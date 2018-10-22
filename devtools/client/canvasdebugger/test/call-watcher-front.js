/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  callWatcherSpec,
} = require("chrome://mochitests/content/browser/devtools/client/canvasdebugger/test/call-watcher-spec");
const protocol = require("devtools/shared/protocol");

/**
 * The corresponding Front object for the CallWatcherActor.
 */
var CallWatcherFront =
exports.CallWatcherFront =
protocol.FrontClassWithSpec(callWatcherSpec, {
  initialize: function(client, { callWatcherActor }) {
    protocol.Front.prototype.initialize.call(this, client, { actor: callWatcherActor });
    this.manage(this);
  },
});
