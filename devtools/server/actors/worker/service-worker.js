/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const protocol = require("devtools/shared/protocol");
const {
  serviceWorkerSpec,
} = require("devtools/shared/specs/worker/service-worker");

const ServiceWorkerActor = protocol.ActorClassWithSpec(serviceWorkerSpec, {
  initialize(conn, worker) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this._worker = worker;
  },

  form() {
    if (!this._worker) {
      return null;
    }

    // handlesFetchEvents is not available if the worker's main script is in the
    // evaluating state.
    const isEvaluating =
      this._worker.state == Ci.nsIServiceWorkerInfo.STATE_PARSED;
    const fetch = isEvaluating ? undefined : this._worker.handlesFetchEvents;

    return {
      actor: this.actorID,
      url: this._worker.scriptSpec,
      state: this._worker.state,
      fetch,
      id: this._worker.id,
    };
  },

  destroy() {
    protocol.Actor.prototype.destroy.call(this);
    this._worker = null;
  },
});

exports.ServiceWorkerActor = ServiceWorkerActor;
