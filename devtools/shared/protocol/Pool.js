/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EventEmitter = require("devtools/shared/event-emitter");

/**
 * Actor and Front implementations
 */

/**
 * A protocol object that can manage the lifetime of other protocol
 * objects. Pools are used on both sides of the connection to help coordinate lifetimes.
 *
 * @param optional conn
 *   Either a DebuggerServerConnection or a DebuggerClient.  Must have
 *   addActorPool, removeActorPool, and poolFor.
 *   conn can be null if the subclass provides a conn property.
 * @constructor
 */
class Pool extends EventEmitter {
  constructor(conn) {
    super();

    if (conn) {
      this.conn = conn;
    }
    this.__poolMap = null;
  }

  /**
   * Return the parent pool for this client.
   */
  parent() {
    return this.conn.poolFor(this.actorID);
  }

  poolFor(actorID) {
    return this.conn.poolFor(actorID);
  }

  /**
   * Override this if you want actors returned by this actor
   * to belong to a different actor by default.
   */
  marshallPool() {
    return this;
  }

  /**
   * Pool is the base class for all actors, even leaf nodes.
   * If the child map is actually referenced, go ahead and create
   * the stuff needed by the pool.
   */
  get _poolMap() {
    if (this.__poolMap) {
      return this.__poolMap;
    }
    this.__poolMap = new Map();
    this.conn.addActorPool(this);
    return this.__poolMap;
  }

  /**
   * Add an actor as a child of this pool.
   */
  manage(actor) {
    if (!actor.actorID) {
      actor.actorID = this.conn.allocID(actor.actorPrefix || actor.typeName);
    } else {
      // If the actor is already registerd in a pool, remove it without destroying it.
      // This happens for example when an addon is reloaded. To see this behavior, take a
      // look at devtools/server/tests/unit/test_addon_reload.js

      // TODO: not all actors have been moved to protocol.js, so they do not all have
      // a parent field. Remove the check for the parent once the conversion is finished
      const parent = this.poolFor(actor.actorID);
      if (parent) {
        parent.unmanage(actor);
      }
    }
    this._poolMap.set(actor.actorID, actor);
  }

  /**
   * Remove an actor as a child of this pool.
   */
  unmanage(actor) {
    this.__poolMap && this.__poolMap.delete(actor.actorID);
  }

  // true if the given actor ID exists in the pool.
  has(actorID) {
    return this.__poolMap && this._poolMap.has(actorID);
  }

  // The actor for a given actor id stored in this pool
  actor(actorID) {
    if (this.__poolMap) {
      return this._poolMap.get(actorID);
    }
    return null;
  }

  // Same as actor, should update debugger connection to use 'actor'
  // and then remove this.
  get(actorID) {
    if (this.__poolMap) {
      return this._poolMap.get(actorID);
    }
    return null;
  }

  // True if this pool has no children.
  isEmpty() {
    return !this.__poolMap || this._poolMap.size == 0;
  }

  // Generator that yields each non-self child of the pool.
  * poolChildren() {
    if (!this.__poolMap) {
      return;
    }
    for (const actor of this.__poolMap.values()) {
      // Self-owned actors are ok, but don't need visiting twice.
      if (actor === this) {
        continue;
      }
      yield actor;
    }
  }

  /**
   * Destroy this item, removing it from a parent if it has one,
   * and destroying all children if necessary.
   */
  destroy() {
    const parent = this.parent();
    if (parent) {
      parent.unmanage(this);
    }
    if (!this.__poolMap) {
      return;
    }
    for (const actor of this.__poolMap.values()) {
      // Self-owned actors are ok, but don't need destroying twice.
      if (actor === this) {
        continue;
      }
      const destroy = actor.destroy;
      if (destroy) {
        // Disconnect destroy while we're destroying in case of (misbehaving)
        // circular ownership.
        actor.destroy = null;
        destroy.call(actor);
        actor.destroy = destroy;
      }
    }
    this.conn.removeActorPool(this, true);
    this.__poolMap.clear();
    this.__poolMap = null;
  }

  /**
   * For getting along with the debugger server pools, should be removable
   * eventually.
   */
  cleanup() {
    this.destroy();
  }
}

exports.Pool = Pool;
