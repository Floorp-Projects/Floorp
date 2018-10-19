/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const protocol = require("devtools/shared/protocol");
const {
  callWatcherSpec,
} = require("chrome://mochitests/content/browser/devtools/client/canvasdebugger/test/call-watcher-spec");
const {CallWatcher} = require("devtools/server/actors/utils/call-watcher");

/**
 * This actor observes function calls on certain objects or globals.
 * It wraps the CallWatcher Helper so that it can be observed by tests
 */
exports.CallWatcherActor = protocol.ActorClassWithSpec(callWatcherSpec, {
  initialize: function(conn, targetActor) {
    protocol.Actor.prototype.initialize.call(this, conn);
    CallWatcher.call(this, conn, targetActor);
  },
  destroy: function(conn) {
    protocol.Actor.prototype.destroy.call(this, conn);
    this.finalize();
  },
  ...CallWatcher.prototype,
});
