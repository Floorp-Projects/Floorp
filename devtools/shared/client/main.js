/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci, Cu } = require("chrome");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const { getStack, callFunctionWithAsyncStack } = require("devtools/shared/platform/stack");

const promise = Cu.import("resource://devtools/shared/deprecated-sync-thenables.js", {}).Promise;

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
 * @param proto object
 *        The prototype object that will be modified.
 */
function eventSource(proto) {
  /**
   * Add a listener to the event source for a given event.
   *
   * @param name string
   *        The event to listen for.
   * @param listener function
   *        Called when the event is fired. If the same listener
   *        is added more than once, it will be called once per
   *        addListener call.
   */
  proto.addListener = function (name, listener) {
    if (typeof listener != "function") {
      throw TypeError("Listeners must be functions.");
    }

    if (!this._listeners) {
      this._listeners = {};
    }

    this._getListeners(name).push(listener);
  };

  /**
   * Add a listener to the event source for a given event. The
   * listener will be removed after it is called for the first time.
   *
   * @param name string
   *        The event to listen for.
   * @param listener function
   *        Called when the event is fired.
   */
  proto.addOneTimeListener = function (name, listener) {
    let l = (...args) => {
      this.removeListener(name, l);
      listener.apply(null, args);
    };
    this.addListener(name, l);
  };

  /**
   * Remove a listener from the event source previously added with
   * addListener().
   *
   * @param name string
   *        The event name used during addListener to add the listener.
   * @param listener function
   *        The callback to remove. If addListener was called multiple
   *        times, all instances will be removed.
   */
  proto.removeListener = function (name, listener) {
    if (!this._listeners || (listener && !this._listeners[name])) {
      return;
    }

    if (!listener) {
      this._listeners[name] = [];
    } else {
      this._listeners[name] =
        this._listeners[name].filter(l => l != listener);
    }
  };

  /**
   * Returns the listeners for the specified event name. If none are defined it
   * initializes an empty list and returns that.
   *
   * @param name string
   *        The event name.
   */
  proto._getListeners = function (name) {
    if (name in this._listeners) {
      return this._listeners[name];
    }
    this._listeners[name] = [];
    return this._listeners[name];
  };

  /**
   * Notify listeners of an event.
   *
   * @param name string
   *        The event to fire.
   * @param arguments
   *        All arguments will be passed along to the listeners,
   *        including the name argument.
   */
  proto.emit = function () {
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
  };
}

/**
 * Set of protocol messages that affect thread state, and the
 * state the actor is in after each message.
 */
const ThreadStateTypes = {
  "paused": "paused",
  "resumed": "attached",
  "detached": "detached",
  "running": "attached"
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
  "serviceWorkerRegistrationListChanged": "serviceWorkerRegistrationList",
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
  "newSource": "newSource",
  "updatedSource": "updatedSource",
  "inspectObject": "inspectObject"
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
const DebuggerClient = exports.DebuggerClient = function (transport) {
  this._transport = transport;
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
  this.expectReply("root", (packet) => {
    this.mainRoot = new RootClient(this, packet);
    this.emit("connected", packet.applicationType, packet.traits);
  });
};

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
DebuggerClient.requester = function (packetSkeleton, config = {}) {
  let { before, after } = config;
  return DevToolsUtils.makeInfallible(function (...args) {
    let outgoingPacket = {
      to: packetSkeleton.to || this.actor
    };

    let maxPosition = -1;
    for (let k of Object.keys(packetSkeleton)) {
      if (packetSkeleton[k] instanceof DebuggerClient.Argument) {
        let { position } = packetSkeleton[k];
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
        let { from } = response;
        response = after.call(this, response);
        if (!response.from) {
          response.from = from;
        }
      }

      // The callback is always the last parameter.
      let thisCallback = args[maxPosition + 1];
      if (thisCallback) {
        thisCallback(response);
      }
    }, "DebuggerClient.requester request callback"));
  }, "DebuggerClient.requester");
};

function arg(pos) {
  return new DebuggerClient.Argument(pos);
}

DebuggerClient.Argument = function (position) {
  this.position = position;
};

DebuggerClient.Argument.prototype.getArgument = function (params) {
  if (!(this.position in params)) {
    throw new Error("Bad index into params: " + this.position);
  }
  return params[this.position];
};

