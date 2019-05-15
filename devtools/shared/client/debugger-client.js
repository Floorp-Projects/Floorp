/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const promise = require("devtools/shared/deprecated-sync-thenables");

const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const { getStack, callFunctionWithAsyncStack } = require("devtools/shared/platform/stack");
const eventSource = require("devtools/shared/client/event-source");
const {
  ThreadStateTypes,
  UnsolicitedNotifications,
  UnsolicitedPauses,
} = require("./constants");

loader.lazyRequireGetter(this, "Authentication", "devtools/shared/security/auth");
loader.lazyRequireGetter(this, "DebuggerSocket", "devtools/shared/security/socket", true);
loader.lazyRequireGetter(this, "EventEmitter", "devtools/shared/event-emitter");

loader.lazyRequireGetter(this, "RootFront", "devtools/shared/fronts/root", true);
loader.lazyRequireGetter(this, "ThreadClient", "devtools/shared/client/thread-client");
loader.lazyRequireGetter(this, "ObjectClient", "devtools/shared/client/object-client");
loader.lazyRequireGetter(this, "Front", "devtools/shared/protocol", true);

/**
 * Creates a client for the remote debugging protocol server. This client
 * provides the means to communicate with the server and exchange the messages
 * required by the protocol in a traditional JavaScript API.
 */
function DebuggerClient(transport) {
  this._transport = transport;
  this._transport.hooks = this;

  // Map actor ID to client instance for each actor type.
  // To be removed once all clients are refactored to protocol.js
  this._clients = new Map();

  this._pendingRequests = new Map();
  this._activeRequests = new Map();
  this._eventsEnabled = true;

  this.traits = {};

  this.request = this.request.bind(this);
  this.localTransport = this._transport.onOutputStreamReady === undefined;

  /*
   * As the first thing on the connection, expect a greeting packet from
   * the connection's root actor.
   */
  this.mainRoot = null;
  this.expectReply("root", (packet) => {
    this.mainRoot = new RootFront(this, packet);

    // Root Front is a special case, managing itself as it doesn't have any parent.
    // It will register itself to DebuggerClient as a Pool via Front._poolMap.
    this.mainRoot.manage(this.mainRoot);

    this.emit("connected", packet.applicationType, packet.traits);
  });
}

/**
 * A declarative helper for defining methods that send requests to the server.
 *
 * @param packetSkeleton
 *        The form of the packet to send. Can specify fields to be filled from
 *        the parameters by using the |arg| function.
 * @param before
 *        The function to call before sending the packet. Is passed the packet,
 *        and the return value is used as the new packet. The |this| context is
 *        the instance of the client object we are defining a method for.
 * @param after
 *        The function to call after the response is received. It is passed the
 *        response, and the return value is considered the new response that
 *        will be passed to the callback. The |this| context is the instance of
 *        the client object we are defining a method for.
 * @return Request
 *         The `Request` object that is a Promise object and resolves once
 *         we receive the response. (See request method for more details)
 */
DebuggerClient.requester = function(packetSkeleton, config = {}) {
  const { before, after } = config;
  return DevToolsUtils.makeInfallible(function(...args) {
    let outgoingPacket = {
      to: packetSkeleton.to || this.actor,
    };

    let maxPosition = -1;
    for (const k of Object.keys(packetSkeleton)) {
      if (packetSkeleton[k] instanceof DebuggerClient.Argument) {
        const { position } = packetSkeleton[k];
        outgoingPacket[k] = packetSkeleton[k].getArgument(args);
        maxPosition = Math.max(position, maxPosition);
      } else {
        outgoingPacket[k] = packetSkeleton[k];
      }
    }

    if (before) {
      outgoingPacket = before.call(this, outgoingPacket);
    }

    return this.request(outgoingPacket, DevToolsUtils.makeInfallible((response) => {
      if (after) {
        const { from } = response;
        response = after.call(this, response);
        if (!response.from) {
          response.from = from;
        }
      }

      // The callback is always the last parameter.
      const thisCallback = args[maxPosition + 1];
      if (thisCallback) {
        thisCallback(response);
      }
      return response;
    }, "DebuggerClient.requester request callback"));
  }, "DebuggerClient.requester");
};

function arg(pos) {
  return new DebuggerClient.Argument(pos);
}
exports.arg = arg;

