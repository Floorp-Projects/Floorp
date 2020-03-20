/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  pushSubscriptionSpec,
} = require("devtools/shared/specs/worker/push-subscription");
const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");

class PushSubscriptionFront extends FrontClassWithSpec(pushSubscriptionSpec) {
  get endpoint() {
    return this._form.endpoint;
  }

  get pushCount() {
    return this._form.pushCount;
  }

  get lastPush() {
    return this._form.lastPush;
  }

  get quota() {
    return this._form.quota;
  }

  form(form) {
    this.actorID = form.actor;
    this._form = form;
  }
}

exports.PushSubscriptionFront = PushSubscriptionFront;
registerFront(PushSubscriptionFront);
