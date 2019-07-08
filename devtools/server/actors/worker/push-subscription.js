/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const protocol = require("devtools/shared/protocol");
const {
  pushSubscriptionSpec,
} = require("devtools/shared/specs/worker/push-subscription");

const PushSubscriptionActor = protocol.ActorClassWithSpec(
  pushSubscriptionSpec,
  {
    initialize(conn, subscription) {
      protocol.Actor.prototype.initialize.call(this, conn);
      this._subscription = subscription;
    },

    form() {
      const subscription = this._subscription;
      return {
        actor: this.actorID,
        endpoint: subscription.endpoint,
        pushCount: subscription.pushCount,
        lastPush: subscription.lastPush,
        quota: subscription.quota,
      };
    },

    destroy() {
      this._subscription = null;
      protocol.Actor.prototype.destroy.call(this);
    },
  }
);
exports.PushSubscriptionActor = PushSubscriptionActor;
