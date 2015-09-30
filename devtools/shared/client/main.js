/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci, Cu, components } = require("chrome");
const Services = require("Services");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");

const promise = Cu.import("resource://gre/modules/devtools/shared/deprecated-sync-thenables.js", {}).Promise;

loader.lazyRequireGetter(this, "events", "sdk/event/core");
loader.lazyRequireGetter(this, "WebConsoleClient", "devtools/shared/webconsole/client", true);
loader.lazyRequireGetter(this, "DebuggerSocket", "devtools/shared/security/socket", true);
loader.lazyRequireGetter(this, "Authentication", "devtools/shared/security/auth");

const noop = () => {};

/**
 * TODO: Get rid of this API in favor of EventTarget (bug 1042642)
 *
 * Add simple event notification to a prototype object. Any object that has
 * some use for event notifications or the observer pattern in general can be
 * augmented with the necessary facilities by passing its prototype to this
 * function.
 *
 * @param aProto object
 *        The prototype object that will be modified.
 */
function eventSource(aProto) {
  /**
   * Add a listener to the event source for a given event.
   *
   * @param aName string
   *        The event to listen for.
   * @param aListener function
   *        Called when the event is fired. If the same listener
   *        is added more than once, it will be called once per
   *        addListener call.
   */
  aProto.addListener = function (aName, aListener) {
    if (typeof aListener != "function") {
      throw TypeError("Listeners must be functions.");
    }

    if (!this._listeners) {
      this._listeners = {};
    }

    this._getListeners(aName).push(aListener);
  };

  /**
   * Add a listener to the event source for a given event. The
   * listener will be removed after it is called for the first time.
   *
   * @param aName string
   *        The event to listen for.
   * @param aListener function
   *        Called when the event is fired.
   */
  aProto.addOneTimeListener = function (aName, aListener) {
    let l = (...args) => {
      this.removeListener(aName, l);
      aListener.apply(null, args);
    };
    this.addListener(aName, l);
  };

  /**
   * Remove a listener from the event source previously added with
   * addListener().
   *
   * @param aName string
   *        The event name used during addListener to add the listener.
   * @param aListener function
   *        The callback to remove. If addListener was called multiple
   *        times, all instances will be removed.
   */
  aProto.removeListener = function (aName, aListener) {
    if (!this._listeners || !this._listeners[aName]) {
      return;
    }
    this._listeners[aName] =
      this._listeners[aName].filter(function (l) { return l != aListener });
  };

  /**
   * Returns the listeners for the specified event name. If none are defined it
   * initializes an empty list and returns that.
   *
   * @param aName string
   *        The event name.
   */
  aProto._getListeners = function (aName) {
    if (aName in this._listeners) {
      return this._listeners[aName];
    }
    this._listeners[aName] = [];
    return this._listeners[aName];
  };

  /**
   * Notify listeners of an event.
   *
   * @param aName string
   *        The event to fire.
   * @param arguments
   *        All arguments will be passed along to the listeners,
   *        including the name argument.
   */
  aProto.emit = function () {
    if (!this._listeners) {
      return;
    }

    let name = arguments[0];
    let listeners = this._getListeners(name).slice(0);

    for (let listener of listeners) {
      try {
        listener.apply(null, arguments);
      } catch (e) {
        // Prevent a bad listener from interfering with the others.
        DevToolsUtils.reportException("notify event '" + name + "'", e);
      }
    }
  }
}

/**
 * Set of protocol messages that affect thread state, and the
 * state the actor is in after each message.
 */
const ThreadStateTypes = {
  "paused": "paused",
  "resumed": "attached",
  "detached": "detached"
};

/**
 * Set of protocol messages that are sent by the server without a prior request
 * by the client.
 */
const UnsolicitedNotifications = {
  "consoleAPICall": "consoleAPICall",
  "eventNotification": "eventNotification",
  "fileActivity": "fileActivity",
  "lastPrivateContextExited": "lastPrivateContextExited",
  "logMessage": "logMessage",
  "networkEvent": "networkEvent",
  "networkEventUpdate": "networkEventUpdate",
  "newGlobal": "newGlobal",
  "newScript": "newScript",
  "tabDetached": "tabDetached",
  "tabListChanged": "tabListChanged",
  "reflowActivity": "reflowActivity",
  "addonListChanged": "addonListChanged",
  "workerListChanged": "workerListChanged",
  "tabNavigated": "tabNavigated",
  "frameUpdate": "frameUpdate",
  "pageError": "pageError",
  "documentLoad": "documentLoad",
  "enteredFrame": "enteredFrame",
  "exitedFrame": "exitedFrame",
  "appOpen": "appOpen",
  "appClose": "appClose",
  "appInstall": "appInstall",
  "appUninstall": "appUninstall",
  "evaluationResult": "evaluationResult",
};

/**
 * Set of pause types that are sent by the server and not as an immediate
 * response to a client request.
 */
const UnsolicitedPauses = {
  "resumeLimit": "resumeLimit",
  "debuggerStatement": "debuggerStatement",
  "breakpoint": "breakpoint",
  "DOMEvent": "DOMEvent",
  "watchpoint": "watchpoint",
  "exception": "exception"
};

/**
 * Creates a client for the remote debugging protocol server. This client
 * provides the means to communicate with the server and exchange the messages
 * required by the protocol in a traditional JavaScript API.
 */
const DebuggerClient = exports.DebuggerClient = function (aTransport)
{
  this._transport = aTransport;
  this._transport.hooks = this;

  // Map actor ID to client instance for each actor type.
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
  this.expectReply("root", (aPacket) => {
    this.mainRoot = new RootClient(this, aPacket);
    this.emit("connected", aPacket.applicationType, aPacket.traits);
  });
}

/**
 * A declarative helper for defining methods that send requests to the server.
 *
 * @param aPacketSkeleton
 *        The form of the packet to send. Can specify fields to be filled from
 *        the parameters by using the |args| function.
 * @param telemetry
 *        The unique suffix of the telemetry histogram id.
 * @param before
 *        The function to call before sending the packet. Is passed the packet,
 *        and the return value is used as the new packet. The |this| context is
 *        the instance of the client object we are defining a method for.
 * @param after
 *        The function to call after the response is received. It is passed the
 *        response, and the return value is considered the new response that
 *        will be passed to the callback. The |this| context is the instance of
 *        the client object we are defining a method for.
 */
DebuggerClient.requester = function (aPacketSkeleton,
                                     { telemetry, before, after }) {
  return DevToolsUtils.makeInfallible(function (...args) {
    let histogram, startTime;
    if (telemetry) {
      let transportType = this._transport.onOutputStreamReady === undefined
        ? "LOCAL_"
        : "REMOTE_";
      let histogramId = "DEVTOOLS_DEBUGGER_RDP_"
        + transportType + telemetry + "_MS";
      histogram = Services.telemetry.getHistogramById(histogramId);
      startTime = +new Date;
    }
    let outgoingPacket = {
      to: aPacketSkeleton.to || this.actor
    };

    let maxPosition = -1;
    for (let k of Object.keys(aPacketSkeleton)) {
      if (aPacketSkeleton[k] instanceof DebuggerClient.Argument) {
        let { position } = aPacketSkeleton[k];
        outgoingPacket[k] = aPacketSkeleton[k].getArgument(args);
        maxPosition = Math.max(position, maxPosition);
      } else {
        outgoingPacket[k] = aPacketSkeleton[k];
      }
    }

    if (before) {
      outgoingPacket = before.call(this, outgoingPacket);
    }

    this.request(outgoingPacket, DevToolsUtils.makeInfallible((aResponse) => {
      if (after) {
        let { from } = aResponse;
        aResponse = after.call(this, aResponse);
        if (!aResponse.from) {
          aResponse.from = from;
        }
      }

      // The callback is always the last parameter.
      let thisCallback = args[maxPosition + 1];
      if (thisCallback) {
        thisCallback(aResponse);
      }

      if (histogram) {
        histogram.add(+new Date - startTime);
      }
    }, "DebuggerClient.requester request callback"));

  }, "DebuggerClient.requester");
};

function args(aPos) {
  return new DebuggerClient.Argument(aPos);
}

DebuggerClient.Argument = function (aPosition) {
  this.position = aPosition;
};

