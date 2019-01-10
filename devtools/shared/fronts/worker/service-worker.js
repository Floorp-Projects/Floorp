/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  pushSubscriptionSpec,
  serviceWorkerRegistrationSpec,
  serviceWorkerSpec,
} = require("devtools/shared/specs/worker/service-worker");
const { FrontClassWithSpec, registerFront } = require("devtools/shared/protocol");

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

  form(form, detail) {
    if (detail === "actorid") {
      this.actorID = form;
      return;
    }

    this.actorID = form.actor;
    this._form = form;
  }
}
exports.PushSubscriptionFront = PushSubscriptionFront;
registerFront(PushSubscriptionFront);

class ServiceWorkerRegistrationFront extends
  FrontClassWithSpec(serviceWorkerRegistrationSpec) {
  get active() {
    return this._form.active;
  }

  get fetch() {
    return this._form.fetch;
  }

  get lastUpdateTime() {
    return this._form.lastUpdateTime;
  }

  get scope() {
    return this._form.scope;
  }

  get type() {
    return this._form.type;
  }

  get url() {
    return this._form.url;
  }

  form(form, detail) {
    if (detail === "actorid") {
      this.actorID = form;
      return;
    }

    this.actorID = form.actor;
    this._form = form;
  }
}
exports.ServiceWorkerRegistrationFront = ServiceWorkerRegistrationFront;
registerFront(ServiceWorkerRegistrationFront);

class ServiceWorkerFront extends FrontClassWithSpec(serviceWorkerSpec) {}
exports.ServiceWorkerFront = ServiceWorkerFront;
registerFront(ServiceWorkerFront);
