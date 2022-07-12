/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Target actor for any of the various kinds of workers.
 *
 * See devtools/docs/backend/actor-hierarchy.md for more details.
 */

// protocol.js uses objects as exceptions in order to define
// error packets.
/* eslint-disable no-throw-literal */

const { Ci } = require("chrome");
const { DevToolsServer } = require("devtools/server/devtools-server");
const { XPCOMUtils } = require("resource://gre/modules/XPCOMUtils.sys.mjs");
const protocol = require("devtools/shared/protocol");
const {
  workerDescriptorSpec,
} = require("devtools/shared/specs/descriptors/worker");
const {
  createWorkerSessionContext,
} = require("devtools/server/actors/watcher/session-context");

loader.lazyRequireGetter(
  this,
  "connectToWorker",
  "devtools/server/connectors/worker-connector",
  true
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "swm",
  "@mozilla.org/serviceworkers/manager;1",
  "nsIServiceWorkerManager"
);

const WorkerDescriptorActor = protocol.ActorClassWithSpec(
  workerDescriptorSpec,
  {
    initialize(conn, dbg) {
      protocol.Actor.prototype.initialize.call(this, conn);
      this._dbg = dbg;

      this._threadActor = null;
      this._transport = null;

      this._dbgListener = {
        onClose: this._onWorkerClose.bind(this),
        onError: this._onWorkerError.bind(this),
      };

      this._dbg.addListener(this._dbgListener);
      this._attached = true;
    },

    form() {
      const form = {
        actor: this.actorID,
        consoleActor: this._consoleActor,
        threadActor: this._threadActor,
        id: this._dbg.id,
        url: this._dbg.url,
        traits: {},
        type: this._dbg.type,
      };
      if (this._dbg.type === Ci.nsIWorkerDebugger.TYPE_SERVICE) {
        /**
         * The ServiceWorkerManager in content processes don't maintain
         * ServiceWorkerRegistrations; record the ServiceWorker's ID, and
         * this data will be merged with the corresponding registration in
         * the parent process.
         */
        if (!DevToolsServer.isInChildProcess) {
          const registration = this._getServiceWorkerRegistrationInfo();
          form.scope = registration.scope;
          const newestWorker =
            registration.activeWorker ||
            registration.waitingWorker ||
            registration.installingWorker;
          form.fetch = newestWorker?.handlesFetchEvents;
        }
      }
      return form;
    },

    detach() {
      if (!this._attached) {
        throw { error: "wrongState" };
      }

      this.destroy();
    },

    destroy() {
      if (this._attached) {
        this._detach();
      }

      this.emit("descriptor-destroyed");
      protocol.Actor.prototype.destroy.call(this);
    },

    async getTarget() {
      if (!this._attached) {
        return { error: "wrongState" };
      }

      if (this._threadActor !== null) {
        return {
          type: "connected",
          threadActor: this._threadActor,
          consoleActor: this._consoleActor,
        };
      }

      try {
        const { transport, workerTargetForm } = await connectToWorker(
          this.conn,
          this._dbg,
          this.actorID,
          {
            sessionContext: createWorkerSessionContext(),
          }
        );

        this._threadActor = workerTargetForm.threadActor;
        this._consoleActor = workerTargetForm.consoleActor;
        this._transport = transport;

        return {
          type: "connected",
          threadActor: this._threadActor,
          consoleActor: this._consoleActor,
          url: this._dbg.url,
        };
      } catch (error) {
        return { error: error.toString() };
      }
    },

    _onWorkerClose() {
      this.destroy();
    },

    _onWorkerError(filename, lineno, message) {
      reportError("ERROR:" + filename + ":" + lineno + ":" + message + "\n");
    },

    _getServiceWorkerRegistrationInfo() {
      return swm.getRegistrationByPrincipal(this._dbg.principal, this._dbg.url);
    },

    _detach() {
      if (this._threadActor !== null) {
        this._transport.close();
        this._transport = null;
        this._threadActor = null;
      }

      this._dbg.removeListener(this._dbgListener);
      this._attached = false;
    },
  }
);

exports.WorkerDescriptorActor = WorkerDescriptorActor;