// Expose these to save callers the trouble of importing DebuggerSocket
DebuggerClient.socketConnect = function (options) {
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
  connect: function (onConnected) {
    let deferred = promise.defer();
    this.emit("connect");

    // Also emit the event on the |DebuggerClient| object (not on the instance),
    // so it's possible to track all instances.
    events.emit(DebuggerClient, "connect", this);

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
  close: function (onClosed) {
    let deferred = promise.defer();
    if (onClosed) {
      deferred.promise.then(onClosed);
    }

    // Disable detach event notifications, because event handlers will be in a
    // cleared scope by the time they run.
    this._eventsEnabled = false;

    let cleanup = () => {
      this._transport.close();
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
    let clients = [...this._clients.values()];
    this._clients.clear();
    const detachClients = () => {
      let client = clients.pop();
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

  /*
   * This function exists only to preserve DebuggerClient's interface;
   * new code should say 'client.mainRoot.listTabs()'.
   */
  listTabs: function (onResponse) {
    return this.mainRoot.listTabs(onResponse);
  },

  /*
   * This function exists only to preserve DebuggerClient's interface;
   * new code should say 'client.mainRoot.listAddons()'.
   */
  listAddons: function (onResponse) {
    return this.mainRoot.listAddons(onResponse);
  },

  getTab: function (filter) {
    return this.mainRoot.getTab(filter);
  },

  /**
   * Attach to a tab actor.
   *
   * @param string tabActor
   *        The actor ID for the tab to attach.
   * @param function onResponse
   *        Called with the response packet and a TabClient
   *        (which will be undefined on error).
   */
  attachTab: function (tabActor, onResponse = noop) {
    if (this._clients.has(tabActor)) {
      let cachedTab = this._clients.get(tabActor);
      let cachedResponse = {
        cacheDisabled: cachedTab.cacheDisabled,
        javascriptEnabled: cachedTab.javascriptEnabled,
        traits: cachedTab.traits,
      };
      DevToolsUtils.executeSoon(() => onResponse(cachedResponse, cachedTab));
      return promise.resolve([cachedResponse, cachedTab]);
    }

    let packet = {
      to: tabActor,
      type: "attach"
    };
    return this.request(packet).then(response => {
      let tabClient;
      if (!response.error) {
        tabClient = new TabClient(this, response);
        this.registerClient(tabClient);
      }
      onResponse(response, tabClient);
      return [response, tabClient];
    });
  },

  attachWorker: function (workerActor, onResponse = noop) {
    let workerClient = this._clients.get(workerActor);
    if (workerClient !== undefined) {
      let response = {
        from: workerClient.actor,
        type: "attached",
        url: workerClient.url
      };
      DevToolsUtils.executeSoon(() => onResponse(response, workerClient));
      return promise.resolve([response, workerClient]);
    }

    return this.request({ to: workerActor, type: "attach" }).then(response => {
      if (response.error) {
        onResponse(response, null);
        return [response, null];
      }

      workerClient = new WorkerClient(this, response);
      this.registerClient(workerClient);
      onResponse(response, workerClient);
      return [response, workerClient];
    });
  },

  /**
   * Attach to an addon actor.
   *
   * @param string addonActor
   *        The actor ID for the addon to attach.
   * @param function onResponse
   *        Called with the response packet and a AddonClient
   *        (which will be undefined on error).
   */
  attachAddon: function (addonActor, onResponse = noop) {
    let packet = {
      to: addonActor,
      type: "attach"
    };
    return this.request(packet).then(response => {
      let addonClient;
      if (!response.error) {
        addonClient = new AddonClient(this, addonActor);
        this.registerClient(addonClient);
        this.activeAddon = addonClient;
      }
      onResponse(response, addonClient);
      return [response, addonClient];
    });
  },

  /**
   * Attach to a Web Console actor.
   *
   * @param string consoleActor
   *        The ID for the console actor to attach to.
   * @param array listeners
   *        The console listeners you want to start.
   * @param function onResponse
   *        Called with the response packet and a WebConsoleClient
   *        instance (which will be undefined on error).
   */
  attachConsole:
  function (consoleActor, listeners, onResponse = noop) {
    let packet = {
      to: consoleActor,
      type: "startListeners",
      listeners: listeners,
    };

    return this.request(packet).then(response => {
      let consoleClient;
      if (!response.error) {
        if (this._clients.has(consoleActor)) {
          consoleClient = this._clients.get(consoleActor);
        } else {
          consoleClient = new WebConsoleClient(this, response);
          this.registerClient(consoleClient);
        }
      }
      onResponse(response, consoleClient);
      return [response, consoleClient];
    });
  },

  /**
   * Attach to a global-scoped thread actor for chrome debugging.
   *
   * @param string threadActor
   *        The actor ID for the thread to attach.
   * @param function onResponse
   *        Called with the response packet and a ThreadClient
   *        (which will be undefined on error).
   * @param object options
   *        Configuration options.
   *        - useSourceMaps: whether to use source maps or not.
   */
  attachThread: function (threadActor, onResponse = noop, options = {}) {
    if (this._clients.has(threadActor)) {
      let client = this._clients.get(threadActor);
      DevToolsUtils.executeSoon(() => onResponse({}, client));
      return promise.resolve([{}, client]);
    }

    let packet = {
      to: threadActor,
      type: "attach",
      options,
    };
    return this.request(packet).then(response => {
      let threadClient;
      if (!response.error) {
        threadClient = new ThreadClient(this, threadActor);
        this.registerClient(threadClient);
      }
      onResponse(response, threadClient);
      return [response, threadClient];
    });
  },

  /**
   * Attach to a trace actor.
   *
   * @param string traceActor
   *        The actor ID for the tracer to attach.
   * @param function onResponse
   *        Called with the response packet and a TraceClient
   *        (which will be undefined on error).
   */
  attachTracer: function (traceActor, onResponse = noop) {
    if (this._clients.has(traceActor)) {
      let client = this._clients.get(traceActor);
      DevToolsUtils.executeSoon(() => onResponse({}, client));
      return promise.resolve([{}, client]);
    }

    let packet = {
      to: traceActor,
      type: "attach"
    };
    return this.request(packet).then(response => {
      let traceClient;
      if (!response.error) {
        traceClient = new TraceClient(this, traceActor);
        this.registerClient(traceClient);
      }
      onResponse(response, traceClient);
      return [response, traceClient];
    });
  },

  /**
   * Fetch the ChromeActor for the main process or ChildProcessActor for a
   * a given child process ID.
   *
   * @param number id
   *        The ID for the process to attach (returned by `listProcesses`).
   *        Connected to the main process if omitted, or is 0.
   */
  getProcess: function (id) {
    let packet = {
      to: "root",
      type: "getProcess"
    };
    if (typeof (id) == "number") {
      packet.id = id;
    }
    return this.request(packet);
  },

  /**
   * Release an object actor.
   *
   * @param string actor
   *        The actor ID to send the request to.
   * @param onResponse function
   *        If specified, will be called with the response packet when
   *        debugging server responds.
   */
  release: DebuggerClient.requester({
    to: arg(0),
    type: "release"
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
  request: function (packet, onResponse) {
    if (!this.mainRoot) {
      throw Error("Have not yet received a hello packet from the server.");
    }
    let type = packet.type || "";
    if (!packet.to) {
      throw Error("'" + type + "' request packet has no destination.");
    }
    if (this._closed) {
      let msg = "'" + type + "' request packet to " +
                "'" + packet.to + "' " +
               "can't be sent as the connection is closed.";
      let resp = { error: "connectionClosed", message: msg };
      if (onResponse) {
        onResponse(resp);
      }
      return promise.reject(resp);
    }

    let request = new Request(packet);
    request.format = "json";
    request.stack = getStack();
    if (onResponse) {
      request.on("json-reply", onResponse);
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
  startBulkRequest: function (request) {
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
   * Arrange to hand the next reply from |actor| to the handler bound to
   * |request|.
   *
   * DebuggerClient.prototype.request / startBulkRequest usually takes care of
   * establishing the handler for a given request, but in rare cases (well,
   * greetings from new root actors, is the only case at the moment) we must be
   * prepared for a "reply" that doesn't correspond to any request we sent.
   */
  expectReply: function (actor, request) {
    if (this._activeRequests.has(actor)) {
      throw Error("clashing handlers for next reply from " + actor);
    }

    // If a handler is passed directly (as it is with the handler for the root
    // actor greeting), create a dummy request to bind this to.
    if (typeof request === "function") {
      let handler = request;
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
  onPacket: function (packet) {
    if (!packet.from) {
      DevToolsUtils.reportException(
        "onPacket",
        new Error("Server did not specify an actor, dropping packet: " +
                  JSON.stringify(packet)));
      return;
    }

    // If we have a registered Front for this actor, let it handle the packet
    // and skip all the rest of this unpleasantness.
    let front = this.getActor(packet.from);
    if (front) {
      front.onPacket(packet);
      return;
    }

    // Check for "forwardingCancelled" here instead of using a client to handle it.
    // This is necessary because we might receive this event while the client is closing,
    // and the clients have already been removed by that point.
    if (this.mainRoot &&
        packet.from == this.mainRoot.actor &&
        packet.type == "forwardingCancelled") {
      this.purgeRequests(packet.prefix);
      return;
    }

    if (this._clients.has(packet.from) && packet.type) {
      let client = this._clients.get(packet.from);
      let type = packet.type;
      if (client.events.indexOf(type) != -1) {
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

    // TODO: Bug 1151156 - Remove once Gecko 40 is on b2g-stable.
    if (!this.traits.noNeedToFakeResumptionOnNavigation) {
      // On navigation the server resumes, so the client must resume as well.
      // We achieve that by generating a fake resumption packet that triggers
      // the client's thread state change listeners.
      if (packet.type == UnsolicitedNotifications.tabNavigated &&
          this._clients.has(packet.from) &&
          this._clients.get(packet.from).thread) {
        let thread = this._clients.get(packet.from).thread;
        let resumption = { from: thread._actor, type: "resumed" };
        thread._onThreadState(resumption);
      }
    }

    // Only try to notify listeners on events, not responses to requests
    // that lack a packet type.
    if (packet.type) {
      this.emit(packet.type, packet);
    }

    if (activeRequest) {
      let emitReply = () => activeRequest.emit("json-reply", packet);
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
  onBulkPacket: function (packet) {
    let { actor } = packet;

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
   * @param status nsresult
   *        The status code that corresponds to the reason for closing
   *        the stream.
   */
  onClosed: function () {
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
    for (let pool of this._pools) {
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
    let reject = function (type, request) {
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
      let packet = { error: "connectionClosed", message: msg };
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
    let fronts = new Set();
    let poolsToVisit = [...this._pools];

    // With protocol.js, each front can potentially have it's own pools containing child
    // fronts, forming a tree.  Descend through all the pools to locate all child fronts.
    while (poolsToVisit.length) {
      let pool = poolsToVisit.shift();
      fronts.add(pool);
      for (let child of pool.poolChildren()) {
        poolsToVisit.push(child);
      }
    }

    // For each front, wait for its requests to settle
    for (let front of fronts) {
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
      if (pool.has(actorID)) {
        return pool;
      }
    }
    return null;
  },

  /**
   * Currently attached addon.
   */
  activeAddon: null
};

eventSource(DebuggerClient.prototype);

function Request(request) {
  this.request = request;
}

Request.prototype = {

  on: function (type, listener) {
    events.on(this, type, listener);
  },

  off: function (type, listener) {
    events.off(this, type, listener);
  },

  once: function (type, listener) {
    events.once(this, type, listener);
  },

  emit: function (type, ...args) {
    events.emit(this, type, ...args);
  },

  get actor() {
    return this.request.to || this.request.actor;
  }

};

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
  attachThread: function (options = {}, onResponse = noop) {
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
    before: function (packet) {
      if (this.thread) {
        this.thread.detach();
      }
      return packet;
    },
    after: function (response) {
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
   * Reload the page in this tab.
   *
   * @param [optional] object options
   *        An object with a `force` property indicating whether or not
   *        this reload should skip the cache
   */
  reload: function (options = { force: false }) {
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

  attachWorker: function (workerActor, onResponse) {
    return this.client.attachWorker(workerActor, onResponse);
  },
};

eventSource(TabClient.prototype);

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
    after: function (response) {
      if (this.thread) {
        this.client.unregisterClient(this.thread);
      }
      this.client.unregisterClient(this);
      return response;
    },
  }),

  attachThread: function (options = {}, onResponse = noop) {
    if (this.thread) {
      let response = [{
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

  _onClose: function () {
    this.removeListener("close", this._onClose);

    if (this.thread) {
      this.client.unregisterClient(this.thread);
    }
    this.client.unregisterClient(this);
    this._isClosed = true;
  },

  reconfigure: function () {
    return Promise.resolve();
  },

  events: ["close"]
};

eventSource(WorkerClient.prototype);

function AddonClient(client, actor) {
  this._client = client;
  this._actor = actor;
  this.request = this._client.request;
  this.events = [];
}

AddonClient.prototype = {
  get actor() {
    return this._actor;
  },
  get _transport() {
    return this._client._transport;
  },

  /**
   * Detach the client from the addon actor.
   *
   * @param function onResponse
   *        Called with the response packet.
   */
  detach: DebuggerClient.requester({
    type: "detach"
  }, {
    after: function (response) {
      if (this._client.activeAddon === this) {
        this._client.activeAddon = null;
      }
      this._client.unregisterClient(this);
      return response;
    },
  })
};

/**
 * A RootClient object represents a root actor on the server. Each
 * DebuggerClient keeps a RootClient instance representing the root actor
 * for the initial connection; DebuggerClient's 'listTabs' and
 * 'listChildProcesses' methods forward to that root actor.
 *
 * @param client object
 *      The client connection to which this actor belongs.
 * @param greeting string
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
function RootClient(client, greeting) {
  this._client = client;
  this.actor = greeting.from;
  this.applicationType = greeting.applicationType;
  this.traits = greeting.traits;
}
exports.RootClient = RootClient;

RootClient.prototype = {
  constructor: RootClient,

  /**
   * Gets the "root" form, which lists all the global actors that affect the entire
   * browser.  This can replace usages of `listTabs` that only wanted the global actors
   * and didn't actually care about tabs.
   */
  getRoot: DebuggerClient.requester({ type: "getRoot" }),

   /**
   * List the open tabs.
   *
   * @param function onResponse
   *        Called with the response packet.
   */
  listTabs: DebuggerClient.requester({ type: "listTabs" }),

  /**
   * List the installed addons.
   *
   * @param function onResponse
   *        Called with the response packet.
   */
  listAddons: DebuggerClient.requester({ type: "listAddons" }),

  /**
   * List the registered workers.
   *
   * @param function onResponse
   *        Called with the response packet.
   */
  listWorkers: DebuggerClient.requester({ type: "listWorkers" }),

  /**
   * List the registered service workers.
   *
   * @param function onResponse
   *        Called with the response packet.
   */
  listServiceWorkerRegistrations: DebuggerClient.requester({
    type: "listServiceWorkerRegistrations"
  }),

  /**
   * List the running processes.
   *
   * @param function onResponse
   *        Called with the response packet.
   */
  listProcesses: DebuggerClient.requester({ type: "listProcesses" }),

  /**
   * Fetch the TabActor for the currently selected tab, or for a specific
   * tab given as first parameter.
   *
   * @param [optional] object filter
   *        A dictionary object with following optional attributes:
   *         - outerWindowID: used to match tabs in parent process
   *         - tabId: used to match tabs in child processes
   *         - tab: a reference to xul:tab element
   *        If nothing is specified, returns the actor for the currently
   *        selected tab.
   */
  getTab: function (filter) {
    let packet = {
      to: this.actor,
      type: "getTab"
    };

    if (filter) {
      if (typeof (filter.outerWindowID) == "number") {
        packet.outerWindowID = filter.outerWindowID;
      } else if (typeof (filter.tabId) == "number") {
        packet.tabId = filter.tabId;
      } else if ("tab" in filter) {
        let browser = filter.tab.linkedBrowser;
        if (browser.frameLoader.tabParent) {
          // Tabs in child process
          packet.tabId = browser.frameLoader.tabParent.tabId;
        } else if (browser.outerWindowID) {
          // <xul:browser> tabs in parent process
          packet.outerWindowID = browser.outerWindowID;
        } else {
          // <iframe mozbrowser> tabs in parent process
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
   * Fetch the WindowActor for a specific window, like a browser window in
   * Firefox, but it can be used to reach any window in the process.
   *
   * @param number outerWindowID
   *        The outerWindowID of the top level window you are looking for.
   */
  getWindow: function ({ outerWindowID }) {
    if (!outerWindowID) {
      throw new Error("Must specify outerWindowID");
    }

    let packet = {
      to: this.actor,
      type: "getWindow",
      outerWindowID,
    };

    return this.request(packet);
  },

  /**
   * Description of protocol's actors and methods.
   *
   * @param function onResponse
   *        Called with the response packet.
   */
  protocolDescription: DebuggerClient.requester({ type: "protocolDescription" }),

  /*
   * Methods constructed by DebuggerClient.requester require these forwards
   * on their 'this'.
   */
  get _transport() {
    return this._client._transport;
  },
  get request() {
    return this._client.request;
  }
};

/**
 * Creates a thread client for the remote debugging protocol server. This client
 * is a front to the thread actor created in the server side, hiding the
 * protocol details in a traditional JavaScript API.
 *
 * @param client DebuggerClient|TabClient
 *        The parent of the thread (tab for tab-scoped debuggers, DebuggerClient
 *        for chrome debuggers).
 * @param actor string
 *        The actor ID for this thread.
 */
function ThreadClient(client, actor) {
  this._parent = client;
  this.client = client instanceof DebuggerClient ? client : client.client;
  this._actor = actor;
  this._frameCache = [];
  this._scriptCache = {};
  this._pauseGrips = {};
  this._threadGrips = {};
  this.request = this.client.request;
}

ThreadClient.prototype = {
  _state: "paused",
  get state() {
    return this._state;
  },
  get paused() {
    return this._state === "paused";
  },

  _pauseOnExceptions: false,
  _ignoreCaughtExceptions: false,
  _pauseOnDOMEvents: null,

  _actor: null,
  get actor() {
    return this._actor;
  },

  get _transport() {
    return this.client._transport;
  },

  _assertPaused: function (command) {
    if (!this.paused) {
      throw Error(command + " command sent while not paused. Currently " + this._state);
    }
  },

  /**
   * Resume a paused thread. If the optional limit parameter is present, then
   * the thread will also pause when that limit is reached.
   *
   * @param [optional] object limit
   *        An object with a type property set to the appropriate limit (next,
   *        step, or finish) per the remote debugging protocol specification.
   *        Use null to specify no limit.
   * @param function onResponse
   *        Called with the response packet.
   */
  _doResume: DebuggerClient.requester({
    type: "resume",
    resumeLimit: arg(0)
  }, {
    before: function (packet) {
      this._assertPaused("resume");

      // Put the client in a tentative "resuming" state so we can prevent
      // further requests that should only be sent in the paused state.
      this._previousState = this._state;
      this._state = "resuming";

      if (this._pauseOnExceptions) {
        packet.pauseOnExceptions = this._pauseOnExceptions;
      }
      if (this._ignoreCaughtExceptions) {
        packet.ignoreCaughtExceptions = this._ignoreCaughtExceptions;
      }
      if (this._pauseOnDOMEvents) {
        packet.pauseOnDOMEvents = this._pauseOnDOMEvents;
      }
      return packet;
    },
    after: function (response) {
      if (response.error && this._state == "resuming") {
        // There was an error resuming, update the state to the new one
        // reported by the server, if given (only on wrongState), otherwise
        // reset back to the previous state.
        if (response.state) {
          this._state = ThreadStateTypes[response.state];
        } else {
          this._state = this._previousState;
        }
      }
      delete this._previousState;
      return response;
    },
  }),

  /**
   * Reconfigure the thread actor.
   *
   * @param object options
   *        A dictionary object of the new options to use in the thread actor.
   * @param function onResponse
   *        Called with the response packet.
   */
  reconfigure: DebuggerClient.requester({
    type: "reconfigure",
    options: arg(0)
  }),

  /**
   * Resume a paused thread.
   */
  resume: function (onResponse) {
    return this._doResume(null, onResponse);
  },

  /**
   * Resume then pause without stepping.
   *
   * @param function onResponse
   *        Called with the response packet.
   */
  resumeThenPause: function (onResponse) {
    return this._doResume({ type: "break" }, onResponse);
  },

  /**
   * Step over a function call.
   *
   * @param function onResponse
   *        Called with the response packet.
   */
  stepOver: function (onResponse) {
    return this._doResume({ type: "next" }, onResponse);
  },

  /**
   * Step into a function call.
   *
   * @param function onResponse
   *        Called with the response packet.
   */
  stepIn: function (onResponse) {
    return this._doResume({ type: "step" }, onResponse);
  },

  /**
   * Step out of a function call.
   *
   * @param function onResponse
   *        Called with the response packet.
   */
  stepOut: function (onResponse) {
    return this._doResume({ type: "finish" }, onResponse);
  },

  /**
   * Immediately interrupt a running thread.
   *
   * @param function onResponse
   *        Called with the response packet.
   */
  interrupt: function (onResponse) {
    return this._doInterrupt(null, onResponse);
  },

  /**
   * Pause execution right before the next JavaScript bytecode is executed.
   *
   * @param function onResponse
   *        Called with the response packet.
   */
  breakOnNext: function (onResponse) {
    return this._doInterrupt("onNext", onResponse);
  },

  /**
   * Interrupt a running thread.
   *
   * @param function onResponse
   *        Called with the response packet.
   */
  _doInterrupt: DebuggerClient.requester({
    type: "interrupt",
    when: arg(0)
  }),

  /**
   * Enable or disable pausing when an exception is thrown.
   *
   * @param boolean pauseOnExceptions
   *        Enables pausing if true, disables otherwise.
   * @param boolean ignoreCaughtExceptions
   *        Whether to ignore caught exceptions
   * @param function onResponse
   *        Called with the response packet.
   */
  pauseOnExceptions: function (pauseOnExceptions,
                               ignoreCaughtExceptions,
                               onResponse = noop) {
    this._pauseOnExceptions = pauseOnExceptions;
    this._ignoreCaughtExceptions = ignoreCaughtExceptions;

    // Otherwise send the flag using a standard resume request.
    if (!this.paused) {
      return this.interrupt(response => {
        if (response.error) {
          // Can't continue if pausing failed.
          onResponse(response);
          return response;
        }
        return this.resume(onResponse);
      });
    }

    onResponse();
    return promise.resolve();
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
      return {};
    }
    return this.interrupt(response => {
      // Can't continue if pausing failed.
      if (response.error) {
        onResponse(response);
        return response;
      }
      return this.resume(onResponse);
    });
  },

  /**
   * Send a clientEvaluate packet to the debuggee. Response
   * will be a resume packet.
   *
   * @param string frame
   *        The actor ID of the frame where the evaluation should take place.
   * @param string expression
   *        The expression that will be evaluated in the scope of the frame
   *        above.
   * @param function onResponse
   *        Called with the response packet.
   */
  eval: DebuggerClient.requester({
    type: "clientEvaluate",
    frame: arg(0),
    expression: arg(1)
  }, {
    before: function (packet) {
      this._assertPaused("eval");
      // Put the client in a tentative "resuming" state so we can prevent
      // further requests that should only be sent in the paused state.
      this._state = "resuming";
      return packet;
    },
    after: function (response) {
      if (response.error) {
        // There was an error resuming, back to paused state.
        this._state = "paused";
      }
      return response;
    },
  }),

  /**
   * Detach from the thread actor.
   *
   * @param function onResponse
   *        Called with the response packet.
   */
  detach: DebuggerClient.requester({
    type: "detach"
  }, {
    after: function (response) {
      this.client.unregisterClient(this);
      this._parent.thread = null;
      return response;
    },
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
    actors: arg(0),
  }),

  /**
   * Promote multiple pause-lifetime object actors to thread-lifetime ones.
   *
   * @param array actors
   *        An array with actor IDs to promote.
   */
  threadGrips: DebuggerClient.requester({
    type: "threadGrips",
    actors: arg(0)
  }),

  /**
   * Return the event listeners defined on the page.
   *
   * @param onResponse Function
   *        Called with the thread's response.
   */
  eventListeners: DebuggerClient.requester({
    type: "eventListeners"
  }),

  /**
   * Request the loaded sources for the current thread.
   *
   * @param onResponse Function
   *        Called with the thread's response.
   */
  getSources: DebuggerClient.requester({
    type: "sources"
  }),

  /**
   * Clear the thread's source script cache. A scriptscleared event
   * will be sent.
   */
  _clearScripts: function () {
    if (Object.keys(this._scriptCache).length > 0) {
      this._scriptCache = {};
      this.emit("scriptscleared");
    }
  },

  /**
   * Request frames from the callstack for the current thread.
   *
   * @param start integer
   *        The number of the youngest stack frame to return (the youngest
   *        frame is 0).
   * @param count integer
   *        The maximum number of frames to return, or null to return all
   *        frames.
   * @param onResponse function
   *        Called with the thread's response.
   */
  getFrames: DebuggerClient.requester({
    type: "frames",
    start: arg(0),
    count: arg(1)
  }),

  /**
   * An array of cached frames. Clients can observe the framesadded and
   * framescleared event to keep up to date on changes to this cache,
   * and can fill it using the fillFrames method.
   */
  get cachedFrames() {
    return this._frameCache;
  },

  /**
   * true if there are more stack frames available on the server.
   */
  get moreFrames() {
    return this.paused && (!this._frameCache || this._frameCache.length == 0
          || !this._frameCache[this._frameCache.length - 1].oldest);
  },

  /**
   * Ensure that at least total stack frames have been loaded in the
   * ThreadClient's stack frame cache. A framesadded event will be
   * sent when the stack frame cache is updated.
   *
   * @param total number
   *        The minimum number of stack frames to be included.
   * @param callback function
   *        Optional callback function called when frames have been loaded
   * @returns true if a framesadded notification should be expected.
   */
  fillFrames: function (total, callback = noop) {
    this._assertPaused("fillFrames");
    if (this._frameCache.length >= total) {
      return false;
    }

    let numFrames = this._frameCache.length;

    this.getFrames(numFrames, total - numFrames, (response) => {
      if (response.error) {
        callback(response);
        return;
      }

      let threadGrips = DevToolsUtils.values(this._threadGrips);

      for (let i in response.frames) {
        let frame = response.frames[i];
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

      callback(response);
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
   * @param grip object
   *        A pause-lifetime object grip returned by the protocol.
   */
  pauseGrip: function (grip) {
    if (grip.actor in this._pauseGrips) {
      return this._pauseGrips[grip.actor];
    }

    let client = new ObjectClient(this.client, grip);
    this._pauseGrips[grip.actor] = client;
    return client;
  },

  /**
   * Get or create a long string client, checking the grip client cache if it
   * already exists.
   *
   * @param grip Object
   *        The long string grip returned by the protocol.
   * @param gripCacheName String
   *        The property name of the grip client cache to check for existing
   *        clients in.
   */
  _longString: function (grip, gripCacheName) {
    if (grip.actor in this[gripCacheName]) {
      return this[gripCacheName][grip.actor];
    }

    let client = new LongStringClient(this.client, grip);
    this[gripCacheName][grip.actor] = client;
    return client;
  },

  /**
   * Return an instance of LongStringClient for the given long string grip that
   * is scoped to the current pause.
   *
   * @param grip Object
   *        The long string grip returned by the protocol.
   */
  pauseLongString: function (grip) {
    return this._longString(grip, "_pauseGrips");
  },

  /**
   * Return an instance of LongStringClient for the given long string grip that
   * is scoped to the thread lifetime.
   *
   * @param grip Object
   *        The long string grip returned by the protocol.
   */
  threadLongString: function (grip) {
    return this._longString(grip, "_threadGrips");
  },

  /**
   * Clear and invalidate all the grip clients from the given cache.
   *
   * @param gripCacheName
   *        The property name of the grip cache we want to clear.
   */
  _clearObjectClients: function (gripCacheName) {
    for (let id in this[gripCacheName]) {
      this[gripCacheName][id].valid = false;
    }
    this[gripCacheName] = {};
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
  _onThreadState: function (packet) {
    this._state = ThreadStateTypes[packet.type];
    // The debugger UI may not be initialized yet so we want to keep
    // the packet around so it knows what to pause state to display
    // when it's initialized
    this._lastPausePacket = packet.type === "resumed" ? null : packet;
    this._clearFrames();
    this._clearPauseGrips();
    packet.type === ThreadStateTypes.detached && this._clearThreadGrips();
    this.client._eventsEnabled && this.emit(packet.type, packet);
  },

  getLastPausePacket: function () {
    return this._lastPausePacket;
  },

  /**
   * Return an EnvironmentClient instance for the given environment actor form.
   */
  environment: function (form) {
    return new EnvironmentClient(this.client, form);
  },

  /**
   * Return an instance of SourceClient for the given source actor form.
   */
  source: function (form) {
    if (form.actor in this._threadGrips) {
      return this._threadGrips[form.actor];
    }

    this._threadGrips[form.actor] = new SourceClient(this, form);
    return this._threadGrips[form.actor];
  },

  /**
   * Request the prototype and own properties of mutlipleObjects.
   *
   * @param onResponse function
   *        Called with the request's response.
   * @param actors [string]
   *        List of actor ID of the queried objects.
   */
  getPrototypesAndProperties: DebuggerClient.requester({
    type: "prototypesAndProperties",
    actors: arg(0)
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
 * @param client DebuggerClient
 *        The debugger client parent.
 * @param actor string
 *        The actor ID for this thread.
 */
function TraceClient(client, actor) {
  this._client = client;
  this._actor = actor;
  this._activeTraces = new Set();
  this._waitingPackets = new Map();
  this._expectedPacket = 0;
  this.request = this._client.request;
  this.events = [];
}

TraceClient.prototype = {
  get actor() {
    return this._actor;
  },
  get tracing() {
    return this._activeTraces.size > 0;
  },

  get _transport() {
    return this._client._transport;
  },

  /**
   * Detach from the trace actor.
   */
  detach: DebuggerClient.requester({
    type: "detach"
  }, {
    after: function (response) {
      this._client.unregisterClient(this);
      return response;
    },
  }),

  /**
   * Start a new trace.
   *
   * @param trace [string]
   *        An array of trace types to be recorded by the new trace.
   *
   * @param name string
   *        The name of the new trace.
   *
   * @param onResponse function
   *        Called with the request's response.
   */
  startTrace: DebuggerClient.requester({
    type: "startTrace",
    name: arg(1),
    trace: arg(0)
  }, {
    after: function (response) {
      if (response.error) {
        return response;
      }

      if (!this.tracing) {
        this._waitingPackets.clear();
        this._expectedPacket = 0;
      }
      this._activeTraces.add(response.name);

      return response;
    },
  }),

  /**
   * End a trace. If a name is provided, stop the named
   * trace. Otherwise, stop the most recently started trace.
   *
   * @param name string
   *        The name of the trace to stop.
   *
   * @param onResponse function
   *        Called with the request's response.
   */
  stopTrace: DebuggerClient.requester({
    type: "stopTrace",
    name: arg(0)
  }, {
    after: function (response) {
      if (response.error) {
        return response;
      }

      this._activeTraces.delete(response.name);

      return response;
    },
  })
};

/**
 * Grip clients are used to retrieve information about the relevant object.
 *
 * @param client DebuggerClient
 *        The debugger client parent.
 * @param grip object
 *        A pause-lifetime object grip returned by the protocol.
 */
function ObjectClient(client, grip) {
  this._grip = grip;
  this._client = client;
  this.request = this._client.request;
}
exports.ObjectClient = ObjectClient;

ObjectClient.prototype = {
  get actor() {
    return this._grip.actor;
  },
  get _transport() {
    return this._client._transport;
  },

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
    before: function (packet) {
      if (this._grip.class != "Function") {
        throw new Error("getDefinitionSite is only valid for function grips.");
      }
      return packet;
    }
  }),

  /**
   * Request the names of a function's formal parameters.
   *
   * @param onResponse function
   *        Called with an object of the form:
   *        { parameterNames:[<parameterName>, ...] }
   *        where each <parameterName> is the name of a parameter.
   */
  getParameterNames: DebuggerClient.requester({
    type: "parameterNames"
  }, {
    before: function (packet) {
      if (this._grip.class !== "Function") {
        throw new Error("getParameterNames is only valid for function grips.");
      }
      return packet;
    },
  }),

  /**
   * Request the names of the properties defined on the object and not its
   * prototype.
   *
   * @param onResponse function Called with the request's response.
   */
  getOwnPropertyNames: DebuggerClient.requester({
    type: "ownPropertyNames"
  }),

  /**
   * Request the prototype and own properties of the object.
   *
   * @param onResponse function Called with the request's response.
   */
  getPrototypeAndProperties: DebuggerClient.requester({
    type: "prototypeAndProperties"
  }),

  /**
   * Request a PropertyIteratorClient instance to ease listing
   * properties for this object.
   *
   * @param options Object
   *        A dictionary object with various boolean attributes:
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
   * @param onResponse function Called with the client instance.
   */
  enumProperties: DebuggerClient.requester({
    type: "enumProperties",
    options: arg(0)
  }, {
    after: function (response) {
      if (response.iterator) {
        return { iterator: new PropertyIteratorClient(this._client, response.iterator) };
      }
      return response;
    },
  }),

  /**
   * Request a PropertyIteratorClient instance to enumerate entries in a
   * Map/Set-like object.
   *
   * @param onResponse function Called with the request's response.
   */
  enumEntries: DebuggerClient.requester({
    type: "enumEntries"
  }, {
    before: function (packet) {
      if (!["Map", "WeakMap", "Set", "WeakSet"].includes(this._grip.class)) {
        throw new Error("enumEntries is only valid for Map/Set-like grips.");
      }
      return packet;
    },
    after: function (response) {
      if (response.iterator) {
        return {
          iterator: new PropertyIteratorClient(this._client, response.iterator)
        };
      }
      return response;
    }
  }),

  /**
   * Request the property descriptor of the object's specified property.
   *
   * @param name string The name of the requested property.
   * @param onResponse function Called with the request's response.
   */
  getProperty: DebuggerClient.requester({
    type: "property",
    name: arg(0)
  }),

  /**
   * Request the prototype of the object.
   *
   * @param onResponse function Called with the request's response.
   */
  getPrototype: DebuggerClient.requester({
    type: "prototype"
  }),

  /**
   * Request the display string of the object.
   *
   * @param onResponse function Called with the request's response.
   */
  getDisplayString: DebuggerClient.requester({
    type: "displayString"
  }),

  /**
   * Request the scope of the object.
   *
   * @param onResponse function Called with the request's response.
   */
  getScope: DebuggerClient.requester({
    type: "scope"
  }, {
    before: function (packet) {
      if (this._grip.class !== "Function") {
        throw new Error("scope is only valid for function grips.");
      }
      return packet;
    },
  }),

  /**
   * Request the promises directly depending on the current promise.
   */
  getDependentPromises: DebuggerClient.requester({
    type: "dependentPromises"
  }, {
    before: function (packet) {
      if (this._grip.class !== "Promise") {
        throw new Error("getDependentPromises is only valid for promise " +
          "grips.");
      }
      return packet;
    }
  }),

  /**
   * Request the stack to the promise's allocation point.
   */
  getPromiseAllocationStack: DebuggerClient.requester({
    type: "allocationStack"
  }, {
    before: function (packet) {
      if (this._grip.class !== "Promise") {
        throw new Error("getAllocationStack is only valid for promise grips.");
      }
      return packet;
    }
  }),

  /**
   * Request the stack to the promise's fulfillment point.
   */
  getPromiseFulfillmentStack: DebuggerClient.requester({
    type: "fulfillmentStack"
  }, {
    before: function (packet) {
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
    before: function (packet) {
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
 * @param client DebuggerClient
 *        The debugger client parent.
 * @param grip Object
 *        A PropertyIteratorActor grip returned by the protocol via
 *        TabActor.enumProperties request.
 */
function PropertyIteratorClient(client, grip) {
  this._grip = grip;
  this._client = client;
  this.request = this._client.request;
}

PropertyIteratorClient.prototype = {
  get actor() {
    return this._grip.actor;
  },

  /**
   * Get the total number of properties available in the iterator.
   */
  get count() {
    return this._grip.count;
  },

  /**
   * Get one or more property names that correspond to the positions in the
   * indexes parameter.
   *
   * @param indexes Array
   *        An array of property indexes.
   * @param callback Function
   *        The function called when we receive the property names.
   */
  names: DebuggerClient.requester({
    type: "names",
    indexes: arg(0)
  }, {}),

  /**
   * Get a set of following property value(s).
   *
   * @param start Number
   *        The index of the first property to fetch.
   * @param count Number
   *        The number of properties to fetch.
   * @param callback Function
   *        The function called when we receive the property values.
   */
  slice: DebuggerClient.requester({
    type: "slice",
    start: arg(0),
    count: arg(1)
  }, {}),

  /**
   * Get all the property values.
   *
   * @param callback Function
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
 * @param client DebuggerClient
 *        The debugger client parent.
 * @param grip Object
 *        A pause-lifetime long string grip returned by the protocol.
 */
function LongStringClient(client, grip) {
  this._grip = grip;
  this._client = client;
  this.request = this._client.request;
}
exports.LongStringClient = LongStringClient;

LongStringClient.prototype = {
  get actor() {
    return this._grip.actor;
  },
  get length() {
    return this._grip.length;
  },
  get initial() {
    return this._grip.initial;
  },
  get _transport() {
    return this._client._transport;
  },

  valid: true,

  /**
   * Get the substring of this LongString from start to end.
   *
   * @param start Number
   *        The starting index.
   * @param end Number
   *        The ending index.
   * @param callback Function
   *        The function called when we receive the substring.
   */
  substring: DebuggerClient.requester({
    type: "substring",
    start: arg(0),
    end: arg(1)
  }),
};

/**
 * A SourceClient provides a way to access the source text of a script.
 *
 * @param client ThreadClient
 *        The thread client parent.
 * @param form Object
 *        The form sent across the remote debugging protocol.
 */
function SourceClient(client, form) {
  this._form = form;
  this._isBlackBoxed = form.isBlackBoxed;
  this._isPrettyPrinted = form.isPrettyPrinted;
  this._activeThread = client;
  this._client = client.client;
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
   * @param callback Function
   *        The callback function called when we receive the response from the server.
   */
  blackBox: DebuggerClient.requester({
    type: "blackbox"
  }, {
    after: function (response) {
      if (!response.error) {
        this._isBlackBoxed = true;
        if (this._activeThread) {
          this._activeThread.emit("blackboxchange", this);
        }
      }
      return response;
    }
  }),

  /**
   * Un-black box this SourceClient's source.
   *
   * @param callback Function
   *        The callback function called when we receive the response from the server.
   */
  unblackBox: DebuggerClient.requester({
    type: "unblackbox"
  }, {
    after: function (response) {
      if (!response.error) {
        this._isBlackBoxed = false;
        if (this._activeThread) {
          this._activeThread.emit("blackboxchange", this);
        }
      }
      return response;
    }
  }),

  /**
   * Get Executable Lines from a source
   *
   * @param callback Function
   *        The callback function called when we receive the response from the server.
   */
  getExecutableLines: function (cb = noop) {
    let packet = {
      to: this._form.actor,
      type: "getExecutableLines"
    };

    return this._client.request(packet).then(res => {
      cb(res.lines);
      return res.lines;
    });
  },

  /**
   * Get a long string grip for this SourceClient's source.
   */
  source: function (callback = noop) {
    let packet = {
      to: this._form.actor,
      type: "source"
    };
    return this._client.request(packet).then(response => {
      return this._onSourceResponse(response, callback);
    });
  },

  /**
   * Pretty print this source's text.
   */
  prettyPrint: function (indent, callback = noop) {
    const packet = {
      to: this._form.actor,
      type: "prettyPrint",
      indent
    };
    return this._client.request(packet).then(response => {
      if (!response.error) {
        this._isPrettyPrinted = true;
        this._activeThread._clearFrames();
        this._activeThread.emit("prettyprintchange", this);
      }
      return this._onSourceResponse(response, callback);
    });
  },

  /**
   * Stop pretty printing this source's text.
   */
  disablePrettyPrint: function (callback = noop) {
    const packet = {
      to: this._form.actor,
      type: "disablePrettyPrint"
    };
    return this._client.request(packet).then(response => {
      if (!response.error) {
        this._isPrettyPrinted = false;
        this._activeThread._clearFrames();
        this._activeThread.emit("prettyprintchange", this);
      }
      return this._onSourceResponse(response, callback);
    });
  },

  _onSourceResponse: function (response, callback) {
    if (response.error) {
      callback(response);
      return response;
    }

    if (typeof response.source === "string") {
      callback(response);
      return response;
    }

    let { contentType, source } = response;
    let longString = this._activeThread.threadLongString(source);
    return longString.substring(0, longString.length).then(function (resp) {
      if (resp.error) {
        callback(resp);
        return resp;
      }

      let newResponse = {
        source: resp.substring,
        contentType: contentType
      };
      callback(newResponse);
      return newResponse;
    });
  },

  /**
   * Request to set a breakpoint in the specified location.
   *
   * @param object location
   *        The location and condition of the breakpoint in
   *        the form of { line[, column, condition] }.
   * @param function onResponse
   *        Called with the thread's response.
   */
  setBreakpoint: function ({ line, column, condition, noSliding }, onResponse = noop) {
    // A helper function that sets the breakpoint.
    let doSetBreakpoint = callback => {
      let root = this._client.mainRoot;
      let location = {
        line,
        column,
      };

      let packet = {
        to: this.actor,
        type: "setBreakpoint",
        location,
        condition,
        noSliding,
      };

      // Backwards compatibility: send the breakpoint request to the
      // thread if the server doesn't support Debugger.Source actors.
      if (!root.traits.debuggerSourceActors) {
        packet.to = this._activeThread.actor;
        packet.location.url = this.url;
      }

      return this._client.request(packet).then(response => {
        // Ignoring errors, since the user may be setting a breakpoint in a
        // dead script that will reappear on a page reload.
        let bpClient;
        if (response.actor) {
          bpClient = new BreakpointClient(
            this._client,
            this,
            response.actor,
            location,
            root.traits.conditionalBreakpoints ? condition : undefined
          );
        }
        onResponse(response, bpClient);
        if (callback) {
          callback();
        }
        return [response, bpClient];
      });
    };

    // If the debuggee is paused, just set the breakpoint.
    if (this._activeThread.paused) {
      return doSetBreakpoint();
    }
    // Otherwise, force a pause in order to set the breakpoint.
    return this._activeThread.interrupt().then(response => {
      if (response.error) {
        // Can't set the breakpoint if pausing failed.
        onResponse(response);
        return response;
      }

      const { type, why } = response;
      const cleanUp = type == "paused" && why.type == "interrupted"
            ? () => this._activeThread.resume()
            : noop;

      return doSetBreakpoint(cleanUp);
    });
  }
};

/**
 * Breakpoint clients are used to remove breakpoints that are no longer used.
 *
 * @param client DebuggerClient
 *        The debugger client parent.
 * @param sourceClient SourceClient
 *        The source where this breakpoint exists
 * @param actor string
 *        The actor ID for this breakpoint.
 * @param location object
 *        The location of the breakpoint. This is an object with two properties:
 *        url and line.
 * @param condition string
 *        The conditional expression of the breakpoint
 */
function BreakpointClient(client, sourceClient, actor, location, condition) {
  this._client = client;
  this._actor = actor;
  this.location = location;
  this.location.actor = sourceClient.actor;
  this.location.url = sourceClient.url;
  this.source = sourceClient;
  this.request = this._client.request;

  // The condition property should only exist if it's a truthy value
  if (condition) {
    this.condition = condition;
  }
}

BreakpointClient.prototype = {

  _actor: null,
  get actor() {
    return this._actor;
  },
  get _transport() {
    return this._client._transport;
  },

  /**
   * Remove the breakpoint from the server.
   */
  remove: DebuggerClient.requester({
    type: "delete"
  }),

  /**
   * Determines if this breakpoint has a condition
   */
  hasCondition: function () {
    let root = this._client.mainRoot;
    // XXX bug 990137: We will remove support for client-side handling of
    // conditional breakpoints
    if (root.traits.conditionalBreakpoints) {
      return "condition" in this;
    }
    return "conditionalExpression" in this;
  },

  /**
   * Get the condition of this breakpoint. Currently we have to
   * support locally emulated conditional breakpoints until the
   * debugger servers are updated (see bug 990137). We used a
   * different property when moving it server-side to ensure that we
   * are testing the right code.
   */
  getCondition: function () {
    let root = this._client.mainRoot;
    if (root.traits.conditionalBreakpoints) {
      return this.condition;
    }
    return this.conditionalExpression;
  },

  /**
   * Set the condition of this breakpoint
   */
  setCondition: function (gThreadClient, condition) {
    let root = this._client.mainRoot;
    let deferred = promise.defer();

    if (root.traits.conditionalBreakpoints) {
      let info = {
        line: this.location.line,
        column: this.location.column,
        condition: condition
      };

      // Remove the current breakpoint and add a new one with the
      // condition.
      this.remove(response => {
        if (response && response.error) {
          deferred.reject(response);
          return;
        }

        this.source.setBreakpoint(info, (resp, newBreakpoint) => {
          if (resp && resp.error) {
            deferred.reject(resp);
          } else {
            deferred.resolve(newBreakpoint);
          }
        });
      });
    } else {
      // The property shouldn't even exist if the condition is blank
      if (condition === "") {
        delete this.conditionalExpression;
      } else {
        this.conditionalExpression = condition;
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
 * @param client DebuggerClient
 *        The debugger client parent.
 * @param form Object
 *        The form sent across the remote debugging protocol.
 */
function EnvironmentClient(client, form) {
  this._client = client;
  this._form = form;
  this.request = this._client.request;
}
exports.EnvironmentClient = EnvironmentClient;

EnvironmentClient.prototype = {

  get actor() {
    return this._form.actor;
  },
  get _transport() {
    return this._client._transport;
  },

  /**
   * Fetches the bindings introduced by this lexical environment.
   */
  getBindings: DebuggerClient.requester({
    type: "bindings"
  }),

  /**
   * Changes the value of the identifier whose name is name (a string) to that
   * represented by value (a grip).
   */
  assign: DebuggerClient.requester({
    type: "assign",
    name: arg(0),
    value: arg(1)
  })
};

eventSource(EnvironmentClient.prototype);
