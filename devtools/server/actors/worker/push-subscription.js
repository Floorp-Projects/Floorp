/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor } = require("resource://devtools/shared/protocol.js");
const {
  pushSubscriptionSpec,
} = require("resource://devtools/shared/specs/worker/push-subscription.js");

class PushSubscriptionActor extends Actor {
  constructor(conn, subscription) {
    super(conn, pushSubscriptionSpec);
    this._subscription = subscription;
  }

  form() {
    const subscription = this._subscription;

    // Note: subscription.pushCount & subscription.lastPush are no longer
    // returned here because the corresponding getters throw on GeckoView.
    // Since they were not used in DevTools they were removed from the
    // actor in Bug 1637687. If they are reintroduced, make sure to provide
    // meaningful fallback values when debugging a GeckoView runtime.
    return {
      actor: this.actorID,
      endpoint: subscription.endpoint,
      quota: subscription.quota,
    };
  }

  destroy() {
    this._subscription = null;
    super.destroy();
  }
}
exports.PushSubscriptionActor = PushSubscriptionActor;
