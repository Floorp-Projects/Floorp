/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  serviceWorkerSpec,
} = require("resource://devtools/shared/specs/worker/service-worker.js");
const {
  FrontClassWithSpec,
  registerFront,
} = require("resource://devtools/shared/protocol.js");
const { LocalizationHelper } = require("resource://devtools/shared/l10n.js");

const L10N = new LocalizationHelper(
  "devtools/client/locales/debugger.properties"
);

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
        return L10N.getStr("serviceWorkerInfo.parsed");
      case Ci.nsIServiceWorkerInfo.STATE_INSTALLING:
        return L10N.getStr("serviceWorkerInfo.installing");
      case Ci.nsIServiceWorkerInfo.STATE_INSTALLED:
        return L10N.getStr("serviceWorkerInfo.installed");
      case Ci.nsIServiceWorkerInfo.STATE_ACTIVATING:
        return L10N.getStr("serviceWorkerInfo.activating");
      case Ci.nsIServiceWorkerInfo.STATE_ACTIVATED:
        return L10N.getStr("serviceWorkerInfo.activated");
      case Ci.nsIServiceWorkerInfo.STATE_REDUNDANT:
        return L10N.getStr("serviceWorkerInfo.redundant");
      default:
        return L10N.getStr("serviceWorkerInfo.unknown");
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
