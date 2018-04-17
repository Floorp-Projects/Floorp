/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const promise = require("devtools/shared/deprecated-sync-thenables");

const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const eventSource = require("devtools/shared/client/event-source");
const {arg, DebuggerClient} = require("devtools/shared/client/debugger-client");
loader.lazyRequireGetter(this, "ThreadClient", "devtools/shared/client/thread-client");

const noop = () => {};

/**
 * Creates a tab client for the remote debugging protocol server. This client
 * is a front to the tab actor created in the server side, hiding the protocol
 * details in a traditional JavaScript API.
 *
 * @param client DebuggerClient
 *        The debugger client parent.
 * @param form object
 *        The protocol form for this tab.
 */
function TabClient(client, form) {
  this.client = client;
  this._actor = form.from;
  this._threadActor = form.threadActor;
  this.javascriptEnabled = form.javascriptEnabled;
  this.cacheDisabled = form.cacheDisabled;
  this.thread = null;
  this.request = this.client.request;
  this.traits = form.traits || {};
  this.events = ["workerListChanged"];
}

TabClient.prototype = {
  get actor() {
    return this._actor;
  },
  get _transport() {
    return this.client._transport;
  },

  /**
   * Attach to a thread actor.
   *
   * @param object options
   *        Configuration options.
   *        - useSourceMaps: whether to use source maps or not.
   * @param function onResponse
   *        Called with the response packet and a ThreadClient
   *        (which will be undefined on error).
   */
  attachThread: function(options = {}, onResponse = noop) {
    if (this.thread) {
      DevToolsUtils.executeSoon(() => onResponse({}, this.thread));
      return promise.resolve([{}, this.thread]);
    }

    let packet = {
      to: this._threadActor,
      type: "attach",
      options,
    };
    return this.request(packet).then(response => {
      if (!response.error) {
        this.thread = new ThreadClient(this, this._threadActor);
        this.client.registerClient(this.thread);
      }
      onResponse(response, this.thread);
      return [response, this.thread];
    });
  },

  /**
   * Detach the client from the tab actor.
   *
   * @param function onResponse
   *        Called with the response packet.
   */
  detach: DebuggerClient.requester({
    type: "detach"
  }, {
    before: function(packet) {
      if (this.thread) {
        this.thread.detach();
      }
      return packet;
    },
    after: function(response) {
      this.client.unregisterClient(this);
      return response;
    },
  }),

  /**
   * Bring the window to the front.
   */
  focus: DebuggerClient.requester({
    type: "focus"
  }, {}),

  /**
   * Ensure relevant pages have error reporting enabled.
   */
  ensureCSSErrorReportingEnabled: DebuggerClient.requester({
    type: "ensureCSSErrorReportingEnabled",
  }, {}),

  /**
   * Reload the page in this tab.
   *
   * @param [optional] object options
   *        An object with a `force` property indicating whether or not
   *        this reload should skip the cache
   */
  reload: function(options = { force: false }) {
    return this._reload(options);
  },
  _reload: DebuggerClient.requester({
    type: "reload",
    options: arg(0)
  }),

  /**
   * Navigate to another URL.
   *
   * @param string url
   *        The URL to navigate to.
   */
  navigateTo: DebuggerClient.requester({
    type: "navigateTo",
    url: arg(0)
  }),

  /**
   * Reconfigure the tab actor.
   *
   * @param object options
   *        A dictionary object of the new options to use in the tab actor.
   * @param function onResponse
   *        Called with the response packet.
   */
  reconfigure: DebuggerClient.requester({
    type: "reconfigure",
    options: arg(0)
  }),

  listWorkers: DebuggerClient.requester({
    type: "listWorkers"
  }),

  attachWorker: function(workerActor, onResponse) {
    return this.client.attachWorker(workerActor, onResponse);
  },
};

eventSource(TabClient.prototype);

module.exports = TabClient;
