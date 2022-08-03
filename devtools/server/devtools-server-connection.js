/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { Pool } = require("devtools/shared/protocol");
var DevToolsUtils = require("devtools/shared/DevToolsUtils");
var { dumpn } = DevToolsUtils;

loader.lazyRequireGetter(this, "EventEmitter", "devtools/shared/event-emitter");
loader.lazyRequireGetter(
  this,
  "DevToolsServer",
  "devtools/server/devtools-server",
  true
);

/**
 * Creates a DevToolsServerConnection.
 *
 * Represents a connection to this debugging global from a client.
 * Manages a set of actors and actor pools, allocates actor ids, and
 * handles incoming requests.
 *
 * @param prefix string
 *        All actor IDs created by this connection should be prefixed
 *        with prefix.
 * @param transport transport
 *        Packet transport for the debugging protocol.
 * @param socketListener SocketListener
 *        SocketListener which accepted the transport.
 *        If this is null, the transport is not that was accepted by SocketListener.
 */
function DevToolsServerConnection(prefix, transport, socketListener) {
  this._prefix = prefix;
  this._transport = transport;
  this._transport.hooks = this;
  this._nextID = 1;
  this._socketListener = socketListener;

  this._actorPool = new Pool(this, "server-connection");
  this._extraPools = [this._actorPool];

  // Responses to a given actor must be returned the the client
  // in the same order as the requests that they're replying to, but
  // Implementations might finish serving requests in a different
  // order.  To keep things in order we generate a promise for each
  // request, chained to the promise for the request before it.
  // This map stores the latest request promise in the chain, keyed
  // by an actor ID string.
  this._actorResponses = new Map();

  /*
   * We can forward packets to other servers, if the actors on that server
   * all use a distinct prefix on their names. This is a map from prefixes
   * to transports: it maps a prefix P to a transport T if T conveys
   * packets to the server whose actors' names all begin with P + "/".
   */
  this._forwardingPrefixes = new Map();

  EventEmitter.decorate(this);
}
exports.DevToolsServerConnection = DevToolsServerConnection;

