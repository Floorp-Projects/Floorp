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
class CallWatcherFront extends protocol.FrontClassWithSpec(callWatcherSpec) {
  constructor(client) {
    super(client);

    // Attribute name from which to retrieve the actorID out of the target actor's form
    this.formAttributeName = "callWatcherActor";
  }
}
exports.CallWatcherFront = CallWatcherFront;
protocol.registerFront(CallWatcherFront);
