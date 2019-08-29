/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  serviceWorkerRegistrationSpec,
} = require("devtools/shared/specs/worker/service-worker-registration");
const {
  FrontClassWithSpec,
  registerFront,
  types,
} = require("devtools/shared/protocol");

class ServiceWorkerRegistrationFront extends FrontClassWithSpec(
  serviceWorkerRegistrationSpec
) {
  get active() {
    return this._form.active;
  }

  get fetch() {
    return this._form.fetch;
  }

  get id() {
    return this.url;
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

  get activeWorker() {
    return this._getServiceWorker("activeWorker");
  }

  get installingWorker() {
    return this._getServiceWorker("installingWorker");
  }

  get waitingWorker() {
    return this._getServiceWorker("waitingWorker");
  }

  _getServiceWorker(type) {
    const workerForm = this._form[type];
    if (!workerForm) {
      return null;
    }
    return types.getType("serviceWorker").read(workerForm, this);
  }

  form(form) {
    this.actorID = form.actor;
    this._form = form;
    // FF70+ ServiceWorkerRegistration actor starts exposing traits object
    this.traits = form.traits || {};
  }
}

exports.ServiceWorkerRegistrationFront = ServiceWorkerRegistrationFront;
registerFront(ServiceWorkerRegistrationFront);
