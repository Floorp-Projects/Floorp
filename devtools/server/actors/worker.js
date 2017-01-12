/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const { DebuggerServer } = require("devtools/server/main");
const Services = require("Services");
const { XPCOMUtils } = require("resource://gre/modules/XPCOMUtils.jsm");
const protocol = require("devtools/shared/protocol");
const {
  workerSpec,
  pushSubscriptionSpec,
  serviceWorkerRegistrationSpec,
  serviceWorkerSpec,
} = require("devtools/shared/specs/worker");

loader.lazyRequireGetter(this, "ChromeUtils");
loader.lazyRequireGetter(this, "events", "sdk/event/core");

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

let WorkerActor = protocol.ActorClassWithSpec(workerSpec, {
  initialize(conn, dbg) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this._dbg = dbg;
    this._attached = false;
    this._threadActor = null;
    this._transport = null;
  },

  form(detail) {
    if (detail === "actorid") {
      return this.actorID;
    }
    let form = {
      actor: this.actorID,
      consoleActor: this._consoleActor,
      url: this._dbg.url,
      type: this._dbg.type
    };
    if (this._dbg.type === Ci.nsIWorkerDebugger.TYPE_SERVICE) {
      let registration = this._getServiceWorkerRegistrationInfo();
      form.scope = registration.scope;
    }
    return form;
  },

  attach() {
    if (this._dbg.isClosed) {
      return { error: "closed" };
    }

    if (!this._attached) {
      // Automatically disable their internal timeout that shut them down
      // Should be refactored by having actors specific to service workers
      if (this._dbg.type == Ci.nsIWorkerDebugger.TYPE_SERVICE) {
        let worker = this._getServiceWorkerInfo();
        if (worker) {
          worker.attachDebugger();
        }
      }
      this._dbg.addListener(this);
      this._attached = true;
    }

    return {
      type: "attached",
      url: this._dbg.url
    };
  },

  detach() {
    if (!this._attached) {
      return { error: "wrongState" };
    }

    this._detach();

    return { type: "detached" };
  },

  destroy() {
    protocol.Actor.prototype.destroy.call(this);
    if (this._attached) {
      this._detach();
    }
  },

  connect(options) {
    if (!this._attached) {
      return { error: "wrongState" };
    }

    if (this._threadActor !== null) {
      return {
        type: "connected",
        threadActor: this._threadActor
      };
    }

    return DebuggerServer.connectToWorker(
      this.conn, this._dbg, this.actorID, options
    ).then(({ threadActor, transport, consoleActor }) => {
      this._threadActor = threadActor;
      this._transport = transport;
      this._consoleActor = consoleActor;

      return {
        type: "connected",
        threadActor: this._threadActor,
        consoleActor: this._consoleActor
      };
    }, (error) => {
      return { error: error.toString() };
    });
  },

  push() {
    if (this._dbg.type !== Ci.nsIWorkerDebugger.TYPE_SERVICE) {
      return { error: "wrongType" };
    }
    let registration = this._getServiceWorkerRegistrationInfo();
    let originAttributes = ChromeUtils.originAttributesToSuffix(
      this._dbg.principal.originAttributes);
    swm.sendPushEvent(originAttributes, registration.scope);
    return { type: "pushed" };
  },

  onClose() {
    if (this._attached) {
      this._detach();
    }

    this.conn.sendActorEvent(this.actorID, "close");
  },

  onError(filename, lineno, message) {
    reportError("ERROR:" + filename + ":" + lineno + ":" + message + "\n");
  },

  _getServiceWorkerRegistrationInfo() {
    return swm.getRegistrationByPrincipal(this._dbg.principal, this._dbg.url);
  },

  _getServiceWorkerInfo() {
    let registration = this._getServiceWorkerRegistrationInfo();
    return registration.getWorkerByID(this._dbg.serviceWorkerID);
  },

  _detach() {
    if (this._threadActor !== null) {
      this._transport.close();
      this._transport = null;
      this._threadActor = null;
    }

    // If the worker is already destroyed, nsIWorkerDebugger.type throws
    // (_dbg.closed appears to be false when it throws)
    let type;
    try {
      type = this._dbg.type;
    } catch (e) {}

    if (type == Ci.nsIWorkerDebugger.TYPE_SERVICE) {
      let worker = this._getServiceWorkerInfo();
      if (worker) {
        worker.detachDebugger();
      }
    }

    this._dbg.removeListener(this);
    this._attached = false;
  }
});

