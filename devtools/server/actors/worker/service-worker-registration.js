/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = require("resource://gre/modules/XPCOMUtils.sys.mjs");
const protocol = require("devtools/shared/protocol");
const {
  serviceWorkerRegistrationSpec,
} = require("devtools/shared/specs/worker/service-worker-registration");
const {
  PushSubscriptionActor,
} = require("devtools/server/actors/worker/push-subscription");
const {
  ServiceWorkerActor,
} = require("devtools/server/actors/worker/service-worker");

XPCOMUtils.defineLazyServiceGetter(
  this,
  "swm",
  "@mozilla.org/serviceworkers/manager;1",
  "nsIServiceWorkerManager"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "PushService",
  "@mozilla.org/push/Service;1",
  "nsIPushService"
);

const ServiceWorkerRegistrationActor = protocol.ActorClassWithSpec(
  serviceWorkerRegistrationSpec,
  {
    /**
     * Create the ServiceWorkerRegistrationActor
     * @param DevToolsServerConnection conn
     *   The server connection.
     * @param ServiceWorkerRegistrationInfo registration
     *   The registration's information.
     */
    initialize(conn, registration) {
      protocol.Actor.prototype.initialize.call(this, conn);
      this._conn = conn;
      this._registration = registration;
      this._pushSubscriptionActor = null;

      // A flag to know if preventShutdown has been called and we should
      // try to allow the shutdown of the SW when the actor is destroyed
      this._preventedShutdown = false;

      this._registration.addListener(this);

      this._createServiceWorkerActors();

      Services.obs.addObserver(this, PushService.subscriptionModifiedTopic);
    },

    onChange() {
      this._destroyServiceWorkerActors();
      this._createServiceWorkerActors();
      this.emit("registration-changed");
    },

    form() {
      const registration = this._registration;
      const evaluatingWorker = this._evaluatingWorker.form();
      const installingWorker = this._installingWorker.form();
      const waitingWorker = this._waitingWorker.form();
      const activeWorker = this._activeWorker.form();

      const newestWorker =
        activeWorker || waitingWorker || installingWorker || evaluatingWorker;

      return {
        actor: this.actorID,
        scope: registration.scope,
        url: registration.scriptSpec,
        evaluatingWorker,
        installingWorker,
        waitingWorker,
        activeWorker,
        fetch: newestWorker?.fetch,
        // Check if we have an active worker
        active: !!activeWorker,
        lastUpdateTime: registration.lastUpdateTime,
        traits: {},
      };
    },

    destroy() {
      protocol.Actor.prototype.destroy.call(this);

      // Ensure resuming the service worker in case the connection drops
      if (this._registration.activeWorker && this._preventedShutdown) {
        this.allowShutdown();
      }

      Services.obs.removeObserver(this, PushService.subscriptionModifiedTopic);
      this._registration.removeListener(this);
      this._registration = null;
      if (this._pushSubscriptionActor) {
        this._pushSubscriptionActor.destroy();
      }
      this._pushSubscriptionActor = null;

      this._destroyServiceWorkerActors();

      this._evaluatingWorker = null;
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
      const { activeWorker } = this._registration;

      // TODO: don't return "started" if there's no active worker.
      if (activeWorker) {
        // This starts up the Service Worker if it's not already running.
        // Note that the Service Workers exist in content processes but are
        // managed from the parent process. This is why we call `attachDebugger`
        // here (in the parent process) instead of in a process script.
        activeWorker.attachDebugger();
        activeWorker.detachDebugger();
      }

      return { type: "started" };
    },

    unregister() {
      const { principal, scope } = this._registration;
      const unregisterCallback = {
        unregisterSucceeded() {},
        unregisterFailed() {
          console.error("Failed to unregister the service worker for " + scope);
        },
        QueryInterface: ChromeUtils.generateQI([
          "nsIServiceWorkerUnregisterCallback",
        ]),
      };
      swm.propagateUnregister(principal, unregisterCallback, scope);

      return { type: "unregistered" };
    },

    push() {
      const { principal, scope } = this._registration;
      const originAttributes = ChromeUtils.originAttributesToSuffix(
        principal.originAttributes
      );
      swm.sendPushEvent(originAttributes, scope);
    },

    /**
     * Prevent the current active worker to shutdown after the idle timeout.
     */
    preventShutdown() {
      if (!this._registration.activeWorker) {
        throw new Error(
          "ServiceWorkerRegistrationActor.preventShutdown could not find " +
            "activeWorker in parent-intercept mode"
        );
      }

      // attachDebugger has to be called from the parent process in parent-intercept mode.
      this._registration.activeWorker.attachDebugger();
      this._preventedShutdown = true;
    },

    /**
     * Allow the current active worker to shut down again.
     */
    allowShutdown() {
      if (!this._registration.activeWorker) {
        throw new Error(
          "ServiceWorkerRegistrationActor.allowShutdown could not find " +
            "activeWorker in parent-intercept mode"
        );
      }

      this._registration.activeWorker.detachDebugger();
      this._preventedShutdown = false;
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
            pushSubscriptionActor = new PushSubscriptionActor(
              this._conn,
              subscription
            );
            this._pushSubscriptionActor = pushSubscriptionActor;
            resolve(pushSubscriptionActor);
          }
        );
      });
    },

    _destroyServiceWorkerActors() {
      this._evaluatingWorker.destroy();
      this._installingWorker.destroy();
      this._waitingWorker.destroy();
      this._activeWorker.destroy();
    },

    _createServiceWorkerActors() {
      const {
        evaluatingWorker,
        installingWorker,
        waitingWorker,
        activeWorker,
      } = this._registration;

      this._evaluatingWorker = new ServiceWorkerActor(
        this._conn,
        evaluatingWorker
      );
      this._installingWorker = new ServiceWorkerActor(
        this._conn,
        installingWorker
      );
      this._waitingWorker = new ServiceWorkerActor(this._conn, waitingWorker);
      this._activeWorker = new ServiceWorkerActor(this._conn, activeWorker);

      // Add the ServiceWorker actors as children of this ServiceWorkerRegistration actor,
      // assigning them valid actorIDs.
      this.manage(this._evaluatingWorker);
      this.manage(this._installingWorker);
      this.manage(this._waitingWorker);
      this.manage(this._activeWorker);
    },
  }
);

exports.ServiceWorkerRegistrationActor = ServiceWorkerRegistrationActor;
