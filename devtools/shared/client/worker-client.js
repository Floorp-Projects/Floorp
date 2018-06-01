/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {DebuggerClient} = require("devtools/shared/client/debugger-client");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const eventSource = require("devtools/shared/client/event-source");
loader.lazyRequireGetter(this, "ThreadClient", "devtools/shared/client/thread-client");

const noop = () => {};

function WorkerClient(client, form) {
  this.client = client;
  this._actor = form.from;
  this._isClosed = false;
  this._url = form.url;

  this._onClose = this._onClose.bind(this);

  this.addListener("close", this._onClose);

  this.traits = {};
}

WorkerClient.prototype = {
  get _transport() {
    return this.client._transport;
  },

  get request() {
    return this.client.request;
  },

  get actor() {
    return this._actor;
  },

  get url() {
    return this._url;
  },

  get isClosed() {
    return this._isClosed;
  },

  detach: DebuggerClient.requester({ type: "detach" }, {
    after: function(response) {
      if (this.thread) {
        this.client.unregisterClient(this.thread);
      }
      this.client.unregisterClient(this);
      return response;
    },
  }),

  attachThread: function(options = {}, onResponse = noop) {
    if (this.thread) {
      const response = [{
        type: "connected",
        threadActor: this.thread._actor,
        consoleActor: this.consoleActor,
      }, this.thread];
      DevToolsUtils.executeSoon(() => onResponse(response));
      return response;
    }

    // The connect call on server doesn't attach the thread as of version 44.
    return this.request({
      to: this._actor,
      type: "connect",
      options,
    }).then(connectResponse => {
      if (connectResponse.error) {
        onResponse(connectResponse, null);
        return [connectResponse, null];
      }

      return this.request({
        to: connectResponse.threadActor,
        type: "attach",
        options,
      }).then(attachResponse => {
        if (attachResponse.error) {
          onResponse(attachResponse, null);
        }

        this.thread = new ThreadClient(this, connectResponse.threadActor);
        this.consoleActor = connectResponse.consoleActor;
        this.client.registerClient(this.thread);

        onResponse(connectResponse, this.thread);
        return [connectResponse, this.thread];
      });
    }, error => {
      onResponse(error, null);
    });
  },

  _onClose: function() {
    this.removeListener("close", this._onClose);

    if (this.thread) {
      this.client.unregisterClient(this.thread);
    }
    this.client.unregisterClient(this);
    this._isClosed = true;
  },

  reconfigure: function() {
    return Promise.resolve();
  },

  events: ["close"]
};

eventSource(WorkerClient.prototype);

module.exports = WorkerClient;