exports.WorkerActor = WorkerActor;

let PushSubscriptionActor = protocol.ActorClassWithSpec(pushSubscriptionSpec, {
  initialize(conn, subscription) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this._subscription = subscription;
  },

  form(detail) {
    if (detail === "actorid") {
      return this.actorID;
    }
    let subscription = this._subscription;
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

let ServiceWorkerActor = protocol.ActorClassWithSpec(serviceWorkerSpec, {
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
    };
  },

  destroy() {
    protocol.Actor.prototype.destroy.call(this);
    this._worker = null;
  },
});

// Lazily load the service-worker-child.js process script only once.
let _serviceWorkerProcessScriptLoaded = false;

let ServiceWorkerRegistrationActor =
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

    let {installingWorker, waitingWorker, activeWorker} = registration;
    this._installingWorker = new ServiceWorkerActor(conn, installingWorker);
    this._waitingWorker = new ServiceWorkerActor(conn, waitingWorker);
    this._activeWorker = new ServiceWorkerActor(conn, activeWorker);

    Services.obs.addObserver(this, PushService.subscriptionModifiedTopic, false);
  },

  onChange() {
    this._installingWorker.destroy();
    this._waitingWorker.destroy();
    this._activeWorker.destroy();

    let {installingWorker, waitingWorker, activeWorker} = this._registration;
    this._installingWorker = new ServiceWorkerActor(this._conn, installingWorker);
    this._waitingWorker = new ServiceWorkerActor(this._conn, waitingWorker);
    this._activeWorker = new ServiceWorkerActor(this._conn, activeWorker);

    events.emit(this, "registration-changed");
  },

  form(detail) {
    if (detail === "actorid") {
      return this.actorID;
    }
    let registration = this._registration;
    let installingWorker = this._installingWorker.form();
    let waitingWorker = this._waitingWorker.form();
    let activeWorker = this._activeWorker.form();

    let isE10s = Services.appinfo.browserTabsRemoteAutostart;
    return {
      actor: this.actorID,
      scope: registration.scope,
      url: registration.scriptSpec,
      installingWorker,
      waitingWorker,
      activeWorker,
      // - In e10s: only active registrations are available.
      // - In non-e10s: registrations always have at least one worker, if the worker is
      // active, the registration is active.
      active: isE10s ? true : !!activeWorker
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
    let scope = this._registration.scope;
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
        events.emit(this, "push-subscription-modified");
        break;
    }
  },

  start() {
    if (!_serviceWorkerProcessScriptLoaded) {
      Services.ppmm.loadProcessScript(
        "resource://devtools/server/service-worker-child.js", true);
      _serviceWorkerProcessScriptLoaded = true;
    }
    Services.ppmm.broadcastAsyncMessage("serviceWorkerRegistration:start", {
      scope: this._registration.scope
    });
    return { type: "started" };
  },

  unregister() {
    let { principal, scope } = this._registration;
    let unregisterCallback = {
      unregisterSucceeded: function () {},
      unregisterFailed: function () {
        console.error("Failed to unregister the service worker for " + scope);
      },
      QueryInterface: XPCOMUtils.generateQI(
        [Ci.nsIServiceWorkerUnregisterCallback])
    };
    swm.propagateUnregister(principal, unregisterCallback, scope);

    return { type: "unregistered" };
  },

  getPushSubscription() {
    let registration = this._registration;
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
