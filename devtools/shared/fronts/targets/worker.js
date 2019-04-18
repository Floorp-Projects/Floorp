/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {workerTargetSpec} = require("devtools/shared/specs/targets/worker");
const { FrontClassWithSpec, registerFront } = require("devtools/shared/protocol");
const { TargetMixin } = require("./target-mixin");

class WorkerTargetFront extends
  TargetMixin(FrontClassWithSpec(workerTargetSpec)) {
  constructor(client) {
    super(client);

    this.traits = {};

    // The actor sends a "close" event, which is translated to "worker-close" by
    // the specification in order to not conflict with Target's "close" event.
    // This event is similar to tabDetached and means that the worker is destroyed.
    // So that we should destroy the target in order to significate that the target
    // is no longer debuggable.
    this.once("worker-close", this.destroy.bind(this));
  }

  form(json) {
    this.actorID = json.actor;
    // `id` was added in Firefox 68 to the worker target actor. Fallback to the actorID
    // when debugging older clients.
    // Fallback can be removed when Firefox 68 will be in the Release channel.
    this.id = json.id || this.actorID;

    // Save the full form for Target class usage.
    // Do not use `form` name to avoid colliding with protocol.js's `form` method
    this.targetForm = json;
    this._url = json.url;
    this.type = json.type;
    this.scope = json.scope;
    this.fetch = json.fetch;
  }

  async attach() {
    if (this._attach) {
      return this._attach;
    }
    this._attach = (async () => {
      const response = await super.attach();

      this._url = response.url;

      // Immediately call `connect` in other to fetch console and thread actors
      // that will be later used by Target.
      const connectResponse = await this.connect({});
      // Set the console actor ID on the form to expose it to Target.attachConsole
      this.targetForm.consoleActor = connectResponse.consoleActor;
      this._threadActor = connectResponse.threadActor;

      return this.attachConsole();
    })();
    return this._attach;
  }

  reconfigure() {
    // Toolbox and options panel are calling this method but Worker Target can't be
    // reconfigured. So we ignore this call here.
    return Promise.resolve();
  }
}

exports.WorkerTargetFront = WorkerTargetFront;
registerFront(exports.WorkerTargetFront);