DebuggerClient.Argument.prototype.getArgument = function (aParams) {
  if (!(this.position in aParams)) {
    throw new Error("Bad index into params: " + this.position);
  }
  return aParams[this.position];
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
   * @param aOnConnected function
   *        If specified, will be called when the greeting packet is
   *        received from the debugging server.
   */
  connect: function (aOnConnected) {
    this.emit("connect");

    // Also emit the event on the |DebuggerServer| object (not on
    // the instance), so it's possible to track all instances.
    events.emit(DebuggerClient, "connect", this);

    this.addOneTimeListener("connected", (aName, aApplicationType, aTraits) => {
      this.traits = aTraits;
      if (aOnConnected) {
        aOnConnected(aApplicationType, aTraits);
      }
    });

    this._transport.ready();
  },

  /**
   * Shut down communication with the debugging server.
   *
   * @param aOnClosed function
   *        If specified, will be called when the debugging connection
   *        has been closed.
   */
  close: function (aOnClosed) {
    // Disable detach event notifications, because event handlers will be in a
    // cleared scope by the time they run.
    this._eventsEnabled = false;

    if (aOnClosed) {
      this.addOneTimeListener('closed', function (aEvent) {
        aOnClosed();
      });
    }

    // Call each client's `detach` method by calling
    // lastly registered ones first to give a chance
    // to detach child clients first.
    let clients = [...this._clients.values()];
    this._clients.clear();
    const detachClients = () => {
      let client = clients.pop();
      if (!client) {
        // All clients detached.
        this._transport.close();
        this._transport = null;
        this._activeRequests.clear();
        this._activeRequests = null;
        this._pendingRequests.clear();
        this._pendingRequests = null;
        return;
      }
      if (client.detach) {
        client.detach(detachClients);
        return;
      }
      detachClients();
    };
    detachClients();
  },

  /*
   * This function exists only to preserve DebuggerClient's interface;
   * new code should say 'client.mainRoot.listTabs()'.
   */
  listTabs: function (aOnResponse) { return this.mainRoot.listTabs(aOnResponse); },

  /*
   * This function exists only to preserve DebuggerClient's interface;
   * new code should say 'client.mainRoot.listAddons()'.
   */
  listAddons: function (aOnResponse) { return this.mainRoot.listAddons(aOnResponse); },

  getTab: function (aFilter) { return this.mainRoot.getTab(aFilter); },

  /**
   * Attach to a tab actor.
   *
   * @param string aTabActor
   *        The actor ID for the tab to attach.
   * @param function aOnResponse
   *        Called with the response packet and a TabClient
   *        (which will be undefined on error).
   */
  attachTab: function (aTabActor, aOnResponse = noop) {
    if (this._clients.has(aTabActor)) {
      let cachedTab = this._clients.get(aTabActor);
      let cachedResponse = {
        cacheDisabled: cachedTab.cacheDisabled,
        javascriptEnabled: cachedTab.javascriptEnabled,
        traits: cachedTab.traits,
      };
      DevToolsUtils.executeSoon(() => aOnResponse(cachedResponse, cachedTab));
      return;
    }

    let packet = {
      to: aTabActor,
      type: "attach"
    };
    this.request(packet, (aResponse) => {
      let tabClient;
      if (!aResponse.error) {
        tabClient = new TabClient(this, aResponse);
        this.registerClient(tabClient);
      }
      aOnResponse(aResponse, tabClient);
    });
  },

  attachWorker: function DC_attachWorker(aWorkerActor, aOnResponse = noop) {
    let workerClient = this._clients.get(aWorkerActor);
    if (workerClient !== undefined) {
      DevToolsUtils.executeSoon(() => aOnResponse({
        from: workerClient.actor,
        type: "attached",
        isFrozen: workerClient.isFrozen,
        url: workerClient.url
      }, workerClient));
      return;
    }

    this.request({ to: aWorkerActor, type: "attach" }, (aResponse) => {
      if (aResponse.error) {
        aOnResponse(aResponse, null);
        return;
      }

      let workerClient = new WorkerClient(this, aResponse);
      this.registerClient(workerClient);
      aOnResponse(aResponse, workerClient);
    });
  },

  /**
   * Attach to an addon actor.
   *
   * @param string aAddonActor
   *        The actor ID for the addon to attach.
   * @param function aOnResponse
   *        Called with the response packet and a AddonClient
   *        (which will be undefined on error).
   */
  attachAddon: function DC_attachAddon(aAddonActor, aOnResponse = noop) {
    let packet = {
      to: aAddonActor,
      type: "attach"
    };
    this.request(packet, aResponse => {
      let addonClient;
      if (!aResponse.error) {
        addonClient = new AddonClient(this, aAddonActor);
        this.registerClient(addonClient);
        this.activeAddon = addonClient;
      }
      aOnResponse(aResponse, addonClient);
    });
  },

  /**
   * Attach to a Web Console actor.
   *
   * @param string aConsoleActor
   *        The ID for the console actor to attach to.
   * @param array aListeners
   *        The console listeners you want to start.
   * @param function aOnResponse
   *        Called with the response packet and a WebConsoleClient
   *        instance (which will be undefined on error).
   */
  attachConsole:
  function (aConsoleActor, aListeners, aOnResponse = noop) {
    let packet = {
      to: aConsoleActor,
      type: "startListeners",
      listeners: aListeners,
    };

    this.request(packet, (aResponse) => {
      let consoleClient;
      if (!aResponse.error) {
        if (this._clients.has(aConsoleActor)) {
          consoleClient = this._clients.get(aConsoleActor);
        } else {
          consoleClient = new WebConsoleClient(this, aResponse);
          this.registerClient(consoleClient);
        }
      }
      aOnResponse(aResponse, consoleClient);
    });
  },

  /**
   * Attach to a global-scoped thread actor for chrome debugging.
   *
   * @param string aThreadActor
   *        The actor ID for the thread to attach.
   * @param function aOnResponse
   *        Called with the response packet and a ThreadClient
   *        (which will be undefined on error).
   * @param object aOptions
   *        Configuration options.
   *        - useSourceMaps: whether to use source maps or not.
   */
  attachThread: function (aThreadActor, aOnResponse = noop, aOptions={}) {
    if (this._clients.has(aThreadActor)) {
      DevToolsUtils.executeSoon(() => aOnResponse({}, this._clients.get(aThreadActor)));
      return;
    }

   let packet = {
      to: aThreadActor,
      type: "attach",
      options: aOptions
    };
    this.request(packet, (aResponse) => {
      if (!aResponse.error) {
        var threadClient = new ThreadClient(this, aThreadActor);
        this.registerClient(threadClient);
      }
      aOnResponse(aResponse, threadClient);
    });
  },

  /**
   * Attach to a trace actor.
   *
   * @param string aTraceActor
   *        The actor ID for the tracer to attach.
   * @param function aOnResponse
   *        Called with the response packet and a TraceClient
   *        (which will be undefined on error).
   */
  attachTracer: function (aTraceActor, aOnResponse = noop) {
    if (this._clients.has(aTraceActor)) {
      DevToolsUtils.executeSoon(() => aOnResponse({}, this._clients.get(aTraceActor)));
      return;
    }

    let packet = {
      to: aTraceActor,
      type: "attach"
    };
    this.request(packet, (aResponse) => {
      if (!aResponse.error) {
        var traceClient = new TraceClient(this, aTraceActor);
        this.registerClient(traceClient);
      }
      aOnResponse(aResponse, traceClient);
    });
  },

  /**
   * Fetch the ChromeActor for the main process or ChildProcessActor for a
   * a given child process ID.
   *
   * @param number aId
   *        The ID for the process to attach (returned by `listProcesses`).
   *        Connected to the main process if omitted, or is 0.
   */
  getProcess: function (aId) {
    let packet = {
      to: "root",
      type: "getProcess"
    }
    if (typeof(aId) == "number") {
      packet.id = aId;
    }
    return this.request(packet);
  },

  /**
   * Release an object actor.
   *
   * @param string aActor
   *        The actor ID to send the request to.
   * @param aOnResponse function
   *        If specified, will be called with the response packet when
   *        debugging server responds.
   */
  release: DebuggerClient.requester({
    to: args(0),
    type: "release"
  }, {
    telemetry: "RELEASE"
  }),

  /**
   * Send a request to the debugging server.
   *
   * @param aRequest object
   *        A JSON packet to send to the debugging server.
   * @param aOnResponse function
   *        If specified, will be called with the JSON response packet when
   *        debugging server responds.
   * @return Request
   *         This object emits a number of events to allow you to respond to
   *         different parts of the request lifecycle.
   *         It is also a Promise object, with a `then` method, that is resolved
   *         whenever a JSON or a Bulk response is received; and is rejected
   *         if the response is an error.
   *         Note: This return value can be ignored if you are using JSON alone,
   *         because the callback provided in |aOnResponse| will be bound to the
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
  request: function (aRequest, aOnResponse) {
    if (!this.mainRoot) {
      throw Error("Have not yet received a hello packet from the server.");
    }
    if (!aRequest.to) {
      let type = aRequest.type || "";
      throw Error("'" + type + "' request packet has no destination.");
    }

    let request = new Request(aRequest);
    request.format = "json";
    request.stack = components.stack;
    if (aOnResponse) {
      request.on("json-reply", aOnResponse);
    }

    this._sendOrQueueRequest(request);

    // Implement a Promise like API on the returned object
    // that resolves/rejects on request response
    let deferred = promise.defer();
    function listenerJson(resp) {
      request.off("json-reply", listenerJson);
      request.off("bulk-reply", listenerBulk);
      if (resp.error) {
        deferred.reject(resp);
      } else {
        deferred.resolve(resp);
      }
    }
    function listenerBulk(resp) {
      request.off("json-reply", listenerJson);
      request.off("bulk-reply", listenerBulk);
      deferred.resolve(resp);
    }
    request.on("json-reply", listenerJson);
    request.on("bulk-reply", listenerBulk);
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
    let actor = request.actor;
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
    let actor = request.actor;
    this.expectReply(actor, request);

    if (request.format === "json") {
      this._transport.send(request.request);
      return false;
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
    let actor = request.actor;
    let queue = this._pendingRequests.get(actor) || [];
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
    let queue = this._pendingRequests.get(actor);
    if (!queue) {
      return;
    }
    let request = queue.shift();
    if (queue.length === 0) {
      this._pendingRequests.delete(actor);
    }
    this._sendRequest(request);
  },

  /**
   * Arrange to hand the next reply from |aActor| to the handler bound to
   * |aRequest|.
   *
   * DebuggerClient.prototype.request / startBulkRequest usually takes care of
   * establishing the handler for a given request, but in rare cases (well,
   * greetings from new root actors, is the only case at the moment) we must be
   * prepared for a "reply" that doesn't correspond to any request we sent.
   */
  expectReply: function (aActor, aRequest) {
    if (this._activeRequests.has(aActor)) {
      throw Error("clashing handlers for next reply from " + uneval(aActor));
    }

    // If a handler is passed directly (as it is with the handler for the root
    // actor greeting), create a dummy request to bind this to.
    if (typeof aRequest === "function") {
      let handler = aRequest;
      aRequest = new Request();
      aRequest.on("json-reply", handler);
    }

    this._activeRequests.set(aActor, aRequest);
  },

  // Transport hooks.

  /**
   * Called by DebuggerTransport to dispatch incoming packets as appropriate.
   *
   * @param aPacket object
   *        The incoming packet.
   */
  onPacket: function (aPacket) {
    if (!aPacket.from) {
      DevToolsUtils.reportException(
        "onPacket",
        new Error("Server did not specify an actor, dropping packet: " +
                  JSON.stringify(aPacket)));
      return;
    }

    // If we have a registered Front for this actor, let it handle the packet
    // and skip all the rest of this unpleasantness.
    let front = this.getActor(aPacket.from);
    if (front) {
      front.onPacket(aPacket);
      return;
    }

    if (this._clients.has(aPacket.from) && aPacket.type) {
      let client = this._clients.get(aPacket.from);
      let type = aPacket.type;
      if (client.events.indexOf(type) != -1) {
        client.emit(type, aPacket);
        // we ignore the rest, as the client is expected to handle this packet.
        return;
      }
    }

    let activeRequest;
    // See if we have a handler function waiting for a reply from this
    // actor. (Don't count unsolicited notifications or pauses as
    // replies.)
    if (this._activeRequests.has(aPacket.from) &&
        !(aPacket.type in UnsolicitedNotifications) &&
        !(aPacket.type == ThreadStateTypes.paused &&
          aPacket.why.type in UnsolicitedPauses)) {
      activeRequest = this._activeRequests.get(aPacket.from);
      this._activeRequests.delete(aPacket.from);
    }

    // If there is a subsequent request for the same actor, hand it off to the
    // transport.  Delivery of packets on the other end is always async, even
    // in the local transport case.
    this._attemptNextRequest(aPacket.from);

    // Packets that indicate thread state changes get special treatment.
    if (aPacket.type in ThreadStateTypes &&
        this._clients.has(aPacket.from) &&
        typeof this._clients.get(aPacket.from)._onThreadState == "function") {
      this._clients.get(aPacket.from)._onThreadState(aPacket);
    }

    // TODO: Bug 1151156 - Remove once Gecko 40 is on b2g-stable.
    if (!this.traits.noNeedToFakeResumptionOnNavigation) {
      // On navigation the server resumes, so the client must resume as well.
      // We achieve that by generating a fake resumption packet that triggers
      // the client's thread state change listeners.
      if (aPacket.type == UnsolicitedNotifications.tabNavigated &&
          this._clients.has(aPacket.from) &&
          this._clients.get(aPacket.from).thread) {
        let thread = this._clients.get(aPacket.from).thread;
        let resumption = { from: thread._actor, type: "resumed" };
        thread._onThreadState(resumption);
      }
    }

    // Only try to notify listeners on events, not responses to requests
    // that lack a packet type.
    if (aPacket.type) {
      this.emit(aPacket.type, aPacket);
    }

    if (activeRequest) {
      let emitReply = () => activeRequest.emit("json-reply", aPacket);
      if (activeRequest.stack) {
        Cu.callFunctionWithAsyncStack(emitReply, activeRequest.stack,
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
    let { actor, type, length } = packet;

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

    let activeRequest = this._activeRequests.get(actor);
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
   * @param aStatus nsresult
   *        The status code that corresponds to the reason for closing
   *        the stream.
   */
  onClosed: function (aStatus) {
    this.emit("closed");

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
    for (let pool of this._pools) {
      pool.cleanup();
    }
  },

  registerClient: function (client) {
    let actorID = client.actor;
    if (!actorID) {
      throw new Error("DebuggerServer.registerClient expects " +
                      "a client instance with an `actor` attribute.");
    }
    if (!Array.isArray(client.events)) {
      throw new Error("DebuggerServer.registerClient expects " +
                      "a client instance with an `events` attribute " +
                      "that is an array.");
    }
    if (client.events.length > 0 && typeof(client.emit) != "function") {
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

  unregisterClient: function (client) {
    let actorID = client.actor;
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

  addActorPool: function (pool) {
    this._pools.add(pool);
  },
  removeActorPool: function (pool) {
    this._pools.delete(pool);
  },
  getActor: function (actorID) {
    let pool = this.poolFor(actorID);
    return pool ? pool.get(actorID) : null;
  },

  poolFor: function (actorID) {
    for (let pool of this._pools) {
      if (pool.has(actorID)) return pool;
    }
    return null;
  },

  /**
   * Currently attached addon.
   */
  activeAddon: null
}

eventSource(DebuggerClient.prototype);

function Request(request) {
  this.request = request;
}

Request.prototype = {

  on: function(type, listener) {
    events.on(this, type, listener);
  },

  off: function(type, listener) {
    events.off(this, type, listener);
  },

  once: function(type, listener) {
    events.once(this, type, listener);
  },

  emit: function(type, ...args) {
    events.emit(this, type, ...args);
  },

  get actor() { return this.request.to || this.request.actor; }

};

/**
 * Creates a tab client for the remote debugging protocol server. This client
 * is a front to the tab actor created in the server side, hiding the protocol
 * details in a traditional JavaScript API.
 *
 * @param aClient DebuggerClient
 *        The debugger client parent.
 * @param aForm object
 *        The protocol form for this tab.
 */
function TabClient(aClient, aForm) {
  this.client = aClient;
  this._actor = aForm.from;
  this._threadActor = aForm.threadActor;
  this.javascriptEnabled = aForm.javascriptEnabled;
  this.cacheDisabled = aForm.cacheDisabled;
  this.thread = null;
  this.request = this.client.request;
  this.traits = aForm.traits || {};
  this.events = ["workerListChanged"];
}

TabClient.prototype = {
  get actor() { return this._actor },
  get _transport() { return this.client._transport; },

  /**
   * Attach to a thread actor.
   *
   * @param object aOptions
   *        Configuration options.
   *        - useSourceMaps: whether to use source maps or not.
   * @param function aOnResponse
   *        Called with the response packet and a ThreadClient
   *        (which will be undefined on error).
   */
  attachThread: function(aOptions={}, aOnResponse = noop) {
    if (this.thread) {
      DevToolsUtils.executeSoon(() => aOnResponse({}, this.thread));
      return;
    }

    let packet = {
      to: this._threadActor,
      type: "attach",
      options: aOptions
    };
    this.request(packet, (aResponse) => {
      if (!aResponse.error) {
        this.thread = new ThreadClient(this, this._threadActor);
        this.client.registerClient(this.thread);
      }
      aOnResponse(aResponse, this.thread);
    });
  },

  /**
   * Detach the client from the tab actor.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  detach: DebuggerClient.requester({
    type: "detach"
  }, {
    before: function (aPacket) {
      if (this.thread) {
        this.thread.detach();
      }
      return aPacket;
    },
    after: function (aResponse) {
      this.client.unregisterClient(this);
      return aResponse;
    },
    telemetry: "TABDETACH"
  }),

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
    options: args(0)
  }, {
    telemetry: "RELOAD"
  }),

  /**
   * Navigate to another URL.
   *
   * @param string url
   *        The URL to navigate to.
   */
  navigateTo: DebuggerClient.requester({
    type: "navigateTo",
    url: args(0)
  }, {
    telemetry: "NAVIGATETO"
  }),

  /**
   * Reconfigure the tab actor.
   *
   * @param object aOptions
   *        A dictionary object of the new options to use in the tab actor.
   * @param function aOnResponse
   *        Called with the response packet.
   */
  reconfigure: DebuggerClient.requester({
    type: "reconfigure",
    options: args(0)
  }, {
    telemetry: "RECONFIGURETAB"
  }),

  listWorkers: DebuggerClient.requester({
    type: "listWorkers"
  }, {
    telemetry: "LISTWORKERS"
  }),

  attachWorker: function (aWorkerActor, aOnResponse) {
    this.client.attachWorker(aWorkerActor, aOnResponse);
  }
};

eventSource(TabClient.prototype);

function WorkerClient(aClient, aForm) {
  this.client = aClient;
  this._actor = aForm.from;
  this._isClosed = false;
  this._isFrozen = aForm.isFrozen;
  this._url = aForm.url;

  this._onClose = this._onClose.bind(this);
  this._onFreeze = this._onFreeze.bind(this);
  this._onThaw = this._onThaw.bind(this);

  this.addListener("close", this._onClose);
  this.addListener("freeze", this._onFreeze);
  this.addListener("thaw", this._onThaw);

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

  get isFrozen() {
    return this._isFrozen;
  },

  detach: DebuggerClient.requester({ type: "detach" }, {
    after: function (aResponse) {
      this.client.unregisterClient(this);
      return aResponse;
    },

    telemetry: "WORKERDETACH"
  }),

  attachThread: function(aOptions = {}, aOnResponse = noop) {
    if (this.thread) {
      DevToolsUtils.executeSoon(() => aOnResponse({
        type: "connected",
        threadActor: this.thread._actor,
      }, this.thread));
      return;
    }

    this.request({
      to: this._actor,
      type: "connect",
      options: aOptions,
    }, (aResponse) => {
      if (!aResponse.error) {
        this.thread = new ThreadClient(this, aResponse.threadActor);
        this.client.registerClient(this.thread);
      }
      aOnResponse(aResponse, this.thread);
    });
  },

  _onClose: function () {
    this.removeListener("close", this._onClose);
    this.removeListener("freeze", this._onFreeze);
    this.removeListener("thaw", this._onThaw);

    this.client.unregisterClient(this);
    this._closed = true;
  },

  _onFreeze: function () {
    this._isFrozen = true;
  },

  _onThaw: function () {
    this._isFrozen = false;
  },

  reconfigure: function () {
    return Promise.resolve();
  },

  events: ["close", "freeze", "thaw"]
};

eventSource(WorkerClient.prototype);

function AddonClient(aClient, aActor) {
  this._client = aClient;
  this._actor = aActor;
  this.request = this._client.request;
  this.events = [];
}

AddonClient.prototype = {
  get actor() { return this._actor; },
  get _transport() { return this._client._transport; },

  /**
   * Detach the client from the addon actor.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  detach: DebuggerClient.requester({
    type: "detach"
  }, {
    after: function(aResponse) {
      if (this._client.activeAddon === this) {
        this._client.activeAddon = null
      }
      this._client.unregisterClient(this);
      return aResponse;
    },
    telemetry: "ADDONDETACH"
  })
};

/**
 * A RootClient object represents a root actor on the server. Each
 * DebuggerClient keeps a RootClient instance representing the root actor
 * for the initial connection; DebuggerClient's 'listTabs' and
 * 'listChildProcesses' methods forward to that root actor.
 *
 * @param aClient object
 *      The client connection to which this actor belongs.
 * @param aGreeting string
 *      The greeting packet from the root actor we're to represent.
 *
 * Properties of a RootClient instance:
 *
 * @property actor string
 *      The name of this child's root actor.
 * @property applicationType string
 *      The application type, as given in the root actor's greeting packet.
 * @property traits object
 *      The traits object, as given in the root actor's greeting packet.
 */
function RootClient(aClient, aGreeting) {
  this._client = aClient;
  this.actor = aGreeting.from;
  this.applicationType = aGreeting.applicationType;
  this.traits = aGreeting.traits;
}
exports.RootClient = RootClient;

RootClient.prototype = {
  constructor: RootClient,

  /**
   * List the open tabs.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  listTabs: DebuggerClient.requester({ type: "listTabs" },
                                     { telemetry: "LISTTABS" }),

  /**
   * List the installed addons.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  listAddons: DebuggerClient.requester({ type: "listAddons" },
                                       { telemetry: "LISTADDONS" }),

  /**
   * List the registered workers.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  listWorkers: DebuggerClient.requester({ type: "listWorkers" },
                                        { telemetry: "LISTWORKERS" }),

  /**
   * List the running processes.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  listProcesses: DebuggerClient.requester({ type: "listProcesses" },
                                          { telemetry: "LISTPROCESSES" }),

  /**
   * Fetch the TabActor for the currently selected tab, or for a specific
   * tab given as first parameter.
   *
   * @param [optional] object aFilter
   *        A dictionary object with following optional attributes:
   *         - outerWindowID: used to match tabs in parent process
   *         - tabId: used to match tabs in child processes
   *         - tab: a reference to xul:tab element
   *        If nothing is specified, returns the actor for the currently
   *        selected tab.
   */
  getTab: function (aFilter) {
    let packet = {
      to: this.actor,
      type: "getTab"
    };

    if (aFilter) {
      if (typeof(aFilter.outerWindowID) == "number") {
        packet.outerWindowID = aFilter.outerWindowID;
      } else if (typeof(aFilter.tabId) == "number") {
        packet.tabId = aFilter.tabId;
      } else if ("tab" in aFilter) {
        let browser = aFilter.tab.linkedBrowser;
        if (browser.frameLoader.tabParent) {
          // Tabs in child process
          packet.tabId = browser.frameLoader.tabParent.tabId;
        } else {
          // Tabs in parent process
          let windowUtils = browser.contentWindow
            .QueryInterface(Ci.nsIInterfaceRequestor)
            .getInterface(Ci.nsIDOMWindowUtils);
          packet.outerWindowID = windowUtils.outerWindowID;
        }
      } else {
        // Throw if a filter object have been passed but without
        // any clearly idenfified filter.
        throw new Error("Unsupported argument given to getTab request");
      }
    }

    return this.request(packet);
  },

  /**
   * Description of protocol's actors and methods.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
   protocolDescription: DebuggerClient.requester({ type: "protocolDescription" },
                                                 { telemetry: "PROTOCOLDESCRIPTION" }),

  /*
   * Methods constructed by DebuggerClient.requester require these forwards
   * on their 'this'.
   */
  get _transport() { return this._client._transport; },
  get request()    { return this._client.request;    }
};

/**
 * Creates a thread client for the remote debugging protocol server. This client
 * is a front to the thread actor created in the server side, hiding the
 * protocol details in a traditional JavaScript API.
 *
 * @param aClient DebuggerClient|TabClient
 *        The parent of the thread (tab for tab-scoped debuggers, DebuggerClient
 *        for chrome debuggers).
 * @param aActor string
 *        The actor ID for this thread.
 */
function ThreadClient(aClient, aActor) {
  this._parent = aClient;
  this.client = aClient instanceof DebuggerClient ? aClient : aClient.client;
  this._actor = aActor;
  this._frameCache = [];
  this._scriptCache = {};
  this._pauseGrips = {};
  this._threadGrips = {};
  this.request = this.client.request;
}

ThreadClient.prototype = {
  _state: "paused",
  get state() { return this._state; },
  get paused() { return this._state === "paused"; },

  _pauseOnExceptions: false,
  _ignoreCaughtExceptions: false,
  _pauseOnDOMEvents: null,

  _actor: null,
  get actor() { return this._actor; },

  get _transport() { return this.client._transport; },

  _assertPaused: function (aCommand) {
    if (!this.paused) {
      throw Error(aCommand + " command sent while not paused. Currently " + this._state);
    }
  },

  /**
   * Resume a paused thread. If the optional aLimit parameter is present, then
   * the thread will also pause when that limit is reached.
   *
   * @param [optional] object aLimit
   *        An object with a type property set to the appropriate limit (next,
   *        step, or finish) per the remote debugging protocol specification.
   *        Use null to specify no limit.
   * @param function aOnResponse
   *        Called with the response packet.
   */
  _doResume: DebuggerClient.requester({
    type: "resume",
    resumeLimit: args(0)
  }, {
    before: function (aPacket) {
      this._assertPaused("resume");

      // Put the client in a tentative "resuming" state so we can prevent
      // further requests that should only be sent in the paused state.
      this._state = "resuming";

      if (this._pauseOnExceptions) {
        aPacket.pauseOnExceptions = this._pauseOnExceptions;
      }
      if (this._ignoreCaughtExceptions) {
        aPacket.ignoreCaughtExceptions = this._ignoreCaughtExceptions;
      }
      if (this._pauseOnDOMEvents) {
        aPacket.pauseOnDOMEvents = this._pauseOnDOMEvents;
      }
      return aPacket;
    },
    after: function (aResponse) {
      if (aResponse.error) {
        // There was an error resuming, back to paused state.
        this._state = "paused";
      }
      return aResponse;
    },
    telemetry: "RESUME"
  }),

  /**
   * Reconfigure the thread actor.
   *
   * @param object aOptions
   *        A dictionary object of the new options to use in the thread actor.
   * @param function aOnResponse
   *        Called with the response packet.
   */
  reconfigure: DebuggerClient.requester({
    type: "reconfigure",
    options: args(0)
  }, {
    telemetry: "RECONFIGURETHREAD"
  }),

  /**
   * Resume a paused thread.
   */
  resume: function (aOnResponse) {
    this._doResume(null, aOnResponse);
  },

  /**
   * Resume then pause without stepping.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  resumeThenPause: function (aOnResponse) {
    this._doResume({ type: "break" }, aOnResponse);
  },

  /**
   * Step over a function call.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  stepOver: function (aOnResponse) {
    this._doResume({ type: "next" }, aOnResponse);
  },

  /**
   * Step into a function call.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  stepIn: function (aOnResponse) {
    this._doResume({ type: "step" }, aOnResponse);
  },

  /**
   * Step out of a function call.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  stepOut: function (aOnResponse) {
    this._doResume({ type: "finish" }, aOnResponse);
  },

  /**
   * Immediately interrupt a running thread.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  interrupt: function(aOnResponse) {
    this._doInterrupt(null, aOnResponse);
  },

  /**
   * Pause execution right before the next JavaScript bytecode is executed.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  breakOnNext: function(aOnResponse) {
    this._doInterrupt("onNext", aOnResponse);
  },

  /**
   * Interrupt a running thread.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  _doInterrupt: DebuggerClient.requester({
    type: "interrupt",
    when: args(0)
  }, {
    telemetry: "INTERRUPT"
  }),

  /**
   * Enable or disable pausing when an exception is thrown.
   *
   * @param boolean aFlag
   *        Enables pausing if true, disables otherwise.
   * @param function aOnResponse
   *        Called with the response packet.
   */
  pauseOnExceptions: function (aPauseOnExceptions,
                               aIgnoreCaughtExceptions,
                               aOnResponse = noop) {
    this._pauseOnExceptions = aPauseOnExceptions;
    this._ignoreCaughtExceptions = aIgnoreCaughtExceptions;

    // If the debuggee is paused, we have to send the flag via a reconfigure
    // request.
    if (this.paused) {
      this.reconfigure({
        pauseOnExceptions: aPauseOnExceptions,
        ignoreCaughtExceptions: aIgnoreCaughtExceptions
      }, aOnResponse);
      return;
    }
    // Otherwise send the flag using a standard resume request.
    this.interrupt(aResponse => {
      if (aResponse.error) {
        // Can't continue if pausing failed.
        aOnResponse(aResponse);
        return;
      }
      this.resume(aOnResponse);
    });
  },

  /**
   * Enable pausing when the specified DOM events are triggered. Disabling
   * pausing on an event can be realized by calling this method with the updated
   * array of events that doesn't contain it.
   *
   * @param array|string events
   *        An array of strings, representing the DOM event types to pause on,
   *        or "*" to pause on all DOM events. Pass an empty array to
   *        completely disable pausing on DOM events.
   * @param function onResponse
   *        Called with the response packet in a future turn of the event loop.
   */
  pauseOnDOMEvents: function (events, onResponse = noop) {
    this._pauseOnDOMEvents = events;
    // If the debuggee is paused, the value of the array will be communicated in
    // the next resumption. Otherwise we have to force a pause in order to send
    // the array.
    if (this.paused) {
      DevToolsUtils.executeSoon(() => onResponse({}));
      return;
    }
    this.interrupt(response => {
      // Can't continue if pausing failed.
      if (response.error) {
        onResponse(response);
        return;
      }
      this.resume(onResponse);
    });
  },

  /**
   * Send a clientEvaluate packet to the debuggee. Response
   * will be a resume packet.
   *
   * @param string aFrame
   *        The actor ID of the frame where the evaluation should take place.
   * @param string aExpression
   *        The expression that will be evaluated in the scope of the frame
   *        above.
   * @param function aOnResponse
   *        Called with the response packet.
   */
  eval: DebuggerClient.requester({
    type: "clientEvaluate",
    frame: args(0),
    expression: args(1)
  }, {
    before: function (aPacket) {
      this._assertPaused("eval");
      // Put the client in a tentative "resuming" state so we can prevent
      // further requests that should only be sent in the paused state.
      this._state = "resuming";
      return aPacket;
    },
    after: function (aResponse) {
      if (aResponse.error) {
        // There was an error resuming, back to paused state.
        this._state = "paused";
      }
      return aResponse;
    },
    telemetry: "CLIENTEVALUATE"
  }),

  /**
   * Detach from the thread actor.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  detach: DebuggerClient.requester({
    type: "detach"
  }, {
    after: function (aResponse) {
      this.client.unregisterClient(this);
      this._parent.thread = null;
      return aResponse;
    },
    telemetry: "THREADDETACH"
  }),

  /**
   * Release multiple thread-lifetime object actors. If any pause-lifetime
   * actors are included in the request, a |notReleasable| error will return,
   * but all the thread-lifetime ones will have been released.
   *
   * @param array actors
   *        An array with actor IDs to release.
   */
  releaseMany: DebuggerClient.requester({
    type: "releaseMany",
    actors: args(0),
  }, {
    telemetry: "RELEASEMANY"
  }),

  /**
   * Promote multiple pause-lifetime object actors to thread-lifetime ones.
   *
   * @param array actors
   *        An array with actor IDs to promote.
   */
  threadGrips: DebuggerClient.requester({
    type: "threadGrips",
    actors: args(0)
  }, {
    telemetry: "THREADGRIPS"
  }),

  /**
   * Return the event listeners defined on the page.
   *
   * @param aOnResponse Function
   *        Called with the thread's response.
   */
  eventListeners: DebuggerClient.requester({
    type: "eventListeners"
  }, {
    telemetry: "EVENTLISTENERS"
  }),

  /**
   * Request the loaded sources for the current thread.
   *
   * @param aOnResponse Function
   *        Called with the thread's response.
   */
  getSources: DebuggerClient.requester({
    type: "sources"
  }, {
    telemetry: "SOURCES"
  }),

  /**
   * Clear the thread's source script cache. A scriptscleared event
   * will be sent.
   */
  _clearScripts: function () {
    if (Object.keys(this._scriptCache).length > 0) {
      this._scriptCache = {}
      this.emit("scriptscleared");
    }
  },

  /**
   * Request frames from the callstack for the current thread.
   *
   * @param aStart integer
   *        The number of the youngest stack frame to return (the youngest
   *        frame is 0).
   * @param aCount integer
   *        The maximum number of frames to return, or null to return all
   *        frames.
   * @param aOnResponse function
   *        Called with the thread's response.
   */
  getFrames: DebuggerClient.requester({
    type: "frames",
    start: args(0),
    count: args(1)
  }, {
    telemetry: "FRAMES"
  }),

  /**
   * An array of cached frames. Clients can observe the framesadded and
   * framescleared event to keep up to date on changes to this cache,
   * and can fill it using the fillFrames method.
   */
  get cachedFrames() { return this._frameCache; },

  /**
   * true if there are more stack frames available on the server.
   */
  get moreFrames() {
    return this.paused && (!this._frameCache || this._frameCache.length == 0
          || !this._frameCache[this._frameCache.length - 1].oldest);
  },

  /**
   * Ensure that at least aTotal stack frames have been loaded in the
   * ThreadClient's stack frame cache. A framesadded event will be
   * sent when the stack frame cache is updated.
   *
   * @param aTotal number
   *        The minimum number of stack frames to be included.
   * @param aCallback function
   *        Optional callback function called when frames have been loaded
   * @returns true if a framesadded notification should be expected.
   */
  fillFrames: function (aTotal, aCallback=noop) {
    this._assertPaused("fillFrames");
    if (this._frameCache.length >= aTotal) {
      return false;
    }

    let numFrames = this._frameCache.length;

    this.getFrames(numFrames, aTotal - numFrames, (aResponse) => {
      if (aResponse.error) {
        aCallback(aResponse);
        return;
      }

      let threadGrips = DevToolsUtils.values(this._threadGrips);

      for (let i in aResponse.frames) {
        let frame = aResponse.frames[i];
        if (!frame.where.source) {
          // Older servers use urls instead, so we need to resolve
          // them to source actors
          for (let grip of threadGrips) {
            if (grip instanceof SourceClient && grip.url === frame.url) {
              frame.where.source = grip._form;
            }
          }
        }

        this._frameCache[frame.depth] = frame;
      }

      // If we got as many frames as we asked for, there might be more
      // frames available.
      this.emit("framesadded");

      aCallback(aResponse);
    });

    return true;
  },

  /**
   * Clear the thread's stack frame cache. A framescleared event
   * will be sent.
   */
  _clearFrames: function () {
    if (this._frameCache.length > 0) {
      this._frameCache = [];
      this.emit("framescleared");
    }
  },

  /**
   * Return a ObjectClient object for the given object grip.
   *
   * @param aGrip object
   *        A pause-lifetime object grip returned by the protocol.
   */
  pauseGrip: function (aGrip) {
    if (aGrip.actor in this._pauseGrips) {
      return this._pauseGrips[aGrip.actor];
    }

    let client = new ObjectClient(this.client, aGrip);
    this._pauseGrips[aGrip.actor] = client;
    return client;
  },

  /**
   * Get or create a long string client, checking the grip client cache if it
   * already exists.
   *
   * @param aGrip Object
   *        The long string grip returned by the protocol.
   * @param aGripCacheName String
   *        The property name of the grip client cache to check for existing
   *        clients in.
   */
  _longString: function (aGrip, aGripCacheName) {
    if (aGrip.actor in this[aGripCacheName]) {
      return this[aGripCacheName][aGrip.actor];
    }

    let client = new LongStringClient(this.client, aGrip);
    this[aGripCacheName][aGrip.actor] = client;
    return client;
  },

  /**
   * Return an instance of LongStringClient for the given long string grip that
   * is scoped to the current pause.
   *
   * @param aGrip Object
   *        The long string grip returned by the protocol.
   */
  pauseLongString: function (aGrip) {
    return this._longString(aGrip, "_pauseGrips");
  },

  /**
   * Return an instance of LongStringClient for the given long string grip that
   * is scoped to the thread lifetime.
   *
   * @param aGrip Object
   *        The long string grip returned by the protocol.
   */
  threadLongString: function (aGrip) {
    return this._longString(aGrip, "_threadGrips");
  },

  /**
   * Clear and invalidate all the grip clients from the given cache.
   *
   * @param aGripCacheName
   *        The property name of the grip cache we want to clear.
   */
  _clearObjectClients: function (aGripCacheName) {
    for (let id in this[aGripCacheName]) {
      this[aGripCacheName][id].valid = false;
    }
    this[aGripCacheName] = {};
  },

  /**
   * Invalidate pause-lifetime grip clients and clear the list of current grip
   * clients.
   */
  _clearPauseGrips: function () {
    this._clearObjectClients("_pauseGrips");
  },

  /**
   * Invalidate thread-lifetime grip clients and clear the list of current grip
   * clients.
   */
  _clearThreadGrips: function () {
    this._clearObjectClients("_threadGrips");
  },

  /**
   * Handle thread state change by doing necessary cleanup and notifying all
   * registered listeners.
   */
  _onThreadState: function (aPacket) {
    this._state = ThreadStateTypes[aPacket.type];
    this._clearFrames();
    this._clearPauseGrips();
    aPacket.type === ThreadStateTypes.detached && this._clearThreadGrips();
    this.client._eventsEnabled && this.emit(aPacket.type, aPacket);
  },

  /**
   * Return an EnvironmentClient instance for the given environment actor form.
   */
  environment: function (aForm) {
    return new EnvironmentClient(this.client, aForm);
  },

  /**
   * Return an instance of SourceClient for the given source actor form.
   */
  source: function (aForm) {
    if (aForm.actor in this._threadGrips) {
      return this._threadGrips[aForm.actor];
    }

    return this._threadGrips[aForm.actor] = new SourceClient(this, aForm);
  },

  /**
   * Request the prototype and own properties of mutlipleObjects.
   *
   * @param aOnResponse function
   *        Called with the request's response.
   * @param actors [string]
   *        List of actor ID of the queried objects.
   */
  getPrototypesAndProperties: DebuggerClient.requester({
    type: "prototypesAndProperties",
    actors: args(0)
  }, {
    telemetry: "PROTOTYPESANDPROPERTIES"
  }),

  events: ["newSource"]
};

eventSource(ThreadClient.prototype);

/**
 * Creates a tracing profiler client for the remote debugging protocol
 * server. This client is a front to the trace actor created on the
 * server side, hiding the protocol details in a traditional
 * JavaScript API.
 *
 * @param aClient DebuggerClient
 *        The debugger client parent.
 * @param aActor string
 *        The actor ID for this thread.
 */
function TraceClient(aClient, aActor) {
  this._client = aClient;
  this._actor = aActor;
  this._activeTraces = new Set();
  this._waitingPackets = new Map();
  this._expectedPacket = 0;
  this.request = this._client.request;
  this.events = [];
}

TraceClient.prototype = {
  get actor()   { return this._actor; },
  get tracing() { return this._activeTraces.size > 0; },

  get _transport() { return this._client._transport; },

  /**
   * Detach from the trace actor.
   */
  detach: DebuggerClient.requester({
    type: "detach"
  }, {
    after: function (aResponse) {
      this._client.unregisterClient(this);
      return aResponse;
    },
    telemetry: "TRACERDETACH"
  }),

  /**
   * Start a new trace.
   *
   * @param aTrace [string]
   *        An array of trace types to be recorded by the new trace.
   *
   * @param aName string
   *        The name of the new trace.
   *
   * @param aOnResponse function
   *        Called with the request's response.
   */
  startTrace: DebuggerClient.requester({
    type: "startTrace",
    name: args(1),
    trace: args(0)
  }, {
    after: function (aResponse) {
      if (aResponse.error) {
        return aResponse;
      }

      if (!this.tracing) {
        this._waitingPackets.clear();
        this._expectedPacket = 0;
      }
      this._activeTraces.add(aResponse.name);

      return aResponse;
    },
    telemetry: "STARTTRACE"
  }),

  /**
   * End a trace. If a name is provided, stop the named
   * trace. Otherwise, stop the most recently started trace.
   *
   * @param aName string
   *        The name of the trace to stop.
   *
   * @param aOnResponse function
   *        Called with the request's response.
   */
  stopTrace: DebuggerClient.requester({
    type: "stopTrace",
    name: args(0)
  }, {
    after: function (aResponse) {
      if (aResponse.error) {
        return aResponse;
      }

      this._activeTraces.delete(aResponse.name);

      return aResponse;
    },
    telemetry: "STOPTRACE"
  })
};

/**
 * Grip clients are used to retrieve information about the relevant object.
 *
 * @param aClient DebuggerClient
 *        The debugger client parent.
 * @param aGrip object
 *        A pause-lifetime object grip returned by the protocol.
 */
function ObjectClient(aClient, aGrip)
{
  this._grip = aGrip;
  this._client = aClient;
  this.request = this._client.request;
}
exports.ObjectClient = ObjectClient;

ObjectClient.prototype = {
  get actor() { return this._grip.actor },
  get _transport() { return this._client._transport; },

  valid: true,

  get isFrozen() {
    return this._grip.frozen;
  },
  get isSealed() {
    return this._grip.sealed;
  },
  get isExtensible() {
    return this._grip.extensible;
  },

  getDefinitionSite: DebuggerClient.requester({
    type: "definitionSite"
  }, {
    before: function (aPacket) {
      if (this._grip.class != "Function") {
        throw new Error("getDefinitionSite is only valid for function grips.");
      }
      return aPacket;
    }
  }),

  /**
   * Request the names of a function's formal parameters.
   *
   * @param aOnResponse function
   *        Called with an object of the form:
   *        { parameterNames:[<parameterName>, ...] }
   *        where each <parameterName> is the name of a parameter.
   */
  getParameterNames: DebuggerClient.requester({
    type: "parameterNames"
  }, {
    before: function (aPacket) {
      if (this._grip["class"] !== "Function") {
        throw new Error("getParameterNames is only valid for function grips.");
      }
      return aPacket;
    },
    telemetry: "PARAMETERNAMES"
  }),

  /**
   * Request the names of the properties defined on the object and not its
   * prototype.
   *
   * @param aOnResponse function Called with the request's response.
   */
  getOwnPropertyNames: DebuggerClient.requester({
    type: "ownPropertyNames"
  }, {
    telemetry: "OWNPROPERTYNAMES"
  }),

  /**
   * Request the prototype and own properties of the object.
   *
   * @param aOnResponse function Called with the request's response.
   */
  getPrototypeAndProperties: DebuggerClient.requester({
    type: "prototypeAndProperties"
  }, {
    telemetry: "PROTOTYPEANDPROPERTIES"
  }),

  /**
   * Request a PropertyIteratorClient instance to ease listing
   * properties for this object.
   *
   * @param options Object
   *        A dictionary object with various boolean attributes:
   *        - ignoreSafeGetters Boolean
   *          If true, do not iterate over safe getters.
   *        - ignoreIndexedProperties Boolean
   *          If true, filters out Array items.
   *          e.g. properties names between `0` and `object.length`.
   *        - ignoreNonIndexedProperties Boolean
   *          If true, filters out items that aren't array items
   *          e.g. properties names that are not a number between `0`
   *          and `object.length`.
   *        - sort Boolean
   *          If true, the iterator will sort the properties by name
   *          before dispatching them.
   * @param aOnResponse function Called with the client instance.
   */
  enumProperties: DebuggerClient.requester({
    type: "enumProperties",
    options: args(0)
  }, {
    after: function(aResponse) {
      if (aResponse.iterator) {
        return { iterator: new PropertyIteratorClient(this._client, aResponse.iterator) };
      }
      return aResponse;
    },
    telemetry: "ENUMPROPERTIES"
  }),

  /**
   * Request the property descriptor of the object's specified property.
   *
   * @param aName string The name of the requested property.
   * @param aOnResponse function Called with the request's response.
   */
  getProperty: DebuggerClient.requester({
    type: "property",
    name: args(0)
  }, {
    telemetry: "PROPERTY"
  }),

  /**
   * Request the prototype of the object.
   *
   * @param aOnResponse function Called with the request's response.
   */
  getPrototype: DebuggerClient.requester({
    type: "prototype"
  }, {
    telemetry: "PROTOTYPE"
  }),

  /**
   * Request the display string of the object.
   *
   * @param aOnResponse function Called with the request's response.
   */
  getDisplayString: DebuggerClient.requester({
    type: "displayString"
  }, {
    telemetry: "DISPLAYSTRING"
  }),

  /**
   * Request the scope of the object.
   *
   * @param aOnResponse function Called with the request's response.
   */
  getScope: DebuggerClient.requester({
    type: "scope"
  }, {
    before: function (aPacket) {
      if (this._grip.class !== "Function") {
        throw new Error("scope is only valid for function grips.");
      }
      return aPacket;
    },
    telemetry: "SCOPE"
  }),

  /**
   * Request the promises directly depending on the current promise.
   */
  getDependentPromises: DebuggerClient.requester({
    type: "dependentPromises"
  }, {
    before: function(aPacket) {
      if (this._grip.class !== "Promise") {
        throw new Error("getDependentPromises is only valid for promise " +
          "grips.");
      }
      return aPacket;
    }
  }),

  /**
   * Request the stack to the promise's allocation point.
   */
  getPromiseAllocationStack: DebuggerClient.requester({
    type: "allocationStack"
  }, {
    before: function(aPacket) {
      if (this._grip.class !== "Promise") {
        throw new Error("getAllocationStack is only valid for promise grips.");
      }
      return aPacket;
    }
  }),

  /**
   * Request the stack to the promise's fulfillment point.
   */
  getPromiseFulfillmentStack: DebuggerClient.requester({
    type: "fulfillmentStack"
  }, {
    before: function(packet) {
      if (this._grip.class !== "Promise") {
        throw new Error("getPromiseFulfillmentStack is only valid for " +
          "promise grips.");
      }
      return packet;
    }
  }),

  /**
   * Request the stack to the promise's rejection point.
   */
  getPromiseRejectionStack: DebuggerClient.requester({
    type: "rejectionStack"
  }, {
    before: function(packet) {
      if (this._grip.class !== "Promise") {
        throw new Error("getPromiseRejectionStack is only valid for " +
          "promise grips.");
      }
      return packet;
    }
  })
};

/**
 * A PropertyIteratorClient provides a way to access to property names and
 * values of an object efficiently, slice by slice.
 * Note that the properties can be sorted in the backend,
 * this is controled while creating the PropertyIteratorClient
 * from ObjectClient.enumProperties.
 *
 * @param aClient DebuggerClient
 *        The debugger client parent.
 * @param aGrip Object
 *        A PropertyIteratorActor grip returned by the protocol via
 *        TabActor.enumProperties request.
 */
function PropertyIteratorClient(aClient, aGrip) {
  this._grip = aGrip;
  this._client = aClient;
  this.request = this._client.request;
}

PropertyIteratorClient.prototype = {
  get actor() { return this._grip.actor; },

  /**
   * Get the total number of properties available in the iterator.
   */
  get count() { return this._grip.count; },

  /**
   * Get one or more property names that correspond to the positions in the
   * indexes parameter.
   *
   * @param indexes Array
   *        An array of property indexes.
   * @param aCallback Function
   *        The function called when we receive the property names.
   */
  names: DebuggerClient.requester({
    type: "names",
    indexes: args(0)
  }, {}),

  /**
   * Get a set of following property value(s).
   *
   * @param start Number
   *        The index of the first property to fetch.
   * @param count Number
   *        The number of properties to fetch.
   * @param aCallback Function
   *        The function called when we receive the property values.
   */
  slice: DebuggerClient.requester({
    type: "slice",
    start: args(0),
    count: args(1)
  }, {}),

  /**
   * Get all the property values.
   *
   * @param aCallback Function
   *        The function called when we receive the property values.
   */
  all: DebuggerClient.requester({
    type: "all"
  }, {}),
};

/**
 * A LongStringClient provides a way to access "very long" strings from the
 * debugger server.
 *
 * @param aClient DebuggerClient
 *        The debugger client parent.
 * @param aGrip Object
 *        A pause-lifetime long string grip returned by the protocol.
 */
function LongStringClient(aClient, aGrip) {
  this._grip = aGrip;
  this._client = aClient;
  this.request = this._client.request;
}
exports.LongStringClient = LongStringClient;

LongStringClient.prototype = {
  get actor() { return this._grip.actor; },
  get length() { return this._grip.length; },
  get initial() { return this._grip.initial; },
  get _transport() { return this._client._transport; },

  valid: true,

  /**
   * Get the substring of this LongString from aStart to aEnd.
   *
   * @param aStart Number
   *        The starting index.
   * @param aEnd Number
   *        The ending index.
   * @param aCallback Function
   *        The function called when we receive the substring.
   */
  substring: DebuggerClient.requester({
    type: "substring",
    start: args(0),
    end: args(1)
  }, {
    telemetry: "SUBSTRING"
  }),
};

/**
 * A SourceClient provides a way to access the source text of a script.
 *
 * @param aClient ThreadClient
 *        The thread client parent.
 * @param aForm Object
 *        The form sent across the remote debugging protocol.
 */
function SourceClient(aClient, aForm) {
  this._form = aForm;
  this._isBlackBoxed = aForm.isBlackBoxed;
  this._isPrettyPrinted = aForm.isPrettyPrinted;
  this._activeThread = aClient;
  this._client = aClient.client;
}

SourceClient.prototype = {
  get _transport() {
    return this._client._transport;
  },
  get isBlackBoxed() {
    return this._isBlackBoxed;
  },
  get isPrettyPrinted() {
    return this._isPrettyPrinted;
  },
  get actor() {
    return this._form.actor;
  },
  get request() {
    return this._client.request;
  },
  get url() {
    return this._form.url;
  },

  /**
   * Black box this SourceClient's source.
   *
   * @param aCallback Function
   *        The callback function called when we receive the response from the server.
   */
  blackBox: DebuggerClient.requester({
    type: "blackbox"
  }, {
    telemetry: "BLACKBOX",
    after: function (aResponse) {
      if (!aResponse.error) {
        this._isBlackBoxed = true;
        if (this._activeThread) {
          this._activeThread.emit("blackboxchange", this);
        }
      }
      return aResponse;
    }
  }),

  /**
   * Un-black box this SourceClient's source.
   *
   * @param aCallback Function
   *        The callback function called when we receive the response from the server.
   */
  unblackBox: DebuggerClient.requester({
    type: "unblackbox"
  }, {
    telemetry: "UNBLACKBOX",
    after: function (aResponse) {
      if (!aResponse.error) {
        this._isBlackBoxed = false;
        if (this._activeThread) {
          this._activeThread.emit("blackboxchange", this);
        }
      }
      return aResponse;
    }
  }),

  /**
   * Get Executable Lines from a source
   *
   * @param aCallback Function
   *        The callback function called when we receive the response from the server.
   */
  getExecutableLines: function(cb){
    let packet = {
      to: this._form.actor,
      type: "getExecutableLines"
    };

    this._client.request(packet, res => {
      cb(res.lines);
    });
  },

  /**
   * Get a long string grip for this SourceClient's source.
   */
  source: function (aCallback) {
    let packet = {
      to: this._form.actor,
      type: "source"
    };
    this._client.request(packet, aResponse => {
      this._onSourceResponse(aResponse, aCallback)
    });
  },

  /**
   * Pretty print this source's text.
   */
  prettyPrint: function (aIndent, aCallback) {
    const packet = {
      to: this._form.actor,
      type: "prettyPrint",
      indent: aIndent
    };
    this._client.request(packet, aResponse => {
      if (!aResponse.error) {
        this._isPrettyPrinted = true;
        this._activeThread._clearFrames();
        this._activeThread.emit("prettyprintchange", this);
      }
      this._onSourceResponse(aResponse, aCallback);
    });
  },

  /**
   * Stop pretty printing this source's text.
   */
  disablePrettyPrint: function (aCallback) {
    const packet = {
      to: this._form.actor,
      type: "disablePrettyPrint"
    };
    this._client.request(packet, aResponse => {
      if (!aResponse.error) {
        this._isPrettyPrinted = false;
        this._activeThread._clearFrames();
        this._activeThread.emit("prettyprintchange", this);
      }
      this._onSourceResponse(aResponse, aCallback);
    });
  },

  _onSourceResponse: function (aResponse, aCallback) {
    if (aResponse.error) {
      aCallback(aResponse);
      return;
    }

    if (typeof aResponse.source === "string") {
      aCallback(aResponse);
      return;
    }

    let { contentType, source } = aResponse;
    let longString = this._activeThread.threadLongString(source);
    longString.substring(0, longString.length, function (aResponse) {
      if (aResponse.error) {
        aCallback(aResponse);
        return;
      }

      aCallback({
        source: aResponse.substring,
        contentType: contentType
      });
    });
  },

  /**
   * Request to set a breakpoint in the specified location.
   *
   * @param object aLocation
   *        The location and condition of the breakpoint in
   *        the form of { line[, column, condition] }.
   * @param function aOnResponse
   *        Called with the thread's response.
   */
  setBreakpoint: function ({ line, column, condition }, aOnResponse = noop) {
    // A helper function that sets the breakpoint.
    let doSetBreakpoint = aCallback => {
      let root = this._client.mainRoot;
      let location = {
        line: line,
        column: column
      };

      let packet = {
        to: this.actor,
        type: "setBreakpoint",
        location: location,
        condition: condition
      };

      // Backwards compatibility: send the breakpoint request to the
      // thread if the server doesn't support Debugger.Source actors.
      if (!root.traits.debuggerSourceActors) {
        packet.to = this._activeThread.actor;
        packet.location.url = this.url;
      }

      this._client.request(packet, aResponse => {
        // Ignoring errors, since the user may be setting a breakpoint in a
        // dead script that will reappear on a page reload.
        let bpClient;
        if (aResponse.actor) {
          bpClient = new BreakpointClient(
            this._client,
            this,
            aResponse.actor,
            location,
            root.traits.conditionalBreakpoints ? condition : undefined
          );
        }
        aOnResponse(aResponse, bpClient);
        if (aCallback) {
          aCallback();
        }
      });
    };

    // If the debuggee is paused, just set the breakpoint.
    if (this._activeThread.paused) {
      doSetBreakpoint();
      return;
    }
    // Otherwise, force a pause in order to set the breakpoint.
    this._activeThread.interrupt(aResponse => {
      if (aResponse.error) {
        // Can't set the breakpoint if pausing failed.
        aOnResponse(aResponse);
        return;
      }

      const { type, why } = aResponse;
      const cleanUp = type == "paused" && why.type == "interrupted"
            ? () => this._activeThread.resume()
            : noop;

      doSetBreakpoint(cleanUp);
    })
  }
};

/**
 * Breakpoint clients are used to remove breakpoints that are no longer used.
 *
 * @param aClient DebuggerClient
 *        The debugger client parent.
 * @param aSourceClient SourceClient
 *        The source where this breakpoint exists
 * @param aActor string
 *        The actor ID for this breakpoint.
 * @param aLocation object
 *        The location of the breakpoint. This is an object with two properties:
 *        url and line.
 * @param aCondition string
 *        The conditional expression of the breakpoint
 */
function BreakpointClient(aClient, aSourceClient, aActor, aLocation, aCondition) {
  this._client = aClient;
  this._actor = aActor;
  this.location = aLocation;
  this.location.actor = aSourceClient.actor;
  this.location.url = aSourceClient.url;
  this.source = aSourceClient;
  this.request = this._client.request;

  // The condition property should only exist if it's a truthy value
  if (aCondition) {
    this.condition = aCondition;
  }
}

BreakpointClient.prototype = {

  _actor: null,
  get actor() { return this._actor; },
  get _transport() { return this._client._transport; },

  /**
   * Remove the breakpoint from the server.
   */
  remove: DebuggerClient.requester({
    type: "delete"
  }, {
    telemetry: "DELETE"
  }),

  /**
   * Determines if this breakpoint has a condition
   */
  hasCondition: function() {
    let root = this._client.mainRoot;
    // XXX bug 990137: We will remove support for client-side handling of
    // conditional breakpoints
    if (root.traits.conditionalBreakpoints) {
      return "condition" in this;
    } else {
      return "conditionalExpression" in this;
    }
  },

  /**
   * Get the condition of this breakpoint. Currently we have to
   * support locally emulated conditional breakpoints until the
   * debugger servers are updated (see bug 990137). We used a
   * different property when moving it server-side to ensure that we
   * are testing the right code.
   */
  getCondition: function() {
    let root = this._client.mainRoot;
    if (root.traits.conditionalBreakpoints) {
      return this.condition;
    } else {
      return this.conditionalExpression;
    }
  },

  /**
   * Set the condition of this breakpoint
   */
  setCondition: function(gThreadClient, aCondition) {
    let root = this._client.mainRoot;
    let deferred = promise.defer();

    if (root.traits.conditionalBreakpoints) {
      let info = {
        line: this.location.line,
        column: this.location.column,
        condition: aCondition
      };

      // Remove the current breakpoint and add a new one with the
      // condition.
      this.remove(aResponse => {
        if (aResponse && aResponse.error) {
          deferred.reject(aResponse);
          return;
        }

        this.source.setBreakpoint(info, (aResponse, aNewBreakpoint) => {
          if (aResponse && aResponse.error) {
            deferred.reject(aResponse);
          } else {
            deferred.resolve(aNewBreakpoint);
          }
        });
      });
    } else {
      // The property shouldn't even exist if the condition is blank
      if (aCondition === "") {
        delete this.conditionalExpression;
      }
      else {
        this.conditionalExpression = aCondition;
      }
      deferred.resolve(this);
    }

    return deferred.promise;
  }
};

eventSource(BreakpointClient.prototype);

/**
 * Environment clients are used to manipulate the lexical environment actors.
 *
 * @param aClient DebuggerClient
 *        The debugger client parent.
 * @param aForm Object
 *        The form sent across the remote debugging protocol.
 */
function EnvironmentClient(aClient, aForm) {
  this._client = aClient;
  this._form = aForm;
  this.request = this._client.request;
}
exports.EnvironmentClient = EnvironmentClient;

EnvironmentClient.prototype = {

  get actor() {
    return this._form.actor;
  },
  get _transport() { return this._client._transport; },

  /**
   * Fetches the bindings introduced by this lexical environment.
   */
  getBindings: DebuggerClient.requester({
    type: "bindings"
  }, {
    telemetry: "BINDINGS"
  }),

  /**
   * Changes the value of the identifier whose name is name (a string) to that
   * represented by value (a grip).
   */
  assign: DebuggerClient.requester({
    type: "assign",
    name: args(0),
    value: args(1)
  }, {
    telemetry: "ASSIGN"
  })
};

eventSource(EnvironmentClient.prototype);
