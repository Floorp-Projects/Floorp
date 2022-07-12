/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const { XPCOMUtils } = require("resource://gre/modules/XPCOMUtils.sys.mjs");
loader.lazyRequireGetter(
  this,
  "ServiceWorkerRegistrationActor",
  "devtools/server/actors/worker/service-worker-registration",
  true
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "swm",
  "@mozilla.org/serviceworkers/manager;1",
  "nsIServiceWorkerManager"
);

function ServiceWorkerRegistrationActorList(conn) {
  this._conn = conn;
  this._actors = new Map();
  this._onListChanged = null;
  this._mustNotify = false;
  this.onRegister = this.onRegister.bind(this);
  this.onUnregister = this.onUnregister.bind(this);
}

ServiceWorkerRegistrationActorList.prototype = {
  getList() {
    // Create a set of registrations.
    const registrations = new Set();
    const array = swm.getAllRegistrations();
    for (let index = 0; index < array.length; ++index) {
      registrations.add(
        array.queryElementAt(index, Ci.nsIServiceWorkerRegistrationInfo)
      );
    }

    // Delete each actor for which we don't have a registration.
    for (const [registration] of this._actors) {
      if (!registrations.has(registration)) {
        this._actors.delete(registration);
      }
    }

    // Create an actor for each registration for which we don't have one.
    for (const registration of registrations) {
      if (!this._actors.has(registration)) {
        this._actors.set(
          registration,
          new ServiceWorkerRegistrationActor(this._conn, registration)
        );
      }
    }

    if (!this._mustNotify) {
      if (this._onListChanged !== null) {
        swm.addListener(this);
      }
      this._mustNotify = true;
    }

    const actors = [];
    for (const [, actor] of this._actors) {
      actors.push(actor);
    }

    return Promise.resolve(actors);
  },

  get onListchanged() {
    return this._onListchanged;
  },

  set onListChanged(onListChanged) {
    if (typeof onListChanged !== "function" && onListChanged !== null) {
      throw new Error("onListChanged must be either a function or null.");
    }

    if (this._mustNotify) {
      if (this._onListChanged === null && onListChanged !== null) {
        swm.addListener(this);
      }
      if (this._onListChanged !== null && onListChanged === null) {
        swm.removeListener(this);
      }
    }
    this._onListChanged = onListChanged;
  },

  _notifyListChanged() {
    this._onListChanged();

    if (this._onListChanged !== null) {
      swm.removeListener(this);
    }
    this._mustNotify = false;
  },

  onRegister(registration) {
    this._notifyListChanged();
  },

  onUnregister(registration) {
    this._notifyListChanged();
  },
};

exports.ServiceWorkerRegistrationActorList = ServiceWorkerRegistrationActorList;