DebuggerClient.Argument = function(position) {
  this.position = position;
};

DebuggerClient.Argument.prototype.getArgument = function(params) {
  if (!(this.position in params)) {
    throw new Error("Bad index into params: " + this.position);
  }
  return params[this.position];
};

// Expose these to save callers the trouble of importing DebuggerSocket
DebuggerClient.socketConnect = function(options) {
  // Defined here instead of just copying the function to allow lazy-load
  return DebuggerSocket.connect(options);
};
DevToolsUtils.defineLazyGetter(DebuggerClient, "Authenticators", () => {
  return Authentication.Authenticators;
});
DevToolsUtils.defineLazyGetter(DebuggerClient, "AuthenticationResult", () => {
  return Authentication.AuthenticationResult;
});

DebuggerClient.prototype = {
  /**
   * Connect to the server and start exchanging protocol messages.
   *
   * @param onConnected function
   *        If specified, will be called when the greeting packet is
   *        received from the debugging server.
   *
   * @return Promise
   *         Resolves once connected with an array whose first element
   *         is the application type, by default "browser", and the second
   *         element is the traits object (help figure out the features
   *         and behaviors of the server we connect to. See RootActor).
   */
  connect: function(onConnected) {
    const deferred = promise.defer();

    this.addOneTimeListener("connected", (name, applicationType, traits) => {
      this.traits = traits;
      if (onConnected) {
        onConnected(applicationType, traits);
      }
      deferred.resolve([applicationType, traits]);
    });

    this._transport.ready();
    return deferred.promise;
  },

  /**
   * Shut down communication with the debugging server.
   *
   * @param onClosed function
   *        If specified, will be called when the debugging connection
   *        has been closed. This parameter is deprecated - please use
   *        the returned Promise.
   * @return Promise
   *         Resolves after the underlying transport is closed.
   */
  close: function(onClosed) {
    const deferred = promise.defer();
    if (onClosed) {
      deferred.promise.then(onClosed);
    }

    // Disable detach event notifications, because event handlers will be in a
    // cleared scope by the time they run.
    this._eventsEnabled = false;

    const cleanup = () => {
      if (this._transport) {
        this._transport.close();
      }
      this._transport = null;
    };

    // If the connection is already closed,
    // there is no need to detach client
    // as we won't be able to send any message.
    if (this._closed) {
      cleanup();
      deferred.resolve();
      return deferred.promise;
    }

    this.addOneTimeListener("closed", deferred.resolve);

    // Call each client's `detach` method by calling
    // lastly registered ones first to give a chance
    // to detach child clients first.
    const clients = [...this._clients.values()];
    this._clients.clear();
    const detachClients = () => {
      const client = clients.pop();
      if (!client) {
        // All clients detached.
        cleanup();
        return;
      }
      if (client.detach) {
        client.detach(detachClients);
        return;
      }
      detachClients();
    };
    detachClients();

    return deferred.promise;
  },

  /**
   * Attach to a global-scoped thread actor for chrome debugging.
   *
   * @param string threadActor
   *        The actor ID for the thread to attach.
   * @param object options
   *        Configuration options.
   */
  attachThread: function(threadActor, options = {}) {
    if (this._clients.has(threadActor)) {
      const client = this._clients.get(threadActor);
      return promise.resolve([{}, client]);
    }

    const packet = {
      to: threadActor,
      type: "attach",
      options,
    };
    return this.request(packet).then(response => {
      const threadClient = new ThreadClient(this, threadActor);
      this.registerClient(threadClient);
      return [response, threadClient];
    });
  },

  /**
   * Release an object actor.
   *
   * @param string actor
   *        The actor ID to send the request to.
   */
  release: DebuggerClient.requester({
    to: arg(0),
    type: "release",
  }),

  /**
   * Send a request to the debugging server.
   *
   * @param packet object
   *        A JSON packet to send to the debugging server.
   * @param onResponse function
   *        If specified, will be called with the JSON response packet when
   *        debugging server responds.
   * @return Request
   *         This object emits a number of events to allow you to respond to
   *         different parts of the request lifecycle.
   *         It is also a Promise object, with a `then` method, that is resolved
   *         whenever a JSON or a Bulk response is received; and is rejected
   *         if the response is an error.
   *         Note: This return value can be ignored if you are using JSON alone,
   *         because the callback provided in |onResponse| will be bound to the
   *         "json-reply" event automatically.
   *
   *         Events emitted:
   *         * json-reply: The server replied with a JSON packet, which is
   *           passed as event data.
   *         * bulk-reply: The server replied with bulk data, which you can read
   *           using the event data object containing:
   *           * actor:  Name of actor that received the packet
   *           * type:   Name of actor's method that was called on receipt
   *           * length: Size of the data to be read
   *           * stream: This input stream should only be used directly if you
   *                     can ensure that you will read exactly |length| bytes
   *                     and will not close the stream when reading is complete
   *           * done:   If you use the stream directly (instead of |copyTo|
   *                     below), you must signal completion by resolving /
   *                     rejecting this deferred.  If it's rejected, the
   *                     transport will be closed.  If an Error is supplied as a
   *                     rejection value, it will be logged via |dumpn|.  If you
   *                     do use |copyTo|, resolving is taken care of for you
   *                     when copying completes.
   *           * copyTo: A helper function for getting your data out of the
   *                     stream that meets the stream handling requirements
   *                     above, and has the following signature:
   *             @param  output nsIAsyncOutputStream
   *                     The stream to copy to.
   *             @return Promise
   *                     The promise is resolved when copying completes or
   *                     rejected if any (unexpected) errors occur.
   *                     This object also emits "progress" events for each chunk
   *                     that is copied.  See stream-utils.js.
   */
  request: function(packet, onResponse) {
    if (!this.mainRoot) {
      throw Error("Have not yet received a hello packet from the server.");
    }
    const type = packet.type || "";
    if (!packet.to) {
      throw Error("'" + type + "' request packet has no destination.");
    }

    // The onResponse callback might modify the response, so we need to call
    // it and resolve the promise with its result if it's truthy.
    const safeOnResponse = response => {
      if (!onResponse) {
        return response;
      }
      return onResponse(response) || response;
    };

    if (this._closed) {
      const msg = "'" + type + "' request packet to " +
                "'" + packet.to + "' " +
               "can't be sent as the connection is closed.";
      const resp = { error: "connectionClosed", message: msg };
      return promise.reject(safeOnResponse(resp));
    }

    const request = new Request(packet);
    request.format = "json";
    request.stack = getStack();

    // Implement a Promise like API on the returned object
    // that resolves/rejects on request response
    const deferred = promise.defer();
    function listenerJson(resp) {
      removeRequestListeners();
      resp = safeOnResponse(resp);
      if (resp.error) {
        deferred.reject(resp);
      } else {
        deferred.resolve(resp);
      }
    }
    function listenerBulk(resp) {
      removeRequestListeners();
      deferred.resolve(safeOnResponse(resp));
    }

    const removeRequestListeners = () => {
      request.off("json-reply", listenerJson);
      request.off("bulk-reply", listenerBulk);
    };

    request.on("json-reply", listenerJson);
    request.on("bulk-reply", listenerBulk);

    this._sendOrQueueRequest(request);
    request.then = deferred.promise.then.bind(deferred.promise);

    return request;
  },

  /**
   * Transmit streaming data via a bulk request.
   *
   * This method initiates the bulk send process by queuing up the header data.
   * The caller receives eventual access to a stream for writing.
   *
   * Since this opens up more options for how the server might respond (it could
   * send back either JSON or bulk data), and the returned Request object emits
   * events for different stages of the request process that you may want to
   * react to.
   *
   * @param request Object
   *        This is modeled after the format of JSON packets above, but does not
   *        actually contain the data, but is instead just a routing header:
   *          * actor:  Name of actor that will receive the packet
   *          * type:   Name of actor's method that should be called on receipt
   *          * length: Size of the data to be sent
   * @return Request
   *         This object emits a number of events to allow you to respond to
   *         different parts of the request lifecycle.
   *
   *         Events emitted:
   *         * bulk-send-ready: Ready to send bulk data to the server, using the
   *           event data object containing:
   *           * stream:   This output stream should only be used directly if
   *                       you can ensure that you will write exactly |length|
   *                       bytes and will not close the stream when writing is
   *                       complete
   *           * done:     If you use the stream directly (instead of |copyFrom|
   *                       below), you must signal completion by resolving /
   *                       rejecting this deferred.  If it's rejected, the
   *                       transport will be closed.  If an Error is supplied as
   *                       a rejection value, it will be logged via |dumpn|.  If
   *                       you do use |copyFrom|, resolving is taken care of for
   *                       you when copying completes.
   *           * copyFrom: A helper function for getting your data onto the
   *                       stream that meets the stream handling requirements
   *                       above, and has the following signature:
   *             @param  input nsIAsyncInputStream
   *                     The stream to copy from.
   *             @return Promise
   *                     The promise is resolved when copying completes or
   *                     rejected if any (unexpected) errors occur.
   *                     This object also emits "progress" events for each chunk
   *                     that is copied.  See stream-utils.js.
   *         * json-reply: The server replied with a JSON packet, which is
   *           passed as event data.
   *         * bulk-reply: The server replied with bulk data, which you can read
   *           using the event data object containing:
   *           * actor:  Name of actor that received the packet
   *           * type:   Name of actor's method that was called on receipt
   *           * length: Size of the data to be read
   *           * stream: This input stream should only be used directly if you
   *                     can ensure that you will read exactly |length| bytes
   *                     and will not close the stream when reading is complete
   *           * done:   If you use the stream directly (instead of |copyTo|
   *                     below), you must signal completion by resolving /
   *                     rejecting this deferred.  If it's rejected, the
   *                     transport will be closed.  If an Error is supplied as a
   *                     rejection value, it will be logged via |dumpn|.  If you
   *                     do use |copyTo|, resolving is taken care of for you
   *                     when copying completes.
   *           * copyTo: A helper function for getting your data out of the
   *                     stream that meets the stream handling requirements
   *                     above, and has the following signature:
   *             @param  output nsIAsyncOutputStream
   *                     The stream to copy to.
   *             @return Promise
   *                     The promise is resolved when copying completes or
   *                     rejected if any (unexpected) errors occur.
   *                     This object also emits "progress" events for each chunk
   *                     that is copied.  See stream-utils.js.
   */
  startBulkRequest: function(request) {
    if (!this.traits.bulk) {
      throw Error("Server doesn't support bulk transfers");
    }
    if (!this.mainRoot) {
      throw Error("Have not yet received a hello packet from the server.");
    }
    if (!request.type) {
      throw Error("Bulk packet is missing the required 'type' field.");
    }
    if (!request.actor) {
      throw Error("'" + request.type + "' bulk packet has no destination.");
    }
    if (!request.length) {
      throw Error("'" + request.type + "' bulk packet has no length.");
    }

    request = new Request(request);
    request.format = "bulk";

    this._sendOrQueueRequest(request);

    return request;
  },

  /**
   * If a new request can be sent immediately, do so.  Otherwise, queue it.
   */
  _sendOrQueueRequest(request) {
    const actor = request.actor;
    if (!this._activeRequests.has(actor)) {
      this._sendRequest(request);
    } else {
      this._queueRequest(request);
    }
  },

  /**
   * Send a request.
   * @throws Error if there is already an active request in flight for the same
   *         actor.
   */
  _sendRequest(request) {
    const actor = request.actor;
    this.expectReply(actor, request);

    if (request.format === "json") {
      this._transport.send(request.request);
      return;
    }

    this._transport.startBulkSend(request.request).then((...args) => {
      request.emit("bulk-send-ready", ...args);
    });
  },

  /**
   * Queue a request to be sent later.  Queues are only drained when an in
   * flight request to a given actor completes.
   */
  _queueRequest(request) {
    const actor = request.actor;
    const queue = this._pendingRequests.get(actor) || [];
    queue.push(request);
    this._pendingRequests.set(actor, queue);
  },

  /**
   * Attempt the next request to a given actor (if any).
   */
  _attemptNextRequest(actor) {
    if (this._activeRequests.has(actor)) {
      return;
    }
    const queue = this._pendingRequests.get(actor);
    if (!queue) {
      return;
    }
    const request = queue.shift();
    if (queue.length === 0) {
      this._pendingRequests.delete(actor);
    }
    this._sendRequest(request);
  },

  /**
   * Arrange to hand the next reply from |actor| to the handler bound to
   * |request|.
   *
   * DebuggerClient.prototype.request / startBulkRequest usually takes care of
   * establishing the handler for a given request, but in rare cases (well,
   * greetings from new root actors, is the only case at the moment) we must be
   * prepared for a "reply" that doesn't correspond to any request we sent.
   */
  expectReply: function(actor, request) {
    if (this._activeRequests.has(actor)) {
      throw Error("clashing handlers for next reply from " + actor);
    }

    // If a handler is passed directly (as it is with the handler for the root
    // actor greeting), create a dummy request to bind this to.
    if (typeof request === "function") {
      const handler = request;
      request = new Request();
      request.on("json-reply", handler);
    }

    this._activeRequests.set(actor, request);
  },

  // Transport hooks.

  /**
   * Called by DebuggerTransport to dispatch incoming packets as appropriate.
   *
   * @param packet object
   *        The incoming packet.
   */
  onPacket: function(packet) {
    if (!packet.from) {
      DevToolsUtils.reportException(
        "onPacket",
        new Error("Server did not specify an actor, dropping packet: " +
                  JSON.stringify(packet)));
      return;
    }

    // Check for "forwardingCancelled" here instead of using a front to handle it.
    // This is necessary because we might receive this event while the client is closing,
    // and the fronts have already been removed by that point.
    if (this.mainRoot &&
        packet.from == this.mainRoot.actorID &&
        packet.type == "forwardingCancelled") {
      this.purgeRequests(packet.prefix);
      return;
    }

    // If we have a registered Front for this actor, let it handle the packet
    // and skip all the rest of this unpleasantness.
    const front = this.getActor(packet.from);
    if (front) {
      front.onPacket(packet);
      return;
    }

    if (this._clients.has(packet.from) && packet.type) {
      const client = this._clients.get(packet.from);
      const type = packet.type;
      if (client.events.includes(type)) {
        client.emit(type, packet);
        // we ignore the rest, as the client is expected to handle this packet.
        return;
      }
    }

    let activeRequest;
    // See if we have a handler function waiting for a reply from this
    // actor. (Don't count unsolicited notifications or pauses as
    // replies.)
    if (this._activeRequests.has(packet.from) &&
        !(packet.type in UnsolicitedNotifications) &&
        !(packet.type == ThreadStateTypes.paused &&
          packet.why.type in UnsolicitedPauses)) {
      activeRequest = this._activeRequests.get(packet.from);
      this._activeRequests.delete(packet.from);
    }

    // If there is a subsequent request for the same actor, hand it off to the
    // transport.  Delivery of packets on the other end is always async, even
    // in the local transport case.
    this._attemptNextRequest(packet.from);

    // Packets that indicate thread state changes get special treatment.
    if (packet.type in ThreadStateTypes &&
        this._clients.has(packet.from) &&
        typeof this._clients.get(packet.from)._onThreadState == "function") {
      this._clients.get(packet.from)._onThreadState(packet);
    }

    // Only try to notify listeners on events, not responses to requests
    // that lack a packet type.
    if (packet.type) {
      this.emit(packet.type, packet);
    }

    if (activeRequest) {
      const emitReply = () => activeRequest.emit("json-reply", packet);
      if (activeRequest.stack) {
        callFunctionWithAsyncStack(emitReply, activeRequest.stack,
                                   "DevTools RDP");
      } else {
        emitReply();
      }
    }
  },

  /**
   * Called by the DebuggerTransport to dispatch incoming bulk packets as
   * appropriate.
   *
   * @param packet object
   *        The incoming packet, which contains:
   *        * actor:  Name of actor that will receive the packet
   *        * type:   Name of actor's method that should be called on receipt
   *        * length: Size of the data to be read
   *        * stream: This input stream should only be used directly if you can
   *                  ensure that you will read exactly |length| bytes and will
   *                  not close the stream when reading is complete
   *        * done:   If you use the stream directly (instead of |copyTo|
   *                  below), you must signal completion by resolving /
   *                  rejecting this deferred.  If it's rejected, the transport
   *                  will be closed.  If an Error is supplied as a rejection
   *                  value, it will be logged via |dumpn|.  If you do use
   *                  |copyTo|, resolving is taken care of for you when copying
   *                  completes.
   *        * copyTo: A helper function for getting your data out of the stream
   *                  that meets the stream handling requirements above, and has
   *                  the following signature:
   *          @param  output nsIAsyncOutputStream
   *                  The stream to copy to.
   *          @return Promise
   *                  The promise is resolved when copying completes or rejected
   *                  if any (unexpected) errors occur.
   *                  This object also emits "progress" events for each chunk
   *                  that is copied.  See stream-utils.js.
   */
  onBulkPacket: function(packet) {
    const { actor } = packet;

    if (!actor) {
      DevToolsUtils.reportException(
        "onBulkPacket",
        new Error("Server did not specify an actor, dropping bulk packet: " +
                  JSON.stringify(packet)));
      return;
    }

    // See if we have a handler function waiting for a reply from this
    // actor.
    if (!this._activeRequests.has(actor)) {
      return;
    }

    const activeRequest = this._activeRequests.get(actor);
    this._activeRequests.delete(actor);

    // If there is a subsequent request for the same actor, hand it off to the
    // transport.  Delivery of packets on the other end is always async, even
    // in the local transport case.
    this._attemptNextRequest(actor);

    activeRequest.emit("bulk-reply", packet);
  },

  /**
   * Called by DebuggerTransport when the underlying stream is closed.
   *
   * @param status nsresult
   *        The status code that corresponds to the reason for closing
   *        the stream.
   */
  onClosed: function() {
    if (this._closed) {
      return;
    }
    this._closed = true;
    this.emit("closed");

    this.purgeRequests();

    // The |_pools| array on the client-side currently is used only by
    // protocol.js to store active fronts, mirroring the actor pools found in
    // the server.  So, read all usages of "pool" as "protocol.js front".
    //
    // In the normal case where we shutdown cleanly, the toolbox tells each tool
    // to close, and they each call |destroy| on any fronts they were using.
    // When |destroy| or |cleanup| is called on a protocol.js front, it also
    // removes itself from the |_pools| array.  Once the toolbox has shutdown,
    // the connection is closed, and we reach here.  All fronts (should have
    // been) |destroy|ed, so |_pools| should empty.
    //
    // If the connection instead aborts unexpectedly, we may end up here with
    // all fronts used during the life of the connection.  So, we call |cleanup|
    // on them clear their state, reject pending requests, and remove themselves
    // from |_pools|.  This saves the toolbox from hanging indefinitely, in case
    // it waits for some server response before shutdown that will now never
    // arrive.
    for (const pool of this._pools) {
      pool.cleanup();
    }
  },

  /**
   * Purge pending and active requests in this client.
   *
   * @param prefix string (optional)
   *        If a prefix is given, only requests for actor IDs that start with the prefix
   *        will be cleaned up.  This is useful when forwarding of a portion of requests
   *        is cancelled on the server.
   */
  purgeRequests(prefix = "") {
    const reject = function(type, request) {
      // Server can send packets on its own and client only pass a callback
      // to expectReply, so that there is no request object.
      let msg;
      if (request.request) {
        msg = "'" + request.request.type + "' " + type + " request packet" +
              " to '" + request.actor + "' " +
              "can't be sent as the connection just closed.";
      } else {
        msg = "server side packet can't be received as the connection just closed.";
      }
      const packet = { error: "connectionClosed", message: msg };
      request.emit("json-reply", packet);
    };

    let pendingRequestsToReject = [];
    this._pendingRequests.forEach((requests, actor) => {
      if (!actor.startsWith(prefix)) {
        return;
      }
      this._pendingRequests.delete(actor);
      pendingRequestsToReject = pendingRequestsToReject.concat(requests);
    });
    pendingRequestsToReject.forEach(request => reject("pending", request));

    let activeRequestsToReject = [];
    this._activeRequests.forEach((request, actor) => {
      if (!actor.startsWith(prefix)) {
        return;
      }
      this._activeRequests.delete(actor);
      activeRequestsToReject = activeRequestsToReject.concat(request);
    });
    activeRequestsToReject.forEach(request => reject("active", request));
  },

  /**
   * Search for all requests in process for this client, including those made via
   * protocol.js and wait all of them to complete.  Since the requests seen when this is
   * first called may in turn trigger more requests, we keep recursing through this
   * function until there is no more activity.
   *
   * This is a fairly heavy weight process, so it's only meant to be used in tests.
   *
   * @return Promise
   *         Resolved when all requests have settled.
   */
  waitForRequestsToSettle() {
    let requests = [];

    // Gather all pending and active requests in this client
    // The request object supports a Promise API for completion (it has .then())
    this._pendingRequests.forEach(requestsForActor => {
      // Each value is an array of pending requests
      requests = requests.concat(requestsForActor);
    });
    this._activeRequests.forEach(requestForActor => {
      // Each value is a single active request
      requests = requests.concat(requestForActor);
    });

    // protocol.js
    // Use a Set because some fronts (like domwalker) seem to have multiple parents.
    const fronts = new Set();
    const poolsToVisit = [...this._pools];

    // With protocol.js, each front can potentially have it's own pools containing child
    // fronts, forming a tree.  Descend through all the pools to locate all child fronts.
    while (poolsToVisit.length) {
      const pool = poolsToVisit.shift();
      // `_pools` contains either Front's or Pool's, we only want to collect Fronts here.
      // Front inherits from Pool which exposes `poolChildren`.
      if (pool instanceof Front) {
        fronts.add(pool);
      }
      for (const child of pool.poolChildren()) {
        poolsToVisit.push(child);
      }
    }

    // For each front, wait for its requests to settle
    for (const front of fronts) {
      if (front.hasRequests()) {
        requests.push(front.waitForRequestsToSettle());
      }
    }

    // Abort early if there are no requests
    if (!requests.length) {
      return Promise.resolve();
    }

    return DevToolsUtils.settleAll(requests).catch(() => {
      // One of the requests might have failed, but ignore that situation here and pipe
      // both success and failure through the same path.  The important part is just that
      // we waited.
    }).then(() => {
      // Repeat, more requests may have started in response to those we just waited for
      return this.waitForRequestsToSettle();
    });
  },

  registerClient: function(client) {
    const actorID = client.actor;
    if (!actorID) {
      throw new Error("DebuggerServer.registerClient expects " +
                      "a client instance with an `actor` attribute.");
    }
    if (!Array.isArray(client.events)) {
      throw new Error("DebuggerServer.registerClient expects " +
                      "a client instance with an `events` attribute " +
                      "that is an array.");
    }
    if (client.events.length > 0 && typeof (client.emit) != "function") {
      throw new Error("DebuggerServer.registerClient expects " +
                      "a client instance with non-empty `events` array to" +
                      "have an `emit` function.");
    }
    if (this._clients.has(actorID)) {
      throw new Error("DebuggerServer.registerClient already registered " +
                      "a client for this actor.");
    }
    this._clients.set(actorID, client);
  },

  unregisterClient: function(client) {
    const actorID = client.actor;
    if (!actorID) {
      throw new Error("DebuggerServer.unregisterClient expects " +
                      "a Client instance with a `actor` attribute.");
    }
    this._clients.delete(actorID);
  },

  /**
   * Actor lifetime management, echos the server's actor pools.
   */
  __pools: null,
  get _pools() {
    if (this.__pools) {
      return this.__pools;
    }
    this.__pools = new Set();
    return this.__pools;
  },

  addActorPool: function(pool) {
    this._pools.add(pool);
  },
  removeActorPool: function(pool) {
    this._pools.delete(pool);
  },
  getActor: function(actorID) {
    const pool = this.poolFor(actorID);
    return pool ? pool.get(actorID) : null;
  },

  poolFor: function(actorID) {
    for (const pool of this._pools) {
      if (pool.has(actorID)) {
        return pool;
      }
    }
    return null;
  },

  /**
   * Currently attached addon.
   */
  activeAddon: null,

  /**
   * Creates an object client for this DebuggerClient and the grip in parameter,
   * @param {Object} grip: The grip to create the ObjectClient for.
   * @returns {ObjectClient}
   */
  createObjectClient: function(grip) {
    return new ObjectClient(this, grip);
  },

  get transport() {
    return this._transport;
  },
};

eventSource(DebuggerClient.prototype);

class Request extends EventEmitter {
  constructor(request) {
    super();
    this.request = request;
  }

  get actor() {
    return this.request.to || this.request.actor;
  }
}

module.exports = {
  arg,
  DebuggerClient,
};
