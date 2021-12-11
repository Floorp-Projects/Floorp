/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const protocol = require("devtools/shared/protocol");
const { symbolSpec } = require("devtools/shared/specs/symbol");
loader.lazyRequireGetter(
  this,
  "createValueGrip",
  "devtools/server/actors/object/utils",
  true
);

/**
 * Creates an actor for the specified symbol.
 *
 * @param {DevToolsServerConnection} conn: The connection to the client.
 * @param {Symbol} symbol: The symbol we want to create an actor for.
 */
const SymbolActor = protocol.ActorClassWithSpec(symbolSpec, {
  initialize(conn, symbol) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.symbol = symbol;
  },

  rawValue: function() {
    return this.symbol;
  },

  destroy: function() {
    // Because symbolActors is not a weak map, we won't automatically leave
    // it so we need to manually leave on destroy so that we don't leak
    // memory.
    this._releaseActor();
    protocol.Actor.prototype.destroy.call(this);
  },

  /**
   * Returns a grip for this actor for returning in a protocol message.
   */
  form: function() {
    const form = {
      type: this.typeName,
      actor: this.actorID,
    };
    const name = getSymbolName(this.symbol);
    if (name !== undefined) {
      // Create a grip for the name because it might be a longString.
      form.name = createValueGrip(name, this.getParent());
    }
    return form;
  },

  /**
   * Handle a request to release this SymbolActor instance.
   */
  release: function() {
    // TODO: also check if this.getParent() === threadActor.threadLifetimePool
    // when the web console moves away from manually releasing pause-scoped
    // actors.
    this._releaseActor();
    this.destroy();
    return {};
  },

  _releaseActor: function() {
    const parent = this.getParent();
    if (parent && parent.symbolActors) {
      delete parent.symbolActors[this.symbol];
    }
  },
});

const symbolProtoToString = Symbol.prototype.toString;

function getSymbolName(symbol) {
  const name = symbolProtoToString.call(symbol).slice("Symbol(".length, -1);
  return name || undefined;
}

/**
 * Create a grip for the given symbol.
 *
 * @param sym Symbol
 *        The symbol we are creating a grip for.
 * @param pool Pool
 *        The actor pool where the new actor will be added.
 */
function symbolGrip(sym, pool) {
  if (!pool.symbolActors) {
    pool.symbolActors = Object.create(null);
  }

  if (sym in pool.symbolActors) {
    return pool.symbolActors[sym].form();
  }

  const actor = new SymbolActor(pool.conn, sym);
  pool.manage(actor);
  pool.symbolActors[sym] = actor;
  return actor.form();
}

module.exports = {
  SymbolActor,
  symbolGrip,
};
