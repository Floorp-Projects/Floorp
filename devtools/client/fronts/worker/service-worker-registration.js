/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  serviceWorkerRegistrationSpec,
} = require("resource://devtools/shared/specs/worker/service-worker-registration.js");
const {
  FrontClassWithSpec,
  registerFront,
  types,
} = require("resource://devtools/shared/protocol.js");

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

  get evaluatingWorker() {
    return this._getServiceWorker("evaluatingWorker");
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
    // @backward-compat { version 70 } ServiceWorkerRegistration actor now exposes traits
    this.traits = form.traits || {};
  }
}

exports.ServiceWorkerRegistrationFront = ServiceWorkerRegistrationFront;
registerFront(ServiceWorkerRegistrationFront);
