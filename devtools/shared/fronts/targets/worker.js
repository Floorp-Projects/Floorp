/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {workerTargetSpec} = require("devtools/shared/specs/targets/worker");
const protocol = require("devtools/shared/protocol");
const {custom} = protocol;

loader.lazyRequireGetter(this, "ThreadClient", "devtools/shared/client/thread-client");

const WorkerTargetFront = protocol.FrontClassWithSpec(workerTargetSpec, {
  initialize: function(client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);

    this.thread = null;
    this.traits = {};

    // TODO: remove once ThreadClient becomes a front
    this.client = client;

    this._isClosed = false;

    this.destroy = this.destroy.bind(this);
    this.on("close", this.destroy);
  },

  get isClosed() {
    return this._isClosed;
  },

  destroy: function() {
    this.off("close", this.destroy);
    this._isClosed = true;

    if (this.thread) {
      this.client.unregisterClient(this.thread);
    }

    this.unmanage(this);

    protocol.Front.prototype.destroy.call(this);
  },

  attach: custom(async function() {
    const response = await this._attach();

    this.url = response.url;

    // Immediately call `connect` in other to fetch console and thread actors
    // that will be later used by Target.
    const connectResponse = await this.connect({});
    this.consoleActor = connectResponse.consoleActor;
    this.threadActor = connectResponse.threadActor;

    return response;
  }, {
    impl: "_attach",
  }),

  detach: custom(async function() {
    if (this.isClosed) {
      return {};
    }
    let response;
    try {
      response = await this._detach();
    } catch (e) {
      console.warn(`Error while detaching the worker target front: ${e.message}`);
    }
    this.destroy();
    return response;
  }, {
    impl: "_detach",
  }),

  reconfigure: function() {
    // Toolbox and options panel are calling this method but Worker Target can't be
    // reconfigured. So we ignore this call here.
    return Promise.resolve();
  },

  attachThread: async function(options = {}) {
    if (this.thread) {
      const response = [{
        type: "connected",
        threadActor: this.thread._actor,
        consoleActor: this.consoleActor,
      }, this.thread];
      return response;
    }

    const attachResponse = await this.client.request({
      to: this.threadActor,
      type: "attach",
      options,
    });
    this.thread = new ThreadClient(this, this.threadActor);
    this.client.registerClient(this.thread);

    return [attachResponse, this.thread];
  },

});

exports.WorkerTargetFront = WorkerTargetFront;
