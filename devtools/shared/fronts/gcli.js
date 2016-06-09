/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Front, FrontClassWithSpec } = require("devtools/shared/protocol");
const { gcliSpec } = require("devtools/shared/specs/gcli");

/**
 *
 */
const GcliFront = exports.GcliFront = FrontClassWithSpec(gcliSpec, {
  initialize: function(client, tabForm) {
    Front.prototype.initialize.call(this, client);
    this.actorID = tabForm.gcliActor;

    // XXX: This is the first actor type in its hierarchy to use the protocol
    // library, so we're going to self-own on the client side for now.
    this.manage(this);
  },
});

// A cache of created fronts: WeakMap<Client, Front>
const knownFronts = new WeakMap();

/**
 * Create a GcliFront only when needed (returns a promise)
 * For notes on target.makeRemote(), see
 * https://bugzilla.mozilla.org/show_bug.cgi?id=1016330#c7
 */
exports.GcliFront.create = function(target) {
  return target.makeRemote().then(() => {
    let front = knownFronts.get(target.client);
    if (front == null && target.form.gcliActor != null) {
      front = new GcliFront(target.client, target.form);
      knownFronts.set(target.client, front);
    }
    return front;
  });
};
