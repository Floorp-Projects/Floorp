/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { extend } = require("devtools/shared/extend");
const { Pool } = require("devtools/shared/protocol");

/**
 * A Special Pool for RootActor and WindowGlobalTargetActor, which allows lazy loaded
 * actors to be added to the pool.
 *
 * Like the Pool, this is a protocol object that can manage the lifetime of other protocol
 * objects. Pools are used on both sides of the connection to help coordinate lifetimes.
 *
 * @param conn
 *   Is a DevToolsServerConnection.  Must have
 *   addActorPool, removeActorPool, and poolFor.
 * @constructor
 */
function LazyPool(conn) {
  this.conn = conn;
}

LazyPool.prototype = extend(Pool.prototype, {
  // The actor for a given actor id stored in this pool
  getActorByID(actorID) {
    if (this.__poolMap) {
      const entry = this._poolMap.get(actorID);
      if (entry instanceof LazyActor) {
        return entry.createActor();
      }
      return entry;
    }
    return null;
  },
});

exports.LazyPool = LazyPool;

/**
 * Populate |parent._extraActors| as specified by |registeredActors|, reusing whatever
 * actors are already there. Add all actors in the final extra actors table to
 * |pool|. _extraActors is treated as a cache for lazy actors
 *
 * The target actor uses this to instantiate actors that other
 * parts of the browser have specified with ActorRegistry.addTargetScopedActor
 *
 * @param factories
 *     An object whose own property names are the names of properties to add to
 *     some reply packet (say, a target actor grip or the "listTabs" response
 *     form), and whose own property values are actor constructor functions, as
 *     documented for addTargetScopedActor
 *
 * @param parent
 *     The parent TargetActor with which the new actors
 *     will be associated. It should support whatever API the |factories|
 *     constructor functions might be interested in, as it is passed to them.
 *     For the sake of CommonCreateExtraActors itself, it should have at least
 *     the following properties:
 *
 *     - _extraActors
 *        An object whose own property names are factory table (and packet)
 *        property names, and whose values are no-argument actor constructors,
 *        of the sort that one can add to a Pool.
 *
 *     - conn
 *        The DevToolsServerConnection in which the new actors will participate.
 *
 *     - actorID
 *        The actor's name, for use as the new actors' parentID.
 * @param pool
 *     An object which implements the protocol.js Pool interface, and has the
 *     following properties
 *
 *     - manage
 *       a function which adds a given actor to an actor pool
 */
function createExtraActors(registeredActors, pool, parent) {
  // Walk over global actors added by extensions.
  const nameMap = {};
  for (const name in registeredActors) {
    let actor = parent._extraActors[name];
    if (!actor) {
      // Register another factory, but this time specific to this connection.
      // It creates a fake actor that looks like an regular actor in the pool,
      // but without actually instantiating the actor.
      // It will only be instantiated on the first request made to the actor.
      actor = new LazyActor(registeredActors[name], parent, pool);
      parent._extraActors[name] = actor;
    }

    // If the actor already exists in the pool, it may have been instantiated,
    // so make sure not to overwrite it by a non-instantiated version.
    if (!pool.has(actor.actorID)) {
      pool.manage(actor);
    }
    nameMap[name] = actor.actorID;
  }
  return nameMap;
}

exports.createExtraActors = createExtraActors;

/**
 * Creates an "actor-like" object which responds in the same way as an ordinary actor
 * but has fewer capabilities (ie, does not manage lifetimes or have it's own pool).
 *
 *
 * @param factories
 *     An object whose own property names are the names of properties to add to
 *     some reply packet (say, a target actor grip or the "listTabs" response
 *     form), and whose own property values are actor constructor functions, as
 *     documented for addTargetScopedActor
 *
 * @param parent
 *     The parent TargetActor with which the new actors
 *     will be associated. It should support whatever API the |factories|
 *     constructor functions might be interested in, as it is passed to them.
 *     For the sake of CommonCreateExtraActors itself, it should have at least
 *     the following properties:
 *
 *     - _extraActors
 *        An object whose own property names are factory table (and packet)
 *        property names, and whose values are no-argument actor constructors,
 *        of the sort that one can add to a Pool.
 *
 *     - conn
 *        The DevToolsServerConnection in which the new actors will participate.
 *
 *     - actorID
 *        The actor's name, for use as the new actors' parentID.
 * @param pool
 *     An object which implements the protocol.js Pool interface, and has the
 *     following properties
 *
 *     - manage
 *       a function which adds a given actor to an actor pool
 */

function LazyActor(factory, parent, pool) {
  this._options = factory.options;
  this._parentActor = parent;
  this._name = factory.name;
  this._pool = pool;

  // needed for taking a place in a pool
  this.typeName = factory.name;
}

LazyActor.prototype = {
  loadModule(id) {
    const options = this._options;
    try {
      return require(id);
      // Fetch the actor constructor
    } catch (e) {
      throw new Error(
        `Unable to load actor module '${options.id}'\n${e.message}\n${e.stack}\n`
      );
    }
  },

  getConstructor() {
    const options = this._options;
    if (options.constructorFun) {
      // Actor definition registered by testing helpers
      return options.constructorFun;
    }
    // Lazy actor definition, where options contains all the information
    // required to load the actor lazily.
    // Exposes `name` attribute in order to allow removeXXXActor to match
    // the actor by its actor constructor name.
    this.name = options.constructorName;
    const module = this.loadModule(options.id);
    const constructor = module[options.constructorName];
    if (!constructor) {
      throw new Error(
        `Unable to find actor constructor named '${this.name}'. (Is it exported?)`
      );
    }
    return constructor;
  },

  /**
   * Return the parent pool for this lazy actor.
   */
  getParent() {
    return this.conn && this.conn.poolFor(this.actorID);
  },

  /**
   * This will only happen if the actor is destroyed before it is created
   * We do not want to use the Pool destruction method, because this actor
   * has no pool. However, it might have a parent that should unmange this
   * actor
   */
  destroy() {
    const parent = this.getParent();
    if (parent) {
      parent.unmanage(this);
    }
  },

  createActor() {
    // Fetch the actor constructor
    const Constructor = this.getConstructor();
    // Instantiate a new actor instance
    const conn = this._parentActor.conn;
    // this should be taken care of once all actors are moved to protocol.js
    const instance = new Constructor(conn, this._parentActor);
    instance.conn = conn;

    // We want the newly-constructed actor to completely replace the factory
    // actor. Reusing the existing actor ID will make sure Pool.manage
    // replaces the old actor with the new actor.
    instance.actorID = this.actorID;

    this._pool.manage(instance);

    return instance;
  },
};
