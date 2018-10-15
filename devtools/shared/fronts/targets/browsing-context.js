/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {browsingContextTargetSpec} = require("devtools/shared/specs/targets/browsing-context");
const protocol = require("devtools/shared/protocol");
const {custom} = protocol;

loader.lazyRequireGetter(this, "ThreadClient", "devtools/shared/client/thread-client");

const BrowsingContextFront = protocol.FrontClassWithSpec(browsingContextTargetSpec, {
  initialize: function(client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);

    this.thread = null;

    // TODO: remove once ThreadClient becomes a front
    this.client = client;
  },

  /**
   * Attach to a thread actor.
   *
   * @param object options
   *        Configuration options.
   *        - useSourceMaps: whether to use source maps or not.
   */
  attachThread: function(options = {}) {
    if (this.thread) {
      return Promise.resolve([{}, this.thread]);
    }

    const packet = {
      to: this._threadActor,
      type: "attach",
      options,
    };
    return this.client.request(packet).then(response => {
      this.thread = new ThreadClient(this, this._threadActor);
      this.client.registerClient(this.thread);
      return [response, this.thread];
    });
  },

  attach: custom(async function() {
    const response = await this._attach();

    this._threadActor = response.threadActor;
    this.javascriptEnabled = response.javascriptEnabled;
    this.cacheDisabled = response.cacheDisabled;
    this.traits = response.traits || {};

    return response;
  }, {
    impl: "_attach"
  }),

  detach: custom(async function() {
    let response;
    try {
      response = await this._detach();
    } catch (e) {
      console.warn(
        `Error while detaching the browsing context target front: ${e.message}`);
    }

    if (this.thread) {
      try {
        await this.thread.detach();
      } catch (e) {
        console.warn(`Error while detaching the thread front: ${e.message}`);
      }
    }

    this.destroy();

    return response;
  }, {
    impl: "_detach"
  }),

  attachWorker: function(workerTargetActor) {
    return this.client.attachWorker(workerTargetActor);
  },
});

exports.BrowsingContextFront = BrowsingContextFront;
