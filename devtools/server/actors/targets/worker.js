/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Target actor for any of the various kinds of workers.
 *
 * See devtools/docs/backend/actor-hierarchy.md for more details.
 */

const { Ci } = require("chrome");
const ChromeUtils = require("ChromeUtils");
const { DebuggerServer } = require("devtools/server/debugger-server");
const { XPCOMUtils } = require("resource://gre/modules/XPCOMUtils.jsm");
const protocol = require("devtools/shared/protocol");
const { workerTargetSpec } = require("devtools/shared/specs/targets/worker");

loader.lazyRequireGetter(this, "ChromeUtils");

XPCOMUtils.defineLazyServiceGetter(
  this,
  "swm",
  "@mozilla.org/serviceworkers/manager;1",
  "nsIServiceWorkerManager"
);

const WorkerTargetActor = protocol.ActorClassWithSpec(workerTargetSpec, {
  initialize(conn, dbg) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this._dbg = dbg;
    this._attached = false;
    this._threadActor = null;
    this._transport = null;
  },

  form() {
    const form = {
      actor: this.actorID,
      consoleActor: this._consoleActor,
      threadActor: this._threadActor,
      id: this._dbg.id,
      url: this._dbg.url,
      traits: {
        isParentInterceptEnabled: swm.isParentInterceptEnabled(),
      },
      type: this._dbg.type,
    };
    if (this._dbg.type === Ci.nsIWorkerDebugger.TYPE_SERVICE) {
      /**
       * With parent-intercept mode, the ServiceWorkerManager in content
       * processes don't maintain ServiceWorkerRegistrations; record the
       * ServiceWorker's ID, and this data will be merged with the
       * corresponding registration in the parent process.
       */
      if (!swm.isParentInterceptEnabled() || !DebuggerServer.isInChildProcess) {
        const registration = this._getServiceWorkerRegistrationInfo();
        form.scope = registration.scope;
        const newestWorker =
          registration.activeWorker ||
          registration.waitingWorker ||
          registration.installingWorker;
        form.fetch = newestWorker && newestWorker.handlesFetchEvents;
      }
    }
    return form;
  },

  attach() {
    if (this._dbg.isClosed) {
      return { error: "closed" };
    }

    if (!this._attached) {
      const isServiceWorker =
        this._dbg.type == Ci.nsIWorkerDebugger.TYPE_SERVICE;
      if (isServiceWorker) {
        this._preventServiceWorkerShutdown();
      }
      this._dbg.addListener(this);
      this._attached = true;
    }

    return {
      type: "attached",
      url: this._dbg.url,
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
    if (this._attached) {
      this._detach();
    }
    protocol.Actor.prototype.destroy.call(this);
  },

  connect(options) {
    if (!this._attached) {
      return { error: "wrongState" };
    }

    if (this._threadActor !== null) {
      return {
        type: "connected",
        threadActor: this._threadActor,
      };
    }

    return DebuggerServer.connectToWorker(
      this.conn,
      this._dbg,
      this.actorID,
      options
    ).then(
      ({ threadActor, transport, consoleActor }) => {
        this._threadActor = threadActor;
        this._transport = transport;
        this._consoleActor = consoleActor;

        return {
          type: "connected",
          threadActor: this._threadActor,
          consoleActor: this._consoleActor,
        };
      },
      error => {
        return { error: error.toString() };
      }
    );
  },

  push() {
    if (this._dbg.type !== Ci.nsIWorkerDebugger.TYPE_SERVICE) {
      return { error: "wrongType" };
    }
    const registration = this._getServiceWorkerRegistrationInfo();
    const originAttributes = ChromeUtils.originAttributesToSuffix(
      this._dbg.principal.originAttributes
    );
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
    const registration = this._getServiceWorkerRegistrationInfo();
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
    } catch (e) {
      // nothing
    }

    const isServiceWorker = type == Ci.nsIWorkerDebugger.TYPE_SERVICE;
    if (isServiceWorker) {
      this._allowServiceWorkerShutdown();
    }

    this._dbg.removeListener(this);
    this._attached = false;
  },

  /**
   * Automatically disable the internal sw timeout that shut them down by calling
   * nsIWorkerInfo.attachDebugger().
   * This can be removed when Bug 1496997 lands.
   */
  _preventServiceWorkerShutdown() {
    if (swm.isParentInterceptEnabled()) {
      // In parentIntercept mode, the worker target actor cannot call attachDebugger
      // because this API can only be called from the parent process. This will be
      // done by the worker target front.
      return;
    }

    const worker = this._getServiceWorkerInfo();
    if (worker) {
      worker.attachDebugger();
    }
  },

  /**
   * Allow the service worker to time out. See _preventServiceWorkerShutdown.
   */
  _allowServiceWorkerShutdown() {
    if (swm.isParentInterceptEnabled()) {
      return;
    }

    const worker = this._getServiceWorkerInfo();
    if (worker) {
      worker.detachDebugger();
    }
  },
});

exports.WorkerTargetActor = WorkerTargetActor;
