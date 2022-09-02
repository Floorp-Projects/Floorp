/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Ci } = require("chrome");
const {
  workerDescriptorSpec,
} = require("devtools/shared/specs/descriptors/worker");
const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const { TargetMixin } = require("devtools/client/fronts/targets/target-mixin");
const {
  DescriptorMixin,
} = require("devtools/client/fronts/descriptors/descriptor-mixin");
const DESCRIPTOR_TYPES = require("devtools/client/fronts/descriptors/descriptor-types");

class WorkerDescriptorFront extends DescriptorMixin(
  TargetMixin(FrontClassWithSpec(workerDescriptorSpec))
) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    this.traits = {};
  }

  descriptorType = DESCRIPTOR_TYPES.WORKER;

  form(json) {
    this.actorID = json.actor;
    this.id = json.id;

    // Save the full form for Target class usage.
    // Do not use `form` name to avoid colliding with protocol.js's `form` method
    this.targetForm = json;
    this._url = json.url;
    this.type = json.type;
    this.scope = json.scope;
    this.fetch = json.fetch;
    this.traits = json.traits;
  }

  get name() {
    // this._url is nullified in TargetMixin#destroy.
    if (!this.url) {
      return null;
    }

    return this.url.split("/").pop();
  }

  get isWorkerDescriptor() {
    return true;
  }

  get isDedicatedWorker() {
    return this.type === Ci.nsIWorkerDebugger.TYPE_DEDICATED;
  }

  get isSharedWorker() {
    return this.type === Ci.nsIWorkerDebugger.TYPE_SHARED;
  }

  get isServiceWorker() {
    return this.type === Ci.nsIWorkerDebugger.TYPE_SERVICE;
  }

  // For now, WorkerDescriptor is morphed into a WorkerTarget when calling this method.
  // Ideally, we would split this into two distinct classes.
  async morphWorkerDescriptorIntoWorkerTarget() {
    // temporary, will be moved once we have a target actor
    return this.getTarget();
  }

  async getTarget() {
    if (this._attach) {
      return this._attach;
    }

    this._attach = (async () => {
      if (this.isDestroyedOrBeingDestroyed()) {
        return this;
      }

      if (this.isServiceWorker) {
        this.registration = await this._getRegistrationIfActive();
        if (this.registration) {
          await this.registration.preventShutdown();
        }
      }

      if (this.isDestroyedOrBeingDestroyed()) {
        return this;
      }

      const workerTargetForm = await super.getTarget();

      // Set the console and thread actor IDs on the form so it is accessible by TargetMixin.getFront
      this.targetForm.consoleActor = workerTargetForm.consoleActor;
      this.targetForm.threadActor = workerTargetForm.threadActor;

      if (this.isDestroyedOrBeingDestroyed()) {
        return this;
      }

      return this;
    })();
    return this._attach;
  }

  async detach() {
    try {
      await super.detach();

      if (this.registration) {
        // Bug 1644772 - Sometimes, the Browser Toolbox fails opening with a connection timeout
        // with an exception related to this call to allowShutdown and its usage of detachDebugger API.
        await this.registration.allowShutdown();
        this.registration = null;
      }
    } catch (e) {
      this.logDetachError(e, "worker");
    }
  }

  async _getRegistrationIfActive() {
    const {
      registrations,
    } = await this.client.mainRoot.listServiceWorkerRegistrations();
    return registrations.find(({ activeWorker }) => {
      return activeWorker && this.id === activeWorker.id;
    });
  }

  reconfigure() {
    // Toolbox and options panel are calling this method but Worker Target can't be
    // reconfigured. So we ignore this call here.
    return Promise.resolve();
  }
}

exports.WorkerDescriptorFront = WorkerDescriptorFront;
registerFront(exports.WorkerDescriptorFront);
