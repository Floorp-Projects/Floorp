/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { method } = require("devtools/shared/protocol");

/**
 * Construct an ActorPool.
 *
 * ActorPools are actorID -> actor mapping and storage.  These are
 * used to accumulate and quickly dispose of groups of actors that
 * share a lifetime.
 */
function ActorPool(connection, label) {
  this.conn = connection;
  this.label = label;
  this._actors = {};
}

ActorPool.prototype = {
  /**
   * Destroy the pool. This will remove all actors from the pool.
   */
  destroy: function APDestroy() {
    for (const id in this._actors) {
      this.removeActor(this._actors[id]);
    }
  },

  /**
   * Add an actor to the pool. If the actor doesn't have an ID, allocate one
   * from the connection.
   *
   * @param Object actor
   *        The actor to be added to the pool.
   */
  addActor: function APAddActor(actor) {
    actor.conn = this.conn;
    if (!actor.actorID) {
      if (!actor.typeName) {
        throw new Error("Actor should a specify a `typeName` attribute");
      }
      actor.actorID = this.conn.allocID(actor.typeName);
    }

    // If the actor is already in a pool, remove it without destroying it.
    if (actor.registeredPool) {
      delete actor.registeredPool._actors[actor.actorID];
    }
    actor.registeredPool = this;

    this._actors[actor.actorID] = actor;
  },

  /**
   * Remove an actor from the pool. If the actor has a destroy method, call it.
   */
  removeActor(actor) {
    delete this._actors[actor.actorID];
    if (actor.destroy) {
      actor.destroy();
      return;
    }
    // Obsolete destruction method name (might still be used by custom actors)
    if (actor.disconnect) {
      actor.disconnect();
    }
  },

  get: function APGet(actorID) {
    return this._actors[actorID] || undefined;
  },

  has: function APHas(actorID) {
    return actorID in this._actors;
  },

  /**
   * Returns true if the pool is empty.
   */
  isEmpty: function APIsEmpty() {
    return Object.keys(this._actors).length == 0;
  },

  /**
   * Match the api expected by the protocol library.
   */
  unmanage: function(actor) {
    return this.removeActor(actor);
  },

  forEach: function(callback) {
    for (const name in this._actors) {
      callback(this._actors[name]);
    }
  },

  // Generator that yields each non-self child of the pool.
  *poolChildren() {
    if (!this._actors) {
      return;
    }
    for (const actor of Object.values(this._actors)) {
      // Self-owned actors are ok, but don't need visiting twice.
      if (actor === this) {
        continue;
      }
      yield actor;
    }
  },

  dumpPool() {
    for (const actor in this._actors) {
      console.log(`>> ${actor}`);
    }
  },
};

exports.ActorPool = ActorPool;

/**
 * A SourceLocation represents a location in a source.
 *
 * @param SourceActor actor
 *        A SourceActor representing a source.
 * @param Number line
 *        A line within the given source.
 * @param Number column
 *        A column within the given line.
 */
function SourceLocation(actor, line, column, lastColumn) {
  this._connection = actor ? actor.conn : null;
  this._actorID = actor ? actor.actorID : undefined;
  this._line = line;
  this._column = column;
  this._lastColumn = lastColumn !== undefined ? lastColumn : column + 1;
}

SourceLocation.prototype = {
  get sourceActor() {
    return this._connection ? this._connection.getActor(this._actorID) : null;
  },

  get url() {
    return this.sourceActor.url;
  },

  get line() {
    return this._line;
  },

  get column() {
    return this._column;
  },

  get lastColumn() {
    return this._lastColumn;
  },

  get sourceUrl() {
    return this.sourceActor.url;
  },

  equals: function(other) {
    return (
      this.sourceActor.url == other.sourceActor.url &&
      this.line === other.line &&
      (this.column === undefined ||
        other.column === undefined ||
        this.column === other.column)
    );
  },

  toJSON: function() {
    return {
      source: this.sourceActor.form(),
      line: this.line,
      column: this.column,
      lastColumn: this.lastColumn,
    };
  },
};

exports.SourceLocation = SourceLocation;

/**
 * A method decorator that ensures the actor is in the expected state before
 * proceeding. If the actor is not in the expected state, the decorated method
 * returns a rejected promise.
 *
 * The actor's state must be at this.state property.
 *
 * @param String expectedState
 *        The expected state.
 * @param String activity
 *        Additional info about what's going on.
 * @param Function methodFunc
 *        The actor method to proceed with when the actor is in the expected
 *        state.
 *
 * @returns Function
 *          The decorated method.
 */
function expectState(expectedState, methodFunc, activity) {
  return function(...args) {
    if (this.state !== expectedState) {
      const msg =
        `Wrong state while ${activity}:` +
        `Expected '${expectedState}', ` +
        `but current state is '${this.state}'.`;
      return Promise.reject(new Error(msg));
    }

    return methodFunc.apply(this, args);
  };
}

exports.expectState = expectState;

/**
 * Proxies a call from an actor to an underlying module, stored
 * as `bridge` on the actor. This allows a module to be defined in one
 * place, usable by other modules/actors on the server, but a separate
 * module defining the actor/RDP definition.
 *
 * @see Framerate implementation: devtools/server/performance/framerate.js
 * @see Framerate actor definition: devtools/server/actors/framerate.js
 */
function actorBridge(methodName, definition = {}) {
  return method(function() {
    return this.bridge[methodName].apply(this.bridge, arguments);
  }, definition);
}
exports.actorBridge = actorBridge;

/**
 * Like `actorBridge`, but without a spec definition, for when the actor is
 * created with `ActorClassWithSpec` rather than vanilla `ActorClass`.
 */
function actorBridgeWithSpec(methodName) {
  return method(function() {
    return this.bridge[methodName].apply(this.bridge, arguments);
  });
}
exports.actorBridgeWithSpec = actorBridgeWithSpec;
