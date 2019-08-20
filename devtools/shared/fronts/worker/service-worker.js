/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  serviceWorkerSpec,
} = require("devtools/shared/specs/worker/service-worker");
const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");

class ServiceWorkerFront extends FrontClassWithSpec(serviceWorkerSpec) {
  get fetch() {
    return this._form.fetch;
  }

  get url() {
    return this._form.url;
  }

  get state() {
    return this._form.state;
  }

  get id() {
    return this._form.id;
  }

  form(form) {
    this.actorID = form.actor;
    this._form = form;
  }
}

exports.ServiceWorkerFront = ServiceWorkerFront;
registerFront(ServiceWorkerFront);
