/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const ChromeUtils = require("ChromeUtils");
const Services = require("Services");
const { XPCOMUtils } = require("resource://gre/modules/XPCOMUtils.jsm");
const protocol = require("devtools/shared/protocol");
const {
  pushSubscriptionSpec,
  serviceWorkerRegistrationSpec,
  serviceWorkerSpec,
} = require("devtools/shared/specs/worker/service-worker");

loader.lazyRequireGetter(this, "ChromeUtils");

XPCOMUtils.defineLazyServiceGetter(
  this, "swm",
  "@mozilla.org/serviceworkers/manager;1",
  "nsIServiceWorkerManager"
);

XPCOMUtils.defineLazyServiceGetter(
  this, "PushService",
  "@mozilla.org/push/Service;1",
  "nsIPushService"
);

const PushSubscriptionActor = protocol.ActorClassWithSpec(pushSubscriptionSpec, {
  initialize(conn, subscription) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this._subscription = subscription;
  },

  form(detail) {
    if (detail === "actorid") {
      return this.actorID;
    }
    const subscription = this._subscription;
    return {
      actor: this.actorID,
      endpoint: subscription.endpoint,
      pushCount: subscription.pushCount,
      lastPush: subscription.lastPush,
      quota: subscription.quota
    };
  },

  destroy() {
    protocol.Actor.prototype.destroy.call(this);
    this._subscription = null;
  },
});

const ServiceWorkerActor = protocol.ActorClassWithSpec(serviceWorkerSpec, {
  initialize(conn, worker) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this._worker = worker;
  },

  form() {
    if (!this._worker) {
      return null;
    }

    return {
      url: this._worker.scriptSpec,
      state: this._worker.state,
      fetch: this._worker.handlesFetchEvents
    };
  },

  destroy() {
    protocol.Actor.prototype.destroy.call(this);
    this._worker = null;
  },
});

// Lazily load the service-worker-process.js process script only once.
let _serviceWorkerProcessScriptLoaded = false;

const ServiceWorkerRegistrationActor =
protocol.ActorClassWithSpec(serviceWorkerRegistrationSpec, {
  /**
   * Create the ServiceWorkerRegistrationActor
   * @param DebuggerServerConnection conn
   *   The server connection.
   * @param ServiceWorkerRegistrationInfo registration
   *   The registration's information.
   */
  initialize(conn, registration) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this._conn = conn;
    this._registration = registration;
    this._pushSubscriptionActor = null;
    this._registration.addListener(this);

    const {installingWorker, waitingWorker, activeWorker} = registration;
    this._installingWorker = new ServiceWorkerActor(conn, installingWorker);
    this._waitingWorker = new ServiceWorkerActor(conn, waitingWorker);
    this._activeWorker = new ServiceWorkerActor(conn, activeWorker);

    Services.obs.addObserver(this, PushService.subscriptionModifiedTopic);
  },

  onChange() {
    this._installingWorker.destroy();
    this._waitingWorker.destroy();
    this._activeWorker.destroy();

    const {installingWorker, waitingWorker, activeWorker} = this._registration;
    this._installingWorker = new ServiceWorkerActor(this._conn, installingWorker);
    this._waitingWorker = new ServiceWorkerActor(this._conn, waitingWorker);
    this._activeWorker = new ServiceWorkerActor(this._conn, activeWorker);

    this.emit("registration-changed");
  },

  form(detail) {
    if (detail === "actorid") {
      return this.actorID;
    }
    const registration = this._registration;
    const installingWorker = this._installingWorker.form();
    const waitingWorker = this._waitingWorker.form();
    const activeWorker = this._activeWorker.form();

    const newestWorker = (activeWorker || waitingWorker || installingWorker);

    const isE10s = Services.appinfo.browserTabsRemoteAutostart;
    return {
      actor: this.actorID,
      scope: registration.scope,
      url: registration.scriptSpec,
      installingWorker,
      waitingWorker,
      activeWorker,
      fetch: newestWorker && newestWorker.fetch,
      // - In e10s: only active registrations are available.
      // - In non-e10s: registrations always have at least one worker, if the worker is
      // active, the registration is active.
      active: isE10s ? true : !!activeWorker,
      lastUpdateTime: registration.lastUpdateTime,
    };
  },

  destroy() {
    protocol.Actor.prototype.destroy.call(this);
    Services.obs.removeObserver(this, PushService.subscriptionModifiedTopic);
    this._registration.removeListener(this);
    this._registration = null;
    if (this._pushSubscriptionActor) {
      this._pushSubscriptionActor.destroy();
    }
    this._pushSubscriptionActor = null;

    this._installingWorker.destroy();
    this._waitingWorker.destroy();
    this._activeWorker.destroy();

    this._installingWorker = null;
    this._waitingWorker = null;
    this._activeWorker = null;
  },

  /**
   * Standard observer interface to listen to push messages and changes.
   */
  observe(subject, topic, data) {
    const scope = this._registration.scope;
    if (data !== scope) {
      // This event doesn't concern us, pretend nothing happened.
      return;
    }
    switch (topic) {
      case PushService.subscriptionModifiedTopic:
        if (this._pushSubscriptionActor) {
          this._pushSubscriptionActor.destroy();
          this._pushSubscriptionActor = null;
        }
        this.emit("push-subscription-modified");
        break;
    }
  },

  start() {
    if (!_serviceWorkerProcessScriptLoaded) {
      Services.ppmm.loadProcessScript(
        "resource://devtools/server/actors/worker/service-worker-process.js", true);
      _serviceWorkerProcessScriptLoaded = true;
    }

    // XXX: Send the permissions down to the content process before starting
    // the service worker within the content process. As we don't know what
    // content process we're starting the service worker in (as we're using a
    // broadcast channel to talk to it), we just broadcast the permissions to
    // everyone as well.
    //
    // This call should be replaced with a proper implementation when
    // ServiceWorker debugging is improved to support multiple content processes
    // correctly.
    Services.perms.broadcastPermissionsForPrincipalToAllContentProcesses(
      this._registration.principal);

    Services.ppmm.broadcastAsyncMessage("serviceWorkerRegistration:start", {
      scope: this._registration.scope
    });
    return { type: "started" };
  },

  unregister() {
    const { principal, scope } = this._registration;
    const unregisterCallback = {
      unregisterSucceeded: function() {},
      unregisterFailed: function() {
        console.error("Failed to unregister the service worker for " + scope);
      },
      QueryInterface: ChromeUtils.generateQI(
        [Ci.nsIServiceWorkerUnregisterCallback])
    };
    swm.propagateUnregister(principal, unregisterCallback, scope);

    return { type: "unregistered" };
  },

  getPushSubscription() {
    const registration = this._registration;
    let pushSubscriptionActor = this._pushSubscriptionActor;
    if (pushSubscriptionActor) {
      return Promise.resolve(pushSubscriptionActor);
    }
    return new Promise((resolve, reject) => {
      PushService.getSubscription(
        registration.scope,
        registration.principal,
        (result, subscription) => {
          if (!subscription) {
            resolve(null);
            return;
          }
          pushSubscriptionActor = new PushSubscriptionActor(this._conn, subscription);
          this._pushSubscriptionActor = pushSubscriptionActor;
          resolve(pushSubscriptionActor);
        }
      );
    });
  },
});

exports.ServiceWorkerRegistrationActor = ServiceWorkerRegistrationActor;