DevToolsServerConnection.prototype = {
  _prefix: null,
  get prefix() {
    return this._prefix;
  },

  /**
   * For a DevToolsServerConnection used in content processes,
   * returns the prefix of the connection it originates from, from the parent process.
   */
  get parentPrefix() {
    this.prefix.replace(/child\d+\//, "");
  },

  _transport: null,
  get transport() {
    return this._transport;
  },

  /**
   * Message manager used to communicate with the parent process,
   * set by child.js. Is only defined for connections instantiated
   * within a child process.
   */
  parentMessageManager: null,

  close(options) {
    if (this._transport) {
      this._transport.close(options);
    }
  },

  send(packet) {
    this.transport.send(packet);
  },

  /**
   * Used when sending a bulk reply from an actor.
   * @see DebuggerTransport.prototype.startBulkSend
   */
  startBulkSend(header) {
    return this.transport.startBulkSend(header);
  },

  allocID(prefix) {
    return this.prefix + (prefix || "") + this._nextID++;
  },

  /**
   * Add a map of actor IDs to the connection.
   */
  addActorPool(actorPool) {
    this._extraPools.push(actorPool);
  },

  /**
   * Remove a previously-added pool of actors to the connection.
   *
   * @param Pool actorPool
   *        The Pool instance you want to remove.
   */
  removeActorPool(actorPool) {
    // When a connection is closed, it removes each of its actor pools. When an
    // actor pool is removed, it calls the destroy method on each of its
    // actors. Some actors, such as ThreadActor, manage their own actor pools.
    // When the destroy method is called on these actors, they manually
    // remove their actor pools. Consequently, this method is reentrant.
    //
    // In addition, some actors, such as ThreadActor, perform asynchronous work
    // (in the case of ThreadActor, because they need to resume), before they
    // remove each of their actor pools. Since we don't wait for this work to
    // be completed, we can end up in this function recursively after the
    // connection already set this._extraPools to null.
    //
    // This is a bug: if the destroy method can perform asynchronous work,
    // then we should wait for that work to be completed before setting this.
    // _extraPools to null. As a temporary solution, it should be acceptable
    // to just return early (if this._extraPools has been set to null, all
    // actors pools for this connection should already have been removed).
    if (this._extraPools === null) {
      return;
    }
    const index = this._extraPools.lastIndexOf(actorPool);
    if (index > -1) {
      this._extraPools.splice(index, 1);
    }
  },

  /**
   * Add an actor to the default actor pool for this connection.
   */
  addActor(actor) {
    this._actorPool.manage(actor);
  },

  /**
   * Remove an actor to the default actor pool for this connection.
   */
  removeActor(actor) {
    this._actorPool.unmanage(actor);
  },

  /**
   * Match the api expected by the protocol library.
   */
  unmanage(actor) {
    return this.removeActor(actor);
  },

  /**
   * Look up an actor implementation for an actorID.  Will search
   * all the actor pools registered with the connection.
   *
   * @param actorID string
   *        Actor ID to look up.
   */
  getActor(actorID) {
    const pool = this.poolFor(actorID);
    if (pool) {
      return pool.getActorByID(actorID);
    }

    if (actorID === "root") {
      return this.rootActor;
    }

    return null;
  },

  _getOrCreateActor(actorID) {
    try {
      const actor = this.getActor(actorID);
      if (!actor) {
        this.transport.send({
          from: actorID ? actorID : "root",
          error: "noSuchActor",
          message: "No such actor for ID: " + actorID,
        });
        return null;
      }

      if (typeof actor !== "object") {
        // Pools should now contain only actor instances (i.e. objects)
        throw new Error(
          `Unexpected actor constructor/function in Pool for actorID "${actorID}".`
        );
      }

      return actor;
    } catch (error) {
      const prefix = `Error occurred while creating actor' ${actorID}`;
      this.transport.send(this._unknownError(actorID, prefix, error));
    }
    return null;
  },

  poolFor(actorID) {
    for (const pool of this._extraPools) {
      if (pool.has(actorID)) {
        return pool;
      }
    }
    return null;
  },

  _unknownError(from, prefix, error) {
    const errorString = prefix + ": " + DevToolsUtils.safeErrorString(error);
    reportError(errorString);
    dumpn(errorString);
    return {
      from,
      error: "unknownError",
      message: errorString,
    };
  },

  _queueResponse(from, type, responseOrPromise) {
    const pendingResponse =
      this._actorResponses.get(from) || Promise.resolve(null);
    const responsePromise = pendingResponse
      .then(() => {
        return responseOrPromise;
      })
      .then(response => {
        if (!this.transport) {
          throw new Error(
            `Connection closed, pending response from '${from}', ` +
              `type '${type}' failed`
          );
        }

        if (!response.from) {
          response.from = from;
        }

        this.transport.send(response);
      })
      .catch(error => {
        if (!this.transport) {
          throw new Error(
            `Connection closed, pending error from '${from}', ` +
              `type '${type}' failed`
          );
        }

        const prefix = `error occurred while queuing response for '${type}'`;
        this.transport.send(this._unknownError(from, prefix, error));
      });

    this._actorResponses.set(from, responsePromise);
  },

  /**
   * This function returns whether the connection was accepted by passed SocketListener.
   *
   * @param {SocketListener} socketListener
   * @return {Boolean} return true if this connection was accepted by socketListener,
   *         else returns false.
   */
  isAcceptedBy(socketListener) {
    return this._socketListener === socketListener;
  },

  /* Forwarding packets to other transports based on actor name prefixes. */

  /*
   * Arrange to forward packets to another server. This is how we
   * forward debugging connections to child processes.
   *
   * If we receive a packet for an actor whose name begins with |prefix|
   * followed by '/', then we will forward that packet to |transport|.
   *
   * This overrides any prior forwarding for |prefix|.
   *
   * @param prefix string
   *    The actor name prefix, not including the '/'.
   * @param transport object
   *    A packet transport to which we should forward packets to actors
   *    whose names begin with |(prefix + '/').|
   */
  setForwarding(prefix, transport) {
    this._forwardingPrefixes.set(prefix, transport);
  },

  /*
   * Stop forwarding messages to actors whose names begin with
   * |prefix+'/'|. Such messages will now elicit 'noSuchActor' errors.
   */
  cancelForwarding(prefix) {
    this._forwardingPrefixes.delete(prefix);

    // Notify the client that forwarding in now cancelled for this prefix.
    // There could be requests in progress that the client should abort rather leaving
    // handing indefinitely.
    if (this.rootActor) {
      this.send(this.rootActor.forwardingCancelled(prefix));
    }
  },

  sendActorEvent(actorID, eventName, event = {}) {
    event.from = actorID;
    event.type = eventName;
    this.send(event);
  },

  // Transport hooks.

  /**
   * Called by DebuggerTransport to dispatch incoming packets as appropriate.
   *
   * @param packet object
   *        The incoming packet.
   */
  onPacket(packet) {
    // If the actor's name begins with a prefix we've been asked to
    // forward, do so.
    //
    // Note that the presence of a prefix alone doesn't indicate that
    // forwarding is needed: in DevToolsServerConnection instances in child
    // processes, every actor has a prefixed name.
    if (this._forwardingPrefixes.size > 0) {
      let to = packet.to;
      let separator = to.lastIndexOf("/");
      while (separator >= 0) {
        to = to.substring(0, separator);
        const forwardTo = this._forwardingPrefixes.get(
          packet.to.substring(0, separator)
        );
        if (forwardTo) {
          forwardTo.send(packet);
          return;
        }
        separator = to.lastIndexOf("/");
      }
    }

    const actor = this._getOrCreateActor(packet.to);
    if (!actor) {
      return;
    }

    let ret = null;

    // handle "requestTypes" RDP request.
    if (packet.type == "requestTypes") {
      ret = {
        from: actor.actorID,
        requestTypes: Object.keys(actor.requestTypes),
      };
    } else if (actor.requestTypes?.[packet.type]) {
      // Dispatch the request to the actor.
      try {
        this.currentPacket = packet;
        ret = actor.requestTypes[packet.type].bind(actor)(packet, this);
      } catch (error) {
        // Support legacy errors from old actors such as thread actor which
        // throw { error, message } objects.
        let errorMessage = error;
        if (error?.error && error?.message) {
          errorMessage = `"(${error.error}) ${error.message}"`;
        }

        const prefix = `error occurred while processing '${packet.type}'`;
        this.transport.send(
          this._unknownError(actor.actorID, prefix, errorMessage)
        );
      } finally {
        this.currentPacket = undefined;
      }
    } else {
      ret = {
        error: "unrecognizedPacketType",
        message: `Actor ${actor.actorID} does not recognize the packet type '${packet.type}'`,
      };
    }

    // There will not be a return value if a bulk reply is sent.
    if (ret) {
      this._queueResponse(packet.to, packet.type, ret);
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
  onBulkPacket(packet) {
    const { actor: actorKey, type } = packet;

    const actor = this._getOrCreateActor(actorKey);
    if (!actor) {
      return;
    }

    // Dispatch the request to the actor.
    let ret;
    if (actor.requestTypes?.[type]) {
      try {
        ret = actor.requestTypes[type].call(actor, packet);
      } catch (error) {
        const prefix = `error occurred while processing bulk packet '${type}'`;
        this.transport.send(this._unknownError(actorKey, prefix, error));
        packet.done.reject(error);
      }
    } else {
      const message = `Actor ${actorKey} does not recognize the bulk packet type '${type}'`;
      ret = { error: "unrecognizedPacketType", message };
      packet.done.reject(new Error(message));
    }

    // If there is a JSON response, queue it for sending back to the client.
    if (ret) {
      this._queueResponse(actorKey, type, ret);
    }
  },

  /**
   * Called by DebuggerTransport when the underlying stream is closed.
   *
   * @param status nsresult
   *        The status code that corresponds to the reason for closing
   *        the stream.
   * @param {object} options
   * @param {boolean} options.isModeSwitching
   *        true when this is called as the result of a change to the devtools.browsertoolbox.scope pref
   */
  onTransportClosed(status, options) {
    dumpn("Cleaning up connection.");
    if (!this._actorPool) {
      // Ignore this call if the connection is already closed.
      return;
    }
    this._actorPool = null;

    this.emit("closed", status, this.prefix);

    // Use filter in order to create a copy of the extraPools array,
    // which might be modified by removeActorPool calls.
    // The isTopLevel check ensures that the pools retrieved here will not be
    // destroyed by another Pool::destroy. Non top-level pools will be destroyed
    // by the recursive Pool::destroy mechanism.
    // See test_connection_closes_all_pools.js for practical examples of Pool
    // hierarchies.
    const topLevelPools = this._extraPools.filter(p => p.isTopPool());
    topLevelPools.forEach(p => p.destroy(options));

    this._extraPools = null;

    this.rootActor = null;
    this._transport = null;
    DevToolsServer._connectionClosed(this);
  },

  dumpPool(pool, output = [], dumpedPools) {
    const actorIds = [];
    const children = [];

    if (dumpedPools.has(pool)) {
      return;
    }
    dumpedPools.add(pool);

    // TRUE if the pool is a Pool
    if (!pool.__poolMap) {
      return;
    }

    for (const actor of pool.poolChildren()) {
      children.push(actor);
      actorIds.push(actor.actorID);
    }
    const label = pool.label || pool.actorID;

    output.push([label, actorIds]);
    dump(`- ${label}: ${JSON.stringify(actorIds)}\n`);
    children.forEach(childPool =>
      this.dumpPool(childPool, output, dumpedPools)
    );
  },

  /*
   * Debugging helper for inspecting the state of the actor pools.
   */
  dumpPools() {
    const output = [];
    const dumpedPools = new Set();

    this._extraPools.forEach(pool => this.dumpPool(pool, output, dumpedPools));

    return output;
  },

  /**
   * In a content child process, ask the DevToolsServer in the parent process
   * to execute a given module setup helper.
   *
   * @param module
   *        The module to be required
   * @param setupParent
   *        The name of the setup helper exported by the above module
   *        (setup helper signature: function ({mm}) { ... })
   * @return boolean
   *         true if the setup helper returned successfully
   */
  setupInParent({ module, setupParent }) {
    if (!this.parentMessageManager) {
      return false;
    }

    return this.parentMessageManager.sendSyncMessage("debug:setup-in-parent", {
      prefix: this.prefix,
      module,
      setupParent,
    });
  },

  /**
   * Instanciates a protocol.js actor in the parent process, from the content process
   * module is the absolute path to protocol.js actor module
   *
   * @param spawnByActorID string
   *        The actor ID of the actor that is requesting an actor to be created.
   *        This is used as a prefix to compute the actor id of the actor created
   *        in the parent process.
   * @param module string
   *        Absolute path for the actor module to load.
   * @param constructor string
   *        The symbol exported by this module that implements Actor.
   * @param args array
   *        Arguments to pass to its constructor
   */
  spawnActorInParentProcess(spawnedByActorID, { module, constructor, args }) {
    if (!this.parentMessageManager) {
      return null;
    }

    const mm = this.parentMessageManager;

    const onResponse = new Promise(done => {
      const listener = msg => {
        if (msg.json.prefix != this.prefix) {
          return;
        }
        mm.removeMessageListener("debug:spawn-actor-in-parent:actor", listener);
        done(msg.json.actorID);
      };
      mm.addMessageListener("debug:spawn-actor-in-parent:actor", listener);
    });

    mm.sendAsyncMessage("debug:spawn-actor-in-parent", {
      prefix: this.prefix,
      module,
      constructor,
      args,
      spawnedByActorID,
    });

    return onResponse;
  },
};
