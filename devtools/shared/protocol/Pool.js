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
 * @param {DevToolsServerConnection|DevToolsClient} [conn]
 *   Either a DevToolsServerConnection or a DevToolsClient. Must have
 *   addActorPool, removeActorPool, and poolFor.
 *   conn can be null if the subclass provides a conn property.
 * @param {String} [label]
 *   An optional label for the Pool.
 * @constructor
 */
class Pool extends EventEmitter {
  constructor(conn, label) {
    super();

    if (conn) {
      this.conn = conn;
    }
    this.label = label;
    this.__poolMap = null;
  }

  /**
   * Return the parent pool for this client.
   */
  getParent() {
    return this.conn.poolFor(this.actorID);
  }

  /**
   * A pool is at the top of its pool hierarchy if it has:
   * - no parent
   * - or it is its own parent
   */
  isTopPool() {
    const parent = this.getParent();
    return !parent || parent === this;
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
      actor.actorID = this.conn.allocID(actor.typeName);
    } else {
      // If the actor is already registered in a pool, remove it without destroying it.
      // This happens for example when an addon is reloaded. To see this behavior, take a
      // look at devtools/server/tests/xpcshell/test_addon_reload.js

      const parent = actor.getParent();
      if (parent) {
        parent.unmanage(actor);
      }
    }
    this._poolMap.set(actor.actorID, actor);
  }

  unmanageChildren(FrontType) {
    for (const front of this.poolChildren()) {
      if (!FrontType || front instanceof FrontType) {
        this.unmanage(front);
      }
    }
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

  /**
   * Search for an actor in this pool, given an actorID
   * @param {String} actorID
   * @returns {Actor/null} Returns null if the actor wasn't found
   */
  getActorByID(actorID) {
    if (this.__poolMap) {
      return this._poolMap.get(actorID);
    }
    return null;
  }

  // Generator that yields each non-self child of the pool.
  *poolChildren() {
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
   * Pools can override this method in order to opt-out of a destroy sequence.
   *
   * For instance, Fronts are destroyed during the toolbox destroy. However when
   * the toolbox is destroyed, the document holding the toolbox is also
   * destroyed. So it should not be necessary to cleanup Fronts during toolbox
   * destroy.
   *
   * For the time being, Fronts (or Pools in general) which want to opt-out of
   * toolbox destroy can override this method and check the value of
   * `this.conn.isToolboxDestroy`.
   */
  skipDestroy() {
    return false;
  }

  /**
   * Destroy this item, removing it from a parent if it has one,
   * and destroying all children if necessary.
   */
  destroy() {
    const parent = this.getParent();
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

      // Some pool-managed values don't extend Pool and won't have skipDestroy
      // defined. For instance, the root actor and the lazy actors.
      if (typeof actor.skipDestroy === "function" && actor.skipDestroy()) {
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
    this.conn.removeActorPool(this);
    this.__poolMap.clear();
    this.__poolMap = null;
  }
}

exports.Pool = Pool;
