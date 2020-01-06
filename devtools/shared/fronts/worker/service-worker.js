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
const { Ci } = require("chrome");

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

  get stateText() {
    switch (this.state) {
      case Ci.nsIServiceWorkerInfo.STATE_PARSED:
        return "parsed";
      case Ci.nsIServiceWorkerInfo.STATE_INSTALLING:
        return "installing";
      case Ci.nsIServiceWorkerInfo.STATE_INSTALLED:
        return "installed";
      case Ci.nsIServiceWorkerInfo.STATE_ACTIVATING:
        return "activating";
      case Ci.nsIServiceWorkerInfo.STATE_ACTIVATED:
        return "activated";
      case Ci.nsIServiceWorkerInfo.STATE_REDUNDANT:
        return "redundant";
      default:
        return "unknown";
    }
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
